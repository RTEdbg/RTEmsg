/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    errors.c
 * @author  B. Premzel
 * @brief   Error reporting for errors detected during the command line parameter
 *          processing and binary file data processing.
 ******************************************************************************/

#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "errors.h"
#include "process_bin_data.h"
#include "files.h"
#include "print_message.h"
#include "print_helper.h"
#include "utf8_helpers.h"


/**
 * @brief Report a fatal error in case when the output files have not been created yet
 *
 * @param error_message Text with the error message
 * @param exit_code Value for the exit() function
 */

__declspec(noreturn) void report_error_and_exit(const char *error_message, int exit_code)
{
    char text[MAX_UTF8_TEXT_LENGTH];

    snprintf(text, MAX_UTF8_TEXT_LENGTH, TXT_RTE_ERROR, exit_code);
    utf8_print_string(text, 0);
    utf8_print_string(error_message, 0);

    if (errno != 0)
    {
        wprintf_s(L" [%s]", _wcserror(errno));
    }

    wprintf(L"\n\n");

    _fcloseall();
    exit(exit_code);
}


/**
 * @brief Report an error found during command line parameter processing,
 *        show the basic instructions and exit the application.
 *
 * @param error_message  Text with the error message
 * @param msg_extension  Additional error information
 */

__declspec(noreturn) void report_error_and_show_instructions(const char *error_message, const char *msg_extension)
{
    char text[MAX_UTF8_TEXT_LENGTH];

    if (msg_extension == NULL)
    {
        snprintf(text, MAX_UTF8_TEXT_LENGTH, "%s\n", error_message);
        utf8_print_string(text, 0);
    }
    else
    {
        snprintf(text, MAX_UTF8_TEXT_LENGTH, "%s: '%s'.\n", error_message, msg_extension);
        utf8_print_string(text, 0);
    }

    printf("\n\n");
    snprintf(text, MAX_UTF8_TEXT_LENGTH,
        TXT_MSG_RTEMSG_VERSION, RTEMSG_VERSION, RTEMSG_SUBVERSION, RTEMSG_REVISION, __DATE__);
    utf8_print_string(text, 0);
    snprintf(text, MAX_UTF8_TEXT_LENGTH, RTEMSG_INSTRUCTIONS);
    utf8_print_string(text, 0);
    exit(EXIT_FATAL_ERR_BAD_PARAMETERS);
}


/**
 * @brief Report a fatal error, close all open files and terminate the application.
 *        Such errors are reported to the Error.log file only.
 *
 * @param error_code        Error number
 * @param additional_text   Additional error related description
 * @param additional_data   Additional error information
 *                          If this parameter is ~1uLL, then display operating system error info also.
 */

__declspec(noreturn) void report_fatal_error_and_exit(uint32_t error_code,
    const char *additional_text, size_t additional_data)
{
    if ((error_code >= TOTAL_ERRORS) || (error_code < FIRST_FATAL_ERROR))
    {
        error_code = FATAL_LAST;        // Unknown error
    }

    if (additional_text == NULL)
    {
        additional_text = "";
    }

    const char *p_err_message = get_message_text(error_code);

    if (g_msg.file.error_log != NULL)
    {
        fprintf(g_msg.file.error_log, "ERR_%03u: ", error_code);
    }

    char text[MAX_UTF8_TEXT_LENGTH];
    snprintf(text, MAX_UTF8_TEXT_LENGTH, TXT_RTE_ERROR, error_code);
    utf8_print_string(text, 0);

    const char *percent_position = strchr(p_err_message, '%'); // NULL if '%' character not found

    if (percent_position == NULL)
    {
        if (g_msg.file.error_log != NULL)
        {
            fprintf(g_msg.file.error_log, "%s", p_err_message);
        }

        snprintf(text, MAX_UTF8_TEXT_LENGTH, "%s", p_err_message);
        utf8_print_string(text, 0);
    }
    else
    {
        if (g_msg.file.error_log != NULL)
        {
            fprintf(g_msg.file.error_log, p_err_message, additional_text, additional_data);
        }

        snprintf(text, MAX_UTF8_TEXT_LENGTH, p_err_message, additional_text, additional_data);
        utf8_print_string(text, 0);

        if (additional_data == ~1uLL)
        {
            if (g_msg.file.error_log != NULL)
            {
                fprintf(g_msg.file.error_log, ": %s", strerror(errno));
            }

            wprintf_s(L": %s", _wcserror(errno));
        }
    }

    wprintf(L"\n");
    _fcloseall();
    exit(error_code);
}


