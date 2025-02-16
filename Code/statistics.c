/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    statistics.c
 * @author  B. Premzel
 * @brief   Statistical functions for the RTEmsg utility (timing and values)
 *          See the RTEdbg manual for detailed description of statistical functions.
 ******************************************************************************/

#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "main.h"
#include "errors.h"
#include "files.h"
#include "format.h"
#include "statistics.h"
#include "print_message.h"
#include "print_helper.h"


/**
 * @brief Determine if the current value is smaller than values in the table and
 *        insert it into the table if it is so.
 *        The smallest value is the first one in the table. If the same peak value
 *        appears several times, then several such values will be in the table together
 *        with the numbers of messages in which these values have been detected.
 *
 * @param min_values  Pointer to the table containing MIN_MAX_VALUES minimal values
 *                    First element contains the smallest value.
 * @param msg_numbers Table with message numbers of messages in which the minimal values appeared
 * @param value       New value for the statistics
 * @param message_no  Message number of the current message
 * @param count       Number of values which have been processed already (count = 0 at first run)
 */

static void determine_minimal_value(double *min_values, uint32_t *msg_numbers, double value,
                                    uint32_t message_no, unsigned count)
{
    /* Check if the new value should be inserted into the table
     * Do not exit until the table is full (MIN_MAX_VALUES are in the table) */
    if (count >= MIN_MAX_VALUES)
    {
        // Exit if the value is larger than or equal to the largest value in the table
        if (value >= min_values[MIN_MAX_VALUES - 1])
        {
            return;
        }

        count = MIN_MAX_VALUES;
    }

    // Find the position where to insert the new value
    unsigned position;

    for (position = 0; position < count; position++)
    {
        if (value < min_values[position])
        {
            break;
        }
    }

    // Prepare space for the new value (move the larger values)
    for (unsigned i = MIN_MAX_VALUES - 1; i > position; i--)
    {
        msg_numbers[i] = msg_numbers[i - 1];
        min_values[i] = min_values[i - 1];
    }

    // Insert the new value and its message number into the table
    msg_numbers[position] = message_no;
    min_values[position] = value;
}


/**
 * @brief Determine if the current value is larger than values in the table and
 *        insert it into the table if it is so.
 *        The largest value is the first one in the table. If the same peak value
 *        appears several times, then several such values will be in the table together
 *        with the numbers of messages in which these values have been detected.
 *
 * @param max_values  Pointer to the table containing MIN_MAX_VALUES maximal values
 *                    First element contains the largest value.
 * @param msg_numbers Table with message numbers of messages in which the maximal values appeared
 * @param value       New value for the statistics
 * @param message_no  Message number of the current message
 * @param count       Number of values which have been processed already (count = 0 at first run)
 */

static void determine_maximal_value(double *max_values, uint32_t *msg_numbers, double value,
                                    uint32_t message_no, unsigned count)
{
    /* Check if the new value should be inserted into the table
     * Do not exit until the table is full (MIN_MAX_VALUES are in the table) */
    if (count >= MIN_MAX_VALUES)
    {
        // Exit if the value is smaller than or equal to the smallest value in the table
        if (value <= max_values[MIN_MAX_VALUES - 1])
        {
            return;
        }

        count = MIN_MAX_VALUES;
    }

    // Find the position where to insert the new value
    unsigned position;

    for (position = 0; position < count; position++)
    {
        if (value > max_values[position])
        {
            break;
        }
    }

    // Prepare space for the new value (move the smaller values)
    for (unsigned i = MIN_MAX_VALUES - 1; i > position; i--)
    {
        msg_numbers[i] = msg_numbers[i - 1];
        max_values[i] = max_values[i - 1];
    }

    // Insert the new value and its message number into the table
    msg_numbers[position] = message_no;
    max_values[position] = value;
}


/**
 * @brief Process the value statistics for the currently processed value
 *
 * @param p_fmt    Pointer to the structure with current message formatting parameters
 * @param format   Pointer to the structure with current value parameters
 */

