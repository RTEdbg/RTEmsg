/*
 *Copyright (c) 2024 Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    messages.c
 * @author  B. Premzel
 * @brief   Functions for handling message strings in the RTEmsg utility.
 *          Messages are loaded at startup from the "Messages.txt" file.
 *          This allows for the use of translated normal and error print strings.
 *          The "Messages.txt" file should be replaced with a locale-specific version.
 ******************************************************************************/

#include "pch.h"
#include <string.h>
#include <stdlib.h>
#include "main.h"
#include "errors.h"
#include "utf8_helpers.h"
#include "decoder.h"


/**
 * @brief Retrieve the pointer to the text of a message based on its format code.
 *
 * @param message_code  The code of the message whose text should be returned.
 *
 * @return              Pointer to the message text. If message_code is invalid or
 *                      the message text is NULL, returns a default error message.
 */

const char *get_message_text(uint32_t message_code)
{
    if (message_code >= TOTAL_MESSAGES)
    {
        message_code = ERR_WRONG_MESSAGE_CODE;
    }
    const char *p_message = g_msg.message_text[message_code];
    if (p_message == NULL)
    {
        p_message = TXT_INTERNAL_MESSAGE_TEXT_UNDEFINED;
    }
    return p_message;
}


/**
 * @brief Read a line of text from the Messages.txt file and report an error
 *        if the line does not exist or is invalid.
 *
 * @param msg_file    Pointer to the Messages.txt file structure.
 * @param message     Pointer to the buffer where the message text is read.
 * @param line_number The line number of the text message.
 */

static void get_and_check_text_from_message_file(FILE *msg_file, char *message, int line_number)
{
    char text[MAX_UTF8_TEXT_LENGTH];

    *message = 0;
    char *rez = fgets(message, MAX_TXT_MESSAGE_LENGTH, msg_file);

    if (feof(msg_file))
    {
        report_error_and_exit(
            TXT_NOT_ENOUGH_MESSAGES_IN_MESSAGES_TXT_FILE,
            EXIT_FATAL_ERR_FAULTY_MESSAGES_FILE);
    }

    size_t length = strlen(message);
    if ((rez == NULL) || (length < 1))
    {
        snprintf(text, MAX_UTF8_TEXT_LENGTH, TXT_MESSAGES_TXT_MUST_CONTAIN, TOTAL_MESSAGES);
        utf8_print_string(text, 0);
        snprintf(text, MAX_UTF8_TEXT_LENGTH, TXT_JUST_N_MESSAGES_FOUND, line_number + 1);
        utf8_print_string(text, 0);
        report_error_and_exit(
            TXT_NOT_ENOUGH_MESSAGES_IN_MESSAGES_TXT_FILE,
            EXIT_FATAL_ERR_FAULTY_MESSAGES_FILE);
    }
    else if (message[length - 1] != '\n') // Check if a complete line was read
    {
        snprintf(text, MAX_UTF8_TEXT_LENGTH, TXT_MESSAGES_REPORT, line_number + 1);
        utf8_print_string(text, 0);
        report_error_and_exit(TXT_MESSAGE_TOO_LONG, EXIT_FATAL_ERR_FAULTY_MESSAGES_FILE);
    }
    message[length - 1] = 0; // Remove the newline character at the end of the line
}


/**
 * @brief Load error and other text messages from Messages.txt.
 *        The message file must be in the same folder as the RTEmsg executable.
 *        Each line of the message file contains one message string.
 *        The file must contain exactly TOTAL_MESSAGES lines of text.
 */

void load_text_messages(void)
{
    char text[MAX_UTF8_TEXT_LENGTH];

    FILE *msg_file = fopen(RTE_MESSAGES_FILE, "r");
    if (msg_file == NULL)
    {
        report_error_and_exit(TXT_CANT_OPEN_MESSAGES_TXT_FILE,
            EXIT_FATAL_ERR_CANNOT_OPEN_MESSAGES_TXT
        );
    }

    char message[MAX_TXT_MESSAGE_LENGTH + 1];
    char *rez;
    for (int i = 0; i < TOTAL_MESSAGES; i++)
    {
        get_and_check_text_from_message_file(msg_file, message, i);
        process_escape_sequences(message, MAX_TXT_MESSAGE_LENGTH);
        size_t length = strlen(message);

        // Allocate buffer for the recently loaded message and copy it to this buffer
        g_msg.message_text[i] = (char *)allocate_memory(length + 1, "msg_txt");
        memcpy(g_msg.message_text[i], message, length + 1);
    }

    // Check if there are surplus messages in the file
    // Only an empty line should be at the end (or nothing)
    message[0] = 0;
    rez = fgets(message, MAX_TXT_MESSAGE_LENGTH, msg_file);
    if (!feof(msg_file) || (rez != NULL) || (strlen(message) > 0))
    {
        snprintf(text, MAX_UTF8_TEXT_LENGTH, TXT_MESSAGES_TXT_SHOULD_CONTAIN, TOTAL_MESSAGES);
        utf8_print_string(text, 0);
        fclose(msg_file);
        report_error_and_exit(TXT_TOO_MANY_MESSAGES_IN_FILE, EXIT_FATAL_ERR_FAULTY_MESSAGES_FILE);
    }

    fclose(msg_file);
}

/*==== End of file ====*/
