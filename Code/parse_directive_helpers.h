/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    parse_directive_helpers.h
 * @authors S. Milivojcev, B. Premzel
 * @brief   Various helper functions for parsing RTEmsg directives.
 ******************************************************************************/

#ifndef PARSE_DIRECTIVE_HELPERS_H
#define PARSE_DIRECTIVE_HELPERS_H

#include <stdbool.h>
#include <setjmp.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "format.h"


struct parse_handle_str
{
    // Error handling during message parsing
    jmp_buf jump_point;                 /*!< Jump point for error handling */
    const char *err_position;           /*!< Position in the input line from which the error will be reported */
    unsigned file_line_num;             /*!< Line number of the currently processed line */
    bool parsing_errors_found;          /*!< true if errors were found during format file parsing */
    struct parse_handle_str *p_parse_parent; /*!< Pointer to the parse_handle of the function that recursively called parse_fmt_file() */

    // Currently open files
    bool write_output_to_header;        /*!< Write the parsing output to a file with the ending ".fmt.h" */
    FILE *p_fmt_file;                   /*!< Pointer to the currently processed format definition file */
    FILE *p_fmt_work_file;              /*!< Pointer to the current work file */
    char work_file_name[MAX_FILENAME_LENGTH]; /*!< Name of the work file (output file) */
    const char *fmt_file_path;          /*!< File name of the currently parsed format definition file */
    char **p_file_line_curr_pos;        /*!< Position in the currently processed line */

    // Pointers to currently used or processed data structures
    msg_data_t *p_current_message;      /*!< Message currently used for the format IDs */
    msg_data_t *p_new_message;          /*!< Message that will replace the current one */
    msg_data_t *p_prev_msg;             /*!< Previously processed message data structure */
    value_format_t *current_format;     /*!< Pointer to the currently processed formatting structure (for one value) */

    // Directives found during a single line processing
    struct found_directive_flags
    {
        bool in_file_select;            /*!< <IN_FILE directive found */
        bool out_file_select;           /*!< >OUT_FILE directive found */
        bool value_spec;                /*!< [value] - value definition found for the current message */
        bool indexed_text;              /*!< |text| - for the %Y type */
    }
    found;

    // Indexes of input and output files for the <IN_FILE, >OUT_FILE, and >>OUT_FILE
    rte_enum_t current_in_file_idx;     /*!< Index of the current <IN_FILE */
    rte_enum_t prev_out_file_idx;       /*!< Output file index for the previously processed message */
    rte_enum_t current_out_file_idx;    /*!< Index of the current >OUT_FILE */
    bool print_to_main_log;             /*!< true if the value should also be printed to the Main.log file */
    special_fmt_t special_fmt;          /*!< Special formatting requirements such as VCD file processing. */
    bool special_fmt_detected;          /*!< false - special_fmt was VCD_NONE before, true - special_fmt > VCD_NONE */
};

typedef struct parse_handle_str parse_handle_t;


/**********  Inline functions  ************/

/**
 * @brief  Skips whitespace characters at the start of pos.
 *
 * @param pos   Current position in text. Modified when finished skipping.
 */

static inline void skip_whitespace(char **pos)
{
    char *p = *pos;

    while (isspace((unsigned char)*p))
    {
        p++;
    }

    *pos = p;
}


/******** Function declarations ********/
rte_enum_t find_enum_idx(char *enum_name, enum enums_type_t enum_type);
void file_name_used_before(parse_handle_t *parse_handle, char *file_name, enum enums_type_t enum_type);
unsigned parse_unsigned_int(parse_handle_t *parse_handle);
char *parse_directive_name(parse_handle_t *parse_handle, const char *name_prefix);
void parse_name(parse_handle_t *parse_handle, char *name);
bool parse_until_specified_character(char **position, char *result, size_t resultSize, char stop_char);
bool parse_quoted_arg(char **position, char *buffer, size_t resultSize);
void parse_file_path_arg(parse_handle_t *parse_handle, char *filePathBuff, size_t max_length);
void set_tags_for_add_newline_to_main_log(void);

#endif // PARSE_DIRECTIVE_HELPERS_H

/*==== End of file ====*/
