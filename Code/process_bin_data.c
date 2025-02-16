/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/******************************************************************************
 * @file    process_bin_data.c
 * @author  B. Premzel
 * @brief   Functions for processing the raw binary data from the input file.
 ******************************************************************************/

#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "errors.h"
#include "decoder.h"
#include "files.h"
#include "format.h"
#include "statistics.h"
#include "print_message.h"
#include "print_helper.h"
#include "read_bin_data.h"


/**
 * @brief Check if the message is smaller than what fits into a single message packet or if
 *        this message contains 4 DATA words and this size fits the format definition.
 *
 * @param no_words  Number of DATA words in the last packet.
 *
 * @return  true - if the message is complete or too large.
 *          false - if additional data might be in the next packet.
 */

__inline bool message_complete(unsigned no_words)
{
    if (no_words < 5u)
    {
        // Packet with a length of less than 5 is already a complete message
        // Only messages that are 5 words long (4 DATA + 1 FMT) can have a continuation.
        return true;
    }

    // Check if a format definition exists for the current format ID.
    if (g_msg.fmt_id < MAX_FMT_IDS)
    {
        msg_data_t *p_fmt = g_fmt[g_msg.fmt_id];

        if (p_fmt != NULL)
        {
            // Do not search for other parts of the message if the message has the correct length already
            unsigned length = p_fmt->msg_len;       // Length [bytes]

            if (length == 0)
            {
                return false;       // Message length zero (MSG0) or unknown
            }

            if ((p_fmt->msg_type == TYPE_EXT_MSG) && (length >= 4uL))
            {
                // Exclude the extended data since the message has not been identified as EXT_MSG yet
                length -= 4uL;
            }

            if (length <= (g_msg.asm_words * 4uL)) // Is the message the correct length or too long?
            {
                return true; // Message size matches the format definition or is too long.
            }
        }
    }

    return false;
}


/**
 * @brief Retrieves the expected length of the sub-packet for a given format ID.
 *        Returns the length of the first packet (1 to 5 words including FMT).
 *
 * @param fmt_id  Format ID of the current message.
 *
 * @return Length of the message (number of DATA words including one FMT word).
 *         Returns 0xFFFFFFFF if the format ID is invalid or no definition exists.
 */

static uint32_t get_packet_length(uint32_t fmt_id)
{
    if (fmt_id >= MAX_FMT_IDS)
    {
        return 0xFFFFFFFFu;     // Return actual packet length if format ID is invalid.
    }

    msg_data_t *p_fmt = g_fmt[fmt_id];

    while ((fmt_id & 0xF) != 0)
    {
        p_fmt = g_fmt[fmt_id];  // The format definition exists?

        if (p_fmt != NULL)
        {
            break;
        }

        fmt_id--;
    }

    if (p_fmt == NULL)
    {
        return 0xFFFFFFFFu;     // Use the actual packet length since the length is unknown
    }

    uint32_t len = p_fmt->msg_len / 4u; // Calculate number of DATA words.

    switch (g_fmt[fmt_id]->msg_type)
    {
        case TYPE_MSG0_4:
            break;

        case TYPE_EXT_MSG:
            if (len > 0)
            {
                len--;  // One data element (up to 8 bits) is part of the FMT word.
            }
            break;

        case TYPE_MSGN:
            if ((len == 0) || (len > 4))
            {
                // Assume a length of 4 DATA words or more if unknown at compile time.
                len = 4u;
            }
            break;
            // The code above assumes that there is an invalid message that should be skipped
            // before the start of the MSGN message and not in the middle of the message.

        case TYPE_MSGX:
            len = 4u;           // Sub packet can could hold up to 4 DATA words
            break;
    }

    len++;                      // Add one for the FMT word
    return len;
}


/**
 * @brief Processes a message packet assembled in the g_msg.assembled_msg buffer.
 *
 * @param no_words  Number of 32-bit words in the last packet (includes DATA words and one FMT word).
 * @param data      The last FMT word found.
 *
 * @return Result as defined by the enum 'asm_msg_t'.
 */

