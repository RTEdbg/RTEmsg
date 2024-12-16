/*
 * Copyright (c) 2024 Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/******************************************************************************
 * @file    decoder.c
 * @author  B. Premzel
 * @brief   Functions for the binary data file processing.
 ******************************************************************************/

#include "pch.h"
#include <ctype.h>
#include <memory.h>
#include "decoder.h"
#include "errors.h"
#include "print_message.h"
#include "print_helper.h"
#include "statistics.h"
#include "process_bin_data.h"
#include "timestamp.h"


/**
 * @brief Prepare date and time string and write it to 'g_msg.date_string'
 *
 * The coding scheme for special system messages specifies the following:
 * The timestamp size is minimally 16 bits. The bottom 16 timestamp bits have the following
 * meaning for the system messages:
 *   - bit 15: must be zero to prevent the possibility of 0xFFFFFFFF for the FMT word
 *   - bits 11 .. 14: define codes for special system information blocks (value: 0 .. 15)
 *   - bits 0 ... 10: top 11 bits of the 43-bit date/time info
 *
 * The 43 bits of the date/time information represent:
 * Description                         No of bits (bits from to)
 *----------------------------------------------------------------
 * Thousands of seconds (000 .. 999)  10   (00 .. 09)
 * Seconds after minute (0 - 59)       6   (10 .. 15)
 * Minutes after hour (0 - 59)         6   (16 .. 21)
 * Hours since midnight (0 - 23)       5   (22 .. 26)
 * Day of month (1 - 31)               5   (27 .. 31)
 * Month (0 - 11; January = 0)         4   (32 .. 35)
 * Year (current year minus 2023)      7   (36 .. 42)
 */

static void prepare_date_and_time_string(void)
{
    uint64_t date_time = g_msg.assembled_msg[0]
        | (((uint64_t)(g_msg.timestamp.l >> (g_msg.hdr_data.fmt_id_bits + 1ul)) & 0x7FFull) << 32ul);
    snprintf(g_msg.date_string, sizeof(g_msg.date_string) - 1, "%04u-%02u-%02u %02u:%02u:%02u.%03u",
        (unsigned)(((date_time >> 36ul) & 0x7Ful) + 2023ul),
        (unsigned)((date_time >> 32ul) & 0x0Ful) + 1u, // Add 1 since month is 0-based
        (unsigned)((date_time >> 27ul) & 0x1Ful),
        (unsigned)((date_time >> 22ul) & 0x1Ful),
        (unsigned)((date_time >> 16ul) & 0x3Ful),
        (unsigned)((date_time >> 10ul) & 0x3Ful),
        (unsigned)(date_time & 0x3FFul)
        );
}


/**
 * @brief Prints the type of message and the date it was logged on the host computer.
 *
 * @param msg_index  Index of the message text from Messages.txt.
 */

static void print_message_type_and_date(uint32_t msg_index)
{
    FILE *out = g_msg.file.main_log;
    fprintf(out, get_message_text(msg_index));
    prepare_date_and_time_string();
    fprintf(out, " %s", g_msg.date_string);
}


/**
 * @brief Processes system messages reserved for streaming mode data logging or
 *        repeated single-shot/post-mortem logging.
 *        These messages are added by utilities running on the host computer.
 *        They are transparent to the programmer using the tools.
 *        The messages are of type MSG1 and contain a data word. Additional information
 *        is included in the timestamp (lowest 15 bits) and in the data word.
 *        See the description of date/time info in the header of prepare_date_and_time_string().
 */

static void process_streaming_mode_messages(void)
{
    g_msg.message_cnt--;    // Do not count internal messages

    // Check if the current message has the correct type and length
    // and reset the statistics if a new snapshot or single-shot logging code is detected
    uint32_t special_message = g_msg.timestamp.l >> (g_msg.hdr_data.fmt_id_bits + 1u + 11ul);

    switch (special_message)
    {
        case SYS_HOST_DATE_TIME_INFO:
            print_message_type_and_date(MSG_DATA_SAMPLED_AT_DATE_TIME);
            break;

        case SYS_DATA_OVERRUN_DETECTED:
            print_message_type_and_date(MSG_DATA_OVERRUN_DETECTED);
            reset_statistics();
            break;

        case SYS_MULTIPLE_LOGGING:
            print_message_type_and_date(MSG_MULTIPLE_DATA_LOGGING);
            g_msg.multiple_logging++;
            reset_statistics();
            break;

        default:
            report_problem(ERR_UNKNOWN_SYS_CODE, special_message);
            break;
    }
}


