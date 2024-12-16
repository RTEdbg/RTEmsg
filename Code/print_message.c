/*
 * Copyright (c) 2024 Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    print_message.c
 * @author  B. Premzel
 * @brief   Functions for printing binary message contents using the format
 *          definition linked lists prepared during the format definition file
 *          parsing.
 ******************************************************************************/

#include "pch.h"
#include <string.h>
#include <math.h>
#include "print_helper.h"
#include "print_message.h"
#include "statistics.h"
#include "files.h"
#include "errors.h"


#if _WIN64 != 1
#error "The software was tested in the 64-bit mode only."
#endif

// Union for conversion between uint32_t and float
typedef union convert_f32
{
    float data_f;
    uint32_t data_u;
} val_convert32_t;

// Union for conversion between uint64_t and double
typedef union convert_f64
{
    double data_f;
    uint64_t data_u;
} val_convert64_t;


/**
 * @brief Prints a binary value with a maximum length of 64 bits.
 *
 * @param out      Output file to which the text will be written.
 * @param value    Binary value to be written.
 * @param size     Number of binary digits to be printed.
 * @param max_size Maximum number of bits to be processed.
 */
 
static void print_binary64(FILE *out, uint64_t value, uint32_t size, uint32_t max_size)
{
    if (size == 0)
    {
        fprintf(out, "?");
        return;
    }

    if (size > max_size)
    {
        size = max_size;
    }

    uint64_t mask = 1uLL << (size - 1uLL);

    for (uint32_t i = 0; i < size; i++)
    {
        if ((((size - i) % 8ul) == 0) && (i != 0))
        {
            fprintf(out, "%c", '\'');
        }

        fprintf(out, "%c", (value & mask) ? '1' : '0');
        mask >>= 1uLL;
    }
}


/**
 * @brief Helper function to print the contents of a message as hexadecimal data.
 *
 * @param out      File to which the data will be printed.
 * @param message  Pointer to the message that will be printed.
 * @param size     Pointer to the number of bytes.
 * @param print_as Print data as bytes (1), 16-bit (2), or 32-bit (4) words.
 */
 
static void hex_print(FILE *out, unsigned char **message, unsigned int *size, int print_as)
{
    unsigned int i = 0;
    unsigned int current_index = 0;
    unsigned char *p_msg = *message;

    do
    {
        fprintf(out, "\n%3X: ", current_index);

        for (i = 0; i < 16u; i += print_as)
        {
            if (print_as == 4u)
            {
                uint32_t value = (p_msg[i + 3u] << 24u) | (p_msg[i + 2u] << 16u)
                    | (p_msg[i + 1] << 8u) | p_msg[i];
                fprintf(out, "%08X ", value);
            }
            else if (print_as == 2u)
            {
                fprintf(out, "%04X ", (p_msg[i + 1] << 8u) | p_msg[i]);
            }
            else
            {
                fprintf(out, "%02X ", p_msg[i]);
            }
        }

        *size -= 16u;
        p_msg += 16u;
        current_index += 16u;
    } while (*size > 16u);

    fprintf(out, "\n%3X: ", current_index);
    *message = p_msg;
}


/**
 * @brief Prints the contents of a complete message as hexadecimal data.
 *        The data can be printed as bytes, 16-bit words, or 32-bit words.
 *
 * @param out       File to which the data will be printed.
 * @param message   Pointer to the message to be printed.
 * @param size      Size of the data to be printed.
 * @param print_as  Format for printing the message (1=bytes, 2=16-bit words, 4=32-bit words).
 */
 
static void hex_print_complete_message(FILE *out, unsigned char *message, unsigned size, unsigned print_as)
{
    unsigned int i = 0;

    if (size > 16u)
    {
        // Print the initial part of the message in 16-byte chunks.
        hex_print(out, &message, &size, print_as);
    }

    // Print the remaining part of the message.
    switch (print_as)
    {
        case 4u:
            for (i = 0; i < size; i += 4u)
            {
                uint32_t value = (message[i + 3u] << 24u) | (message[i + 2u] << 16u)
                    | (message[i + 1u] << 8u) | message[i];
                fprintf(out, "%08X ", value);
            }
            break;

        case 2u:
            for (i = 0; i < size; i += 2u)
            {
                fprintf(out, "%04X ", (message[i + 1u] << 8u) | message[i]);
            }
            break;

        default:
            for (i = 0; i < size; i++)
            {
                fprintf(out, "%02X ", message[i]);
            }
            break;
    }

    if (size > 16u)
    {
        fprintf(out, "\n");
    }
}


/**
 * @brief Scales a value using a multiplier and offset.
 *        No scaling is applied if the multiplier is zero.
 *
 * @param fmt       Pointer to the current value descriptor.
 * @param data      Value to be scaled.
 */

static void value_scaling(value_format_t *fmt, double data)
{
    // Store the original data in case the multiplier is zero.
    g_msg.value.data_double = data;

    if (fmt->mult != 0)
    {
        g_msg.value.data_double = (data + fmt->offset) * fmt->mult;

        // Convert the scaled double value to integer if the format type is integer (%d, %u, etc.).
        g_msg.value.data_i64 = (int64_t)(g_msg.value.data_double + 0.5);
        g_msg.value.data_u64 = (uint64_t)(g_msg.value.data_double + 0.5);
    }
}


/**
 * @brief Assembles a value from a specified bit address and size in bits.
 *
 * @param size      Size of the value in bits.
 * @param address   Bit address of the value (number of bits to the first bit of the value).
 * @param message   Pointer to the buffer containing the message to be processed.
 *
 * @return Assembled value as a 64-bit unsigned integer.
 */

