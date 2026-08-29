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

#include "gui_debug_memory_provider.h"

#include "emu.h"
#include "firmware.h"
#include "geartowns.h"

static const int DEBUG_MEMORY_MAX_TRANSACTION_SIZE = 0x100000;

static int firmware_region_id(GT_Firmware_Type type)
{
    switch (type)
    {
        case GT_FIRMWARE_SYSTEM: return GT_DEBUG_REGION_SYSTEM_ROM;
        case GT_FIRMWARE_OS: return GT_DEBUG_REGION_OS_ROM;
        case GT_FIRMWARE_FONT: return GT_DEBUG_REGION_FONT_ROM;
        case GT_FIRMWARE_DICTIONARY: return GT_DEBUG_REGION_DICTIONARY_ROM;
        case GT_FIRMWARE_FONT20: return GT_DEBUG_REGION_FONT20_ROM;
        default: return 0;
    }
}

static const u8* firmware_region_data(Firmware* firmware, GT_Firmware_Type type)
{
    if (!IsValidPointer(firmware))
        return NULL;

    switch (type)
    {
        case GT_FIRMWARE_SYSTEM: return firmware->GetSystemRom();
        case GT_FIRMWARE_OS: return firmware->GetOsRom();
        case GT_FIRMWARE_FONT: return firmware->GetFontRom();
        case GT_FIRMWARE_DICTIONARY: return firmware->GetDictionaryRom();
        case GT_FIRMWARE_FONT20: return firmware->GetFont20Rom();
        default: return NULL;
    }
}

DebugMemoryProvider::DebugMemoryProvider()
{
    m_last_message[0] = 0;
    m_request_undo = false;
    m_request_redo = false;
    m_changed = false;
}

DebugMemoryProvider::~DebugMemoryProvider()
{
}

void DebugMemoryProvider::Init()
{
    Reset();
}

void DebugMemoryProvider::Reset()
{
    m_pending.clear();
    m_undo.clear();
    m_redo.clear();
    m_last_message[0] = 0;
    m_request_undo = false;
    m_request_redo = false;
    m_changed = true;
}

void DebugMemoryProvider::Update()
{
    if (m_request_undo)
    {
        m_request_undo = false;
        if (!m_undo.empty())
        {
            WriteTransaction transaction = m_undo.back();
            m_undo.pop_back();
            std::vector<u8> after = transaction.after;
            transaction.after = transaction.before;
            if (ApplyTransaction(transaction, false))
            {
                transaction.after = after;
                m_redo.push_back(transaction);
                SetMessage("Memory edit undone");
            }
        }
    }

    if (m_request_redo)
    {
        m_request_redo = false;
        if (!m_redo.empty())
        {
            WriteTransaction transaction = m_redo.back();
            m_redo.pop_back();
            if (ApplyTransaction(transaction, false))
            {
                m_undo.push_back(transaction);
                SetMessage("Memory edit redone");
            }
        }
    }

    for (size_t i = 0; i < m_pending.size(); i++)
    {
        WriteTransaction transaction = m_pending[i];
        if (ApplyTransaction(transaction, true))
        {
            m_undo.push_back(transaction);
            m_redo.clear();
            SetMessage("Memory edit applied at an emulation safe point");
        }
    }
    m_pending.clear();
}

int DebugMemoryProvider::GetRegionCount() const
{
    GeartownsCore* core = emu_get_core();
    if (!IsValidPointer(core))
        return 0;

    int count = 0;
    Memory* memory = core->GetMemory();
    if (IsValidPointer(memory))
        count += memory->GetDebugRegionCount();

    Firmware* firmware = core->GetFirmware();
    if (IsValidPointer(firmware) && firmware->IsReady())
        count += GT_FIRMWARE_COUNT;

    Media* media = core->GetMedia();
    if (!emu_is_media_loading() && IsValidPointer(media) && media->IsReady())
        count++;
    return count;
}

bool DebugMemoryProvider::GetRegion(int index, GT_Debug_Memory_Region& region) const
{
    GeartownsCore* core = emu_get_core();
    if (!IsValidPointer(core) || index < 0)
        return false;

    Memory* memory = core->GetMemory();
    int memory_count = IsValidPointer(memory) ? memory->GetDebugRegionCount() : 0;
    if (index < memory_count)
        return memory->GetDebugRegion(index, region);
    return GetExternalRegion(index - memory_count, region);
}

bool DebugMemoryProvider::GetRegionById(int id, GT_Debug_Memory_Region& region) const
{
    int count = GetRegionCount();
    for (int i = 0; i < count; i++)
    {
        if (GetRegion(i, region) && region.id == id)
            return true;
    }
    return false;
}

