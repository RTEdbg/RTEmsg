/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

 /*******************************************************************************
  * @file    vcd.h
  * @author  B. Premzel
  * @brief   Support for the generation of Value change dump (VCD) output files.
  ******************************************************************************/

#ifndef _VCD_H
#define _VCD_H

#include <stdio.h>
#include <stdint.h>
#include "parse_directive_helpers.h"

// Function prototypes
void vcd_add_text_to_string(const char* text);
void vcd_reset_structure(void);
void vcd_finalize_variable(rte_enum_t file_no);
void vcd_finalize_files(void);
bool is_a_vcd_file(const char* filename);
void vcd_print_double(const char* fmt_string, double value);
void vcd_print_uint(const char* fmt_string, uint64_t value);
void vcd_print_int(const char* fmt_string, int64_t value);
void vcd_print_string(const char* fmt_string, const char* text);
void vcd_check_variable_format(parse_handle_t* parse_handle, char* format_text);
void vcd_message_post_processing(void);
void vcd_write_pulse_var_data(void);

#endif  // _VCD_H

/*==== End of file ====*/