static uint64_t extract_bit_sized_value(uint32_t size, uint32_t address, uint8_t *message)
{
    uint64_t value = 0;

    while (size > 0)
    {
        unsigned bit_address = address & 7u;
        unsigned byte_address = address >> 3u;
        value >>= 1;

        if (*(message + byte_address) & (1u << bit_address))
        {
            value |= (1uLL << 63u);
        }

        address++;
        size--;
    }

    return value;
}


/**
 * @brief Extract the value with specified length starting with the specified
 *        bit address of the lowest value bit.
 *        Value is extracted to the 64-bit unsigned variable g_msg.value.data_u64
 *        and its signed value to the g_msg.value.data_i64.
 *
 * @param fmt    Pointer to the value descriptor for the current printed value.
 */

static void extract_value_from_message(value_format_t *fmt)
{
    uint32_t size = fmt->data_size;             // Number of bits to extract
    uint32_t address = fmt->bit_address;        // Bit address of the value in the message

    if (size == 0)         // If size is 0, use the entire message
    {
        // No need to check the bit address; address zero will be used by the printing function
        return;
    }

    if (size > 64u)
    {
        save_decoding_error(ERR_DECODE_VALUE_SIZE_TOO_LARGE, size, 64, fmt->fmt_string);
        return;
    }

    // Ensure the value fits within the received message length
    unsigned end_address = size + address;

    if (end_address > (g_msg.asm_size * 8u))
    {
        save_decoding_error(ERR_DECODE_VALUE_NOT_IN_MESSAGE, end_address,
            g_msg.asm_size * 8u, fmt->fmt_string);
        return;
    }

    uint8_t *message = (uint8_t *)g_msg.assembled_msg;
    uint64_t value = 0;

    if (((size | address) & 7) == 0)
    {
        // Extract the value as bytes since both size and address are byte-aligned
        address >>= 3;      // Convert bit address to byte address
        size >>= 3;         // Convert size from bits to bytes
        message += address;

        do
        {
            value >>= 8u;
            value |= ((uint64_t)*message) << (64u - 8u);
            message++;
        } while (--size != 0);
    }
    else
    {
        // Extract the value bit by bit
        value = extract_bit_sized_value(size, address, (uint8_t *)message);
    }

    unsigned shift = 64u - fmt->data_size;
    g_msg.value.data_u64 = value >> shift;
    g_msg.value.data_i64 = (int64_t)value;
    g_msg.value.data_i64 >>= shift;
}


/**
 * @brief Saves the current value to memory at the specified index.
 * @note  g_msg.value is initialized to zero at the start of value processing. If the value
 *        cannot be set correctly, g_msg.value remains zero for printing purposes.
 *
 * @param memo       Index in the g_msg.enums[] array.
 */

static void save_to_memo(rte_enum_t memo)
{
    if ((memo >= NUMBER_OF_FILTER_BITS) && (memo < MAX_ENUMS))
    {
        if (g_msg.enums[memo].type == MEMO_TYPE)
        {
            g_msg.enums[memo].memo_value = g_msg.value.data_double;
        }
        else
        {
            save_internal_decoding_error(INT_SET_MEMO_TYPE_IS_NOT_MEMO, memo);
        }
    }
    else
    {
        save_internal_decoding_error(INT_SET_MEMO_OUT_OF_RANGE, memo);
    }
}


/**
 * @brief Prepares a 32-bit variable for printing.
 *        This is used only for values where the number of bits and type are unspecified.
 *        The values are stored in the 'g_msg.value' structure.
 *
 * @param fmt    Pointer to the structure containing value preparation/conversion and print information.
 *
 * @return  true if an error is detected, false otherwise.
 */

static bool process_value_auto(value_format_t *fmt)
{
    // Union for converting integers to float or double
    val_convert32_t convert_value32;

    // Automatically determine the correct value type for a 32-bit value from the logged message
    // The bitfield address must be divisible by 32 and the length must be 32
    if ((fmt->bit_address % 32u) != 0)
    {
        save_decoding_error(ERR_DECODE_AUTO_VALUE_ADDRESS_NOT_DIVISIBLE_BY_32,
            fmt->bit_address, 32u, fmt->fmt_string);
        return true;
    }

    if (fmt->data_size != 32u)
    {
        save_decoding_error(ERR_DECODE_AUTO_VALUE_SIZE_NOT_32, fmt->data_size, 32u, fmt->fmt_string);
        return true;
    }

    if (fmt->mult != 0)
    {
        save_decoding_error(ERR_AUTO_VALUE_AND_SCALING, 0, 0, fmt->fmt_string);
        return true;
    }

    extract_value_from_message(fmt);

    switch (fmt->fmt_type)
    {
        case PRINT_DOUBLE:
            convert_value32.data_u = (uint32_t)g_msg.value.data_u64;
            g_msg.value.data_double = (double)convert_value32.data_f;
            value_scaling(fmt, g_msg.value.data_double);
            break;

        case PRINT_INT64:
            g_msg.value.data_double = (double)g_msg.value.data_i64;
            value_scaling(fmt, (double)g_msg.value.data_i64);
            break;

        case PRINT_UINT64:
            g_msg.value.data_double = (double)g_msg.value.data_u64;
            value_scaling(fmt, (double)g_msg.value.data_u64);
            break;

        case PRINT_STRING:
            // The pointer to the start of value.data_u64 is already prepared
            break;

        default:
            save_internal_decoding_error(INT_INCORRECT_AUTO_VALUE_TYPE, fmt->fmt_type);
            break;
    }

    return false;   // No error detected
}


/**
 * @brief Converts a 16-bit half-precision float to a standard float.
 *
 * @note: Based on the implementation from:
 *        stackoverflow.com/questions/6162651/half-precision-floating-point-in-java
 *
 * @param  hbits  The half-precision float stored as a uint16_t.
 * @return The converted value as a float.
 */

