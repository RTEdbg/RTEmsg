/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    parse_fmt_string.c
 * @authors S. Milivojcev, B. Premzel
 * @brief   Parsing of formatting strings in the format definition files.
 ******************************************************************************/

#include "pch.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include "format.h"
#include "parse_fmt_string.h"
#include "parse_directive_helpers.h"
#include "parse_directive.h"
#include "parse_error_reporting.h"


// Address of the first bit of the next value for the message being prepared for printing
static unsigned parse_bit_address;


/**
 * @brief Validates the format definition and sets up the data structure for hex dump print type.
 *
 * @param currentSubstring Pointer to the current formatting sub-string
 * @param parse_handle     Pointer to the handle of the currently parsed file
 * @param len              Length of the formatting sub-string
 */

static void parse_hex_print_fmt_data(char *currentSubstring, parse_handle_t *parse_handle, size_t len)
{
    if (len < 3)    // Example: "%2H"
    {
        catch_parsing_error(parse_handle, ERR_PARSE_TYPE_HEX, currentSubstring);
    }

    value_format_t *current_format = parse_handle->current_format;
    current_format->data_size = 0;

    switch (currentSubstring[len - 2])
    {
        case '1':
            current_format->fmt_type = PRINT_HEX1U;
            break;

        case '2':
            current_format->fmt_type = PRINT_HEX2U;
            break;

        case '4':
            current_format->fmt_type = PRINT_HEX4U;
            break;

        default:
            catch_parsing_error(parse_handle, ERR_PARSE_TYPE_HEX, currentSubstring);
    }

    if (currentSubstring[len - 3] != '%')
    {
        catch_parsing_error(parse_handle, ERR_PARSE_TYPE_ADDITIONAL_FORMATTING, currentSubstring);
    }

    // Remove the special formatting type (RTEdbg specific) since fprintf() does not support it
    currentSubstring[len - 3] = '\0';

    if (current_format->data_type != VALUE_AUTO)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_HEX_PRINT_VALUE_NOT_ALLOWED, NULL);
    }
}


/**
 * @brief Validates if statistics and memo are defined for special types where they are not allowed.
 *
 * @param parse_handle    Pointer to the handle of the currently parsed file
 * @param current_format  Pointer to the currently prepared formatting structure
 */

static void check_bad_definitions_for_the_DWHM_types(parse_handle_t *parse_handle,
    value_format_t *current_format)
{
    if (current_format->get_memo || current_format->put_memo)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_MEMO_NOT_ALLOWED, NULL);
    }

    if (current_format->value_stat != NULL)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_STATISTICS_NOT_ALLOWED, NULL);
    }
}


/**
 * @brief Validates if the value fits into the message and checks alignment for variables
 *        whose values have not been defined using the [nn:mmF] value size/address definition.
 *
 * @param parse_handle   Pointer to the handle of the currently parsed file
 * @param fmt_char       Format type character
 */

static void check_fmt_type_data(parse_handle_t *parse_handle, char fmt_char)
{
    value_format_t *current_format = parse_handle->current_format;

    // Check if the selected text has been defined {...} but not used
    if ((fmt_char != 'Y') && (current_format->in_file > 0))
    {
        if (g_msg.enums[current_format->in_file].type == Y_TEXT_TYPE)
        {
            catch_parsing_error(parse_handle, ERR_PARSE_Y_TEXT_NOT_USED, NULL);
        }
    }

    // Skip address & size check if the data type has a zero size (e.g., for "%t")
    if (current_format->data_size == 0)
    {
        return;
    }

    // Validate if the value fits into the message (based on the defined message size)
    unsigned last_bit_address = current_format->bit_address + current_format->data_size;
    if (((last_bit_address > (parse_handle->p_current_message->msg_len * 8u))
            && (parse_handle->p_current_message->msg_len != 0))
        || ((parse_handle->p_current_message->msg_len == 0)
            && (parse_handle->p_current_message->msg_type == TYPE_MSG0_4)))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_TYPE_MSG_SIZE, NULL);
    }

    // Ensure the address of an AUTO type variable (not defined with [...]) is divisible by 32
    if ((current_format->data_type == VALUE_AUTO) && (current_format->bit_address % 32))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_TYPE_NOT_DIV_32, NULL);
    }
}


