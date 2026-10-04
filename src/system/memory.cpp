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

#include <stdint.h>
#include "memory.h"

Memory::Memory()
{
    InitPointer(m_cpu_read_pages);
    InitPointer(m_cpu_write_pages);
    m_cpu_map_generation = 0;
    m_debug_region_count = 0;
    m_physical_address_mask = 0xFFFFFFFF;
    m_map_generation = 1;
    m_debug_snapshot_id = 1;
    InitPointer(m_working_ram);
    m_working_ram_size = 0;
    InitPointer(m_state.main_ram);
    InitPointer(m_video_ram);
    m_video_ram_size = 0;
    memset(m_debug_regions, 0, sizeof(m_debug_regions));
}

Memory::~Memory()
{
    SafeDeleteArray(m_cpu_read_pages);
    SafeDeleteArray(m_cpu_write_pages);
    SafeDeleteArray(m_state.main_ram);
}

void Memory::Init()
{
    if (!IsValidPointer(m_state.main_ram))
        m_state.main_ram = new u8[GT_MAIN_RAM_SIZE];

    Reset();
}

void Memory::Reset()
{
    ClearDebugRegions();
    m_physical_address_mask = 0xFFFFFFFF;

    if (IsValidPointer(m_state.main_ram))
        memset(m_state.main_ram, 0, GT_MAIN_RAM_SIZE);
}

INLINE const Memory::DebugRegion* Memory::FindMappedSpan(u32 physical, u32 size) const
{
    if (size == 0 || (u64)physical + size > 0x100000000ULL)
        return NULL;

    u32 masked_bits = ~m_physical_address_mask;

    if (masked_bits != 0)
    {
        u32 boundary = masked_bits & (0U - masked_bits);

        if ((u64)(physical & (boundary - 1)) + size > boundary)
            return NULL;
    }

    u32 bus_address = NormalizePhysicalAddress(physical);
    u64 span_end = (u64)bus_address + size;
    u64 next_boundary = 0x100000000ULL;

    for (int i = 0; i < m_debug_region_count; i++)
    {
        const DebugRegion& region = m_debug_regions[i];

        if ((region.info.flags & GT_DEBUG_REGION_MAPPED) == 0)
            continue;

        u64 start = region.info.physical_base;
        u64 end = start + region.info.size;

        if (bus_address >= start && bus_address < end)
        {
            if (span_end <= end && span_end <= next_boundary)
                return &region;

            return NULL;
        }

        // An earlier region has priority if it starts inside this span
        if (start > bus_address && start < next_boundary)
            next_boundary = start;
    }

    return NULL;
}

u8 Memory::Read8Physical(u32 physical, GT_Bus_Access_Context& context)
{
    UNUSED(context);

    if (m_cpu_map_generation == m_map_generation && IsValidPointer(m_cpu_read_pages[physical >> 12]))
        return m_cpu_read_pages[physical >> 12][physical & 0xFFF];

    u8 value = 0xFF;
    DebugReadBus(NormalizePhysicalAddress(physical), value);
    return value;
}

u16 Memory::Read16Physical(u32 physical, GT_Bus_Access_Context& context)
{
    const u8* data = GetPhysicalReadSpan(physical, 2);

    if (IsValidPointer(data))
        return (u16)data[0] | ((u16)data[1] << 8);

    u16 value = Read8Physical(physical, context);
    value |= (u16)Read8Physical(physical + 1, context) << 8;
    return value;
}

u32 Memory::Read32Physical(u32 physical, GT_Bus_Access_Context& context)
{
    const u8* data = GetPhysicalReadSpan(physical, 4);

    if (IsValidPointer(data))
        return (u32)data[0] | ((u32)data[1] << 8) | ((u32)data[2] << 16) | ((u32)data[3] << 24);

    u32 value = Read16Physical(physical, context);
    value |= (u32)Read16Physical(physical + 2, context) << 16;
    return value;
}

void Memory::Write8Physical(u32 physical, u8 value, GT_Bus_Access_Context& context)
{
    WriteBus(NormalizePhysicalAddress(physical), value, context);
}

void Memory::Write16Physical(u32 physical, u16 value, GT_Bus_Access_Context& context)
{
    if (!IsValidPointer(context.observe_memory_write))
    {
        const DebugRegion* region = FindMappedSpan(physical, 2);

        if (IsValidPointer(region) && IsValidPointer(region->write_data) &&
            (region->info.flags & GT_DEBUG_REGION_WRITABLE) != 0)
        {
            u32 offset = NormalizePhysicalAddress(physical) - region->info.physical_base;
            u8* data = region->write_data + offset;

            data[0] = (u8)value;
            data[1] = (u8)(value >> 8);
            m_debug_snapshot_id += 2;
            return;
        }
    }

    Write8Physical(physical, (u8)value, context);
    Write8Physical(physical + 1, (u8)(value >> 8), context);
}

