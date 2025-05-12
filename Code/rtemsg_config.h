/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    rtemsg_config.h
 * @author  B. Premzel
 * @brief   Compile time configuration options for the RTEdbg message decoder.
 ******************************************************************************/

#ifndef _RTEMSG_CONFIG_H
#define _RTEMSG_CONFIG_H

#define RTEMSG_VERSION            1
#define RTEMSG_SUBVERSION         1
#define RTEMSG_REVISION           1

#define RTEMSG_DEBUG_MODE         0       // Print messages together with their indexes to Errors.log

#define RTEDBG_BUFFER_SIZE 0x20000        // Maximal number of 32 bit words loaded at once for large file processing
    /* Used for binary data loaded to the g_msg->rte_buffer for streaming or 'multi data' logging modes. */
    /* Should be at least twice the size of maximal logged message. */
    /* Larger binary files are loaded in chunks of maximally this size. */

#define MAX_RTEDBG_BUFFER_SIZE (uint32_t)(0x8000000 + 5)
    /* Max. number of 32b words (max. file size for post mortem or single-shot data decoding is 0.5 GiB)
     * Defines max. memory size used for the buffer with logged data */

#define MAX_ERRORS_REPORTED          20   // Maximal number of errors reported during the format file parsing
#define MIN_MAX_VALUES               10   // Number of min./max. values saved for each variable and timing statistics
#define TOP_MESSAGES                 10   // Number of top messages for which the statistics will be printed
#define MAX_FMT_ID_BITS             16u   // Max. number of index bits (2^N = max. number of different message types)
                                          // 16 = max. value to reserve 32 - 16 - 1 = minimally 15 bits for timestamps
#define NUMBER_OF_FILTER_BITS       32u   // This value is fixed (should not be modified)
#define MAX_ERRORS_IN_SINGLE_MESSAGE 10   // Maximal number of errors shown during single message decoding
#define MAX_FILE_OPEN_TIME         1500   // Max. time [ms] to wait if the fopen fails due to EACCES error

#define MAX_TXT_MESSAGE_LENGTH      500   // Max. line length for text in Messages.txt file
#define MAX_INPUT_LINE_LENGTH      2004   // Max. line length for the format definition files (2000 effective length)
#define MAX_FILE_MODE_LENGTH          5   // Maximal size of the OUT_FILE() file mode argument
#define MAX_SHORTENED_STRING         80   // Max. reported string length during data decoding error reporting
#define BIN_FILE_DATE_LENGTH         26   // For date string - i.e. "2023-06-23 14:30:55.999"
#define MAX_NO_OF_CHARS_PRINTED_FOR_ADDINFO_REPORTING 80 // Max. additional info reported during the fmt decoding
#define MAX_NAME_LENGTH             100   // Size limit for the file and filter names and filter descriptors
#define MAX_HEADGUARD_LENGTH         80   // Max. size of automatically generated headguard for the fmt header files
#define MAX_FILENAME_LENGTH   _MAX_PATH   // Maximal length of a file name
#define MAX_FILEPATH_LENGTH   _MAX_PATH   // Support for large paths is not yet implemented
#define CMP_BUFSIZ                 2048   // Size of buffers for the file compare
#define MAX_IN_FILE_SIZE      10000000LL  // Maximal IN_FILE() size
#define MAX_UTF8_TEXT_LENGTH MAX_FILEPATH_LENGTH   // Max. length of UTF-8 text (for string conversion)
#define MIN_STACK_SPACE        20000ULL   // Minimal stack space to continue code execution

// Thresholds for the -ts command line argument check.
// After changing these values, the text FATAL_BAD_TS_PARAMETER_VALUE has to be changed also.
#define MAX_NEGATIVE_TSTAMP_DIFF    0.33
#define MAX_POSITIVE_TSTAMP_DIFF    0.33
#define MIN_TIMESTAMP_DIFF          0.01
#define NORMALIZED_TSTAMP_PERIOD        (int64_t)(0x100000000ull)
#define DEFAULT_POSITIVE_TIMESTAMP_DIFF (int64_t)( MAX_POSITIVE_TSTAMP_DIFF * (double)NORMALIZED_TSTAMP_PERIOD)
#define DEFAULT_NEGATIVE_TIMESTAMP_DIFF (int64_t)(-0.10 * (double)NORMALIZED_TSTAMP_PERIOD)

#define MAX_RAW_DATA_SIZE 256u
  // Max. number of consecutive words in circular buffer with bit 0 = 0 (when no FMT word is found)
//#define STREAM_BUFF_SIZE (32u * 1024u)      // Experimental


#define MAX_ENUMS 2000u // Maximal number of enumerated filters, input / output files and memories
#if MAX_ENUMS <= 256u
#define rte_enum_t uint8_t
#elif MAX_ENUMS <= 65536u
#define rte_enum_t uint16_t
#else
#error "Value out of range"
#endif

// The default input files
#define RTE_MESSAGES_FILE          "Messages.txt"           // Error and other messages and printf strings
#define RTE_MAIN_FMT_FILE          "rte_main_fmt.h"         // Main format definition file

// Names of RTEmsg utility output files
#define RTE_MAIN_LOG_FILE          "Main.log"               // Main log file
#define RTE_ERR_FILE               "Errors.log"             // Error messages
#define RTE_FILTER_FILE            "Filter_names.txt"       // Filter descriptions
#define RTE_STAT_MAIN_FILE         "Stat_main.log"          // General statistics
#define RTE_STAT_VALUES_FILE       "Statistics.csv"         // Minimal, maximal and mean values of selected variables
#define RTE_STAT_MSG_COUNTERS_FILE "Stat_msgs_found.txt"    // Messages found during decoding
#define RTE_STAT_MISSING_MSGS_FILE "Stat_msgs_missing.txt"  // Messages that were not detected during decoding
#define RTE_MSG_TIMESTAMPS_FILE    "Timestamps.csv"         // Relative timestamp values
#define RTE_FORMAT_DBG_FILE        "Format.csv"             // Information about formatting data structures

#define DEFAULT_ERROR_REPORT       "%F:%L: error: ERR_%E %D => \"%A\"\n"
#define TXT_MSG_RTEMSG_VERSION     "RTEmsg v%u.%02u.%02u (Build date: %s)\n"

#define FIRST_FATAL_ERROR          30u                      // Number of first error

#endif  // _RTEMSG_CONFIG_H

/*==== End of file ====*/
