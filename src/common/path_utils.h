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

#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <stdio.h>
#include <string.h>
#include <string>
#include "string_utils.h"

inline void append_path_component(std::string& path, const char* component)
{
    if (path.length() > 0)
    {
        char last = path[path.length() - 1];
        if (last != '/' && last != '\\')
        {
#if defined(_WIN32)
            path += "\\";
#else
            path += "/";
#endif
        }
    }

    path += component;
}

inline bool is_absolute_path(const char* path)
{
    if (!path || path[0] == '\0')
        return false;

    if (path[0] == '\\' || path[0] == '/')
        return true;

    return ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':';
}

inline bool is_absolute_path(const std::string& path)
{
    return is_absolute_path(path.c_str());
}

inline const char* get_filename(const char* path)
{
    const char* start = path ? path : "";
    const char* slash = strrchr(start, '/');
    const char* backslash = strrchr(start, '\\');

    if (!slash || (backslash && backslash > slash))
        slash = backslash;

    if (slash)
        start = slash + 1;

    return start;
}

inline void get_directory(const char* path, char* directory, size_t directory_size)
{
    if (!directory || directory_size == 0)
        return;

    directory[0] = '\0';

    if (!path)
        return;

    const char* slash = strrchr(path, '/');
    const char* backslash = strrchr(path, '\\');

    if (!slash || (backslash && backslash > slash))
        slash = backslash;

    if (!slash)
    {
        strncpy_fit(directory, ".", directory_size);
        return;
    }

    size_t length = (size_t)(slash - path);
    if (length >= directory_size)
        length = directory_size - 1;

    memcpy(directory, path, length);
    directory[length] = '\0';
}

inline void get_filename_without_extension(const char* path, char* name, size_t name_size)
{
    if (!name || name_size == 0)
        return;

    name[0] = '\0';

    const char* start = get_filename(path);

    const char* dot = strrchr(start, '.');
    size_t length = dot ? (size_t)(dot - start) : strlen(start);
    if (length >= name_size)
        length = name_size - 1;

    memcpy(name, start, length);
    name[length] = '\0';
}

inline bool join_path(const char* directory, const char* path, char* out_path, size_t out_path_size)
{
    if (!directory || !path || !out_path || out_path_size == 0)
        return false;

    if (is_absolute_path(path))
    {
        strncpy_fit(out_path, path, out_path_size);
        return true;
    }

    size_t directory_length = strlen(directory);
    const char* separator = (directory_length > 0 && directory[directory_length - 1] != '/' && directory[directory_length - 1] != '\\') ? "/" : "";
    int written = snprintf(out_path, out_path_size, "%s%s%s", directory, separator, path);
    if (written < 0 || (size_t)written >= out_path_size)
    {
        out_path[0] = '\0';
        return false;
    }

    return true;
}

inline void append_extension_if_missing(std::string& path, const char* extension)
{
    if (extension && extension[0] != '\0' && !ends_with_no_case(path.c_str(), extension))
        path += extension;
}

inline std::string path_filename(const std::string& path)
{
    size_t separator = path.find_last_of("/\\");
    return separator == std::string::npos ? path : path.substr(separator + 1);
}

inline std::string path_directory(const std::string& path)
{
    size_t separator = path.find_last_of("/\\");
    return separator == std::string::npos ? std::string(".") : path.substr(0, separator == 0 ? 1 : separator);
}

inline std::string path_join(const std::string& directory, const std::string& name)
{
    if (directory.empty())
        return name;

    char last = directory[directory.size() - 1];
    return ((last == '/') || (last == '\\')) ? directory + name : directory + "/" + name;
}

inline bool path_has_extension(const std::string& path)
{
    size_t dot = path.find_last_of('.');
    size_t separator = path.find_last_of("/\\");
    return (dot != std::string::npos) && ((separator == std::string::npos) || (dot > separator));
}

inline std::string path_strip_extension(const std::string& path)
{
    return path_has_extension(path) ? path.substr(0, path.find_last_of('.')) : path;
}

inline std::string path_strip_extension(const std::string& path, const char* extension)
{
    return ends_with_no_case(path, extension) ? path.substr(0, path.size() - strlen(extension)) : path;
}

inline std::string trim_trailing_separators(const std::string& path)
{
    std::string result(path);

    while ((result.size() > 1) && ((result[result.size() - 1] == '/') || (result[result.size() - 1] == '\\')))
        result.resize(result.size() - 1);

    return result;
}

#endif /* PATH_UTILS_H */
