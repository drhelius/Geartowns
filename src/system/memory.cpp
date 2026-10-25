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

static const u8 k_memory_oak2_index[219] =
{
/* 0x0800 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x0810 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4F, 0x41, 0x4B, 0x30, 0x30, 0x32,
/* 0x0820 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x0830 */ 0x00, 0x00, 0x00, 0x40, 0x00, 0x17, 0xC0, 0x00, 0x00, 0x80, 0x00, 0xA0, 0x08, 0x00, 0x00, 0x00,
/* 0x0840 */ 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x02, 0x20, 0x02, 0x20, 0x03, 0xA0,
/* 0x0850 */ 0x03, 0xA0, 0x03, 0xA0, 0x03, 0xA0, 0x03, 0xA0, 0x05, 0x20, 0x05, 0x3C, 0x05, 0x46, 0x05, 0x46,
/* 0x0860 */ 0x06, 0xC6, 0x06, 0xC6, 0x06, 0xC6, 0x06, 0xC6, 0x06, 0xC6, 0x06, 0xC6, 0x08, 0x46, 0x08, 0x46,
/* 0x0870 */ 0x08, 0x46, 0x08, 0x46, 0x08, 0x46, 0x08, 0x46, 0x08, 0x46, 0x08, 0x46, 0x08, 0x46, 0x08, 0x65,
/* 0x0880 */ 0x08, 0x65, 0x08, 0x65, 0x09, 0xF1, 0x09, 0xF1, 0x09, 0xF1, 0x09, 0xF1, 0x09, 0xF1, 0x09, 0xF1,
/* 0x0890 */ 0x09, 0xF1, 0x09, 0xF1, 0x09, 0xF1, 0x09, 0xF1, 0x09, 0xF1, 0x0B, 0x71, 0x0B, 0x71, 0x0B, 0x71,
/* 0x08A0 */ 0x0B, 0x71, 0x0B, 0x71, 0x0B, 0x71, 0x0B, 0x71, 0x0B, 0x71, 0x0B, 0x71, 0x0B, 0x71, 0x0B, 0x71,
/* 0x08B0 */ 0x0B, 0x71, 0x0B, 0x71, 0x0B, 0x71, 0x0B, 0x71, 0x0B, 0x71, 0x0B, 0x71, 0x0B, 0x71, 0x0B, 0x71,
/* 0x08C0 */ 0x00, 0x00, 0xA0, 0x86, 0x02, 0x20, 0x88, 0x03, 0xA0, 0x8D, 0x05, 0x20, 0x91, 0x06, 0xC6, 0x97,
/* 0x08D0 */ 0x08, 0x46, 0xA3, 0x09, 0xF1, 0xAE, 0x0B, 0x71, 0xFF, 0x0C, 0xF1
};

static const u8 k_memory_oak2_words_0[38] =
{
/* 0x0D60 */ 0x8D, 0xB7, 0x88, 0x8D, 0xB8, 0x83, 0xAC, 0x8A, 0xCA, 0x08, 0x2F, 0x77, 0xC1, 0x40, 0x8D, 0xB7,
/* 0x0D70 */ 0x88, 0x8D, 0xB8, 0x83, 0x91, 0x94, 0xCA, 0x08, 0x2F, 0x76, 0xC1, 0x40, 0x8E, 0x9D, 0xBE, 0xAF,
/* 0x0D80 */ 0xCA, 0x04, 0x5A, 0x65, 0xD4, 0x40
};

static const u8 k_memory_oak2_words_1[43] =
{
/* 0x1086 */ 0x9F, 0x95, 0xA8, 0x8D, 0xBE, 0x8D, 0xB7, 0x83, 0xAE, 0xB8, 0x83, 0xCA, 0x0B, 0x2F, 0x7C, 0xC1,
/* 0x1096 */ 0x40, 0x9F, 0x95, 0xA8, 0x8D, 0xBE, 0x86, 0x82, 0x8D, 0xCA, 0x08, 0x2F, 0x7B, 0xC1, 0x40, 0xA2,
/* 0x10A6 */ 0x82, 0x8F, 0x82, 0xCA, 0x04, 0x4A, 0x3F, 0x40, 0x2E, 0xD4, 0x40
};