/**
 * @brief Ensures the format type is preceded by a '%' and removes it for unsupported types.
 *
 * @param fmt_string     The format string being parsed.
 * @param parse_handle   Pointer to the current file's parse handle.
 * @param len            Length of the format string.
 * @param chk_value_spec Flag to check for value specification.
 */

static void check_and_eliminate_percent(char *fmt_string, parse_handle_t *parse_handle,
    size_t len, bool chk_value_spec)
{
    // Report an error if the extended formatting option contains the formatting field and
    // not just % and type character.
    // Erase the extended info since fprintf() does not recognize it
    if (fmt_string[len - 2] == '%')
    {
        fmt_string[len - 2] = '\0'; // Terminate the substring.
    }
    else
    {
        catch_parsing_error(parse_handle, ERR_PARSE_TYPE_ADDITIONAL_FORMATTING, fmt_string);
    }

    // Check for value specification errors if required.
    if (chk_value_spec && parse_handle->found.value_spec)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_VAL_DEF_NOT_FOR_SPECIAL_FMT, NULL);
    }
}


/**
 * @brief Validates and sets data for %s and %W format types.
 *
 * @param parse_handle  Pointer to the current file's parse handle.
 */

static void check_s_and_W_type_data(parse_handle_t *parse_handle)
{
    value_format_t *current_format = parse_handle->current_format;

    if (current_format->data_type == VALUE_AUTO)
    {
        // Use the entire message content as a string if data type is auto.
        current_format->data_size = 0;
    }
    else if (current_format->data_size & 7u)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_SW_SIZE_NOT_DIVISIBLE_BY_8, NULL);
    }

    // The bit address must be divisible by 8
    if (current_format->bit_address & 7u)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_SW_ADDR_NOT_DIVISIBLE_BY_8, NULL);
    }
}


/**
 * @brief Assigns the format type based on the character found in the format definition.
 *
 * @param currentSubstring Pointer to the current substring.
 * @param parse_handle     Pointer to the current file's parse handle.
 * @param fmt_char         Character representing the format type.
 */

static void fill_in_fmt_type(char *currentSubstring, parse_handle_t *parse_handle, char fmt_char)
{
    size_t len = strlen(currentSubstring);
    value_format_t *current_format = parse_handle->current_format;

    switch (fmt_char)
    {
        case 'd':
        case 'i':
            current_format->fmt_type = PRINT_INT64;
            break;

        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
            current_format->fmt_type = PRINT_DOUBLE;
            break;

        case 't':
            current_format->fmt_type = PRINT_TIMESTAMP;
            current_format->data_size = 0;
            check_and_eliminate_percent(currentSubstring, parse_handle, len, true);
            break;

        case 'T':
            current_format->fmt_type = PRINT_dTIMESTAMP;
            current_format->data_size = 0;
            check_and_eliminate_percent(currentSubstring, parse_handle, len, true);
            break;

        case 'N':
            current_format->fmt_type = PRINT_MSG_NO;
            current_format->data_size = 0;
            check_and_eliminate_percent(currentSubstring, parse_handle, len, true);
            break;

        case 'D':
            current_format->fmt_type = PRINT_DATE;
            current_format->data_size = 0;
            check_and_eliminate_percent(currentSubstring, parse_handle, len, true);
            check_bad_definitions_for_the_DWHM_types(parse_handle, current_format);
            break;

        case 'M':
            current_format->fmt_type = PRINT_MSG_FMT_ID_NAME;
            current_format->data_size = 0;
            check_and_eliminate_percent(currentSubstring, parse_handle, len, true);
            check_bad_definitions_for_the_DWHM_types(parse_handle, current_format);
            break;

        case 'W':
            current_format->fmt_type = PRINT_BIN_TO_FILE;

            if (current_format->data_type == VALUE_AUTO)
            {
                current_format->data_size = 0;
            }

            check_and_eliminate_percent(currentSubstring, parse_handle, len, false);
            check_s_and_W_type_data(parse_handle);
            check_bad_definitions_for_the_DWHM_types(parse_handle, current_format);
            break;

        case 'H':
            parse_hex_print_fmt_data(currentSubstring, parse_handle, len);
            check_bad_definitions_for_the_DWHM_types(parse_handle, current_format);
            break;

        case 'Y':
            current_format->fmt_type = PRINT_SELECTED_TEXT;

            if (parse_handle->current_format->in_file == 0)
            {
                catch_parsing_error(parse_handle, ERR_PARSE_Y_TEXT_UNDEFINED, NULL);
            }

            check_and_eliminate_percent(currentSubstring, parse_handle, len, false);
            break;

        case 'B':
            current_format->fmt_type = PRINT_BINARY;
            check_and_eliminate_percent(currentSubstring, parse_handle, len, false);
            break;

        case 's':
            current_format->fmt_type = PRINT_STRING;
            check_s_and_W_type_data(parse_handle);
            break;

        default:    // 'c', 'o', 'u', 'x', 'X'
            current_format->fmt_type = PRINT_UINT64;
            break;
    }

    check_fmt_type_data(parse_handle, fmt_char);
}


