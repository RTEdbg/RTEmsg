/*
 * Copyright (c) 2024 Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    parse_error_reporting.c
 * @authors B. Premzel, S. Milivojcev
 * @brief   Error reporting during the format definition file processing.
 *          The error reporting is configurable. See the manual - command line options.
 ******************************************************************************/

#include "pch.h"
#include <Windows.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "parse_error_reporting.h"
#include "utf8_helpers.h"
#include "main.h"
#include "files.h"


/**
 * @brief Outputs additional error information.
 *        Truncates the text to a specified byte limit, ensuring proper UTF-8 handling.
 *
 * @param additional_info  Contextual information about the error location in the parsed text.
 */

static void print_additional_info(const char *additional_info)
{
    size_t size = strlen(additional_info);
    char *text = allocate_memory(size + 1, "tmp");
    strcpy(text, additional_info);

    // Replace non-printable characters with spaces
    for (size_t i = 0; i < size; i++)
    {
        if ((text[i] >= 0) && (text[i] < ' '))
        {
            text[i] = ' ';
        }
    }

    size_t end = utf8_truncate(text, MAX_NO_OF_CHARS_PRINTED_FOR_ADDINFO_REPORTING);
    utf8_print_string(text, end);
    fprintf(g_msg.file.error_log, "%.*s", (int)end, text);
    free(text);
}


/**
 * @brief Outputs the full path of files located in the FMT directory.
 *        Falls back to the short file name if the full path is unavailable.
 *
 * @param file_name  The file name excluding its full path.
 */

static void print_full_path_info(const char *file_name)
{
    open_format_folder();

    char full_name[_MAX_PATH];

    if (_fullpath(full_name, file_name, _MAX_PATH) == NULL)
    {
        // Fallback to short filename if full path retrieval fails
        utf8_print_string(file_name, 0);
        fprintf(g_msg.file.error_log, "%s/%s", g_msg.param.fmt_folder, file_name);
        return;
    }
    
    if (g_msg.param.codepage_utf8)
    {
        utf8_print_string(full_name, 0);
    }
    else
    {
        wchar_t w_file_name[_MAX_PATH];
        wchar_t w_full_name[_MAX_PATH];
        int converted = MultiByteToWideChar(CP_UTF8, 0,
            file_name, (int)strlen(file_name), w_file_name, (int)_MAX_PATH);
        w_file_name[converted] = L'\0';

        if (converted != 0)
        {
            if (_wfullpath(w_full_name, w_file_name, _MAX_PATH) == NULL)
            {
                converted = 0;
            }
        }

        if (converted != 0)
        {
            wprintf(L"%s", w_full_name);
        }
        else
        {
            utf8_print_string(file_name, 0);
        }
    }

    fprintf(g_msg.file.error_log, "%s/%s", g_msg.param.fmt_folder, full_name);
}


/**
 * @brief Outputs the error description and appends the C library error message
 *        if the errno global variable is set.
 *
 * @param err_descr   Error description according to the error number (error index).
 * @param err_number  A copy of errno variable.
 *                    The errno could get a new value during font conversion and because
 *                    of this a copy of errno is made at the start of error printing.
 */

static void print_error_description(const char *err_descr, int err_number)
{
    utf8_print_string(err_descr, 0);
    fprintf(g_msg.file.error_log, "%s", err_descr);

    if (err_number)
    {
        if (g_msg.param.codepage_utf8)
        {
            printf(" [%s]", strerror(err_number));
        }
        else
        {
            wprintf_s(L" [%s]", _wcserror(err_number));
        }

        fprintf(g_msg.file.error_log, " [%s]", strerror(err_number));
    }
}


/**
 * @brief Prints the format definition error information according to the command line
 *        parameter "-e=...." or according to the DEFAULT_ERROR_REPORT.
 *
 * @param parse_handle     Pointer to the parse data structure for the currently parsed file.
 * @param parsing_error    Error number and also number of error message in the Messages.txt file.
 * @param additional_info  Additional information about the error (substring in which the error was detected).
 */

static void print_parsing_error(parse_handle_t *parse_handle,
    error_msg_t parsing_error, const char *additional_info)
{
    const char *err_report_definition = g_msg.param.report_error;     // Error printing definition
    const char *err_descr = get_message_text(parsing_error);
    int err_number = errno;     // The errno could change during false UTF-8 string conversion

    while (*err_report_definition != 0)
    {
        if (*err_report_definition != '%')
        {
            putc(*err_report_definition, stdout);
            putc(*err_report_definition++, g_msg.file.error_log);
            continue;
        }

        err_report_definition++;       // Skip the '%'

        switch (*err_report_definition)
        {
            case 'L':
                printf("%u", parse_handle->file_line_num);
                fprintf(g_msg.file.error_log, "%u", parse_handle->file_line_num);
                break;

            case 'E':
                printf("%u", (parsing_error));
                fprintf(g_msg.file.error_log, "%u", parsing_error);
                break;

            case 'P':
                print_full_path_info(parse_handle->fmt_file_path);
                break;

            case 'F':
                utf8_print_string(parse_handle->fmt_file_path, 0);
                fprintf(g_msg.file.error_log, "%s", parse_handle->fmt_file_path);
                break;

            case 'D':
                print_error_description(err_descr, err_number);
                break;

            case 'A':
                print_additional_info(additional_info);
                break;

            default:
                // Character after the '%' not recognized
                printf("???");
                fprintf(g_msg.file.error_log, "???");
                break;
        }

        err_report_definition++;
    }
}


/**
 * @brief Prints the error information to the console and Errors.log file.
 *        The catch_parsing_error() function does not return. It jumps back to the
 *        parse_fmt_file() function.
 *        Such implementation greatly simplifies the error reporting implementation
 *        since the format definition parsing functions do not have to propagate
 *        the error information back to the main parsing function parse_fmt_file().
 *
 * @param parse_handle    Pointer to the parse data structure for the currently parsed file.
 * @param parsing_error   Error number and also number of error message in the Messages.txt file.
 * @param err_context     Additional information for where the error happened in the parsed text.
 *                       If a NULL value is passed then the previously saved text is used.
 */

void report_parsing_error(parse_handle_t *parse_handle, error_msg_t parsing_error, const char *err_context)
{
    // Set locale to system default during error printing to the console
    setlocale(LC_ALL, "");

    if (g_msg.param.codepage_utf8)
    {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }

    if ((parsing_error >= TOTAL_ERRORS) || (parsing_error < ERR_PARSE_UNKNOWN))
    {
        parsing_error = ERR_PARSE_UNKNOWN;
    }

    parse_handle->parsing_errors_found = true;

    if (err_context == NULL)
    {
        err_context = parse_handle->err_position;

        if (err_context == NULL)
        {
            err_context = "???";
        }
    }

    if (g_msg.total_errors < MAX_ERRORS_REPORTED)
    {
        print_parsing_error(parse_handle, parsing_error, err_context);
    }

    // Set locale "C" - continue to use dots as decimal separators during format file parsing
    setlocale(LC_ALL, "C");

    g_msg.total_errors++;
    g_msg.error_counter[parsing_error]++;   // Count errors
    _set_errno(0);
}


/**
 * @brief See the description for the 'report_parsing_error()' function.
 */

__declspec(noreturn)
void catch_parsing_error(parse_handle_t *parse_handle, error_msg_t parsing_error, const char *err_context)
{
    report_parsing_error(parse_handle, parsing_error, err_context);
    longjmp(parse_handle->jump_point, parsing_error);
}

/*==== End of file ====*/
