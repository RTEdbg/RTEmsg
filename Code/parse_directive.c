/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    parse_directive.c
 * @authors S. Milivojcev, B. Premzel
 * @brief   Handles parsing of RTEmsg directives in format definition files.
 ******************************************************************************/

#include "pch.h"
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <Windows.h>
#include "parse_directive_helpers.h"
#include "parse_fmt_string.h"
#include "parse_directive.h"
#include "parse_directive_msg.h"
#include "parse_file_handling.h"
#include "parse_error_reporting.h"
#include "files.h"
#include "errors.h"
#include "decoder.h"


/**
 * @brief Ensures there is enough stack space remaining.
 *        This function is used in potentially recursive functions.
 */

__declspec(noinline) void check_stack_space(void)
{
    ULONG_PTR low_limit, high_limit;
    GetCurrentThreadStackLimits(&low_limit, &high_limit);
    ULONG_PTR current_low_addr = (ULONG_PTR)(&low_limit);
    ULONG_PTR remaining = current_low_addr - low_limit;

    // Check stack integrity - remaining space should not exceed total stack size
    // or be critically low (< 5KB)
    if ((remaining > (high_limit - low_limit)) || (remaining < 5000))
    {
        __fastfail(EXIT_FAST_FAIL_INCORRECT_STACK);
    }

    if (remaining < MIN_STACK_SPACE)
    {
        report_fatal_error_and_exit(FATAL_STACK_LOW, "", (size_t)remaining);
    }
}


/**
 * @brief Resets the parse handle to its default state.
 *
 * @param parse_handle   Pointer to the main parse handle structure.
 */

static void reset_parse_handle(parse_handle_t *parse_handle)
{
    parse_handle->p_new_message = NULL;
    parse_handle->found.in_file_select = false;
    parse_handle->found.out_file_select = false;
}


/**
 * @brief Validates format parsing and resets the parse handle.
 *        Should be invoked at the start of each keyword parsing.
 *
 * @param parse_handle    Pointer to the main parse handle structure.
 */

static void check_and_reset_fmt_parsing(parse_handle_t *parse_handle)
{
    check_if_the_last_msg_is_empty(parse_handle);
    reset_parse_handle(parse_handle);
    parse_handle->p_current_message = NULL;
}


/**
 * @brief Checks and skips a closing bracket ')'.
 *        Ensures no extra text follows the closing bracket.
 *
 * @param parse_handle   Pointer to the main parse handle structure.
 * @param position       Current position in text, updated after skipping.
 */

static void check_closing_bracket(parse_handle_t *parse_handle, char **position)
{
    skip_whitespace(position);

    if (*(*position) != ')')
    {
        catch_parsing_error(parse_handle, ERR_PARSE_NO_CLOSING_BRACKET, *position);
    }

    (*position)++;
    skip_whitespace(position);

    if (*(*position) != '\0')
    {
        catch_parsing_error(parse_handle, ERR_PARSE_SURPLUS_TEXT, *position);
    }
}


/**
 * @brief Verifies and skips an opening bracket '('.
 *
 * @param parse_handle   Pointer to the main parse handle structure.
 * @param position       Current position in text, updated after skipping.
 */

static void check_opening_bracket(parse_handle_t *parse_handle, char **position)
{
    skip_whitespace(position);

    if (*(*position) != '(')
    {
        catch_parsing_error(parse_handle, ERR_PARSE_NO_OPENING_BRACKET, *position);
    }

    (*position)++;
    skip_whitespace(position);
}


/**
 * @brief Ensures the last message in the file or before the INCLUDE() directive
 *        has a format definition.
 *
 * @param parse_handle   Pointer to the main parse handle structure.
 */

void check_if_the_last_msg_is_empty(parse_handle_t *parse_handle)
{
    if ((parse_handle->p_current_message != NULL)
         && (parse_handle->p_current_message->format->fmt_string == NULL))
    {
        report_parsing_error(parse_handle,
            ERR_PARSE_MSG_EMPTY, parse_handle->p_current_message->message_name);
    }
}