static float convert_half_float_to_float(uint16_t hbits)
{
    val_convert32_t convert_value32;

    int mant = hbits & 0x03ffu;           // 10 bits mantissa
    int exp = hbits & 0x7c00u;            // 5 bits exponent

    if (exp == 0x7c00u)                   // NaN/Inf
    {
        exp = 0x3fc00u;                   // -> NaN/Inf
    }
    else if (exp != 0)                    // normalized value
    {
        exp += 0x1c000u;                  // exp - 15 + 127

        if ((mant == 0) && (exp > 0x1c400)) // smooth transition
        {
            convert_value32.data_u =
                (((hbits & 0x8000) << 16u) | (exp << 13u) | 0x3ffu);
            return convert_value32.data_f;
        }
    }
    else if (mant != 0)                   // && exp==0 -> subnormal
    {
        exp = 0x1c400u;                   // make it normal

        do
        {
            mant <<= 1u;                  // mantissa * 2
            exp -= 0x400u;                // decrease exp by 1
        }
        while ((mant & 0x400u) == 0);     // while not normal

        mant &= 0x3ffu;                   // discard subnormal bit
    }                                     // else +/-0 -> +/-0

    // combine all parts
    convert_value32.data_u = (
        (hbits & 0x8000u) << 16u          // sign  << ( 31 - 15 )
        | (exp | mant) << 13u);           // value << ( 23 - 10 )
    return convert_value32.data_f;
}


/**
 * @brief Processes float/double type values (16, 32, and 64-bit).
 *
 * @param fmt   Pointer to the structure containing value conversion and print information.
 */

static void process_double_value(value_format_t *fmt)
{
    // Unions for converting integers to float or double
    val_convert32_t convert_value32;
    val_convert64_t convert_value64;

    switch (fmt->data_size)
    {
        case 16:
            extract_value_from_message(fmt);
            g_msg.value.data_double =
                (double)convert_half_float_to_float((uint16_t)g_msg.value.data_u64);
            break;

        case 32:
            extract_value_from_message(fmt);
            convert_value32.data_u = (uint32_t)g_msg.value.data_u64;
            g_msg.value.data_double = (double)convert_value32.data_f;
            break;

        case 64:
            extract_value_from_message(fmt);
            convert_value64.data_u = g_msg.value.data_u64;
            g_msg.value.data_double = convert_value64.data_f;
            break;

        default:
            save_decoding_error(ERR_DECODE_FLOAT_SIZE_MUST_BE_16_32_OR_64, fmt->data_size, 0,
                                fmt->fmt_string);
            return;
    }

    value_scaling(fmt, g_msg.value.data_double);
}


/**
 * @brief Processes memo type values.
 *
 * @param fmt   Pointer to the structure containing value conversion and print information.
 */

static void process_memo(value_format_t *fmt)
{
    if ((fmt->get_memo >= NUMBER_OF_FILTER_BITS) && (fmt->get_memo <= MAX_ENUMS))
    {
        if ((g_msg.enums[fmt->get_memo].name != NULL) && (g_msg.enums[fmt->get_memo].type == MEMO_TYPE))
        {
            g_msg.value.data_double = g_msg.enums[fmt->get_memo].memo_value;
            g_msg.value.data_i64 = (int64_t)g_msg.value.data_double;
            g_msg.value.data_u64 = (uint64_t)g_msg.value.data_double;
        }
        else
        {
            save_internal_decoding_error(INT_GET_MEMO_TYPE_IS_NOT_MEMO, 0);
            return;
        }

        value_scaling(fmt, g_msg.value.data_double);
    }
    else
    {
        save_internal_decoding_error(INT_GET_MEMO_OUT_OF_RANGE, fmt->get_memo);
        return;
    }
}


/**
 * @brief Address and data size (bits) must be divisible by 8 for %W, hex dump and float values
 *
 * @param fmt            Pointer to the structure containing value preparation/conversion and print information.
 * @param divisible_by_8 Indicates if the values must be divisible by 8 (true) or not (false).
 */

static void check_value_bit_address(value_format_t *fmt, bool divisible_by_8)
{
    if (divisible_by_8)
    {
        // Data size must be divisible by 8
        if ((fmt->data_size & 7) != 0)
        {
            save_decoding_error(ERR_DECODE_DATA_SIZE_NOT_DIVISIBLE_BY_8, fmt->data_size, 8, fmt->fmt_string);
            return;
        }

        // String bit address must be divisible by 8
        if ((fmt->bit_address & 7) != 0)
        {
            save_decoding_error(ERR_DECODE_ADDRESS_NOT_DIVISIBLE_BY_8, fmt->bit_address, 8, fmt->fmt_string);
            return;
        }
    }
}


/**
 * @brief Calculates the relative timestamp difference from the previous message with the same value.
 *
 * @param fmt   Pointer to the structure containing value preparation/conversion and print information.
 */

static void prepare_message_time_period(value_format_t *fmt)
{
    if (g_msg.fmt_id >= MAX_FMT_IDS)
    {
        return;
    }

    msg_data_t *p_msg = g_fmt[g_msg.fmt_id];

    if (p_msg == NULL)
    {
        return;
    }

    // Calculate the time difference only if the message has been decoded at least once before
    if (p_msg->counter > 0)
    {
        g_msg.value.data_double = g_msg.timestamp.f - p_msg->time_last_message;
        value_scaling(fmt, g_msg.value.data_double);
    }
}


/**
 * @brief Calculates the time difference between the current time and a specified message.
 *
 * @param fmt   Pointer to the structure containing value preparation/conversion and print information.
 */

static void prepare_time_difference(value_format_t *fmt)
{
    unsigned fmt_timer_start = fmt->fmt_id_timer;

    if (fmt_timer_start >= MAX_FMT_IDS)
    {
        return;
    }

    msg_data_t *p_fmt = g_fmt[fmt_timer_start];

    if (p_fmt == NULL)
    {
        return;
    }

    if (p_fmt->counter > 0)
    {
        double time_diff = g_msg.timestamp.f - p_fmt->time_last_message;
        g_msg.value.data_u64 = (uint64_t)time_diff;
        g_msg.value.data_i64 = (int64_t)time_diff;
        g_msg.value.data_double = time_diff;
        value_scaling(fmt, g_msg.value.data_double);
    }
}


