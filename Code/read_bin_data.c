/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    read_bin_data.c
 * @author  B. Premzel
 * @brief   Read data from the binary data file into memory and prepare it
 *          for decoding.
 ******************************************************************************/

#include "pch.h"
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include "read_bin_data.h"
#include "errors.h"
#include "files.h"
#include "utf8_helpers.h"


/**
 * @brief Checks if there is any data (words different from 0xFFFFFFFF) in the buffer.
 *
 * @return      DATA_FOUND    - Data is present in the buffer.
 *              NO_DATA_FOUND - No data found in the buffer.
 */

int data_in_the_buffer(void)
{
    // Return NO_DATA_FOUND if the buffer is empty
    if (g_msg.in_size == 0)
    {
        return NO_DATA_FOUND;
    }

    // Return NO_DATA_FOUND if the index exceeds the buffer size
    if (g_msg.index >= g_msg.in_size)
    {
        return NO_DATA_FOUND;
    }

    // Search for the first word not equal to 0xFFFFFFFF
    while (g_msg.index < g_msg.in_size)
    {
        uint32_t data = g_msg.rte_buffer[g_msg.index];

        if (data != 0xFFFFFFFFuL)
        {
            return DATA_FOUND;
        }

        g_msg.unfinished_words++;
        g_msg.index++;
    }

    return NO_DATA_FOUND;
}


/**
 * @brief Loads binary data from the binary data file into the buffer.
 *
 * @param buffer   Pointer to the start of the buffer where data should be read.
 * @param no_words Number of 32-bit words to read.
 *
 * @return Number of words successfully read from the file.
 */

static uint32_t load_bin_words(uint32_t *buffer, uint32_t no_words)
{
    if (g_msg.file.rte_data == NULL)
    {
        return 0;
    }

    size_t words_read = fread(buffer, sizeof(uint32_t), no_words, g_msg.file.rte_data);

    if ((words_read != no_words) || ferror(g_msg.file.rte_data))
    {
        report_problem(ERR_READ_BIN_FILE_PROBLEM, (int)words_read);
        fprintf(g_msg.file.main_log, get_message_text(MSG_SIZE_SHOULD_BE), no_words);

        // Fill the remainder of the buffer with 0xFFFFFFFF (invalid data)
        memset(&buffer[words_read], 0xFF, (no_words - words_read) * sizeof(uint32_t));
    }

    return (uint32_t)words_read;
}


/**
 * @brief Loads additional data into the buffer for binary data processing.
 *        Copies remaining data to the start of the buffer and resets the index.
 *        Loads new data into the rest of the buffer.
 */

void load_data_block(void)
{
    if (g_msg.complete_file_loaded)
    {
        return;
    }

    uint32_t space_in_buffer = RTEDBG_BUFFER_SIZE;
    uint32_t remaining_words = g_msg.in_size - g_msg.index;

    if (g_msg.index < g_msg.in_size)
    {
        if (remaining_words >= RTEDBG_BUFFER_SIZE)
        {
            report_fatal_error_and_exit(FATAL_INTERNAL_ERROR, TXT_REMAINING_WORDS, remaining_words);
        }

        // Move remaining data to the start of the buffer
        memmove(&g_msg.rte_buffer[0], &g_msg.rte_buffer[g_msg.index], sizeof(uint32_t) * remaining_words);
        g_msg.already_processed_data += g_msg.index;

        // Calculate available space in the buffer
        space_in_buffer = RTEDBG_BUFFER_SIZE - remaining_words;
    }

    g_msg.index = 0;
    g_msg.in_size -= remaining_words;

    if (space_in_buffer > 0)
    {
        uint32_t words_read = load_bin_words(&g_msg.rte_buffer[g_msg.in_size], space_in_buffer);

        if ((words_read == 0) || feof(g_msg.file.rte_data))
        {
            g_msg.complete_file_loaded = true;
        }

        g_msg.in_size += words_read;     // Update the buffer size with the number of words read
    }
}