/**
 * @brief Process system messages: long_timestamp and timestamp frequency.
 */

static void process_system_messages(void)
{
    // Ensure the message type is MSG1, which should contain one data word.
    if (g_msg.asm_size != 4)
    {
        report_problem(ERR_BAD_SYSTEM_MESSAGE, g_msg.asm_size);
        return;
    }

    switch (g_msg.fmt_id & 0xFFFFFFFEu)
    {
        case MSG1_SYS_LONG_TIMESTAMP:
        {
            uint32_t new_timestamp_h = g_msg.assembled_msg[0];

            if ((new_timestamp_h == 0) && (g_msg.timestamp.h != 0))
            {
                // Reset statistics if the embedded system was restarted.
                reset_statistics();
            }

            if (new_timestamp_h == 0xFFFFFFFFuL)
            {
                // Reset statistics if the long timestamp value is 0xFFFFFFFF, which is logged by 
                // the rte_restart_timing() function. This is applicable even if the long timestamp
                // is not enabled for the project. The timestamp is set to zero for this message.
                reset_statistics();
                g_msg.timestamp.h = 0;
                g_msg.timestamp.l = 0;
                g_msg.timestamp.f = 0;
            }
            else
            {
                g_msg.timestamp.h = new_timestamp_h;
            }
            break;
        }

        case MSG1_SYS_TSTAMP_FREQUENCY:
            // Update the timestamp frequency with the value in g_msg.assembled_msg[0].
            if (g_msg.assembled_msg[0] == 0)
            {
                report_problem(ERR_TIMESTAMP_FREQUENCY_ZERO, 0);
            }
            else
            {
                // Do not change frequency if the new value should be zero
                g_msg.timestamp.current_frequency = g_msg.assembled_msg[0];
                g_msg.timestamp.multiplier = (double)(1ULL << g_msg.hdr_data.timestamp_shift)
                / (double)g_msg.timestamp.current_frequency
                    / (double)(1uLL << (1u + g_msg.hdr_data.fmt_id_bits));
            }
            break;

        default:
            save_internal_decoding_error(INT_DECODING_SYS_MESSAGE, 0);
            break;
    }
}


/**
 * @brief Prepare a message logged with the rte_msgx() function.
 *        The last byte of the message indicates the number of bytes the message contains.
 *
 * @return  true  - The size of the message (in bytes) matches the expected size based on the message length (in words).
 *          false - The message size is incorrect.
 */

static bool prepare_message_msgx(void)
{
    // If the data length is zero, a message with no data was logged.
    if (g_msg.asm_size == 0)
    {
        report_problem(ERR_MSGX_SIZE_EMPTY, 0U);
        return false;       // MSGX type message must contain at least one DATA word.
    }

    unsigned size = (g_msg.assembled_msg[g_msg.asm_words - 1U] >> 24u) & 0xFFU;

    if ((uint32_t)size > (g_msg.asm_size - 1U))
    {
        report_problem2(ERR_MSGX_SIZE_TOO_LARGE, size, g_msg.asm_size - 1U);
        hex_dump_current_message(false);
        return false;        // The message is corrupted.
    }

    if ((uint32_t)size < (g_msg.asm_size - 4U))
    {
        report_problem2(ERR_MSGX_SIZE_TOO_SMALL, size, g_msg.asm_size - 4U);
        hex_dump_current_message(false);
        return false;       // The message is corrupted.
    }

    // Verify that the unused portion of the last DATA word is zero.
    unsigned last_word = g_msg.assembled_msg[g_msg.asm_words - 1U] & 0x00FFFFFFU;

    if ((last_word >> ((size & 3U) * 8U)) != 0)
    {
        report_problem(ERR_MSGX_CORRUPTED, 0U);
        hex_dump_current_message(false);
        return false;       // The message is corrupted.
    }

    // Update the message size to the logged size value and clear the buffer past the last message byte.
    g_msg.asm_size = size;
    uint8_t *data_end = (uint8_t *)g_msg.assembled_msg;
    data_end[size + 0] = 0;
    data_end[size + 1] = 0;
    data_end[size + 2] = 0;
    data_end[size + 3] = 0;

    return true;            // The message is valid.
}