/**
 * @brief Checks if the current line contains a C-style comment.
 *        The comment must be closed within the same line.
 *
 * @param parse_handle   Pointer to the handle of the currently parsed file.
 * @param file_line      Buffer containing the line read from the format file.
 * @return               true if the line is a comment, false otherwise.
 */

static inline bool is_commented_out(parse_handle_t *parse_handle, char *file_line)
{
    char *start = file_line;
    char *end = start + strlen(start) - 1;

    while ((end > start) && isspace((unsigned char)*end))
    {
        end--;
    }

    end--;

    size_t len = strlen(start);

    if (len == 0)
    {
        return true;
    }

    if (len <= 3)
    {
        return false;
    }

    if (strncmp(start, "/*", 2) != 0)
    {
        return false;
    }

    if ((end[1] != '/') || (end[0] != '*'))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_UNFINISHED_COMMENT, start);
    }

    return true;
}


/**
 * @brief Parses the MEMO(NAME, Optional initial value) directive.
 *        Defines a memo variable with an optional initial value.
 *
 * @param parse_handle  Pointer to the parse handle for format definitions.
 */

static void parse_memo(parse_handle_t *parse_handle)
{
    g_msg.enums[g_msg.enums_found].type = MEMO_TYPE;
    char **position = parse_handle->p_file_line_curr_pos;
    check_and_reset_fmt_parsing(parse_handle);

    ADVANCE_POSITION(position, "MEMO");
    check_opening_bracket(parse_handle, position);
    g_msg.enums[g_msg.enums_found].name = parse_directive_name(parse_handle, "M_");

    if (*(*position) == ',')
    {
        (*position)++;  // Skip the ',' character

        // Parse the initial value for the memo
        char *end_position = NULL;
        double memo_init_val = strtod(*position, &end_position);

        if (*position == end_position)  // No valid number was parsed
        {
            catch_parsing_error(parse_handle, ERR_PARSE_MEMO_INIT_VAL, *position);
        }

        *position = end_position;
        g_msg.enums[g_msg.enums_found].memo_value = memo_init_val;
    }

    check_closing_bracket(parse_handle, position);
    g_msg.enums_found++;
}


/**
 * @brief Parses the formatting string. Processes text between quotes,
 *        handles escape sequences, and splits text into sub-strings.
 *        Necessary for printing one value at a time during binary data file processing.
 *
 * @param parse_handle  Pointer to the parse handle for format definitions.
 */

static void parse_fmt_text(parse_handle_t *parse_handle)
{
    char **position = parse_handle->p_file_line_curr_pos;
    char *text_after_quote = (*position) + 1;
    bool quote_found = false;

    // Skip the initial quote and locate the closing quote
    for (char *p = (*position) + 1; *p != '\0'; p++)
    {
        int c = (unsigned char)*p;

        if (c == '\\')
        {
            char next_char = *(p + 1);

            if ((next_char == '"') || (next_char == '\\'))
            {
                p++;
            }
        }
        else if (c == '"')
        {
            *p = '\0';
            *position = p + 1;
            quote_found = true;
            break;
        }
    }

    if (!quote_found)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_INVALID_TEXT, *position);
    }

    process_escape_sequences(text_after_quote, MAX_INPUT_LINE_LENGTH - 1);
    separate_fmt_strings(text_after_quote, parse_handle);

    // Reset the selected files (files to which the print output will be directed)
    parse_handle->current_in_file_idx = 0;
    parse_handle->current_out_file_idx = 0;
    parse_handle->print_to_main_log = false;

    // Ensure no text remains after the closing quote
    skip_whitespace(position);

    if (*(*position) != '\0')
    {
        catch_parsing_error(parse_handle, ERR_PARSE_SURPLUS_TEXT, *position);
    }
}


/**
 * @brief Parses the input file selection directive: <INFILE
 *        Specifies which input file to read from.
 *
 * @param parse_handle  Pointer to the parse handle for format definitions.
 */