__inline asm_msg_t process_the_message_packet(unsigned no_words, uint32_t data)
{
    // Process the data contained in the FMT word
    g_msg.fmt_id = data >> g_msg.hdr_data.fmt_id_shift;
    g_msg.timestamp.l = (data & 0xFFFFFFFEu) << g_msg.hdr_data.fmt_id_bits;
    uint32_t additional_data = g_msg.fmt_id;
    uint32_t msg_len = get_packet_length(g_msg.fmt_id);
        // Function returns 0xFFFFFFFF if the message length is unknown

    if ((msg_len == 0xFFFFFFFFu) && (no_words > 5u))
    {
        // A FMT word was found but it is invalid and the message contains > 4 DATA words
        // Treat the FMT word as a MSG0 message type if packet contains more than 4 DATA words.
        g_msg.bad_packet_words = no_words - 1u; // Mark all DATA words as BAD_BLOCK
        g_msg.index -= 1u;                      // and do processing for the FMT once again
        return BAD_BLOCK;
    }

    if (no_words > msg_len)                     // Message length over max. defined by the programmer?
    {
        // Check if just a few words have to be skipped and the remainder is a valid message
        // with a defined length (number of data words). 
        g_msg.bad_packet_words = no_words - msg_len;    // Skip the probably invalid data
        g_msg.index -= msg_len;                         // and process the remainder again
        return BAD_BLOCK;
    }

    if ((g_fmt[g_msg.fmt_id] == NULL) || (g_fmt[g_msg.fmt_id]->msg_type != TYPE_EXT_MSG))
    {
        additional_data &= 0x0Fu;
    }

    unsigned n = no_words - 1u;
    uint32_t and_mask = 0xFFFFFFFFu;

    while (n > 0)
    {
        --n;
        g_msg.raw_data[n] = (g_msg.raw_data[n] >> 1) | ((additional_data & 1) << 31);
        additional_data >>= 1u;
        and_mask <<= 1u;
    }

    g_msg.additional_data = additional_data;
    g_msg.fmt_id &= and_mask; // Remove bits used for the 31st bit of DATA words

    // Transfer data from the current packet to the assembled message
    for (n = 0; n < (no_words - 1); n++)
    {
        g_msg.assembled_msg[g_msg.asm_words] = g_msg.raw_data[n];
        g_msg.asm_words++;
    }

    if (msg_len == 0xFFFFFFFFu)
    {
        return DATA_FOUND;          // Returns DATA FOUND even if the format ID is invalid
    }

    if (message_complete(no_words))
    {
        return DATA_FOUND;
    }

    return FMT_WORD_OK;
}


/**
 * @brief Skips all unfinished words containing 0xFFFFFFFF.
 *
 * @return Number of unfinished words skipped.
 */

__inline unsigned skip_unfinished_words(void)
{
    unsigned no_unfinished = 0;
    
    for (uint32_t idx = g_msg.index; idx < g_msg.in_size; idx++)
    {
        if (g_msg.rte_buffer[idx] != 0xFFFFFFFFuL)
        {
            break;
        }

        no_unfinished++;
        g_msg.index++;
    }

    return no_unfinished;
}


/**
 * @brief Searches for a message packet consisting of up to four DATA words and one FMT word.
 *        Assembles the message in g_msg.assembled_msg, with the message size stored in g_msg.asm_words.
 *        The DATA words are simply written to the buffer without processing.
 *
 * @param packet_words  Output: pointer to the number of words found in the message packet.
 * @param p_data        Output: pointer to the FMT data word (if the FMT is found).
 *
 * @return Result as defined by the enum 'asm_msg_t'.
 */

