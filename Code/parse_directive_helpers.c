/*
 * Copyright (c) 2024 Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    parse_directive_helpers.c
 * @brief   Helper functions for parsing RTEmsg directives.
 * @authors S. Milivojcev, B. Premzel
 ******************************************************************************/

#include "pch.h"
#include "decoder.h"
#include "errors.h"
#include "parse_error_reporting.h"


/**
 * @brief Checks if a given name has already been defined in g_msg.enums[].
 *        Reports an error if the name is already in use.
 *
 * @param newName       The name to be checked.
 * @param parse_handle  Pointer to the parse handle.
 */

static void check_if_enums_name_exists(const char *newName, parse_handle_t *parse_handle)
{
    for (unsigned int i = 0; i < g_msg.enums_found; i++)
    {
        if ((g_msg.enums[i].name != NULL) && (strcmp(newName, g_msg.enums[i].name) == 0))
        {
            catch_parsing_error(parse_handle, ERR_PARSE_ENUMS_NAME_EXISTS, newName);
        }
    }
}


/**
 * @brief   Finds the index of an enum with the specified name and type.
 *
 * @param enum_name   The name of the enum to search for.
 * @param enum_type   The type of the enum to search for.
 *
 * @return  0         If the enum with the specified name and type is not found.
 *          >=32      The index of the enum with the specified name and type.
 */

rte_enum_t find_enum_idx(char *enum_name, enum enums_type_t enum_type)
{
    rte_enum_t result = 0;

    for (rte_enum_t i = NUMBER_OF_FILTER_BITS; i < g_msg.enums_found; i++)
    {
        if ((g_msg.enums[i].name != NULL)
            && (strcmp(g_msg.enums[i].name, enum_name) == 0)
            && (g_msg.enums[i].type == enum_type)
            )
        {
            result = i;
            break;
        }
    }

    return result;
}


/**
 * @brief   Checks if an IN_FILE or OUT_FILE with the same name has already been defined.
 *
 * @param parse_handle  Pointer to the parse handle.
 * @param file_name     The name of the input or output file.
 * @param enum_type     The type of the enum to search for.
 */

void file_name_used_before(parse_handle_t *parse_handle, char *file_name, enum enums_type_t enum_type)
{
    for (rte_enum_t i = NUMBER_OF_FILTER_BITS; i < g_msg.enums_found; i++)
    {
        if ((g_msg.enums[i].type == enum_type)
            && (g_msg.enums[i].file_name != NULL)
            && (strcmp(g_msg.enums[i].file_name, file_name) == 0)
            )
        {
            catch_parsing_error(parse_handle,
                ERR_PARSE_IN_OUT_FILE_NAME_USED_TWICE, g_msg.enums[i].name);
        }
    }
}


/**
 * @brief  Parses the text until it reaches the specified stop character.
 *         Updates the position to where parsing stopped.
 *
 * @param position      Current position in the text. Modified to where parsing stops.
 * @param result        Buffer to store the parsed text.
 * @param result_size   Size of the result buffer.
 * @param stop_char     Character that terminates parsing.
 *
 * @return false    If an error is detected.
 *         true     If the stop character is found.
 */

bool parse_until_specified_character(char **position, char *result, size_t result_size, char stop_char)
{
    size_t index = 0;
    bool finished = false;

    for (char *p = *position; *p != 0; p++)
    {
        char c = *p;

        if (c == stop_char)
        {
            finished = true;
            *position = p;
            break;
        }

        if (index >= (result_size - 1))
        {
            index = result_size - 1;
            break;
        }

        result[index++] = c;
    }

    result[index] = '\0'; // Terminate the sub-string
    return finished;
}


/**
 * @brief  Parses a quoted argument (sub-string).
 *
 * @param position         Current position in the parsed text.
 * @param buffer           Buffer to store the parsed text.
 * @param result_size      Size of the result buffer.
 *
 * @return  true    If the quoted string is found and copied to the buffer.
 *          false   If an error is found.
 */

