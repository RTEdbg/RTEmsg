/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    main.h
 * @author  B. Premzel
 * @brief   Definition of working variables for the binary file processing.
 ******************************************************************************/

#ifndef _MAIN_H
#define _MAIN_H

#define _CRT_SECURE_NO_WARNINGS 1   // Disable deprecation warnings

#include <stdio.h>
#include <stdbool.h>
#include "rtemsg_config.h"
#include "rtedbg.h"
#include "messages.h"


#define MAX_FMT_IDS  (1ul << MAX_FMT_ID_BITS)


/* @brief Values returned by the functions assemble_message() and data_in_the_buffer() */
typedef enum assemble_msg_code_t
{
    FMT_WORD_OK,            /*!< The FMT word was found */
    NO_DATA_FOUND,          /*!< The buffer is empty - containing just 0xFFFFFFFF values */
    DATA_FOUND,             /*!< Valid message was found and assembled */
    BAD_BLOCK,              /*!< Data block found without a valid fmt_index/timestamp word */
    UNFINISHED_BLOCK,       /*!< Unfinished block found (one or more consecutive 0xFFFFFFFF values) */
    MESSAGE_TOO_LONG,       /*!< Message longer than 4 * RTE_MAX_SUBPACKETS */
    END_OF_BUFFER           /*!< End of buffer reached */
} asm_msg_t;


/* @brief Structure containing command line parameters */
typedef struct _param_t
{
    char *working_folder;               //!< Output folder in which all output files are created
    char *fmt_folder;                   //!< Folder containing formatting specification files
    char *data_file_name;               //!< Binary data file name
    bool check_syntax_and_compile;      //!< Check the format file syntax and generate format definition headers
    bool create_backup;                 //!< Enable the parser to generate file backups
    bool value_statistics_enabled;      //!< Execute statistics and print report
    bool message_statistics_enabled;    //!< Generate files with information about number of received messages and missed messages
    bool debug;                         //!< Enable additional debugging support
    bool create_timestamp_file;         //!< Generate statistic files for check of timestamps
    bool purge_defines;                 //!< Eliminate all #define directives from the format files during parsing
    bool additional_newline;            //!< Print additional newline after information for every message to Main.log
    bool codepage_utf8;                 //!< Use the CP_UTF8 while printing the parsing error messages to the console
    char time_unit;                     //!< Specify time unit for the timestamps
    double time_multiplier;             //!< Time multiplier - used for printing of timestamps
    char number_of_format_id_bits;      //!< Number of bits used for the format ID
    char *locale_name;                  //!< String defined with option "-locale=..."
    char *timestamp_print;              //!< String defined with command line parameter "-T=..."
    char *msg_number_print;             //!< String defined with command line parameter "-no=..."
    char *report_error;                 //!< Error reporting definition
    int64_t max_positive_tstamp_diff;   //!< Max. timestamp difference for an incrementing timestamp
    int64_t max_negative_tstamp_diff;   //!< Max. difference for a timestamp that is behind the previous one
    double max_positive_tstamp_diff_f;  //!< Max. positive difference in ms - command line argument
    double max_negative_tstamp_diff_f;  //!< Max. negative difference in ms - command line argument
} param_t;


typedef struct _rte_files_t
{
    wchar_t *start_folder;          /*!< Pointer to the folder from which the software has been started */

    // Working FILE pointers (files used during binary data file parsing)
    FILE *rte_data;                 /*!< Pointer to the data logging structure  - loaded from binary file */
    FILE *main_log;                 /*!< Pointer to the Main.log file structure */
    FILE *error_log;                /*!< Pointer to the Error.log file structure */
    FILE *statistics_log;           /*!< Pointer to the Stat_main.txt file structure */
    FILE *timestamps;               /*!< Pointer to the Timestamps.csv file structure */
} rte_files_t;


/**
 * @brief The following values define what data was read from the binary data file.
 *        Logging mode: g_msg->hdr_data.logging_mode
 */
typedef enum
{
    MODE_UNKNOWN           = 0,
    MODE_POST_MORTEM       = 1,             //!< Post mortem data
    MODE_SINGLE_SHOT       = 2,             //!< Single shot data
    MODE_STREAMING         = 0xFFFFFFF0uL,  //!< Streaming mode data logging
    MULTIPLE_DATA_CAPTURE  = 0xFFFFFFF4uL   //!< Multiple single shot snapshots
} logging_mode_t;