static void parse_select_in_file(parse_handle_t *parse_handle)
{
    if (parse_handle->found.in_file_select)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_SELECT_IN_FILE_MULTIPLE_IN_LINE,
            *parse_handle->p_file_line_curr_pos);
    }

    parse_handle->found.in_file_select = true;

    char **position = parse_handle->p_file_line_curr_pos;
    ADVANCE_POSITION(position, ">");

    static char parsed_name[MAX_NAME_LENGTH];
    parse_name(parse_handle, parsed_name);

    if (parse_handle->p_current_message == NULL)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_IN_OUT_SELECT_NO_MSG, *position);
    }

    parse_handle->current_in_file_idx = find_enum_idx(parsed_name, IN_FILE_TYPE);

    if (!parse_handle->current_in_file_idx)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_IN_SELECT_UNDEFINED, parsed_name);
    }
}


/**
 * @brief Parses output redirection using ">" or ">>".
 *        ">" redirects output to the specified file only.
 *        ">>" redirects to the specified file and Main.log.
 *
 * @param parse_handle  Pointer to the parse handle for format definitions.
 */

static void parse_select_out_file(parse_handle_t *parse_handle)
{
    if (parse_handle->found.out_file_select)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_SELECT_OUT_FILE_MULTIPLE_IN_LINE,
            *parse_handle->p_file_line_curr_pos);
    }

    parse_handle->found.out_file_select = true;

    char **position = parse_handle->p_file_line_curr_pos;
    skip_whitespace(position);
    bool double_greater_than_sign = false;

    (*position)++;          // Skip the ">" character

    if (*(*position) == '>')
    {
        (*position)++;      // Skip the second ">" character
        double_greater_than_sign = true;
    }

    static char parsed_name[MAX_NAME_LENGTH];
    parse_name(parse_handle, parsed_name);

    if (parse_handle->p_current_message == NULL)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_IN_OUT_SELECT_NO_MSG, *position);
    }

    parse_handle->current_out_file_idx = find_enum_idx(parsed_name, OUT_FILE_TYPE);

    if (!parse_handle->current_out_file_idx)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_OUT_SELECT_UNDEFINED, parsed_name);
    }

    if (double_greater_than_sign)
    {
        parse_handle->print_to_main_log = true;
    }
}


/**
 * @brief Parses the FMT_ALIGN(format_id) directive.
 *        Sets a new starting value for the format ID search, aligning it to the specified value.
 *        The new value is the old value rounded up to the alignment value.
 *
 * @param parse_handle  Pointer to the format definitions parse handle.
 */

static void parse_fmt_align(parse_handle_t *parse_handle)
{
    check_and_reset_fmt_parsing(parse_handle);
    char **position = parse_handle->p_file_line_curr_pos;
    ADVANCE_POSITION(position, "FMT_ALIGN");
    check_opening_bracket(parse_handle, position);

    unsigned new_align_value = parse_unsigned_int(parse_handle);

    if (new_align_value > (unsigned)g_msg.hdr_data.topmost_fmt_id)
    {
        g_msg.total_errors = MAX_ERRORS_REPORTED - 1;   // Fatal error: stop parsing
        catch_parsing_error(parse_handle, ERR_PARSE_FMT_ALIGN_OVER_MAX, NULL);
    }

    if (!is_power_of_two((size_t)new_align_value))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_FMT_ALIGN_PWR_OF_2, NULL);
    }

    g_msg.fmt_ids_defined = (g_msg.fmt_ids_defined + (new_align_value - 1)) & ~(new_align_value - 1);
    g_msg.fmt_align_value = g_msg.fmt_ids_defined;

    check_closing_bracket(parse_handle, position);
}


/**
 * @brief Parses the FMT_START(format_id) directive.
 *        Sets a new starting value for the format ID search, ensuring it is not less than the last assigned ID.
 *
 * @param parse_handle  Pointer to the format definitions parse handle.
 */

