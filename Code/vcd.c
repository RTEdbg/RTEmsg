/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */


/*******************************************************************************
 * @file    vcd.c
 * @author  B. Premzel
 * @brief   Support for the generation of Value change dump (VCD) output files.
 *          This data is used to analyze the timing of the instrumented firmware.
 * 
 * Various documents:
 *    https://en.wikipedia.org/wiki/Value_change_dump
 *    https://www.mathworks.com/help/hdlverifier/ref/tovcdfile.html
 *    https://www.chipverify.com/verilog/verilog-vcd
 *    https://zipcpu.com/blog/2017/07/31/vcd.html   (Writing your own VCD File)
 *    https://web.archive.org/web/20120323132708/http://www.beyondttl.com/vcd.php
 ******************************************************************************/

#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <locale.h>
#include <windows.h>
#include "vcd.h"
#include "errors.h"
#include "files.h"
#include "parse_error_reporting.h"
#include "print_helper.h"


/**
 * @brief A working data structure for processing data that is written to VCD files.
 */

typedef struct _vcd
{
    // Temporary buffer for sprintf before copying to "string".
    char temp_string[128];

    // Working variables for data processing for $var Name and Value fields.
    char string[VCD_MAX_ASSEMBLED_STRING_LEN + 1];
    unsigned index;                     // Index to the 'string' and its length
    unsigned start_of_name;             // Index of first Name character
    unsigned end_of_name;               // Index of last Name character
    unsigned start_of_value;            // Index of first Value character (this is the terminating '\0')
    unsigned end_of_value;              // Index of last Value character (terminating '\0')
    bool empty_string_added_to_name;    // True if the string added from vcd_print_string() had zero length.
} vcd_t;

static vcd_t vcd;


/**
 * @brief  Reset the VCD variable structure.
 *         Structure contains working variables used for the generation of VCP $var.
 *         Function is called before processing of every VCD variable.
 */

void vcd_reset_structure(void)
{
    memset(&vcd, 0, sizeof(vcd));
}


/**
 * @brief  Check if the VCD variable format is correct.
 *         The variable definition must start with one of the supported
 *         data type characters: B, W, F, S, contain one equal sign (=) and
 *         have at least one alphanumeric character Name and Variable fields.
 * 
 * @note   Function is called during format file parsing only.
 * 
 * @param  variable_text Pointer to the variable format text.
 */

void vcd_check_variable_format(parse_handle_t* parse_handle, char* variable_text)
{
    char* text = variable_text;
    int type = toupper(*text);
    char* variable_start = NULL;

    switch (type)
    {
        case 'B':   // Bit type
        case 'F':   // Float type
        case 'S':   // String type
        case 'A':   // Analog type
            break;

        default:
            catch_parsing_error(parse_handle, ERR_PARSE_VCD_VAR_TYPE_NOT_OK, variable_text);
    }

    unsigned equal_signs_found = 0;
    unsigned name_length = 0;
    unsigned value_length = 0;
    text++;                     // Skip the variable type

    if (*text != ' ')
    {
        catch_parsing_error(parse_handle,
            ERR_MANDATORY_SPACE_AFTER_VAR_TYPE, variable_text);
    }

    text++;                     // Skip the space after the variable type

    while (*text != '\0')
    {
        if (*text == '=')
        {
            equal_signs_found++;
        }
        else if (*text != ' ')
        {
            if (equal_signs_found)
            {
                value_length++;
                variable_start = text;
            }
            else
            {
                name_length++;
            }
        }

        if (*text < ' ')
        {
            catch_parsing_error(parse_handle,
                ERR_PARSE_UTF8_NOT_ALLOWED, variable_text);
        }

        text++;
    }

    if (equal_signs_found != 1)
    {
        catch_parsing_error(parse_handle,
            ERR_PARSE_VCD_EQUAL_SIGN_PROBLEM, variable_text);
    }

    if ((name_length < 1) || (value_length < 1))
    {
        catch_parsing_error(parse_handle,
            ERR_PARSE_VCD_VAR_DEFINITION_TOO_SHORT, variable_text);
    }

    int special_char = toupper(*variable_start);

    // Check for Toggle, Pulse and Reset special characters
    if (value_length == 1)
    {
        switch (type)
        {
            case 'B':
                if (strchr("TPRXZ01", special_char) == NULL)
                {
                    catch_parsing_error(parse_handle, ERR_PARSE_WRONG_SPECIAL_CHARACTER, variable_text);
                }
                break;

            case 'F':
            case 'A':
                if (strchr("R0123456789", special_char) == NULL)
                {
                    catch_parsing_error(parse_handle, ERR_PARSE_WRONG_SPECIAL_CHARACTER, variable_text);
                }
                break;

            default:
                // Allow any value for the string type
                break;
        }
    }
}


/**
 * @brief  Check the VCD variable type.
 *         Function is called during message decoding only.
 *
 * @return vcd_type_t  Type of VCD variable found (VCD_TYPE_...).
 *         Returns VCD_TYPE_NONE if an error is found.
 */

