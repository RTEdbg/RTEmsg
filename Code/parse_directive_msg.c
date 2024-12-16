/*
 * Copyright (c) 2024 Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    parse_directive_msg.c
 * @author  B. Premzel
 * @brief   Functions for parsing MSG, MSGN, MSGX, and EXT_MSG formatting directives.
 ******************************************************************************/

#include "pch.h"
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "parse_directive_msg.h"
#include "parse_directive.h"
#include "parse_file_handling.h"
#include "parse_error_reporting.h"


/**
 * @brief Parse number data for the MSG0 .. MSG4 directive.
 *
 * @param parse_handle    Pointer to the handle of the currently parsed file.
 *
 * @return  Format ID of the currently processed message.
 */

static unsigned parse_msg_num(parse_handle_t *parse_handle)
{
    unsigned msg_num = parse_unsigned_int(parse_handle);

    if (msg_num > 4u)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_MSG_SIZE_0_4, NULL);
    }

    unsigned fmt_ids = 1U << msg_num;
    parse_handle->p_new_message->msg_len = 4u * msg_num;
    return assign_fmt_id(fmt_ids, parse_handle->p_new_message);
}


/**
 * @brief Parse number data for the EXT_MSGx_y directive.
 *        x - number of 32-bit data words, y - number of additional data bits.
 *        x = 0 ... 4, y = 1 ... 8   (x + y <= 8)
 *
 * @param parse_handle    Pointer to the handle of the currently parsed file.
 *
 * @return  Format ID of the currently processed message.
 */

static unsigned parse_ext_msg_num(parse_handle_t *parse_handle)
{
    unsigned msg_num = parse_unsigned_int(parse_handle);

    if (msg_num > 4u)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_EXT_MSG_SIZE, NULL);
    }

    if (**parse_handle->p_file_line_curr_pos != '_')
    {
        catch_parsing_error(parse_handle, ERR_PARSE_EXPECTING_UNDERSCORE, NULL);
    }

    (*parse_handle->p_file_line_curr_pos)++;   // Skip the '_'

    unsigned extended_data_length = parse_unsigned_int(parse_handle);

    if ((extended_data_length > (8u - msg_num)) || (extended_data_length < 1u))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_EXT_MSG_NO_BITS, NULL);
    }

    parse_handle->p_new_message->ext_data_mask = (1u << extended_data_length) - 1u;
    parse_handle->p_new_message->msg_len = 4u + msg_num * 4u;
    return assign_fmt_id((1u << (extended_data_length + msg_num)), parse_handle->p_new_message);
}


/**
 * @brief Parse the MSGX directive.
 *
 * @param parse_handle    Pointer to the handle of the currently parsed file.
 *
 * @return  Format ID of the currently processed message.
 */

static unsigned parse_msgx_num(parse_handle_t *parse_handle)
{
    return assign_fmt_id(16, parse_handle->p_new_message);
}


/**
 * @brief Parse number data for the MSGN and MSGNnn directives (nn = number of words).
 *
 * @param parse_handle    Pointer to the handle of the currently parsed file.
 *
 * @return  Format ID of the currently processed message.
 */

static unsigned parse_msgn_num(parse_handle_t *parse_handle)
{
    char *start = *parse_handle->p_file_line_curr_pos;

    unsigned current_message_fmt_id;

    if (*start == '_')      // No number after the MSGN (size unknown at compile time).
    {
        current_message_fmt_id = assign_fmt_id(16, parse_handle->p_new_message);
    }
    else                    // Number after the MSGN prefix (known message size).
    {
        unsigned msg_num = parse_unsigned_int(parse_handle);
        current_message_fmt_id = assign_fmt_id(16, parse_handle->p_new_message);
        parse_handle->p_new_message->msg_len = msg_num * 4u;

        if (msg_num > MAX_MSG_LENGTH)
        {
            catch_parsing_error(parse_handle, ERR_PARSE_MSG_DEFINITION_TOO_BIG, NULL);
        }

        if (msg_num == 0)
        {
            catch_parsing_error(parse_handle, ERR_PARSE_MSG0_NOT_ALLOWED, NULL);
        }
    }

    return current_message_fmt_id;
}