/**
 * @brief Parses the format definition for message timestamp [t], period [T], or message number [N].
 *
 * @param position       Pointer to the current position in the format line being parsed.
 * @param current_format Pointer to the data structure for the current value.
 * @param parse_handle   Pointer to the handle of the currently parsed format definition file.
 * @param type           Type of value to be printed.
 */

static void parse_special_spec(char **position, value_format_t *current_format,
    parse_handle_t *parse_handle, enum data_type_t type)
{
    char *p = (*position) + 2; // Move past the already validated "[T", "[t", or "[N".

    if (*p++ != ']')
    {
        catch_parsing_error(parse_handle, ERR_PARSE_EXPECTING_SQUARE_BRACKET, *position);
    }

    current_format->data_type = type;
    current_format->data_size = 0;
    *position = p;
}


/**
 * @brief Parses the format definition for memory recall [M_NAME].
 *
 * @param position       Pointer to the current position in the format line being parsed.
 * @param current_format Pointer to the data structure for the current value.
 * @param parse_handle   Pointer to the handle of the currently parsed format definition file.
 */

static void parse_memo_recall_spec(char **position, value_format_t *current_format,
    parse_handle_t *parse_handle)
{
    char *p = (*position) + 1; // Move past the '['.
    skip_whitespace(&p);

    char selection[MAX_NAME_LENGTH];

    if (!parse_until_specified_character(&p, selection, MAX_NAME_LENGTH, ']'))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_RECALL_DEFINITION, *position);
    }

    rte_enum_t memo_index = find_enum_idx(selection, MEMO_TYPE);

    if (memo_index == 0)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_INVALID_NAME, selection);
    }

    current_format->get_memo = memo_index;
    current_format->data_type = VALUE_MEMO;
    current_format->data_size = 0;
    *position = p + 1; // Move past the ']'.
}


/**
 * @brief Parses the format definition for relative timestamp calculation [t-MSG_NAME].
 *
 * @param position       Pointer to the current position in the format line being parsed.
 * @param current_format Pointer to the data structure for the current value.
 * @param parse_handle   Pointer to the handle of the currently parsed format definition file.
 */

static void parse_relative_timestamp_spec(char **position, value_format_t *current_format,
    parse_handle_t *parse_handle)
{
    char *p = (*position) + 3; // Move past the already validated "[t-".
    char selection[MAX_NAME_LENGTH];

    if (!parse_until_specified_character(&p, selection, MAX_NAME_LENGTH, ']'))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_TIMESTAMP_DEFINITION, *position);
    }

    uint32_t msg_index = find_message_format_index(selection);

    if (msg_index == MSG_NAME_NOT_FOUND)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_TIMESTAMP_MSG_NOT_FOUND, *position);
    }

    current_format->fmt_id_timer = msg_index;
    current_format->data_type = VALUE_TIME_DIFF;
    current_format->data_size = 0;
    *position = p + 1; // Move past the ']'.
}


/**
 * @brief Parses the <M_NAME> formatting definition to memorize the value to MEMO(M_NAME).
 *
 * @param position      Pointer to the current position in the string being parsed.
 * @param parse_handle  Pointer to the handle of the currently parsed file.
 */

static void parse_remember_spec(char **position, parse_handle_t *parse_handle)
{
    char *p = (*position) + 1; // Move past the '<'.
    char *start = p - 1;       // Set the error position to the '<'.
    skip_whitespace(&p);

    char selection[MAX_NAME_LENGTH];

    if (!parse_until_specified_character(&p, selection, MAX_NAME_LENGTH, '>'))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_REMEMBER_MEMO_NOT_FOUND, start);
    }

    if (*selection == '\0')
    {
        catch_parsing_error(parse_handle, ERR_PARSE_REMEMBER_MEMO_NOT_FOUND, start);
    }

    rte_enum_t memo_index = find_enum_idx(selection, MEMO_TYPE);

    if (memo_index == 0)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_REMEMBER_MEMO_NOT_FOUND, selection);
    }

    if (parse_handle->current_format->put_memo)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_OVERDEFINITION_ANGLEBRACKETS, start);
    }

    parse_handle->current_format->put_memo = memo_index;

    *position = p + 1; // Move past the '>' and update the pointer.
}