static vcd_type_t vcd_check_variable_type(void)
{
    vcd_type_t vcd_type;

    // The $var print definition must start with VCD value type B, W, F or S
    switch (toupper(vcd.string[0]))
    {
        case 'B':
            vcd_type = VCD_TYPE_BIT;
            break;

        case 'F':
            vcd_type = VCD_TYPE_FLOAT;
            break;

        case 'S':
            vcd_type = VCD_TYPE_STRING;
            break;

        case 'A':
            vcd_type = VCD_TYPE_ANALOG;
            break;

        default:
            save_decoding_error(ERR_VCD_VALUE_TYPE_CHAR_NOT_FOUND, 0, 0, vcd.string);
            vcd_type = VCD_TYPE_NONE;
            break;
    }

    return vcd_type;
}


/**
 * @brief  Check and correct the VCD variable data.
           Find the positions and size of Name and Value fields.
 *         Function is called during message decoding only.
 * 
 * @param  vcd_type   Type of VCD variable (VCD_TYPE_...).
 * 
 * @return  Returns true if data is ok and false otherwise.
 */

static bool vcd_variable_data_correct(vcd_type_t vcd_type)
{
    bool name_has_alphabetic_char = false;
    bool equal_signs_found = false;

    if (vcd.index < 4)
    {
        save_decoding_error(ERR_VCD_NAME_TOO_SHORT, 0, 0, &vcd.string[1]);
        return false;
    }

    // Remove leading space(s) before Name
    for (vcd.start_of_name = 1; vcd.start_of_name < vcd.index; vcd.start_of_name++)
    {
        if (vcd.string[vcd.start_of_name] != ' ')
        {
            break;
        }
    }

    // Equal sign defines end of Name and start of Value
    for (vcd.end_of_name = vcd.start_of_name; vcd.end_of_name < vcd.index; vcd.end_of_name++)
    {
        if (isalpha(vcd.string[vcd.end_of_name]))
        {
            name_has_alphabetic_char = true;
        }

        if (vcd.string[vcd.end_of_name] == '=')
        {
            equal_signs_found++;
            vcd.start_of_value = vcd.end_of_name + 1;
            break;
        }
    }

    // Remove trailing space(s) after Name
    while (vcd.end_of_name > vcd.start_of_name)
    {
        if (vcd.string[vcd.end_of_name - 1] != ' ')
        {
            break;
        }

        vcd.end_of_name--;
    }

    // Remove leading space(s) before Value
    for (; vcd.start_of_value < vcd.index; vcd.start_of_value++)
    {
        if (vcd.string[vcd.start_of_value] != ' ')
        {
            break;
        }
    }

    vcd.end_of_value = vcd.index;

    // Remove trailing space(s) after Value
    while (vcd.end_of_value > vcd.start_of_value)
    {
        if (vcd.string[vcd.end_of_value - 1] != ' ')
        {
            break;
        }

        vcd.end_of_value--;
    }

    // Convert non-alphanumeric characters to underscores in Name
    for (unsigned i = vcd.start_of_name; i < vcd.end_of_name; i++)
    {
        if (!isalnum(vcd.string[i]))
        {
            vcd.string[i] = '_';
        }
    }

    // Convert non-printable ASCII and UTF characters to underscores in Value (only for the string type)
    for (unsigned i = vcd.start_of_value; i < vcd.end_of_value; i++)
    {
        if ((vcd_type == VCD_TYPE_STRING) && (vcd.string[i] <= ' '))
        {
            vcd.string[i] = '_';
        }
    }

    /* Only verify the presence of the first equals sign (=) to identify the assignment.
     * Do not check for subsequent occurrences, as they may be valid characters within a
     * string value. */
    if (equal_signs_found == 0)
    {
        save_decoding_error(ERR_VCD_EQUAL_SIGN_NOT_FOUND, 0, 0, vcd.string);
        return false;
    }

    // Check if the name and value fields contain at least one character
    size_t name_len = vcd.end_of_name - vcd.start_of_name;
    size_t value_len = vcd.end_of_value - vcd.start_of_value;

    if ((name_len < 1) || (!name_has_alphabetic_char))
    {
        save_decoding_error(ERR_VCD_NAME_TOO_SHORT, 0, 0, &vcd.string[vcd.start_of_name]);
        return false;
    }

    if (value_len < 1)
    {
        save_decoding_error(ERR_VCD_VARIABLE_TOO_SHORT, 0, 0, &vcd.string[vcd.start_of_value]);
        return false;
    }

    vcd.string[vcd.end_of_name] = '\0';     // Terminate the Name
    vcd.string[vcd.end_of_value] = '\0';    //  and the Value

    return true;
}


/**
 * @brief Prints the current message's timestamp to the VCD file.
 *
 * @param out   Pointer to the output file.
 * @param fmt   Pointer to the current value parameters.
 */