/**
 * @brief Prepares a value for printing by setting up the necessary information
 *        for the 'print_message()' function. The values are stored in the
 *        'g_msg.value' structure. Each data type is prepared, if possible, as a
 *        64-bit integer, 64-bit unsigned integer, double, and string.
 * @note  The 'g_msg.value' is initialized to zero at the start of processing.
 *        If the value cannot be set correctly, it remains zero for printing.
 *
 * @param fmt            Pointer to the structure containing value preparation,
 *                       conversion, and print information.
 * @param divisible_by_8 Indicates whether the value must be divisible by 8
 *                       (true) or must not be divisible by 8 (false).
 */

static void prepare_value(value_format_t *fmt, bool divisible_by_8)
{
    if (fmt->fmt_string == NULL)
    {
        save_internal_decoding_error(INT_FMT_STRING_NULL, 0);
        return;
    }

    check_value_bit_address(fmt, divisible_by_8);

    // Validate data length based on the data type and prepare the data,
    // or report an error if the data or address is invalid.
    switch (fmt->data_type)
    {
        case VALUE_AUTO:
        {
            int rez = process_value_auto(fmt);

            if (rez != 0)
            {
                return;     // Error detected - stop the value processing
            }

            break;
        }

        case VALUE_INT64:           // Signed integers (2 to 64 bits long)
            if (fmt->data_size < 2)
            {
                save_decoding_error(ERR_DECODE_TOO_SMALL_INT_DATA_SIZE, fmt->data_size, 1u, fmt->fmt_string);
                return;
            }

            extract_value_from_message(fmt);
            g_msg.value.data_double = (double)g_msg.value.data_i64;
            value_scaling(fmt, (double)g_msg.value.data_i64);
            break;

        case VALUE_UINT64:          // Unsigned integers (1 to 64 bits long)
            if (fmt->data_size < 1)
            {
                save_decoding_error(ERR_DECODE_TOO_SMALL_UINT_DATA_SIZE, fmt->data_size, 0, fmt->fmt_string);
                return;
            }

            extract_value_from_message(fmt);
            g_msg.value.data_double = (double)g_msg.value.data_u64;
            value_scaling(fmt, (double)g_msg.value.data_u64);
            break;

        case VALUE_DOUBLE:          // Float (32b) or double (64b) values
            process_double_value(fmt);
            break;

        case VALUE_STRING:          // Strings
            // If the length of data is zero then the complete message is printed
            extract_value_from_message(fmt);
            break;

        case VALUE_dTIMESTAMP:      // Difference between the current timestamp and timestamp of the previous value
            prepare_message_time_period(fmt);
            break;

        case VALUE_TIMESTAMP:       // Value of current timestamp
            g_msg.value.data_double = g_msg.timestamp.f;
            value_scaling(fmt, g_msg.value.data_double);
            break;

        case VALUE_MEMO:            // Use the memorized value
            process_memo(fmt);
            break;

        case VALUE_MESSAGE_NO:      // Number of current message
            g_msg.value.data_u64 = g_msg.message_cnt;
            g_msg.value.data_i64 = (int64_t)g_msg.message_cnt;
            g_msg.value.data_double = (double)g_msg.message_cnt;
            break;

        case VALUE_TIME_DIFF:       // Time difference between current time and specified message
            prepare_time_difference(fmt);
            break;

        default:
            save_internal_decoding_error(INT_BAD_DATA_TYPE, fmt->data_type);
            return;
    }

    // Write the value to the memory (if memory is defined for this value)
    rte_enum_t memo = fmt->put_memo;

    if (memo != 0)
    {
        save_to_memo(memo);
    }
}


/**
 * @brief Choose the text from the indexed text buffer to the output buffer.
 *        Copy the last one if the index is over the maximal number of text messages.
 *        The first byte of each message contains length of message (1 .. 255 bytes).
 *        Length of message == 0 => end of indexed text.
 *
 * @param y_text      Pointer to the buffer containing indexed text.
 * @param txt_buffer  Buffer where the selected text will be copied.
 * @param index       Index of the desired text in 'y_text'.
 *
 * @return            Pointer to the copied text in the buffer.
 */

static const char *copy_selected_text(char *y_text, char *txt_buffer, unsigned index)
{
    const char *text = TXT_UNDEFINED_TEXT;
    size_t text_length = 0;
    *txt_buffer = '\0';

    if ((y_text != NULL) && (*y_text != 0))
    {
        if (*y_text != 0)
        {
            text_length = (unsigned char)*y_text;

            for (size_t i = 0; i < index; i++)
            {
                if (y_text[1 + text_length] == 0)   // Already at the last message in the list?
                {
                    break;
                }

                y_text += text_length + 1u;         // Skip the current message
                text_length = (unsigned char)*y_text;
            }
        }

        if (text_length > 0)
        {
            strncpy(txt_buffer, y_text + 1u, text_length);
            txt_buffer[text_length] = '\0';
            text = txt_buffer;
        }
    }

    return text;
}


/**
 * @brief Return pointer to the indexed text (or last text message of the list if out of bounds).
 *        Select the text from a list of text messages. The first byte of every message contains
 *        number of characters in the message - see below:
 *          no_characters1, "non zero terminated string with 'no_characters1' length"
 *              ...
 *          no_charactersN, "non zero terminated string with 'no_charactersN' length"
 *          0 - end of list
 *
 * @param in_file  Index of the structure containing indexed text messages.
 * @param index    Index to select the desired text message.
 *
 * @return         Pointer to the selected text or NULL if no valid text is found.
 */

