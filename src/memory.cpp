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

#include "memory.h"

Memory::Memory()
{
    m_debug_region_count = 0;
    m_physical_address_mask = 0xFFFFFFFF;
    m_map_generation = 1;
    m_debug_snapshot_id = 1;
    InitPointer(m_working_ram);
    m_working_ram_size = 0;
    InitPointer(m_video_ram);
    m_video_ram_size = 0;
    memset(m_debug_regions, 0, sizeof(m_debug_regions));
}

Memory::~Memory()
{
}

void Memory::Init()
{
    Reset();
}

void Memory::Reset()
{
    ClearDebugRegions();
    m_physical_address_mask = 0xFFFFFFFF;
}

void Memory::ClearDebugRegions()
{
    memset(m_debug_regions, 0, sizeof(m_debug_regions));
    m_debug_region_count = 0;
    InitPointer(m_working_ram);
    m_working_ram_size = 0;
    InitPointer(m_video_ram);
    m_video_ram_size = 0;
    m_map_generation++;
    m_debug_snapshot_id++;
}

bool Memory::RegisterDebugRegion(int id, const char* name, const u8* read,
    u8* write, u32 size, u32 physical_base, u32 flags)
{
    if (id <= 0 || !IsValidPointer(name) || name[0] == 0 || size == 0 ||
        m_debug_region_count >= GT_DEBUG_MEMORY_MAX_REGIONS ||
        IsValidPointer(FindRegion(id)))
        return false;
    if ((flags & GT_DEBUG_REGION_READABLE) != 0 && !IsValidPointer(read))
        return false;
    if ((flags & GT_DEBUG_REGION_WRITABLE) != 0 && !IsValidPointer(write))
        return false;
    if ((flags & GT_DEBUG_REGION_MAPPED) != 0 &&
        (u64)physical_base + size > 0x100000000ULL)
        return false;

    DebugRegion& region = m_debug_regions[m_debug_region_count++];
    memset(&region, 0, sizeof(region));
    region.info.id = id;
    strncpy_fit(region.info.name, name, sizeof(region.info.name));
    region.info.size = size;
    region.info.physical_base = physical_base;
    region.info.flags = flags;
    region.read = read;
    region.write = write;

    UpdateConveniencePointers();
    m_map_generation++;
    m_debug_snapshot_id++;
    return true;
}

int Memory::GetDebugRegionCount() const
{
    return m_debug_region_count;
}

bool Memory::GetDebugRegion(int index, GT_Debug_Memory_Region& region) const
{
    if (index < 0 || index >= m_debug_region_count)
        return false;
    region = m_debug_regions[index].info;
    return true;
}

void Memory::DebugReadRegionBlock(int id, u32 offset, u8* data,
    GT_Debug_Memory_Status* status, u32 size) const
{
    if (!IsValidPointer(data) || !IsValidPointer(status))
        return;

    for (u32 i = 0; i < size; i++)
    {
        u64 current = (u64)offset + i;
        if (current > 0xFFFFFFFFULL)
        {
            data[i] = 0;
            status[i] = GT_DEBUG_MEMORY_UNMAPPED;
        }
        else
            status[i] = DebugReadRegion(id, (u32)current, data[i]);
    }
}

bool Memory::DebugWriteRegionBlock(int id, u32 offset, const u8* data, u32 size)
{
    DebugRegion* region = FindRegion(id);
    if (!IsValidPointer(region) || !IsValidPointer(data) || size == 0 ||
        (region->info.flags & GT_DEBUG_REGION_WRITABLE) == 0 ||
        !IsValidPointer(region->write) || (u64)offset + size > region->info.size)
        return false;

    memcpy(region->write + offset, data, size);
    m_debug_snapshot_id++;
    return true;
}

bool Memory::TryPeekPhysical(u32 physical, u8& value) const
{
    GT_Debug_Memory_Status status = DebugReadBus(
        NormalizePhysicalAddress(physical), value);
    return status == GT_DEBUG_MEMORY_VALID || status == GT_DEBUG_MEMORY_READ_ONLY;
}

bool Memory::TryPeekPhysicalBlock(u32 physical, u8* data, u32 size) const
{
    if (!IsValidPointer(data))
        return false;

    for (u32 i = 0; i < size; i++)
    {
        if ((u64)physical + i > 0xFFFFFFFFULL ||
            !TryPeekPhysical(physical + i, data[i]))
            return false;
    }
    return true;
}

