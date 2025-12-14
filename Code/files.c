/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    files.c
 * @author  B. Premzel
 * @brief   Helper functions for file handling and Messages.txt file processing.
 ******************************************************************************/

#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <time.h>
#include <windows.h>
#include "main.h"
#include "errors.h"
#include "files.h"
#include "decoder.h"
#include "utf8_helpers.h"


/**
 * @brief  Get the file size
 *
 * @param  fp  pointer to the file
 * @return file size or -1 if the file size cannot be determined
 */

int64_t get_file_size(FILE *fp)
{
    _fseeki64(fp, 0, SEEK_END);
    int64_t size = (int64_t)ftell(fp);
    rewind(fp);
    return size;
}


/**
 * @brief Set current folder to the folder from which the application was started
 */

void jump_to_start_folder(void)
{
    // Jump back to the start folder
    if (_wchdir(g_msg.file.start_folder) != 0)
    {
        report_error_and_exit(get_message_text(FATAL_CANT_CHANGE_TO_START_FOLDER),
            EXIT_FATAL_ERR_START_FOLDER);
    }
}


/**
 * @brief Set current folder to the format folder
 */

void open_format_folder(void)
{
    // Set current folder to the format folder. To do this, must go to the start folder first.
    jump_to_start_folder();

    if (utf8_chdir(g_msg.param.fmt_folder) != 0)
    {
        printf("\n[%s]: ", g_msg.param.fmt_folder);
        report_error_and_exit(get_message_text(FATAL_CANT_OPEN_FORMAT_FOLDER),
            EXIT_FATAL_ERR_OUTPUT_FOLDER);
    }
}


/**
 * @brief Set current folder to the output folder
 */

void open_output_folder(void)
{
    // Jump to the start folder in case the output folder name is given relative to it
    jump_to_start_folder();

    if (utf8_chdir(g_msg.param.working_folder) != 0)
    {
        printf("\n[%s]: ", g_msg.param.working_folder);
        report_error_and_exit(get_message_text(FATAL_CANT_OPEN_OUTPUT_FOLDER),
            EXIT_FATAL_ERR_OUTPUT_FOLDER);
    }
}


/**
 * @brief Get the folder name from which the RTEmsg app has been started,
 *        store it to the main data structure and change current dir to this folder.
 */

void setup_working_folder_info(void)
{
    g_msg.file.start_folder = _wgetcwd(NULL, 0);

    if (g_msg.file.start_folder == NULL)
    {
        report_error_and_exit(TXT_CANT_GET_CURRENT_FOLDER_NAME, EXIT_FATAL_ERR_GETCWD_START);
    }

    // Set current_folder to the folder from where the software has been started
    char *app_folder;

    if (_get_pgmptr(&app_folder) != 0)
    {
        report_error_and_exit(TXT_CANT_GET_APP_START_FOLDER, EXIT_FATAL_ERR_PGMPTR);
    }

    char *chr_position = strrchr(app_folder, '\\');

    if (chr_position != NULL)
    {
        *chr_position = 0;      // Delete the "\\RTEmsg.exe" at the end
    }

    // Jump to the RTEmsg app folder - the file with message strings is located there
    if (_chdir(app_folder) != 0)
    {
        report_error_and_exit(TXT_CANT_JUMP_TO_APP_FOLDER, EXIT_FATAL_ERR_PGMFOLDER);
    }
}


/**
 * @brief Open all system files which are used during the data decoding and
 *        load the error and other messages
 */

void create_error_file(void)
{
    // Create Error.log in the output folder
    open_output_folder();

    g_msg.file.error_log = fopen(RTE_ERR_FILE, "w");

    if (g_msg.file.error_log == NULL)
    {
        report_error_and_exit(get_message_text(FATAL_CANT_CREATE_ERR_LOG_FILE),
            EXIT_FATAL_ERR_CREATE_ERR_FILE);
    }

    g_msg.file.main_log = g_msg.file.error_log;   // The complete output goes to the Errors.log until the format parsing is finished
}


