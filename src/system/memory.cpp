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
#include "../common/trace_logger.h"
#include "../common/state_serializer.h"

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
    m_main_ram_size = GT_MAIN_RAM_SIZE;
    InitPointer(m_state.main_ram);
    memset(m_state.cmos, 0, sizeof(m_state.cmos));
    m_state.main_memory = false;
    m_state.boot_ram = false;
    m_state.dictionary = false;
    m_state.dictionary_bank = 0;
    InitPointer(m_video_ram);
    m_video_ram_size = 0;
    InitPointer(m_trace_logger);
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
        m_state.main_ram = new u8[m_main_ram_size];

    Reset();
}

void Memory::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

// Power-on reset, backup RAM keeps its contents
void Memory::Reset()
{
    ClearDebugRegions();
    m_physical_address_mask = 0xFFFFFFFF;

    if (IsValidPointer(m_state.main_ram))
        memset(m_state.main_ram, 0, m_main_ram_size);
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
    const DebugRegion* found = FindMappedRegion(bus_address);

    if (!IsValidPointer(found) || span_end > (u64)found->info.physical_base + found->info.size)
        return NULL;

    bool found_overlay = (found->info.flags & GT_DEBUG_REGION_OVERLAY) != 0;

    // A region with priority over the one found must not start inside this span
    for (int i = 0; i < m_debug_region_count; i++)
    {
        const DebugRegion& region = m_debug_regions[i];

        if (&region == found)
            break;

        if ((region.info.flags & GT_DEBUG_REGION_MAPPED) == 0)
            continue;

        bool overlay = (region.info.flags & GT_DEBUG_REGION_OVERLAY) != 0;
        u64 start = region.info.physical_base;

        if (overlay == found_overlay && start < span_end && start + region.info.size > bus_address)
            return NULL;
    }

    if (!found_overlay)
    {
        for (int i = 0; i < m_debug_region_count; i++)
        {
            const DebugRegion& region = m_debug_regions[i];
            u64 start = region.info.physical_base;

            if ((region.info.flags & (GT_DEBUG_REGION_MAPPED | GT_DEBUG_REGION_OVERLAY)) ==
                (GT_DEBUG_REGION_MAPPED | GT_DEBUG_REGION_OVERLAY) &&
                start < span_end && start + region.info.size > bus_address)
                return NULL;
        }
    }

    return found;
}

u8 Memory::Read8Physical(u32 physical, GT_Bus_Access_Context& context)
{
    if (m_cpu_map_generation == m_map_generation && IsValidPointer(m_cpu_read_pages[physical >> 12]))
        return m_cpu_read_pages[physical >> 12][physical & 0xFFF];

    return ReadBus(NormalizePhysicalAddress(physical), context);
}

u16 Memory::Read16Physical(u32 physical, GT_Bus_Access_Context& context)
{
    const u8* data = GetPhysicalReadSpan(physical, 2);

    if (IsValidPointer(data))
        return read_u16_le(data);

    u16 value = Read8Physical(physical, context);
    value |= (u16)Read8Physical(physical + 1, context) << 8;
    return value;
}

u32 Memory::Read32Physical(u32 physical, GT_Bus_Access_Context& context)
{
    const u8* data = GetPhysicalReadSpan(physical, 4);

    if (IsValidPointer(data))
        return read_u32_le(data);

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

            write_u16_le(data, value);
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

            write_u32_le(data, value);
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

    if ((flags & (GT_DEBUG_REGION_MAPPED | GT_DEBUG_REGION_OVERLAY)) != 0 && (u64)physical_base + size > 0x100000000ULL)
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
// A read handler with side effects needs a peek handler for passive reads
bool Memory::RegisterHandlerRegion(int id, const char* name, u32 size, u32 physical_base, u32 flags, void* device,
    GT_Memory_Read8_Fn read8, GT_Memory_Write8_Fn write8, GT_Memory_Read8_Fn peek8)
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

    if ((flags & (GT_DEBUG_REGION_MAPPED | GT_DEBUG_REGION_OVERLAY)) != 0 && (u64)physical_base + size > 0x100000000ULL)
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
    region.peek8 = peek8;

    m_map_generation++;
    m_debug_snapshot_id++;
    return true;
}

// A banked view switches on or off, only the host pages it covers are rebuilt
bool Memory::SetRegionMapped(int id, bool mapped)
{
    DebugRegion* region = FindRegion(id);

    if (!IsValidPointer(region))
        return false;

    if (((region->info.flags & GT_DEBUG_REGION_MAPPED) != 0) == mapped)
        return true;

    if (mapped)
        region->info.flags |= GT_DEBUG_REGION_MAPPED;
    else
        region->info.flags &= ~GT_DEBUG_REGION_MAPPED;

    RemapRange(region->info.physical_base, region->info.size);
    return true;
}