static void vcd_add_timestamp(FILE* out_file, vcd_file_data_t* p_vcd_data)
{
    uint64_t timestamp_ns;

    if (g_msg.timestamp.f > (double)UINT64_MAX / 1e9)
    {
        // Limit timestamp to UINT64_MAX ns
        timestamp_ns = UINT64_MAX;
    }
    else
    {
        timestamp_ns = (uint64_t)(g_msg.timestamp.f * 1e9);
    }

    if ((p_vcd_data == NULL) || (out_file == NULL))
    {
        return;
    }

    if (p_vcd_data->writing_disabled)
    {
        return;         // Writing disabled due to timestamp errors
    }

    if (g_msg.message_cnt == p_vcd_data->msg_no_of_last_timestamp)
    {
        return;         // Timestamp already added for this message
    }

    if (!g_msg.timestamp.first_timestamp_processed)
    {
        g_msg.timestamp.first_timestamp_processed = true;
        g_msg.timestamp.first_timestamp_ns = timestamp_ns;
    }

    if (timestamp_ns <= p_vcd_data->last_timestamp_ns)
    {
        // Timestamp error - timestamps must be increasing
        p_vcd_data->timestamp_error_found = true;
        p_vcd_data->consecutive_timestamp_errors++;
        if (p_vcd_data->consecutive_timestamp_errors >= VCD_MAX_CONSECUTIVE_TIMESTAMP_ERRORS)
        {
            p_vcd_data->writing_disabled = true;
            save_decoding_error(ERR_VCD_TOO_MANY_CONSECUTIVE_TIMESTAMP_ERRORS,
                p_vcd_data->consecutive_timestamp_errors, 0, vcd.string);
        }

        p_vcd_data->last_timestamp_ns++;    // Increment last timestamp to maintain monotonicity
    }
    else
    {
        p_vcd_data->last_timestamp_ns = timestamp_ns;
        p_vcd_data->timestamp_error_found = false;
        p_vcd_data->consecutive_timestamp_errors = 0;
    }

    g_msg.timestamp.last_timestamp_ns = timestamp_ns;

    // Write the timestamp to the VCD file
    fprintf(out_file, "#%llu\n", p_vcd_data->last_timestamp_ns);
    p_vcd_data->msg_no_of_last_timestamp = g_msg.message_cnt;
}


/**
 * @brief Generate VCD variable identificator string
 *
 * @note  The VCD standard (IEEE 1364) specifies that the identifier may be
 *        composed of one or more printable ASCII characters from '!' to '~'.
 *
 * @param id - numerical ID value (0 = first variable in the list)
 *
 * @return pointer to ID string
 */

#define FIRST_ID_CHAR   '!'
#define LAST_ID_CHAR    '~'
#define NO_ID_CHARS ((LAST_ID_CHAR - FIRST_ID_CHAR) + 1)

static const char* id_string(size_t id)
{
    static char id_text[16];
    char* text = id_text;

    do
    {
        *text = (id % NO_ID_CHARS) + FIRST_ID_CHAR;
        id /= NO_ID_CHARS;
        text++;
    }
    while (id > 0);

    *text = '\0';
    id_text[VCD_MAX_ID_LENGTH - 1] = '\0';

    return id_text;
}


/**
 * @brief  Process special VCD variable values: Toggle, Pulse and Reset.
 * 
 * Value == "P" for a bit type variable, generate a pulse (1 ns wide).
 * Value == "T" for a bit type variable, toggle the value.
 * Value == "R" for a bit or float type variable, reset the value to zero.
 * 
 * @param  p_vcd_data Pointer to VCD file data structure.
 * @param  vcd_type   Type of VCD variable found (VCD_TYPE_...).
 * @param  value_text Pointer to the Value field of the VCD variable.
 * @param  id_text    Pointer to the ID field of the VCD variable.
 * @param var_index   Number of variable to be processed
 * 
 * @return  Returns true if data should be saved to the VCD file and false otherwise.
 */

static bool vcd_process_special_values(vcd_file_data_t* p_vcd_data, vcd_type_t vcd_type,
    char* value_text, const char* id_text, size_t var_index)
{
    if (var_index >= VCD_MAX_VARIABLES_PER_FILE)
    {
        return false;
    }

    if (strlen(value_text) != 1)
    {
        p_vcd_data->previous_bit_value[var_index] = 1;  // Value processed - should be written to the VCD file
        return true;
    }

    if ((vcd_type != VCD_TYPE_BIT) && (vcd_type != VCD_TYPE_FLOAT))
    {
        p_vcd_data->previous_bit_value[var_index] = 1;  // Value processed - should be written to the VCD file
        return true;    // Special values only for bit and float variable types
    }

    switch (toupper(*value_text))
    {
        case '0':
        case '1':
            p_vcd_data->previous_bit_value[var_index] = *value_text;
            break;

        case 'R':       // Reset value to zero
            if ((vcd_type == VCD_TYPE_BIT) || (vcd_type == VCD_TYPE_FLOAT))
            {
                if (p_vcd_data->previous_bit_value[var_index] == 0)
                {
                    return false;   // Do not reset to zero if no data has been processed yet
                }

                if (p_vcd_data->previous_bit_value[var_index] == '0')
                {
                    return false;   // Already at zero
                }

                value_text[0] = '0';
                p_vcd_data->previous_bit_value[var_index] = value_text[0];
            }
            break;

        case 'T':       // Toggle value
            if (vcd_type == VCD_TYPE_BIT)
            {
                if (p_vcd_data->previous_bit_value[var_index] != '1')
                {
                    value_text[0] = '1';
                }
                else
                {
                    value_text[0] = '0';
                }

                p_vcd_data->previous_bit_value[var_index] = value_text[0];
            }
            break;

        case 'P':       // Pulse generation request
            if (p_vcd_data->pulse_variable_id[0] != '\0')
            {
                // Pulse variable already defined for this message
                save_decoding_error(ERR_VCD_PULSE_VARIABLE_ALREADY_DEFINED, 0, 0, " ");
                return false;
            }

            // Save the last variable ID and type for pulse generation later
            strncpy(p_vcd_data->pulse_variable_id, id_text, VCD_MAX_ID_LENGTH);
            value_text[0] = '1';
            p_vcd_data->previous_bit_value[var_index] = '0';    // Will return to zero after the 1 ns pulse
            break;

        default:
            p_vcd_data->previous_bit_value[var_index] = 1;      // Previous value processed
            break;
    }

    return true;
}


