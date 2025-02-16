/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/******************************************************************************
 * @file    parse_directive.h
 * @authors S. Milivojcev, B. Premzel
 * @brief   Functions for parsing RTEmsg directives in format definition files.
 ******************************************************************************/

#ifndef PARSE_DIRECTIVE_H
#define PARSE_DIRECTIVE_H

#include "main.h"
#include "parse_directive_helpers.h"

/* Macro to advance the position pointer by the length of the string literal minus 1
 * (to account for the null terminator) */
#define ADVANCE_POSITION(position, x) (*position += sizeof(x) - 1)

__declspec(noinline) void check_stack_space(void);
void parse_fmt_file(const char *filepath, parse_handle_t *parent_parse_handle);
void check_if_the_last_msg_is_empty(parse_handle_t *parse_handle);

#endif // PARSE_DIRECTIVE_H

/*==== End of file ====*/
