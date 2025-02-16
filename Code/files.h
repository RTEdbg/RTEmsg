/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    files.h
 * @author  B. Premzel
 * @brief   Helper functions for file handling and removing old files.
 ******************************************************************************/

#ifndef _FILES_H
#define _FILES_H

#include <stdio.h>
#include <stdint.h>

void create_error_file(void);
void create_main_log_file(void);
void open_output_folder(void);
void open_format_folder(void);
void jump_to_start_folder(void);
void setup_working_folder_info(void);
int64_t get_file_size(FILE *fp);
char *prepare_folder_name(char *name, unsigned error_code);
void remove_old_files(void);
void remove_file(const char *file_name);

#endif  // _FILES_H

/*==== End of file ====*/