bool Memory::SetRegionData(int id, const u8* read_data, u8* write_data)
{
    DebugRegion* region = FindRegion(id);

    if (!IsValidPointer(region) ||
        ((region->info.flags & GT_DEBUG_REGION_READABLE) != 0 && !IsValidPointer(read_data)) ||
        ((region->info.flags & GT_DEBUG_REGION_WRITABLE) != 0 && !IsValidPointer(write_data)))
        return false;

    if (region->read_data == read_data && region->write_data == write_data)
        return true;

    region->read_data = read_data;
    region->write_data = write_data;

    if ((region->info.flags & GT_DEBUG_REGION_MAPPED) != 0)
        RemapRange(region->info.physical_base, region->info.size);
    else
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

u32 Memory::GetMainRAMSize() const
{
    return m_main_ram_size;
}

// The reset that follows clears the new RAM and maps it
void Memory::SetMainRAMSize(u32 size)
{
    if (IsValidPointer(m_state.main_ram) && (size == m_main_ram_size))
        return;

    SafeDeleteArray(m_state.main_ram);
    m_main_ram_size = size;
    m_state.main_ram = new u8[m_main_ram_size];
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

// Overlays win over the regions below them, then the earliest registered region wins
const Memory::DebugRegion* Memory::FindMappedRegion(u32 bus_address) const
{
    const DebugRegion* found = NULL;

    for (int i = 0; i < m_debug_region_count; i++)
    {
        const DebugRegion& region = m_debug_regions[i];
        u64 start = region.info.physical_base;
        u64 end = start + region.info.size;

        if ((region.info.flags & GT_DEBUG_REGION_MAPPED) == 0 || (u64)bus_address < start || (u64)bus_address >= end)
            continue;

        if ((region.info.flags & GT_DEBUG_REGION_OVERLAY) != 0)
            return &region;

        if (!IsValidPointer(found))
            found = &region;
    }

    return found;
}

Memory::DebugRegion* Memory::FindMappedRegion(u32 bus_address)
{
    DebugRegion* found = NULL;

    for (int i = 0; i < m_debug_region_count; i++)
    {
        DebugRegion& region = m_debug_regions[i];
        u64 start = region.info.physical_base;
        u64 end = start + region.info.size;

        if ((region.info.flags & GT_DEBUG_REGION_MAPPED) == 0 || (u64)bus_address < start || (u64)bus_address >= end)
            continue;

        if ((region.info.flags & GT_DEBUG_REGION_OVERLAY) != 0)
            return &region;

        if (!IsValidPointer(found))
            found = &region;
    }

    return found;
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

// Debugger reads never call a handler's emulated read, only its peek
GT_Debug_Memory_Status Memory::ReadRegion(const DebugRegion& region, u32 offset, u8& value) const
{
    if ((region.info.flags & GT_DEBUG_REGION_READABLE) == 0)
        return GT_DEBUG_MEMORY_UNAVAILABLE;

    if (IsValidPointer(region.peek8))
        value = region.peek8(region.device, offset);
    else if (IsValidPointer(region.read_data))
        value = region.read_data[offset];
    else
        return GT_DEBUG_MEMORY_UNAVAILABLE;

    return (region.info.flags & GT_DEBUG_REGION_WRITABLE) != 0 ? GT_DEBUG_MEMORY_VALID : GT_DEBUG_MEMORY_READ_ONLY;
}

// Unmapped and unreadable addresses float high like the I/O space
// Registers end the CPU batch, so polling them sees time move like a port does
u8 Memory::ReadBus(u32 bus_address, GT_Bus_Access_Context& context)
{
    const DebugRegion* region = FindMappedRegion(bus_address);

    if (!IsValidPointer(region) || (region->info.flags & GT_DEBUG_REGION_READABLE) == 0)
        return 0xFF;

    u32 offset = bus_address - region->info.physical_base;

    if ((region->info.flags & GT_DEBUG_REGION_MMIO) != 0)
        context.end_batch = true;

    if (IsValidPointer(region->read8))
        return region->read8(region->device, offset);

    if (IsValidPointer(region->read_data))
        return region->read_data[offset];

    return 0xFF;
}

void Memory::WriteBus(u32 bus_address, u8 value, GT_Bus_Access_Context& context)
{
    DebugRegion* region = FindMappedRegion(bus_address);

    if (!IsValidPointer(region) ||
        (!IsValidPointer(region->write_data) && !IsValidPointer(region->write8)) ||
        (region->info.flags & GT_DEBUG_REGION_WRITABLE) == 0)
        return;

    if ((region->info.flags & GT_DEBUG_REGION_MMIO) != 0)
        context.end_batch = true;

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
            UpdateCPUPage(page);
    }

    m_cpu_map_generation = m_map_generation;
}

void Memory::UpdateCPUPage(u32 page)
{
    u32 physical = page << 12;
    const DebugRegion* region = FindMappedSpan(physical, 0x1000);

    InitPointer(m_cpu_read_pages[page]);
    InitPointer(m_cpu_write_pages[page]);

    if (!IsValidPointer(region) ||
        (region->info.flags & (GT_DEBUG_REGION_MMIO | GT_DEBUG_REGION_VIDEO | GT_DEBUG_REGION_AUDIO)) != 0)
        return;

    u32 offset = NormalizePhysicalAddress(physical) - region->info.physical_base;

    if ((region->info.flags & GT_DEBUG_REGION_READABLE) != 0 && IsValidPointer(region->read_data))
        m_cpu_read_pages[page] = region->read_data + offset;

    if ((region->info.flags & GT_DEBUG_REGION_WRITABLE) != 0 && IsValidPointer(region->write_data))
        m_cpu_write_pages[page] = region->write_data + offset;
}

// Up to date host pages are patched in place, the new generation makes the CPU drop its cached pointers
void Memory::RemapRange(u32 base, u32 size)
{
    bool patch = m_cpu_map_generation == m_map_generation && IsValidPointer(m_cpu_read_pages) &&
        m_physical_address_mask == 0xFFFFFFFF && size != 0;

    m_map_generation++;
    m_debug_snapshot_id++;

    if (!patch)
        return;

    u32 last = (u32)(((u64)base + size - 1) >> 12);

    for (u32 page = base >> 12; page <= last; page++)
        UpdateCPUPage(page);

    m_cpu_map_generation = m_map_generation;
}

// Reset maps the boot ROM and the FM-R view, with the dictionary and CMOS window off
void Memory::ResetMapping()
{
    m_state.main_memory = false;
    m_state.boot_ram = false;
    m_state.dictionary = false;
    m_state.dictionary_bank = 0;
    ApplyMapping();
}

u8 Memory::ReadMappingControl(u16 port) const
{
    switch (port)
    {
        case 0x0404:
            return m_state.main_memory ? 0x80 : 0x00;
        case 0x0480:
            return (m_state.boot_ram ? 0x02 : 0x00) | (m_state.dictionary ? 0x01 : 0x00);
        case 0x0484:
            return m_state.dictionary_bank;
        default:
            return 0xFF;
    }
}

void Memory::WriteMappingControl(u16 port, u8 value)
{
    switch (port)
    {
        case 0x0404:
            m_state.main_memory = (value & 0x80) != 0;
            break;
        case 0x0480:
            m_state.boot_ram = (value & 0x02) != 0;
            m_state.dictionary = (value & 0x01) != 0;
            break;
        case 0x0484:
            m_state.dictionary_bank = value & 0x0F;
            break;
        default:
            return;
    }

    ApplyMapping();

    if (unlikely(IsValidPointer(m_trace_logger) &&
        m_trace_logger->IsEventEnabled(TRACE_SYSTEM, TRACE_SYSTEM_MEMORY_MAP)))
    {
        GT_Trace_Entry entry = {};
        entry.type = TRACE_SYSTEM;
        entry.event = TRACE_SYSTEM_MEMORY_MAP;
        entry.system.port = port;
        entry.system.value = value;
        entry.system.address = m_state.dictionary_bank;
        entry.system.flags = (m_state.main_memory ? 0x01 : 0x00) | (m_state.boot_ram ? 0x02 : 0x00) |
            (m_state.dictionary ? 0x04 : 0x00);
        m_trace_logger->TraceLog(entry);
    }
}

// 0404h bit 7 swaps C0000-EFFFF between the FM-R view and RAM, 0480h bit 1 swaps the boot ROM at F8000
// for RAM and 0480h bit 0 shows the dictionary and CMOS windows inside the FM-R view
void Memory::ApplyMapping()
{
    bool fmr = !m_state.main_memory;
    const DebugRegion* dictionary = FindRegion(GT_DEBUG_REGION_DICTIONARY_ROM);

    if (IsValidPointer(dictionary))
        SetRegionData(GT_DEBUG_REGION_DICTIONARY_ROM_LOW_WINDOW,
            dictionary->read_data + m_state.dictionary_bank * 0x8000, NULL);

    SetRegionMapped(GT_DEBUG_REGION_SYSTEM_ROM_LOW_ALIAS, !m_state.boot_ram);
    SetRegionMapped(GT_DEBUG_REGION_FMR_PLANES, fmr);
    SetRegionMapped(GT_DEBUG_REGION_FMR_TEXT, fmr);
    SetRegionMapped(GT_DEBUG_REGION_FMR_REGISTERS, fmr);
    SetRegionMapped(GT_DEBUG_REGION_FMR_VIEW, fmr);
    SetRegionMapped(GT_DEBUG_REGION_DICTIONARY_ROM_LOW_WINDOW, fmr && m_state.dictionary);
    SetRegionMapped(GT_DEBUG_REGION_CMOS_LOW_WINDOW, fmr && m_state.dictionary);
}

void Memory::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void Memory::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void Memory::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE_ARRAY(serializer, m_state.main_ram, m_main_ram_size);
    G_SERIALIZE_ARRAY(serializer, m_state.cmos, GT_CMOS_SIZE);
    G_SERIALIZE(serializer, m_state.main_memory);
    G_SERIALIZE(serializer, m_state.boot_ram);
    G_SERIALIZE(serializer, m_state.dictionary);
    G_SERIALIZE(serializer, m_state.dictionary_bank);
}

void Memory::SanitizeState()
{
    m_state.dictionary_bank &= 0x0F;
    ApplyMapping();
    m_debug_snapshot_id++;
}
