/*
 * Copyright (c) 2024 Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    parse_file_handling.c
 * @authors S. Milivojcev, B. Premzel
 * @brief   Various helper functions for the format definition file handling
 ******************************************************************************/

#include "pch.h"
#include <time.h>
#include "parse_file_handling.h"
#include <windows.h>
#include "files.h"
#include "utf8_helpers.h"
#include "main.h"
#include "parse_error_reporting.h"
#include "decoder.h"


/**
 * @brief Generates a headguard string from a given input string (file name).
 *
 * @param in_buff    The input string, which may contain multibyte characters.
 * @param out_buff   A buffer to store the resulting headguard string.
 * @param buff_size  The size of the out_buff in bytes.
 */

static void create_headguard_string(const char *in_buff, char *out_buff, size_t buff_size)
{
    *out_buff = '\0';

    if (buff_size < 6)
    {
        return;
    }

    *out_buff++ = 'R';       // Add the prefix "RTE_"
    *out_buff++ = 'T';
    *out_buff++ = 'E';
    *out_buff++ = '_';
    buff_size -= 5;          // Reserve space for "RTE_" and null terminator

    const char *p_slash = strrchr(in_buff, '/');
    const char *p_backslash = strrchr(in_buff, '\\');

    if (p_backslash > p_slash)
    {
        p_slash = p_backslash;      // Start from the last slash or backslash
    }

    if (p_slash == NULL)
    {
        p_slash = in_buff;
    }

    char *string = out_buff;

    for (size_t i = 0; i < buff_size; i++)
    {
        int c = (unsigned char)(*p_slash++);

        if (c == '\0')
        {
            break;
        }

        if (isascii(c))
        {
            // Convert non-alphanumeric ASCII characters to underscores and lowercase to uppercase
            c = toupper(c);

            if (!isalnum(c))
            {
                c = '_';
            }
        }
        else
        {
            // Convert a multibyte character to a representative ASCII character
            c = 'A' + ((c & 0x0F) ^ ((c & 0xF0) >> 4));
        }

        *string++ = (char)c;
    }

    *string = '\0';
}


/**
 * @brief Attempts to open a file and checks for errors. If the error is EACCES (permission denied),
 *        the function will retry opening the file until the timeout defined by MAX_FILE_OPEN_TIME [ms].
 *
 * @param filename    The path of the file to be opened.
 * @param file        A pointer to store the file pointer of the opened file.
 *
 * @return  true if the file was successfully opened, false if an error occurred.
 */

static bool open_file(const char *filename, FILE **file)
{
    clock_t start_time = clock();

    do
    {
        *file = utf8_fopen(filename, "r+");

        if (*file != NULL)
        {
            return true;
        }

        // If the error was EACCES (permission denied), the file might still be blocked by the IDE
        if (errno != EACCES)
        {
            break;
        }

        Sleep(10);      // Wait before retrying
    }
    while ((clock() - start_time) <= MAX_FILE_OPEN_TIME);

    return false;
}


/**
 * @brief Creates a file with specified initial text. The file type is determined by the write_mode.
 *
 * @param filename              The path of the file to be created.
 * @param initial_text          The text to be written after the file is created.
 * @param write_mode            The fopen mode parameter.
 *
 * @return  A pointer to the file, or NULL if the file could not be created.
 */

FILE *create_file(char *filename, char *initial_text, const char *write_mode)
{
    FILE *new_file = utf8_fopen(filename, write_mode);

    if (new_file != NULL)
    {
        if (initial_text != NULL)
        {
            process_escape_sequences(initial_text, strlen(initial_text));

            if (*initial_text != '\0')
            {
                fputs(initial_text, new_file);
            }
        }
    }

    return new_file;
}


/**
 * @brief Creates a temporary work file for parsing output. If the new content differs from the
 *        existing header file, the work file will replace the current header file.
 *
 * @param parse_handle  Pointer to the main parse handle structure.
 *
 * @return  true if the file is successfully created, false otherwise.
 */