static const u8 k_memory_cmos_directory[60] =
{
/* 0x0000 */ 0x01, 0xFF, 0x42, 0x4F, 0x4F, 0x54, 0xA8, 0x00, 0x40, 0x00, 0x01, 0xFE, 0x53, 0x45, 0x54, 0x55,
/* 0x0010 */ 0xE8, 0x00, 0x00, 0x01, 0x01, 0xFD, 0x4C, 0x4F, 0x47, 0x20, 0xE8, 0x01, 0x10, 0x03, 0x01, 0xFC,
/* 0x0020 */ 0x4F, 0x41, 0x53, 0x59, 0xF8, 0x04, 0x20, 0x00, 0x01, 0xFB, 0x58, 0x45, 0x4E, 0x49, 0x18, 0x05,
/* 0x0030 */ 0x10, 0x00, 0x01, 0xFA, 0x54, 0x4F, 0x57, 0x4E, 0x28, 0x05, 0x00, 0x01
};

static const u8 k_memory_cmos_header[10] =
{
/* 0x00A0 */ 0x28, 0x06, 0x99, 0x01, 0xB5, 0x7F, 0x79, 0x41, 0xFF, 0xFF
};

static const u8 k_memory_cmos_setup[113] =
{
/* 0x00E8 */ 0x95, 0x78, 0x8E, 0x6D, 0x92, 0xCA, 0x00, 0x00, 0x00, 0x01, 0x05, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0x00F8 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0x0108 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x02, 0x19, 0xF4, 0x01, 0xF4, 0x01, 0x02, 0x19,
/* 0x0118 */ 0xF4, 0x01, 0xF4, 0x01, 0x02, 0x19, 0xF4, 0x01, 0xF4, 0x01, 0x02, 0x19, 0xF4, 0x01, 0xF4, 0x01,
/* 0x0128 */ 0x02, 0x19, 0xF4, 0x01, 0xF4, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x03, 0x10, 0x00,
/* 0x0138 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x0148 */ 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
/* 0x0158 */ 0x02
};

static const u8 k_memory_cmos_towns[21] =
{
/* 0x05A8 */ 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 0x05B8 */ 0x00, 0x09, 0x00, 0x00, 0x09
};

