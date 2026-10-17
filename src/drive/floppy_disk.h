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

#ifndef FLOPPY_DISK_H
#define FLOPPY_DISK_H

#include <iostream>
#include "../common/common.h"

class StateSerializer;
struct FloppyImage_Weak;

enum FloppyDisk_Media
{
    FLOPPY_MEDIA_2D = 0,
    FLOPPY_MEDIA_2DD,
    FLOPPY_MEDIA_2HD
};

struct FloppyDisk_Sector
{
    u32 header;
    u8 id[4];
    u16 size;
    bool fm;
    bool deleted;
    u8 status;
};

class FloppyDisk
{
public:
    struct FloppyDisk_State
    {
        bool inserted;
        bool write_protected;
        bool dirty;
        u8 media;
        u16 rpm;
        u32 base_crc;
        u32 size;
        u32 data_crc;
    };

public:
    FloppyDisk();
    ~FloppyDisk();
    bool Insert(const u8* data, u32 size, bool write_protected, u32 base_crc, u32 data_crc = 0, const u8* source = NULL, u32 source_size = 0);
    void Eject();
    void Swap(FloppyDisk& other);
    bool IsInserted() const;
    bool IsWriteProtected() const;
    void SetWriteProtected(bool write_protected);
    bool IsDirty() const;
    void ClearDirty();
    u32 GetWriteCount() const;
    u32 GetBaseCRC() const;
    u32 GetDataCRC() const;
    FloppyDisk_Media GetMedia() const;
    u32 GetRPM() const;
    const u8* GetImage() const;
    u32 GetImageSize() const;
    int GetSectors(int track, FloppyDisk_Sector* sectors) const;
    u8 ReadByte(u32 offset, u8 fill) const;
    u8 ReadWeakByte(u32 offset, u8 fill) const;
    bool HasWeakBytes(u32 offset, u32 size) const;
    u32 GetWeakByteCount() const;
    void WriteByte(u32 offset, u8 value);
    void SetSectorStatus(u32 header, bool deleted, u8 status);
    bool ReplaceTrack(int track, const u8* sectors, u32 size, u32 rpm);
    FloppyDisk_State* GetState();
    void SaveState(std::ostream& stream, bool media = true);
    void LoadState(std::istream& stream, int version = GT_SAVESTATE_VERSION);

private:
    void Identify();
    u32 FindWeakByte(u32 offset) const;
    void RemoveWeakByte(u32 offset);
    void ClearWeakBytes();
    void Serialize(StateSerializer& serializer, bool& media, int version);
    void SanitizeState();

private:
    FloppyDisk_State m_state;
    u8* m_image;
    u32 m_write_count;
    FloppyImage_Weak* m_weak;
    u32 m_weak_count;
};

#include "floppy_disk_inline.h"

#endif /* FLOPPY_DISK_H */