/**
 * @brief Report that there is no formatting definition for the currently decoded message
 *
 * @param fmt_id     The format ID of the current message.
 * @param last_index The starting index of the current message.
 */

void report_no_definition_for_current_message(unsigned fmt_id, uint32_t last_index)
{
    debug_print_message_info(last_index);
    report_problem(ERR_NO_FORMATTING_DEFINITION_FOR_CODE, fmt_id);
    hex_dump_current_message(true);
}


/**
 * @brief Performs additional processing on the message and verifies its correctness.
 *
 * @param type   The type of the currently processed message.
 * @param mask   The AND mask that specifies the number of low format ID bits used for the extended data.
 *
 * @return  true if the message type is correct, false otherwise.
 */

static bool prepare_msg_and_check_it(enum msg_type_t type, uint32_t mask)
{
    // Set the message size in bytes based on the number of DATA words.
    // The size of extended data is added before decoding if the message type is TYPE_EXT_MSG.
    g_msg.asm_size = 4uL * g_msg.asm_words;

    // Clear one word past the message size to ensure the message can be printed as a zero-terminated string.
    g_msg.assembled_msg[g_msg.asm_words] = 0;
    bool message_ok = true;

    switch (type)
    {
        case TYPE_EXT_MSG:
            // Add the extended data information to the assembled message data.
            g_msg.assembled_msg[g_msg.asm_words] = g_msg.additional_data & mask;
            g_msg.assembled_msg[g_msg.asm_words + 1] = 0;
            g_msg.asm_size += 4;        // Add an additional word with extended data.
            g_msg.fmt_id &= ~mask;      // Clear format ID bits used for the extended data.
            break;

        case TYPE_MSGX:
            // Prepare the data logged with the rte_msgx() function.
            message_ok = prepare_message_msgx();
            break;

        default:
            break;
    }

    return message_ok;
}


/**
 * @brief Processes a message from the binary data file.
 *
 * @param last_index The index in g_msg.rte_buffer pointing to the last message processed.
 */

void process_message(uint32_t last_index)
{
    // Verify if a formatting definition exists for the current format ID.
    uint32_t current_fmt_id = g_msg.fmt_id;

    g_msg.message_cnt++;

    if (current_fmt_id >= MAX_FMT_IDS)
    {
        report_no_definition_for_current_message(current_fmt_id, last_index);
        return;
    }

    if (g_msg.param.debug &&
        (current_fmt_id < g_msg.fmt_ids_defined) && (g_fmt[current_fmt_id] != NULL))
    {
        debug_print_message_hex(last_index); // In debug mode, print the hex contents first.
    }

    msg_data_t *p_fmt = g_fmt[current_fmt_id];

    if (p_fmt == NULL)
    {
        report_no_definition_for_current_message(current_fmt_id, last_index);
        return;
    }

    bool message_ok = prepare_msg_and_check_it(p_fmt->msg_type, p_fmt->ext_data_mask);

    // Verify if the message size matches the expected size as specified in the format file.
    if ((p_fmt->msg_len != 0) && (g_msg.asm_size != p_fmt->msg_len))
    {
        report_problem2(ERR_MSG_SIZE_DOES_NOT_MATCH_DEFINITION, g_msg.asm_size, p_fmt->msg_len);

        if (p_fmt->msg_type == TYPE_EXT_MSG)
        {
            g_msg.asm_words++;      // Also print the extended data.
        }

        hex_dump_current_message(true);
        return;
    }

    // Process special system messages.
    if (g_msg.fmt_id < 4u)        // MSG1_SYS_LONG_TIMESTAMP or MSG1_SYS_TSTAMP_FREQUENCY
    {
        process_system_messages();
    }

    if (g_msg.fmt_id == MSG1_SYS_STREAMING_MODE_LOGGING)
    {
        process_streaming_mode_messages();
    }
    else
    {
        prepare_timestamp_value();

        if (message_ok)
        {
            print_message();
        }
    }
}


/**
 * @brief Converts a hexadecimal character to its numerical value.
 *
 * @param data  Hexadecimal character.
 *
 * @return      Numerical value of the converted digit, or 0 if not a valid hex digit.
 */

