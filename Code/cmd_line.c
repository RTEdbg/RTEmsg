/*
 * Copyright (c) 2024 Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    cmd_line.c
 * @author  B. Premzel
 * @brief   Command line parameter processing functions
 ******************************************************************************/

#include "pch.h"
#include <stdlib.h>
#include <string.h>
#include "cmd_line.h"
#include "errors.h"
#include "files.h"
#include "decoder.h"


/**
 * @brief Processes the timestamp command line parameter.
 * @param unit    Timestamp unit. Valid values:
 *                      - "s"  : seconds
 *                      - "m"  or "ms" : milliseconds
 *                      - "u"  or "us" : microseconds
 * @param parameter_text Full parameter text to include in error message if validation fails.
 */

static void process_timestamp_parameter(const char *unit, const char *parameter_text)
{
    if (strcmp(unit, "s") == 0)
    {
        g_msg.param.time_multiplier = 1.0;
    }
    else if ((strcmp(unit, "m") == 0) || (strcmp(unit, "ms") == 0))
    {
        g_msg.param.time_multiplier = 1e3;
    }
    else if ((strcmp(unit, "u") == 0) || (strcmp(unit, "us") == 0))
    {
        g_msg.param.time_multiplier = 1e6;
    }
    else
    {
        report_error_and_show_instructions(
            get_message_text(FATAL_BAD_TIME_PARAMETER_VALUE), parameter_text);
    }

    g_msg.param.time_unit = *unit;
}


/**
 * @brief Processes the timestamp difference bounds for validation.
 *
 * @param values          String containing two semicolon-separated values representing the
 *                        maximum allowed negative and positive timestamp differences.
 * @param parameter_text  Full parameter text to include in error message if validation fails.
 */

static void process_the_timestamp_diff_value(const char *values, const char *parameter_text)
{
    int no_values = sscanf(values, "%lf;%lf",
                           &g_msg.param.max_negative_tstamp_diff_f, &g_msg.param.max_positive_tstamp_diff_f);

    if ((no_values != 2)
        || (g_msg.param.max_negative_tstamp_diff_f >= 0.0)
        || (g_msg.param.max_positive_tstamp_diff_f <= 0.0))
    {
        report_error_and_show_instructions(
            get_message_text(FATAL_BAD_TS_PARAMETER_VALUE), parameter_text);
    }
}


/**
 * @brief Check and processes the timestamp difference bounds.
 */

void check_timestamp_diff_values(void)
{
    if (g_msg.param.max_negative_tstamp_diff_f == 0.0)
    {
        return;     // No -ts command line argument
    }

    double frequency =
        (double)g_msg.rte_header.timestamp_frequency / (double)(1ULL << g_msg.hdr_data.timestamp_shift);
    double timestamp_period_ms =
        1000.0 / frequency * (double)(1ULL << (32U - 1U - g_msg.hdr_data.fmt_id_bits));
    g_msg.param.max_negative_tstamp_diff_f /= timestamp_period_ms;
    g_msg.param.max_positive_tstamp_diff_f /= timestamp_period_ms;

    if ((g_msg.param.max_negative_tstamp_diff_f < -MAX_NEGATIVE_TSTAMP_DIFF)
        || (g_msg.param.max_negative_tstamp_diff_f > -MIN_TIMESTAMP_DIFF)
        || (g_msg.param.max_positive_tstamp_diff_f > MAX_POSITIVE_TSTAMP_DIFF)
        || (g_msg.param.max_positive_tstamp_diff_f < MIN_TIMESTAMP_DIFF))
    {
        report_error_and_show_instructions(
            get_message_text(FATAL_BAD_TS_PARAMETER_VALUE), "");
    }

    g_msg.param.max_positive_tstamp_diff =
        (int64_t)(g_msg.param.max_positive_tstamp_diff_f * (double)NORMALIZED_TSTAMP_PERIOD);
    g_msg.param.max_negative_tstamp_diff =
        (int64_t)(g_msg.param.max_negative_tstamp_diff_f * (double)NORMALIZED_TSTAMP_PERIOD);
}


/**
 * @brief Processes the value that defines the number of format ID bits.
 *
 * @param number          String containing the number to parse.
 * @param parameter_text  Full parameter text to include in error message if validation fails.
 */

static void process_the_N_value(const char *number, const char *parameter_text)
{
    unsigned int n = 0;
    (void)sscanf(number, "%u", &n);  // The 'n' variable has a value 0 if the conversion fails

    if ((n < 9U) || (n > 16U))
    {
        report_error_and_show_instructions(
            get_message_text(FATAL_BAD_N_PARAMETER_VALUE), parameter_text);
    }

    g_msg.param.number_of_format_id_bits = (char)n;
}


