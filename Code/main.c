/*
 * Copyright (c) 2024 Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    main.c
 * @author  B. Premzel
 * @brief   Main file for format definition file parsing and binary data processing.
 ******************************************************************************/

#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <assert.h>
#include <math.h>
#include <locale.h>
#include <windows.h>
#include <sys/stat.h>
#include <time.h>
#include "main.h"
#include "parse_error_reporting.h"
#include "rtedbg.h"
#include "process_bin_data.h"
#include "files.h"
#include "errors.h"
#include "format.h"
#include "decoder.h"
#include "statistics.h"
#include "rtemsg_config.h"
#include "print_message.h"
#include "print_helper.h"
#include "read_bin_data.h"
#include "parse_directive.h"
#include "cmd_line.h"
#include "utf8_helpers.h"


/**********************************************/
/*****  G L O B A L   V A R I A B L E S  ******/
/**********************************************/

rte_msg_t g_msg;    /*!< Main data structure: file pointers, raw and assembled data, etc. */
    // All other structures and buffers are allocated according to the amount of data in
    // the binary file, specifications in format definition files etc.

static unsigned number_of_fatal_exceptions = 0; // Prevents lockup during fatal error reporting

/**
 * @brief  Check if the value is a power of 2
 *
 * @param n   The number to check.
 *
 * @return TRUE if the number is a power of 2, FALSE otherwise.
 */

bool is_power_of_two(size_t n)
{
    if (n == 0)
        return false;

    double log2_val = log2((double)n);
    return ceil(log2_val) == floor(log2_val);
}


/**
 * @brief Allocate memory for buffers and structures and initialize it to zero.
 *        The function does not return to the caller if the memory cannot be allocated.
 *
 * @param size Size of the memory to allocate in bytes.
 * @param memory_name Name of the buffer or structure for error reporting.
 *
 * @return  Pointer to the allocated memory buffer.
 */

void *allocate_memory(size_t size, const char *memory_name)
{
    static size_t memory_allocated = 0;

    if (size == 0)
    {
        report_fatal_error_and_exit(FATAL_BAD_MALLOC_PARAMETER, NULL, 0);
    }

    void *buffer = calloc(size, 1);   // Allocate and zero-initialize memory
    if (buffer == NULL)
    {
        report_fatal_error_and_exit(FATAL_MALLOC_FAILED, memory_name, memory_allocated);
    }

    memory_allocated += size;
    return buffer;
}


/**
 * @brief  Allocates memory for a string that is large enough to store a copy of the input string.
 *         Then copies the input string contents to the newly allocated string.
 *
 * @param string_to_duplicate    The string to duplicate.
 *
 * @return  Pointer to the duplicated string.
 */

char *duplicate_string(const char *string_to_duplicate)
{
    if (string_to_duplicate == NULL)
    {
        report_fatal_error_and_exit(FATAL_BAD_MALLOC_PARAMETER, NULL, 0);
    }

    size_t strSize = strlen(string_to_duplicate) + 1;
    char *string_duplicate = allocate_memory(strSize, "StringDup");
    strcpy_s(string_duplicate, strSize, string_to_duplicate);

    return string_duplicate;
}


/**
 * @brief Prints command line parameters and RTEmsg utility version and revision.
 * 
 * @param argc Number of command line parameters.
 * @param argv Array of command line parameter strings.
 */

static void print_cmd_line_parameters(int argc, char *argv[])
{
    FILE *out = g_msg.file.main_log;

    fprintf(out, get_message_text(MSG_COMMAND_LINE_PARAMS));
    for (int i = 1; i < argc; i++)
    {
        fprintf(out, "\"%s\" ", argv[i]);
    }
}


/**
 * @brief Prints the full binary file name and creation date, and prepares the date string
 *        for the special format type "%D".
 */

static void print_data_file_name_and_date(void)
{
    FILE *out = g_msg.file.main_log;

    fprintf(out, TXT_MSG_RTEMSG_VERSION, RTEMSG_VERSION, RTEMSG_SUBVERSION, RTEMSG_REVISION, __DATE__);

    struct _stat stbuf;
    int rez = _stat(g_msg.param.data_file_name, &stbuf);
    if (rez == 0)
    {
        fprintf(out, get_message_text(MSG_BIN_FILE_NAME_DATE));
        struct tm *tmp = localtime(&stbuf.st_mtime);
        strftime(g_msg.date_string, BIN_FILE_DATE_LENGTH, "%Y-%m-%d %H:%M:%S", tmp);
        fprintf(out, "\"%s\" %s\n", g_msg.param.data_file_name, g_msg.date_string);
    }
}


/**
 * @brief Create the Timestamps.csv file and write the header
 *        if this functionality was enabled with a command line parameter
 */