u32 DebugMemoryProvider::GetAddressLimit(
    const GT_Debug_Memory_Address& address) const
{
    if (address.space == GT_DEBUG_MEMORY_IO)
        return 0xFFFF;
    if (address.space != GT_DEBUG_MEMORY_REGION)
        return 0xFFFFFFFF;

    GT_Debug_Memory_Region region;
    if (!GetRegionById(address.region, region) || region.size == 0)
        return 0;
    return region.size - 1;
}

void DebugMemoryProvider::ReadBlock(const GT_Debug_Memory_Address& address,
    u8* data, GT_Debug_Memory_Status* status, u32 size,
    GT_Debug_Memory_Block_Info* info) const
{
    if (!IsValidPointer(data) || !IsValidPointer(status))
        return;

    if (IsValidPointer(info))
    {
        info->snapshot_id = GetSnapshotId();
        info->map_generation = GetMapGeneration();
    }

    memset(data, 0, size);
    for (u32 i = 0; i < size; i++)
        status[i] = GT_DEBUG_MEMORY_UNAVAILABLE;

    GeartownsCore* core = emu_get_core();
    if (!IsValidPointer(core))
        return;

    if (address.space == GT_DEBUG_MEMORY_REGION)
    {
        Memory* memory = core->GetMemory();
        bool memory_region = false;
        if (IsValidPointer(memory))
        {
            int region_count = memory->GetDebugRegionCount();
            for (int i = 0; i < region_count; i++)
            {
                GT_Debug_Memory_Region region;
                if (memory->GetDebugRegion(i, region) &&
                    region.id == address.region)
                {
                    memory_region = true;
                    break;
                }
            }
        }
        if (memory_region)
            memory->DebugReadRegionBlock(address.region, address.address,
                data, status, size);
        else
            ReadExternalRegion(address.region, address.address, data, status, size);
        return;
    }

    Memory* memory = core->GetMemory();
    I386* cpu = core->GetI386();
    if (address.space == GT_DEBUG_MEMORY_PHYSICAL && IsValidPointer(memory))
    {
        memory->DebugReadPhysicalBlock(address.address, data, status, size);
        return;
    }
    if (address.space == GT_DEBUG_MEMORY_BUS && IsValidPointer(memory))
    {
        memory->DebugReadBusBlock(address.address, data, status, size);
        return;
    }
    if (!IsValidPointer(cpu))
        return;

    for (u32 i = 0; i < size; i++)
    {
        u64 current = (u64)address.address + i;
        if (current > 0xFFFFFFFFULL)
        {
            status[i] = GT_DEBUG_MEMORY_UNMAPPED;
            continue;
        }

        bool valid = false;
        if (address.space == GT_DEBUG_MEMORY_LINEAR)
            valid = cpu->TryPeekLinear((u32)current, data[i]);
        else if (address.space == GT_DEBUG_MEMORY_LOGICAL)
        {
            if (address.segment_register >= 0)
            {
                valid = cpu->TryPeekLogical(
                    (I386_Segment_Register)address.segment_register,
                    (u32)current, data[i]);
            }
            else
                valid = cpu->TryPeekLogical(address.segment, (u32)current, data[i]);
        }
        status[i] = valid ? GT_DEBUG_MEMORY_VALID : GT_DEBUG_MEMORY_UNAVAILABLE;
    }
}

bool DebugMemoryProvider::Translate(const GT_Debug_Memory_Address& address,
    GT_Debug_Memory_Translation& translation) const
{
    memset(&translation, 0, sizeof(translation));
    GeartownsCore* core = emu_get_core();
    if (!IsValidPointer(core))
    {
        strncpy_fit(translation.reason, "Core is not available",
            sizeof(translation.reason));
        return false;
    }

    if (address.space == GT_DEBUG_MEMORY_REGION)
    {
        GT_Debug_Memory_Region region;
        if (!GetRegionById(address.region, region) || address.address >= region.size)
        {
            strncpy_fit(translation.reason, "Region offset is outside the region",
                sizeof(translation.reason));
            return false;
        }
        translation.region_valid = true;
        translation.region = region.id;
        translation.region_offset = address.address;
        strncpy_fit(translation.region_name, region.name,
            sizeof(translation.region_name));
        if ((region.flags & GT_DEBUG_REGION_MAPPED) != 0)
        {
            translation.bus_valid = true;
            translation.bus = region.physical_base + address.address;
        }
        return true;
    }

    Memory* memory = core->GetMemory();
    I386* cpu = core->GetI386();
    if (address.space == GT_DEBUG_MEMORY_PHYSICAL && IsValidPointer(memory))
        return memory->DebugTranslatePhysical(address.address, translation);
    if (address.space == GT_DEBUG_MEMORY_BUS && IsValidPointer(memory))
        return memory->DebugTranslateBus(address.address, translation);
    if (address.space == GT_DEBUG_MEMORY_LINEAR && IsValidPointer(cpu))
        return cpu->DebugTranslateLinear(address.address, translation);
    if (address.space == GT_DEBUG_MEMORY_LOGICAL && IsValidPointer(cpu))
    {
        if (address.segment_register >= 0)
        {
            return cpu->DebugTranslateLogical(
                (I386_Segment_Register)address.segment_register,
                address.address, translation);
        }
        return cpu->DebugTranslateLogical(address.segment, address.address,
            translation);
    }

    strncpy_fit(translation.reason, address.space == GT_DEBUG_MEMORY_IO ?
        "Passive I/O inspection is not implemented" :
        "Address translation is unavailable", sizeof(translation.reason));
    return false;
}