static const u8 k_memory_cmos_tail[15] =
{
/* 0x07D2 */ 0x01, 0x02, 0x03, 0x04, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const u32 k_memory_cmos_setup_offset = 0x00E8;
static const u32 k_memory_cmos_setup_size = 0x100;
static const int k_memory_cmos_drive_count = 16;
static const int k_memory_cmos_first_hard_disk_drive = 3;

Memory::Memory()
{
    InitPointer(m_cpu_read_pages);
    InitPointer(m_cpu_write_pages);
    InitPointer(m_cpu_page_regions);
    InitPointer(m_cpu_page_bus);
    m_cpu_map_generation = 0;
    m_debug_region_count = 0;
    m_physical_address_mask = 0xFFFFFFFF;
    m_features = 0;
    SetDMAWindows(NULL);
    m_nmi_vector_protect = false;
    m_backup_protect = 0;
    m_map_generation = 1;
    m_region_generation = 1;
    m_debug_snapshot_id = 1;
    InitPointer(m_working_ram);
    m_working_ram_size = 0;
    m_main_ram_size = GT_MAIN_RAM_SIZE;
    InitPointer(m_state.main_ram);
    ResetCMOS(GT_MAX_FLOPPY_DRIVES);
    m_state.main_memory = false;
    m_state.boot_ram = false;
    m_state.dictionary = false;
    m_state.dictionary_bank = 0;
    m_state.card_bank = 0;
    m_state.card_attribute = false;
    memset(m_state.backup_ram, 0, sizeof(m_state.backup_ram));
    InitPointer(m_video_ram);
    m_video_ram_size = 0;
    InitPointer(m_trace_logger);
    InitPointer(m_access_context);
    memset(m_debug_regions, 0, sizeof(m_debug_regions));
#if !defined(GT_DISABLE_DYNAREC)
    InitPointer(m_code_pages);
    m_code_page_count = 0;
    InitPointer(m_code_write_context);
    InitPointer(m_code_write_callback);
#endif
}

Memory::~Memory()
{
    SafeDeleteArray(m_cpu_read_pages);
    SafeDeleteArray(m_cpu_write_pages);
    SafeDeleteArray(m_cpu_page_regions);
    SafeDeleteArray(m_cpu_page_bus);
    SafeDeleteArray(m_state.main_ram);
#if !defined(GT_DISABLE_DYNAREC)
    SafeDeleteArray(m_code_pages);
#endif
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
    m_nmi_vector_protect = false;
    m_backup_protect = 0;
    m_state.card_bank = 0;
    m_state.card_attribute = false;

    if (IsValidPointer(m_state.main_ram))
        memset(m_state.main_ram, 0, m_main_ram_size);
}

// Blank CMOS with the stock OAK/2 user dictionary that Towns OS boot disks write with DICUTY
// Some titles load OAK/2 without that fallback and stop at a hidden DOS prompt when it is missing
// Two-drive machines also get the records the BIOS writes to a blank CMOS and a Towns OS SETUP record with
// single-drive mode off, IO.SYS replaces a missing one with single-drive mode on
void Memory::ResetCMOS(int floppy_drives)
{
    memset(m_state.cmos, 0, sizeof(m_state.cmos));
    memcpy(m_state.cmos + 0x0800, k_memory_oak2_index, sizeof(k_memory_oak2_index));
    memcpy(m_state.cmos + 0x0D60, k_memory_oak2_words_0, sizeof(k_memory_oak2_words_0));
    memcpy(m_state.cmos + 0x1086, k_memory_oak2_words_1, sizeof(k_memory_oak2_words_1));

    if (floppy_drives > 1)
        ResetCMOSSetup();
}

bool Memory::LoadCMOSFile(const u8* data, size_t size, int floppy_drives)
{
    int version = 0;

    if (size == GT_CMOS_FILE_SIZE)
    {
        if ((data[0] | (data[1] << 8)) != GT_CMOS_FILE_MAGIC)
            return false;

        version = data[2] | (data[3] << 8);
        data += GT_CMOS_FILE_HEADER_SIZE;
    }
    else if (size != GT_CMOS_SIZE)
        return false;

    memcpy(m_state.cmos, data, GT_CMOS_SIZE);

    if ((version < 1) && (floppy_drives > 1))
        ResetCMOSSetup();

    return true;
}

void Memory::SaveCMOSFile(u8* data) const
{
    data[0] = GT_CMOS_FILE_MAGIC & 0xFF;
    data[1] = GT_CMOS_FILE_MAGIC >> 8;
    data[2] = GT_CMOS_FILE_VERSION & 0xFF;
    data[3] = GT_CMOS_FILE_VERSION >> 8;
    memcpy(data + GT_CMOS_FILE_HEADER_SIZE, m_state.cmos, GT_CMOS_SIZE);
}

bool Memory::AssignCMOSDriveLetters(int unit, u16 partitions)
{
    u8* setup = m_state.cmos + k_memory_cmos_setup_offset;
    u8* drives = setup + 6;
    bool assigned = false;
    int drive = k_memory_cmos_first_hard_disk_drive;
    u8 sum = 0;

    for (u32 i = 0; i < k_memory_cmos_setup_size; i++)
        sum += setup[i];

    if ((sum != 0) || (memcmp(setup, k_memory_cmos_setup, 6) != 0))
        return false;

    for (int i = 0; i < k_memory_cmos_drive_count; i++)
    {
        u8 id = drives[(i * 2) + 1];

        if ((drives[i * 2] == 0x02) && ((id >> 4) == unit))
            partitions &= (u16)~(1 << (id & 0x0F));
    }

    for (int partition = 0; partition < 16; partition++)
    {
        if ((partitions & (1 << partition)) == 0)
            continue;

        while ((drive < k_memory_cmos_drive_count) && (drives[drive * 2] != 0xFF))
            drive++;

        if (drive >= k_memory_cmos_drive_count)
            break;

        drives[drive * 2] = 0x02;
        drives[(drive * 2) + 1] = (u8)((unit << 4) | partition);
        assigned = true;
    }

    if (!assigned)
        return false;

    sum = 0;

    for (u32 i = 0; i < k_memory_cmos_setup_size - 1; i++)
        sum += setup[i];

    setup[k_memory_cmos_setup_size - 1] = (u8)(0 - sum);
    return true;
}

void Memory::ResetCMOSSetup()
{
    u8* setup = m_state.cmos + k_memory_cmos_setup_offset;
    u32 header_sum = 0;
    u8 sum = 0;

    for (u32 i = 0; i < 0xA4; i += 2)
        header_sum += m_state.cmos[i] | (m_state.cmos[i + 1] << 8);

    // The BIOS clears every record unless the word at A4h is the sum of the words below it and A6h holds 4179h
    if ((u32)(m_state.cmos[0xA4] | (m_state.cmos[0xA5] << 8)) != (header_sum & 0xFFFF) ||
        m_state.cmos[0xA6] != 0x79 || m_state.cmos[0xA7] != 0x41)
    {
        memset(m_state.cmos, 0, 0x0800);
        memcpy(m_state.cmos, k_memory_cmos_directory, sizeof(k_memory_cmos_directory));
        memcpy(m_state.cmos + 0x00A0, k_memory_cmos_header, sizeof(k_memory_cmos_header));
        m_state.cmos[0x052E] = 0x02;
        memcpy(m_state.cmos + 0x05A8, k_memory_cmos_towns, sizeof(k_memory_cmos_towns));
        memcpy(m_state.cmos + 0x07D2, k_memory_cmos_tail, sizeof(k_memory_cmos_tail));
    }

    memset(setup, 0, k_memory_cmos_setup_size);
    memcpy(setup, k_memory_cmos_setup, sizeof(k_memory_cmos_setup));

    for (u32 i = 0; i < k_memory_cmos_setup_size - 1; i++)
        sum += setup[i];

    setup[k_memory_cmos_setup_size - 1] = (u8)(0 - sum);
}

INLINE bool Memory::IsCanonicalPage(u32 page) const
{
    return (m_physical_address_mask & 0xFFF) == 0xFFF && ((page << 12) & ~m_physical_address_mask) == 0;
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

INLINE const Memory::DebugRegion* Memory::FindPageSpan(u32 physical, u32 size) const
{
    if (m_cpu_map_generation == m_map_generation && size != 0 && (u64)(physical & 0xFFF) + size <= 0x1000)
    {
        u8 index = m_cpu_page_regions[physical >> 12];

        if (index != 0)
            return &m_debug_regions[index - 1];
    }

    return FindMappedSpan(physical, size);
}

INLINE bool Memory::IsHandlerSpan(const DebugRegion* region, u32 offset, u32 size) const
{
    return offset < region->handler_offset + region->handler_size && offset + size > region->handler_offset;
}

// WRPROT keeps every bus master from writing the NMI vector in main RAM
INLINE bool Memory::IsProtectedSpan(const DebugRegion* region, u32 offset, u32 size) const
{
    return offset < k_memory_nmi_vector + k_memory_nmi_vector_size && offset + size > k_memory_nmi_vector &&
        m_nmi_vector_protect && region->info.id == GT_DEBUG_REGION_MAIN_RAM;
}

#if !defined(GT_DISABLE_DYNAREC)
// Writes that reach RAM pages holding translated code outside the CPU fast path
INLINE void Memory::NotifyCodeWrite(const DebugRegion* region, u32 offset, u32 size)
{
    if (region->write_data != m_working_ram || !IsValidPointer(m_code_pages))
        return;

    u32 first = offset >> 12;
    u32 last = (offset + size - 1) >> 12;

    for (u32 page = first; page <= last && page < m_code_page_count; page++)
    {
        if (m_code_pages[page] != 0)
        {
            m_code_write_callback(m_code_write_context, offset, size);
            return;
        }
    }
}
#endif

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
        const DebugRegion* region = FindPageSpan(physical, 2);

        if (IsValidPointer(region) && (region->info.flags & GT_DEBUG_REGION_WRITABLE) != 0)
        {
            u32 offset = NormalizePhysicalAddress(physical) - region->info.physical_base;

            if (IsValidPointer(region->write_data) && !IsHandlerSpan(region, offset, 2) && !IsProtectedSpan(region, offset, 2))
            {
                write_u16_le(region->write_data + offset, value);
                m_debug_snapshot_id += 2;
#if !defined(GT_DISABLE_DYNAREC)
                NotifyCodeWrite(region, offset, 2);
#endif
                return;
            }

            if (IsValidPointer(region->write16))
            {
                GT_Bus_Access_Context* outer = m_access_context;
                m_access_context = &context;
                region->write16(region->device, offset, value);
                m_access_context = outer;
                m_debug_snapshot_id += 2;
                return;
            }
        }
    }

    Write8Physical(physical, (u8)value, context);
    Write8Physical(physical + 1, (u8)(value >> 8), context);
}