/**
 * @brief Reads raw binary data from a file containing pre-processed data collected during
 *        streaming mode logging or multiple snapshot/single-shot data logging sessions.
 *        The buffer index must be initialized to zero.
 *
 * @param file_size  Size of the binary file in bytes, excluding the rtedbg_header.
 * 
 * NOTE: This function is untested as the tools for streaming data collection
 *       are still under development. The file format specification may change.
 */

static void load_streaming_log_data(__int64 file_size)
{
    if (file_size < 4)
    {
        report_fatal_error_and_exit(FATAL_NO_BIN_DATA, NULL, 0);
    }

    if (g_msg.rte_header.last_index != 0)
    {
        report_problem(ERR_INDEX_SHOULD_BE_ZERO, g_msg.rte_header.last_index);
    }

    // Allocate memory for the binary data buffer
    g_msg.rte_buffer = (uint32_t *)allocate_memory(RTEDBG_BUFFER_SIZE * sizeof(uint32_t), "binFile");
    g_msg.rte_buffer_size = RTEDBG_BUFFER_SIZE;

    // Load the initial data block from the binary data file
    g_msg.in_size = 0;
    g_msg.index = 0;
    g_msg.complete_file_loaded = false;
    load_data_block();
}


/**
 * @brief Loads post-mortem (or snapshot) data starting from the index
 *        pointing to the 'oldest' data, which was logged first.
 *        This data contains 0xFFFFFFFF if the circular buffer was not filled
 *        completely at least once. This is part 1/2 or a complete load if last_index is zero.
 *
 * @param data_size   Number of words to load from the binary file.
 * @param index       Index from which the data should be loaded.
 */

static void load_post_mortem_data_part1(uint32_t data_size, uint32_t index)
{
    if (data_size > g_msg.rte_buffer_size)
    {
        report_fatal_error_and_exit(FATAL_INTERNAL_ERROR_VALUE_TOO_LARGE, "bin load 1 of 2", data_size);
    }

    fseek(g_msg.file.rte_data, sizeof(rtedbg_header_t) + sizeof(uint32_t) * index, SEEK_SET);

    if (ferror(g_msg.file.rte_data))
    {
        report_fatal_error_and_exit(ERR_BIN_DATA_FILE_FSEEK, NULL, errno);
    }

    (void)load_bin_words(g_msg.rte_buffer, data_size);

    // Skip initial words with the value 0xFFFFFFFF (if any)
    uint32_t i;

    for (i = 0; i < data_size; i++)
    {
        if (g_msg.rte_buffer[i] != 0xFFFFFFFFuL)
        {
            break;
        }
    }

    g_msg.index = i;
    g_msg.in_size = data_size;
}


/**
 * @brief Loads post-mortem (or snapshot) data preceding the index.
 *        Part 2/2 - data found in the buffer before last_index.
 *
 * @param data_size   Size of the buffer reserved for the binary data.
 * @param start_index Index from which the data will be loaded.
 */

static void load_post_mortem_data_part2(uint32_t data_size, uint32_t start_index)
{
    if ((data_size + g_msg.in_size) > g_msg.rte_buffer_size)
    {
        report_fatal_error_and_exit(FATAL_INTERNAL_ERROR_VALUE_TOO_LARGE, "bin load 2 of 2", data_size);
    }

    if (data_size > 0)
    {
        fseek(g_msg.file.rte_data, sizeof(rtedbg_header_t) + sizeof(uint32_t) * start_index, SEEK_SET);

        if (ferror(g_msg.file.rte_data))
        {
            report_fatal_error_and_exit(ERR_BIN_DATA_FILE_FSEEK, NULL, errno);
        }

        uint32_t words_read = load_bin_words(&g_msg.rte_buffer[g_msg.in_size], data_size);
        g_msg.in_size += words_read;
    }
}