/**
 * @brief Saves the name of the binary data file.
 *        Reports an error if a data file has already been defined (only one data file name is allowed).
 *
 * @param file_name The binary data file name to save.
 */

static void save_data_file_name(char *file_name)
{
    if (g_msg.param.data_file_name == NULL)
    {
        g_msg.param.data_file_name = prepare_folder_name(file_name, 0);
    }
    else
    {
        report_error_and_show_instructions(
            get_message_text(FATAL_UNKNOWN_PARAM_OR_FILE_DEFINED_TWICE), file_name);
    }
}


/**
 * @brief Processes the '-e=' error reporting command line parameter.
 *
 * @param cmd_line_parameter  The full parameter text starting with "-e=".
 */

static void process_error_command_line_option(char *cmd_line_parameter)
{
    char *definition = &cmd_line_parameter[3];

    // Strip off the quotation marks at the start and end of definition (if present).
    if (*definition == '"')
    {
        definition++;
        size_t len = strlen(definition);

        if ((len > 0) && (definition[len - 1] == '"'))
        {
            definition[len - 1] = '\0';
        }
    }

    process_escape_sequences(definition, MAX_TXT_MESSAGE_LENGTH);
    g_msg.param.report_error = definition;
}


/**
 * @brief Sets default parameters for the application.
 */

static void set_default_parameters(void)
{
    // Define the maximum number of format IDs.
    // Note: The last two format IDs are reserved for system messages.
    g_msg.hdr_data.topmost_fmt_id = (1ul << g_msg.param.number_of_format_id_bits) - 2ul;

    // Set the default print format for timestamps if not specified via command line.
    if (g_msg.param.timestamp_print == NULL)
    {
        switch (g_msg.param.time_unit)
        {
            case 'u':  // Microseconds
                g_msg.param.timestamp_print = "%8.2f";
                break;

            case 'm':  // Milliseconds
                g_msg.param.timestamp_print = "%8.3f";
                break;

            default:   // Seconds (or invalid unit)
                g_msg.param.timestamp_print = "%8.6f";
                break;
        }
    }
}


/**
 * @brief Processes a single command line parameter.
 *
 * @param argv  String containing the command line parameter to process.
 */

static void process_one_cmd_line_parameter(char *argv)
{
    if (argv[0] != '-')
    {
        save_data_file_name(argv);
    }
    else if (strcmp(argv, "-c") == 0)
    {
        g_msg.param.check_syntax_and_compile = true;
    }
    else if (strcmp(argv, "-utf8") == 0)
    {
        g_msg.param.codepage_utf8 = true;
    }
    else if (strcmp(argv, "-back") == 0)
    {
        g_msg.param.create_backup = true;
    }
    else if (strncmp(argv, "-nr=", 4) == 0)
    {
        g_msg.param.msg_number_print = allocate_memory(strlen(argv) - 3, "nr");
        strcpy(g_msg.param.msg_number_print + 1, &argv[4]);
        g_msg.param.msg_number_print[0] = '%';
    }
    else if (strcmp(argv, "-stat=all") == 0)
    {
        g_msg.param.value_statistics_enabled = true;
        g_msg.param.message_statistics_enabled = true;
    }
    else if (strcmp(argv, "-stat=msg") == 0)
    {
        g_msg.param.message_statistics_enabled = true;
    }
    else if (strcmp(argv, "-stat=value") == 0)
    {
        g_msg.param.value_statistics_enabled = true;
    }
    else if (strcmp(argv, "-debug") == 0)
    {
        g_msg.param.debug = true;
    }
    else if (strcmp(argv, "-timestamps") == 0)
    {
        g_msg.param.create_timestamp_file = true;
    }
    else if (strncmp(argv, "-e=", 3) == 0)
    {
        process_error_command_line_option(argv);
    }
    else if (strncmp(argv, "-time=", 6) == 0)
    {
        process_timestamp_parameter(&argv[6], argv);
    }
    else if (strncmp(argv, "-locale=", 8) == 0)
    {
        g_msg.param.locale_name = &argv[8];
    }
    else if (strncmp(argv, "-newline", 8) == 0)
    {
        g_msg.param.additional_newline = true;
    }
    else if (strncmp(argv, "-N=", 3) == 0)
    {
        process_the_N_value(&argv[3], argv);
    }
    else if (strcmp(argv, "-purge") == 0)
    {
        g_msg.param.purge_defines = true;
    }
    else if (strncmp(argv, "-T=", 3) == 0)
    {
        g_msg.param.timestamp_print = (char *)allocate_memory(strlen(argv) - 2, "tstamp");
        strcpy(g_msg.param.timestamp_print + 1, &argv[3]);
        g_msg.param.timestamp_print[0] = '%';
    }
    else if (strncmp(argv, "-ts=", 4) == 0)
    {
        process_the_timestamp_diff_value(&argv[4], argv);
    }
    else
    {
        report_error_and_show_instructions(
            get_message_text(FATAL_UNKNOWN_CMD_LINE_OPTION), argv);
    }
}


