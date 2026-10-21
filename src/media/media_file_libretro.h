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

#ifndef MEDIA_FILE_LIBRETRO_H
#define MEDIA_FILE_LIBRETRO_H

#if defined(__LIBRETRO__)

#include "media_file.h"
#include "libretro_vfs_file.h"

class MediaFileLibretro : public MediaFile
{
public:
    MediaFileLibretro();
    virtual ~MediaFileLibretro();

    static void SetVfsInterface(const retro_vfs_interface* iface, unsigned version);
    static bool HasVfsInterface();
    static bool HasVfsDirectories();
    static bool ListDirectory(const char* path, std::vector<std::string>& folders, std::vector<std::string>& files);
    static bool IsCdRomUriPath(const char* path);

    virtual bool Open(const char* path, bool writable) override;
    virtual bool Create(const char* path) override;
    virtual void Close() override;
    virtual bool IsOpen() const override;
    virtual bool IsValid() const override;
    virtual bool CanSeek() const override;
    virtual s64 GetSize() override;
    virtual s64 Tell() override;
    virtual bool Seek(s64 offset) override;
    virtual s64 Read(void* buffer, u64 size) override;
    virtual s64 Write(const void* buffer, u64 size) override;
    virtual bool Flush() override;

private:
    bool Reopen();
    bool SkipBytes(s64 bytes);
    unsigned GetOpenHints(const char* path);
    unsigned GetOpenMode() const;

private:
    static const retro_vfs_interface* s_vfs_interface;
    static unsigned s_vfs_version;
    LibretroVfsFile m_file;
    char* m_path;
    s64 m_position;
    bool m_writable;
};

#endif

#endif /* MEDIA_FILE_LIBRETRO_H */