static bool create_work_file(parse_handle_t *parse_handle)
{
    char header_guard_text[MAX_HEADGUARD_LENGTH];
    create_headguard_string(parse_handle->fmt_file_path, header_guard_text, MAX_HEADGUARD_LENGTH);

    if (*header_guard_text == '\0')    // Check if headguard string was created
    {
        return false;
    }

    snprintf(parse_handle->work_file_name, MAX_FILENAME_LENGTH,
        "%s%s", parse_handle->fmt_file_path, ".work");

    FILE *new_file = utf8_fopen(parse_handle->work_file_name, "w+T");

    if (new_file == NULL)
    {
        return false;
    }

    // Write header guards
    if (parse_handle->write_output_to_header)
    {
        fprintf(new_file, "/* %s */\n\n", get_message_text(MSG_HEADER_CAVEAT));
    }

    fprintf(new_file, "#ifndef %s\n", header_guard_text);
    fprintf(new_file, "#define %s\n", header_guard_text);
    parse_handle->p_fmt_work_file = new_file;

    return true;
}


/**
 * @brief Compares the contents of two files and closes them. Determines if the contents are identical.
 *
 * @param src  Pointer to the source format definition file.
 * @param dst  Pointer to the work file.
 *
 * @return  true if contents are identical, false otherwise.
 */

static bool compare_and_close_files(FILE *src, FILE *dst)
{
    bool are_identical = false;

    char src_buffer[CMP_BUFSIZ];
    char dst_buffer[CMP_BUFSIZ];
    int comparison_result;

    rewind(src);
    rewind(dst);

    do
    {
        // Read data blocks from src and dst
        size_t src_bytes_read = fread(src_buffer, sizeof(char), CMP_BUFSIZ, src);
        size_t dst_bytes_read = fread(dst_buffer, sizeof(char), CMP_BUFSIZ, dst);

        // If the number of bytes read differs, the files are not identical
        if (src_bytes_read != dst_bytes_read)
        {
            break;
        }

        // If both files have reached EOF, they are identical
        if ((src_bytes_read == 0) && (dst_bytes_read == 0))
        {
            are_identical = true;
            break;
        }

        comparison_result = memcmp(src_buffer, dst_buffer, src_bytes_read);
    }
    while (comparison_result == 0);

    fclose(src);
    fclose(dst);

    return are_identical;
}


/**
 * @brief Opens the format definition file and prepares the work file.
 *
 * @param parse_handle  Pointer to the main parse handle structure.
 *
 * @return  true if files are prepared successfully, false if an error occurs.
 */

bool setup_parse_files(parse_handle_t *parse_handle)
{
    open_format_folder();

    size_t name_len = strlen(parse_handle->fmt_file_path);

    if (MAX_FILENAME_LENGTH <= (name_len + 5 + 1))   /* 5+1 =".work" extension + '\0'*/
    {
        report_parsing_error(parse_handle->p_parse_parent,
            ERR_PARSE_FILE_FILENAME_TOO_LONG, parse_handle->fmt_file_path);
        return false;
    }

    // If the filename ending = ".fmt" write the output to the header file (file ending .h)
    if (name_len >= 4)
    {
        if (strcmp(&parse_handle->fmt_file_path[name_len - 4], ".fmt") == 0)
        {
            parse_handle->write_output_to_header = true;
        }
    }

    if (!open_file(parse_handle->fmt_file_path, &parse_handle->p_fmt_file))
    {
        report_parsing_error(parse_handle->p_parse_parent,
            ERR_PARSE_FILE_CANNOT_OPEN_FMT_FILE, parse_handle->fmt_file_path);
        return false;
    }

    if (g_msg.param.check_syntax_and_compile && !create_work_file(parse_handle))
    {
        report_parsing_error(parse_handle->p_parse_parent,
            ERR_PARSE_FILE_CANNOT_CREATE_FMT_WORK_FILE, parse_handle->work_file_name);
        return false;
    }

    return true;
}


/**
 * @brief   Compares the contents of the header file and work file.
 *          If they differ, replaces the header file with the work file.
 *          If they are identical, deletes the work file.
 *
 * @param parse_handle   Pointer to the main parse handle structure.
 */