bool DebugMemoryProvider::QueueWrite(const GT_Debug_Memory_Address& address,
    const u8* data, u32 size)
{
    if (!IsValidPointer(data) || size == 0 ||
        size > DEBUG_MEMORY_MAX_TRANSACTION_SIZE)
    {
        SetMessage("Invalid or oversized memory edit");
        return false;
    }

    WriteTransaction transaction;
    transaction.address = address;
    transaction.after.assign(data, data + size);
    transaction.map_generation = GetMapGeneration();
    m_pending.push_back(transaction);
    SetMessage("Memory edit queued for the next safe point");
    return true;
}

void DebugMemoryProvider::RequestUndo()
{
    if (CanUndo())
        m_request_undo = true;
}

void DebugMemoryProvider::RequestRedo()
{
    if (CanRedo())
        m_request_redo = true;
}

bool DebugMemoryProvider::CanUndo() const
{
    return !m_undo.empty();
}

bool DebugMemoryProvider::CanRedo() const
{
    return !m_redo.empty();
}

bool DebugMemoryProvider::ConsumeChanged()
{
    bool changed = m_changed;
    m_changed = false;
    return changed;
}

const char* DebugMemoryProvider::GetLastMessage() const
{
    return m_last_message;
}

bool DebugMemoryProvider::GetRegisterValue(const char* name, u32& value) const
{
    GeartownsCore* core = emu_get_core();
    return IsValidPointer(core) && IsValidPointer(core->GetI386()) &&
        core->GetI386()->GetDebugRegisterValue(name, value);
}

const char* DebugMemoryProvider::GetSpaceName(GT_Debug_Memory_Space space)
{
    switch (space)
    {
        case GT_DEBUG_MEMORY_LOGICAL: return "Logical";
        case GT_DEBUG_MEMORY_LINEAR: return "Linear";
        case GT_DEBUG_MEMORY_PHYSICAL: return "Physical";
        case GT_DEBUG_MEMORY_BUS: return "Bus";
        case GT_DEBUG_MEMORY_REGION: return "Region";
        case GT_DEBUG_MEMORY_IO: return "I/O Ports";
        default: return "Unknown";
    }
}

const char* DebugMemoryProvider::GetStatusName(GT_Debug_Memory_Status status)
{
    switch (status)
    {
        case GT_DEBUG_MEMORY_VALID: return "Writable";
        case GT_DEBUG_MEMORY_READ_ONLY: return "Read-only";
        case GT_DEBUG_MEMORY_UNMAPPED: return "Unmapped";
        case GT_DEBUG_MEMORY_UNAVAILABLE: return "Passive read unavailable";
        default: return "Unknown";
    }
}

bool DebugMemoryProvider::GetExternalRegion(int index,
    GT_Debug_Memory_Region& region) const
{
    GeartownsCore* core = emu_get_core();
    if (!IsValidPointer(core) || index < 0)
        return false;

    Firmware* firmware = core->GetFirmware();
    if (IsValidPointer(firmware) && firmware->IsReady())
    {
        if (index < GT_FIRMWARE_COUNT)
        {
            GT_Firmware_Type type = (GT_Firmware_Type)index;
            memset(&region, 0, sizeof(region));
            region.id = firmware_region_id(type);
            strncpy_fit(region.name, Firmware::GetComponentName(type),
                sizeof(region.name));
            region.size = (u32)Firmware::GetExpectedSize(type);
            region.flags = GT_DEBUG_REGION_READABLE |
                GT_DEBUG_REGION_EXECUTABLE | GT_DEBUG_REGION_ROM;
            return true;
        }
        index -= GT_FIRMWARE_COUNT;
    }

    Media* media = core->GetMedia();
    if (index == 0 && !emu_is_media_loading() && IsValidPointer(media) &&
        media->IsReady())
    {
        memset(&region, 0, sizeof(region));
        region.id = GT_DEBUG_REGION_MEDIA_IMAGE;
        strncpy_fit(region.name, "Media Image", sizeof(region.name));
        region.size = (u32)media->GetSize();
        region.flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_ROM;
        return true;
    }
    return false;
}

