/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    timestamp.h
 * @author  B. Premzel
 * @brief   Supporting functions for the message timestamp decoding
 ******************************************************************************/

#ifndef  _TSTAMP_H
#define _TSTAMP_H

#include "format.h"


/**
 * @brief  Check if the message is OK and if it contains a valid format ID.
 *         Only the individual blocks of data are checked for messages 
 *         longer than five words (DATA words + FMT word).
 *
 * @param p_fmt_id    Pointer to the format ID (input and output value)
 * @param data_words  Number of data words (including the FMT word) in the last sub-packet
 *
 * @return  true - Format ID and message are valid, false - not valid
 */

static inline bool fmt_id_valid(uint32_t *p_fmt_id, uint32_t data_words)
{
    if (*p_fmt_id == 0xFFFFFFFF)
    {
        return false;       // Invalid data - message incompletely written to the buffer
    }

    uint32_t fmt_id = *p_fmt_id >> g_msg.hdr_data.fmt_id_shift;
    *p_fmt_id = fmt_id;

    if (fmt_id >= MAX_FMT_IDS)
    {
        return false;
    }

    msg_data_t *p_fmt = g_fmt[fmt_id];

    if (p_fmt == NULL)
    {
        return false;
    }

    uint32_t length = p_fmt->msg_len / 4u;      // Message length (number of DATA words)

    switch (p_fmt->msg_type)
    {
        case TYPE_MSGX:
            return true;                        // Message length not defined
            break;

        case TYPE_MSGN:
            if ((length == 0) ||                // Message length not defined (unknown at compile time)?
                (data_words == 4u) ||           // Full length sub-packet?
                ((data_words & 3u) == (length & 3)) // Remainder of a sub-packet?
               )
            {
                return true;                    // The message is OK
            }
            break;

        case TYPE_EXT_MSG:
            if ((length - 1) == data_words)     // This message type contains up to 8 additional bits (one additional word)
            {
                return true;
            }
            break;

        case TYPE_MSG0_8:                       // Messages with a known length
            if (length == data_words)
            {
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}


/**
 * @brief  Check the difference from the previous message and increase the high part
 *         of the timestamp counter if the low counter part overflowed.
 *
 * @param tstamp_h_counter  Pointer to the higher part of timestamp counter (input and output)
 * @param old_tstamp_l      Pointer to the previous timestamp_l value (input and output)
 * @param new_tstamp_l      New timestamp_l value
 *
 * @return  true  - timestamp difference is less than max. defined for the consecutive messages
 *          false - timestamp difference too large
 */

static inline bool
small_tstamp_difference(uint32_t *tstamp_h_counter, uint32_t *old_tstamp_l, uint32_t new_tstamp_l)
{
    int64_t time_diff = (int64_t)new_tstamp_l - (int64_t)(*old_tstamp_l);

    if ((time_diff >= 0) && (time_diff <= g_msg.param.max_positive_tstamp_diff))
    {
        /* New timestamp is a bit larger than the old one and no overflow.
         * No need to update the tstamp_h_counter. */
        *old_tstamp_l = new_tstamp_l;
        return true;
    }

    if ((time_diff < 0) && (time_diff >= g_msg.param.max_negative_tstamp_diff))
    {
        /* New timestamp smaller than the old one and no timestamp.l underflow
         * No need to update the tstamp_h_counter and old_tstamp_l. */
        return true;
    }

    if ((g_msg.timestamp.old >= (NORMALIZED_TSTAMP_PERIOD / 2ull))
        && (time_diff <= -(NORMALIZED_TSTAMP_PERIOD - g_msg.param.max_positive_tstamp_diff)))
    {
        // The tstamp_l value overflowed => increment the tstamp_h
        (*tstamp_h_counter)++;
        *old_tstamp_l = new_tstamp_l;
        return true;
    }

    if ((g_msg.timestamp.old < (NORMALIZED_TSTAMP_PERIOD / 2ull))
        && (time_diff >= (NORMALIZED_TSTAMP_PERIOD + g_msg.param.max_negative_tstamp_diff)))
    {
        /* The timestamp appears to be from a previous timestamp period
         * and the timestamp search values do not have to be updated. */
        return true;
    }

    return false;   // Too large difference (the calculated tstamp_h would not be accurate)
}


/**
 * @brief  Find a message with a long timestamp and update the timestamp.h if found.
 * The search for the long timestamp is stopped if:
 *      - a long timestamp is found (or rte_restart_timing = 0xFFFFFFFF)
 *      - end of buffer is reached
 *      - message with a too large difference from the previous timestamp is found
 *
 * @return  true - long timestamp found
 *          false - long timestamp not found
 */

static inline bool long_timestamp_found(void)
{
    if (!g_msg.hdr_data.long_timestamp_used)
    {
        // Long timestamps not enabled for the embedded system firmware
        return false;
    }

    uint32_t data = 0xFFFFFFFF;
    uint32_t old_timestamp_l = g_msg.timestamp.l;
    uint32_t tstamp_h_counter = 0;
    uint32_t data_words = 0;

    if (g_msg.index >= g_msg.in_size)       // Bad index value?
    {
        return false;
    }

    for (uint32_t index = g_msg.index; index < g_msg.in_size; )
    {
        uint32_t previous_data = data;
        data = g_msg.rte_buffer[index];
        index++;
        g_msg.timestamp.searched_to_index = index;

        if ((data & 1) == 0)    // DATA word?
        {
            if (++data_words > 4ul)
            {
                /* Invalid message - max. 4 data words possible.
                 * Stop the search for a long_timestamp if faulty data is found. */
                return false;
            }

            continue;
        }

        uint32_t fmt_id = data;   // fmt_id check and processing is done in the fmt_id_valid() function

        if (!fmt_id_valid(&fmt_id, data_words))
        {
            data_words = 0;
            continue;   // Skip invalid data (it will be skipped later during message decoding also)
        }

        // Strip the timestamp from the FMT word and normalize it
        uint32_t new_timestamp_l = (data & 0xFFFFFFFEul) << g_msg.hdr_data.fmt_id_bits;

        // Stop the search if the streaming logging mark is found (marks the end of one block of data)
        if ((fmt_id == MSG1_SYS_STREAMING_MODE_LOGGING) && (data_words == 1))
        {
            // We do not check if the streaming mode is correct here (will be done during the message decoding)
            return false;
        }

        // The long timestamp is used for time synchronization
        if ((fmt_id == MSG1_SYS_LONG_TIMESTAMP) && (data_words == 1))
        {
            uint32_t timestamp_h = (previous_data >> 1ul) |
                ((data << (g_msg.hdr_data.fmt_id_bits - 1ul)) & 0x80000000ul);

            if (timestamp_h == 0xFFFFFFFFul)    // The value is logged by the rte_restart_timing()
            {
                return false;
            }

            if (timestamp_h >= tstamp_h_counter)    // The calculated value should not be negative
            {
                if (small_tstamp_difference(&tstamp_h_counter, &old_timestamp_l, new_timestamp_l) == false)
                {
                    /* Do not use this long timestamp since the timestamp difference from the previous
                     * timestamp is large (transmissions or logging have been interrupted). */
                    return false;
                }

                g_msg.timestamp.h = timestamp_h - tstamp_h_counter;
                return true;    // Long timestamp found and the value is OK
            }

            return false;
        }

        /* Check if the timestamp difference between the preceding and current message is large.
         * For large differences, assume that transmissions or logging have been interrupted and
         * interrupt the search for a long timestamp. The search will continue after the current
         * message is decoded. */
        if (small_tstamp_difference(&tstamp_h_counter, &old_timestamp_l, new_timestamp_l) == false)
        {
            return false;
        }

        data_words = 0;
    }

    return false;
}


/**
 * @brief Prepare the timestamp value for the current message or search for the 
 *        next long timestamp if the value cannot be determined.
 *
 * This function checks if the difference between the current message timestamp and the previous
 * one is within the allowed range (either fixed maximum values or defined by the parameter).
 * If the new value is within this range, the 64-bit timestamp value is updated. Otherwise, it
 * initiates a search for the long timestamp message.
 *
 * @param p_new_timestamp  Modified only if the complete 64-bit value needs to be updated
 */

static inline void process_timestamp_value(uint64_t *p_new_timestamp)
{
    int64_t time_diff = (int64_t)g_msg.timestamp.l - (int64_t)g_msg.timestamp.old;
    bool search_next_long_tstamp = false;
    bool update_old_tstamp_value = true;

     if ((time_diff >= 0) && (time_diff <= g_msg.param.max_positive_tstamp_diff))
    {
        /* New timestamp is slightly larger than the old one, no overflow occurred
         * No need to update the high part of the timestamp */
    }
    else if ((time_diff < 0) && (time_diff >= g_msg.param.max_negative_tstamp_diff))
    {
        /* New timestamp smaller than the old one and no timestamp.l underflow
         * No need to update timestamp.h and timestamp.old */
        update_old_tstamp_value = false;
    }
    else if ((g_msg.timestamp.old >= (NORMALIZED_TSTAMP_PERIOD / 2ull))
             && (time_diff <= -(NORMALIZED_TSTAMP_PERIOD - g_msg.param.max_positive_tstamp_diff))
             && (!g_msg.timestamp.no_previous_tstamp))
    {
        /* The low part of the timestamp overflowed, increment the high part.
         * Ensure at least four messages have been processed since the last increment.
         * This is an ad-hoc solution for cases where data loss has occurred due to
         * insufficient bandwidth to the host, system reset, etc. */
        if ((g_msg.message_cnt - g_msg.timestamp.msg_long_tstamp_incremented) >= 4ul)
        {
            g_msg.timestamp.msg_long_tstamp_incremented = g_msg.message_cnt;
            g_msg.timestamp.h++;
        }

        *p_new_timestamp = (((uint64_t)g_msg.timestamp.h) << 32u) | (uint64_t)g_msg.timestamp.l;
    }
    else if ((g_msg.timestamp.old < (NORMALIZED_TSTAMP_PERIOD / 2ull))
             && (time_diff >= (NORMALIZED_TSTAMP_PERIOD + g_msg.param.max_negative_tstamp_diff))
             && (!g_msg.timestamp.no_previous_tstamp))
    {
        /* The timestamp.l seems to be from the previous timestamp period.
         * The (timestamp.h - 1) will be used for the current message timestamp. */
        uint32_t stamp = 0;

        if (g_msg.timestamp.h > 0)
        {
            stamp = g_msg.timestamp.h - 1;
        }

        *p_new_timestamp = ((uint64_t)stamp << 32u) | (uint64_t)g_msg.timestamp.l;
        update_old_tstamp_value = false;
    }
    else
    {
        /* Large timestamp difference detected, initiate search for the long timestamp message.
         * Assume data transmission or logging was interrupted. */
        search_next_long_tstamp = true;
        g_msg.timestamp.mark_problematic_tstamps = !g_msg.timestamp.no_previous_tstamp;
    }

    if (update_old_tstamp_value || g_msg.timestamp.no_previous_tstamp)
    {
        g_msg.timestamp.old = g_msg.timestamp.l;
    }

    // Search for a long timestamp if it hasn't been found or searched for in subsequent messages
    if ((search_next_long_tstamp && (g_msg.timestamp.searched_to_index < g_msg.index))
        || g_msg.timestamp.no_previous_tstamp)
    {
        // Search for the next long timestamp
        if (long_timestamp_found())
        {
            /* Update the current message timestamp based on the next long timestamp.
             * The long_timestamp_found() function updates the high part of the timestamp. */
            *p_new_timestamp = (((uint64_t)g_msg.timestamp.h) << 32u) | (uint64_t)g_msg.timestamp.l;
            g_msg.timestamp.old = g_msg.timestamp.l;
        }
    }
}


/**
 * @brief Prepare the timestamp value for the current message (make it ready for printing).
 *        Attempts to retrieve the high part of the timestamp if the long timestamp message
 *        has not been found yet or if a large timestamp difference was detected (indicating
 *        potential data loss).
 */

static inline void prepare_timestamp_value(void)
{
    uint64_t new_timestamp = (((uint64_t)g_msg.timestamp.h) << 32u) | (uint64_t)g_msg.timestamp.l;

    if (g_msg.fmt_id == MSG1_SYS_LONG_TIMESTAMP)
    {
        g_msg.timestamp.old = g_msg.timestamp.l;
        g_msg.timestamp.long_timestamp_found = true;
    }
    else if (g_msg.fmt_id != MSG1_SYS_STREAMING_MODE_LOGGING)
    {
        process_timestamp_value(&new_timestamp);
            // TODO: Check if the g_msg.timestamp.old should be set here (always or on first encounter)
    }

    g_msg.timestamp.no_previous_tstamp = false;
    g_msg.timestamp.f = g_msg.timestamp.multiplier * (double)new_timestamp;
}

#endif  // _TSTAMP_H

/*==== End of file ====*/