/**
 * @brief Processes the RTEmsg parameter file.
 *        The file name must start with '@'.
 *        Line 1: output folder (working folder) name
 *        Line 2: FMT folder name
 *        Remaining lines: optional command line parameters
 *
 * @param file_name Name of the file with RTEmsg parameters
 */
static void process_parameter_file(char *file_name)
{
    if (*file_name != '@')
    {
        report_error_and_show_instructions(
            get_message_text(FATAL_BAD_PARAM_FILE), NULL);
    }

    file_name++;    // Skip '@'

    jump_to_start_folder();
    FILE *par_file = fopen(file_name, "r");

    if (par_file == NULL)
    {
        printf("\n[%s]: ", file_name);
        report_error_and_exit(get_message_text(FATAL_CANT_OPEN_PARAMETER_FILE),
                              EXIT_FATAL_ERR_BAD_PARAMETERS);
    }

    // Process the mandatory parameters: output folder and FMT folder
    char file_line[MAX_INPUT_LINE_LENGTH];

    if (fgets(file_line, MAX_INPUT_LINE_LENGTH, par_file) == NULL)
    {
        report_error_and_exit(get_message_text(FATAL_MISSING_OUTPUT_FOLDER),
                              EXIT_FATAL_ERR_BAD_PARAMETERS);
    }

    g_msg.param.working_folder =
        duplicate_string(prepare_folder_name(file_line, FATAL_MISSING_OUTPUT_FOLDER));

    if (fgets(file_line, MAX_INPUT_LINE_LENGTH, par_file) == NULL)
    {
        report_error_and_exit(get_message_text(FATAL_MISSING_FMT_FOLDER),
                              EXIT_FATAL_ERR_BAD_PARAMETERS);
    }

    g_msg.param.fmt_folder =
        duplicate_string(prepare_folder_name(file_line, FATAL_MISSING_FMT_FOLDER));

    // Process the optional parameters
    while (fgets(file_line, MAX_INPUT_LINE_LENGTH, par_file) != NULL)
    {
        // Remove the newline character
        char *newline = strrchr(file_line, '\n');

        if (newline != NULL)
        {
            *newline = '\0';
        }

        if (strlen(file_line) > 0)
        {
            process_one_cmd_line_parameter(duplicate_string(prepare_folder_name(file_line, 0)));
        }
    }

    fclose(par_file);
}


/**
 * @brief Processes command line parameters for the application.
 * @details Accepts two formats:
 *          1. @<parameter_file>
 *             Parameter file contains:
 *             - Line 1: Working folder path
 *             - Line 2: FMT folder path
 *             - Additional lines: Optional parameters
 *          2. <working_folder> <fmt_folder> [options...]
 *             Direct command line parameters
 *
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 */
void process_command_line_parameters(int argc, char *argv[])
{
    // Initialize default parameter values
    g_msg.param.report_error = DEFAULT_ERROR_REPORT;
    g_msg.param.time_multiplier = 1.0;
    g_msg.param.max_negative_tstamp_diff = DEFAULT_NEGATIVE_TIMESTAMP_DIFF;
    g_msg.param.max_positive_tstamp_diff = DEFAULT_POSITIVE_TIMESTAMP_DIFF;

    if (argc == 2)
    {
        // Process parameter file if only one argument is provided
        process_parameter_file(argv[1]);
    }
    else if (argc >= 3)
    {
        // Process mandatory parameters: working folder and FMT folder
        g_msg.param.working_folder = prepare_folder_name(argv[1], FATAL_MISSING_OUTPUT_FOLDER);
        g_msg.param.fmt_folder = prepare_folder_name(argv[2], FATAL_MISSING_FMT_FOLDER);

        // Process additional command line options
        for (int i = 3; i < argc; i++)
        {
            process_one_cmd_line_parameter(argv[i]);
        }
    }
    else
    {
        // Report error if not enough command line parameters are provided
        report_error_and_show_instructions(
            get_message_text(FATAL_NOT_ENOUGH_CMD_LINE_PARAMETERS), NULL);
    }

    // Ensure the number of format ID bits is set
    if (g_msg.param.number_of_format_id_bits == 0)
    {
        report_error_and_show_instructions(get_message_text(FATAL_PARAMETER_N_MISSING), "");
    }

    // Set any remaining default parameters
    set_default_parameters();
}

/*==== End of file ====*/