/**
 * @brief Checks the last four words of the buffer to determine if they contain part of a message.
 *        The end of the circular buffer includes four additional words to speed up data logging.
 *        The FMT word (with bit 0 = 1) is the last message word.
 *
 * @param buffer_size Size of the buffer.
 *
 * @return Number of words to skip at the end of the buffer.
 */

static uint32_t check_data_at_end_of_circular_buffer(uint32_t buffer_size)
{
    if (buffer_size < 5UL)
    {
        return 0;
    }

    // Have the last words in the buffer not been overwritten yet?
    if (g_msg.rte_buffer[buffer_size - 5UL] == 0xFFFFFFFFUL)
    {
        return 4;   // Do not skip any data at the start of the buffer
    }

    uint32_t i;

    for (i = buffer_size - 5UL; i < buffer_size; i++)
    {
        if ((g_msg.rte_buffer[i] & 1UL) != 0)
        {
            break;
        }
    }

    return buffer_size - i - 1UL;
}


/**
 * @brief Counts the number of words in the buffer that are 0xFFFFFFFF (indicating emptiness).
 *        The search stops at the first non-empty word.
 *
 * @param buffer  Pointer to the binary data buffer.
 * @param size    Size of the buffer.
 * 
 * @return The number of words that contain 0xFFFFFFFF.
 */

static uint32_t check_empty_data(uint32_t *buffer, uint32_t size)
{
    uint32_t counter = 0;

    do
    {
        if (*buffer++ != 0xFFFFFFFFuL)
        {
            break;
        }

        counter++;
    }
    while (--size != 0);

    return counter;
}

/**
 * @brief Determines if the end of the buffer contains only empty data and should be excluded from decoding.
 *
 * @param last_index   The index where data logging stopped.
 * @param words_read   The number of data words read from the input file.
 *
 * @return true if empty data is found at the end of the circular buffer, false otherwise.
 */

static bool empty_data_at_end_of_buffer(uint32_t last_index, uint32_t words_read)
{
    bool empty_data_at_end = true;

    for (uint32_t i = last_index; i < words_read; i++)
    {
        if (g_msg.rte_buffer[i] != 0xFFFFFFFFuL)
        {
            empty_data_at_end = false;
            break;
        }
    }

    return empty_data_at_end;
}


/**
 * @brief Validates the binary data file size against the expected buffer size from the header.
 *        Adjusts the buffer size if the file contains more or less data than expected.
 *
 * @param  data_size  Size of the binary file (in bytes) excluding the rtedbg_header.
 * @return true if the buffer size was changed, false otherwise.
 */

static bool check_data_size(int64_t data_size)
{
    bool buffer_size_changed = false;

    // Expected size of the data logging buffer (in 32-bit words), including a 4-word trailer.
    uint32_t buffer_size = g_msg.rte_header.buffer_size;

    if (buffer_size == 0)
    {
        report_fatal_error_and_exit(FATAL_BUFFER_SIZE_IN_HEADER_IS_ZERO, NULL, 0);
    }

    if (data_size > (int64_t)(buffer_size * sizeof(uint32_t)))
    {
        /* The complete file will be interpreted, although the header shows that there should be
         * less data. Buffer size is increased to the size of file although the remainder of file
         * could contain erroneous data. */
        report_problem(ERR_BIN_FILE_CONTAINS_TOO_MUCH_DATA, (int)buffer_size);
        buffer_size = (uint32_t)((uint64_t)data_size / sizeof(uint32_t));
        buffer_size_changed = true;
    }
    else if (data_size < (int64_t)(buffer_size * sizeof(uint32_t)))
    {
        // The file contains less data than expected. Adjust the buffer size to match the file size.
        report_problem(ERR_NOT_ENOUGH_DATA_IN_BIN_FILE, (int)buffer_size);
        buffer_size = (uint32_t)((uint64_t)data_size / sizeof(uint32_t));
        buffer_size_changed = true;
    }

    if (buffer_size > MAX_RTEDBG_BUFFER_SIZE)
    {
        buffer_size = MAX_RTEDBG_BUFFER_SIZE;
        buffer_size_changed = true;
        report_problem(ERR_MESSAGE_FILE_SIZE_TRUNCATED, MAX_RTEDBG_BUFFER_SIZE * sizeof(uint32_t));
    }

    // Ensure the last circular buffer index is within bounds.
    if (g_msg.rte_header.last_index >= buffer_size)
    {
        g_msg.rte_header.last_index = buffer_size;
        report_problem(ERR_INDEX_IN_CIRCULAR_BUFFER_OUT_OF_RANGE, g_msg.rte_header.last_index);
    }

    g_msg.rte_buffer_size = buffer_size;
    return buffer_size_changed;
}