void value_statistic(msg_data_t *p_fmt, value_format_t *format)
{
    if ((format == NULL) || (p_fmt == NULL))
    {
        return;
    }

    value_stats_t *stat = format->value_stat;

    if (stat == NULL)
    {
        return;
    }

    determine_minimal_value(stat->min, stat->min_msg_no, g_msg.value.data_double, g_msg.message_cnt,
                            stat->counter);
    determine_maximal_value(stat->max, stat->max_msg_no, g_msg.value.data_double, g_msg.message_cnt,
                            stat->counter);

    // Prepare data for the average value
    stat->counter++;
    stat->sum += g_msg.value.data_double;
}


/**
 * @brief Print the statistical data for one of the values
 *
 * @param out           Pointer to the output file
 * @param message_name  Name of the message in which the current value is
 * @param p_val         Pointer to the structure with formatting definitions
 */

static void write_data_for_one_value(FILE *out, const char *message_name, value_format_t *p_val)
{
    if ((out == NULL) || (p_val == NULL))
    {
        return;
    }

    unsigned int stat_count = p_val->value_stat->counter;

    if (stat_count > MIN_MAX_VALUES)
    {
        stat_count = MIN_MAX_VALUES;
    }

    const char *val_name = p_val->value_stat->name;

    if (val_name == NULL)
    {
        val_name = get_message_text(MSG_UNDEFINED_NAME);
    }

    fprintf(out, get_message_text(MSG_VALUE_STATISTICS_MAXIMUMS), val_name);

    for (unsigned int i = 0; i < stat_count; i++)
    {
        fprintf(out, ";%g", p_val->value_stat->max[i]);
    }

    if (message_name == NULL)
    {
        message_name = get_message_text(MSG_UNDEFINED_NAME);
    }

    fprintf(out, get_message_text(MSG_VALUE_STATISTICS_MSG_NR_MAX), message_name);

    for (unsigned int i = 0; i < stat_count; i++)
    {
        print_message_number(out, p_val->value_stat->max_msg_no[i]);
        fputc(';', out);
    }

    fprintf(out, get_message_text(MSG_VALUE_STATISTICS_MINIMUMS));

    for (unsigned int i = 0; i < stat_count; i++)
    {
        fprintf(out, ";%g", p_val->value_stat->min[i]);
    }

    fprintf(out, get_message_text(MSG_VALUE_STATISTICS_MSG_NR_MIN));

    for (unsigned int i = 0; i < stat_count; i++)
    {
        print_message_number(out, p_val->value_stat->min_msg_no[i]);
        fputc(';', out);
    }

    fprintf(out, get_message_text(MSG_VALUE_STATISTICS_MSG_AVERAGE),
        p_val->value_stat->sum / (double)p_val->value_stat->counter, p_val->value_stat->counter);
}


/**
 * @brief Structure to track top messages by frequency or buffer usage
 */
typedef struct
{
    unsigned fmt_id;    // Format ID of the message
    unsigned count;     // Number of occurrences or buffer space used (in bytes)
} top_msgs_t;

/**
 * @brief Print a list of message types with the highest frequency or buffer usage
 *
 * @param out         Output file
 * @param top_msgs    Pointer to the array containing the statistics
 * @param msgs_found  Number of messages found
 * @param description Message ID for description text to print
 */

static void print_msg_statistics_worker(FILE *out, top_msgs_t *top_msgs,
    size_t msgs_found, uint32_t description)
{
    if ((out == NULL) || (top_msgs == NULL))
    {
        return;
    }

    if (msgs_found > TOP_MESSAGES)
    {
        msgs_found = TOP_MESSAGES;
    }

    if ((msgs_found > 0) && (out != NULL))
    {
        fprintf(out, get_message_text(description));

        for (unsigned i = 0; i < msgs_found; i++)
        {
            msg_data_t *p_fmt = g_fmt[top_msgs[i].fmt_id];
            const char *name = NULL;

            if (p_fmt != NULL)
            {
                name = p_fmt->message_name;
            }

            if (name == NULL)
            {
                name = get_message_text(MSG_UNDEFINED_NAME);
            }

            fprintf(out, "\n%2u %6u %s", i + 1, top_msgs[i].count, name);
        }
    }
}