__inline static asm_msg_t find_fmt_word(unsigned *packet_words, uint32_t *p_data)
{
    uint32_t data;

    do          // Search for a message packet (FMT words have bit 0 set to 1)
    {
        if ((*packet_words >= MAX_RAW_DATA_SIZE) ||
            (g_msg.index >= g_msg.in_size))     // End of buffer?
        {
            g_msg.bad_packet_words = *packet_words;
            return BAD_BLOCK;       // Too many DATA words without a FMT word
        }

        data = g_msg.rte_buffer[g_msg.index];

        if (data == 0xFFFFFFFFuL)
        {
            if (g_msg.asm_words > 0)
            {
                // It is not clear here if the message that is already in the g_msg.asm_words
                // was logged correctly - return the status that the message was found.
                // If the message is too short then this will be detected during decoding.
                // Information about the unfinished word(s) will be reported during the
                // next call to this function.
                g_msg.index -= *packet_words;   // Rewind back to the end of valid data
                *packet_words = 0;              // and reject the last sub-packet
                return DATA_FOUND;
            }
            else if (*packet_words > 0)
            {
                g_msg.bad_packet_words = *packet_words;
                return BAD_BLOCK;
            }
            else
            {
                // Skip all unfinished words and report the error.
                g_msg.unfinished_words = skip_unfinished_words();
                return UNFINISHED_BLOCK;
            }
        }

        g_msg.index++;
        g_msg.raw_data[*packet_words] = data;
        ++(*packet_words);
    }
    while ((data & 1) == 0);        // Repeat until the FMT word is found

    *p_data = data;                 // FMT word found
    return FMT_WORD_OK;
}


/**
 * @brief  Checks the last (unfinished) message in the buffer.
 * 
 * @param packet_words  Number of words found in the last packet.
 * 
 * @return Result as defined by the enum 'asm_msg_t'.
 */

static asm_msg_t check_last_message_in_the_buffer(unsigned packet_words)
{
    g_msg.binary_file_decoding_finished = true;

    if ((g_msg.asm_words > 0) && (packet_words == 0))
    {
        // A message has been found, although it may not be complete.
        return DATA_FOUND;
    }
    else if (packet_words > 0)
    {
        // Incomplete message without the FMT word.
        g_msg.bad_packet_words = packet_words;
        return BAD_BLOCK;
    }

    return END_OF_BUFFER;
}


/**
 * @brief  Determines if the next packet is a continuation of the message currently being composed.
 *         Subpackets belonging to the same message share the same timestamp and format ID.
 * 
 * @param timestamp_and_fmt_index Timestamp and format ID of the message currently processed.
 *
 * @return true if the following data packet belongs to the same message.
 */

static bool next_packet_is_the_continuation_of_the_message(uint32_t timestamp_and_fmt_index)
{
    uint32_t no_words = 0;

    for (uint32_t idx = g_msg.index; idx < g_msg.in_size; idx++)
    {
        if (++no_words > 5)         // Format word must be within next 5 words
        {
            return false;
        }

        uint32_t data = g_msg.rte_buffer[idx];

        if (data == 0xFFFFFFFFuL)   // Unfinished message?
        {
            return false;
        }

        if ((data & 1) != 0)        // FMT word?
        {
            if ((data & g_msg.hdr_data.timestamp_and_index_mask) == timestamp_and_fmt_index)
            {
                return true;        // Message continuation found
            }

            return false;
        }
    }

    return false;
}


/**
 * @brief  Assembles a message from one or more message blocks sharing the same 
 *         timestamp and format ID. 
 *         Data words (DATA) and format/timestamp words (FMT) written to the buffer
 *         using RTEdbg library functions are reassembled here into messages 
 *         identical to those recorded in the embedded system.
 *         The assembled message is stored in g_msg.assembled_msg, with its size 
 *         in g_msg.asm_words.
 *
 * @return Result as defined by the enum 'asm_msg_t'
 */

static asm_msg_t assemble_message(void)
{
    g_msg.asm_words = 0;
    unsigned packet_words = 0;

    // Assemble the message into g_msg.assembled_msg
    while (g_msg.index < g_msg.in_size)
    {
        uint32_t data;
        packet_words = 0;       // Number of data words in a message packet (including FMT)
        asm_msg_t rez = find_fmt_word(&packet_words, &data);

        if (rez != FMT_WORD_OK)
        {
            return rez;
        }

        uint32_t timestamp_and_fmt_index = data & g_msg.hdr_data.timestamp_and_index_mask;
        // This is the message tag used to search for additional packets in the message

        rez = process_the_message_packet(packet_words, data);

        if (rez != FMT_WORD_OK)
        {
            return rez;
        }

        if (!next_packet_is_the_continuation_of_the_message(timestamp_and_fmt_index))
        {
            return DATA_FOUND;
        }

        // Check if the message has reached the limit (and continuation follows)
        if (g_msg.asm_words >= (4U * g_msg.hdr_data.max_msg_blocks))
        {
            return MESSAGE_TOO_LONG;
        }
    }

    return check_last_message_in_the_buffer(packet_words);
}