/**
 * @brief Worker function for the report_problem_with_string()
 *        Prints the information to the specified output file.
 *
 * @param out           Pointer to the output file
 * @param error_code    Number of the error for which the information has to be displayed
 * @param name          Additional string to print
 */

static void report_problem_with_string_worker(FILE *out, unsigned int error_code, const char *name)
{
    const char *p_message = get_message_text(error_code);

    fprintf(out, "\n");

    if (g_msg.message_cnt > 0)
    {
        print_message_number(out, g_msg.message_cnt);
        fputc(' ', out);
    }

    fprintf(out, "ERR_%03u: ", error_code);
    fprintf(out, p_message, name);

    if (errno != 0)
    {
        fprintf(out, ": %s", strerror(errno));
    }
}


/**
 * @brief Report an error to the Main.log and Error.log
 *        Additional parameter = string
 *
 * @param error_code    Number of the error for which the information has to be displayed
 * @param name          String with additional information about the problem
 */

void report_problem_with_string(uint32_t error_code, const char *name)
{
    if ((error_code < FIRST_ERROR) || (error_code >= TOTAL_ERRORS))
    {
        error_code = ERR_DECODE_UNKNOWN_ERROR;
    }

    if (g_msg.file.error_log != NULL)
    {
        report_problem_with_string_worker(g_msg.file.error_log, error_code, name);
    }

    if ((g_msg.file.main_log != NULL) && (g_msg.file.main_log != g_msg.file.error_log))
    {
        report_problem_with_string_worker(g_msg.file.main_log, error_code, name);
    }

    g_msg.total_errors++;
    g_msg.error_counter[error_code]++;   // Count errors
    _set_errno(0);
}


/**
 * @brief Worker function for the report_problem2()
 *
 * @param out               Pointer to the output file
 * @param error_code        Number of the error for which the information has to be displayed
 * @param additional_data   Additional error information (part 1)
 * @param additional_data2  Additional error information (part 2)
 */

static void report_problem2_worker(FILE *out,
    uint32_t error_code, uint32_t additional_data, uint32_t additional_data2)
{
    const char *p_message = get_message_text(error_code);

    fprintf(out, "\n");

    if (g_msg.message_cnt > 0)
    {
        print_message_number(out, g_msg.message_cnt);
        fputc(' ', out);
    }

    fprintf(out, "ERR_%03u: ", error_code);

    // Print the format ID name if the format definition for this format ID exists
    print_format_id_name(out);

    fprintf(out, p_message, additional_data, additional_data2);
}


/**
 * @brief Report an error to the Main.log and Error.log
 *        Increment the appropriate error counter.
 *        Handles two additional data parameters for detailed error information.
 *
 * @param error_code        Number of the error for which the information has to be displayed
 * @param additional_data   Additional error information (part 1)
 * @param additional_data2  Additional error information (part 2)
 */

void report_problem2(uint32_t error_code, uint32_t additional_data, uint32_t additional_data2)
{
    if ((error_code < FIRST_ERROR) || (error_code >= TOTAL_ERRORS))
    {
        error_code = ERR_DECODE_UNKNOWN_ERROR;
    }

    if (g_msg.file.error_log != NULL)
    {
        report_problem2_worker(g_msg.file.error_log, error_code, additional_data, additional_data2);
    }

    if ((g_msg.file.main_log != NULL) && (g_msg.file.main_log != g_msg.file.error_log))
    {
        report_problem2_worker(g_msg.file.main_log, error_code, additional_data, additional_data2);
    }

    g_msg.total_errors++;
    g_msg.error_counter[error_code]++;   // Increment error count for the specific error code
}


/**
 * @brief Worker function for the report_problem()
 *        Handles error reporting with optional system error message.
 *
 * @param out               Pointer to the output file
 * @param error_code        Number of the error for which the information has to be displayed
 * @param additional_data   Additional error information (-1 = show system error)
 */

