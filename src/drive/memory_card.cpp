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

#include <new>
#include <time.h>
#include "memory_card.h"
#include "../system/memory.h"
#include "../common/state_serializer.h"
#include "../common/trace_logger.h"

static u32 s_sessions = 0;

MemoryCard::MemoryCard()
{
    memset(&m_state, 0, sizeof(m_state));
    InitPointer(m_memory);
    InitPointer(m_trace_logger);
    InitPointer(m_read_callback_context);
    InitPointer(m_read_callback);
    m_features = 0;
    m_window_size = 0;
    m_window_base[0] = 0;
    m_window_base[1] = 0;
    InitPointer(m_image);
    m_image_size = 0;
    m_write_count = 0;
    m_session = 0;
    m_next_write = 1;
    m_last_write = 0;
    m_journal_base = 0;
    m_journal_start = 0;
    m_journal_count = 0;
}

MemoryCard::~MemoryCard()
{
    SafeDeleteArray(m_image);
}

void MemoryCard::Init(Memory* memory)
{
    m_memory = memory;
    Reset(true);
}

void MemoryCard::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

void MemoryCard::SetReadCallback(void* context, GT_Read_Callback callback)
{
    m_read_callback_context = context;
    m_read_callback = callback;
}

void MemoryCard::SetFeatures(u32 features)
{
    m_features = features;
}

void MemoryCard::SetWindowSize(u32 size)
{
    m_window_size = size;
}

void MemoryCard::Reset(bool power_on)
{
    if (power_on)
        m_state.change = false;

    m_state.bank = 0;
    m_state.attribute = false;
    UpdateWindows();
}

bool MemoryCard::Insert(const u8* data, u32 size, bool write_protected)
{
    if (!IsValidPointer(data) || !IsValidSize(size))
        return false;

    u32 image_size = (size + 0xFFF) & ~0xFFFU;
    u8* image = new (std::nothrow) u8[image_size];

    if (!IsValidPointer(image))
        return false;

    memcpy(image, data, size);
    memset(image + size, 0xFF, image_size - size);
    SafeDeleteArray(m_image);
    m_image = image;
    m_image_size = image_size;
    m_state.inserted = true;
    m_state.write_protected = write_protected;
    m_state.dirty = false;
    m_state.change = true;
    m_state.size = size;
    m_session = ((u64)time(NULL) << 20) + ++s_sessions;
    m_next_write = 1;
    m_last_write = 0;
    m_journal_base = 0;
    ClearJournal();
    std::vector<MemoryCardJournalEntry>().swap(m_redo);
    UpdateWindows();
    TraceEvent(0x048A, Peek(0x048A), 0x01);
    return true;
}

void MemoryCard::Eject()
{
    if (!m_state.inserted)
        return;

    SafeDeleteArray(m_image);
    m_image_size = 0;
    m_state.inserted = false;
    m_state.write_protected = false;
    m_state.dirty = false;
    m_state.change = true;
    m_state.size = 0;
    m_session = 0;
    m_next_write = 1;
    m_last_write = 0;
    m_journal_base = 0;
    ClearJournal();
    std::vector<MemoryCardJournalEntry>().swap(m_redo);
    UpdateWindows();
    TraceEvent(0x048A, Peek(0x048A), 0x02);
}

bool MemoryCard::IsValidSize(u64 size)
{
    return (size != 0) && (size <= k_memory_card_max_size) && ((size % k_memory_card_size_unit) == 0);
}

bool MemoryCard::Restore(u64 session, u64 write)
{
    if (!m_state.inserted || (session != m_session))
        return false;

    if (write == m_last_write)
        return true;

    return (write < m_last_write) ? Undo(write) : Redo(write);
}

bool MemoryCard::Decodes(u16 port) const
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

u8 MemoryCard::Read(u16 port)
{
    u8 value = Peek(port);

    if (port != 0x048A)
        return value;

    if (m_state.change)
    {
        m_state.change = false;
        TraceEvent(port, value, 0x00);
    }

    if (IsValidPointer(m_read_callback))
        m_read_callback(m_read_callback_context);

    return value;
}