/**
 * @brief Display a list of message types with the highest frequencies of occurrence
 */

static void print_messages_with_top_frequencies(void)
{
    unsigned msgs_found = 0; // Number of different messages found (the value is truncated to TOP_MESSAGES)
    top_msgs_t top_msgs[TOP_MESSAGES] = { 0 };
    unsigned fmt_ids = g_msg.fmt_ids_defined;

    // Search for messages with top frequencies
    msg_data_t *p_fmt = NULL;

    for (unsigned i = 0; i < fmt_ids; i++)
    {
        if (p_fmt == g_fmt[i])      // Skip if already processed this format
        {
            continue;
        }

        p_fmt = g_fmt[i];

        if ((p_fmt == NULL) || (p_fmt->counter_total == 0))
        {
            continue;   // No messages of this type have been received
        }

        /*----------------------------------------------------------------------
         * Check if the message count has a higher frequency of occurrences than
         * the current ones in the table and insert it if it does. */
        unsigned value = p_fmt->counter_total;

        if (msgs_found >= TOP_MESSAGES)
        {
            msgs_found = TOP_MESSAGES;

            // Exit if the value is smaller or equal to the smallest value in the table
            if (value <= top_msgs[TOP_MESSAGES - 1].count)
            {
                continue;
            }
        }

        // Find insertion position
        unsigned position;

        for (position = 0; position < msgs_found; position++)
        {
            if (value > top_msgs[position].count)
            {
                break;
            }
        }

        // Shift larger values down
        for (unsigned nn = TOP_MESSAGES - 1; nn > position; nn--)
        {
            top_msgs[nn] = top_msgs[nn - 1];
        }

        // Insert new value
        top_msgs[position].fmt_id = i;
        top_msgs[position].count = value;
        msgs_found++;
    }

    print_msg_statistics_worker(g_msg.file.statistics_log, top_msgs, msgs_found,
        MSG_MESSAGES_WITH_TOP_FREQUENCY);
}


/**
 * @brief Display a list of message types with the highest circular buffer usage
 */

static void print_messages_with_top_buffer_usage(void)
{
    unsigned msgs_found = 0; // Number of different messages found (truncated to TOP_MESSAGES)
    top_msgs_t top_msgs[TOP_MESSAGES] = { 0 };

    // Search for messages with top buffer usage
    msg_data_t *p_fmt = NULL;

    for (unsigned i = 0; i < g_msg.fmt_ids_defined; i++)
    {
        if (p_fmt == g_fmt[i])      // Skip if already processed this format
        {
            continue;
        }

        p_fmt = g_fmt[i];

        if ((p_fmt == NULL) || (p_fmt->total_data_received == 0))
        {
            continue;
        }

        /*---------------------------------------------------------------
         * Check if the message count has a higher buffer usage than the
         * current ones in the table and insert it if it does. */
        unsigned value = p_fmt->total_data_received * 4;    // Convert words to bytes

        if (msgs_found >= TOP_MESSAGES)
        {
            msgs_found = TOP_MESSAGES;

            // Exit if the value is smaller or equal to the smallest value in the table
            if (value <= top_msgs[TOP_MESSAGES - 1].count)
            {
                continue;
            }
        }

        // Find insertion position
        unsigned position;

        for (position = 0; position < msgs_found; position++)
        {
            if (value > top_msgs[position].count)
            {
                break;
            }
        }

        // Shift larger values down
        for (unsigned nn = TOP_MESSAGES - 1; nn > position; nn--)
        {
            top_msgs[nn] = top_msgs[nn - 1];
        }

        // Insert new value
        top_msgs[position].fmt_id = i;
        top_msgs[position].count = value;
        msgs_found++;
    }

    print_msg_statistics_worker(g_msg.file.statistics_log, top_msgs, msgs_found,
        MSG_MESSAGES_WITH_TOP_BUFFER_USAGE);
}


/**
 * @brief Print the percentage of format IDs that are used.
 */

