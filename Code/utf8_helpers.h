/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    utf8_helpers.h
 * @author  B. Premzel
 * @brief   Header file for supporting functions that handle UTF-8 filenames
 *          and console output
 ******************************************************************************/

#ifndef _UTF8_HELPERS_H
#define _UTF8_HELPERS_H

#include <stdio.h>  // For FILE type definition

FILE * utf8_fopen(const char *filename, const char *mode);
void utf8_print_string(const char *text, size_t length);
int utf8_remove(const char *name);
int utf8_rename(const char *old_name, const char *new_name);
int utf8_chdir(const char *dir_name);
size_t utf8_truncate(const char *text, size_t length);

#endif // _UTF8_HELPERS_H

/*==== End of file ====*/