/**
 * @brief  Data for VCD variable types: name and size in bits.
 */
static const struct
{
    const char* id_char;            // Character that identifies value type
    const char* name;               // $var name of type
    const unsigned size;            // Value size in bits
}
vcd_type_data[VCD_TYPE_LAST] = 
{
    {"", "", 0},
    {"", "wire", 1},                // Single bit values.
    {"r", "real", 64},              // Integers and floating point values with up to 64 bits.
    {"s", "string", VCD_STRING_VALUE_MAX_LEN * 8},  // Max. string length in bits
             // Many VCD viewers don't strictly limit string length to the declared bit width
    {"r", "real", 64}               // Analog values treated as real numbers
};


/**
 * @brief  Print the Value to VCD file and save it's name to the list of variables (if not there already).
 *         Do the special Value processing if required.
 * 
 * @param  file_idx   Index of output file.
 * @param  vcd_type   Type of VCD variable found (VCD_TYPE_...).
 * @param  name       Pointer to the Name field of the VCD variable.
 * @param  value_text Pointer to the Value field of the VCD variable.
 */

static void vcd_save_variable(rte_enum_t file_idx, vcd_type_t vcd_type, const char* name, char* value_text)
{
    if ((vcd_type == VCD_TYPE_NONE) || (vcd_type >= VCD_TYPE_LAST))
    {
        return;
    }

    // Check if a string printf had an empty value
    // If part of the name is a string, then do not use such an (incomplete) name.
    if (vcd.empty_string_added_to_name)
    {
        return;
    }

    if (strlen(value_text) == 0)
    {
        return;
    }

    if ((file_idx >= g_msg.enums_found) || (file_idx < NUMBER_OF_FILTER_BITS))
    {
        return;
    }

    if (g_msg.enums[file_idx].type != OUT_FILE_TYPE)
    {
        return;
    }

    FILE* out_file = g_msg.enums[file_idx].p_file;
    vcd_file_data_t* p_vcd_data = g_msg.enums[file_idx].vcd_data;

    if ((p_vcd_data == NULL) || (out_file == NULL))
    {
        return;
    }

    if (p_vcd_data->writing_disabled)
    {
        return;         // Writing disabled due to timestamp errors
    }

    size_t var_index;
    const char* id_text = NULL;

    // Check if the variable is already in the list
    for (var_index = 0; var_index < p_vcd_data->no_variables; var_index++)
    {
        vcd_var_data_t* p_var = p_vcd_data->p_vcd[var_index];
        if (strcmp(p_var->name, name) == 0)
        {
            if (vcd_type != p_var->variable_type)
            {
                // Variable found, but with different ID
                save_decoding_error(ERR_VARIABLE_DEFINED_WITH_DIFFERENT_TYPE_BEFORE,
                    0, 0, vcd.string);
                return;
            }

            id_text = p_var->id;
            break;
        }
    }

    // If not found, add it to the list
    if (var_index == p_vcd_data->no_variables)
    {
        if (p_vcd_data->discard_excessive_variables)
        {
            return;     // Too many variables found - do not add more
        }

        if (p_vcd_data->no_variables >= VCD_MAX_VARIABLES_PER_FILE)
        {
            save_decoding_error(ERR_TOO_MANY_VARIABLES_PER_VCD_FILE, 
                VCD_MAX_VARIABLES_PER_FILE, 0, vcd.string);
            p_vcd_data->discard_excessive_variables = true;
            return;
        }

        vcd_var_data_t* p_new_var = (vcd_var_data_t*)allocate_memory(sizeof(vcd_var_data_t), "vcd_var");
        strncpy(p_new_var->name, name, VCD_MAX_VAR_NAME_LENGTH);
        p_new_var->name[VCD_MAX_VAR_NAME_LENGTH - 1] = '\0';
        id_text = id_string(p_vcd_data->no_variables);      // Generate variable ID
        strncpy(p_new_var->id, id_text, VCD_MAX_ID_LENGTH);
        p_new_var->id[VCD_MAX_ID_LENGTH - 1] = '\0';
        p_new_var->variable_type = vcd_type;
        p_vcd_data->p_vcd[p_vcd_data->no_variables] = p_new_var;
        p_vcd_data->no_variables++;
    }

    if (id_text != NULL)
    {
        if (! vcd_process_special_values(p_vcd_data, vcd_type, value_text, id_text, var_index))
        {
            return;     // Do not write value to VCD file
        }

        // Add the timestamp to the VCD file if it has not been added already for the current message.
        vcd_add_timestamp(out_file, p_vcd_data);

        // Print the Value to the VCD file
        if (vcd_type == VCD_TYPE_BIT)
        {
            // Bit value type
            fprintf(out_file, "%s%s\n", value_text, id_text);
        }
        else
        {
            // Other value types
            fprintf(out_file, "%s%s %s\n", vcd_type_data[vcd_type].id_char, value_text, id_text);
        }

        g_msg.enums[file_idx].vcd_data->data_written = true;
    }
}