static void parse_fmt_start(parse_handle_t *parse_handle)
{
    check_and_reset_fmt_parsing(parse_handle);
    char **position = parse_handle->p_file_line_curr_pos;
    ADVANCE_POSITION(position, "FMT_START");
    check_opening_bracket(parse_handle, position);

    unsigned new_fmt_position = parse_unsigned_int(parse_handle);

    if (new_fmt_position >= g_msg.hdr_data.topmost_fmt_id)
    {
        g_msg.total_errors = MAX_ERRORS_REPORTED - 1;   // Fatal error: stop parsing
        catch_parsing_error(parse_handle, ERR_PARSE_FMT_ALIGN_OVER_MAX, NULL);
    }

    if (g_msg.fmt_ids_defined > new_fmt_position)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_FMT_START_ALIGNMENT, NULL);
    }

    g_msg.fmt_ids_defined = new_fmt_position;
    g_msg.fmt_align_value = new_fmt_position;

    check_closing_bracket(parse_handle, position);
}


/**
 * @brief Parses the FILTER(NAME, "Optional description") directive.
 *        Defines a message filter with an optional description.
 *
 * @param parse_handle  Pointer to the format definitions parse handle.
 */

static void parse_filter(parse_handle_t *parse_handle)
{
    unsigned filter_no = g_msg.filter_enums;
    g_msg.enums[filter_no].type = FILTER_TYPE;
    char **position = parse_handle->p_file_line_curr_pos;

    check_and_reset_fmt_parsing(parse_handle);

    if (filter_no >= NUMBER_OF_FILTER_BITS)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_FILTER_MAX_ENUMS, *position);
    }

    ADVANCE_POSITION(position, "FILTER");
    check_opening_bracket(parse_handle, position);
    g_msg.enums[filter_no].name = parse_directive_name(parse_handle, "F_");

    if (*(*position) == ',')
    {
        (*position)++;  // Skip ','

        char filter_descr[MAX_NAME_LENGTH];

        if (!parse_quoted_arg(position, filter_descr, MAX_NAME_LENGTH - 1))
        {
            catch_parsing_error(parse_handle, ERR_PARSE_FILTER_DESC, *position);
        }

        if (*filter_descr == '\0')  // Empty description?
        {
            catch_parsing_error(parse_handle, ERR_PARSE_FILTER_DESC, *position);
        }

        process_escape_sequences(filter_descr, MAX_NAME_LENGTH);
        g_msg.enums[filter_no].filter_description = duplicate_string(filter_descr);
    }

    check_closing_bracket(parse_handle, position);
    g_msg.filter_enums++;
}


/**
 * @brief Parses the INCLUDE("Format_file") directive.
 *        Recursively includes and parses another format definition file.
 *
 * @param parse_handle  Pointer to the data structure for the current file parsing.
 */

static void parse_include(parse_handle_t *parse_handle)
{
    char **position = parse_handle->p_file_line_curr_pos;
    char file_path[MAX_FILEPATH_LENGTH];

    check_and_reset_fmt_parsing(parse_handle);
    ADVANCE_POSITION(position, "INCLUDE");
    check_opening_bracket(parse_handle, position);
    parse_file_path_arg(parse_handle, file_path, MAX_FILEPATH_LENGTH);

    // Recursively call the parse file function for every included file
    parse_fmt_file(file_path, parse_handle);

    // Reset the handle to detect if a formatting text without a MSG_xx name is after the INCLUDE()
    reset_parse_handle(parse_handle);
    check_closing_bracket(parse_handle, position);
}


/**
 * @brief Checks the OUT_FILE() file mode parameter. Only the following mode characters
 *        are allowed: 'w', 'b', 'a', 'x', 't', and '+'.
 *
 * @param parse_handle  Pointer to the format definitions parse handle.
 * @param file_mode     String with the file mode - e.g., "w" or "wb".
 */

static void check_file_mode(parse_handle_t *parse_handle, const char *file_mode)
{
    size_t len = strlen(file_mode);

    if (len < 1)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_FILE_MODE_EMPTY, NULL);
    }

    for (unsigned i = 0; i < len; i++)
    {
        switch (file_mode[i])
        {
            case 'w':
            case 'a':
            case 'b':
            case 'x':
            case 't':
            case '+':
                break;

            default:
                catch_parsing_error(parse_handle, ERR_PARSE_ERROR_IN_FILE_MODE, file_mode);
        }
    }
}