/**
 * @brief Parses the statistics definition: |statistics|
 *        Activates statistics and assigns a name to the value using the text between the |...|.
 *
 * @param position      Pointer to the current position in the string being parsed.
 * @param parse_handle  Pointer to the handle of the currently parsed file.
 */

static void parse_statistics_spec(char **position, parse_handle_t *parse_handle)
{
    char *p = (*position) + 1;         // Move past the opening '|'.
    char selection[MAX_NAME_LENGTH];

    if (!parse_until_specified_character(&p, selection, MAX_NAME_LENGTH, '|'))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_BAD_STATISTICS_NAME, *position);
    }

    if (*selection == '\0')
    {
        catch_parsing_error(parse_handle, ERR_PARSE_EMPTY_STATISTICS, *position);
    }

    if (parse_handle->current_format->value_stat != NULL)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_OVERDEFINITION_PIPEBRACKETS, *position);
    }

    parse_handle->current_format->value_stat = allocate_memory(sizeof(value_stats_t), "stat_s");
    parse_handle->current_format->value_stat->name = duplicate_string(selection);

    *position = p + 1;          // Move past the closing '|'.
}


/**
 * @brief Saves the indexed text data to the enums data structure
 *        or reports an error if there is no space in the structure or if the indexed text has
 *        already been defined for the currently parsed format definition.
 *
 * @param position      Pointer to the current position in the string being parsed.
 * @param parse_handle  Pointer to the handle of the currently parsed file.
 * @param buff          Pointer to the prepared data with indexed text.
 */

static void save_the_indexed_text_line(char **position, parse_handle_t *parse_handle, const char *buff)
{
    if (g_msg.enums_found >= MAX_ENUMS)
    {
        g_msg.total_errors = MAX_ERRORS_REPORTED - 1;   // Fatal error => stop parsing.
        catch_parsing_error(parse_handle, ERR_PARSE_MAX_ENUMS, *position);
    }

    if (parse_handle->current_format->in_file)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_Y_TEXT_OVERDEFINED, *position);
    }

    g_msg.enums[g_msg.enums_found].name = "#Y_TEXT";
    g_msg.enums[g_msg.enums_found].type = Y_TEXT_TYPE;
    g_msg.enums[g_msg.enums_found].in_file_txt = duplicate_string(buff);
    parse_handle->current_format->in_file = (rte_enum_t)g_msg.enums_found;
    g_msg.enums_found++;
    g_msg.fmt_ids_defined++;
}


/**
 * @brief Parses the indexed text: {string1|string2|...|stringN}
 *        The indexed text is read into a string. Each individual sub-string
 *        can have a size from 1 to 255. The first character of each sub-string defines
 *        the length of that line (number of characters). There is no '\0' character at
 *        the end of each sub-string. A zero-length character defines the end of data.
 *
 * @param position      Pointer to the current position in the string being parsed.
 * @param parse_handle  Pointer to the handle of the currently parsed file.
 */

static void parse_indexed_text(char **position, parse_handle_t *parse_handle)
{
    char buff[MAX_INPUT_LINE_LENGTH];
    size_t number_of_substrings = 0;
    size_t index = 1;
    size_t start_index = 0;
    char *p;

    for (p = (*position) + 1; *p != '\0'; p++, index++)
    {
        int c = (unsigned char)*p;

        if (c == '\0')
        {
            catch_parsing_error(parse_handle, ERR_PARSE_INDEXED_TEXT_UNFINISHED, *position);
        }

        if (index >= (MAX_INPUT_LINE_LENGTH - 1))
        {
            catch_parsing_error(parse_handle, ERR_PARSE_LINE_TOO_LONG, "");
        }

        buff[index] = (char)c;

        if ((c == '|') || (c == '}'))
        {
            size_t len = index - start_index - 1;

            if ((len < 1) || (len > 255))
            {
                catch_parsing_error(parse_handle, ERR_PARSE_BAD_INDEXED_TEXT_LENGTH, *position);
            }

            buff[start_index] = (char)len;
            number_of_substrings++;

            if (c == '}')
            {
                break;
            }

            start_index = index;
        }
    }

    buff[index] = '\0';

    if (number_of_substrings < 2)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_INDEXED_TEXT_ATLEAST_2_OPTIONS, *position);
    }

    save_the_indexed_text_line(position, parse_handle, buff);
    *position = p + 1;               // Move past the '}'.

    parse_handle->found.indexed_text = true;
}