static int convert_x_digit(int data)
{
    int result = 0;
    data = toupper(data);

    if (isxdigit(data))
    {
        if (data <= '9')
        {
            result = data - '0';
        }
        else
        {
            result = data - 'A' + 10;
        }
    }

    return result;
}


/**
 * @brief Converts an octal number '\nnn' in a string to an integer.
 *
 * @param message         Pointer to the string containing the octal number.
 * @param chars_processed Pointer to the counter of total characters processed.
 * @param octal_value     Pointer to store the processed octal value.
 *
 * @return Pointer to the character following the octal number in the string.
 */

static char *convert_octal_number(char *message, size_t *chars_processed, unsigned int *octal_value)
{
    size_t result = 0;
    unsigned int data = *message;

    do
    {
        message++;
        result = (result * 8u) + data - '0';
        (*chars_processed)++;
        data = *message;

        if (data == 0)
        {
            break;
        }
    } while ((data >= '0') && (data <= '7'));

    *octal_value = (unsigned int)result;
    message--;
    return message;
}


/**
 * @brief Converts an escaped character to its actual character.
 *
 * @param data  Character to be converted.
 *
 * @return Converted character, or the original character if not an escape sequence.
 */

static unsigned convert_esc_char(unsigned data)
{
    switch (data)
    {
        case '\\':
            data = '\\';
            break;

        case 'n':
            data = '\n';
            break;

        case 'a':
            data = '\a';
            break;

        case 'v':
            data = '\v';
            break;

        case 'r':
            data = '\r';
            break;

        case 't':
            data = '\t';
            break;

        case 'f':
            data = '\f';
            break;

        case '?':
            data = '\?';
            break;

        case '\'':
            data = '\'';
            break;

        case '\"':
            data = '\"';
            break;

        default:
            break;
    }

    return data;
}


/**
 * @brief Converts a hexadecimal number '\xnn' in a string to an integer.
 *
 * @param message         Pointer to the string containing the hexadecimal number.
 * @param chars_processed Pointer to the counter of total characters processed.
 * @param hex_value       Pointer to store the processed hex value.
 *
 * @return Pointer to the character following the hex number in the string.
 */

static char *convert_hex_number(char *message, size_t *chars_processed, unsigned int *hex_value)
{
    if (isxdigit(*message))
    {
        unsigned int data = *message;
        unsigned int result = 0;

        do
        {
            message++;
            result = (result * 16u) + convert_x_digit(data);
            (*chars_processed)++;
            data = *message;

            if (data == 0)
            {
                break;
            }
        } while (isxdigit(data));

        *hex_value = result;
    }

    message--;
    return message;
}


/**
 * @brief Converts octal and hex values of an escape sequence within a string.
 *
 * @param p_message Pointer to the string pointer (string with escape sequences).
 * @param idx       Pointer to the index in the string.
 * @param data      Pointer to the input and output data value.
 */
 
static void process_hex_and_octal_values(char **p_message, size_t *idx, unsigned *data)
{
    char *message = *p_message;

    if (*data == 'x')
    {
        // Process hexadecimal number
        if (isxdigit(message[1]))
        {
            message++;       // Skip the 'x' character
            message = convert_hex_number(message, idx, data);
        }
    }
    else
    {
        if ((*data >= '0') && (*data < '8'))
        {
            message = convert_octal_number(message, idx, data);
        }
        else
        {
            *data = convert_esc_char(*data);
        }
    }

    *p_message = message;
}


/**
 * @brief Processes escape sequences in a string and replaces them with their binary equivalents.
 *        See: en.wikipedia.org/wiki/Escape_sequences_in_C
 *
 * @param message     String that may contain escape sequences to be converted.
 * @param max_length  Maximum length of the message.
 */
 
void process_escape_sequences(char *message, size_t max_length)
{
    char *output = message;     // Copy the processed values to the input string since
                                //  the output string is always shorter or has the same length
    size_t i;

    for (i = 0; i < max_length; i++)
    {
        unsigned int data = *message;

        if (data == 0)
        {
            break;
        }

        if (data == '\\')
        {
            message++;
            data = *message;
            i++;

            if (data == 0)
            {
                break;      // Ignore the '\' at the end of the line
            }

            process_hex_and_octal_values(&message, &i, &data);
        }

        *output = (char)data;
        output++;
        message++;
    }

    *output = 0;
}

/*==== End of file ====*/
