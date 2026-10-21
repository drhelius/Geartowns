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

#ifndef MEDIA_FILE_H
#define MEDIA_FILE_H

#include <string>
#include <vector>
#include "../common/types.h"

struct retro_vfs_interface;

class MediaFile
{
public:
    virtual ~MediaFile();

    static MediaFile* OpenFile(const char* path, bool writable = false);
    static MediaFile* OpenNewFile(const char* path);
    static bool ListDirectory(const char* path, std::vector<std::string>& folders, std::vector<std::string>& files);
    static void SetVfsInterface(const retro_vfs_interface* iface, unsigned version = 1);
    static bool HasVfsInterface();

    virtual bool Open(const char* path, bool writable) = 0;
    virtual bool Create(const char* path) = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() const = 0;
    virtual bool IsValid() const = 0;
    virtual bool CanSeek() const = 0;
    virtual s64 GetSize() = 0;
    virtual s64 Tell() = 0;
    virtual bool Seek(s64 offset) = 0;
    virtual s64 Read(void* buffer, u64 size) = 0;
    virtual s64 Write(const void* buffer, u64 size) = 0;
    virtual bool Flush() = 0;
    bool ReadExact(void* buffer, u64 size);
    bool ReadAt(u64 offset, void* buffer, u64 size);
    bool WriteAt(u64 offset, const void* buffer, u64 size);
};

#endif /* MEDIA_FILE_H */
