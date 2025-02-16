/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    parse_file_handling.h
 * @authors S. Milivojcev, B. Premzel
 * @brief   Function declarations for handling format definition files.
 ******************************************************************************/

#ifndef _PARSE_FILE_HANDLING_H
#define _PARSE_FILE_HANDLING_H

#include "parse_directive_helpers.h"

/* Function declarations for file handling operations */
FILE *create_file(char *filename, char *initial_text, const char *write_type);
bool setup_parse_files(parse_handle_t *parse_handle);
void check_and_replace_work_file(parse_handle_t *parse_handle);
void read_file_to_indexed_text(const char *filename, parse_handle_t *parse_handle);
void write_define_to_work_file(parse_handle_t *parse_handle, const char *name, unsigned int value);

#endif // _PARSE_FILE_HANDLING_H

/*==== End of file ====*/
