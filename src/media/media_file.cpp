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

#include "media_file.h"
#include "media_file_native.h"
#include "../common/common.h"

#if defined(__LIBRETRO__)
#include "media_file_libretro.h"
#include "../cdrom/cdrom_common.h"
#endif

MediaFile::~MediaFile()
{

}

MediaFile* MediaFile::OpenFile(const char* path, bool writable)
{
#if defined(__LIBRETRO__)
    if (MediaFileLibretro::HasVfsInterface())
    {
        MediaFile* file = new MediaFileLibretro;
        if (file->Open(path, writable))
            return file;

        SafeDelete(file);
        return NULL;
    }

    if (IsCdRomUri(path))
        return NULL;
#endif

    MediaFile* file = new MediaFileNative;
    if (file->Open(path, writable))
        return file;

    SafeDelete(file);
    return NULL;
}

MediaFile* MediaFile::OpenNewFile(const char* path)
{
#if defined(__LIBRETRO__)
    if (MediaFileLibretro::HasVfsInterface())
    {
        MediaFile* file = new MediaFileLibretro;

        if (file->Create(path))
            return file;

        SafeDelete(file);
        return NULL;
    }
#endif

    MediaFile* file = new MediaFileNative;

    if (file->Create(path))
        return file;

    SafeDelete(file);
    return NULL;
}

bool MediaFile::ListDirectory(const char* path, std::vector<std::string>& folders, std::vector<std::string>& files)
{
    folders.clear();
    files.clear();

    if (!IsValidPointer(path) || (path[0] == '\0'))
        return false;

#if defined(__LIBRETRO__)
    if (MediaFileLibretro::HasVfsDirectories())
        return MediaFileLibretro::ListDirectory(path, folders, files);
#endif

    return MediaFileNative::ListDirectory(path, folders, files);
}

bool MediaFile::Exists(const char* path)
{
    if (!IsValidPointer(path) || (path[0] == '\0'))
        return false;

#if defined(__LIBRETRO__)
    if (MediaFileLibretro::HasVfsInterface())
        return MediaFileLibretro::Exists(path);
#endif

    return MediaFileNative::Exists(path);
}

bool MediaFile::IsDirectory(const char* path)
{
    if (!IsValidPointer(path) || (path[0] == '\0'))
        return false;

#if defined(__LIBRETRO__)
    if (MediaFileLibretro::HasVfsDirectories())
        return MediaFileLibretro::IsDirectory(path);
#endif

    return MediaFileNative::IsDirectory(path);
}

static MediaFile* open_whole_file(const char* path, u64 max_size, bool allow_empty, u64* size)
{
    MediaFile* file = MediaFile::OpenFile(path);

    if (!IsValidPointer(file))
        return NULL;

    s64 file_size = file->GetSize();

    if ((file_size < 0) || ((u64)file_size > max_size) || ((u64)file_size > (u64)SIZE_MAX) ||
        ((file_size == 0) && !allow_empty))
    {
        SafeDelete(file);
        return NULL;
    }

    *size = (u64)file_size;
    return file;
}

bool MediaFile::ReadFile(const char* path, u64 max_size, std::vector<u8>* data, bool allow_empty)
{
    u64 size = 0;
    data->clear();
    MediaFile* file = open_whole_file(path, max_size, allow_empty, &size);

    if (!IsValidPointer(file))
        return false;

    data->resize((size_t)size);
    bool ok = (size == 0) || file->ReadExact(&(*data)[0], size);
    SafeDelete(file);

    if (!ok)
        data->clear();

    return ok;
}

bool MediaFile::ReadFile(const char* path, u64 max_size, std::string* data, bool allow_empty)
{
    u64 size = 0;
    data->clear();
    MediaFile* file = open_whole_file(path, max_size, allow_empty, &size);

    if (!IsValidPointer(file))
        return false;

    data->resize((size_t)size);
    bool ok = (size == 0) || file->ReadExact(&(*data)[0], size);
    SafeDelete(file);

    if (!ok)
        data->clear();

    return ok;
}

bool MediaFile::WriteFileAtomic(const char* path, const void* data, u64 size)
{
    std::string temporary = std::string(path) + ".tmp";
    MediaFile* file = OpenNewFile(temporary.c_str());
    bool ok = IsValidPointer(file) && ((size == 0) || file->WriteAt(0, data, size)) && file->Flush();
    SafeDelete(file);
    ok = ok && ReplaceFile(temporary.c_str(), path);

    if (!ok)
        RemoveFile(temporary.c_str());

    return ok;
}

bool MediaFile::ReplaceFile(const char* temporary, const char* path)
{
    if (IsDirectory(path))
        return false;

    if (RenameFile(temporary, path))
        return true;

    std::string old = std::string(path) + ".old";
    RemoveFile(old.c_str());
    bool moved = RenameFile(path, old.c_str());
    bool ok = RenameFile(temporary, path);

    if (ok)
        RemoveFile(old.c_str());
    else if (moved)
        RenameFile(old.c_str(), path);

    return ok;
}

bool MediaFile::RenameFile(const char* from, const char* to)
{
#if defined(__LIBRETRO__)
    if (MediaFileLibretro::HasVfsInterface())
        return MediaFileLibretro::RenameFile(from, to);
#endif

    return rename_file_utf8(from, to);
}

bool MediaFile::RemoveFile(const char* path)
{
#if defined(__LIBRETRO__)
    if (MediaFileLibretro::HasVfsInterface())
        return MediaFileLibretro::RemoveFile(path);
#endif

    return remove_file_utf8(path);
}

void MediaFile::SetVfsInterface(const retro_vfs_interface* iface, unsigned version)
{
#if defined(__LIBRETRO__)
    MediaFileLibretro::SetVfsInterface(iface, version);
#else
    (void)iface;
    (void)version;
#endif
}

bool MediaFile::HasVfsInterface()
{
#if defined(__LIBRETRO__)
    return MediaFileLibretro::HasVfsInterface();
#else
    return false;
#endif
}

bool MediaFile::ReadExact(void* buffer, u64 size)
{
    if ((size > (u64)SIZE_MAX) || (!IsValidPointer(buffer) && (size != 0)))
        return false;

    u8* destination = reinterpret_cast<u8*>(buffer);
    u64 total = 0;

    while (total < size)
    {
        s64 read = Read(destination + total, size - total);
        if ((read <= 0) || ((u64)read > (size - total)))
            return false;

        total += (u64)read;
    }

    return true;
}

bool MediaFile::ReadAt(u64 offset, void* buffer, u64 size)
{
    if (!CanSeek() || (offset > (u64)INT64_MAX))
        return false;

    if (!Seek((s64)offset))
        return false;

    return ReadExact(buffer, size);
}

bool MediaFile::WriteAt(u64 offset, const void* buffer, u64 size)
{
    if (!CanSeek() || (offset > (u64)INT64_MAX) || (size > (u64)INT64_MAX) || !IsValidPointer(buffer))
        return false;

    if (!Seek((s64)offset))
        return false;

    return Write(buffer, size) == (s64)size;
}
