/*
 * Copyright (c) 2024 Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    parse_directive_msg.h
 * @author  B. Premzel
 * @brief   Functions for parsing MSG, MSGN, MSGX, and EXT_MSG formatting directives.
 ******************************************************************************/

#ifndef PARSE_DIRECTIVE_MSG_H
#define PARSE_DIRECTIVE_MSG_H

#include "format.h"
#include "parse_directive_helpers.h"

typedef unsigned (*parse_msg_no_t)(parse_handle_t *parse_handle);

void parse_msg_directives(parse_handle_t *parse_handle);

#endif // PARSE_DIRECTIVE_MSG_H

/*==== End of file ====*/