void Memory::Write32Physical(u32 physical, u32 value, GT_Bus_Access_Context& context)
{
    if (!IsValidPointer(context.observe_memory_write))
    {
        const DebugRegion* region = FindMappedSpan(physical, 4);

        if (IsValidPointer(region) && IsValidPointer(region->write_data) &&
            (region->info.flags & GT_DEBUG_REGION_WRITABLE) != 0)
        {
            u32 offset = NormalizePhysicalAddress(physical) - region->info.physical_base;
            u8* data = region->write_data + offset;

            data[0] = (u8)value;
            data[1] = (u8)(value >> 8);
            data[2] = (u8)(value >> 16);
            data[3] = (u8)(value >> 24);
            m_debug_snapshot_id += 4;
            return;
        }
    }

    Write16Physical(physical, (u16)value, context);
    Write16Physical(physical + 2, (u16)(value >> 16), context);
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

bool Memory::RegisterDebugRegion(int id, const char* name, const u8* read_data, u8* write_data, u32 size, u32 physical_base, u32 flags)
{
    if (id <= 0 ||
        !IsValidPointer(name) ||
        name[0] == 0 ||
        size == 0 ||
        m_debug_region_count >= GT_DEBUG_MEMORY_MAX_REGIONS ||
        IsValidPointer(FindRegion(id)))
        return false;

    if ((flags & GT_DEBUG_REGION_READABLE) != 0 && !IsValidPointer(read_data))
        return false;

    if ((flags & GT_DEBUG_REGION_WRITABLE) != 0 && !IsValidPointer(write_data))
        return false;

    if ((flags & GT_DEBUG_REGION_MAPPED) != 0 && (u64)physical_base + size > 0x100000000ULL)
        return false;

    DebugRegion& region = m_debug_regions[m_debug_region_count++];
    memset(&region, 0, sizeof(region));
    region.info.id = id;
    strncpy_fit(region.info.name, name, sizeof(region.info.name));
    region.info.size = size;
    region.info.physical_base = physical_base;
    region.info.flags = flags;
    region.read_data = read_data;
    region.write_data = write_data;

    UpdateRAMRegions();
    m_map_generation++;
    m_debug_snapshot_id++;
    return true;
}

// Handler regions never get host pages, so every CPU access reaches the device
// Their read handler must be free of side effects unless the region is flagged as MMIO
bool Memory::RegisterHandlerRegion(int id, const char* name, u32 size, u32 physical_base, u32 flags, void* device,
    GT_Memory_Read8_Fn read8, GT_Memory_Write8_Fn write8)
{
    if (id <= 0 ||
        !IsValidPointer(name) ||
        name[0] == 0 ||
        size == 0 ||
        m_debug_region_count >= GT_DEBUG_MEMORY_MAX_REGIONS ||
        IsValidPointer(FindRegion(id)))
        return false;

    if ((flags & GT_DEBUG_REGION_READABLE) != 0 && !IsValidPointer(read8))
        return false;

    if ((flags & GT_DEBUG_REGION_WRITABLE) != 0 && !IsValidPointer(write8))
        return false;

    if ((flags & GT_DEBUG_REGION_MAPPED) != 0 && (u64)physical_base + size > 0x100000000ULL)
        return false;

    DebugRegion& region = m_debug_regions[m_debug_region_count++];
    memset(&region, 0, sizeof(region));
    region.info.id = id;
    strncpy_fit(region.info.name, name, sizeof(region.info.name));
    region.info.size = size;
    region.info.physical_base = physical_base;
    region.info.flags = flags;
    region.device = device;
    region.read8 = read8;
    region.write8 = write8;

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

void Memory::DebugReadRegionBlock(int id, u32 offset, u8* data, GT_Debug_Memory_Status* status, u32 size) const
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

    if (!IsValidPointer(region) ||
        !IsValidPointer(data) ||
        size == 0 ||
        (region->info.flags & GT_DEBUG_REGION_WRITABLE) == 0 ||
        !IsValidPointer(region->write_data) ||
        (u64)offset + size > region->info.size)
        return false;

    memcpy(region->write_data + offset, data, size);
    m_debug_snapshot_id++;
    return true;
}

bool Memory::TryPeekPhysical(u32 physical, u8& value) const
{
    if (m_cpu_map_generation == m_map_generation && IsValidPointer(m_cpu_read_pages[physical >> 12]))
    {
        value = m_cpu_read_pages[physical >> 12][physical & 0xFFF];
        return true;
    }

    GT_Debug_Memory_Status status = DebugReadBus(NormalizePhysicalAddress(physical), value);
    return status == GT_DEBUG_MEMORY_VALID || status == GT_DEBUG_MEMORY_READ_ONLY;
}

const u8* Memory::GetPhysicalReadSpan(u32 physical, u32 size) const
{
    if (size != 0 && (u64)(physical & 0xFFF) + size <= 0x1000 && m_cpu_map_generation == m_map_generation &&
        IsValidPointer(m_cpu_read_pages[physical >> 12]))
        return m_cpu_read_pages[physical >> 12] + (physical & 0xFFF);

    const DebugRegion* region = FindMappedSpan(physical, size);

    if (!IsValidPointer(region) || !IsValidPointer(region->read_data) ||
        (region->info.flags & GT_DEBUG_REGION_READABLE) == 0)
        return NULL;

    return region->read_data + (NormalizePhysicalAddress(physical) - region->info.physical_base);
}

bool Memory::TryPeekPhysicalBlock(u32 physical, u8* data, u32 size) const
{
    if (!IsValidPointer(data))
        return false;

    const u8* source = GetPhysicalReadSpan(physical, size);

    if (IsValidPointer(source))
    {
        uintptr_t source_address = (uintptr_t)source;
        uintptr_t destination_address = (uintptr_t)data;
        uintptr_t distance = source_address > destination_address ?
            source_address - destination_address : destination_address - source_address;

        if (distance >= size)
        {
            memcpy(data, source, size);
            return true;
        }
    }

    for (u32 i = 0; i < size; i++)
    {
        if ((u64)physical + i > 0xFFFFFFFFULL || !TryPeekPhysical(physical + i, data[i]))
            return false;
    }

    return true;
}

bool Memory::TryPeekBus(u32 bus_address, u8& value) const
{
    GT_Debug_Memory_Status status = DebugReadBus(bus_address, value);
    return status == GT_DEBUG_MEMORY_VALID || status == GT_DEBUG_MEMORY_READ_ONLY;
}

void Memory::DebugReadPhysicalBlock(u32 physical, u8* data, GT_Debug_Memory_Status* status, u32 size) const
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

void Memory::DebugReadBusBlock(u32 bus_address, u8* data, GT_Debug_Memory_Status* status, u32 size) const
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

        if (!IsValidPointer(region) ||
            (!IsValidPointer(region->write_data) && !IsValidPointer(region->write8)) ||
            (region->info.flags & GT_DEBUG_REGION_WRITABLE) == 0)
            return false;
    }

    for (u32 i = 0; i < size; i++)
    {
        u32 bus = NormalizePhysicalAddress(physical + i);
        DebugRegion* region = FindMappedRegion(bus);
        u32 offset = bus - region->info.physical_base;

        if (IsValidPointer(region->write8))
            region->write8(region->device, offset, data[i]);
        else
            region->write_data[offset] = data[i];
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

        if (!IsValidPointer(region) ||
            (!IsValidPointer(region->write_data) && !IsValidPointer(region->write8)) ||
            (region->info.flags & GT_DEBUG_REGION_WRITABLE) == 0)
            return false;
    }

    for (u32 i = 0; i < size; i++)
    {
        DebugRegion* region = FindMappedRegion(bus_address + i);
        u32 offset = bus_address + i - region->info.physical_base;

        if (IsValidPointer(region->write8))
            region->write8(region->device, offset, data[i]);
        else
            region->write_data[offset] = data[i];
    }

    m_debug_snapshot_id++;
    return true;
}