/**
 * @brief Parses value scaling specifiers in the format (+/-offset*multiplier).
 *        Both offset and multiplier can be specified, but the sign is mandatory for the offset,
 *        and '*' is required for the multiplier.
 *
 * @param position              Pointer to the current parsing position in the string.
 * @param parse_handle          Pointer to the parse handle of the current file.
 * @param found_square_brackets Indicates if a value definition has been parsed (false if not).
 */

static void parse_scaling_spec(char **position, parse_handle_t *parse_handle,
    bool found_square_brackets)
{
    char *p = (*position) + 1;         // Skip the '(' character.
    skip_whitespace(&p);
    char *stop_char = NULL;
    double offset = 0.0;
    double multiplier = 1.0;

    switch (*p)
    {
        case '+':
        case '-':
            offset = strtod(p, &stop_char);

            if (p >= stop_char)
            {
                catch_parsing_error(parse_handle, ERR_PARSE_SCALING_INVALID_FORMAT, *position);
            }

            p = stop_char;
            //lint -fallthrough
            
        case '*':
            if (*p == '*')
            {
                p++;
                multiplier = strtod(p, &stop_char);

                if (p >= stop_char)
                {
                    catch_parsing_error(parse_handle, ERR_PARSE_SCALING_INVALID_FORMAT, *position);
                }

                p = stop_char;
            }

            if (*p != ')')
            {
                catch_parsing_error(parse_handle, ERR_PARSE_SCALING_INVALID_FORMAT, *position);
            }
            break;

        default:
            catch_parsing_error(parse_handle, ERR_PARSE_SCALING_INVALID_FORMAT, *position);
    }

    p++;
    *position = p;

    if (parse_handle->current_format->mult != 0)
    {
        // Scaling has already been defined for this value.
        catch_parsing_error(parse_handle, ERR_PARSE_OVERDEFINITION_PARENTHESES, *position);
    }

    if (multiplier == 0)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_SCALING_ZERO_MULTIPLIER, *position);
    }

    if (!found_square_brackets)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_MUST_HAVE_VALUE_DEF, *position);
    }

    parse_handle->current_format->mult = multiplier;
    parse_handle->current_format->offset = offset;
}


/**
 * @brief Parsing of the value scaling specifiers (+/-offset*multiplier)
 *        Either offset or multiplier parameter (or both) can be provided.
 *        The sign MUST be provided for the offset and the '*' for the multiplier.
 *
 * @param parse_handle      Pointer to the parse handle of the current file.
 * @param size              Size of the value in bits (must be between 1 and 64).
 * @param address           Address or offset for the value, depending on the sign.
 * @param sign              Sign of the address ('+', '-', or 0 if no sign).
 * @param two_values_found  Indicates if both address and data fields were found (true) or just data (false).
 */

static void check_and_set_value_definition(parse_handle_t *parse_handle,
    unsigned long size, unsigned long address, char sign, bool two_values_found)
{
    if ((size < 1) || (size > 64))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_VALUE_nnmmF_INVALID_SIZE, NULL);
    }

    parse_handle->current_format->data_size = size;

    if (two_values_found)
    {
        if (sign == '+')
        {
            parse_bit_address += address;
        }
        else if (sign == '-')
        {
            if (parse_bit_address < address)      // Prevent negative address values.
            {
                catch_parsing_error(parse_handle, ERR_PARSE_VALUE_nnmmF_mm_NEGATIVE_ADDR, NULL);
            }

            parse_bit_address -= address;
        }
        else
        {
            parse_bit_address = address;
        }
    }
    else
    {
        if (sign != 0)
        {
            catch_parsing_error(parse_handle, ERR_PARSE_VALUE_SIGN, NULL);
        }
    }

    parse_handle->current_format->bit_address = parse_bit_address;
}


/**
 * @brief Sets the data type for the current value and verifies the correctness of the
 *        bit address and bit size for this value.
 *
 * @param parse_handle  Pointer to the handle of the currently parsed file.
 * @param type          Character representing the type of value ('s'=string, 'i'=signed int, 'f'=float, 'u'=unsigned).
 */

