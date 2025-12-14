/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    print_message.h
 * @author  B. Premzel
 * @brief   Header file for functions related to printing message contents
 *          using the format definition linked lists prepared during the
 *          format definition file parsing.
 ******************************************************************************/

#ifndef _PRINT_MESSAGE_H
#define _PRINT_MESSAGE_H

#include "main.h"
#include "format.h"

void print_message(void);
void print_current_message_name(FILE* out, value_format_t* fmt);

#endif // _PRINT_MESSAGE_H

/*==== End of file ====*/