typedef struct _header_data_t
{
    /* Data from the RTEdbg structure header - prepared for use */
    unsigned topmost_fmt_id;           /*!< Number of top format ID used for streaming logging modes */
                                       /* All format IDs logged with the embedded firmware must be less than this value */
    bool buffer_size_is_power_of_2;    /*!< TRUE - Data buffer size is a power of 2 */
    bool single_shot_enabled;          /*!< TRUE - single shot logging enabled at compile time */
    bool long_timestamp_used;          /*!< TRUE - long timestamp functionality was used in the embedded project */
    bool single_shot_active;           /*!< TRUE - data was logged in the single shot mode */
    logging_mode_t logging_mode;       /*!< Data logging mode */
    uint16_t max_msg_blocks;           /*!< Maximal number of message blocks in logged message */
    uint8_t  timestamp_shift;          /*!< Timestamp timer frequency is divided by (2^timestamp_shift) */
    uint8_t  fmt_id_bits;              /*!< Number of bits used for the format ID encoding */
    uint8_t  fmt_id_shift;             /*!< Shift the FMT word right by this value to get the format ID */
    uint32_t timestamp_and_index_mask; /*!< AND value used for cutting off the additional data and bit 0
                                          from the FMT word */
} rte_header_data_t;


enum enums_type_t
{
    FILTER_TYPE,                    // FILTER() data
    OUT_FILE_TYPE,                  // OUT_FILE() data
    IN_FILE_TYPE,                   // IN_FILE() data
    Y_TEXT_TYPE,                    // Data defined inside of formatting definition with {text1|text2|...}
    MEMO_TYPE                       // MEMO() data
};

/**
 * @brief Data for enums: rtedbg_filter, rtedbg_out_file, rtedbg_in_file and rtedbg_memo
 */
typedef struct _enum_data_t
{
    char *name;                      /*!< Name of the enumerated value: filter, memo, in_file, out_file */
    enum enums_type_t type;          /*!< Type of data in the union */
    union def_union
    {
        char *filter_description;    /*!< FILTER:   pointer to filter description (NULL if not specified) */
        FILE *p_file;                /*!< OUT_FILE: pointer to the file structure */
        char *in_file_txt;           /*!< IN_FILE and Y_TEXT_TYPE:  
                                        pointer to the first text loaded from the file or
                                        to text defined with {text1|text2|...} 
                                        See the description of function 'get_selected_text()' for
                                        how the data is formatted - how it has to be prepared */
        double memo_value;           /*!< MEMO:     memorizing of temporary values */
    };
    char *file_name;                 /*!< Name of the file defined with OUT_FILE() or IN_FILE() */
} enum_data_t;


/* @brief Structure with values prepared for printing.
          The value is always prepared as integer, unsigned integer and double.
*/
typedef struct _value_t
{
    double   data_double;     //!< Currently processed (printed) value as floating point 
    int64_t  data_i64;        //!< The same value as a 64-bit integer
    uint64_t data_u64;        //!< The same value as a 64-bit unsigned integer
    uint64_t data_null;       //!< Zeroes - in case the uint64 is printed as a zero terminated string
} value_t;


/**
 * @brief Errors are logged to a data structure if the immediate printing would
 *        interfere with the message printing process. A zero value is used for
 *        all printed values that cannot be retrieved correctly from the message.
 *        Errors are printed after the current message is printed completely.
 */
typedef struct _error_log_t
{
    uint32_t error_number;    //!< Error number which has been detected during decoding
    uint32_t value_number;    //!< Consecutive number of executed fprintf for a single message 
                              //!< (PRINT_PLAIN_TEXT not counted)
    uint32_t data1;           //!< Additional data saved
    uint32_t data2;           //!< Additional data saved
    const char *fmt_text;     //!< Pointer to formatting string for which the problem was found
} error_log_t;


/* Timestamp processing */
typedef struct _timestamp_t
{
    double   f;                     /*!< Full timestamp of current message in seconds */
    double   multiplier;            /*!< Multiplier for calculation of time[seconds] from the 64-bit integer value */
    uint32_t current_frequency;     /*!< Timestamp frequency used for calculation of full timestamp values */
    uint32_t h;                     /*!< Highest 32 bits of the timestamp; total length = 64 - 1 - RTE_FMT_ID_BITS */
    uint32_t l;                     /*!< Normalized lower part of the last message timestamp - shifted left by N+5 */
    uint32_t old;                   /*!< Value of timestamp.l from previous message (not updated if the last one was from a delayed write) */
    uint32_t searched_to_index;     /*!< Index in bin data buffer up to which the search for the long_timestamp was done already */
    uint32_t msg_long_tstamp_incremented; /*!< Message number when the timestamp.h was incremented */
    uint32_t suspicious_timestamp;  /*!< Suspicious timestamp detected - number of such messages */
    bool mark_problematic_tstamps;  /*!< Add asterisk before the message number */
    bool no_previous_tstamp;        /*!< The timestamp.old value is not valid */
    bool long_timestamp_found;      /*!< At least one long timestamp found */
} timestamp_t;