/**
 * @brief Open the Main.log and Stat_main.log files.
 *        The Main.log file is created during the binary file decoding process.
 */

void create_main_log_file(void)
{
    // Open the output folder to create Main.log
    open_output_folder();

    // Create Main.log
    g_msg.file.main_log = fopen(RTE_MAIN_LOG_FILE, "w");

    if (g_msg.file.main_log == NULL)
    {
        report_problem_with_string(ERR_CANT_CREATE_DEBUG_FILE, RTE_MAIN_LOG_FILE);
    }

#if defined STREAM_BUFF_SIZE
    /* Allocate a buffer for the stream; a larger buffer may not significantly increase write speed
     * The setvbuf function optimizes I/O performance by controlling stream buffering, reducing 
     * frequent operations and improving efficiency.
     */
    char *stream_buffer = allocate_memory(STREAM_BUFF_SIZE, "strBuff");
    int rez = setvbuf(g_msg.file.main_log, stream_buffer, _IOFBF, STREAM_BUFF_SIZE);
#endif

    // Create main statistics log file
    g_msg.file.statistics_log = fopen(RTE_STAT_MAIN_FILE, "w");

    if (g_msg.file.statistics_log == NULL)
    {
        report_problem_with_string(FATAL_CANT_CREATE_FILE, RTE_STAT_MAIN_FILE);
    }
}


/**
 * @brief Deletes a specified file and reports an error if deletion fails.
 *
 * @param file_name Name of the file to be deleted.
 */

void remove_file(const char *file_name)
{
    _set_errno(0);
        int result = utf8_remove(file_name);

    if (result == -1)
        {
        if (errno != ENOENT)    // ENOENT indicates the file does not exist, which is acceptable
        {
            report_problem_with_string(ERR_COULD_NOT_DELETE_FILE, file_name);
        }
    }
}


/**
 * @brief Deletes all files that can be generated with command line parameters.
 *        Files from previous runs should be removed from the output folder to avoid confusion
 *        about which files were created during the latest data decoding.
 */

void remove_old_files(void)
{
    open_output_folder();

    remove_file(RTE_STAT_MSG_COUNTERS_FILE);
    remove_file(RTE_STAT_MISSING_MSGS_FILE);
    remove_file(RTE_STAT_VALUES_FILE);

    if (g_msg.param.create_timestamp_file == 0)
    {
        remove_file(RTE_MSG_TIMESTAMPS_FILE);
    }

    if (g_msg.param.debug == 0)
    {
        remove_file(RTE_FORMAT_DBG_FILE);
    }

    jump_to_start_folder();
    g_msg.total_errors = 0;         // Reset error count, ignoring errors related to file deletion
}


/**
 * @brief Prepares a file or folder name by removing newlines, quotation marks, and
 *        trailing directory separators ('\\' or '/').
 *
 * @param name        Pointer to the file/folder name.
 * @param error_code  Error code to report if the name starts with '-'.
 *
 * @return      Pointer to the modified folder name.
 */

char *prepare_folder_name(char *name, unsigned error_code)
{
    size_t len = strlen(name);

    // Check if an option was mistakenly provided instead of a folder or file name
    if ((*name == '-') && (error_code > 0))
    {
        report_error_and_show_instructions(get_message_text(error_code), name);
    }

    // Remove any newline character present in the name
    char *newline = strrchr(name, '\n');

    if (newline != NULL)
    {
        *newline = '\0';
    }

    if (len >= 2)
    {
        if (*name == '"')
        {
            name++;
            len--;
        }

        if (name[len - 1u] == '"')
        {
            name[len - 1u] = '\0';
            len--;
        }
    }

    if (len > 1)
    {
        // Remove the trailing directory separator if present
        if ((name[len - 1] == '/') || (name[len - 1] == '\\'))
        {
            name[len - 1] = '\0';
        }
    }

    return name;
}

/*==== End of file ====*/