/**
 * @brief Parses the IN_FILE(NAME, "File name") directive.
 *
 * @param parse_handle  Pointer to the format definitions parse handle.
 */

static void parse_in_file(parse_handle_t *parse_handle)
{
    g_msg.enums[g_msg.enums_found].type = IN_FILE_TYPE;
    char **position = parse_handle->p_file_line_curr_pos;

    check_and_reset_fmt_parsing(parse_handle);
    ADVANCE_POSITION(position, "IN_FILE");
    check_opening_bracket(parse_handle, position);
    g_msg.enums[g_msg.enums_found].name = parse_directive_name(parse_handle, "");

    if (*(*position) != ',')
    {
        catch_parsing_error(parse_handle, ERR_PARSE_EXPECTING_COMMA, *position);
    }

    (*position)++;
    static char file_path[MAX_FILEPATH_LENGTH];
    parse_file_path_arg(parse_handle, file_path, MAX_FILEPATH_LENGTH);
    check_closing_bracket(parse_handle, position);

    // Ensure the IN_FILE() with the same file path hasn't been defined before
    file_name_used_before(parse_handle, file_path, IN_FILE_TYPE);
    g_msg.enums[g_msg.enums_found].file_name = duplicate_string(file_path);

    // Skip reading the file if only syntax check/compile is initiated
    if (g_msg.param.check_syntax_and_compile == 0)
    {
        open_format_folder();
        read_file_to_indexed_text(file_path, parse_handle);
    }

    g_msg.enums_found++;
    parse_handle->p_file_line_curr_pos = position;
}


/**
 * @brief Parses the OUT_FILE(NAME, "File path", "mode", "Optional initial text") directive.
 *        Defines an output file for message logging with optional initial content.
 *
 * @param parse_handle  Pointer to the format definitions parse handle.
 */

static void parse_out_file(parse_handle_t *parse_handle)
{
    g_msg.enums[g_msg.enums_found].type = OUT_FILE_TYPE;
    char **position = parse_handle->p_file_line_curr_pos;

    static char file_path[MAX_FILEPATH_LENGTH];
    char file_mode[MAX_FILE_MODE_LENGTH];
    static char parsedInitText[MAX_INPUT_LINE_LENGTH];
    *parsedInitText = 0;

    check_and_reset_fmt_parsing(parse_handle);
    *position += sizeof("OUT_FILE") - 1;
    check_opening_bracket(parse_handle, position);

    // Parse NAME argument
    g_msg.enums[g_msg.enums_found].name = parse_directive_name(parse_handle, "");

    // Parse file path argument
    skip_whitespace(position);

    if (*(*position) != ',')
    {
        catch_parsing_error(parse_handle, ERR_PARSE_EXPECTING_COMMA, *position);
    }

    (*position)++;
    parse_file_path_arg(parse_handle, file_path, MAX_FILEPATH_LENGTH);

    // Ensure the OUT_FILE() with the same file path hasn't been defined before
    file_name_used_before(parse_handle, file_path, OUT_FILE_TYPE);
    g_msg.enums[g_msg.enums_found].file_name = duplicate_string(file_path);

    // Parse file mode argument
    skip_whitespace(position);

    if (*(*position) != ',')
    {
        catch_parsing_error(parse_handle, ERR_PARSE_EXPECTING_COMMA, *position);
    }

    (*position)++;

    if (!parse_quoted_arg(position, file_mode, MAX_FILE_MODE_LENGTH))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_FILE_MODE, *position);
    }

    check_file_mode(parse_handle, file_mode);

    // Parse optional initial text (e.g., for a CSV file)
    skip_whitespace(position);

    if (*(*position) == ',')
    {
        (*position)++;

        if (!parse_quoted_arg(position, parsedInitText, MAX_INPUT_LINE_LENGTH - 1))
        {
            catch_parsing_error(parse_handle, ERR_PARSE_OUT_FILE_INIT_TEXT, *position);
        }
    }

    check_closing_bracket(parse_handle, position);

    // Skip file creation if only formatting definitions are checked and compiled
    if (g_msg.param.check_syntax_and_compile == 0)
    {
        open_output_folder();
        FILE *new_file = create_file(file_path, parsedInitText, file_mode);

        if (new_file == NULL)
        {
            catch_parsing_error(parse_handle, ERR_PARSE_OUT_NOT_CREATED, file_path);
        }

        g_msg.enums[g_msg.enums_found].p_file = new_file;
    }

    g_msg.enums_found++;
    parse_handle->p_file_line_curr_pos = position;
}


