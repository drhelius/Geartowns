/*
 * Geartowns - FM Towns Emulator
 * Copyright (C) 2026  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <string>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <libgen.h>
#elif defined(__linux__)
#include <unistd.h>
#include <linux/limits.h>
#endif
#if !defined(_WIN32)
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <math.h>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "geartowns.h"

static inline void sdl_log_error(const char* action, const char* file, int line)
{
    const char* error = SDL_GetError();
    if (error && error[0] != '\0')
    {
        Log("SDL Error: %s (%s:%d) - %s", action, file, line, error);
        SDL_ClearError();
    }
}

#define SDL_ERROR(action) sdl_log_error(action, __FILE__, __LINE__)

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY_PATTERN_SPACED "%c%c%c%c %c%c%c%c"
#define BYTE_TO_BINARY_PATTERN_ALL_SPACED "%c %c %c %c %c %c %c %c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')

static inline int get_reset_value(int option)
{
    switch (option)
    {
        case 0:
            return -1;
        case 1:
            return 0x0000;
        case 2:
            return 0xFFFF;
        default:
            return -1;
    }
}

static inline std::string base64_encode(const unsigned char* data, int size)
{
    static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result;
    result.reserve(((size + 2) / 3) * 4);

    int i = 0;
    while (i < size)
    {
        unsigned char byte1 = data[i++];
        bool has_byte2 = i < size;
        unsigned char byte2 = has_byte2 ? data[i++] : 0;
        bool has_byte3 = i < size;
        unsigned char byte3 = has_byte3 ? data[i++] : 0;

        result.push_back(base64_chars[byte1 >> 2]);
        result.push_back(base64_chars[((byte1 & 0x03) << 4) | (byte2 >> 4)]);
        result.push_back(has_byte2 ? base64_chars[((byte2 & 0x0F) << 2) | (byte3 >> 6)] : '=');
        result.push_back(has_byte3 ? base64_chars[byte3 & 0x3F] : '=');
    }

    return result;
}

static inline void format_local_time(time_t timestamp, const char* format, char* buffer, size_t size)
{
    struct tm time_info;
    if (get_host_time(timestamp, false, &time_info))
        strftime(buffer, size, format, &time_info);
    else if (size > 0)
        buffer[0] = '\0';
}

static inline void get_date_time_string(time_t timestamp, char* buffer, size_t size)
{
    format_local_time(timestamp, "%Y-%m-%d %H:%M:%S", buffer, size);
}

static inline void get_file_date_time_string(time_t timestamp, char* buffer, size_t size)
{
    format_local_time(timestamp, "%Y-%m-%d %H%M%S", buffer, size);
}

static inline void get_current_date_time_string(char* buffer, size_t size)
{
    time_t timestamp = time(NULL);
    get_date_time_string(timestamp, buffer, size);
}

static inline bool path_exists(const char* path)
{
    if (!path || path[0] == '\0')
        return false;

#if defined(_WIN32)
    DWORD attributes = GetFileAttributesW(utf8_to_wstring(path).c_str());
    return attributes != INVALID_FILE_ATTRIBUTES;
#else
    struct stat info;
    return stat(path, &info) == 0;
#endif
}

static inline void get_executable_path(char* path, size_t size)
{
    if (!path || size == 0)
        return;

    path[0] = '\0';

#if defined(_WIN32)
    wchar_t wide_path[1024];
    DWORD len = GetModuleFileNameW(NULL, wide_path, 1024);
    if (len > 0 && len < 1024 && WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, path, (int)size, NULL, NULL) > 0)
    {
        char* last_slash = strrchr(path, '\\');
        if (last_slash) *last_slash = '\0';

        char* server_pos = strrchr(path, '\\');
        const char* directory_name = server_pos ? server_pos + 1 : path;
        if (strcmp(directory_name, "server") == 0)
        {
            if (server_pos == path + 2 && path[1] == ':')
                path[3] = '\0';
            else if (server_pos == path)
                path[1] = '\0';
            else if (server_pos)
                *server_pos = '\0';
            else
                path[0] = '\0';
        }
    }
    else
    {
        path[0] = '\0';
    }
#elif defined(__APPLE__)
    char executable_path[PATH_MAX];
    char resolved_path[PATH_MAX];
    uint32_t bufsize = (uint32_t)sizeof(executable_path);
    if (_NSGetExecutablePath(executable_path, &bufsize) == 0)
    {
        char* dir = dirname(realpath(executable_path, resolved_path) ? resolved_path : executable_path);
        size_t length = dir ? strlen(dir) : size;
        if (length >= size)
        {
            path[0] = '\0';
            return;
        }
        memmove(path, dir, length + 1);

        char* server_pos = strrchr(path, '/');
        const char* directory_name = server_pos ? server_pos + 1 : path;
        if (strcmp(directory_name, "server") == 0)
        {
            if (server_pos == path)
                path[1] = '\0';
            else if (server_pos)
                *server_pos = '\0';
            else
                path[0] = '\0';
        }
        else if (ends_with(path, "/Contents/MacOS"))
        {
            size_t len = strlen(path);
            const char* repl = "Resources";

            // Replace the trailing "MacOS" with "Resources"
            if (len >= strlen("MacOS") && (len - strlen("MacOS") + strlen(repl) + 1) <= size)
            {
                memcpy(path + (len - strlen("MacOS")), repl, strlen(repl) + 1);
            }
        }
    }
    else
    {
        path[0] = '\0';
    }
#elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", path, size - 1);
    if (len >= 0 && (size_t)len < size - 1)
    {
        path[len] = '\0';
        char* last_slash = strrchr(path, '/');
        if (last_slash) *last_slash = '\0';

        char* server_pos = strrchr(path, '/');
        const char* directory_name = server_pos ? server_pos + 1 : path;
        if (strcmp(directory_name, "server") == 0)
        {
            if (server_pos == path)
                path[1] = '\0';
            else if (server_pos)
                *server_pos = '\0';
            else
                path[0] = '\0';
        }
    }
    else
    {
        path[0] = '\0';
    }
#else
    const char* base_path = SDL_GetBasePath();
    if (base_path)
    {
        strncpy_fit(path, base_path, size);
        size_t len = strlen(path);
        while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\'))
            path[--len] = '\0';

        char* server_pos = strrchr(path, '/');
        const char* directory_name = server_pos ? server_pos + 1 : path;
        if (strcmp(directory_name, "server") == 0)
        {
            if (server_pos == path)
                path[1] = '\0';
            else if (server_pos)
                *server_pos = '\0';
            else
                path[0] = '\0';
        }
    }
#endif
}

static inline void strip_color_tags(std::string& str)
{
    size_t pos = 0;
    while ((pos = str.find('{', pos)) != std::string::npos)
    {
        size_t end_pos = str.find('}', pos);
        if (end_pos != std::string::npos)
            str.erase(pos, end_pos - pos + 1);
        else
            pos++;
    }
}

#endif /* UTILS_H */