static const char *get_selected_text(rte_enum_t in_file, unsigned index)
{
    static char txt_buffer[256];

    if (in_file < MAX_ENUMS)
    {
        if ((g_msg.enums[in_file].type == Y_TEXT_TYPE) || (g_msg.enums[in_file].type == IN_FILE_TYPE))
        {
            char *y_text = g_msg.enums[in_file].in_file_txt;

            if (y_text == NULL)
            {
                save_internal_decoding_error(INT_DECODE_Y_TYPE_STRING_NULL, 0);
            }
            else
            {
                copy_selected_text(y_text, txt_buffer, index);
            }
        }
        else
        {
            save_internal_decoding_error(INT_DECODE_Y_TYPE_STRING, g_msg.enums[in_file].type);
        }
    }

    return txt_buffer;
}


/**
 * @brief Logs timestamps to the 'Timestamps.csv' file.
 *        Each timestamp is logged with its corresponding message number.
 */

static void timestamp_logging(void)
{
    static double previous_time = 0;

    if (g_msg.file.timestamps == NULL)       // Check if relative timestamps are disabled
    {
        return;
    }

    // Do not print timestamp for the first message or if a decoding error was reported
    if ((g_msg.messages_processed_after_restart > 0) && (g_msg.msg_error_counter == 0))
    {
        double timestamp_diff = (g_msg.timestamp.f - previous_time) * g_msg.param.time_multiplier;
        print_message_number(g_msg.file.timestamps, g_msg.message_cnt);
        fprintf(g_msg.file.timestamps, ";%8.6f;%g\n",
            g_msg.timestamp.f * g_msg.param.time_multiplier, timestamp_diff);
    }

    previous_time = g_msg.timestamp.f;
}


/**
 * @brief Hex dump the complete message to the output file.
 *
 * @param out   Pointer to the output file.
 * @param fmt   Pointer to the current value formatting parameters.
 */

static void hex_dump_complete_message_to_file(FILE *out, value_format_t *fmt)
{
    unsigned print_as = 1u;         // Default = print as bytes 

    switch (fmt->fmt_type)
    {
        case PRINT_HEX1U:
            print_as = 1u;
            break;

        case PRINT_HEX2U:
            print_as = 2u;
            break;

        case PRINT_HEX4U:
            print_as = 4u;
            break;

        default:
            break;
    }

    unsigned size = g_msg.asm_size;
    unsigned char *address = (unsigned char *)g_msg.assembled_msg;
    unsigned bytes_to_skip = (fmt->bit_address + 7U) / 8U;

    if (size < bytes_to_skip)
    {
        return;     // Nothing to print
    }

    size -= bytes_to_skip;
    address += bytes_to_skip;

    fprintf(out, fmt->fmt_string);
    hex_print_complete_message(out, address, size, print_as);

    if (fmt->print_copy_to_main_log)
    {
        fprintf(g_msg.file.main_log, fmt->fmt_string);
        hex_print_complete_message(g_msg.file.main_log, address, size, print_as);
    }
}


/**
 * @brief Print the time and date of data transfer from the embedded system.
 *
 * @param out   Pointer to the output file.
 * @param fmt   Pointer to the current value formatting parameters.
 */

static void print_date_to_file(FILE *out, value_format_t *fmt)
{
    fprintf(out, "%s%s", fmt->fmt_string, g_msg.date_string);

    if (fmt->print_copy_to_main_log)
    {
        fprintf(g_msg.file.main_log, "%s%s", fmt->fmt_string, g_msg.date_string);
    }
}


/**
 * @brief Write complete (or partial) binary message to the output file.
 *
 * @param out   Pointer to the output file.
 * @param fmt   Pointer to the current value formatting parameters.
 */

static void write_binary_message_data_to_file(FILE *out, value_format_t *fmt)
{
    if (fmt->data_size == 0)    // Write the complete message
    {
        fprintf(out, fmt->fmt_string);
        fwrite((unsigned char *)g_msg.assembled_msg, 1, g_msg.asm_size, out);

        if (fmt->print_copy_to_main_log)
        {
            fprintf(g_msg.file.main_log, fmt->fmt_string);
            fwrite((unsigned char *)g_msg.assembled_msg, 1, g_msg.asm_size, g_msg.file.main_log);
        }
    }
    else
    {
        // Write just the specified part of the message (length up to 64 bits).
        if ((fmt->data_size & 7) != 0)
        {
            save_decoding_error(ERR_DECODE_DATA_SIZE_NOT_DIVISIBLE_BY_8,
                                fmt->data_size, 8, fmt->fmt_string);
            return;
        }

        prepare_value(fmt, true);
        fprintf(out, fmt->fmt_string);
        fwrite((const char *)&g_msg.value.data_u64, 1, fmt->data_size / 8u, out);

        if (fmt->print_copy_to_main_log)
        {
            fprintf(g_msg.file.main_log, fmt->fmt_string);
            fwrite((const char *)&g_msg.value.data_u64, 1, fmt->data_size / 8u, g_msg.file.main_log);
        }
    }
}


/**
 * @brief Calculate total data size for a particular message (including the FMT words).
 *
 * @param p_fmt   Pointer to the current message parameters.
 */

static void calculate_total_message_size(msg_data_t *p_fmt)
{
    uint32_t total_words = g_msg.asm_words;      // Number of data words found for this message
    uint32_t remainder = total_words & 3uL;
    total_words = (total_words / 4uL) * 5uL;

    if (remainder != 0)
    {
        total_words += remainder + 1uL;         // Add for the FMT word
    }

    if (total_words == 0)                       // RTE_MSG0 or RTE_EXT_MSG0?
    {
        total_words = 1u;
    }

    p_fmt->total_data_received += total_words;
}