static void create_timestamps_file(void)
{
    open_output_folder();
    if (g_msg.param.create_timestamp_file)
    {
        g_msg.file.timestamps = fopen(RTE_MSG_TIMESTAMPS_FILE, "w");
        if (g_msg.file.timestamps != NULL)
        {
#if defined STREAM_BUFF_SIZE
            char *stream_buffer = allocate_memory(STREAM_BUFF_SIZE, "strBuff");
            int rez = setvbuf(g_msg.file.main_log, stream_buffer, _IOFBF, STREAM_BUFF_SIZE);
#endif
            fprintf(g_msg.file.timestamps, get_message_text(MSG_TIMESTAMP_DIFFERENCES));
            switch (g_msg.param.time_unit)
            {
                case 'm':
                    fprintf(g_msg.file.timestamps, "[ms]\n");
                    break;

                case 'u':
                    fprintf(g_msg.file.timestamps, "[µs]\n");
                    break;

                default:
                    fprintf(g_msg.file.timestamps, "[s]\n");
                    break;
            }
        }
        else
        {
            report_problem_with_string(FATAL_CANT_CREATE_FILE, RTE_MSG_TIMESTAMPS_FILE);
        }
    }
}


/**
 * @brief Prints the execution time to Main.log.
 *
 * @param begin     RTEmsg app starting time.
 * @param end       Format parsing end time.
 */

static void print_execution_time(clock_t begin, clock_t end) {
    clock_t end2 = clock();

    double elapsed_parsing = ((double)end - (double)begin) / (double)(CLOCKS_PER_SEC);
    double elapsed_bin_file_processing = ((double)end2 - (double)end) / (double)(CLOCKS_PER_SEC);

    if (g_msg.param.check_syntax_and_compile == false)
    {
        fprintf(g_msg.file.main_log, get_message_text(MSG_TOTAL_TIME_ELAPSED),
            elapsed_parsing, elapsed_bin_file_processing);
    }
    else
    {
        fprintf(g_msg.file.error_log, get_message_text(MSG_TIME_ELAPSED), elapsed_parsing);
    }
}


/**
 * @brief Set the locale for message printing based on user or system settings.
 */

static void set_message_printing_locale(void)
{
    if (g_msg.param.locale_name != NULL)
    {
        // Set the locale to the user-defined setting
        setlocale(LC_ALL, g_msg.param.locale_name);
    }
    else
    {
        // Use the system's default locale
        setlocale(LC_ALL, "");
    }

    if (g_msg.param.codepage_utf8)
    {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }
}


/**
 * @brief Report a fatal error encountered during message decoding.
 */

static void report_fatal_error_during_message_decoding(void)
{
    // Additional error reporting is not yet implemented
    if (++number_of_fatal_exceptions < 2)
    {
        if ((g_msg.file.main_log != NULL) && (g_msg.file.main_log != g_msg.file.error_log))
        {
            fprintf(g_msg.file.main_log, "\nFatal exception occurred while processing binary files!");
        }
        fprintf(g_msg.file.error_log, "\nFatal exception occurred while processing binary files!");
        _fcloseall();
        (void)_wchdir(g_msg.file.start_folder);      // Return to the initial working directory
    }
}


/**
 * @brief Report a fatal error during format file processing and close all files.
 */

static void report_fatal_error_during_format_file_processing(void)
{
    // Additional error reporting is not yet implemented
    if (++number_of_fatal_exceptions < 2)
    {
        fprintf(g_msg.file.error_log, "\nFatal exception occurred during format file processing!");
        // TODO: Report the name and line of the currently processed format definition file
        open_output_folder();
        remove_file(RTE_MAIN_LOG_FILE);             // Remove the Main.log file
        _fcloseall();
        (void)_wchdir(g_msg.file.start_folder);     // Return to the initial working directory
    }
}


/**
 * @brief Remove files from previous RTEmsg runs if they exist.
 *        This prevents confusion from finding decoded information when an error is reported.
 */

static void remove_invalid_files(void)
{
    _fcloseall();
    open_output_folder();               // Open the folder where output files are created
    remove_file(RTE_MAIN_LOG_FILE);
    remove_file(RTE_STAT_MAIN_FILE);
    remove_file(RTE_STAT_MSG_COUNTERS_FILE);
    remove_file(RTE_STAT_MISSING_MSGS_FILE);
}


/**
 * @brief Print final notes and warnings to the log.
 */

static void print_notes_and_warnings(void)
{
    bool print_long_timestamp_warning =
        (!g_msg.timestamp.long_timestamp_found) && RTE_USE_LONG_TIMESTAMP;

    if (print_long_timestamp_warning || g_msg.timestamp.suspicious_timestamp)
    {
        fprintf(g_msg.file.main_log, get_message_text(MSG_NOTE));
        if (print_long_timestamp_warning)
        {
            fprintf(g_msg.file.main_log, get_message_text(MSG_WARNING_NO_LONG_TSTAMP_FOUND));
        }

        if (g_msg.timestamp.suspicious_timestamp)
        {
            fprintf(g_msg.file.main_log,
                get_message_text(MSG_NOTE_SUSPICIOUS_TIMESTAMPS_FOUND),
                g_msg.timestamp.suspicious_timestamp
            );
        }

        fprintf(g_msg.file.main_log, "\n");
    }
}


/**
 * @brief Prepare the data structure for the topmost format definition used
 *        for MSG1_SYS_STREAMING_MODE_LOGGING (internal system messages).
 *        This is the only system message format structure not initialized
 *        during 'rte_system.h' format file parsing.
 */

