/*
 * Copyright (c) 2024 Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    print_helper.h
 * @author  B. Premzel
 * @brief   Header file for helper functions related to message printing.
 ******************************************************************************/

#ifndef _PRINT_HELPER_H
#define _PRINT_HELPER_H

#include "main.h"
#include "format.h"

void print_message_number(FILE *out, uint32_t msg_no);
void print_timestamp(FILE *out, double timestamp);
void dump_filter_names_to_file(void);
void hex_dump_current_message(bool print_words);
const char *strip_newlines_and_shorten_string(const char *text, char spec_char);
void save_internal_decoding_error(uint32_t sys_error, uint32_t data2);
void save_decoding_error(uint32_t err_no, uint32_t data1, uint32_t data2, const char *fmt_text);
void print_decoding_errors(void);

#endif  // _PRINT_HELPER_H

/*==== End of file ====*/