/**
 * @brief Finalize the VCD variable processing.
 *        Function is called after processing of every VCD variable.
 * 
 * @param  file_idx Index of output file.
 */

void vcd_finalize_variable(rte_enum_t file_idx)
{
    if (!g_msg.vcd_files_processed)
    {
        return;
    }

    vcd_type_t vcd_type = vcd_check_variable_type();

    if (vcd_type  == VCD_TYPE_NONE)
    {
        return;     // Do not save message with incorrect type.
    }

    if (!vcd_variable_data_correct(vcd_type))
    {
        return;     // Do not save incorrect message.
    }

    // Print the Value to VCD file and save it's name to the list of variables (if not there already)
    vcd_save_variable(file_idx,
        vcd_type, &vcd.string[vcd.start_of_name], &vcd.string[vcd.start_of_value]);
}


/**
 * @brief  Add text to the VCD variable string.
 * 
 * @param  text Pointer to the text to be added.
 */

void vcd_add_text_to_string(const char * text)
{
    while (vcd.index < VCD_MAX_ASSEMBLED_STRING_LEN)
    {
        vcd.string[vcd.index] = *text;

        if (*text == '\0')
        {
            break;
        }

        vcd.index++;
        text++;
    }

    vcd.string[VCD_MAX_ASSEMBLED_STRING_LEN - 1] = '\0';
}


/**
 * @brief  Add VCD file header information. 
 * 
 * The information includes:
 *    - date and time of VCD file generation
 *    - date and time of binary data file creation
 *    - RTEmsg version and date of compilation
 *    - timescale (1 ns)
 * 
 * @param  out_file Pointer to the output VCD file.
 */

static void vcd_add_header(FILE * out_file)
{
    // Add current time info
    fprintf(out_file, "$date\n   ");
    time_t t = time(NULL);
    struct tm* currentTime = localtime(&t);
    fprintf(out_file, "%04d-%02d-%02d %02d:%02d:%02d",
        currentTime->tm_year + 1900, currentTime->tm_mon + 1, currentTime->tm_mday,
        currentTime->tm_hour, currentTime->tm_min, currentTime->tm_sec);
    fprintf(out_file, "\n$end\n");

    // Add RTEmsg version and date
    fprintf(out_file, "$version\n   ");
    print_rtemsg_version(out_file);
    fprintf(out_file, "$end\n");

    // Add the binary data file info
    fprintf(out_file, "$comment\n   ");
    jump_to_start_folder();
    print_data_file_name_and_date(out_file);
    open_output_folder();
    fprintf(out_file, "$end\n");

    fprintf(out_file, "$timescale\n   1ns\n$end\n\n$scope module RTEdbg $end\n");
}


/**
 * @brief  Write VCD variable names and definitions to the VCD file.
 *         Variable definitions are sorted alphabetically by variable name.
 *
 * @param  out_file Pointer to the output VCD file.
 * @param  vcd_data Pointer to the VCD file data structure.
 */

static void vcd_write_var_names_to_file(FILE* out_file, vcd_file_data_t* vcd_data, FILE* gtkw_file)
{
    if (vcd_data == NULL)
    {
        return;
    }

    bool printed[VCD_MAX_VARIABLES_PER_FILE];
    memset(printed, 0, sizeof(printed));
    size_t valid_count = 0;
    bool print_default_signal_name_prefix = true;

    for (size_t var_index = 0; var_index < vcd_data->no_variables; var_index++)
    {
        vcd_var_data_t* p_vcd = vcd_data->p_vcd[var_index];

        if (p_vcd != NULL)
        {
            valid_count++;
        }
    }

    size_t printed_count = 0;
    while (printed_count < valid_count)
    {
        size_t min_idx = vcd_data->no_variables;
        const char* min_name = NULL;

        for (size_t j = 0; j < vcd_data->no_variables; j++)
        {
            if (printed[j])
            {
                continue;
            }

            vcd_var_data_t* p_this = vcd_data->p_vcd[j];

            if (p_this == NULL)
            {
                continue;
            }

            const char* this_name = p_this->name;

            if (min_name == NULL || strcmp(this_name, min_name) < 0)
            {
                min_name = this_name;
                min_idx = j;
            }
        }

        if (min_idx == vcd_data->no_variables)
        {
            // No more valid variables found (should not happen)
            break;
        }

        printed[min_idx] = true;
        printed_count++;

        // Write single variable definition to VCD file
        vcd_var_data_t* p_vcd = vcd_data->p_vcd[min_idx];

        if (p_vcd == NULL)
        {
            continue;
        }

        if (vcd_data->previous_bit_value[min_idx] == 0)
        {
            // Variable was never assigned a value - do not print it
            continue;
        }

        fprintf(out_file, "$var %s %d %s %s $end\n",
            vcd_type_data[p_vcd->variable_type].name,
            vcd_type_data[p_vcd->variable_type].size,
            p_vcd->id, p_vcd->name);

        if (gtkw_file != NULL)
        {
            if (p_vcd->variable_type == VCD_TYPE_ANALOG)
            {
                fprintf(gtkw_file, "@88028\n");     // Analog data, step mode, resizing = all data
                                // "@10028" analog data, interpolated, resizing = screen data
                                // "@8028" analog data, step mode, resizing = all data
                                // "-" - empty line or analog height extension if prefixed with @20000
                                // "-comment text" - comments
                print_default_signal_name_prefix = true;
            }
            else
            {
                if (print_default_signal_name_prefix)
                {
                    fprintf(gtkw_file, "@28\n");    // Variable name not selected
                }

                print_default_signal_name_prefix = false;
            }

            // Write variable to GTKWave .gtkw configuration file (list of signals to display)
            fprintf(gtkw_file, "RTEdbg.%s\n", p_vcd->name);

            if (print_default_signal_name_prefix)
            {
                fprintf(gtkw_file, "@20000\n-\n");  // Analog height extension (one line only)
            }
        }
    }

    fprintf(out_file, "$upscope $end\n$enddefinitions $end\n\n");
}