/**
 * @brief Prints the time difference between the current and previous message timestamps for the same message.
 *
 * @param out    Pointer to the output file.
 * @param p_fmt  Pointer to the current message parameters.
 * @param fmt    Pointer to the current value parameters.
 */

static void print_dTimeStamp_to_file(FILE *out, msg_data_t *p_fmt, value_format_t *fmt)
{
    double value = 0;

    if (p_fmt->counter > 0)
    {
        value = g_msg.timestamp.f - p_fmt->time_last_message;
    }

    fprintf(out, fmt->fmt_string);
    print_timestamp(out, value);

    if (fmt->print_copy_to_main_log)
    {
        fprintf(g_msg.file.main_log, fmt->fmt_string);
        print_timestamp(g_msg.file.main_log, value);
    }

    g_msg.value.data_double = value;

    // Save the value to memory if a memo is defined for this value.
    rte_enum_t memo = fmt->put_memo;

    if (memo != 0)
    {
        save_to_memo(memo);
    }
}


/**
 * @brief Prints the current message format ID name.
 *
 * @param out   Pointer to the output file.
 * @param fmt   Pointer to the current value parameters.
 */

static void print_current_message_name(FILE *out, value_format_t *fmt)
{
    fprintf(out, fmt->fmt_string);
    fprintf(out, get_format_id_name(g_msg.fmt_id));

    if (fmt->print_copy_to_main_log)
    {
        fprintf(g_msg.file.main_log, fmt->fmt_string);
        // The message name is not printed to Main.log again as it was already printed.
    }
}


/**
 * @brief Prints the current message number to the specified file.
 *
 * @param out   Pointer to the output file.
 * @param fmt   Pointer to the current value parameters.
 */

static void print_current_message_number(FILE *out, value_format_t *fmt)
{
    fprintf(out, fmt->fmt_string);
    print_message_number(out, g_msg.message_cnt);

    if (fmt->print_copy_to_main_log)
    {
        fprintf(g_msg.file.main_log, fmt->fmt_string);
        // The message number is not printed to Main.log again as it was already printed.
    }

    // Save the value to memory if a memo is defined for this value.
    rte_enum_t memo = fmt->put_memo;

    if (memo != 0)
    {
        save_to_memo(memo);
    }
}


/**
 * @brief Prints the current message's timestamp to the file.
 *
 * @param out   Pointer to the output file.
 * @param fmt   Pointer to the current value parameters.
 */

static void print_timestamp_to_file(FILE *out, value_format_t *fmt)
{
    fprintf(out, fmt->fmt_string);
    print_timestamp(out, g_msg.timestamp.f);
    g_msg.value.data_double = g_msg.timestamp.f;

    if (fmt->print_copy_to_main_log)
    {
        fprintf(g_msg.file.main_log, fmt->fmt_string);
        // The timestamp is not printed to Main.log again as it was already printed.
    }

    // Save the value to memory if a memo is defined for this value.
    rte_enum_t memo = fmt->put_memo;

    if (memo != 0)
    {
        save_to_memo(memo);
    }
}


/**
 * @brief Prints the selected text ('%Y' printf type) to the specified file.
 *
 * @param out   Pointer to the output file.
 * @param fmt   Pointer to the current value parameters.
 */

static void print_selected_text(FILE *out, value_format_t *fmt)
{
    prepare_value(fmt, 0);
    fprintf(out, "%s", fmt->fmt_string);

    // Retrieve the text from a list of text messages
    const char *text = get_selected_text(fmt->in_file, (unsigned)(g_msg.value.data_u64));
    // The first byte contains the string length information

    if (text != NULL)
    {
        fprintf(out, "%s", text);
    }

    if (fmt->print_copy_to_main_log)
    {
        fprintf(g_msg.file.main_log, "%s", fmt->fmt_string);

        if (text != NULL)
        {
            fprintf(g_msg.file.main_log, "%s", text);
        }
    }
}


/**
 * @brief Prints the text from the current message to the specified file.
 *
 * @param out   Pointer to the output file.
 * @param fmt   Pointer to the current value parameters.
 */

static void print_message_as_string_to_file(FILE *out, value_format_t *fmt)
{
    if (fmt->data_size == 0)    // Print the entire message?
    {
        fprintf(out, fmt->fmt_string, g_msg.assembled_msg);
    }
    else
    {
        prepare_value(fmt, true);
        fprintf(out, fmt->fmt_string, (const char *)&g_msg.value.data_u64);
    }

    if (fmt->print_copy_to_main_log)
    {
        if (fmt->data_size == 0)    // Print the entire message?
        {
            fprintf(g_msg.file.main_log, fmt->fmt_string, g_msg.assembled_msg);
        }
        else
        {
            fprintf(g_msg.file.main_log, fmt->fmt_string, (const char *)&g_msg.value.data_u64);
        }
    }
}


/**
 * @brief Prints binary data from the current message to the specified file.
 *
 * @param out   Pointer to the output file.
 * @param fmt   Pointer to the current value parameters.
 */

static void print_binary_value_to_file(FILE *out, value_format_t *fmt)
{
    prepare_value(fmt, false);
    fprintf(out, fmt->fmt_string);
    g_msg.value.data_double = (double)g_msg.value.data_u64;

    if (fmt->data_type == VALUE_UINT64)
    {
        print_binary64(out, g_msg.value.data_u64, fmt->data_size, fmt->data_size);

        if (fmt->print_copy_to_main_log)
        {
            fprintf(g_msg.file.main_log, fmt->fmt_string);
            print_binary64(g_msg.file.main_log, g_msg.value.data_u64, fmt->data_size, fmt->data_size);
        }
    }
    else
    {
        save_decoding_error(ERR_PRINT_BIN_VALUE_TYPE, fmt->data_type, 0, fmt->fmt_string);
    }
}


