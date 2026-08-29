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

#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include "common.h"
#include "debug_memory.h"

class Memory
{
public:
    Memory();
    ~Memory();
    void Init();
    void Reset();

    void ClearDebugRegions();
    bool RegisterDebugRegion(int id, const char* name, const u8* read,
        u8* write, u32 size, u32 physical_base, u32 flags);
    int GetDebugRegionCount() const;
    bool GetDebugRegion(int index, GT_Debug_Memory_Region& region) const;
    void DebugReadRegionBlock(int id, u32 offset, u8* data,
        GT_Debug_Memory_Status* status, u32 size) const;
    bool DebugWriteRegionBlock(int id, u32 offset, const u8* data, u32 size);

    bool TryPeekPhysical(u32 physical, u8& value) const;
    bool TryPeekPhysicalBlock(u32 physical, u8* data, u32 size) const;
    bool TryPeekBus(u32 bus_address, u8& value) const;
    void DebugReadPhysicalBlock(u32 physical, u8* data,
        GT_Debug_Memory_Status* status, u32 size) const;
    void DebugReadBusBlock(u32 bus_address, u8* data,
        GT_Debug_Memory_Status* status, u32 size) const;
    bool DebugWritePhysicalBlock(u32 physical, const u8* data, u32 size);
    bool DebugWriteBusBlock(u32 bus_address, const u8* data, u32 size);
    bool DebugTranslatePhysical(u32 physical,
        GT_Debug_Memory_Translation& translation) const;
    bool DebugTranslateBus(u32 bus_address,
        GT_Debug_Memory_Translation& translation) const;

    void SetPhysicalAddressMask(u32 mask);
    u32 GetMapGeneration() const;
    u64 GetDebugSnapshotId() const;

    u8* GetWorkingRAM();
    size_t GetWorkingRAMSize() const;
    u8* GetVideoRAM();
    size_t GetVideoRAMSize() const;

private:
    struct DebugRegion
    {
        GT_Debug_Memory_Region info;
        const u8* read;
        u8* write;
    };

    const DebugRegion* FindRegion(int id) const;
    DebugRegion* FindRegion(int id);
    const DebugRegion* FindMappedRegion(u32 bus_address) const;
    DebugRegion* FindMappedRegion(u32 bus_address);
    GT_Debug_Memory_Status DebugReadRegion(int id, u32 offset, u8& value) const;
    GT_Debug_Memory_Status DebugReadBus(u32 bus_address, u8& value) const;
    u32 NormalizePhysicalAddress(u32 physical) const;
    void UpdateConveniencePointers();

private:
    DebugRegion m_debug_regions[GT_DEBUG_MEMORY_MAX_REGIONS];
    int m_debug_region_count;
    u32 m_physical_address_mask;
    u32 m_map_generation;
    u64 m_debug_snapshot_id;
    u8* m_working_ram;
    size_t m_working_ram_size;
    u8* m_video_ram;
    size_t m_video_ram_size;
};

#endif /* MEMORY_H */
