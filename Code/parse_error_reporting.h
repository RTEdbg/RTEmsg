/*
 * Copyright (c) 2024 Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    parse_error_reporting.h
 * @authors B. Premzel, S. Milivojcev
 * @brief   Error reporting during the format definition file processing.
 ******************************************************************************/

#ifndef PARSE_ERROR_REPORTING_H
#define PARSE_ERROR_REPORTING_H

#include "parse_directive_helpers.h"

void report_parsing_error(parse_handle_t *parse_handle, error_msg_t parsing_error, const char *err_context);

__declspec(noreturn)
void catch_parsing_error(parse_handle_t *parse_handle, error_msg_t parsing_error, const char *err_context);

#endif // PARSE_ERROR_REPORTING_H

/*==== End of file ====*/