/**
 * @brief Copy the contents from input file to output file in larger chunks.
 *
 * @param in   Pointer to the input file.
 * @param out  Pointer to the output file.
 *
 * @return 0 if copy was successful, -1 otherwise.
 *         Both files are closed in either case.
 */

static int file_copy(FILE* in, FILE* out)
{
    if ((in == NULL) || (out == NULL))
    {
        if (in != NULL)
        {
            fclose(in);
        }

        if (out != NULL)
        {
            fclose(out);
        }

        return -1;
    }

    char buffer[4096];
    size_t bytes_read;
    size_t bytes_written;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), in)) > 0)
    {
        bytes_written = fwrite(buffer, 1, bytes_read, out);

        if (bytes_written != bytes_read)
        {
            fclose(in);
            fclose(out);
            return -1;
        }
    }

    if (ferror(in))
    {
        fclose(in);
        fclose(out);
        return -1;
    }

    fclose(in);
    int err = fclose(out);

    if (err != 0)
    {
        return -1;
    }

    return 0;
}


/**
 * @brief Create GTKWave configuration file for the given VCD file.
 *        The GTKWave file format has been compatibility-tested with GTKWave v3.3.100.
 *
 * @param file_name Pointer to the VCD file name.
 * @param max_name_length Maximum length of signal names in characters.
 *
 * @return Pointer to the opened GTKWave configuration file or NULL 
 *        if error occurred or gtkw file generation is disabled.
 */

static FILE* create_gtkw_file(const char* file_name, unsigned max_name_length)
{
    if (g_msg.param.do_not_generate_gtkw_file)
    {
        return NULL;
    }

    FILE* gtkw_file = NULL;
    char gtkw_file_name[MAX_FILEPATH_LENGTH + 1];

    size_t len = strlen(file_name);

    if (len < 5)
    {
        return NULL;
    }

    len = len - 4;   // Length without .vcd
    snprintf(gtkw_file_name, sizeof(gtkw_file_name), "%.*s.gtkw", (int)len, file_name);
    gtkw_file = fopen(gtkw_file_name, "w");

    if (gtkw_file == NULL)
    {
        report_problem_with_string(ERR_CANNOT_CREATE_GTKW_FILE, gtkw_file_name);
        return NULL;
    }

    /* Get the dimensions of the rectangle representing the primary monitor's working
        * area available to applications (excluding the taskbar).
        */
    unsigned screen_width = 1920;       // Default values
    unsigned screen_height = 1080;
    RECT workArea;

    if (SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0))
    {
        screen_width = workArea.right - workArea.left;
        screen_height = workArea.bottom - workArea.top;
    }

    screen_height -= 27;

    /* Estimate the scaling factor for the GTKWave time axis.
     * The exact formula depends on the width of the Signals window, as GTKWave 
     * adjusts this width based on the length of the signal names.
     */
    unsigned signals_width = ((max_name_length + 1) * 10 + 114);   // Approx. width in pixels (including borders)
    double time_diff = (double)(g_msg.timestamp.last_timestamp_ns - g_msg.timestamp.first_timestamp_ns);

    if (time_diff < 1.0)
    {
        time_diff = 1.0;   // Prevent division by zero
    }

    double scaling_factor = log2(((screen_width - signals_width) / (1920. - signals_width) * 8.2) / time_diff);

    char* orig_locale = setlocale(LC_ALL, NULL);
    setlocale(LC_ALL, "C");             // Use '.' in decimal values instead of system default

    // Write GTKWave configuration file header
    fprintf(gtkw_file, "[*] GTKWave configuration file generated by RTEmsg\n[*]\n");
    fprintf(gtkw_file, "[dumpfile] \"%s\"\n", file_name);
    fprintf(gtkw_file, "[timestart] %llu\n", g_msg.timestamp.first_timestamp_ns);
    fprintf(gtkw_file, "[size] %u %u\n", screen_width, screen_height);
    fprintf(gtkw_file, "[pos] -1 -1\n");            // Default window position
    fprintf(gtkw_file,
        "*%g %llu -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1\n",
        scaling_factor, g_msg.timestamp.first_timestamp_ns);
    fprintf(gtkw_file, "[sst_width] 1\n");          // Width of signal selection tree = force minimum required
    fprintf(gtkw_file, "[signals_width] 1\n");      // Width of signals pane = force minimum required
    fprintf(gtkw_file, "[sst_expanded] 0\n");       // Signal selection tree collapsed
    fprintf(gtkw_file, "[sst_vpaned_height] 1\n");  // Height of signal selection tree pane = force minimum required
    setlocale(LC_ALL, orig_locale);                 // Restore the previous locale

    return gtkw_file;
}