/**
 * @brief Reads raw binary data from a file containing the g_rte_dbg structure from the embedded system.
 *        This function prepares data collected during post-mortem data logging mode.
 *        Snapshots are logged using the same mode and decoded similarly.
 *        Data logging may be interrupted at any time, and the buffer may be empty or overwritten multiple times.
 *        For post-mortem logging, last_index can be anywhere in the circular buffer.
 *        Data must be reorganized so that the most recently written data in the circular buffer is decoded first.
 *        The value of last_index points to the location past the last FMT word written.
 *        Data loading is divided into two parts for better readability.
 *        Part 1 loads data after last_index (oldest data), and Part 2 loads data before last_index.
 *
 * @param data_size  Size of the binary file (in bytes) excluding the rtedbg_header.
 */

static void load_post_mortem_data(int64_t data_size)
{
    bool buffer_size_changed = check_data_size(data_size);
    uint32_t last_index = g_msg.rte_header.last_index;

    // Allocate the necessary buffer space
    uint32_t buffer_size = g_msg.rte_header.buffer_size;
    g_msg.rte_buffer_size = 16u + buffer_size;
    g_msg.rte_buffer = (uint32_t *)allocate_memory(g_msg.rte_buffer_size * sizeof(uint32_t), "binFil2");

    // Read the complete circular buffer contents
    fseek(g_msg.file.rte_data, sizeof(rtedbg_header_t), SEEK_SET);

    if (ferror(g_msg.file.rte_data))
    {
        report_fatal_error_and_exit(ERR_BIN_DATA_FILE_FSEEK, NULL, errno);
    }

    uint32_t words_read = load_bin_words(g_msg.rte_buffer, buffer_size);

    if (words_read != buffer_size)
    {
        buffer_size = words_read;   // Adjust the size to the actual number of words read from the file
        buffer_size_changed = true;

        if (last_index > words_read)
        {
            g_msg.in_size = words_read;
            return;
        }
    }

    uint32_t empty_data_at_start = check_empty_data(g_msg.rte_buffer, last_index);

    if (empty_data_at_end_of_buffer(last_index, words_read))
    {
        g_msg.index = empty_data_at_start;
        g_msg.in_size = last_index;
        return;
    }

    uint32_t skip_at_start = 0;
    uint32_t skip_at_end = check_data_at_end_of_circular_buffer(buffer_size);

    if (buffer_size_changed)
    {
        skip_at_end = 0;
    }
    else
    {
        if (g_msg.hdr_data.buffer_size_is_power_of_2 && (buffer_size > (2u * 4u)))
        {
            /* If the embedded system circular buffer size (RTE_BUFFER_SIZE) is a power of 2
             * then (according to the method of limiting the buffer index) four words are
             * dropped every time. Example: if three words are dropped at the end of
             * circular buffer then one word has to be dropped at the start. */
            skip_at_start = 4 - skip_at_end;
        }
    }
    
    load_post_mortem_data_part1(buffer_size - last_index - skip_at_end, last_index);
    load_post_mortem_data_part2(last_index - skip_at_start, skip_at_start);
}


/**
 * @brief Loads raw binary data from a file containing the g_rtedbg structure from the embedded system.
 *        This function prepares data collected during single-shot data logging mode.
 *        The buffer index defines where data logging stopped. Data was written to the buffer only once.
 *        Note: This function is also used if the software cannot determine the logging mode.
 *
 * @param data_size  Size of the binary file (in bytes) excluding the rtedbg_header.
 */