static void check_and_replace_header_file(parse_handle_t *parse_handle)
{
    // Close the .fmt file now - it has been parsed already
    fclose(parse_handle->p_fmt_file);

    // If parsing errors were found, delete the work file
    if (parse_handle->parsing_errors_found)
    {
        fclose(parse_handle->p_fmt_work_file);

        if (utf8_remove(parse_handle->work_file_name) != 0)
        {
            report_parsing_error(parse_handle->p_parse_parent,
                ERR_PARSE_FILE_WORK_CANNOT_REMOVE, parse_handle->work_file_name);
        }

        return;
    }

    // Construct the header file name and attempt to open it
    char header_file_name[MAX_FILENAME_LENGTH];
    snprintf(header_file_name, MAX_FILENAME_LENGTH, "%s.h", parse_handle->fmt_file_path);

    _set_errno(0);
    FILE *header_file = utf8_fopen(header_file_name, "r");

    if (header_file != NULL)
    {
        // Check if the header and work files are the same
        bool same_contents = false;

        if (!parse_handle->parsing_errors_found)
        {
            same_contents = compare_and_close_files(header_file, parse_handle->p_fmt_work_file);
        }
        else
        {
            fclose(parse_handle->p_fmt_work_file);
        }

        if (same_contents || parse_handle->parsing_errors_found)
        {
            // Remove the work file since it is the same as the header
            if (utf8_remove(parse_handle->work_file_name) != 0)
            {
                report_parsing_error(parse_handle->p_parse_parent,
                    ERR_PARSE_FILE_WORK_CANNOT_REMOVE, parse_handle->work_file_name);
            }

            return;
        }

        // Remove the header file to replace it with the work file
        if (utf8_remove(header_file_name) != 0)
        {
            report_parsing_error(parse_handle->p_parse_parent,
                ERR_PARSE_FILE_HEADER_CANNOT_REMOVE, header_file_name);
            return;
        }
    }
    else if (errno != ENOENT)   // Check if the file does not exist
    {
        // File could not be accessed (i.e. blocked by the toolchain editor)
        report_parsing_error(parse_handle->p_parse_parent,
            ERR_PARSE_FILE_HEADER_CANNOT_OPEN, header_file_name);
        return;
    }
    else
    {
        fclose(parse_handle->p_fmt_work_file);
    }

    // Rename the work file to the header file
    if (utf8_rename(parse_handle->work_file_name, header_file_name) != 0)
    {
        report_parsing_error(parse_handle->p_parse_parent,
            ERR_PARSE_FILE_WORK_CANNOT_RENAME, header_file_name);
    }
}


/**
 * @brief   Compares the contents of the fmt and work file.
 *          If they differ, replaces the fmt file with the work file.
 *          Creates a backup if the -back option is enabled.
 *          If they are identical, deletes the work file.
 *
 * @param parse_handle   Pointer to the main parse handle structure.
 */

void check_and_replace_work_file(parse_handle_t *parse_handle)
{
    open_format_folder();
    _set_errno(0);

    // Use a different procedure if the output of a .fmt file is written to the header
    if (parse_handle->write_output_to_header)
    {
        check_and_replace_header_file(parse_handle);
        return;
    }

    // Check if the format and work files are the same
    bool same_contents = false;

    if (!parse_handle->parsing_errors_found)
    {
        same_contents = compare_and_close_files(parse_handle->p_fmt_file, parse_handle->p_fmt_work_file);
    }
    else
    {
        fclose(parse_handle->p_fmt_work_file);
        fclose(parse_handle->p_fmt_file);
    }

    // Discard the work file if it matches the format file or if parsing errors were found
    if (same_contents || parse_handle->parsing_errors_found)
    {
        if (utf8_remove(parse_handle->work_file_name) != 0)
        {
            report_parsing_error(parse_handle->p_parse_parent,
                ERR_PARSE_FILE_WORK_CANNOT_REMOVE, parse_handle->work_file_name);
        }

        return;
    }

    // Backup the FMT file if the backup option is enabled
    if (g_msg.param.create_backup)
    {
        char backup_file_name[MAX_FILENAME_LENGTH];
        snprintf(backup_file_name, MAX_FILENAME_LENGTH, "%s.bak", parse_handle->fmt_file_path);

        utf8_remove(backup_file_name);     // Remove existing backup file

        // Rename the FMT file to create a backup
        if (utf8_rename(parse_handle->fmt_file_path, backup_file_name) != 0)
        {
            int err = errno;    // Preserve the error code for 'report_parsing_error()'
            utf8_remove(parse_handle->work_file_name); // Attempt to remove the work file
            _set_errno(err);

            report_parsing_error(parse_handle->p_parse_parent,
                ERR_PARSE_FILE_FMT_CANNOT_RENAME, backup_file_name);
            return;
        }
    }
    else // Replace the FMT file with the work file
    {
        // Remove the FMT file first
        if (utf8_remove(parse_handle->fmt_file_path) != 0)
        {
            report_parsing_error(parse_handle->p_parse_parent,
                ERR_PARSE_FILE_FMT_CANNOT_REMOVE, parse_handle->fmt_file_path);
            return;
        }
    }

    // Rename the work file to the FMT file
    if (utf8_rename(parse_handle->work_file_name, parse_handle->fmt_file_path) != 0)
    {
        report_parsing_error(parse_handle->p_parse_parent,
            ERR_PARSE_FILE_WORK_CANNOT_RENAME, parse_handle->fmt_file_path);
    }
}