void Memory::Write32Physical(u32 physical, u32 value, GT_Bus_Access_Context& context)
{
    if (!IsValidPointer(context.observe_memory_write))
    {
        const DebugRegion* region = FindPageSpan(physical, 4);

        if (IsValidPointer(region) && (region->info.flags & GT_DEBUG_REGION_WRITABLE) != 0)
        {
            u32 offset = NormalizePhysicalAddress(physical) - region->info.physical_base;

            if (IsValidPointer(region->write_data) && !IsHandlerSpan(region, offset, 4) && !IsProtectedSpan(region, offset, 4))
            {
                write_u32_le(region->write_data + offset, value);
                m_debug_snapshot_id += 4;
#if !defined(GT_DISABLE_DYNAREC)
                NotifyCodeWrite(region, offset, 4);
#endif
                return;
            }

            if (IsValidPointer(region->write32))
            {
                GT_Bus_Access_Context* outer = m_access_context;
                m_access_context = &context;
                region->write32(region->device, offset, value);
                m_access_context = outer;
                m_debug_snapshot_id += 4;
                return;
            }
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
    m_region_generation++;
    m_debug_snapshot_id++;
#if !defined(GT_DISABLE_DYNAREC)
    SafeDeleteArray(m_code_pages);
    m_code_page_count = 0;
#endif
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
    m_region_generation++;
    m_debug_snapshot_id++;
    return true;
}

// Handler regions only get host pages for the data given with SetRegionData, other CPU accesses reach the device
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
    m_region_generation++;
    m_debug_snapshot_id++;
    return true;
}

bool Memory::SetRegionWideHandlers(int id, GT_Memory_Write16_Fn write16, GT_Memory_Write32_Fn write32)
{
    DebugRegion* region = FindRegion(id);

    if (!IsValidPointer(region) || !IsValidPointer(region->write8))
        return false;

    region->write16 = write16;
    region->write32 = write32;
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
        ((region->info.flags & GT_DEBUG_REGION_READABLE) != 0 && !IsValidPointer(read_data) &&
        !IsValidPointer(region->read8)) ||
        ((region->info.flags & GT_DEBUG_REGION_WRITABLE) != 0 && !IsValidPointer(write_data) &&
        !IsValidPointer(region->write8)))
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

// CPU writes to these pages, and reads too when asked, reach the handlers even though the region has host data
bool Memory::SetRegionHandlerRange(int id, u32 offset, u32 size, bool reads)
{
    DebugRegion* region = FindRegion(id);

    if (size == 0)
        offset = 0;

    if (!IsValidPointer(region) ||
        ((offset | size) & 0xFFF) != 0 ||
        (u64)offset + size > region->info.size ||
        ((region->info.flags & GT_DEBUG_REGION_READABLE) != 0 && !IsValidPointer(region->read8)) ||
        ((region->info.flags & GT_DEBUG_REGION_WRITABLE) != 0 && !IsValidPointer(region->write8)))
        return false;

    if (region->handler_offset == offset && region->handler_size == size && region->handler_reads == reads)
        return true;

    u32 old_offset = region->handler_offset;
    u32 old_size = region->handler_size;
    region->handler_offset = offset;
    region->handler_size = size;
    region->handler_reads = reads;

    if ((region->info.flags & GT_DEBUG_REGION_MAPPED) == 0)
    {
        m_debug_snapshot_id++;
        return true;
    }

    if (old_size != 0)
        RemapRange(region->info.physical_base + old_offset, old_size);

    if (size != 0 && (offset != old_offset || size != old_size))
        RemapRange(region->info.physical_base + offset, size);

    return true;
}

// A handler that must see its access in time moves machine time up to it first
// Debugger accesses have no context and stay at the current machine time
void Memory::SynchronizeAccess()
{
    GT_Bus_Access_Context* context = m_access_context;

    if (IsValidPointer(context) && IsValidPointer(context->synchronize_memory))
        context->synchronize_memory(context->synchronize_context, *context);
}

void Memory::SynchronizeRegister(GT_Bus_Access_Context& context)
{
    context.end_batch = true;

    if (IsValidPointer(context.synchronize_memory))
        context.synchronize_memory(context.synchronize_context, context);
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
#if !defined(GT_DISABLE_DYNAREC)
    NotifyCodeWrite(region, offset, size);
#endif
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

    const DebugRegion* region = FindPageSpan(physical, size);

    if (!IsValidPointer(region) || !IsValidPointer(region->read_data) ||
        (region->info.flags & GT_DEBUG_REGION_READABLE) == 0)
        return NULL;

    u32 offset = NormalizePhysicalAddress(physical) - region->info.physical_base;

    if (region->handler_reads && IsHandlerSpan(region, offset, size))
        return NULL;

    return region->read_data + offset;
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
        {
            region->write_data[offset] = data[i];
#if !defined(GT_DISABLE_DYNAREC)
            NotifyCodeWrite(region, offset, 1);
#endif
        }
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
        {
            region->write_data[offset] = data[i];
#if !defined(GT_DISABLE_DYNAREC)
            NotifyCodeWrite(region, offset, 1);
#endif
        }
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

void Memory::SetDMAWindows(const GT_Machine_DMA_Window* windows)
{
    for (u32 slot = 0; slot < k_memory_dma_slots; slot++)
        m_dma_map[slot] = slot << 20;

    for (int i = 0; IsValidPointer(windows) && (windows[i].count != 0); i++)
    {
        for (u32 slot = 0; slot < windows[i].count; slot++)
            m_dma_map[(windows[i].slot + slot) & (k_memory_dma_slots - 1)] = windows[i].base;
    }
}

void Memory::SetNMIVectorProtect(bool protect)
{
    if (m_nmi_vector_protect == protect)
        return;

    m_nmi_vector_protect = protect;
    RemapRange(k_memory_nmi_vector & ~0xFFFU, 0x1000);
}

void Memory::SetBackupRAMProtect(u8 protect)
{
    m_backup_protect = protect;
}

u8 Memory::ReadBackupRAMCallback(void* device, u32 offset)
{
    Memory* memory = (Memory*)device;
    return (offset & 0x01) != 0 ? 0xFF : memory->m_state.backup_ram[offset >> 1];
}

void Memory::WriteBackupRAMCallback(void* device, u32 offset, u8 value)
{
    Memory* memory = (Memory*)device;

    if ((offset & 0x01) != 0 || (memory->m_backup_protect & (0x02 << (offset >> 13))) != 0)
        return;

    memory->m_state.backup_ram[offset >> 1] = value;
}

u32 Memory::GetRegionGeneration() const
{
    return m_region_generation;
}

#if !defined(GT_DISABLE_DYNAREC)
void Memory::SetCodeWriteCallback(void* context, GT_Memory_Code_Write_Fn callback)
{
    m_code_write_context = context;
    m_code_write_callback = callback;
}

// A RAM page with translated code keeps no CPU write pointer, so every CPU write to it reaches NotifyCodeWrite
// An up to date host page is patched in place, the new generation makes the CPU drop its cached pointers
void Memory::SetCodePage(u32 page, bool code)
{
    if (!IsValidPointer(m_code_pages) || page >= m_code_page_count || (m_code_pages[page] != 0) == code)
        return;

    m_code_pages[page] = code ? 1 : 0;

    const DebugRegion* ram = FindRegion(GT_DEBUG_REGION_MAIN_RAM);
    bool patch = m_cpu_map_generation == m_map_generation && IsValidPointer(m_cpu_read_pages) && IsValidPointer(ram);

    m_map_generation++;

    if (!patch)
        return;

    u32 cpu_page = (ram->info.physical_base >> 12) + page;

    if (IsCanonicalPage(cpu_page))
        UpdateCPUPage(cpu_page);

    m_cpu_map_generation = m_map_generation;
}

// Host memory of a ROM region, which no write ever reaches
bool Memory::IsReadOnlyHost(const u8* host) const
{
    for (int i = 0; i < m_debug_region_count; i++)
    {
        const DebugRegion& region = m_debug_regions[i];

        if ((region.info.flags & GT_DEBUG_REGION_ROM) != 0 && IsValidPointer(region.read_data) &&
            !IsValidPointer(region.write_data) && host >= region.read_data && host < region.read_data + region.info.size)
            return true;
    }

    return false;
}
#endif

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
// Registers move machine time up to the start of their instruction and end the CPU batch
u8 Memory::ReadBus(u32 bus_address, GT_Bus_Access_Context& context)
{
    const DebugRegion* region = FindPageSpan(bus_address, 1);

    if (!IsValidPointer(region) || (region->info.flags & GT_DEBUG_REGION_READABLE) == 0)
        return 0xFF;

    u32 offset = bus_address - region->info.physical_base;

    if ((region->info.flags & GT_DEBUG_REGION_MMIO) != 0)
        SynchronizeRegister(context);

    if (IsValidPointer(region->read8))
    {
        GT_Bus_Access_Context* outer = m_access_context;
        m_access_context = &context;
        u8 value = region->read8(region->device, offset);
        m_access_context = outer;
        return value;
    }

    if (IsValidPointer(region->read_data))
        return region->read_data[offset];

    return 0xFF;
}

void Memory::WriteBus(u32 bus_address, u8 value, GT_Bus_Access_Context& context)
{
    const DebugRegion* region = FindPageSpan(bus_address, 1);

    if (!IsValidPointer(region) ||
        (!IsValidPointer(region->write_data) && !IsValidPointer(region->write8)) ||
        (region->info.flags & GT_DEBUG_REGION_WRITABLE) == 0)
        return;

    if ((region->info.flags & GT_DEBUG_REGION_MMIO) != 0)
        SynchronizeRegister(context);

    u32 offset = bus_address - region->info.physical_base;

    if (IsValidPointer(context.observe_memory_write))
    {
        u8 previous = 0;
        ReadRegion(*region, offset, previous);
        context.observe_memory_write(context.memory_write_context, bus_address, previous, value);
    }

    if (IsValidPointer(region->write8))
    {
        GT_Bus_Access_Context* outer = m_access_context;
        m_access_context = &context;
        region->write8(region->device, offset, value);
        m_access_context = outer;
    }
    else if (!IsProtectedSpan(region, offset, 1))
    {
        region->write_data[offset] = value;
#if !defined(GT_DISABLE_DYNAREC)
        NotifyCodeWrite(region, offset, 1);
#endif
    }

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

#if !defined(GT_DISABLE_DYNAREC)
    u32 code_pages = (u32)((m_working_ram_size + 0xFFF) >> 12);

    if (code_pages != m_code_page_count)
    {
        SafeDeleteArray(m_code_pages);
        m_code_page_count = code_pages;

        if (code_pages != 0)
        {
            m_code_pages = new u8[code_pages];
            memset(m_code_pages, 0, code_pages);
        }
    }
#endif

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
        m_cpu_page_regions = new u8[page_count];
        m_cpu_page_bus = new u8[page_count];
    }

    memset(m_cpu_read_pages, 0, page_count * sizeof(m_cpu_read_pages[0]));
    memset(m_cpu_write_pages, 0, page_count * sizeof(m_cpu_write_pages[0]));
    memset(m_cpu_page_regions, 0, page_count);
    memset(m_cpu_page_bus, 0, page_count);

    for (u32 page = 0; page <= (m_physical_address_mask >> 12); page++)
    {
        if (IsCanonicalPage(page))
            UpdateCPUPage(page);
    }

    m_cpu_map_generation = m_map_generation;
}

void Memory::UpdateCPUPage(u32 page)
{
    UpdateCPUPage(page, FindMappedSpan(page << 12, 0x1000));
}

void Memory::UpdateCPUPage(u32 page, const DebugRegion* region)
{
    u32 physical = page << 12;

    m_cpu_page_regions[page] = IsValidPointer(region) ? (u8)(region - m_debug_regions + 1) : 0;
    m_cpu_page_bus[page] = IsValidPointer(region) ? GetBusClass(region->info) : (u8)GT_BUS_NONE;
    InitPointer(m_cpu_read_pages[page]);
    InitPointer(m_cpu_write_pages[page]);

    if (!IsValidPointer(region) || (region->info.flags & (GT_DEBUG_REGION_MMIO | GT_DEBUG_REGION_AUDIO)) != 0)
        return;

    u32 offset = NormalizePhysicalAddress(physical) - region->info.physical_base;
    bool handler = IsHandlerSpan(region, offset, 0x1000);

    if ((region->info.flags & GT_DEBUG_REGION_READABLE) != 0 && IsValidPointer(region->read_data) &&
        !(handler && region->handler_reads))
        m_cpu_read_pages[page] = region->read_data + offset;

    if ((region->info.flags & GT_DEBUG_REGION_WRITABLE) != 0 && IsValidPointer(region->write_data) && !handler &&
        !IsProtectedSpan(region, offset, 0x1000))
    {
#if !defined(GT_DISABLE_DYNAREC)
        if (region->write_data == m_working_ram && IsValidPointer(m_code_pages) && (offset >> 12) < m_code_page_count &&
            m_code_pages[offset >> 12] != 0)
            return;
#endif
        m_cpu_write_pages[page] = region->write_data + offset;
    }
}

u8 Memory::GetBusClass(const GT_Debug_Memory_Region& info)
{
    switch (info.id)
    {
        case GT_DEBUG_REGION_MAIN_RAM:
            return GT_BUS_DRAM;
        case GT_DEBUG_REGION_VRAM:
        case GT_DEBUG_REGION_VRAM_TWO_PAGE:
        case GT_DEBUG_REGION_VRAM_SINGLE_PAGE:
            return GT_BUS_VRAM;
        case GT_DEBUG_REGION_SPRITE_RAM:
            return GT_BUS_SPRITE;
        case GT_DEBUG_REGION_SYSTEM_ROM:
        case GT_DEBUG_REGION_SYSTEM_ROM_LOW_ALIAS:
        case GT_DEBUG_REGION_OS_ROM:
        case GT_DEBUG_REGION_FONT_ROM:
        case GT_DEBUG_REGION_FONT20_ROM:
        case GT_DEBUG_REGION_DICTIONARY_ROM:
        case GT_DEBUG_REGION_DICTIONARY_ROM_LOW_WINDOW:
        case GT_DEBUG_REGION_EXTENSION_ROM:
            return GT_BUS_ROM;
        case GT_DEBUG_REGION_CMOS:
        case GT_DEBUG_REGION_CMOS_LOW_WINDOW:
        case GT_DEBUG_REGION_BACKUP_RAM:
            return GT_BUS_CMOS;
        case GT_DEBUG_REGION_FMR_PLANES:
        case GT_DEBUG_REGION_FMR_TEXT:
            return GT_BUS_FMR;
        case GT_DEBUG_REGION_PCM_WINDOW:
            return GT_BUS_PCM;
        default:
            return (info.flags & GT_DEBUG_REGION_MMIO) != 0 ? (u8)GT_BUS_MMIO : (u8)GT_BUS_NONE;
    }
}

// Up to date host pages are patched in place, the new generation makes the CPU drop its cached pointers
void Memory::RemapRange(u32 base, u32 size)
{
    bool patch = m_cpu_map_generation == m_map_generation && IsValidPointer(m_cpu_read_pages) && size != 0;

    m_map_generation++;
    m_debug_snapshot_id++;

    if (!patch)
        return;

    u32 last = (u32)(((u64)base + size - 1) >> 12);
    const DebugRegion* region = ((base | size) & 0xFFF) == 0 ? FindMappedSpan(base, size) : NULL;

    for (u32 page = base >> 12; page <= last; page++)
    {
        if (!IsCanonicalPage(page))
            continue;

        if (IsValidPointer(region))
            UpdateCPUPage(page, region);
        else
            UpdateCPUPage(page);
    }

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

void Memory::SetFeatures(u32 features)
{
    m_features = features;
}

bool Memory::Decodes(u16 port) const
{
    switch (port)
    {
        case 0x0490:
        case 0x0491:
            return (m_features & GT_MACHINE_FEATURE_JEIDA4) != 0;
        default:
            return true;
    }
}

u8 Memory::ReadMappingControl(u16 port) const
{
    switch (port)
    {
        case 0x0490:
            return Decodes(port) ? m_state.card_bank : 0xFF;
        case 0x0491:
            return Decodes(port) ? (m_state.card_attribute ? 0x01 : 0x00) : 0xFF;
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
        case 0x0490:
            if (Decodes(port))
                m_state.card_bank = value & 0x3F;
            return;
        case 0x0491:
            if (Decodes(port))
                m_state.card_attribute = (value & 0x01) != 0;
            return;
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
    Serialize(serializer, GT_SAVESTATE_VERSION);
}

void Memory::LoadState(std::istream& stream, int version)
{
    StateSerializer serializer(stream);
    Serialize(serializer, version);
    SanitizeState();
}

void Memory::Serialize(StateSerializer& serializer, int version)
{
    UNUSED(version);
    G_SERIALIZE_ARRAY(serializer, m_state.main_ram, m_main_ram_size);
    G_SERIALIZE_ARRAY(serializer, m_state.cmos, GT_CMOS_SIZE);
    G_SERIALIZE(serializer, m_state.main_memory);
    G_SERIALIZE(serializer, m_state.boot_ram);
    G_SERIALIZE(serializer, m_state.dictionary);
    G_SERIALIZE(serializer, m_state.dictionary_bank);
    G_SERIALIZE(serializer, m_state.card_bank);
    G_SERIALIZE(serializer, m_state.card_attribute);
    G_SERIALIZE_ARRAY(serializer, m_state.backup_ram, k_memory_backup_ram_size);
}

void Memory::SanitizeState()
{
    m_state.dictionary_bank &= 0x0F;
    m_state.card_bank &= 0x3F;
    ApplyMapping();
    m_debug_snapshot_id++;
}