u8 MemoryCard::Peek(u16 port) const
{
    switch (port)
    {
        case 0x048A:
        {
            u8 value = m_state.change ? k_memory_card_status_change : 0x00;

            if (!m_state.inserted)
                return value | k_memory_card_status_absent | k_memory_card_status_write_protect;

            return value | (m_state.write_protected ? k_memory_card_status_write_protect : 0x00);
        }
        case 0x0490:
            return Decodes(port) ? m_state.bank : 0xFF;
        case 0x0491:
            return Decodes(port) ? (m_state.attribute ? 0x01 : 0x00) : 0xFF;
        default:
            return 0xFF;
    }
}

void MemoryCard::Write(u16 port, u8 value)
{
    if (!Decodes(port))
        return;

    switch (port)
    {
        case 0x0490: m_state.bank = value & 0x3F; break;
        case 0x0491: m_state.attribute = (value & 0x01) != 0; break;
        default: return;
    }

    UpdateWindows();
    TraceEvent(port, value, 0x00);
}

void MemoryCard::UpdateWindows()
{
    bool jeida4 = (m_features & GT_MACHINE_FEATURE_JEIDA4) != 0;
    bool sx = m_window_size == 0x100000;
    m_window_base[0] = (jeida4 && sx) ? (u32)m_state.bank << 20 : 0;
    m_window_base[1] = (u32)((m_state.bank >> 4) & 0x03) << 24;
    UpdateWindow(GT_DEBUG_REGION_MEMORY_CARD_WINDOW, 0);
    UpdateWindow(GT_DEBUG_REGION_MEMORY_CARD_BANK_WINDOW, 1);
}

void MemoryCard::UpdateWindow(int id, int window)
{
    if (!IsValidPointer(m_memory) || (m_window_size == 0))
        return;

    u32 base = m_window_base[window];
    u32 valid = 0;

    if (m_state.inserted && !m_state.attribute && (base < m_image_size))
        valid = MIN(m_image_size - base, m_window_size);

    m_memory->SetRegionData(id, valid != 0 ? m_image + base : NULL, NULL);
    m_memory->SetRegionHandlerRange(id, valid, m_window_size - valid, true);
    m_memory->SetRegionMapped(id, m_state.inserted);
}

u8 MemoryCard::ReadWindowCallback(void* device, u32 offset)
{
    MemoryCard* card = static_cast<MemoryCard*>(device);
    return card->ReadCard(card->m_window_base[0] + offset);
}

void MemoryCard::WriteWindowCallback(void* device, u32 offset, u8 value)
{
    MemoryCard* card = static_cast<MemoryCard*>(device);
    card->WriteCard(card->m_window_base[0] + offset, value);
}

u8 MemoryCard::ReadBankWindowCallback(void* device, u32 offset)
{
    MemoryCard* card = static_cast<MemoryCard*>(device);
    return card->ReadCard(card->m_window_base[1] + offset);
}

void MemoryCard::WriteBankWindowCallback(void* device, u32 offset, u8 value)
{
    MemoryCard* card = static_cast<MemoryCard*>(device);
    card->WriteCard(card->m_window_base[1] + offset, value);
}

void MemoryCard::WriteCard(u32 offset, u8 value)
{
    if (!m_state.inserted || m_state.attribute || m_state.write_protected || (offset >= m_state.size))
        return;

    u8 old = m_image[offset];

    if (old == value)
        return;

    Journal(offset, old, m_next_write);
    m_redo.clear();
    m_last_write = m_next_write++;
    m_image[offset] = value;
    m_state.dirty = true;
    m_write_count++;
}

