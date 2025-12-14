/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    format.h
 * @author  B. Premzel
 * @brief   Helper and diagnostic functions for format definition processing.
 ******************************************************************************/

#ifndef _FORMAT_H
#define _FORMAT_H

#include "main.h"


/**
 * @brief Data structure for statistics about values found in messages.
 *        Several min. and max. values are logged together with
 *        the numbers of messages where the values appeared.
 */
typedef struct
{
    const char *name;                    //!< Name of value for which the statistics will be done
    double sum;                          //!< Sum of values - used to calculate the average value
    uint32_t counter;                    //!< How many times the value has been added to the sum
    uint32_t min_msg_no[MIN_MAX_VALUES]; //!< Message numbers where the min. values were found
    uint32_t max_msg_no[MIN_MAX_VALUES]; //!< Message numbers where the max. values were found
    double min[MIN_MAX_VALUES];          //!< Minimal values
    double max[MIN_MAX_VALUES];          //!< Maximal values
} value_stats_t;


/**
 * @brief Definitions of value types which will be printed using the fprintf function.
 *        Values in comments indicate which format definitions each enum refers to.
 *        Only the formatting type characters defined here are allowed for the RTEmsg.
 */
enum fmt_type_t
{
    PRINT_PLAIN_TEXT,           // No format type field character was found in the string
    PRINT_STRING,               // "%s"
    PRINT_SELECTED_TEXT,        // "%Y"
    PRINT_UINT64,               // "%u", "%c", "%x", "%o", "%X", "%lu", "%lx", "%lX", "%lo" etc.
    PRINT_INT64,                // "%d", "%i", "%ld", "%li" etc.
    PRINT_DOUBLE,               // "%f", "%F", "%e", "%E", "%g", "%G", "%a", "%A"
    PRINT_BINARY,               // "%B"
    PRINT_TIMESTAMP,            // "%t"
    PRINT_dTIMESTAMP,           // "%T"
    PRINT_MSG_NO,               // "%N"
    PRINT_HEX1U,                // "%1H"
    PRINT_HEX2U,                // "%2H"
    PRINT_HEX4U,                // "%4H"
    PRINT_BIN_TO_FILE,          // "%W"
    PRINT_DATE,                 // "%D"
    PRINT_MSG_FMT_ID_NAME       // "%M"
    // Not assigned:  *C, I, J, K, O, P, Q, R, *S, U, V, *Z, b, j, k, m, *n, *p, q, r, v, w, y, z
    //  '*' - see special types: https://learn.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=msvc-170
};


/**
 * @brief Definition of value types that will be printed (type of value after the '%').
 */
enum data_type_t
{
    VALUE_AUTO,                 // Automatically convert a 32-bit value to either float, unsigned or signed integer
    VALUE_UINT64,               // Unsigned integers up to 64 bits
    VALUE_INT64,                // Signed integers up to 64 bits
    VALUE_DOUBLE,               // Float (32b) or double (64b) values - depends on specified data size
    VALUE_STRING,               // Strings with unknown length (zero terminated)
    VALUE_TIMESTAMP,            // Value of current timestamp
    VALUE_dTIMESTAMP,           // Difference between the current timestamp and timestamp of the previous (same) value
    VALUE_MEMO,                 // Value from specified memory
    VALUE_TIME_DIFF,            // Use difference between the timestamps (current message - specified message)
    VALUE_MESSAGE_NO            // Number of current message
};


/**
 * @brief Formatting values for a single data value (single number or other data type).
 *        If a message contains more than one value then a linked list of structures is used.
 */
typedef struct value_format
{
    const char *fmt_string;         /*!< Formatting string for this data value */
    rte_enum_t out_file;            /*!< File to which the data will be printed (0 => print to "Main.log" only) */
    rte_enum_t in_file;             /*!< Index to structure with IN_FILE data - for the '%Y' directive */
                                    /*@note The same structure is used for text defined with {text1|text2|...}*/
    rte_enum_t get_memo;            /*!< Index to MEMO structure from which the value will be read (NULL = OFF) */
    rte_enum_t put_memo;            /*!< Index to MEMO structure to which the value will be stored (NULL = OFF) */
    uint32_t fmt_id_timer;          /*!< Format ID of message which is used for calculation of time difference */
    uint32_t bit_address;           /*!< Address of the first data bit in a message belonging to this value */
    uint32_t data_size;             /*!< Size of data to be printed [number of bits] */
    enum data_type_t data_type;     /*!< Type of data formatting (signed, unsigned, float, ...) */
    enum fmt_type_t fmt_type;       /*!< Which data type should be used for the fprintf() */
    bool print_copy_to_main_log;    /*!< != 0 => Print copy of data printed to defined file to Main.log file. */
    special_fmt_t special_fmt;      /*!< != 0 => Special formating requirement. */
    double mult;                    /*!< Multiplier for data scaling (0 = no scaling) */
    double offset;                  /*!< Offset added before the multiplication */
    value_stats_t *value_stat;      /*!< Value statistics (NULL = no statistics for this value) */
    struct value_format *format;    /*!< Pointer to the formatting data for the next value (NULL = end of linked list) */
} value_format_t;


/*@brief Define type of message for which the decoding function must expect. */
enum msg_type_t
{
    TYPE_MSG0_8,                    /*!< For MSG0 .. MSGnn - known length at compile time */
    TYPE_MSGN,                      /*!< For MSGN - known and unknown length (0 = unknown) */
    TYPE_EXT_MSG,                   /*!< For EXT_MSG0..4 - known length */
    TYPE_MSGX                       /*!< For MSGX - unknown length */
};

/**
 * @brief Structure containing information for one message type
 */
typedef struct
{
    const char *message_name;       /*!< Name of this message type - i.e. MSG2_NAME */
    enum msg_type_t msg_type;       /*!< Type of message (most message types have known length) */
    bool add_nl_to_main_log;        /*!< true - add newline before next message in Main.log */
    uint16_t ext_data_mask;         /*!< AND mask used to select the extended info from the format_id */
    uint32_t msg_len;               /*!< Expected message length in bytes (0 - unknown at compile time) */
    uint32_t counter;               /*!< Number of same message type received and successfully processed after
                                     *   reset, sleep or single-shot, snap-shot. */
    uint32_t counter_total;         /*!< Total number of same message type received and successfully processed */
    uint32_t total_data_received;   /*!< Total number of words received with this message type - including the FMT word */
    double time_last_message;       /*!< Time stamp value [s] - time when the last message was logged */
    value_format_t *format;         /*!< Pointer to start of a linked list with formatting data */
} msg_data_t;


/***** Global variables *****/
extern msg_data_t *g_fmt[MAX_FMT_IDS]; /*!< Pointers to structures with formatting definitions */

#define MSG_NAME_NOT_FOUND 0xFFFFFFFF


/***** Function declarations *****/
void print_format_decoding_information(void);
const char *get_format_id_name(unsigned fmt_id);
void print_format_id_name(FILE *out);
unsigned assign_fmt_id(unsigned no_fmt_ids, msg_data_t *p_fmt);
uint32_t find_message_format_index(const char *name);

#endif   // _FORMAT_H

/*==== End of file ====*/
