/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    print_helper.c
 * @author  B. Premzel
 * @brief   Helper functions for message printing
 ******************************************************************************/

#include "pch.h"
#include <string.h>
#include "print_helper.h"
#include "statistics.h"
#include "files.h"
#include "errors.h"


/**
 * @brief Records error information for the current value being processed.
 *        Errors are initially stored in a buffer and printed after the
 *        entire message decoding is completed. A maximum of MAX_ERRORS_IN_SINGLE_MESSAGE
 *        errors are logged per message. The number of additional numerical values
 *        printed depends on the number of %u parameters provided (uint32_t type).
 *
 * @param err_no        Error code of the reported error.
 * @param data1         Additional information about the error.
 * @param data2         Additional information about the error.
 * @param fmt_text      Text used for formatting the current value.
 */

void save_decoding_error(uint32_t err_no, uint32_t data1, uint32_t data2, const char *fmt_text)
{
    if ((err_no < FIRST_ERROR) || (err_no >= ERR_PARSE_UNKNOWN))
    {
        err_no = ERR_DECODE_UNKNOWN_ERROR;
    }

    g_msg.total_errors++;
    g_msg.error_counter[err_no]++;   // Increment the error count

    if (g_msg.msg_error_counter >= MAX_ERRORS_IN_SINGLE_MESSAGE)
    {
        return;
    }

    g_msg.error_log[g_msg.msg_error_counter].error_number = err_no;
    g_msg.error_log[g_msg.msg_error_counter].value_number = g_msg.error_value_no;
    g_msg.error_log[g_msg.msg_error_counter].data1 = data1;
    g_msg.error_log[g_msg.msg_error_counter].data2 = data2;
    g_msg.error_log[g_msg.msg_error_counter].fmt_text = fmt_text;
    g_msg.msg_error_counter++;
}


/**
 * @brief Records internal errors, which are unexpected during normal code execution.
 *
 * @param sys_error     Error code of the reported internal error.
 * @param data2         Additional information about the error.
 */

void save_internal_decoding_error(uint32_t sys_error, uint32_t data2)
{
    save_decoding_error(ERR_INTERNAL_ERROR, sys_error, data2, "");
}


/**
 * @brief Cleans a string by replacing non-printable characters with tildes
 *        and truncating it if necessary.
 *
 * @param  text      Input string.
 * @param  spec_char Special character to be additionally stripped (if non-zero).
 *
 * @return Pointer to the processed string ready for printing.
 */

const char *strip_newlines_and_shorten_string(const char *text, char spec_char)
{
    static char out_string[MAX_SHORTENED_STRING];

    if (text == NULL)
    {
        out_string[0] = '?';
        out_string[1] = '\0';
    }
    else
    {
        size_t i;

        for (i = 0; i < (MAX_SHORTENED_STRING - 4); i++)
        {
            char c = text[i];

            if (c == '\0')
            {
                break;
            }

            if (c < ' ')    // Replace non-printable characters with tildes
            {
                c = '~';
            }

            // Replace special characters with a single quote to prevent issues
            // with applications like LibreOffice when processing CSV files.
            if (c == spec_char)
            {
                c = '\'';
            }

            out_string[i] = c;
        }

        if (i == (MAX_SHORTENED_STRING - 4))
        {
            for ( ; i < (MAX_SHORTENED_STRING - 1); i++)
            {
                out_string[i] = '.';
            }
        }

        out_string[i] = '\0';
    }

    return out_string;
}


/**
 * @brief Prints decoding errors to the specified output file.
 *        Errors are logged only after the complete decoding of an individual message.
 * 
 * @param out  Pointer to the output file.
 */
 
static void print_decoding_errors_to_file(FILE *out)
{
    if (out == NULL)
    {
        return;
    }

    fprintf(out, "\n");
    print_message_number(out, g_msg.message_cnt);
    fprintf(out, get_message_text(MSG_DECODING_ERRORS_FOUND));

    if (g_msg.msg_error_counter >= MAX_ERRORS_IN_SINGLE_MESSAGE)
    {
        g_msg.msg_error_counter = MAX_ERRORS_IN_SINGLE_MESSAGE;
        fprintf(out, get_message_text(MSG_TOO_MANY_ERRORS_FIRST_SHOWN), MAX_ERRORS_IN_SINGLE_MESSAGE);
    }

    for (unsigned i = 0; i < g_msg.msg_error_counter; i++)
    {
        const char *text = strip_newlines_and_shorten_string(g_msg.error_log[i].fmt_text, 0);

        unsigned err_no = g_msg.error_log[i].error_number;

        if ((err_no < FIRST_ERROR) || (err_no >= ERR_PARSE_UNKNOWN))
        {
            err_no = ERR_DECODE_UNKNOWN_ERROR;
        }

        if (*text == '\0')
        {
            // Log the raw error data (data1 and data2) to report internal errors (ERR_INTERNAL_ERROR)
            fprintf(out, "\n-->#%u ERR_%03u: 0x%X 0x%X",
                g_msg.error_log[i].value_number, err_no,
                g_msg.error_log[i].data1, g_msg.error_log[i].data2);
            continue;
        }

        fprintf(out, "\n-->#%u - \"%s\"\n ERR_%03u: ", g_msg.error_log[i].value_number, text, err_no);

        // Format the error message using the error number
        text = get_message_text(err_no);
        fprintf(out, text, g_msg.error_log[i].data1, g_msg.error_log[i].data2);
    }
}


