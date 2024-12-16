/*
 * Copyright (c) 2024 Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    decoder.h
 * @author  B. Premzel
 * @brief   Functions for binary data file processing.
 ******************************************************************************/

#ifndef DECODER_H_
#define DECODER_H_

#include "main.h"

void process_message(uint32_t last_index);
void process_escape_sequences(char *message, size_t max_length);
void report_no_definition_for_current_message(unsigned fmt_id, uint32_t last_index);

#endif /* DECODER_H_ */

/*==== End of file ====*/