static void prepare_sys_msg_fmt_structure(void)
{
    msg_data_t *p_fmt = allocate_memory(sizeof(msg_data_t), "sysFmt");
    g_fmt[MSG1_SYS_STREAMING_MODE_LOGGING] = p_fmt;
    p_fmt->msg_len = 4u;
    p_fmt->msg_type = TYPE_MSG0_4;
    p_fmt->message_name = "sys";
}


/**
 * @brief Main function for processing binary data files.
 * 
 * @param argc Number of command line parameters
 * @param argv Array of command line parameter strings
 */

static void Process_binary_data_file(int argc, char *argv[])
{
    create_main_log_file();
    load_and_check_rtedbg_header();     // Read and verify the header
    print_data_file_name_and_date();    // Print header information
    print_cmd_line_parameters(argc, argv);
    print_bin_file_header_info();
    check_timestamp_diff_values();      // Check the values of the -ts command line argument (the timestamp period is known here)
    load_data_from_binary_file();
    reset_statistics();

    if (data_in_the_buffer() == NO_DATA_FOUND)
    {
        report_fatal_error_and_exit(FATAL_NO_DATA_IN_BINARY_INPUT_FILE, g_msg.param.data_file_name, 0);
    }

    // Add an empty line before the first message if errors were reported
    if (g_msg.total_errors > 0)
    {
        fprintf(g_msg.file.main_log, "\n");
    }

    // Allocate buffer for message processing
    g_msg.assembled_msg = (uint32_t *)allocate_memory(
        sizeof(uint32_t) * 4u * (1u + (size_t)g_msg.hdr_data.max_msg_blocks) + 20u, "Asm_msg");
    prepare_sys_msg_fmt_structure();

    print_msg_intro();
    process_bin_data_worker();           // Process the loaded binary data
    write_statistics_to_file();          // Generate various statistics files (if enabled)
    report_decode_error_summary();
    print_notes_and_warnings();
}


/**
 * @brief RTEmsg - Main function for the binary data decoding utility.
 *        Refer to the RTEdbg library and tools manual for more details.
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line argument strings.
 *
 * @return Error code or zero if no errors were detected.
 */

int main(int argc, char *argv[])
{
    int ret_value = 0;
    clock_t begin_parsing = clock();    // Start measuring execution time.
   
    setup_working_folder_info();
    load_text_messages();               // Load text strings for errors and other messages.
    process_command_line_parameters(argc, argv);

    if (g_msg.param.codepage_utf8)
    {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }

    create_error_file();                // Create default output files and load error messages.

    __try
    {
        setlocale(LC_ALL, "C");         // Set locale to "C" to use dots as decimal separators.
        create_timestamps_file();
        remove_old_files();

        // Initialize the first 32 enum locations for filter information.
        g_msg.enums_found = NUMBER_OF_FILTER_BITS;

        parse_fmt_file(RTE_MAIN_FMT_FILE, NULL);    // Begin parsing the main format file.
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        report_fatal_error_during_format_file_processing();
        return EXIT_FATAL_EXCEPTION_DETECTED_FMT;
    }

    clock_t end_parsing = clock();      // End of format definition parsing and start of binary file processing.
    set_message_printing_locale();

    char text[MAX_UTF8_TEXT_LENGTH];
    if (g_msg.total_errors > 0)
    {
        if (!g_msg.param.check_syntax_and_compile)
        {
            snprintf(text, MAX_UTF8_TEXT_LENGTH, get_message_text(MSG_ERRORS_DURING_FMT_PROCESSING));
            utf8_print_string(text, 0);
        }
        remove_invalid_files();
        (void)_wchdir(g_msg.file.start_folder);      // Return to the initial working directory.
        return EXIT_FATAL_FMT_PARSING_ERRORS;
    }

    print_format_decoding_information();
    dump_filter_names_to_file();

    if (!g_msg.param.check_syntax_and_compile)
    {
        __try
        {
            Process_binary_data_file(argc, argv);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            report_fatal_error_during_message_decoding();
            return EXIT_FATAL_EXCEPTION_DETECTED_BIN;
        }

        if (g_msg.total_errors > 0)
        {
            ret_value = EXIT_FATAL_DECODING_ERRORS_DETECTED;
            if (g_msg.binary_file_decoding_finished)
            {
                ret_value = EXIT_NON_FATAL_DECODING_ERRORS_DETECTED;
            }
        }
    }

    print_execution_time(begin_parsing, end_parsing);
    _fcloseall();
    (void)_wchdir(g_msg.file.start_folder);      // Return to the initial working directory.
    return ret_value;
}

// Validate that the first errors in two error groups have the correct values.
static_assert(FATAL_NO_DATA_IN_BINARY_INPUT_FILE == FIRST_FATAL_ERROR, "Must have a value of 30");
static_assert(FIRST_ERROR == 100u, "Must have a value of 100");
static_assert(ERR_PARSE_UNKNOWN == 200u, "Must have a value of 200");

/*==== End of file ====*/
