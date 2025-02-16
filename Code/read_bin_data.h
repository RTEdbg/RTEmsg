/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    read_bin_data.h
 * @author  B. Premzel
 * @brief   Header file for reading data from the binary data file into memory
 *          and preparing it for decoding.
 ******************************************************************************/

#ifndef _READ_BIN_DATA_H
#define _READ_BIN_DATA_H

#include "main.h"

int  data_in_the_buffer(void);
void load_data_from_binary_file(void);
void print_bin_file_header_info(void);
void print_msg_intro(void);
void load_and_check_rtedbg_header(void);
void load_data_block(void);

#endif  // _READ_BIN_DATA_H

/*==== End of file ====*/