/**
 * @brief Parses the RTEmsg directives and formatting text.
 *        Prioritizes the most frequent directives for faster processing.
 *
 * @param parse_handle  Pointer to the format definitions parse handle.
 */

static void parse_directive(parse_handle_t *parse_handle)
{
    check_stack_space();    // Ensure sufficient stack space for recursive calls

    skip_whitespace(parse_handle->p_file_line_curr_pos);
    char *fmt_text = *parse_handle->p_file_line_curr_pos;
    parse_handle->err_position = fmt_text;

    if (*fmt_text == '\"')
    {
        parse_fmt_text(parse_handle);   // Parse the formatting text
    }
    else if (strncmp(fmt_text, "MEMO", sizeof("MEMO") - 1) == 0)
    {
        parse_memo(parse_handle);
    }
    else if (*fmt_text == '>')
    {
        parse_select_out_file(parse_handle);
    }
    else if (*fmt_text == '<')
    {
        parse_select_in_file(parse_handle);
    }
    else if ((*fmt_text == 'M') || (*fmt_text == 'E'))
    {
        parse_msg_directives(parse_handle);
    }
    else if (strncmp(fmt_text, "FILTER", sizeof("FILTER") - 1) == 0)
    {
        parse_filter(parse_handle);
        write_define_to_work_file(parse_handle,
            g_msg.enums[g_msg.filter_enums - 1].name,
            g_msg.filter_enums - 1
        );
    }
    else if (strncmp(fmt_text, "INCLUDE", sizeof("INCLUDE") - 1) == 0)
    {
        parse_include(parse_handle);
    }
    else if (strncmp(fmt_text, "OUT_FILE", sizeof("OUT_FILE") - 1) == 0)
    {
        parse_out_file(parse_handle);
    }
    else if (strncmp(fmt_text, "IN_FILE", sizeof("IN_FILE") - 1) == 0)
    {
        parse_in_file(parse_handle);
    }
    else if (strncmp(fmt_text, "FMT_ALIGN", sizeof("FMT_ALIGN") - 1) == 0)
    {
        parse_fmt_align(parse_handle);
    }
    else if (strncmp(fmt_text, "FMT_START", sizeof("FMT_START") - 1) == 0)
    {
        parse_fmt_start(parse_handle);
    }
    else
    {
        catch_parsing_error(parse_handle, ERR_PARSE_UNRECOGNIZED_DIRECTIVE, fmt_text);
    }

    skip_whitespace(parse_handle->p_file_line_curr_pos);

    if (*(*parse_handle->p_file_line_curr_pos) != '\0')     // Check for extra text in the line
    {
        parse_directive(parse_handle);
    }

    reset_parse_handle(parse_handle);
}


/**
 * @brief Parses a line from the format definition files.
 *        Ignores lines with C-style comments and C directives like #define/#if/#endif.
 *
 * @param parse_handle  Pointer to the current file's parse handle.
 * @param file_line     Pointer to the line of data from the format definition file.
 */