static void load_single_shot_data(int64_t data_size)
{
    if ((g_msg.rte_header.last_index == 0)     // index = 0 - data was not logged yet
        && (RTE_SINGLE_SHOT_WAS_ACTIVE))
    {
        report_fatal_error_and_exit(FATAL_SINGLE_SHOT_AND_INDEX_IS_ZERO, NULL, 0);
    }

    (void)check_data_size(data_size);

    uint32_t buffer_size = g_msg.rte_buffer_size;
        // Size of the data logging buffer (number of 32-bit words) including the extra 4 words (buffer trailer)

    g_msg.rte_buffer = (uint32_t *)allocate_memory(buffer_size * sizeof(uint32_t), "binFil1");

    // Skip the binary file header
    fseek(g_msg.file.rte_data, sizeof(rtedbg_header_t), SEEK_SET);

    if (ferror(g_msg.file.rte_data))
    {
        report_fatal_error_and_exit(ERR_BIN_DATA_FILE_FSEEK, NULL, errno);
    }

    // Load the captured data
    uint32_t words_read = load_bin_words(g_msg.rte_buffer, buffer_size);
    g_msg.in_size = words_read;

    // Skip the initial words with a value of 0xFFFFFFFF (data not written)
    uint32_t i;

    for (i = 0; i < g_msg.in_size; i++)
    {
        if (g_msg.rte_buffer[i] != 0xFFFFFFFFuL)
        {
            break;
        }
    }

    g_msg.index = i;
}


/**
 * @brief Reads raw binary data from a file containing the g_rtedbg structure from the embedded system.
 *        This function determines the appropriate loading method based on the logging mode and prepares
 *        the data for decoding if necessary.
 */

void load_data_from_binary_file(void)
{
    int64_t size = get_file_size(g_msg.file.rte_data);

    /* Ensure file size is a multiple of 4, as 32-bit values are recorded.
     * The check for size >= sizeof(rtedbg_header_t) is performed in load_and_check_rtedbg_header(). */
    if ((size & 3) != 0)
    {
        report_problem(ERR_BIN_FILE_SIZE_NOT_DIVISIBLE_BY_4, 0);
        size &= ~3uLL;       // Remove any extra bytes
    }

    size -= sizeof(rtedbg_header_t);
        // Adjust size to exclude the header, which has already been loaded

    switch (g_msg.hdr_data.logging_mode)
    {
        case MODE_POST_MORTEM:
            if (g_msg.rte_header.last_index > g_msg.rte_header.buffer_size)
            {
                // Possible data corruption. Attempt to load as single-shot mode for decoding.
                report_problem(ERR_INDEX_IN_CIRCULAR_BUFFER_OUT_OF_RANGE, g_msg.rte_header.last_index);
                load_single_shot_data(size);
            }
            else
            {
                if (size < (int64_t)(g_msg.rte_header.buffer_size * sizeof(uint32_t)))
                {
                    // Data may have been intentionally shortened (e.g., incomplete buffer fill after single-shot start).
                    load_single_shot_data(size);
                }
                else
                {
                    load_post_mortem_data(size);
                }
            }

            g_msg.complete_file_loaded = true;
                // Entire data file has been loaded (unless shortened by check_data_size function)
            fclose(g_msg.file.rte_data);
            break;

        case MODE_SINGLE_SHOT:
            load_single_shot_data(size);

            if (g_msg.in_size > g_msg.rte_header.last_index)
            {
                g_msg.in_size = g_msg.rte_header.last_index;
            }

            g_msg.complete_file_loaded = true;
                // Entire data file has been loaded (unless shortened by check_data_size function)
            fclose(g_msg.file.rte_data);
            break;

        case MODE_STREAMING:
        case MULTIPLE_DATA_CAPTURE:
            load_streaming_log_data(size);
            break;

        default:
            report_fatal_error_and_exit(FATAL_UNKNOWN_LOGGING_MODE, NULL, g_msg.hdr_data.logging_mode);
    }
}


