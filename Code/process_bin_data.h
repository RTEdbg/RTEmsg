/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    process_bin_data.h
 * @author  B. Premzel
 * @brief   Header file for functions related to processing raw binary data from
 *          the input file and debugging message output.
 ******************************************************************************/

#ifndef _PROCESS_BIN_DATA_H
#define _PROCESS_BIN_DATA_H

#include "main.h"

void process_bin_data_worker(void);
void debug_print_message_info(uint32_t last_index);
void debug_print_message_hex(uint32_t start_index);

#endif // _PROCESS_BIN_DATA_H

/*==== End of file ====*/