static void check_and_set_data_type(parse_handle_t *parse_handle, char type)
{
    value_format_t *current_format = parse_handle->current_format;

    switch (type)
    {
        case 's':
            current_format->data_type = VALUE_STRING;

            if (current_format->bit_address & 7u)   // The bit address of a string value must be divisible by 8
            {
                catch_parsing_error(parse_handle, ERR_PARSE_SW_ADDR_NOT_DIVISIBLE_BY_8, NULL);
            }
            break;

        case 'i':
            current_format->data_type = VALUE_INT64;
            break;

        case 'f':
            current_format->data_type = VALUE_DOUBLE;

            if (current_format->bit_address % 8)
            {
                catch_parsing_error(parse_handle, ERR_PARSE_SW_ADDR_NOT_DIVISIBLE_BY_8, NULL);
            }
            if (!((current_format->data_size == 16u)
                || current_format->data_size == 32u
                || current_format->data_size == 64u))
            {
                catch_parsing_error(parse_handle, ERR_PARSE_VALUE_DOUBLE_LENGTH, NULL);
            }
            break;

        default:    // 'u'
            current_format->data_type = VALUE_UINT64;
            break;
    }
}


/**
 * @brief Parses the value specifier [+/-nn:mmF], [nn:mmF], or [mmF].
 *        F - type of data (f=float, u=unsigned, i=signed int, s=string).
 *        nn - address of value to be printed (taken from the currently processed message).
 *        mm - size of value (number of bits).
 *
 * @param position      Pointer to the currently parsed string.
 * @param parse_handle  Pointer to the handle of the currently parsed file.
 */

static void parse_value_data(char **position, parse_handle_t *parse_handle)
{
    char sign = 0;
    char type = 'u';            // Default type is unsigned.
    bool two_values_found = false;
    unsigned long address = 0;

    char *p = (*position) + 1;    // skip the '['

    if ((*p == '+') || (*p == '-'))
    {
        sign = *p++;
    }

    char *end = NULL;
    unsigned long size = strtoul(p, &end, 10);

    if (end <= p)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_VALUE_INVALID_CHAR, *position);
    }

    p = end;

    if (*p == ':')
    {
        p++;
        address = size;         // The first value was the address.
        size = strtoul(p, &end, 10);

        if (end <= p)
        {
            catch_parsing_error(parse_handle, ERR_PARSE_VALUE_INVALID_CHAR, *position);
        }

        p = end;
        two_values_found = true;
    }

    if (strchr("fuis", *p))     // f-float, u-unsigned, i-signed integer, s-string.
    {
        type = *p++;
    }

    if (*p++ != ']')
    {
        catch_parsing_error(parse_handle, ERR_PARSE_VALUE_UNFINISHED, *position);
    }

    *position = p;     // Update the position.

    check_and_set_value_definition(parse_handle, size, address, sign, two_values_found);
    check_and_set_data_type(parse_handle, type);
}


/**
 * @brief Parses the value definitions: [N], [T-MSG_NAME], [T], [t], [M_NAME] etc.
 *
 * @param position      Pointer to the current position in the string being parsed.
 * @param parse_handle  Pointer to the handle of the currently parsed file.
 */

static void parse_square_brackets(char **position, parse_handle_t *parse_handle)
{
    char *p = *position;
    value_format_t *current_format = parse_handle->current_format;
    parse_handle->err_position = p;

    //[N]
    if (*(p + 1) == 'N')
    {
        parse_special_spec(position, current_format, parse_handle, VALUE_MESSAGE_NO);
    }
    // Check for [t-MSG_NAME] before [T] to avoid confusion
    else if ((*(p + 1) == 't') && (*(p + 2) == '-'))
    {
        parse_relative_timestamp_spec(position, current_format, parse_handle);
    }
    // Check for [t]
    else if (*(p + 1) == 't')
    {
        parse_special_spec(position, current_format, parse_handle, VALUE_TIMESTAMP);
    }
    // Check for [T]
    else if (*(p + 1) == 'T')
    {
        parse_special_spec(position, current_format, parse_handle, VALUE_dTIMESTAMP);
    }
    // Check for [M_NAME] recall
    else if ((*(p + 1) == 'M') && (*(p + 2) == '_'))
    {
        parse_memo_recall_spec(position, current_format, parse_handle);
    }
    // Check for [nn:mmF] or [mmF]
    else
    {
        parse_value_data(position, parse_handle);
    }

    parse_handle->found.value_spec = true;
}