/**
 * @brief   Reads a file and converts it to a string for the %Y specifier.
 *          This function is used when parsing the <IN_FILE directive.
 *          The entire file content is read into a single string. Each line
 *          can have a length between 1 and 255 characters. The first byte of each line
 *          indicates the line's length (excluding the length byte itself). Lines do not
 *          end with a '\0' character. A line length of zero indicates the end of data.
 *
 * @param filename       The name of the file to be read.
 * @param parse_handle   Pointer to the main parse handle structure.
 */

void read_file_to_indexed_text(const char *filename, parse_handle_t *parse_handle)
{
    FILE *file = NULL;

    if (!open_file(filename, &file))
    {
        catch_parsing_error(parse_handle, ERR_PARSE_IN_FILE_SELECT_ERROR, filename);
    }

    __int64 file_size = get_file_size(file);

    if ((file_size < 0) || (file_size > MAX_IN_FILE_SIZE))
    {
        fclose(file);
        catch_parsing_error(parse_handle, ERR_PARSE_IN_FILE_TOO_LONG, filename);
    }

    unsigned char *str = allocate_memory((size_t)(file_size + 2), "Yfile");
    size_t no_read = fread(str + 1, 1, (size_t)file_size, file);
    str[no_read + 1] = 0;
    unsigned char *start_line = str;
    unsigned char *pos = str + 1;
    size_t no_found = 0;

    while ((*pos != 0) && (*(pos + 1) != 0))
    {
        if ((*pos == '\n') || (*pos == 0))
        {
            size_t line_length = pos - start_line;

            if ((line_length > 256) || (line_length < 2))
            {
                *start_line = 0;
                fclose(file);
                catch_parsing_error(parse_handle, ERR_PARSE_IN_FILE_SELECT_INVALID_OPTIONS, filename);
            }

            *start_line = (unsigned char)(line_length - 1);
            start_line = pos;
            no_found++;
        }

        pos++;
    }

    if (no_found < 2)
    {
        catch_parsing_error(parse_handle, ERR_PARSE_IN_FILE_SELECT_MIN_TWO_LINES, filename);
    }

    *pos = 0;
    g_msg.enums[g_msg.enums_found].in_file_txt = (char *)str;
    fclose(file);
}


/**
 * @brief  Writes a '#define NAME VALUE' directive to the work file.
 *
 * @param parse_handle  Pointer to the main parse handle structure.
 * @param name          The name of the define.
 * @param value         The value to define.
 */

void write_define_to_work_file(parse_handle_t *parse_handle, const char *name, unsigned int value)
{
    if (g_msg.param.check_syntax_and_compile)
    {
        if ((parse_handle->p_fmt_work_file != NULL) && (!g_msg.param.purge_defines))
        {
            fprintf(parse_handle->p_fmt_work_file, "#define %s %dU\n", name, value);
        }
    }
}

/*==== End of file ====*/