bool Memory::DebugTranslatePhysical(u32 physical, GT_Debug_Memory_Translation& translation) const
{
    translation.physical_valid = true;
    translation.physical = physical;
    translation.bus = NormalizePhysicalAddress(physical);
    return DebugTranslateBus(translation.bus, translation);
}

bool Memory::DebugTranslateBus(u32 bus_address, GT_Debug_Memory_Translation& translation) const
{
    translation.bus_valid = true;
    translation.bus = bus_address;
    translation.map_generation = m_map_generation;

    const DebugRegion* region = FindMappedRegion(bus_address);

    if (!IsValidPointer(region))
    {
        strncpy_fit(translation.reason, "Bus address is unmapped", sizeof(translation.reason));
        return false;
    }

    translation.region_valid = true;
    translation.region = region->info.id;
    translation.region_offset = bus_address - region->info.physical_base;
    strncpy_fit(translation.region_name, region->info.name, sizeof(translation.region_name));
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

u8* Memory::GetMainRAM()
{
    return m_state.main_ram;
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

        if ((region.info.flags & GT_DEBUG_REGION_MAPPED) != 0 && (u64)bus_address >= start && (u64)bus_address < end)
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

        if ((region.info.flags & GT_DEBUG_REGION_MAPPED) != 0 && (u64)bus_address >= start && (u64)bus_address < end)
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

    return ReadRegion(*region, offset, value);
}

GT_Debug_Memory_Status Memory::DebugReadBus(u32 bus_address, u8& value) const
{
    value = 0;
    const DebugRegion* region = FindMappedRegion(bus_address);

    if (!IsValidPointer(region))
        return GT_DEBUG_MEMORY_UNMAPPED;

    return ReadRegion(*region, bus_address - region->info.physical_base, value);
}

GT_Debug_Memory_Status Memory::ReadRegion(const DebugRegion& region, u32 offset, u8& value) const
{
    if ((region.info.flags & GT_DEBUG_REGION_READABLE) == 0)
        return GT_DEBUG_MEMORY_UNAVAILABLE;

    if (IsValidPointer(region.read8))
        value = region.read8(region.device, offset);
    else if (IsValidPointer(region.read_data))
        value = region.read_data[offset];
    else
        return GT_DEBUG_MEMORY_UNAVAILABLE;

    return (region.info.flags & GT_DEBUG_REGION_WRITABLE) != 0 ? GT_DEBUG_MEMORY_VALID : GT_DEBUG_MEMORY_READ_ONLY;
}

void Memory::WriteBus(u32 bus_address, u8 value, GT_Bus_Access_Context& context)
{
    DebugRegion* region = FindMappedRegion(bus_address);

    if (!IsValidPointer(region) ||
        (!IsValidPointer(region->write_data) && !IsValidPointer(region->write8)) ||
        (region->info.flags & GT_DEBUG_REGION_WRITABLE) == 0)
        return;

    u32 offset = bus_address - region->info.physical_base;

    if (IsValidPointer(context.observe_memory_write))
    {
        u8 previous = 0;
        ReadRegion(*region, offset, previous);
        context.observe_memory_write(context.memory_write_context, bus_address, previous, value);
    }

    if (IsValidPointer(region->write8))
        region->write8(region->device, offset, value);
    else
        region->write_data[offset] = value;

    m_debug_snapshot_id++;
}

u32 Memory::NormalizePhysicalAddress(u32 physical) const
{
    return physical & m_physical_address_mask;
}

void Memory::UpdateRAMRegions()
{
    DebugRegion* ram = FindRegion(GT_DEBUG_REGION_MAIN_RAM);
    m_working_ram = IsValidPointer(ram) ? ram->write_data : NULL;
    m_working_ram_size = IsValidPointer(ram) ? ram->info.size : 0;

    DebugRegion* vram = FindRegion(GT_DEBUG_REGION_VRAM);
    m_video_ram = IsValidPointer(vram) ? vram->write_data : NULL;
    m_video_ram_size = IsValidPointer(vram) ? vram->info.size : 0;
}

void Memory::PrepareCPUMap()
{
    if (m_cpu_map_generation == m_map_generation)
        return;

    const u32 page_count = 1U << 20;

    if (!IsValidPointer(m_cpu_read_pages))
    {
        m_cpu_read_pages = new const u8*[page_count];
        m_cpu_write_pages = new u8*[page_count];
    }

    memset(m_cpu_read_pages, 0, page_count * sizeof(m_cpu_read_pages[0]));
    memset(m_cpu_write_pages, 0, page_count * sizeof(m_cpu_write_pages[0]));

    // A mask inside a page cannot be represented by a contiguous host pointer
    if ((m_physical_address_mask & 0xFFF) == 0xFFF)
    {
        for (u32 page = 0; page < page_count; page++)
        {
            u32 physical = page << 12;
            const DebugRegion* region = FindMappedSpan(physical, 0x1000);

            if (!IsValidPointer(region) ||
                (region->info.flags & (GT_DEBUG_REGION_MMIO | GT_DEBUG_REGION_VIDEO | GT_DEBUG_REGION_AUDIO)) != 0)
                continue;

            u32 offset = NormalizePhysicalAddress(physical) - region->info.physical_base;

            if ((region->info.flags & GT_DEBUG_REGION_READABLE) != 0 && IsValidPointer(region->read_data))
                m_cpu_read_pages[page] = region->read_data + offset;

            if ((region->info.flags & GT_DEBUG_REGION_WRITABLE) != 0 && IsValidPointer(region->write_data))
                m_cpu_write_pages[page] = region->write_data + offset;
        }
    }

    m_cpu_map_generation = m_map_generation;
}