/**
 * @brief  Get the maximum signal name width in characters for all variables in the VCD file.
 * 
 * @param  p_vcd_data Pointer to the VCD file data structure.
 * 
 * @return  Maximum signal name width in characters.
 */

static unsigned get_max_signal_name_width(vcd_file_data_t* p_vcd_data)
{
    unsigned max_width = 0;

    if (p_vcd_data == NULL)
    {
        return 0;
    }

    for (size_t var_index = 0; var_index < p_vcd_data->no_variables; var_index++)
    {
        vcd_var_data_t* p_vcd = p_vcd_data->p_vcd[var_index];
        if (p_vcd != NULL)
        {
            unsigned name_length = (unsigned)strlen(p_vcd->name);
            if (name_length > max_width)
            {
                max_width = name_length;
            }
        }
    }

    return max_width;
}


/**
 * @brief Finalize the VCD files. They contain only timestamps and variable values.
 *        Rename .vcd to .vcd.tmp files, create new .vcd files with headers.
 *        Add variable definitions and append the .vcd.tmp files content.
 *        Remove the .vcd.temporary files.
 */

void vcd_finalize_files(void)
{
    if (!g_msg.vcd_files_processed)
    {
        return;
    }

    size_t no_of_files = g_msg.enums_found;

    if (no_of_files > MAX_ENUMS)
    {
        no_of_files = MAX_ENUMS;
    }

    open_output_folder();

    for (size_t i = NUMBER_OF_FILTER_BITS; i < no_of_files; i++)
    {
        if (g_msg.enums[i].type != OUT_FILE_TYPE)
        {
            continue;
        }

        FILE* out_file = g_msg.enums[i].p_file;
        vcd_file_data_t* p_vcd_data = g_msg.enums[i].vcd_data;

        if ((out_file == NULL) || (p_vcd_data == NULL))
        {
            continue;
        }

        fclose(out_file);
        g_msg.enums[i].p_file = NULL;

        // Rename the VCD file to TEMP
        char temp_vcd_file_name[MAX_FILEPATH_LENGTH + 1];
        snprintf(temp_vcd_file_name, sizeof(temp_vcd_file_name), "%s.tmp", g_msg.enums[i].file_name);

        // Remove existing TEMP file if any
        remove_file(temp_vcd_file_name);

        if (rename(g_msg.enums[i].file_name, temp_vcd_file_name) != 0)
        {
            report_problem_with_string(ERR_CANNOT_RENAME_VCD_FILE_TO_TEMP, g_msg.enums[i].file_name);
            continue;
        }

        out_file = fopen(g_msg.enums[i].file_name, "w");    // Create new VCD file (final version)

        if (out_file == NULL)
        {
            report_problem_with_string(ERR_CANNOT_CREATE_VCD_FILE, g_msg.enums[i].file_name);
            continue;
        }

        unsigned max_signal_name_width = get_max_signal_name_width(p_vcd_data);

        // Create GTKWave configuration file and its header
        FILE* gtkw_file = create_gtkw_file(g_msg.enums[i].file_name, max_signal_name_width);

        vcd_add_header(out_file);                           // Write VCD header
        vcd_write_var_names_to_file(out_file, p_vcd_data, gtkw_file);  // and variable definitions

        // Append the TEMP VCD file (it contains timestamps and variable values)
        FILE* temp_file = fopen(temp_vcd_file_name, "r");

        if (temp_file == NULL)
        {
            report_problem_with_string(ERR_CANNOT_OPEN_TEMP_VCD_FILE_FOR_READING, temp_vcd_file_name);
            fclose(out_file);
            continue;
        }

        // Copy the temporary file to the output file in larger chunks
        if (file_copy(temp_file, out_file) != 0)
        {
            report_problem_with_string(ERR_VCD_COPY_FAILED, g_msg.enums[i].file_name);
            remove_file(g_msg.enums[i].file_name); // Remove incomplete output file too
        }

        if (gtkw_file != NULL)
        {
            fclose(gtkw_file);
        }

        remove_file(temp_vcd_file_name);       // Delete the temporary VCD file
    }
}


/**
 * @brief Checks if the filename ends with .VCD or .vcd extension, ignoring case.
 * 
 * @param filename The filename string.
 * @return true if the extension is .vcd (case-insensitive); false otherwise.
 */

bool is_a_vcd_file(const char* filename)
{
    const size_t required_length = 4;   // Required length for ".vcd"
    size_t len = strlen(filename);

    if (len < required_length)
    {
        return false;
    }

    // Pointer to the last 4 characters (the expected extension)
    const char* extension = filename + len - required_length;

    if (_strcmpi(extension, ".vcd") == 0)
    {
        return true;
    }

    return false;
}


/**
 * @brief Add the double value to the vcd.string.
 *
 * @param fmt_string  Pointer to format string.
 * @param value       Value to be printed.
 */

void vcd_print_double(const char* fmt_string, double value)
{
    /* To circumvent GTKWave's limitation in handling non-normal floating-point values (such as NaN or INF),
     * the software substitutes these values with a numerical placeholder: 9.99e99 (instead of a random value
     * that appears on the graph otherwise). 
     * This substitution ensures the VCD viewer can display the data without errors. */

     // Check if the value is not normal, i.e. not an infinity, subnormal, not-a-number or zero
    if (!isnormal(value))
    {
        if (value != 0.)
        {
            value = 9.99e99;     // Use a very large normal value instead
        }
    }

    char* orig_locale = setlocale(LC_ALL, NULL);
    setlocale(LC_ALL, "C");             // Use '.' in decimal values instead of system default
    snprintf(vcd.temp_string, sizeof(vcd.temp_string), fmt_string, value);
    setlocale(LC_ALL, orig_locale);     // Restore the previous locale
    vcd_add_text_to_string(vcd.temp_string);
}