static void print_number_of_fmt_ids_used(void)
{
    unsigned no_used = 0;       // Number of format IDs used
    unsigned no_defined = g_msg.fmt_ids_defined;

    if (no_defined > g_msg.hdr_data.topmost_fmt_id)
    {
        no_defined = g_msg.hdr_data.topmost_fmt_id;
    }
    
    for (unsigned i = 0; i < no_defined; i++)
    {
        if (g_fmt[i] != NULL)
        {
            no_used++;
        }
    }

    if (g_msg.file.statistics_log != NULL)
    {
        fprintf(g_msg.file.statistics_log,
                get_message_text(MSG_FMT_IDS_USED),
                no_used,
                g_msg.hdr_data.topmost_fmt_id,
                100. * (double)no_used / (double)g_msg.hdr_data.topmost_fmt_id
               );
    }
}


/**
 * @brief Report problematic messages to the output file.
 *
 * @param out  Pointer to the output file
 */

static void report_problematic_messages(FILE *out)
{
    if (g_msg.total_bad_packet_words > 0)
    {
        fprintf(out, get_message_text(MSG_STAT_MSGS_WITH_MISSING_FMT),
            g_msg.total_bad_packet_words);
    }
        
    if (g_msg.total_unfinished_words > 0)
    {
        fprintf(out, get_message_text(MSG_STAT_MSGS_WITH_UNFINISHED_WORDS),
            g_msg.total_unfinished_words);
    }
}


/**
 * @brief Write common statistics information to the file 'Stat_main.txt'.
 * The following information is written to the main statistics file:
 *   - number of messages processed
 *   - number and percentage of total format IDs used
 *   - list of 10 message types with the highest frequency of occurrence
 *   - list of 10 messages with the largest circular buffer space consumption
 *   - number of messages with errors found during decoding
 *   - number of separate snapshots (if multiple snapshots have been assembled into one binary data file).
 */

void print_common_statistics(void)
{
    if (g_msg.message_cnt == 0)
    {
        return;
    }

    FILE *out = g_msg.file.statistics_log;

    if (out == NULL)
    {
        return;
    }

    fprintf(out, get_message_text(MSG_STAT_TOTAL_MESSAGES),
        g_msg.message_cnt);

    report_problematic_messages(out);

    if (g_msg.multiple_logging > 1)
    {
        fprintf(out, get_message_text(MSG_STAT_MULTIPLE_LOGGING),
            g_msg.multiple_logging);
    }

    print_number_of_fmt_ids_used();

    // Do not display messages with the highest frequencies if no messages have been decoded
    if (g_msg.message_cnt > 1uL)
    {
        print_messages_with_top_frequencies();
        print_messages_with_top_buffer_usage();
    }
}


/**
 * @brief Write collected value statistics data for minimal/maximal values.
 */

void print_value_statistics(void)
{
    FILE *out = fopen(RTE_STAT_VALUES_FILE, "w");

    if (out == NULL)
    {
        report_problem_with_string(FATAL_CANT_CREATE_FILE, RTE_STAT_VALUES_FILE);
        return;
    }

    int values_found = 0;       // Number of message values for which statistics were found
    msg_data_t *p_already_processed = NULL;

    for (unsigned int i = 0; i < g_msg.fmt_ids_defined; i++)
    {
        msg_data_t *p_fmt = g_fmt[i];

        if (p_fmt == NULL)
        {
            continue;   // No messages of this type have been received
        }

        if (p_already_processed == p_fmt)
        {
            continue;   // The structure has been processed already
        }

        p_already_processed = p_fmt;

        value_format_t *p_fmt_val = p_fmt->format;

        while (p_fmt_val != NULL)
        {
            if (p_fmt_val->value_stat != NULL)
            {
                if (p_fmt_val->value_stat->counter != 0)
                {
                    write_data_for_one_value(out, p_fmt->message_name, p_fmt_val);
                    values_found++;
                }
            }

            p_fmt_val = p_fmt_val->format;
        }
    }

    if (values_found == 0)
    {
        fprintf(out, get_message_text(MSG_NO_VALUE_STATISTICS_FOUND));
    }

    fclose(out);
}