/**
 * @brief  Determines the data logging mode and initializes related variables.
 */

static void check_logging_mode(void)
{
    if (g_msg.hdr_data.single_shot_enabled && g_msg.hdr_data.single_shot_active)
    {
        g_msg.hdr_data.logging_mode = MODE_SINGLE_SHOT;
    }
    else
    {
        if (g_msg.rte_header.buffer_size < 0xFFFF0000)
        {
            g_msg.hdr_data.logging_mode = MODE_POST_MORTEM;
        }
        else
        {
            switch (g_msg.rte_header.buffer_size)
            {
                case MODE_STREAMING:
                case MULTIPLE_DATA_CAPTURE:
                    g_msg.hdr_data.logging_mode = g_msg.rte_header.buffer_size;
                    break;

                default:
                    /* Error is not reported here. It will be reported when the buffer
                     * size will be checked later (buffer size too large). */
                    g_msg.hdr_data.logging_mode = MODE_UNKNOWN;
                    break;
            }
        }
    }
}


/**
 * @brief  Loads the RTEdbg data structure header from the binary data file and validates it.
 */

void load_and_check_rtedbg_header(void)
{
    jump_to_start_folder();
    
    if (g_msg.param.data_file_name == NULL)
    {
        report_fatal_error_and_exit(FATAL_NO_BIN_FILE, NULL, 0);
    }

    FILE *bin_data_file = utf8_fopen(g_msg.param.data_file_name, "rb");

    if (bin_data_file == NULL)
    {
        report_fatal_error_and_exit(FATAL_OPEN_BIN_DATA_FILE, g_msg.param.data_file_name, ~1uLL);
    }

    g_msg.file.rte_data = bin_data_file;

    // Ensure the file size is at least as large as the RTEdbg structure header
    int64_t file_size = get_file_size(bin_data_file);

    if (sizeof(rtedbg_header_t) >= (uint64_t)file_size)
    {
        report_fatal_error_and_exit(FATAL_FILE_MUST_CONTAIN_MIN_DATA_SIZE,
            g_msg.param.data_file_name, file_size);
    }

    size_t data_read = fread(&g_msg.rte_header, 1, sizeof(g_msg.rte_header), bin_data_file);

    if ((data_read != sizeof(g_msg.rte_header)) || ferror(bin_data_file))
    {
        report_fatal_error_and_exit(FATAL_READ_BIN_DATA_FILE, g_msg.param.data_file_name, ~1uLL);
    }

    // Validate the header values
    if (sizeof(rtedbg_header_t) != RTE_HEADER_SIZE)
    {
        report_fatal_error_and_exit(FATAL_BAD_HEADER_SIZE, NULL, 0);
    }

    if ((RTE_CFG_RESERVED_BITS != 0) || (RTE_CFG_RESERVED2 != 0))
    {
        report_fatal_error_and_exit(FATAL_HDR_RESERVED_BITS_NON_ZERO, NULL, 0);
    }

    if (g_msg.rte_header.timestamp_frequency == 0)
    {
        report_problem(ERR_INITIAL_TIMESTAMP_FREQUENCY_ZERO, 0);
        g_msg.rte_header.timestamp_frequency = 1;
    }

    /* Use the last timestamp frequency value (from data logging structure).
     * For applications that switch frequency often the exact frequency is not
     * known for the first part of decoding - until the first message with 
     * frequency information (MSG1_SYS_TSTAMP_FREQUENCY) is found in the buffer. */
    g_msg.timestamp.current_frequency = g_msg.rte_header.timestamp_frequency;

    // Unpack the values from binary file header for faster execution during the bin data decoding
    uint8_t fmt_id_bits = RTE_FMT_ID_BITS + 9u;
    g_msg.hdr_data.single_shot_enabled = RTE_SINGLE_SHOT_LOGGING_ENABLED;
    g_msg.hdr_data.long_timestamp_used = RTE_USE_LONG_TIMESTAMP;
    g_msg.hdr_data.single_shot_active  = RTE_SINGLE_SHOT_WAS_ACTIVE;
    g_msg.hdr_data.timestamp_shift     = RTE_TIMESTAMP_SHIFT;
    g_msg.hdr_data.max_msg_blocks      = RTE_MAX_MSG_BLOCKS;
    g_msg.hdr_data.buffer_size_is_power_of_2 = RTE_BUFF_SIZE_IS_POWER_OF_2;
    g_msg.timestamp.multiplier = (double)(1ULL << g_msg.hdr_data.timestamp_shift)
                                / (double)g_msg.timestamp.current_frequency
                                / (double)(1uLL << (1u + fmt_id_bits));

    /* Prepare the values used during message processing in the
     * process_the_message_packet() and assemble_message() functions. */
    g_msg.hdr_data.fmt_id_bits = fmt_id_bits;
    g_msg.hdr_data.timestamp_and_index_mask = 0xFFFFFFFE & (~(0xFu << (32ul - fmt_id_bits)));
    g_msg.hdr_data.fmt_id_shift = 32u - fmt_id_bits;

    if (g_msg.hdr_data.fmt_id_bits > MAX_FMT_ID_BITS)
    {
        report_fatal_error_and_exit(FATAL_FMT_ID_BITS_TOO_LARGE, NULL, MAX_FMT_ID_BITS);
    }

    if (g_msg.hdr_data.fmt_id_bits != g_msg.param.number_of_format_id_bits)
    {
        report_fatal_error_and_exit(FATAL_FMT_ID_BITS_DOES_NOT_MATCH,
            NULL, g_msg.hdr_data.fmt_id_bits);
    }

    if (g_msg.hdr_data.single_shot_active && (!g_msg.hdr_data.single_shot_enabled))
    {
        report_problem(ERR_SINGLE_SHOT_ACTIVE_BUT_NOT_ENABLED_IN_FW, 0);
    }

    check_logging_mode();
}


