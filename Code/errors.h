/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    errors.h
 * @author  B. Premzel
 * @brief   General error reporting and handling of issues during binary data
 *          file processing.
 ******************************************************************************/

#ifndef _ERRORS_H
#define _ERRORS_H

#include "main.h"

// Exit codes for various error scenarios
#define EXIT_FATAL_FMT_PARSING_ERRORS            1u      // Errors during command file or format definition processing
#define EXIT_FATAL_DECODING_ERRORS_DETECTED      2u      // Fatal errors during binary file processing
#define EXIT_NON_FATAL_DECODING_ERRORS_DETECTED  3u      // Non-fatal errors during binary file processing
#define EXIT_FAST_FAIL_INCORRECT_STACK           4u      // Incorrect program stack (stack space exhausted)
#define EXIT_FATAL_EXCEPTION_DETECTED_FMT        5u      // Fatal exception during format file decoding
#define EXIT_FATAL_EXCEPTION_DETECTED_BIN        6u      // Fatal exception during binary file decoding

// Exit values for fatal errors that cannot be logged to Error.log
// These errors may occur before Error.log is created
#define EXIT_FATAL_ERR_GETCWD_START             10u      // Error getting current working directory
#define EXIT_FATAL_ERR_PGMPTR                   11u      // Error getting program path
#define EXIT_FATAL_ERR_PGMFOLDER                12u      // Error getting program folder
#define EXIT_FATAL_ERR_START_FOLDER             13u      // Error with start folder
#define EXIT_FATAL_ERR_OUTPUT_FOLDER            14u      // Error with output folder
#define EXIT_FATAL_ERR_CREATE_ERR_FILE          15u      // Error creating error log file
#define EXIT_FATAL_ERR_BAD_PARAMETERS           16u      // Invalid command line parameters
#define EXIT_FATAL_ERR_FAULTY_MESSAGES_FILE     17u      // Error with messages file format
#define EXIT_FATAL_ERR_CANNOT_OPEN_MESSAGES_TXT 18u      // Cannot open messages.txt file
// Note: Numbers up to and including 29 are reserved for fatal error codes

// Enumeration for internal error reporting
enum internal_errors_t
{
    INT_SET_MEMO_OUT_OF_RANGE = 1,            // Memo index out of valid range during set
    INT_GET_MEMO_OUT_OF_RANGE,                // Memo index out of valid range during get
    INT_SET_MEMO_TYPE_IS_NOT_MEMO,            // Attempting to set non-memo type as memo
    INT_GET_MEMO_TYPE_IS_NOT_MEMO,            // Attempting to get non-memo type as memo
    INT_DECODING_SYS_MESSAGE,                 // Error decoding system message
    INT_INCORRECT_AUTO_VALUE_TYPE,            // Invalid auto value type
    INT_FMT_ID_OUT_OF_RANGE,                  // Format ID outside valid range
    INT_FMT_STRING_NULL,                      // Format string is NULL
    INT_BAD_DATA_TYPE,                        // Invalid data type
    INT_DECODE_INTERNAL_UNKNOWN_TYPE,         // Unknown type during internal decoding
    INT_OUT_FILE_INDEX_OUT_OF_RANGE,          // Output file index out of valid range
    INT_BAD_OUT_FILE_TYPE,                    // Invalid output file type
    INT_OUT_FILE_PTR_NULL,                    // Output file pointer is NULL
    INT_DECODE_Y_TYPE_STRING,                 // Error decoding Y-type string
    INT_DECODE_Y_TYPE_STRING_NULL             // Y-type string is NULL
};

// Function declarations for error reporting
__declspec(noreturn) void report_fatal_error_and_exit(uint32_t error_code,
    const char *additional_text, size_t additional_value);
void report_problem(uint32_t error_code, int additional_data);
void report_problem2(uint32_t error_code, uint32_t additional_data,
    uint32_t additional_data2);
void report_problem_with_string(uint32_t error_code, const char *name);
__declspec(noreturn) void report_error_and_exit(const char *error_message, int exit_code);
__declspec(noreturn) void report_error_and_show_instructions(const char *error_message,
    const char *msg_extension);
void report_decode_error_summary(void);

#endif  // _ERRORS_H

/*==== End of file ====*/
