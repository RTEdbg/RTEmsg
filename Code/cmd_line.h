/*
 * Copyright (c) 2024 Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/******************************************************************************
 * @file    cmd_line.h
 * @author  B. Premzel
 * @brief   Command line parameter processing functions
 ******************************************************************************/

#ifndef CMD_LINE_H_
#define CMD_LINE_H_

#include "main.h"

void process_command_line_parameters(int argc, char *argv[]);
void check_timestamp_diff_values(void);

#endif /* CMD_LINE_H_ */

/*==== End of file ====*/