bool parse_quoted_arg(char **position, char *buffer, size_t result_size)
{
    skip_whitespace(position);
    char *p = *position;

    if (*p++ != '"')
    {
        return false; // Text must start with a quote
    }

    size_t index = 0;

    for ( ; *p != 0; p++)
    {
        if (index >= result_size)
        {
            buffer[result_size - 1] = 0;
            break; // Text too long
        }

        int c = (unsigned char)*p;

        if (c == '"')
        {
            buffer[index] = 0; // Terminate the sub-string (result)
            *position = p + 1;
            return true;
        }

        buffer[index++] = c;
    }

    return false; // The closing quote not found
}


/**
 * @brief Parse an unsigned integer from the current position in the parsed line.
 *
 * @param parse_handle     Pointer to the handle of the currently parsed file.
 *
 * @return  The unsigned integer value found at the current position in the parsed line.
 */

unsigned parse_unsigned_int(parse_handle_t *parse_handle)
{
    char **position = parse_handle->p_file_line_curr_pos;
    char *end_position = NULL;
    unsigned long value = strtoul(*position, &end_position, 10);

    if (end_position <= *position)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_EXPECTING_NUMBER, *position);
    }

    *position = end_position;

    return (unsigned)value;
}


/**
 * @brief Parse a name that may contain alphanumeric characters and underscores ('_').
 *        If the name has already been used, an error is reported.
 *
 * @param parse_handle   Pointer to the parse handle for the current formatting file.
 * @param name           Buffer to store the parsed name.
 */

void parse_name(parse_handle_t *parse_handle, char *name)
{
    char **position = parse_handle->p_file_line_curr_pos;
    skip_whitespace(position);

    size_t counter = 0;
    char *p;

    for (p = *position; *p != '\0'; p++)
    {
        int c = (unsigned char)*p;

        if (isascii(c) && (isalnum(c) || (c == '_')))
        {
            if (counter >= (MAX_NAME_LENGTH - 1))
            {
                catch_parsing_error(parse_handle, ERR_PARSE_NAME_TOO_LONG, "");
            }

            name[counter++] = (char)c;
        }
        else
        {
            break;
        }
    }

    name[counter] = '\0'; // Null-terminate the string

    if (counter < 1)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_INVALID_NAME, *position);
    }

    *position = p;
    skip_whitespace(position);
}


/**
 * @brief Parse a directive name, ensuring it has not been used before, and return a duplicate.
 *        The directive name must have a specified prefix, unless the prefix is NULL.
 *
 * @param parse_handle   Pointer to the parse handle for the current formatting file.
 * @param name_prefix    Required prefix for the name, e.g., "F_" for filters.
 *
 * @return  Pointer to the duplicated string containing the directive name.
 */

char *parse_directive_name(parse_handle_t *parse_handle, const char *name_prefix)
{
    char name[MAX_NAME_LENGTH];
    parse_name(parse_handle, name);

    // Validate the name prefix if provided
    if ((name_prefix != NULL) && (strncmp(name, name_prefix, strlen(name_prefix)) != 0))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_BAD_NAME_PREFIX, name_prefix);
    }

    check_if_enums_name_exists(name, parse_handle);

    if (g_msg.enums_found >= MAX_ENUMS)
    {
        g_msg.total_errors = MAX_ERRORS_REPORTED - 1; // Fatal error: stop parsing
        catch_parsing_error(parse_handle, ERR_PARSE_MAX_ENUMS, NULL);
    }

    return duplicate_string(name);
}


/**
 * @brief Parse a file path from a quoted argument.
 *
 * @param parse_handle    Pointer to the parse handle for the current formatting file.
 * @param file_path       Buffer to store the parsed file path.
 * @param max_length      Maximum size of the file path buffer.
 */

void parse_file_path_arg(parse_handle_t *parse_handle, char *file_path, size_t max_length)
{
    char **position = parse_handle->p_file_line_curr_pos;
    skip_whitespace(position);
    char *start = *position;

    if (!parse_quoted_arg(position, file_path, max_length))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_IN_OUT_FILE_PATH, start);
    }

    if (*file_path == '\0')
    {
        catch_parsing_error(parse_handle, ERR_PARSE_IN_OUT_FILE_PATH, start);
    }
}

/*==== End of file ====*/
