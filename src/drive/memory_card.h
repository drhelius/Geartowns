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

#ifndef MEMORY_CARD_H
#define MEMORY_CARD_H

#include <iostream>
#include <vector>
#include "../common/common.h"

class Memory;
class StateSerializer;
class TraceLogger;

struct MemoryCardJournalEntry
{
    u64 write;
    u32 offset;
    u8 value;
};

class MemoryCard
{
public:
    struct MemoryCard_State
    {
        bool inserted;
        bool write_protected;
        bool dirty;
        bool change;
        u8 bank;
        bool attribute;
        u32 size;
    };

public:
    MemoryCard();
    ~MemoryCard();
    void Init(Memory* memory);
    void SetTraceLogger(TraceLogger* trace_logger);
    void SetReadCallback(void* context, GT_Read_Callback callback);
    void SetFeatures(u32 features);
    void SetWindowSize(u32 size);
    void Reset(bool power_on);
    bool Insert(const u8* data, u32 size, bool write_protected);
    void Eject();
    bool IsInserted() const;
    bool IsWriteProtected() const;
    void SetWriteProtected(bool write_protected);
    bool IsDirty() const;
    void ClearDirty();
    u32 GetWriteCount() const;
    u32 GetSize() const;
    const u8* GetImage() const;
    u64 GetSession() const;
    u64 GetLastWrite() const;
    bool Restore(u64 session, u64 write);
    bool Decodes(u16 port) const;
    u8 Read(u16 port);
    u8 Peek(u16 port) const;
    void Write(u16 port, u8 value);
    void UpdateWindows();
    u32 GetWindowBase(int window) const;
    static bool IsValidSize(u64 size);
    static u8 ReadWindowCallback(void* device, u32 offset);
    static void WriteWindowCallback(void* device, u32 offset, u8 value);
    static u8 ReadBankWindowCallback(void* device, u32 offset);
    static void WriteBankWindowCallback(void* device, u32 offset, u8 value);
    MemoryCard_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream, int version = GT_SAVESTATE_VERSION);

private:
    u8 ReadCard(u32 offset) const;
    void WriteCard(u32 offset, u8 value);
    void UpdateWindow(int id, int window);
    bool Undo(u64 write);
    bool Redo(u64 write);
    void Journal(u32 offset, u8 value, u64 write);
    void ClearJournal();
    void TraceEvent(u16 port, u8 value, u8 flags);
    void Serialize(StateSerializer& serializer, int version);
    void SanitizeState();

private:
    MemoryCard_State m_state;
    Memory* m_memory;
    TraceLogger* m_trace_logger;
    void* m_read_callback_context;
    GT_Read_Callback m_read_callback;
    u32 m_features;
    u32 m_window_size;
    u32 m_window_base[2];
    u8* m_image;
    u32 m_image_size;
    u32 m_write_count;
    u64 m_session;
    u64 m_next_write;
    u64 m_last_write;
    u64 m_journal_base;
    std::vector<MemoryCardJournalEntry> m_journal;
    u32 m_journal_start;
    u32 m_journal_count;
    std::vector<MemoryCardJournalEntry> m_redo;
};

static const u32 k_memory_card_max_size = 64 * 1024 * 1024;
static const u32 k_memory_card_size_unit = 1024;
static const u32 k_memory_card_journal_entries = (32 * 1024 * 1024) / sizeof(MemoryCardJournalEntry);
static const u8 k_memory_card_status_change = 0x80;
static const u8 k_memory_card_status_absent = 0x06;
static const u8 k_memory_card_status_write_protect = 0x01;

#include "memory_card_inline.h"

#endif /* MEMORY_CARD_H */
