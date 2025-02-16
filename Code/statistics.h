/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    statistics.h
 * @author  B. Premzel
 * @brief   Statistical functions for the RTEmsg utility
 ******************************************************************************/

#include "main.h"

#ifndef _STATISTICS_HEADER_
#define _STATISTICS_HEADER_

void print_value_statistics(void);
void print_common_statistics(void);
void value_statistic(msg_data_t *p_fmt, value_format_t *format);
void write_statistics_to_file(void);
void reset_statistics(void);
void print_message_frequency_statistics(void);

#endif  // _STATISTICS_HEADER_

/*==== End of file ====*/