/**
 * @brief Prints decoding errors collected during the processing of a single message.
 */

void print_decoding_errors(void)
{
    if (g_msg.msg_error_counter != 0)
    {
        if (g_msg.file.main_log != NULL)
        {
            print_decoding_errors_to_file(g_msg.file.main_log);
        }

        if (g_msg.file.error_log != NULL)
        {
            print_decoding_errors_to_file(g_msg.file.error_log);
        }

        g_msg.print_nl_to_main_log = true;
    }
}


/**
 * @brief Prints the message number to the specified output file using
 *        the format string defined by a command line parameter.
 *
 * @param out       Pointer to the output file.
 * @param msg_no    Message number.
 */
 
void print_message_number(FILE *out, uint32_t msg_no)
{
    const char *string_text = "N%05u";

    if (g_msg.param.msg_number_print != NULL)
    {
        string_text = g_msg.param.msg_number_print;
    }

    fprintf(out, string_text, msg_no);
}


/**
 * @brief Prints the timestamp value using the format specified by a command line parameter.
 *
 * @param out       Pointer to the file where the timestamp will be printed.
 * @param timestamp Timestamp value in seconds.
 */
 
void print_timestamp(FILE *out, double timestamp)
{
    fprintf(out, g_msg.param.timestamp_print, timestamp * g_msg.param.time_multiplier);
}


/**
 * @brief Dumps the contents of the current message in hexadecimal format to Main.log.
 *        The format can be either 8-bit or 32-bit hex numbers.
 * @param print_words If true, prints as 32-bit words; if false, prints as 8-bit bytes.
 */

void hex_dump_current_message(bool print_words)
{
    if (g_msg.asm_words == 0)
    {
        return;
    }

    fprintf(g_msg.file.main_log, "\n  >>>");
    const char *fmt_name = "";
    uint32_t code = g_msg.fmt_id;

    if (code < MAX_FMT_IDS)
    {
        if (g_fmt[code] != NULL)
        {
            if (g_fmt[code]->message_name != NULL)
            {
                fmt_name = g_fmt[code]->message_name;
            }
        }
    }

    fprintf(g_msg.file.main_log, get_message_text(MSG_FMT_ID), code);

    if (*fmt_name != '\0')
    {
        fprintf(g_msg.file.main_log, ", %s", fmt_name);
    }

    fprintf(g_msg.file.main_log, get_message_text(MSG_HEX_DUMP));

    if (print_words)
    {
        for (unsigned i = 0; i < g_msg.asm_words; i++)
        {
            fprintf(g_msg.file.main_log, " %08X", g_msg.assembled_msg[i]);
        }
    }
    else
    {
        unsigned char *data = (unsigned char *)&g_msg.assembled_msg[0];

        for (unsigned i = 0; i < (g_msg.asm_words * 4U); i++)
        {
            fprintf(g_msg.file.main_log, " %02X", data[i]);
        }
    }
}


/**
 * @brief Dumps filter descriptions to the file defined by RTE_FILTER_FILE.
 *        If a filter description is not available, the filter name is used instead.
 */

void dump_filter_names_to_file(void)
{
    if (!g_msg.param.check_syntax_and_compile)
    {
        return;
    }

    open_output_folder();
    FILE *out = fopen(RTE_FILTER_FILE, "w");

    if (out == NULL)
    {
        report_problem_with_string(FATAL_CANT_CREATE_FILE, RTE_FILTER_FILE);
        return;
    }

    for (unsigned i = 0; i < NUMBER_OF_FILTER_BITS; i++)
    {
        const char *name = ""; // Default to an empty string if no name or description is available.

        // Use filter description if available, otherwise use the filter name
        if (g_msg.enums[i].filter_description != NULL)
        {
            name = g_msg.enums[i].filter_description;
        }
        else if (g_msg.enums[i].name != NULL)
        {
            name = g_msg.enums[i].name;
        }

        fprintf(out, "%s\n", name);
    }

    fclose(out);
}

/*==== End of file ====*/