/**
 * @brief Add an unsigned integer value to the vcd.string.
 *
 * @param fmt_string  Pointer to format string.
 * @param value       Value to be printed.
 */

void vcd_print_uint(const char* fmt_string, uint64_t value)
{
    snprintf(vcd.temp_string, sizeof(vcd.temp_string), fmt_string, value);
    vcd_add_text_to_string(vcd.temp_string);
}


/**
 * @brief Add an integer value to the vcd.string.
 *
 * @param fmt_string  Pointer to format string.
 * @param value       Value to be printed.
 */

void vcd_print_int(const char* fmt_string, int64_t value)
{
    snprintf(vcd.temp_string, sizeof(vcd.temp_string), fmt_string, value);
    vcd_add_text_to_string(vcd.temp_string);
}


/**
 * @brief Add a string to the vcd.string.
 *
 * @param fmt_string  Pointer to format string.
 * @param value       String to be printed.
 */

void vcd_print_string(const char* fmt_string, const char* text)
{
    // Empty string used for the Name field?
    if ((text[0] == '\0') && (strchr(vcd.string, '=') == NULL))
    {
        // Do not write a variable that does not have a complete name to the output file.
        vcd.empty_string_added_to_name = true;

    }

    snprintf(vcd.temp_string, sizeof(vcd.temp_string), fmt_string, text);
    vcd_add_text_to_string(vcd.temp_string);
}


/**
 * @brief  Post processing of VCD files after a message has been processed.
 *         The function adds the message number as variable "N" to all VCD files
 *         and also information about timestamp errors if any were found.
 *         It is called after processing (decoding) of every message.
 */

void vcd_message_post_processing(void)
{
    if (!g_msg.vcd_files_processed)
    {
        return;
    }

    size_t no_of_files = g_msg.enums_found;

    if (no_of_files > (MAX_ENUMS - 1))
    {
        no_of_files = MAX_ENUMS - 1;
    }

    for (rte_enum_t enums_index = NUMBER_OF_FILTER_BITS; enums_index < no_of_files; enums_index++)
    {
        if (g_msg.enums[enums_index].vcd_data != NULL)
        {
            if (! g_msg.enums[enums_index].vcd_data->data_written)
            {
                continue;
            }

            // Add "N=message_number" to the VCD file
            snprintf(vcd.temp_string, sizeof(vcd.temp_string), "%u", g_msg.message_cnt);
            vcd_save_variable(enums_index, VCD_TYPE_FLOAT, "N", vcd.temp_string);

            vcd.temp_string[0] = '0';
            vcd.temp_string[1] = '\0';

            if (g_msg.enums[enums_index].vcd_data->timestamp_error_found)
            {
                vcd.temp_string[0] = '1';
                g_msg.enums[enums_index].vcd_data->timestamp_error_found = false;
            }

            if (g_msg.enums[enums_index].vcd_data->last_timestamp_error_value == vcd.temp_string[0])
            {
                g_msg.enums[enums_index].vcd_data->data_written = false;
                continue;   // Same value as before - do not write to VCD file
            }

            // Add "T=timestamp_error_flag" to the VCD file
            vcd_save_variable(enums_index, VCD_TYPE_BIT, "TsJumpBack", vcd.temp_string);
            g_msg.enums[enums_index].vcd_data->last_timestamp_error_value = vcd.temp_string[0];
            g_msg.enums[enums_index].vcd_data->data_written = false;
        }
    }
}


/**
 * @brief  Write pulse variable data to all open VCD files.
 *         Just one pulse value is allowed per message in a VCD file.
 *         Function is called after processing of every message.
 *         Non zero value of p_vcd_file->pulse_variable_id indicates that a pulse is to be generated.
 */

void vcd_write_pulse_var_data(void)
{
    if (!g_msg.vcd_files_processed)
    {
        return;     // No VCD files open
    }

    if (g_msg.enums_found >= MAX_ENUMS)
    {
        return;
    }

    for (uint32_t idx = NUMBER_OF_FILTER_BITS; idx < g_msg.enums_found; idx++)
    {
        if (g_msg.enums[idx].type != OUT_FILE_TYPE)
        {
            continue;
        }

        vcd_file_data_t* p_vcd_file = g_msg.enums[idx].vcd_data;

        if (p_vcd_file == NULL)
        {
            continue;
        }

        if (p_vcd_file->pulse_variable_id[0] == '\0')
        {
            continue;   // No pulse to be generated
        }

        p_vcd_file->last_timestamp_ns++;    // Add a '0' value 1 ns after the '1' was emitted
        fprintf(g_msg.enums[idx].p_file, "#%llu\n", p_vcd_file->last_timestamp_ns);
        fprintf(g_msg.enums[idx].p_file, "0%s\n", p_vcd_file->pulse_variable_id);

        // Reset the pulse status after writing
        p_vcd_file->pulse_variable_id[0] = '\0';
    }
}

/*==== End of file ====*/