static void parse_input_line(parse_handle_t *parse_handle, char *file_line)
{
    if (strlen(file_line) >= (MAX_INPUT_LINE_LENGTH - 4))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_LINE_TOO_LONG, "");
    }

    char *pos = file_line;
    skip_whitespace(&pos);

    if (*pos == '#')    // Ignore C directives added by RTEmsg
    {
        if (!parse_handle->write_output_to_header)
        {
            return;         // Skip #if/#define added to the header file
        }

        catch_parsing_error(parse_handle, ERR_PARSE_C_DIRECTIVES_NOT_ALLOWED, pos);
    }

    if (g_msg.param.check_syntax_and_compile)
    {
        fprintf(parse_handle->p_fmt_work_file, "%s", file_line);
    }

    if (is_commented_out(parse_handle, pos))
    {
        return;
    }

    if ((*pos != '/') || (*(pos + 1) != '/'))   // Ensure formatting definitions start with '//'
    {
        catch_parsing_error(parse_handle, ERR_PARSE_UNRECOGNIZED_DIRECTIVE, file_line);
    }

    pos += 2;       // Skip the '//'
    parse_handle->p_file_line_curr_pos = &pos;
    parse_directive(parse_handle);
    parse_handle->p_file_line_curr_pos = NULL;
}


/**
 * @brief Resets the format of the current message to default values if an error is reported.
 *        This helps prevent further errors for the same MSG directive.
 *
 * @param parse_handle  Pointer to the parse handle of the current file.
 */

static void set_default_fmt(parse_handle_t *parse_handle)
{
    if ((parse_handle->p_current_message == NULL) || 
        (parse_handle->p_current_message->format == NULL) || 
        (parse_handle->p_current_message->format->fmt_string != NULL))
    {
        return;
    }

    parse_handle->p_current_message->format->fmt_type = PRINT_PLAIN_TEXT;
    parse_handle->p_current_message->format->fmt_string = "";
}


/**
 * @brief Parses the format definition file line by line.
 *
 * @param filepath  Path to the format definition file.
 * @param parent_parse_handle  Pointer to the parent parse handle for nested includes, or NULL if top-level.
 */

void parse_fmt_file(const char *filepath, parse_handle_t *parent_parse_handle)
{
    parse_handle_t parse_handle = { 0 };
    parse_handle.fmt_file_path = filepath;
    parse_handle.p_parse_parent = &parse_handle;

    if (parent_parse_handle != NULL)
    {
        parse_handle.p_parse_parent = parent_parse_handle;
    }

    char file_line[MAX_INPUT_LINE_LENGTH];
    check_stack_space();    // Ensure there is enough stack space for recursive calls

    if (!setup_parse_files(&parse_handle))
    {
        return;     // Failed to open the format definition file
    }

    if (parse_handle.p_fmt_file == NULL)
    {
        return;
    }

    if (setjmp(parse_handle.jump_point))
    {
        // Control returns here after an error is reported by 'catch_parsing_error()'
        reset_parse_handle(&parse_handle);
        set_default_fmt(&parse_handle);
    }

    for ( ;; )
    {
        if (g_msg.total_errors >= MAX_ERRORS_REPORTED)
        {
            parse_handle.parsing_errors_found = true;
            break;
        }

        _set_errno(0);

        if (fgets(file_line, MAX_INPUT_LINE_LENGTH, parse_handle.p_fmt_file) == NULL)
        {
            file_line[0] = '\0';

            if (errno != 0)
            {
                report_parsing_error(parse_handle.p_parse_parent,
                    ERR_PARSE_READ_FROM_FMT_FILE, parse_handle.fmt_file_path);
            }

            break;
        }

        ++parse_handle.file_line_num;
        parse_input_line(&parse_handle, file_line);
    }

    check_if_the_last_msg_is_empty(&parse_handle);

    // If syntax check/compile is enabled, copy work file to original if they differ, and close files
    if (g_msg.param.check_syntax_and_compile && (parse_handle.p_fmt_work_file != NULL))
    {
        fprintf(parse_handle.p_fmt_work_file, "#endif\n");
        check_and_replace_work_file(&parse_handle);
    }
    else if (parse_handle.p_fmt_file != NULL)
    {
        fclose(parse_handle.p_fmt_file);
    }
}

/*==== End of file ====*/