/**
 * @brief  Prints the current data logging mode: single-shot, post-mortem, etc.
 */

static void print_data_logging_mode(void)
{
    switch (g_msg.hdr_data.logging_mode)
    {
        case MODE_POST_MORTEM:
            fprintf(g_msg.file.main_log, get_message_text(MSG_POST_MORTEM_LOGGING));

            if (g_msg.param.debug)
            {
                fprintf(g_msg.file.main_log, get_message_text(MSG_SINGLE_SHOT_MODE),
                    g_msg.hdr_data.single_shot_active ?
                    get_message_text(MSG_ENABLED) : get_message_text(MSG_DISABLED)
                );
            }
            break;

        case MODE_SINGLE_SHOT:
            fprintf(g_msg.file.main_log, get_message_text(MSG_SINGLE_SHOT_LOGGING));
            break;

        case MODE_STREAMING:
            fprintf(g_msg.file.main_log, get_message_text(MSG_STREAMING_MODE_LOGGING));
            break;

        case MULTIPLE_DATA_CAPTURE:
            fprintf(g_msg.file.main_log, get_message_text(MSG_MULTIPLE_DATA_CAPTURE));
            break;

        default:
            report_problem(ERR_UNKNOWN_LOGGING_MODE, g_msg.hdr_data.logging_mode);
            break;
    }
}


/**
 * @brief Prints the names of enabled message filters.
 *        If no filters are enabled, displays values for filter_copy.
 */