/**
 * @brief Prints the format ID and timestamp information
 *
 * @param out   Pointer to the output file
 */

static void debug_print_format_id_name_info(FILE *out)
{
    fprintf(out, "TstampL:%u/old: %u",
        g_msg.timestamp.l >> (g_msg.hdr_data.fmt_id_bits + 1u),
        g_msg.timestamp.old >> (g_msg.hdr_data.fmt_id_bits + 1u)
    );

    const char *name = NULL;

    if (g_msg.fmt_id < MAX_FMT_IDS)
    {
        msg_data_t *p_fmt = g_fmt[g_msg.fmt_id];

        if (p_fmt != NULL)
        {
            name = p_fmt->message_name;
        }
    }

    if (name != NULL)
    {
        fprintf(out, " FMT:%u(%s) ", g_msg.fmt_id, name);
    }
    else
    {
        fprintf(out, " FMT:%u ", g_msg.fmt_id);
    }
}


/**
 * @brief Prints message as hex data if errors in the message have been detected
 *
 * @param out   Pointer to the output file
 */

static void debug_print_bad_packet_words(FILE *out)
{
    if ((g_msg.bad_packet_words > 0) && (g_msg.asm_words > 0))
    {
        fprintf(out, ":: ");
    }

    if (g_msg.bad_packet_words > 0)
    {
        fprintf(out, get_message_text(MSG_BAD_PACKET_WORDS));
        unsigned no_words = g_msg.bad_packet_words;

        if (no_words > MAX_RAW_DATA_SIZE)
        {
            no_words = MAX_RAW_DATA_SIZE;
        }

        for (uint32_t i = 0; i < no_words; i++)
        {
            fprintf(out, "0x%08X ", g_msg.raw_data[i]);
        }
    }
}


/**
 * @brief Prints the message as 32-bit words and bytes for debugging.
 *
 * @param start_index  Index of the first word of the current message in the buffer.
 */

void debug_print_message_hex(uint32_t start_index)
{
    FILE *out = g_msg.file.main_log;

    fprintf(out, "\n  >>> ");
    print_message_number(out, g_msg.message_cnt);
    fprintf(out, " %s: %llu ", get_message_text(MSG_INDEX),
        start_index + g_msg.already_processed_data);

    if ((g_msg.unfinished_words == 0) && (g_msg.bad_packet_words == 0))
    {
        debug_print_format_id_name_info(out);
    }

    if (g_msg.unfinished_words > 0)
    {
        fprintf(out, get_message_text(MSG_NUMBER_OF_UNFINISHED_WORDS));
        return;     // Nothing more to do since the message is not OK
    }

    // Output the message contents
    if (g_msg.asm_words > 0)
    {
        if (g_msg.bad_packet_words > 0)
        {
            fprintf(out, get_message_text(MSG_PARTIALLY_DATA_OK), g_msg.fmt_id);
        }
        else
        {
            fprintf(out, "hex: ");
        }

        for (uint32_t i = 0; i < g_msg.asm_words; i++)
        {
            fprintf(out, "0x%08X ", g_msg.assembled_msg[i]);
        }

        if (g_msg.bad_packet_words == 0)
        {
            fprintf(out, "---");
            uint8_t *message = (uint8_t *)&g_msg.assembled_msg[0];

            for (uint32_t i = 0; i < g_msg.asm_words * 4; i++)
            {
                if ((i % 4) == 0)
                {
                    fprintf(out, " ");
                }

                fprintf(out, "0x%02X ", *message++);
            }
        }
    }

    debug_print_bad_packet_words(out);
}


/**
 * @brief Prints the message number and index of the current message.
 *
 * @param last_index  Index of the last processed message.
 */

void debug_print_message_info(uint32_t last_index)
{
    if ((g_msg.param.debug != 0) && (g_msg.file.main_log != NULL))
    {
        fprintf(g_msg.file.main_log, "\n  >>>");
        print_message_number(g_msg.file.main_log, g_msg.message_cnt);
        fprintf(g_msg.file.main_log, ", %s: %llu",
            get_message_text(MSG_INDEX), last_index + g_msg.already_processed_data);
    }
}