bool Memory::TryPeekBus(u32 bus_address, u8& value) const
{
    GT_Debug_Memory_Status status = DebugReadBus(bus_address, value);
    return status == GT_DEBUG_MEMORY_VALID || status == GT_DEBUG_MEMORY_READ_ONLY;
}

void Memory::DebugReadPhysicalBlock(u32 physical, u8* data,
    GT_Debug_Memory_Status* status, u32 size) const
{
    if (!IsValidPointer(data) || !IsValidPointer(status))
        return;

    for (u32 i = 0; i < size; i++)
    {
        u64 current = (u64)physical + i;
        if (current > 0xFFFFFFFFULL)
        {
            data[i] = 0;
            status[i] = GT_DEBUG_MEMORY_UNMAPPED;
        }
        else
            status[i] = DebugReadBus(NormalizePhysicalAddress((u32)current), data[i]);
    }
}

void Memory::DebugReadBusBlock(u32 bus_address, u8* data,
    GT_Debug_Memory_Status* status, u32 size) const
{
    if (!IsValidPointer(data) || !IsValidPointer(status))
        return;

    for (u32 i = 0; i < size; i++)
    {
        u64 current = (u64)bus_address + i;
        if (current > 0xFFFFFFFFULL)
        {
            data[i] = 0;
            status[i] = GT_DEBUG_MEMORY_UNMAPPED;
        }
        else
            status[i] = DebugReadBus((u32)current, data[i]);
    }
}

bool Memory::DebugWritePhysicalBlock(u32 physical, const u8* data, u32 size)
{
    if (!IsValidPointer(data) || size == 0)
        return false;

    for (u32 i = 0; i < size; i++)
    {
        u64 current = (u64)physical + i;
        if (current > 0xFFFFFFFFULL)
            return false;

        u32 bus = NormalizePhysicalAddress((u32)current);
        DebugRegion* region = FindMappedRegion(bus);
        if (!IsValidPointer(region) || !IsValidPointer(region->write) ||
            (region->info.flags & GT_DEBUG_REGION_WRITABLE) == 0)
            return false;
    }

    for (u32 i = 0; i < size; i++)
    {
        u32 bus = NormalizePhysicalAddress(physical + i);
        DebugRegion* region = FindMappedRegion(bus);
        region->write[bus - region->info.physical_base] = data[i];
    }
    m_debug_snapshot_id++;
    return true;
}

bool Memory::DebugWriteBusBlock(u32 bus_address, const u8* data, u32 size)
{
    if (!IsValidPointer(data) || size == 0)
        return false;

    for (u32 i = 0; i < size; i++)
    {
        u64 current = (u64)bus_address + i;
        if (current > 0xFFFFFFFFULL)
            return false;

        DebugRegion* region = FindMappedRegion((u32)current);
        if (!IsValidPointer(region) || !IsValidPointer(region->write) ||
            (region->info.flags & GT_DEBUG_REGION_WRITABLE) == 0)
            return false;
    }

    for (u32 i = 0; i < size; i++)
    {
        DebugRegion* region = FindMappedRegion(bus_address + i);
        region->write[bus_address + i - region->info.physical_base] = data[i];
    }
    m_debug_snapshot_id++;
    return true;
}

bool Memory::DebugTranslatePhysical(u32 physical,
    GT_Debug_Memory_Translation& translation) const
{
    translation.physical_valid = true;
    translation.physical = physical;
    translation.bus = NormalizePhysicalAddress(physical);
    return DebugTranslateBus(translation.bus, translation);
}

bool Memory::DebugTranslateBus(u32 bus_address,
    GT_Debug_Memory_Translation& translation) const
{
    translation.bus_valid = true;
    translation.bus = bus_address;
    translation.map_generation = m_map_generation;

    const DebugRegion* region = FindMappedRegion(bus_address);
    if (!IsValidPointer(region))
    {
        strncpy_fit(translation.reason, "Bus address is unmapped",
            sizeof(translation.reason));
        return false;
    }

    translation.region_valid = true;
    translation.region = region->info.id;
    translation.region_offset = bus_address - region->info.physical_base;
    strncpy_fit(translation.region_name, region->info.name,
        sizeof(translation.region_name));
    translation.reason[0] = 0;
    return true;
}