/**
 * @brief Write data to all statistics files (if enabled).
 */

void write_statistics_to_file(void)
{
    // Add all counter values to the counter_total to get correct statistical data
    reset_statistics();

    open_output_folder();
    print_common_statistics();

    if (g_msg.param.message_statistics_enabled)
    {
        print_message_frequency_statistics();
    }

    if (g_msg.message_cnt > 0)    // If messages have been found, then write the statistics data
    {
        if (g_msg.param.value_statistics_enabled)
        {
            print_value_statistics();
        }
    }
}


/**
 * @brief Add the current values of message counters to the counter_total
 *        and reset the statistic counters to zero.
 *        This function is called after a sleep message is processed or
 *        if some other potential timing problem in the binary data file is found.
 */

void reset_statistics(void)
{
    msg_data_t *p_already_processed = NULL;

    g_msg.error_warning_in_msg = g_msg.message_cnt + 1;
        // A warning is displayed after the error(s)

    for (unsigned int i = 0; i < g_msg.fmt_ids_defined; i++)
    {
        msg_data_t *p_fmt = g_fmt[i];

        if (p_fmt == NULL)
        {
            continue;   // No messages of this type have been received
        }

        if (p_already_processed == p_fmt)
        {
            continue;   // The structure has been processed already
        }

        p_already_processed = p_fmt;

        // Add the message counter values to the total count and reset the counters
        p_fmt->counter_total += p_fmt->counter;
        p_fmt->counter = 0;
    }

    g_msg.timestamp.searched_to_index = 0;
    g_msg.timestamp.no_previous_tstamp = true;
    g_msg.timestamp.mark_problematic_tstamps = false;
    g_msg.timestamp.old = 0;

    g_msg.messages_processed_after_restart = 0;
}


/**
 * @brief Generate statistics about the messages that have been found and
 *        the list of messages that have not been found in the binary file.
 */

void print_message_frequency_statistics(void)
{
    msg_data_t *p_already_processed = NULL;

    FILE *msgs_found = NULL;
    FILE *msgs_not_found = NULL;

    errno_t rez = fopen_s(&msgs_found, RTE_STAT_MSG_COUNTERS_FILE, "w");

    if (rez != 0)
    {
        report_problem_with_string(FATAL_CANT_CREATE_FILE, RTE_STAT_MSG_COUNTERS_FILE);
    }

    rez = fopen_s(&msgs_not_found, RTE_STAT_MISSING_MSGS_FILE, "w");

    if (rez != 0)
    {
        report_problem_with_string(FATAL_CANT_CREATE_FILE, RTE_STAT_MISSING_MSGS_FILE);
    }

    for (unsigned int i = 0; i < g_msg.fmt_ids_defined; i++)
    {
        msg_data_t *p_fmts = g_fmt[i];

        if (p_fmts == NULL)
        {
            continue;   // No messages of this type have been received
        }

        if (p_already_processed == p_fmts)
        {
            continue;   // The structure has been processed already
        }

        p_already_processed = p_fmts;

        uint32_t counter = p_fmts->counter_total;
        const char *message_name = p_fmts->message_name;

        if (message_name == NULL)
        {
            message_name = get_message_text(MSG_UNDEFINED_NAME);
        }

        if (counter == 0)
        {
            if (msgs_not_found != NULL)
            {
                fprintf(msgs_not_found, "%s\n", message_name);
            }
        }
        else
        {
            if (msgs_found != NULL)
            {
                fprintf(msgs_found, "%5u - %s\n", counter, message_name);
            }
        }
    }

    if (msgs_found != NULL)
    {
        fclose(msgs_found);
    }

    if (msgs_not_found != NULL)
    {
        fclose(msgs_not_found);
    }
}


#if MIN_MAX_VALUES < 2
#error "MIN_MAX_VALUES must be > 1"
#endif
#if TOP_MESSAGES < 2
#error "TOP_MESSAGES must be > 1"
#endif

/*==== End of file ====*/