/**
 * @brief Checks if additional data should be loaded (streaming mode or multiple snapshots).
 *        Ensures at least two messages remain in the buffer, considering the maximum possible message size.
 */

__inline static void load_additional_data(void)
{
    if (!g_msg.complete_file_loaded)
    {
        size_t remaining_words = g_msg.in_size - g_msg.index;

        if (remaining_words <= (2ull * g_msg.hdr_data.max_msg_blocks * 5ull * sizeof(uint32_t)))
        {
            load_data_block();   // Add new data to the data that has not been decoded yet
        }
    }
}


/**
 * @brief Reports information about a bad block.
 *
 * @param last_fmt_id  Highest format ID value.
 */

static void report_bad_block(unsigned last_fmt_id)
{
    g_msg.message_cnt++;
    debug_print_message_info(last_fmt_id);
    uint32_t asm_words = g_msg.asm_words;

    if (asm_words != 0)
    {
        asm_words += ((asm_words + 3u) / 4u);   // Include FMT words
    }

    uint32_t total_words = g_msg.bad_packet_words + asm_words;

    report_problem(ERR_BAD_BLOCK_FOUND, total_words);
    debug_print_message_hex(last_fmt_id);   // Print the data as hex numbers
}


/**
 * @brief Reports information about a message that exceeds the allowed size.
 *
 * @param last_fmt_id  The highest format ID value encountered.
 */

static void report_a_too_long_message(unsigned last_fmt_id)
{
    g_msg.message_cnt++;
    debug_print_message_info(last_fmt_id);
    report_problem(ERR_MESSAGE_TOO_LONG, 0);
    fprintf(g_msg.file.main_log, get_message_text(MSG_FMT_ID), g_msg.fmt_id);
    debug_print_message_hex(last_fmt_id);
}


/**
 * @brief Reports information about an incomplete or partially incomplete message.
 *
 * @param last_fmt_id  The most recent format ID value.
 */

static void report_an_unfinished_block(unsigned last_fmt_id)
{
    g_msg.message_cnt++;
    debug_print_message_info(last_fmt_id);
    report_problem(ERR_UNFINISHED_BLOCK, g_msg.unfinished_words);
}


/**
 * @brief Issues a warning if errors were detected during the processing of the
 *        first message in the snapshot. Adds an extra newline if RTEmsg debug mode is active.
 *
 * @param last_error_counter  The count of errors before processing the current message.
 */

static void report_warning_if_error_found_in_first_message(uint32_t last_error_counter)
{
    if ((g_msg.message_cnt == g_msg.error_warning_in_msg) &&
        (g_msg.total_errors != last_error_counter))
    {
        fprintf(g_msg.file.main_log, 
            get_message_text(MSG_WARN_ERROR_IN_FIRST_SNAPSHOT_MSG));
    }

    if ((g_msg.param.debug) || (g_msg.param.additional_newline))
    {
        fprintf(g_msg.file.main_log, "\n");
    }
}


/**
 * @brief Decodes the data loaded from the binary data file.
 *        The binary data is stored in g_msg.rte_buffer.
 */

void process_bin_data_worker(void)
{
    for ( ;; )
    {
        uint32_t last_index = g_msg.index;
        asm_msg_t code = assemble_message();
        uint32_t last_error_counter = g_msg.total_errors;

        switch (code)
        {
            case END_OF_BUFFER:
                return;

            case DATA_FOUND:
                // Decode and print the message content
                process_message(last_index);
                break;

            case BAD_BLOCK:
                report_bad_block(last_index);
                break;

            case UNFINISHED_BLOCK:
                report_an_unfinished_block(last_index);
                break;

            case MESSAGE_TOO_LONG:
                report_a_too_long_message(last_index);
                break;

            default:
                report_problem(ERR_ASSEMBLE_MSG_INTERNAL_PROBLEM, 0);
                break;
        }

        g_msg.total_bad_packet_words += g_msg.bad_packet_words;
        g_msg.total_unfinished_words += g_msg.unfinished_words;
        g_msg.bad_packet_words = 0;
        g_msg.unfinished_words = 0;

        report_warning_if_error_found_in_first_message(last_error_counter);
        load_additional_data();  // Load more data from the binary file if necessary.
    }
}

/*==== End of file ====*/