/**
 * @brief Checks if statistics can be generated for the current value.
 *
 * @param value_type   Type of the current value.
 * @return true if statistics are possible, false otherwise.
 */

static bool statistics_possible_for_the_value(enum fmt_type_t value_type)
{
    // Verify if the message is of a valid type
    if ((value_type == PRINT_UINT64)
        || (value_type == PRINT_BINARY)
        || (value_type == PRINT_INT64)
        || (value_type == PRINT_DOUBLE)
        || (value_type == PRINT_TIMESTAMP)
        || (value_type == PRINT_dTIMESTAMP))
    {
        return true;
    }
    else
    {
        return false;
    }
}


/**
 * @brief Executes statistics for the current value if enabled.
 *
 * @param p_fmt  Pointer to the current message parameters.
 * @param fmt    Pointer to the current value parameters.
 */

static void process_statistics_for_the_current_value(msg_data_t *p_fmt, value_format_t *fmt)
{
    // Verify if statistics are enabled and applicable for the current value.
    if ((fmt->value_stat != NULL) && (g_msg.param.value_statistics_enabled))
    {
        // For timers, check if previous data is available (previous message exists)
        if (fmt->data_type == VALUE_dTIMESTAMP)
        {
            if (p_fmt->counter == 0)     // No previous message for this message type
            {
                return;
            }
        }
        else if (fmt->data_type == VALUE_TIME_DIFF)
        {
            uint32_t code = fmt->fmt_id_timer;

            if (code < MAX_FMT_IDS)
            {
                if ((g_fmt[code] != NULL) && (g_fmt[code]->counter == 0))
                {
                    return;
                }
            }
        }

        // Execute statistics if the value type supports it.
        if (statistics_possible_for_the_value(fmt->fmt_type))
        {
            value_statistic(p_fmt, fmt);
        }
    }
}


/**
 * @brief Retrieves formatting definitions for the specified format ID.
 *
 * @param current_fmt_id  Format ID of the message to be printed.
 * @return Pointer to the structure with formatting definitions, or NULL if invalid.
 */

static msg_data_t *check_and_get_print_info(uint32_t current_fmt_id)
{
    // Ensure output files are available.
    if ((g_msg.file.main_log == NULL) || (g_msg.file.error_log == NULL))
    {
        report_fatal_error_and_exit(FATAL_INT_ERR_NO_OUT_FILES,
            (g_msg.file.main_log == NULL) ? "1" : "0",
            g_msg.file.error_log == NULL);
    }

    // Validate format ID range.
    if (current_fmt_id >= MAX_FMT_IDS)
    {
        fprintf(g_msg.file.main_log, "???");
        save_internal_decoding_error(INT_FMT_ID_OUT_OF_RANGE, current_fmt_id);
        return NULL;
    }

    msg_data_t *p_fmt = g_fmt[current_fmt_id];

    // Ensure format definition exists.
    if ((p_fmt == NULL) || (p_fmt->format == NULL))
    {
        report_problem(ERR_MESSAGE_MUST_CONTAIN_ONE_FMT_DEFINITION, 0);
        return NULL;
    }

    return p_fmt;
}


/**
 * @brief Return the pointer to FILE to which the data will be printed
 *        Report an error if an incorrect index was found
 *
 * @param fmt   Pointer to the current formatting structure
 *
 * @return  Pointer to the file structure
 */

static FILE *get_out_file(value_format_t *fmt)
{
    unsigned out_file = fmt->out_file;
    FILE *out = g_msg.file.main_log;    // Default output file = Main.log

    // Validate output file index.
    if ((out_file >= NUMBER_OF_FILTER_BITS) && (out_file < MAX_ENUMS))
    {
        if (g_msg.enums[out_file].type == OUT_FILE_TYPE)
        {
            if (g_msg.enums[out_file].p_file != NULL)
            {
                out = g_msg.enums[out_file].p_file;
            }
            else
            {
                save_internal_decoding_error(INT_OUT_FILE_PTR_NULL, 0);
            }
        }
        else
        {
            save_internal_decoding_error(INT_BAD_OUT_FILE_TYPE, fmt->out_file);
        }
    }
    else if (out_file != 0)     // 0 = default (print to Main.log)
    {
        save_internal_decoding_error(INT_OUT_FILE_INDEX_OUT_OF_RANGE, fmt->out_file);
    }

    return out;
}


/**
 * @brief Report an error if extended data is found 
 *        There should be no extended information if the message type is MSGN or MSGX.
 *
 * @param msg_type  The type of the current message.
 */

static void check_extended_data(enum msg_type_t msg_type)
{
    if ((msg_type == TYPE_MSGN) || (msg_type == TYPE_MSGX))
    {
        if (g_msg.additional_data != 0)
        {
            report_problem(ERR_UNWANTED_EXTENDED_DATA, g_msg.additional_data);
            g_msg.additional_data = 0;
        }
    }
}


/**
 * @brief Prints an unsigned integer value to the specified file.
 *
 * @param out   Pointer to the output file.
 * @param fmt   Pointer to the current value parameters.
 */

static void print_uint(FILE *out, value_format_t *fmt)
{
    prepare_value(fmt, false);
    fprintf(out, fmt->fmt_string, g_msg.value.data_u64);

    if (fmt->print_copy_to_main_log)
    {
        fprintf(g_msg.file.main_log, fmt->fmt_string, g_msg.value.data_u64);
    }
}


/**
 * @brief Prints a signed integer value to the specified file.
 *
 * @param out   Pointer to the output file.
 * @param fmt   Pointer to the current value parameters.
 */

static void print_int(FILE *out, value_format_t *fmt)
{
    prepare_value(fmt, false);
    fprintf(out, fmt->fmt_string, g_msg.value.data_i64);

    if (fmt->print_copy_to_main_log)
    {
        fprintf(g_msg.file.main_log, fmt->fmt_string, g_msg.value.data_i64);
    }
}