bool DebugMemoryProvider::ReadExternalRegion(int id, u32 offset, u8* data,
    GT_Debug_Memory_Status* status, u32 size) const
{
    GeartownsCore* core = emu_get_core();
    if (!IsValidPointer(core))
        return false;

    const u8* source = NULL;
    u32 source_size = 0;
    Firmware* firmware = core->GetFirmware();
    if (IsValidPointer(firmware) && firmware->IsReady())
    {
        for (int i = 0; i < GT_FIRMWARE_COUNT; i++)
        {
            GT_Firmware_Type type = (GT_Firmware_Type)i;
            if (firmware_region_id(type) == id)
            {
                source = firmware_region_data(firmware, type);
                source_size = (u32)Firmware::GetExpectedSize(type);
                break;
            }
        }
    }

    if (id == GT_DEBUG_REGION_MEDIA_IMAGE && !emu_is_media_loading())
    {
        Media* media = core->GetMedia();
        if (IsValidPointer(media) && media->IsReady())
        {
            source = media->GetData();
            source_size = (u32)media->GetSize();
        }
    }

    for (u32 i = 0; i < size; i++)
    {
        u64 current = (u64)offset + i;
        if (IsValidPointer(source) && current < source_size)
        {
            data[i] = source[current];
            status[i] = GT_DEBUG_MEMORY_READ_ONLY;
        }
        else
        {
            data[i] = 0;
            status[i] = GT_DEBUG_MEMORY_UNMAPPED;
        }
    }
    return IsValidPointer(source);
}

bool DebugMemoryProvider::WriteNow(const GT_Debug_Memory_Address& address,
    const u8* data, u32 size)
{
    GeartownsCore* core = emu_get_core();
    if (!IsValidPointer(core) || !IsValidPointer(core->GetMemory()))
        return false;

    Memory* memory = core->GetMemory();
    if (address.space == GT_DEBUG_MEMORY_REGION)
        return memory->DebugWriteRegionBlock(address.region, address.address, data, size);
    if (address.space == GT_DEBUG_MEMORY_PHYSICAL)
        return memory->DebugWritePhysicalBlock(address.address, data, size);
    if (address.space == GT_DEBUG_MEMORY_BUS)
        return memory->DebugWriteBusBlock(address.address, data, size);
    return false;
}

bool DebugMemoryProvider::ApplyTransaction(WriteTransaction& transaction,
    bool capture_before)
{
    u32 size = (u32)transaction.after.size();
    if (size == 0 || !ValidateWritable(transaction.address, size))
    {
        SetMessage("Memory edit rejected: range is read-only or unavailable");
        return false;
    }
    if (transaction.map_generation != GetMapGeneration())
    {
        SetMessage("Memory edit rejected: the memory map changed");
        return false;
    }

    if (capture_before)
    {
        transaction.before.resize(size);
        std::vector<GT_Debug_Memory_Status> status(size);
        ReadBlock(transaction.address, &transaction.before[0], &status[0], size, NULL);
    }
    if (!WriteNow(transaction.address, &transaction.after[0], size))
    {
        SetMessage("Memory edit was not accepted by the selected source");
        return false;
    }
    m_changed = true;
    return true;
}

bool DebugMemoryProvider::ValidateWritable(
    const GT_Debug_Memory_Address& address, u32 size) const
{
    std::vector<u8> data(size);
    std::vector<GT_Debug_Memory_Status> status(size);
    ReadBlock(address, &data[0], &status[0], size, NULL);
    for (u32 i = 0; i < size; i++)
    {
        if (status[i] != GT_DEBUG_MEMORY_VALID)
            return false;
    }
    return true;
}

u64 DebugMemoryProvider::GetSnapshotId() const
{
    GeartownsCore* core = emu_get_core();
    if (!IsValidPointer(core))
        return 0;

    u64 id = 0;
    if (IsValidPointer(core->GetMemory()))
        id = core->GetMemory()->GetDebugSnapshotId();
    if (!emu_is_media_loading() && IsValidPointer(core->GetMedia()) &&
        core->GetMedia()->IsReady())
        id ^= ((u64)core->GetMedia()->GetCRC() << 32);
    return id;
}

u32 DebugMemoryProvider::GetMapGeneration() const
{
    GeartownsCore* core = emu_get_core();
    return IsValidPointer(core) && IsValidPointer(core->GetMemory()) ?
        core->GetMemory()->GetMapGeneration() : 0;
}

void DebugMemoryProvider::SetMessage(const char* message)
{
    strncpy_fit(m_last_message, message, sizeof(m_last_message));
}