static void report_problem_worker(FILE *out, uint32_t error_code, int additional_data)
{
    const char *p_message = get_message_text(error_code);

    fprintf(out, "\n");

    if (g_msg.message_cnt > 0)
    {
        print_message_number(out, g_msg.message_cnt);
        fputc(' ', out);
    }

    if (error_code == ERR_MESSAGE_TOO_LONG)
    {
        print_format_id_name(out);
    }

    fprintf(out, "ERR_%03u: ", error_code);

    const char *percent_position = strchr(p_message, '%'); // Check for '%' in message

    if (percent_position == NULL)
    {
        fprintf(out, p_message);
    }
    else
    {
        fprintf(out, p_message, additional_data);
    }

    if ((additional_data == -1) && (errno != 0))
    {
        fprintf(out, ": %s", strerror(errno));
    }
}


/**
 * @brief Report an error to the Main.log and Error.log
 *        Increment the appropriate error counter.
 *        If message string contains '%', then use the additional value also.
 *        If additional_data == -1, print system error message also.
 *
 * @param error_code       Number of the error for which the information has to be displayed
 * @param additional_data  Additional error information (-1 = show system error)
 */

void report_problem(uint32_t error_code, int additional_data)
{
    if ((error_code >= TOTAL_ERRORS) || (error_code < FIRST_FATAL_ERROR))
    {
        error_code = FATAL_LAST;        // Unknown error
    }

    if (g_msg.file.error_log != NULL)
    {
        report_problem_worker(g_msg.file.error_log, error_code, additional_data);
    }

    if ((g_msg.file.main_log != NULL) && (g_msg.file.main_log != g_msg.file.error_log))
    {
        report_problem_worker(g_msg.file.main_log, error_code, additional_data);
    }

    g_msg.total_errors++;
    g_msg.error_counter[error_code]++;   // Increment error count for the specific error code
    _set_errno(0);                       // Reset errno after handling
}


/**
 * @brief Write error and warning counters to the main and error log files.
 *        Provides a summary of errors and warnings detected.
 */

void report_decode_error_summary(void)
{
    // Print error and warning summary to the main and error log files
    fprintf(g_msg.file.main_log, "\n\n");

    if (g_msg.total_errors == 0)
    {
        fprintf(g_msg.file.main_log, "%s", get_message_text(MSG_NO_ERRORS_DETECTED));
    }
    else
    {
        fprintf(g_msg.file.main_log, get_message_text(MSG_TOTAL_ERRORS), g_msg.total_errors);
    }

    if (g_msg.total_errors > 0)
    {
        fprintf(g_msg.file.error_log, "%s", get_message_text(MSG_ERROR_SUMMARY));

        // Print information about detected errors to the error log file only
        for (uint32_t i = FIRST_ERROR; i < TOTAL_ERRORS; i++)
        {
            if (g_msg.error_counter[i] > 0)
            {
                fputc('\n', g_msg.file.error_log);
                fprintf(g_msg.file.error_log,
                    get_message_text(MSG_ERROR_COUNTER),
                    g_msg.error_counter[i],
                    i,
                    get_message_text(i));
            }
        }
    }

    if (g_msg.total_errors != 0)
    {
        fprintf(g_msg.file.error_log, "\n\n");
    }

    if (g_msg.total_errors == 0)
    {
        fprintf(g_msg.file.error_log, "%s", get_message_text(MSG_NO_ERRORS_DETECTED));
    }
    else
    {
        fprintf(g_msg.file.error_log, get_message_text(MSG_TOTAL_ERRORS), g_msg.total_errors);
    }

#if RTEMSG_DEBUG_MODE == 1
    if (g_msg.param.debug)
    {
        fprintf(g_msg.file.error_log, "\n\n--- ERROR MESSAGE LIST ---");

        for (unsigned i = FIRST_FATAL_ERROR; i < TOTAL_ERRORS; i++)
        {
            fprintf(g_msg.file.error_log, "\nERR_%03u: \"%s\"", i, get_message_text(i));
        }

        fprintf(g_msg.file.error_log, "\n-----------------------\n");
    }
#endif /* if RTEMSG_DEBUG_MODE == 1 */
}

/*==== End of file ====*/