static void print_filter_info(void)
{
    fprintf(g_msg.file.main_log, get_message_text(MSG_HEADER_INFO_FILTER),
        g_msg.rte_header.filter, g_msg.rte_header.filter_copy);
    uint32_t filter = g_msg.rte_header.filter;
    uint32_t filter_copy = g_msg.rte_header.filter_copy;

    // Check if any filters are defined
    unsigned filters_defined = 0;

    for (uint32_t i = 0; i < NUMBER_OF_FILTER_BITS; i++)
    {
        if (g_msg.enums[i].name != NULL)
        {
            filters_defined++;
        }
    }

    if (filters_defined > 0)
    {
        fprintf(g_msg.file.main_log, get_message_text(MSG_ENABLED_FILTER_NAMES));
    }

    for (uint32_t i = 0; i < NUMBER_OF_FILTER_BITS; i++)
    {
        unsigned filter_enabled = 0;

        if ((filter & 0x80000000UL) != 0)
        {
            filter_enabled = 1;
        }

        unsigned filter_copy_enabled = 0;

        if ((filter_copy & 0x80000000UL) != 0)
        {
            filter_copy_enabled = 1;
        }

        const char *name = "";

        if (g_msg.enums[i].filter_description != NULL)
        {
            name = g_msg.enums[i].filter_description;
        }
        else if (g_msg.enums[i].name != NULL)
        {
            name = g_msg.enums[i].name;
        }

        if (*name != '\0')
        {
            fprintf(g_msg.file.main_log, "%3u = %u(%u) \"%s\"\n", i, filter_enabled, filter_copy_enabled, name);
        }

        filter <<= 1u;
        filter_copy <<= 1u;
    }
}


/**
 * @brief Prints information from the binary file header.
 *        Provides additional details if debug mode is enabled.
 */

void print_bin_file_header_info(void)
{
    if (g_msg.rte_header.buffer_size != 0xFFFFFFFF)     // Skip buffer size for streaming mode
    {
        fprintf(g_msg.file.main_log, get_message_text(MSG_HEADER_INFO),
            g_msg.rte_header.buffer_size,
            g_msg.rte_header.last_index);
    }

    double frequency =
        (double)g_msg.rte_header.timestamp_frequency / (double)(1ULL << g_msg.hdr_data.timestamp_shift);
    double timestamp_period_ms =
        1000.0 / frequency * (double)(1ULL << (32U - 1U - g_msg.hdr_data.fmt_id_bits));
    fprintf(g_msg.file.main_log, get_message_text(MSG_HEADER_INFO2),
        (double)g_msg.rte_header.timestamp_frequency / 1e6,
        (1u << g_msg.hdr_data.timestamp_shift),
        frequency / 1e6,
        timestamp_period_ms);

    if (g_msg.param.debug)
    {
        // Print configuration word details
        fprintf(g_msg.file.main_log, get_message_text(MSG_HEADER_INFO_CFG),
            g_msg.hdr_data.fmt_id_bits, g_msg.hdr_data.max_msg_blocks * 16uL);

        const char *txt_yes = get_message_text(MSG_YES);
        const char *txt_no  = get_message_text(MSG_NO);
        fprintf(g_msg.file.main_log, get_message_text(MSG_HEADER_PWR2_AND_LONG_TSTAMP),
            g_msg.hdr_data.buffer_size_is_power_of_2 ? txt_yes : txt_no,
            g_msg.hdr_data.long_timestamp_used ? txt_yes : txt_no);
    }

    print_data_logging_mode();

    if (!RTE_MSG_FILTERING_ENABLED)
    {
        fprintf(g_msg.file.main_log, get_message_text(MSG_FILTERING_DISABLED));
    }
    else
    {
        print_filter_info();
    }
}


/**
 * @brief Prints the introductory text that describes message info printed to the Main.log file.
 */

void print_msg_intro(void)
{
    const char *unit_txt = "[s] ";

    switch (g_msg.param.time_unit)
    {
        case 'm':
            unit_txt = "[ms]";
            break;

        case 'u':
            unit_txt = "[us]";
            break;

        default:
            break;
    }

    // Print the unit used for timestamp values
    fprintf(g_msg.file.main_log, get_message_text(MSG_MAIN_INTRO), unit_txt);
    fprintf(g_msg.file.main_log, "\n- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n");
}

/*==== End of file ====*/