/**
 * @brief Parses various formatting type extensions that define the printed value.
 *        These follow the '%' in the formatting definition and specify
 *        the value to be printed (bit position, size, scaling) and other
 *        options like statistics, memo, etc.
 *
 * @param position      Pointer to the current position in the text being parsed.
 * @param parse_handle  Pointer to the handle of the currently parsed file.
 */
 
static void parse_special_format(char **position, parse_handle_t *parse_handle)
{
    bool found_square_brackets = false;
    bool finished = false;

    for (char *p = *position; (*p != '\0') && !finished; )
    {
        switch (*p)
        {
            case '(':
                parse_scaling_spec(&p, parse_handle, found_square_brackets);
                break;

            case '[':
                if (found_square_brackets)
                {
                    catch_parsing_error(parse_handle, ERR_PARSE_OVERDEFINITION_SQUAREBRACKETS, *position);
                }

                parse_square_brackets(&p, parse_handle);
                found_square_brackets = true;
                break;

            case '{':
                parse_indexed_text(&p, parse_handle);
                break;

            case '<':
                parse_remember_spec(&p, parse_handle);
                break;

            case '|':
                parse_statistics_spec(&p, parse_handle);
                break;

            default:
                *position = p;
                finished = true;
                break;
        }
    }

    // Default to a bit length of 32 if no value specification is found
    if (!parse_handle->found.value_spec)
    {
        parse_handle->current_format->data_size = 32;
    }

    parse_handle->current_format->bit_address = parse_bit_address;
}


/**
 * @brief Prepares the format definition structure and initializes values.
 *        Reports an error if no MSG directive precedes the formatting string.
 *
 * @param parse_handle Pointer to the structure used during format definition parsing.
 */

static void prepare_or_continue_fmt(parse_handle_t *parse_handle)
{
    // Reset flags
    parse_handle->found.indexed_text = false;
    parse_handle->found.value_spec = false;

    if (parse_handle->p_current_message == NULL)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_NO_PRIOR_MSG, NULL);
    }

    // Determine if a new message structure is being filled or if the current one continues
    if (parse_handle->p_prev_msg != parse_handle->p_current_message)
    {
        // Processing a new message
        parse_handle->p_prev_msg = parse_handle->p_current_message;
        parse_handle->current_format = parse_handle->p_current_message->format;
        parse_bit_address = 0; // Reset bit address for a new message
    }
    else
    {
        // Continue with the existing message
        parse_handle->current_format->format = allocate_memory(sizeof(value_format_t), "valFormat");
        parse_handle->current_format = parse_handle->current_format->format;
    }

    // Reset bit address if the output file has changed
    if (parse_handle->prev_out_file_idx != parse_handle->current_out_file_idx)
    {
        parse_bit_address = 0;
    }

    parse_handle->prev_out_file_idx = parse_handle->current_out_file_idx;
    parse_handle->current_format->out_file = parse_handle->current_out_file_idx;
    parse_handle->current_format->print_copy_to_main_log = parse_handle->print_to_main_log;
    parse_handle->current_format->in_file = parse_handle->current_in_file_idx;
}


/**
 * @brief Validates the formatting definitions for '%Y' selected text printing.
 *
 * @param parse_handle Pointer to the structure used during format definition parsing.
 * @param substring    Pointer to the sub-string being parsed.
 */

static void check_y_type_formatting(parse_handle_t *parse_handle, char *substring)
{
    if (parse_handle->found.indexed_text !=
        (parse_handle->current_format->fmt_type == PRINT_SELECTED_TEXT))
        // One cannot be defined without the other
    {
        if ((parse_handle->current_format->in_file == 0) &&
            !g_msg.param.check_syntax_and_compile)
        {
            catch_parsing_error(parse_handle, ERR_PARSE_INDEXED_TEXT_INCOMPLETE, substring);
        }
    }
}


/**
 * @brief Completes the parsing of a format definition sub-string.
 *        The main part of the sub-string has been processed. *p points to the format type.
 *        This function appends remaining characters and updates the format structure.
 *
 * @param fmt_substring  Pointer to the sub-string being assembled.
 * @param index          Current index in the sub-string.
 * @param p              Pointer to the printf string pointer (text definition line being parsed).
 * @param parse_handle   Pointer to the structure used during format definition parsing.
 */