void Memory::SetPhysicalAddressMask(u32 mask)
{
    if (m_physical_address_mask != mask)
    {
        m_physical_address_mask = mask;
        m_map_generation++;
        m_debug_snapshot_id++;
    }
}

u32 Memory::GetMapGeneration() const
{
    return m_map_generation;
}

u64 Memory::GetDebugSnapshotId() const
{
    return m_debug_snapshot_id;
}

u8* Memory::GetWorkingRAM()
{
    return m_working_ram;
}

size_t Memory::GetWorkingRAMSize() const
{
    return m_working_ram_size;
}

u8* Memory::GetVideoRAM()
{
    return m_video_ram;
}

size_t Memory::GetVideoRAMSize() const
{
    return m_video_ram_size;
}

const Memory::DebugRegion* Memory::FindRegion(int id) const
{
    for (int i = 0; i < m_debug_region_count; i++)
    {
        if (m_debug_regions[i].info.id == id)
            return &m_debug_regions[i];
    }
    return NULL;
}

Memory::DebugRegion* Memory::FindRegion(int id)
{
    for (int i = 0; i < m_debug_region_count; i++)
    {
        if (m_debug_regions[i].info.id == id)
            return &m_debug_regions[i];
    }
    return NULL;
}

const Memory::DebugRegion* Memory::FindMappedRegion(u32 bus_address) const
{
    for (int i = 0; i < m_debug_region_count; i++)
    {
        const DebugRegion& region = m_debug_regions[i];
        u64 start = region.info.physical_base;
        u64 end = start + region.info.size;
        if ((region.info.flags & GT_DEBUG_REGION_MAPPED) != 0 &&
            (u64)bus_address >= start && (u64)bus_address < end)
            return &region;
    }
    return NULL;
}

Memory::DebugRegion* Memory::FindMappedRegion(u32 bus_address)
{
    for (int i = 0; i < m_debug_region_count; i++)
    {
        DebugRegion& region = m_debug_regions[i];
        u64 start = region.info.physical_base;
        u64 end = start + region.info.size;
        if ((region.info.flags & GT_DEBUG_REGION_MAPPED) != 0 &&
            (u64)bus_address >= start && (u64)bus_address < end)
            return &region;
    }
    return NULL;
}

GT_Debug_Memory_Status Memory::DebugReadRegion(int id, u32 offset, u8& value) const
{
    value = 0;
    const DebugRegion* region = FindRegion(id);
    if (!IsValidPointer(region) || offset >= region->info.size)
        return GT_DEBUG_MEMORY_UNMAPPED;
    if ((region->info.flags & GT_DEBUG_REGION_READABLE) == 0 ||
        !IsValidPointer(region->read))
        return GT_DEBUG_MEMORY_UNAVAILABLE;

    value = region->read[offset];
    return (region->info.flags & GT_DEBUG_REGION_WRITABLE) != 0 ?
        GT_DEBUG_MEMORY_VALID : GT_DEBUG_MEMORY_READ_ONLY;
}

GT_Debug_Memory_Status Memory::DebugReadBus(u32 bus_address, u8& value) const
{
    value = 0;
    const DebugRegion* region = FindMappedRegion(bus_address);
    if (!IsValidPointer(region))
        return GT_DEBUG_MEMORY_UNMAPPED;
    return DebugReadRegion(region->info.id,
        bus_address - region->info.physical_base, value);
}

u32 Memory::NormalizePhysicalAddress(u32 physical) const
{
    return physical & m_physical_address_mask;
}

void Memory::UpdateConveniencePointers()
{
    DebugRegion* ram = FindRegion(GT_DEBUG_REGION_MAIN_RAM);
    m_working_ram = IsValidPointer(ram) ? ram->write : NULL;
    m_working_ram_size = IsValidPointer(ram) ? ram->info.size : 0;

    DebugRegion* vram = FindRegion(GT_DEBUG_REGION_VRAM);
    m_video_ram = IsValidPointer(vram) ? vram->write : NULL;
    m_video_ram_size = IsValidPointer(vram) ? vram->info.size : 0;
}