/**
 * @brief Prints a double value to the specified file.
 *
 * @param out   Pointer to the output file.
 * @param fmt   Pointer to the current value parameters.
 */

static void print_double(FILE *out, value_format_t *fmt)
{
    prepare_value(fmt, false);
    fprintf(out, fmt->fmt_string, g_msg.value.data_double);

    if (fmt->print_copy_to_main_log)
    {
        fprintf(g_msg.file.main_log, fmt->fmt_string, g_msg.value.data_double);
    }
}


/**
 * @brief Prints plain text to the specified file.
 *
 * @param out   Pointer to the output file.
 * @param fmt   Pointer to the current value parameters.
 */

static void print_plain_text(FILE *out, value_format_t *fmt)
{
    fprintf(out, fmt->fmt_string);

    if (fmt->print_copy_to_main_log)
    {
        fprintf(g_msg.file.main_log, fmt->fmt_string);
    }
}


/**
 * @brief Processes a single format structure and prints the corresponding value.
 *
 * @param out    Pointer to the output file.
 * @param p_fmt  Pointer to the current message parameters.
 * @param fmt    Pointer to the current value parameters.
 */

static void print_single_value(FILE *out, msg_data_t *p_fmt, value_format_t *fmt)
{
    // Determine the appropriate print function based on the format type
    switch (fmt->fmt_type)
    {
        case PRINT_PLAIN_TEXT:      // No format specifier in the string
            print_plain_text(out, fmt);
            break;

        case PRINT_STRING:          // "%s"
            print_message_as_string_to_file(out, fmt);
            break;

        case PRINT_SELECTED_TEXT:   // "%Y"
            print_selected_text(out, fmt);
            break;

        case PRINT_UINT64:          // "%c", "%u", "%x", "%o", "%X", "%lu", "%lx", "%lX", "%lo", etc.
            print_uint(out, fmt);
            break;

        case PRINT_INT64:           // "%d", "%i", "%ld", "%li", etc.
            print_int(out, fmt);
            break;

        case PRINT_DOUBLE:          // "%f", "%F", "%e", "%E", "%g", "%G", "%a", "%A"
            print_double(out, fmt);
            break;

        case PRINT_BINARY:          // "%b", "%B"
            print_binary_value_to_file(out, fmt);
            break;

        case PRINT_TIMESTAMP:       // "%t"
            print_timestamp_to_file(out, fmt);
            break;

        case PRINT_dTIMESTAMP:      // "%T"
            print_dTimeStamp_to_file(out, p_fmt, fmt);
            break;

        case PRINT_MSG_NO:          // "%N"
            print_current_message_number(out, fmt);
            break;

        case PRINT_MSG_FMT_ID_NAME: // "%M"
            print_current_message_name(out, fmt);
            break;

        case PRINT_HEX1U:           // "%1H"
        case PRINT_HEX2U:           // "%2H"
        case PRINT_HEX4U:           // "%4H"
            hex_dump_complete_message_to_file(out, fmt);
            break;

        case PRINT_BIN_TO_FILE:     // "%W"
            write_binary_message_data_to_file(out, fmt);
            break;

        case PRINT_DATE:            // "%D"
            print_date_to_file(out, fmt);
            break;

        default:
            // Unknown format type
            save_internal_decoding_error(INT_DECODE_INTERNAL_UNKNOWN_TYPE, fmt->fmt_type);
            break;
    }
}


/**
 * @brief Prints a message based on the format definition file(s).
 *        Ensure the message length matches the definition before invoking this function.
 */

void print_message(void)
{
    g_msg.error_value_no = 0;               // Counter of processed values for the same message
    g_msg.msg_error_counter = 0;
    msg_data_t *p_fmt = check_and_get_print_info(g_msg.fmt_id);

    if (p_fmt == NULL)
    {
        return;                             // Exit if no format information is found
    }

    check_extended_data(p_fmt->msg_type);

    // Print the message information for the Main.log file (mandatory data)
    fprintf(g_msg.file.main_log, "\n");

    // Flag the message number with an asterisk if the timestamp is flagged as suspicious
    if (g_msg.timestamp.mark_problematic_tstamps)
    {
        fprintf(g_msg.file.main_log, "#");
        g_msg.timestamp.mark_problematic_tstamps = false;
        g_msg.timestamp.suspicious_timestamp++;
    }

    print_message_number(g_msg.file.main_log, g_msg.message_cnt);
    fprintf(g_msg.file.main_log, " ");
    print_timestamp(g_msg.file.main_log, g_msg.timestamp.f);
    fprintf(g_msg.file.main_log, " %s: ", get_format_id_name(g_msg.fmt_id));

    timestamp_logging();
    g_msg.messages_processed_after_restart++;
    value_format_t *fmt = p_fmt->format;

    while (fmt != NULL)
    {
        // Reset the value structure to ensure no residual data is present
        memset(&g_msg.value, 0, sizeof(g_msg.value));
        FILE *out = get_out_file(fmt);       // The file to which the data will be printed

        if (fmt->fmt_type != PRINT_PLAIN_TEXT)
        {
            g_msg.error_value_no++;
        }

        print_single_value(out, p_fmt, fmt);
        process_statistics_for_the_current_value(p_fmt, fmt);
        fmt = fmt->format;      // Continue processing the next formatting definition
    }

    print_decoding_errors();    // Print error information detected during decoding (if any)

    if (g_msg.msg_error_counter > 0)
    {
        g_msg.timestamp.no_previous_tstamp = true;      // Restart log-timestamp search after an error is detected
    }

    p_fmt->counter++;        // Increment message counter
    calculate_total_message_size(p_fmt);

    p_fmt->time_last_message = g_msg.timestamp.f;       // Store the timestamp of the current message
}

/*==== End of file ====*/
