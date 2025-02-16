/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    format.c
 * @author  B. Premzel
 * @brief   Helper and diagnostic functions for the formatting definition processing.
 ******************************************************************************/

#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include "main.h"
#include "errors.h"
#include "format.h"
#include "files.h"
#include "print_message.h"
#include "print_helper.h"


/**
 * @brief Array of pointers to formatting definitions for message decoding.
 *        A format ID is considered defined if the pointer value is non-null.
 */

msg_data_t *g_fmt[MAX_FMT_IDS];


/**
 * @brief Assigns a format ID to a new message type.
 *        Utilizes the first sufficiently large space aligned to no_fmt_ids.
 *        The search begins at 'g_msg.fmt_align_value'.
 *
 * @param no_fmt_ids    Number of format IDs to reserve.
 *                      Must be a power of 2.
 *                      Examples: 1 for MSG0, 2 for MSG1, 4 for MSG2, etc.
 * @param p_msg_data    Pointer to the memory reserved for message data.
 *
 * @return >= 0 - Assigned format ID number.
 *         0xFFFFFFFF - Error (format ID not assigned).
 */

unsigned assign_fmt_id(unsigned no_fmt_ids, msg_data_t *p_msg_data)
{
    if (no_fmt_ids == 0)
    {
        return 0xFFFFFFFF;      // Error
    }

    // Locate the first unassigned format ID and update the index to skip already assigned IDs.
    for ( ; g_msg.fmt_align_value < g_msg.hdr_data.topmost_fmt_id; g_msg.fmt_align_value++)
    {
        if (g_fmt[g_msg.fmt_align_value] == NULL)
        {
            break;
        }
    }

    // Search for the first unassigned format ID ('no_fmt_ids' consecutive IDs must be free).
    unsigned fmt_id = g_msg.fmt_align_value;
    bool fmt_id_assigned = false;

    // Align the format ID since up to 'no_fmt_ids' consecutive codes will be reserved.
    fmt_id = (fmt_id + no_fmt_ids - 1u) & (~(no_fmt_ids - 1u));

    for ( ; fmt_id < g_msg.hdr_data.topmost_fmt_id; fmt_id += no_fmt_ids)
    {
        if (g_fmt[fmt_id] == NULL)    // Check if not assigned yet.
        {
            fmt_id_assigned = true;

            for (unsigned i = 0; i < no_fmt_ids; i++)
            {
                if (g_fmt[fmt_id + i] != NULL)
                {
                    fmt_id_assigned = false;
                    break; // Exit the inner loop if any required IDs are already assigned.
                }
            }

            if (fmt_id_assigned)
            {
                break; // Exit the outer loop if a suitable range is found.
            }
        }
    }

    if (fmt_id_assigned)
    {
        uint32_t new_limit = 0;

        if (((fmt_id + no_fmt_ids) < g_msg.hdr_data.topmost_fmt_id)
            && (no_fmt_ids < g_msg.hdr_data.topmost_fmt_id))
        {
            new_limit = fmt_id + no_fmt_ids;
        }
        else
        {
            return 0xFFFFFFFF;      // Error
        }

        if (new_limit > g_msg.fmt_ids_defined)
        {
            g_msg.fmt_ids_defined = new_limit;
        }

        for (unsigned i = 0; i < no_fmt_ids; i++)
        {
            g_fmt[fmt_id + i] = p_msg_data;
        }

        return fmt_id;
    }

    return 0xFFFFFFFF;      // No space available for the format ID.
}


/**
 * @brief Finds the index of a message with a given name.
 *
 * @param name  Name of the message to search for.
 *
 * @return  Index of the message with the given name (in the enum_data_t enums[]).
 *          MSG_NAME_NOT_FOUND if the message with the given name is not found.
 */