bool MemoryCard::Undo(u64 write)
{
    u32 newer = 0;

    while ((newer < m_journal_count) &&
        (m_journal[(m_journal_start + m_journal_count - 1 - newer) % k_memory_card_journal_entries].write > write))
        newer++;

    if (newer == m_journal_count ? write != m_journal_base :
        m_journal[(m_journal_start + m_journal_count - 1 - newer) % k_memory_card_journal_entries].write != write)
        return false;

    for (u32 i = 0; i < newer; i++)
    {
        m_journal_count--;
        const MemoryCardJournalEntry& entry =
            m_journal[(m_journal_start + m_journal_count) % k_memory_card_journal_entries];
        MemoryCardJournalEntry redo;
        redo.write = entry.write;
        redo.offset = entry.offset;
        redo.value = m_image[entry.offset];
        m_redo.push_back(redo);
        m_image[entry.offset] = entry.value;
    }

    if (newer != 0)
    {
        m_state.dirty = true;
        m_write_count++;

        if (IsValidPointer(m_memory))
            m_memory->InvalidateDebugSnapshot();
    }

    m_last_write = write;
    return true;
}

bool MemoryCard::Redo(u64 write)
{
    size_t count = m_redo.size();
    size_t older = 0;

    while ((older < count) && (m_redo[count - 1 - older].write < write))
        older++;

    if ((older == count) || (m_redo[count - 1 - older].write != write))
        return false;

    for (size_t i = 0; i <= older; i++)
    {
        MemoryCardJournalEntry entry = m_redo.back();
        m_redo.pop_back();
        Journal(entry.offset, m_image[entry.offset], entry.write);
        m_image[entry.offset] = entry.value;
    }

    m_state.dirty = true;
    m_write_count++;

    if (IsValidPointer(m_memory))
        m_memory->InvalidateDebugSnapshot();

    m_last_write = write;
    return true;
}

void MemoryCard::Journal(u32 offset, u8 value, u64 write)
{
    if (m_journal_count == k_memory_card_journal_entries)
    {
        m_journal_base = m_journal[m_journal_start].write;
        m_journal_start = (m_journal_start + 1) % k_memory_card_journal_entries;
        m_journal_count--;
    }

    u32 index = (m_journal_start + m_journal_count) % k_memory_card_journal_entries;

    if (index == m_journal.size())
        m_journal.push_back(MemoryCardJournalEntry());

    MemoryCardJournalEntry& entry = m_journal[index];
    entry.write = write;
    entry.offset = offset;
    entry.value = value;
    m_journal_count++;
}

void MemoryCard::ClearJournal()
{
    std::vector<MemoryCardJournalEntry>().swap(m_journal);
    m_journal_start = 0;
    m_journal_count = 0;
}

void MemoryCard::TraceEvent(u16 port, u8 value, u8 flags)
{
    if (!IsValidPointer(m_trace_logger) || !m_trace_logger->IsEventEnabled(TRACE_SYSTEM, TRACE_SYSTEM_MEMORY_CARD))
        return;

    GT_Trace_Entry entry = {};
    entry.type = TRACE_SYSTEM;
    entry.event = TRACE_SYSTEM_MEMORY_CARD;
    entry.system.port = port;
    entry.system.value = value;
    entry.system.address = m_state.bank;
    entry.system.flags = flags | (m_state.attribute ? 0x04 : 0x00);
    m_trace_logger->TraceLog(entry);
}

void MemoryCard::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer, GT_SAVESTATE_VERSION);
}

void MemoryCard::LoadState(std::istream& stream, int version)
{
    StateSerializer serializer(stream);
    Serialize(serializer, version);
    SanitizeState();
}

void MemoryCard::Serialize(StateSerializer& serializer, int version)
{
    if (version < 30)
    {
        m_state.change = false;
        m_state.bank = 0;
        m_state.attribute = false;
        return;
    }

    G_SERIALIZE(serializer, m_state.change);
    G_SERIALIZE(serializer, m_state.bank);
    G_SERIALIZE(serializer, m_state.attribute);
}

void MemoryCard::SanitizeState()
{
    m_state.bank &= 0x3F;
    UpdateWindows();
}