static void finalize_substring(char *fmt_substring, size_t index, char **p, parse_handle_t *parse_handle)
{
    char *fmt_string = *p;
    char c = *fmt_string;
    char fmt_char = c;              // The first character is the formatting type
    int special_format = false;

    if (strchr("tTNWHYBsDM", fmt_char))
    {
        special_format = true;
    }

    // Append text after the format type until the next '%' or end of string
    do
    {
        fmt_substring[index++] = c;
        fmt_string++;
        c = *fmt_string;

        if ((c == '\\') || (c == '%'))
        {
            // Stop appending after the first escape character or '%'
            break;
        }
        
        // No additional text allowed after special format types
        if (special_format)
        {
            break;
        }
    } while (c != '\0');

    fmt_substring[index] = '\0';
    char *format_string = duplicate_string(fmt_substring);
    fill_in_fmt_type(format_string, parse_handle, fmt_char);
    parse_handle->current_format->fmt_string = format_string;

    check_y_type_formatting(parse_handle, fmt_substring);
    parse_bit_address += parse_handle->current_format->data_size; // Update bit address for next value

    *p = fmt_string;
}


/**
 * @brief Parses the formatting string into sub-strings.
 *        This function breaks down the formatting string into sub-strings because the print_message()
 *        function can only process one value at a time. It organizes the data for print_message()
 *        as a linked list of format definition structures. The function is recursive, processing
 *        each sub-string individually. It completes when the entire string is parsed or reports
 *        an error if parsing fails.
 *
 * See the Microsoft printf format specification:
 * https://learn.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=msvc-170
 *
 * @param buffer       Pointer to the string containing the formatting definition.
 * @param parse_handle Pointer to the structure used during format definition parsing.
 */

void separate_fmt_strings(char *buffer, parse_handle_t *parse_handle)
{
    if (*buffer == '\0')
    {
        catch_parsing_error(parse_handle, ERR_PARSE_EMPTY_STRING, "");
    }

    check_stack_space();  // Ensure sufficient stack space for recursion
    prepare_or_continue_fmt(parse_handle);

    char fmt_substring[MAX_INPUT_LINE_LENGTH];
    size_t substr_index = 0;
    bool percent_found = false;
    parse_handle->err_position = buffer;

    for (char *p = buffer; *p != '\0'; )
    {
        if (substr_index >= MAX_INPUT_LINE_LENGTH)
        {
            catch_parsing_error(parse_handle, ERR_PARSE_LINE_TOO_LONG, "");
        }

        int c = (unsigned char)*p;

        if (percent_found)
        {
            if (isdigit(c) || strchr("-+#hl. ", c))  // Check for flag and width specifiers
            {
                fmt_substring[substr_index++] = (char)c;
                p++;
                continue;
            }

            // Validate C style and special (extended) type field characters
            if (strchr("dicouxXeEfFgGaAtTNWHYBsDM", c) == NULL)
            {
                // Invalid type character or incomplete format
                catch_parsing_error(parse_handle, ERR_PARSE_TYPE_UNRECOGNIZED, buffer);
            }

            percent_found = false;
            finalize_substring(fmt_substring, substr_index, &p, parse_handle);
            substr_index = 0;

            if (*p != '\0')
            {
                separate_fmt_strings(p, parse_handle);
            }

            break;
        }

        fmt_substring[substr_index++] = (char)c;
        p++;

        if (c == '%')
        {
            if (*p == '%')  // Handle escaped percent
            {
                fmt_substring[substr_index++] = *p++;
            }
            else
            {
                // Process value definitions like [], {}, <>, (), ||, etc.
                // These must follow the '%' immediately
                parse_special_format(&p, parse_handle);
                percent_found = true;
            }
        }
    }

    if (percent_found)
    {
        // No valid format type found after the percent character
        catch_parsing_error(parse_handle, ERR_PARSE_UNFINISHED, buffer);
    }

    // If substr_index > 0, finalize the current format definition as plain text
    if (substr_index > 0)
    {
        fmt_substring[substr_index] = '\0';
        parse_handle->current_format->fmt_string = duplicate_string(fmt_substring);
        parse_handle->current_format->data_size = 0;
        parse_handle->current_format->fmt_type = PRINT_PLAIN_TEXT;
        parse_handle->current_format->bit_address = parse_bit_address;
    }
}

/*==== End of file ====*/