uint32_t find_message_format_index(const char *name)
{
    uint32_t result = MSG_NAME_NOT_FOUND;

    if (name == NULL)
    {
        return result;
    }

    msg_data_t *p_already_processed = NULL;

    for (uint16_t i = 0; i < g_msg.fmt_ids_defined; i++)
    {
        msg_data_t *p_fmt = g_fmt[i];

        if (p_fmt == NULL)
        {
            continue;   // No formatting definitions for this message type.
        }

        if (p_already_processed == p_fmt)
        {
            continue;   // The format structure has already been processed.
        }

        p_already_processed = p_fmt;

        if ((p_fmt->message_name != NULL) && (strcmp(p_fmt->message_name, name) == 0))
        {
            result = i;
            break;
        }
    }

    return result;
}


/**
 * @brief Print indexed text. If the index is larger than number of texts then
 *        the 'unknown' text will be printed instead.
 *
 * @param out    File to which the text will be written.
 * @param index  Index of the text to be written.
 * @param text   Pointer to a string containing several texts separated by commas.
 */

static void print_indexed_text(FILE *out, unsigned index, const char *text)
{
    unsigned i;

    for (i = 0; i < index; i++)
    {
        while ((*text != 0) && (*text != ','))
        {
            text++;
        }

        if (*text == 0)
        {
            break;
        }

        text++;         // Move to the next text after the comma
    }

    if (*text == 0)     // Text with the specified index was not found.
    {
        fprintf(out, get_message_text(MSG_UNDEFINED_NAME));
    }
    else
    {
        while ((*text != 0) && (*text != ','))
        {
            fputc(*text++, out);
        }

        fputc('\t', out);
    }
}


/**
 * @brief Retrieves the name of an enumerated type. Returns an empty string if not defined.
 *        The function returns names for non-filter enums.
 * 
 * @param index Index used for g_msg.enums[].
 *
 * @return      Name of the enumerated type.
 */

static const char *get_enums_name(unsigned index)
{
    const char *name = NULL;

    if (index < MAX_ENUMS)
    {
        name = g_msg.enums[index].name;
    }

    if ((index < NUMBER_OF_FILTER_BITS) || (name == NULL))
    {
        name = "";
    }

    return name;
}


/**
 * @brief Print the information about the value_format_t formatting string to the file
 *
 * @param out       File to which the information will be written
 * @param p_val_fmt Pointer to the structure containing single value decoding data
 */

static void print_single_value_formatting_data(FILE *out, value_format_t *p_val_fmt)
{
    const char *fmt_string = p_val_fmt->fmt_string;

    if (p_val_fmt->fmt_string == NULL)
    {
        fmt_string = "undefined";
    }
    else
    {
        fmt_string = strip_newlines_and_shorten_string(fmt_string, '"');
    }

    const char *enums_name = "";

    if (p_val_fmt->out_file >= NUMBER_OF_FILTER_BITS)
    {
        enums_name = get_enums_name(p_val_fmt->out_file);
    }

    if (*enums_name == 0)
    {
        enums_name = RTE_MAIN_LOG_FILE;
    }

    const char *copy_to_main = "";

    if (p_val_fmt->print_copy_to_main_log > 0)
    {
        copy_to_main = ">>";
    }

    fprintf(out, "%s\t%s%s\t", fmt_string, copy_to_main, enums_name);

    switch (p_val_fmt->fmt_type)
    {
        case PRINT_PLAIN_TEXT:
        case PRINT_MSG_NO:
        case PRINT_TIMESTAMP:
        case PRINT_dTIMESTAMP:
            fprintf(out, "---\t");
            break;

        default:
            print_indexed_text(out, p_val_fmt->data_type,
                "AUTO,UINT64,INT64,DOUBLE,STRING,TIMESTAMP,dTIMESTAMP,MEMO,TIME_DIFF,MESSAGE_NO");
            break;
    }

    print_indexed_text(out, p_val_fmt->fmt_type, 
        "PLAIN_TEXT,TEXT,SELECTED_TEXT,UINT64,INT64,DOUBLE,BINARY,TIMESTAMP,"
        "dTIMESTAMP,MSG_NO,HEX1U,HEX2U,HEX4U,HEX1L,HEX2L,HEX4L,BIN_TO_FILE,DATE,MSG_NAME"
        );
    fprintf(out, "%u\t%u\t", p_val_fmt->bit_address, p_val_fmt->data_size);
    fprintf(out, "%s\t%s\t%s\t", 
        get_enums_name(p_val_fmt->get_memo),
        get_enums_name(p_val_fmt->put_memo),
        get_enums_name(p_val_fmt->in_file)
        );
    fprintf(out, "%g\t%g\t", p_val_fmt->offset, p_val_fmt->mult);

    const char *timer_name = "";
    unsigned int fmt_id = p_val_fmt->fmt_id_timer;

    if (fmt_id != 0)
    {
        timer_name = get_format_id_name(fmt_id);
    }

    const char *value_statistic_name = "";

    if (p_val_fmt->value_stat != NULL)
    {
        const char *name = p_val_fmt->value_stat->name;

        if (name != NULL)
        {
            value_statistic_name = name;
        }
    }

    fprintf(out, "%s\t%s\t", timer_name, value_statistic_name);
    fprintf(out, "\n");
}


