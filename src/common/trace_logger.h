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

#ifndef TRACE_LOGGER_H
#define TRACE_LOGGER_H

#include "common.h"

#define GT_TRACE_DEFAULT_CAPACITY 100000
#define GT_TRACE_NAME_SIZE 56

enum GT_Trace_Type
{
    TRACE_CPU = 0,
    TRACE_INTERRUPT,
    TRACE_IO,
    TRACE_DMA,
    TRACE_CDROM,
    TRACE_FDC,
    TRACE_VIDEO,
    TRACE_TYPE_COUNT
};

#define TRACE_FLAG(type) (1U << (type))
#define TRACE_FLAG_ALL ((1U << TRACE_TYPE_COUNT) - 1)

enum GT_Trace_Interrupt_Event
{
    TRACE_INTERRUPT_REQUEST = 0,
    TRACE_INTERRUPT_ENTER
};

enum GT_Trace_DMA_Event
{
    TRACE_DMA_REQUEST = 0,
    TRACE_DMA_END
};

enum GT_Trace_CDROM_Event
{
    TRACE_CDROM_COMMAND = 0,
    TRACE_CDROM_STATUS
};

enum GT_Trace_FDC_Event
{
    TRACE_FDC_COMMAND = 0,
    TRACE_FDC_END
};

enum GT_Trace_Video_Event
{
    TRACE_VIDEO_VSYNC = 0
};

struct GT_Trace_Entry
{
    u64 cycle;
    u8 type;
    u8 event;
    union
    {
        struct
        {
            u32 linear;
            u32 eip;
            u32 registers[8];
            u32 eflags;
            u16 cs;
            u8 mode;
            u8 size;
            u8 bytes[15];
            char name[GT_TRACE_NAME_SIZE];
        } cpu;

        struct
        {
            u32 from;
            u32 to;
            u32 error_code;
            u8 vector;
            u8 source;
            u8 line;
            u8 has_error_code;
        } interrupt;

        struct
        {
            u32 pc;
            u32 value;
            u16 port;
            u8 size;
            u8 write;
        } io;

        struct
        {
            u32 address;
            u16 count;
            u8 channel;
            u8 mode;
            u8 terminal;
        } dma;

        struct
        {
            u8 bytes[8];
            u8 command;
        } cdrom;

        struct
        {
            u8 command;
            u8 track;
            u8 sector;
            u8 data;
            u8 status;
            s8 drive;
        } fdc;

        struct
        {
            u32 frame;
        } video;
    };
};

class TraceLogger
{
public:
    TraceLogger();
    ~TraceLogger();
    void Init(const u64* clocks);
    void Clear();
    bool SetCapacity(u32 capacity);
    u32 GetCapacity() const;
    void Start(u32 flags);
    void Stop();
    bool IsRunning() const;
    void SetActive(bool active);
    u32 GetFlags() const;
    INLINE bool IsEnabled(int type) const;
    INLINE GT_Trace_Entry* Record(int type, u8 event);
    u32 GetCount() const;
    u64 GetSequence() const;
    const GT_Trace_Entry& GetEntry(u32 index) const;

private:
    void UpdateActiveFlags();

private:
    GT_Trace_Entry* m_buffer;
    const u64* m_clocks;
    u32 m_capacity;
    u32 m_position;
    u32 m_count;
    u32 m_flags;
    u32 m_active_flags;
    bool m_running;
    bool m_active;
    u64 m_sequence;
};

// Events are only recorded while the debugger runs the machine and the logger is started
INLINE bool TraceLogger::IsEnabled(int type) const
{
    return (m_active_flags & TRACE_FLAG(type)) != 0;
}

INLINE GT_Trace_Entry* TraceLogger::Record(int type, u8 event)
{
    GT_Trace_Entry* entry = &m_buffer[m_position];
    entry->cycle = *m_clocks;
    entry->type = (u8)type;
    entry->event = event;

    m_position = m_position + 1 == m_capacity ? 0 : m_position + 1;

    if (m_count < m_capacity)
        m_count++;

    m_sequence++;
    return entry;
}

#endif /* TRACE_LOGGER_H */