/* @brief Main data structure */
typedef struct _rte_msg_t
{
    rte_files_t file;                  /*!< Pointers to input and output files used during data decoding */
    param_t param;                     /*!< Parsed values of command line parameters */
    rtedbg_header_t rte_header;        /*!< Header of the embedded system debug structure dbgData */
    rte_header_data_t hdr_data;        /*!< Pre-processed data from the RTEdbg structure header */

    /* Variables for the currently processed message from the binary data file */
    timestamp_t timestamp;
    uint32_t fmt_id;                    /*!< Format ID of currently processed message */
    uint32_t additional_data;           /*!< Additional data packed together with timestamp/format ID to the same word */
    uint32_t asm_words;                 /*!< Assembled message size [# DATA words] - not including additional data */
    uint32_t asm_size;                  /*!< Assembled message size [bytes] - including additional data word if available */
    uint32_t *assembled_msg;            /*!< Message data (including additional data bits) */

    /* Various values */
    char date_string[BIN_FILE_DATE_LENGTH]; /*!< String with date and time of binary data file creation - for "%D" */
    uint32_t messages_processed_after_restart; /*!< Counter of messages processed after reset/restart */
    value_t value;                      /*!< Currently processed/printed numerical value */

    /* Binary data file processing variables */
    uint32_t index;                     /*!< Index to the rte_buffer */
    uint32_t message_cnt;               /*!< Counter of all messages found - including messages with problems */
    uint32_t multiple_logging;          /*!< Number of separate snapshots in the binary data file */
    size_t   already_processed_data;    /*!< Total number of data already processed in the working buffer */
    uint32_t in_size;                   /*!< Total size of the loaded buffer [number of words] */
    uint32_t error_warning_in_msg;      /*!< Number of message in which a warning is displayed after the error(s) - if any */
    uint32_t *rte_buffer;               /*!< Pointer to data from the embedded system circular data logging buffer */
    uint32_t rte_buffer_size;           /*!< Size of the allocated memory for the buffer [32b words] */
    uint32_t raw_data[MAX_RAW_DATA_SIZE + 8u]; /*!< Raw data copied from the rte_buffer */
    bool     complete_file_loaded;      /*!< true - binary file completely loaded, false - partially loaded */

    /* Enumerated value types: IN_FILE, OUT_FILE, MEMO, FILTER, inline selected text definition */
    enum_data_t enums[MAX_ENUMS + 1u];  /*!< Enumerated values: filters, memos, in_files and out_files.
                                         *   The first 32 entries are reserved for the 32 filters. The following 
                                         *   ones are MEMO, IN_FILE, OUT_FILE, and selected text definitions */
    uint32_t enums_found;               /*!< Number of enums found in the format definition file(s) */
    uint32_t filter_enums;              /*!< Number of filters (up to 32 may be defined) */

    /* Variables for the format ID number assignment */
    uint32_t fmt_ids_defined;           /*!< Number includes empty space reserved using ALIGN */
    uint32_t fmt_align_value;           /*!< Minimal value of the next format ID */

    /* Error counting during a single message decoding */
    uint32_t unfinished_words;          /*!< Number of consecutive words with a value of 0xFFFFFFFF.
                                         *   Such a value indicates that the default value in the buffer was not 
                                         * overwritten with the logged one during writing to the circular buffer.*/
    uint32_t bad_packet_words;          /*!< Number of DATA words in a packet without a FMT word */
    
    /* Error information logged during a single message decoding */
    error_log_t error_log[MAX_ERRORS_IN_SINGLE_MESSAGE];
    uint32_t msg_error_counter;         /*!< Errors detected during single message decoding */
    uint32_t error_value_no;            /*!< 0 = first decoded value of message, 1 = second one, etc. */
                                        /* The number applies to the %x - x = type */
    /* General error counters */
    uint32_t total_unfinished_words;    /*!< Total number of unfinished_words containing 0xFFFFFFFF */
    uint32_t total_bad_packet_words;    /*!< Total number of bad_packet_words */
    uint32_t total_errors;              /*!< Total number of errors detected during the RTEmsg app execution */
    uint32_t error_counter[TOTAL_ERRORS+1]; /*!< Counters for individual error message types */
    bool binary_file_decoding_finished; /*!< true - the binary file decoding finished normally */

    // Messages loaded from the Message.txt file
    char *message_text[TOTAL_MESSAGES+1];  /*!< Pointers to the text messages loaded from the file */
} rte_msg_t;


/***** Global variables *****/
extern rte_msg_t g_msg;                 /*!< Main global data structure for the binary data file decoding */


/***** Function declarations *****/
void *allocate_memory(size_t size, const char *memory_name);
char *duplicate_string(const char *string_to_duplicate);
bool   is_power_of_two(size_t n);

#endif   // _MAIN_H

/*==== End of file ====*/