/**
 * @brief Export the complete information from the format decoding structures.
 *        The information is written to a CSV type file "Format.csv"
 */

void print_format_decoding_information(void)
{
    if (g_msg.param.debug == false)
    {
        return;
    }

    open_output_folder();
    FILE *out = fopen(RTE_FORMAT_DBG_FILE, "w");

    if (out == NULL)
    {
        report_problem_with_string(ERR_CANT_CREATE_DEBUG_FILE, RTE_FORMAT_DBG_FILE);
        return;
    }

    fprintf(out,
        "FMT\tName\tType\tLength\t"
        "String\tOutput\tData type\tFmt_type\tAddr\tSize\t"
        "Get.memo\tPut.memo\tIn.file/memo\tOffset\tMult\tTimer\tStatistics\t\n"
    );

    msg_data_t *p_fmt = NULL;

    for (unsigned i = 0; i < g_msg.fmt_ids_defined; i++)
    {
        if ((p_fmt == g_fmt[i])       // Data has been printed already since the next pointer points
            || (g_fmt[i] == NULL))    // to the same structure or was not defined for this format ID
        {
            continue;                 // Skip empty or already processed formatting definitions
        }

        p_fmt = g_fmt[i];
        fprintf(out, "%u\t", i);

        // "name;type;length;shift;"
        fprintf(out, "%s\t", (p_fmt->message_name == NULL) ? "undefined" : p_fmt->message_name);
        print_indexed_text(out, p_fmt->msg_type, "MSG0_NN,MSGN,EXT_MSG,MSGX");
        fprintf(out, "%u\t", p_fmt->msg_len);

        size_t fmt_counter = 0;
        value_format_t *p_val_fmt = p_fmt->format;

        while (p_val_fmt != NULL)
        {
            if (fmt_counter > 0)
            {
                fprintf(out, "\t\t\t\t");
            }

            fmt_counter++;

            print_single_value_formatting_data(out, p_val_fmt);
            p_val_fmt = p_val_fmt->format;
        }
    }

    fclose(out);
}


/**
 * @brief Get name of the format ID (as defined in the format definition file)
 *
 * @param fmt_id  Formatting code
 *
 * @return pointer to text with format ID name or (undefined) if not defined
 */

const char *get_format_id_name(unsigned fmt_id)
{
    const char *p_name = NULL;

    if (fmt_id < MAX_FMT_IDS)
    {
        p_name = g_fmt[fmt_id]->message_name;
    }

    if (p_name == NULL)
    {
        p_name = get_message_text(MSG_UNDEFINED_NAME);
    }

    return p_name;
}


/**
 * @brief Print format ID name if it was defined or nothing otherwise
 *
 * @param out   Pointer to the output file
 */

void print_format_id_name(FILE *out)
{
    // Check if the format ID is valid and if the formatting definition exists for this format ID
    uint32_t current_fmt_id = g_msg.fmt_id;

    if (current_fmt_id < MAX_FMT_IDS)
    {
        msg_data_t *p_fmt = g_fmt[current_fmt_id];

        if (p_fmt != NULL)
        {
            const char *name = get_format_id_name(current_fmt_id);
            fprintf(out, "'%s', ", name);
        }
    }
}

/*==== End of file ====*/
