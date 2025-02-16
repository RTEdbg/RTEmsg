/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    utf8_helpers.c
 * @author  B. Premzel
 * @brief   Supporting functions for UTF-8 filenames and printing to the console
 ******************************************************************************/

#include "pch.h"
#include <stdlib.h>
#include <stdio.h>
#include <Windows.h>
#include <wchar.h>
#include "rtemsg_config.h"
#include "utf8_helpers.h"
#include "main.h"


/**
 * @brief Opens the Windows filesystem file using a UTF-8 encoded filename.
 *        The filename is converted to wide characters. If the name cannot
 *        be converted to wide characters, it tries to open the file using
 *        the original filename.
 *
 * @param filename  file name (ASCII or UTF-8)
 * @param mode      file access mode
 *
 * @return Pointer to the file or NULL if an error occurs
 */

FILE *utf8_fopen(const char  *filename, const char *mode)
{
    size_t filename_len = strlen(filename);
    size_t mode_len = strlen(mode);

    if ((filename_len == 0) || (mode_len == 0) || (filename_len >= MAX_FILEPATH_LENGTH))
    {
        return NULL;
    }

    wchar_t path[MAX_FILEPATH_LENGTH];
    wchar_t wmode[10];

    int len = MultiByteToWideChar(CP_UTF8, 0, mode, (int)mode_len, wmode, 10);
    wmode[len] = L'\0';

    len = MultiByteToWideChar(CP_UTF8, 0, filename, (int)filename_len, path, MAX_FILEPATH_LENGTH);
    path[len] = L'\0';

    FILE *file;

    if (len == 0)
    {
        // Could not convert the file name to wide characters
        // Try to open the file using the original name
        file = fopen(filename, mode);
    }
    else
    {
        file = _wfopen(path, wmode);

        if (file == NULL)
        {
            file = fopen(filename, mode);
        }
    }

    return file;
}


/**
 * @brief Prints the UTF-8 string using wide characters (printing to the console)
 *
 * @param text         Pointer to the UTF-8 (or ASCII) text
 * @param print_length Maximum number of printed characters
 */

void utf8_print_string(const char *text, size_t print_length)
{
    wchar_t wtext[MAX_UTF8_TEXT_LENGTH];

    if (print_length > (MAX_UTF8_TEXT_LENGTH - 1))
    {
        print_length = (MAX_UTF8_TEXT_LENGTH - 1);
    }

    size_t len = strlen(text);

    if ((len > print_length) && (print_length > 0))
    {
        len = print_length;
    }

    len = utf8_truncate(text, len);

    int converted = MultiByteToWideChar(CP_UTF8, 0, text, (int)len, wtext, MAX_UTF8_TEXT_LENGTH);

    if ((converted == 0) || g_msg.param.codepage_utf8)
    {
        // String conversion was not successful or UTF-8 codepage requested
        // Print as UTF-8 text instead
        printf("%.*s", (int)print_length, text);
    }
    else
    {
        wtext[converted] = L'\0';
        wprintf(L"%s", wtext);
    }
}


/**
 * @brief Removes the Windows filesystem file using a UTF-8 encoded filename.
 *        The filename is converted to wide characters. If the name cannot
 *        be converted to wide characters, it tries to remove the file using
 *        the original UTF-8 filename.
 *
 * @param filename  name of the file to be deleted (ASCII or UTF-8)
 *
 * @return remove() function return value or -1 if an error is detected
 */

int utf8_remove(const char *filename)
{
    size_t filename_len = strlen(filename);
    int result;

    if ((filename_len == 0) || (filename_len >= MAX_FILEPATH_LENGTH))
    {
        return -1;
    }

    wchar_t path[MAX_FILEPATH_LENGTH];
    int len = MultiByteToWideChar(CP_UTF8, 0, filename, (int)filename_len, path, MAX_FILEPATH_LENGTH);
    path[len] = L'\0';

    if (len == 0)
    {
        // Could not convert the file name to wide characters
        // Try to remove the file using the original name
        result = remove(filename);
    }
    else
    {
        result = _wremove(path);
    }

    return result;
}


/**
 * @brief Renames the Windows filesystem file using UTF-8 encoded filenames.
 *        The filenames are converted to wide characters. If the names cannot
 *        be converted to wide characters, it tries to rename the file using
 *        the original UTF-8 filenames.
 *
 * @param old_name  name of the file to be renamed
 * @param new_name  new file name
 *
 * @return rename() function return value or -1 if an error is detected
 */

int utf8_rename(const char *old_name, const char *new_name)
{
    size_t old_name_len = strlen(old_name);
    size_t new_name_len = strlen(new_name);
    int result;

    if ((old_name_len == 0) || (new_name_len == 0)
        || (old_name_len >= MAX_FILEPATH_LENGTH) || (new_name_len >= MAX_FILEPATH_LENGTH))
    {
        return -1;
    }

    wchar_t old_path[MAX_FILEPATH_LENGTH];
    wchar_t new_path[MAX_FILEPATH_LENGTH];

    int len1 = MultiByteToWideChar(CP_UTF8, 0, new_name, (int)new_name_len, new_path, MAX_FILEPATH_LENGTH);
    new_path[len1] = L'\0';

    int len2 = MultiByteToWideChar(CP_UTF8, 0, old_name, (int)old_name_len, old_path, MAX_FILEPATH_LENGTH);
    old_path[len2] = L'\0';

    if ((len2 == 0) || (len1 == 0))
    {
        // Could not convert the file names to wide characters
        // Try to rename the file using the original names
        result = rename(old_name, new_name);
    }
    else
    {
        result = _wrename(old_path, new_path);
    }

    return result;
}


/**
 * @brief Returns the correct truncating index (index of the last valid UTF-8 character).
 *        This avoids truncation of UTF-8 strings in the middle of a UTF-8 character.
 *        The UTF-8 characters can be from one to four bytes long.
 *
 * @param text    Pointer to the UTF-8 string
 * @param length  The desired maximum string length
 *
 * @return Index of last complete UTF-8 character
 */

size_t utf8_truncate(const char *text, size_t length)
{
    if (length < 3)
    {
        return length;
    }

    if (text[length - 1] & 0x80)        // A multi-byte UTF-8 character?
    {
        if (text[length - 1] & 0x40)
        {
            return length - 1;          // Two bytes per multi-byte UTF-8 character
        }

        if ((text[length - 2] & 0xe0) == 0xe0)
        {
            return length - 2;          // Three bytes per multi-byte UTF-8 character
        }

        if ((text[length - 3] & 0xf0) == 0xf0)
        {
            return length - 3;          // Four bytes per multi-byte UTF-8 character
        }
    }

    return length;
}


/**
 * @brief Changes the directory to the specified directory.
 *        The UTF-8 type directory name is converted to the wide char version.
 *        The _wchdir() function is used to change the directory.
 *
 * @param dir_name   Pointer to the UTF-8 or ASCII string with the name of the directory
 *
 * @return 0 if successful or -1 if failure
 */

int utf8_chdir(const char *dir_name)
{
    size_t dir_len = strlen(dir_name);

    if ((dir_len == 0) || (dir_len >= MAX_FILEPATH_LENGTH))
    {
        return -1;      // Error
    }

    wchar_t dir_path[MAX_FILEPATH_LENGTH];
    int len = MultiByteToWideChar(CP_UTF8, 0, dir_name, (int)dir_len, dir_path, MAX_FILEPATH_LENGTH);
    dir_path[len] = L'\0';

    return _wchdir(dir_path);
}

/*==== End of file ====*/
