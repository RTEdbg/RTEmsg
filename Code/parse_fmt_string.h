/*
 * Copyright (c) 2024 Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    parse_fmt_string.h
 * @authors S. Milivojcev, B. Premzel
 * @brief   Header file for parsing formatting strings in the format definition files.
 ******************************************************************************/

#ifndef PARSE_FMT_STRING_H
#define PARSE_FMT_STRING_H

#include "parse_directive_helpers.h"

void separate_fmt_strings(char *buffer, parse_handle_t *parse_handle);

#endif // PARSE_FMT_STRING_H

/*==== End of file ====*/
