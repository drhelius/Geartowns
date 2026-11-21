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

#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#if !defined(__LIBRETRO__)
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#endif
#include "log.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>
#undef MINIZ_NO_ZLIB_COMPATIBLE_NAMES

#if defined(_WIN32)
inline std::wstring utf8_to_wstring(const char* utf8_str)
{
    if (!utf8_str || utf8_str[0] == '\0')
        return std::wstring();

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
    if (size_needed <= 0)
        return std::wstring();

    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, &wstr[0], size_needed);
    return wstr;
}

inline std::string wstring_to_utf8(const wchar_t* wide_str)
{
    if (!wide_str || wide_str[0] == L'\0')
        return std::string();

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, NULL, 0, NULL, NULL);
    if (size_needed <= 0)
        return std::string();

    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, &str[0], size_needed, NULL, NULL);
    str.resize(size_needed - 1);
    return str;
}

#define open_ifstream_utf8(stream, path, mode) \
    do { \
        std::wstring wpath = utf8_to_wstring(path); \
        stream.open(wpath.c_str(), mode); \
    } while(0)

#define open_ofstream_utf8(stream, path, mode) \
    do { \
        std::wstring wpath = utf8_to_wstring(path); \
        stream.open(wpath.c_str(), mode); \
    } while(0)

inline FILE* fopen_utf8(const char* path, const char* mode)
{
    std::wstring wpath = utf8_to_wstring(path);
    std::wstring wmode = utf8_to_wstring(mode);
    return _wfopen(wpath.c_str(), wmode.c_str());
}

inline bool rename_file_utf8(const char* from, const char* to)
{
    std::wstring wfrom = utf8_to_wstring(from);
    std::wstring wto = utf8_to_wstring(to);
    return MoveFileExW(wfrom.c_str(), wto.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

inline bool remove_file_utf8(const char* path)
{
    std::wstring wpath = utf8_to_wstring(path);
    return DeleteFileW(wpath.c_str()) != 0;
}
#else
#define open_ifstream_utf8(stream, path, mode) \
    stream.open(path, mode)

#define open_ofstream_utf8(stream, path, mode) \
    stream.open(path, mode)

inline FILE* fopen_utf8(const char* path, const char* mode)
{
    return fopen(path, mode);
}

inline bool rename_file_utf8(const char* from, const char* to)
{
    return rename(from, to) == 0;
}

inline bool remove_file_utf8(const char* path)
{
    return remove(path) == 0;
}
#endif

inline bool create_directory_if_not_exists(const char* path)
{
#if defined(_WIN32)
    int res = _wmkdir(utf8_to_wstring(path).c_str());
#else
    int res = mkdir(path, 0755);
#endif
    if (res == 0 || errno == EEXIST)
        return true;
    else
        return false;
}

inline bool create_directory_tree_if_not_exists(const char* path)
{
    std::string directory(path);

    for (size_t i = 1; i < directory.length(); i++)
    {
        if (directory[i] == '/' || directory[i] == '\\')
            create_directory_if_not_exists(directory.substr(0, i).c_str());
    }

    return create_directory_if_not_exists(path);
}

#if !defined(__LIBRETRO__)
#if !defined(_WIN32)
inline bool remove_directory_and_contents_at(int parent_fd, const char* name)
{
    int fd = openat(parent_fd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);

    if (fd < 0)
        return errno == ENOENT || unlinkat(parent_fd, name, 0) == 0;

    DIR* dir = fdopendir(fd);

    if (!dir)
    {
        close(fd);
        return false;
    }

    bool ok = true;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL)
    {
        const char* entry_name = entry->d_name;

        if (strcmp(entry_name, ".") && strcmp(entry_name, ".."))
            ok = remove_directory_and_contents_at(dirfd(dir), entry_name) && ok;
    }

    closedir(dir);
    return ok && (unlinkat(parent_fd, name, AT_REMOVEDIR) == 0);
}
#endif

inline bool remove_directory_and_contents(const char* path)
{
#if defined(_WIN32)
    std::wstring wpath = utf8_to_wstring(path);
    DWORD attributes = GetFileAttributesW(wpath.c_str());

    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }

    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY))
        return DeleteFileW(wpath.c_str()) != 0;

    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT)
        return RemoveDirectoryW(wpath.c_str()) != 0;

    std::wstring search = utf8_to_wstring((std::string(path) + "\\*").c_str());
    WIN32_FIND_DATAW fd;
    HANDLE find = FindFirstFileW(search.c_str(), &fd);
    bool ok = true;

    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L".."))
            {
                std::string item = std::string(path) + "\\" + wstring_to_utf8(fd.cFileName);
                ok = remove_directory_and_contents(item.c_str()) && ok;
            }
        }
        while (FindNextFileW(find, &fd));

        FindClose(find);
    }

    return ok && (RemoveDirectoryW(wpath.c_str()) != 0);
#else
    return remove_directory_and_contents_at(AT_FDCWD, path);
#endif
}
#endif

inline bool is_safe_zip_entry_name(const char* name)
{
    if (name[0] == '\0' || name[0] == '/' || name[0] == '\\' || name[1] == ':')
        return false;

    const char* component = name;

    while (true)
    {
        size_t length = strcspn(component, "/\\");

        if (length == 2 && component[0] == '.' && component[1] == '.')
            return false;

        if (component[length] == '\0')
            return true;

        component += length + 1;
    }
}

inline bool extract_zip_to_folder(const char* zip_path, const char* out_folder)
{
    using namespace std;
    Debug("Extracting ZIP archive %s...", zip_path);
    Debug("Output folder: %s", out_folder);

    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    mz_bool status = mz_zip_reader_init_file(&zip_archive, zip_path, 0);

    if (!status)
    {
        Error("mz_zip_reader_init_file() failed!");
        return false;
    }

    string out_dir(out_folder);

    if (!create_directory_tree_if_not_exists(out_dir.c_str()))
    {
        Error("Failed to create directory %s", out_dir.c_str());
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);

    for (mz_uint i = 0; i < num_files; i++)
    {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
        {
            Error("mz_zip_reader_file_stat failed for file %d", i);
            mz_zip_reader_end(&zip_archive);
            return false;
        }

        if (!is_safe_zip_entry_name(file_stat.m_filename))
        {
            Error("Unsafe path in ZIP archive: %s", file_stat.m_filename);
            mz_zip_reader_end(&zip_archive);
            return false;
        }

        string name(file_stat.m_filename);
        size_t separator = name.find('/');

        while (separator != string::npos)
        {
            string directory = out_dir + "/" + name.substr(0, separator);

            if (!create_directory_if_not_exists(directory.c_str()))
            {
                Error("Failed to create directory %s", directory.c_str());
                mz_zip_reader_end(&zip_archive);
                return false;
            }

            separator = name.find('/', separator + 1);
        }

        if (file_stat.m_is_directory)
            continue;

        string out_path = out_dir + "/" + name;

        if (!mz_zip_reader_extract_to_file(&zip_archive, i, out_path.c_str(), 0))
        {
            Error("Failed to extract %s", out_path.c_str());
            mz_zip_reader_end(&zip_archive);
            return false;
        }

        Debug("Extracted: %s", out_path.c_str());
    }

    mz_zip_reader_end(&zip_archive);
    Debug("Extraction complete.");

    return true;
}

#endif /* FILE_UTILS_H */