/**
 * @brief Parse the message name (contains only alphanumeric characters and '_').
 *        Report an error if the name has been defined (used) already.
 *
 * @param parse_handle    Pointer to the handle of the currently parsed file.
 */

static void parse_msg_name(parse_handle_t *parse_handle)
{
    char name[MAX_NAME_LENGTH];
    parse_name(parse_handle, name);

    if (find_message_format_index(name) != MSG_NAME_NOT_FOUND)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_MSG_NAME_EXISTS, name);
    }

    parse_handle->p_new_message->message_name = duplicate_string(name);
}


/**
 * @brief Parse the MSG directives - common part of parsing for this directive.
 *
 * @param parse_handle    Pointer to the handle of the currently parsed file.
 * @param chars_to_skip   Number of characters to skip (prefix length - i.e. length of "MSG").
 * @param msg_type        Message type - i.e. TYPE_EXT_MSG.
 * @param p_parse_msg_no  Pointer to function that sets data structure accordingly to the
 *                        message number.
 */

static void parse_msg_directive(parse_handle_t *parse_handle, size_t chars_to_skip,
    enum msg_type_t msg_type, parse_msg_no_t p_parse_msg_no)
{
    check_if_the_last_msg_is_empty(parse_handle);

    if (parse_handle->p_new_message != NULL)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_MSG_MULTIPLE_IN_LINE, NULL);
    }

    if (parse_handle->found.in_file_select || parse_handle->found.out_file_select)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_MSG_IN_LINE_AFTER_IN_OUT_SELECT, NULL);
    }

    parse_handle->p_new_message = (msg_data_t *)allocate_memory(sizeof(msg_data_t), "MsgParse");
    parse_handle->p_new_message->msg_type = msg_type;

    char **position = parse_handle->p_file_line_curr_pos;
    char *p_start = *position;
    *position = *position + chars_to_skip;      // Skip the message name prefix.

    value_format_t *new_format =
        (value_format_t *)allocate_memory(sizeof(value_format_t), "ValFmt");
    parse_handle->p_new_message->format = new_format;

    unsigned current_message_fmt_id = p_parse_msg_no(parse_handle);

    if (current_message_fmt_id == 0xFFFFFFFF)     // Format ID not assigned?
    {
        g_msg.total_errors = MAX_ERRORS_REPORTED - 1;   // Fatal error => stop parsing.
        catch_parsing_error(parse_handle, ERR_PARSE_FMT_ID_NOT_ASSIGNED, NULL);
    }

    // The MSG directive must be followed by '_' and at least one alphanumeric character.
    if (((*position)[0] != '_') || (!isalnum((*position)[1])))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_MSG_DEFINITION, NULL);
    }

    *position = p_start;    // Point to the start of "MSG.." or "EXT_MSG.." name.
    parse_msg_name(parse_handle);
    parse_handle->p_current_message = parse_handle->p_new_message;
    write_define_to_work_file(parse_handle,
        parse_handle->p_current_message->message_name, current_message_fmt_id);
}


/**
 * @brief Parses format definitions for the MSG, MSGX, EXT_MSG, and MSGN directives.
 *
 * @param parse_handle     Pointer to the handle of the currently parsed file.
 */

void parse_msg_directives(parse_handle_t *parse_handle)
{
    char *fmt_text = *parse_handle->p_file_line_curr_pos;

    if (strncmp(fmt_text, "MSGN", 4) == 0)
    {
        parse_msg_directive(parse_handle, 4, TYPE_MSGN, parse_msgn_num);
    }
    else if (strncmp(fmt_text, "MSGX", 4) == 0)
    {
        parse_msg_directive(parse_handle, 4, TYPE_MSGX, parse_msgx_num);
    }
    else if (strncmp(fmt_text, "MSG", 3) == 0)
    {
        parse_msg_directive(parse_handle, 3, TYPE_MSG0_4, parse_msg_num);
    }
    else if (strncmp(fmt_text, "EXT_MSG", 7) == 0)
    {
        parse_msg_directive(parse_handle, 7, TYPE_EXT_MSG, parse_ext_msg_num);
    }
    else
    {
        catch_parsing_error(parse_handle, ERR_PARSE_UNRECOGNIZED_DIRECTIVE, fmt_text);
    }
}

/*==== End of file ====*/
