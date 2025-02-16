/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    text.h
 * @author  B. Premzel
 * @brief   Text messages that cannot be in the Messages.txt file are
 *          defined here. They are used during various fatal error reporting
 *          while the Messages.txt file has not been loaded yet.
 *
 * @note    The text coding in this file should be UTF-8.
 ******************************************************************************/

#ifndef _TEXT_H
#define _TEXT_H

#define RTEMSG_INSTRUCTIONS \
        "Syntax: RTEmsg output_folder fmt_folder {options} {input_file}" \
        "\n     or RTEmsg @parameter_file" \
        "\n\nSee the RTEdbg manual for a description of the command line arguments.\n"

#define TXT_INTERNAL_MESSAGE_TEXT_UNDEFINED             "Internal error - message text undefined"
#define TXT_REMAINING_WORDS                             "remaining words"
#define TXT_UNDEFINED_TEXT                              "<undefined text>"
#define TXT_CANT_OPEN_MESSAGES_TXT_FILE                 "Cannot open or read the Messages.txt file. It must be in the same folder as RTEmsg.exe."
#define TXT_NOT_ENOUGH_MESSAGES_IN_MESSAGES_TXT_FILE    "Not enough messages in the 'Messages.txt' file."
#define TXT_MESSAGES_TXT_MUST_CONTAIN                   "The 'Messages.txt' file should contain %u lines.\n"
#define TXT_JUST_N_MESSAGES_FOUND                       "Only %u messages found. "
#define TXT_MESSAGES_REPORT                             "File 'Messages.txt' - line %u. "
#define TXT_MESSAGES_TXT_SHOULD_CONTAIN                 "The file 'Messages.txt' should contain %u lines.\n"
#define TXT_MESSAGE_TOO_LONG                            "Message too long."
#define TXT_TOO_MANY_MESSAGES_IN_FILE                   "Too many messages in the 'Messages.txt' file."
#define TXT_CANT_GET_CURRENT_FOLDER_NAME                "\nCan't get the current folder name\n"
#define TXT_CANT_GET_APP_START_FOLDER                   "\nCan't get RTEmsg application start folder name\n"
#define TXT_CANT_JUMP_TO_APP_FOLDER                     "\nCan't change to the RTEmsg application folder\n"
#define TXT_RTE_ERROR                                   "\nRTEmsg ERR_%03u: "

#endif // _TEXT_H

/*==== End of file ====*/
