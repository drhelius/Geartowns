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

#include "sprite.h"
#include "../common/trace_logger.h"
#include "../common/state_serializer.h"

Sprite::Sprite()
{
    InitPointer(m_vram);
    InitPointer(m_sprite_ram);
    InitPointer(m_timing);
    InitPointer(m_trace_logger);
    m_entry_reciprocal = 0;
    memset(&m_state, 0, sizeof(m_state));
}

Sprite::~Sprite()
{
}

void Sprite::Init(u8* vram, const u8* sprite_ram, const GT_Machine_Video_Timing* timing)
{
    m_vram = vram;
    m_sprite_ram = sprite_ram;
    m_timing = timing;
    Reset();
}

void Sprite::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

void Sprite::Reset()
{
    memset(&m_state, 0, sizeof(m_state));
    m_state.clear_row = k_sprite_clear_rows;
    m_state.clear_half_clocks = m_timing->sprite_clear;
    m_state.entry_half_clocks = m_timing->sprite_entry_15khz;
    UpdateEntryReciprocal();
}

// DP1 is written in bit 7 and read back in bit 4
u8 Sprite::Read(u16 port) const
{
    if (port == 0x0450)
        return m_state.address;

    u8 value = m_state.registers[m_state.address];
    return m_state.address == k_sprite_display_page ? (u8)(value >> 3) : value;
}

void Sprite::Write(u16 port, u8 value)
{
    if (port == 0x0450)
    {
        m_state.address = value & 0x07;
        return;
    }

    m_state.registers[m_state.address] = value & k_sprite_register_masks[m_state.address];

    if (unlikely(IsValidPointer(m_trace_logger) &&
        m_trace_logger->IsEventEnabled(TRACE_SPRITE, TRACE_SPRITE_REGISTER)))
        TraceEvent(TRACE_SPRITE_REGISTER, value);
}

// Each transfer swaps the halves, clears the new work half and walks the index from the first entry to 1023
// The first index and the time per entry are latched here, later changes only affect the next transfer
void Sprite::StartTransfer(u64 clocks, bool high_line_rate)
{
    m_state.page = !m_state.page;
    m_state.busy = true;
    m_state.start_clocks = clocks;
    m_state.clear_half_clocks = m_timing->sprite_clear;
    m_state.entry_half_clocks = high_line_rate ? m_timing->sprite_entry_31khz : m_timing->sprite_entry_15khz;
    m_state.first_index = GetFirstIndex();
    m_state.clear_row = 1;
    m_state.entry = 0;
    m_state.pixel = 0;
    UpdateEntryReciprocal();

    if (unlikely(IsValidPointer(m_trace_logger) &&
        m_trace_logger->IsEventEnabled(TRACE_SPRITE, TRACE_SPRITE_TRANSFER_START)))
        TraceEvent(TRACE_SPRITE_TRANSFER_START, 0);
}

// The previous transfer still runs, so this frame starts none and the shown half stays the same
void Sprite::TraceBusyAtVSync()
{
    if (IsValidPointer(m_trace_logger) && m_trace_logger->IsEventEnabled(TRACE_SPRITE, TRACE_SPRITE_BUSY_AT_VSYNC))
        TraceEvent(TRACE_SPRITE_BUSY_AT_VSYNC, 0);
}

// The clear and every index entry, drawn or not, take the model's time in half clocks
// with the entry's 256 pixels spread over its time
void Sprite::Run(u64 clocks)
{
    u64 elapsed = (clocks - m_state.start_clocks) * 2;
    u32 clear_time = m_state.clear_half_clocks;
    u32 entry_time = m_state.entry_half_clocks;

    // The first two lines of the work half hold the clear pattern, copied over the rest one VRAM row at a time
    if (m_state.clear_row < k_sprite_clear_rows)
    {
        u32 rows = MIN((u32)MIN(elapsed, (u64)clear_time) * k_sprite_clear_rows / clear_time + 1, k_sprite_clear_rows);
        u8* work = GetWorkHalf();

        for (; m_state.clear_row < rows; m_state.clear_row++)
            memcpy(work + m_state.clear_row * k_sprite_clear_row_size, work, k_sprite_clear_row_size);
    }

    if (elapsed < clear_time)
        return;

    u64 transfer = elapsed - clear_time;
    u32 count = k_sprite_entries - m_state.first_index;

    while (m_state.entry < count)
    {
        u64 offset = transfer - (u64)m_state.entry * entry_time;
        u32 end = offset >= entry_time ? k_sprite_pixels : (u32)(((offset << 8) * m_entry_reciprocal) >> 32);

        if (end <= m_state.pixel)
            return;

        if (m_state.pixel == 0)
            LoadEntry();

        DrawPixels(end);

        if (m_state.pixel < k_sprite_pixels)
            return;

        m_state.entry++;
        m_state.pixel = 0;
    }

    m_state.busy = false;

    if (unlikely(IsValidPointer(m_trace_logger) &&
        m_trace_logger->IsEventEnabled(TRACE_SPRITE, TRACE_SPRITE_TRANSFER_END)))
        TraceEvent(TRACE_SPRITE_TRANSFER_END, 0);
}

// Pixels due within an entry without dividing, exact while entries take at most 4096 half clocks
void Sprite::UpdateEntryReciprocal()
{
    m_entry_reciprocal = ((1ULL << 32) + m_state.entry_half_clocks - 1) / m_state.entry_half_clocks;
}

// The entry is read when its first pixel is due, later CPU writes to it wait for the next transfer
void Sprite::LoadEntry()
{
    const u8* entry = m_sprite_ram + ((u32)(m_state.first_index + m_state.entry) << 3);
    u32 x = read_u16_le(entry);
    u32 y = read_u16_le(entry + 2);
    m_state.entry_attributes = read_u16_le(entry + 4);
    m_state.entry_color = read_u16_le(entry + 6);

    if ((m_state.entry_attributes & k_sprite_attribute_offset) != 0)
    {
        x += ((m_state.registers[k_sprite_offset_x + 1] & 0x01) << 8) | m_state.registers[k_sprite_offset_x];
        y += ((m_state.registers[k_sprite_offset_y + 1] & 0x01) << 8) | m_state.registers[k_sprite_offset_y];
    }

    m_state.entry_x = x & 0x1FF;
    m_state.entry_y = y & 0x1FF;
}

void Sprite::DrawPixels(u32 end)
{
    u32 pixel = m_state.pixel;
    u32 x = m_state.entry_x;
    u32 y = m_state.entry_y;
    m_state.pixel = (u16)end;

    if ((m_state.entry_color & k_sprite_color_hide) != 0)
        return;

    // Coordinates wrap at 512, so an entry parked away from the 256x256 window writes nothing
    if ((x >= 256 && x + 15 < 512) || (y >= 256 && y + 15 < 512 + 2))
        return;

    if ((m_state.entry_color & k_sprite_color_table) != 0)
        DrawPattern<true>(pixel, end);
    else
        DrawPattern<false>(pixel, end);
}

// Pixels are walked in pattern order, so a halved sprite keeps the last opaque pixel that lands on each spot
// Lines 0 and 1 hold the clear pattern and are never drawn
// Transparent pixels store back what is there, as branching on pattern data mispredicts too often
template<bool indexed>
void Sprite::DrawPattern(u32 pixel, u32 end)
{
    u16 attributes = m_state.entry_attributes;
    u16 color = m_state.entry_color;
    u32 origin_x = m_state.entry_x;
    u32 origin_y = m_state.entry_y;
    const u8* pattern = m_sprite_ram + ((u32)(attributes & (indexed ? 0x03FF : 0x03FC)) << 7);
    const u8* table = m_sprite_ram + ((u32)(color & 0x0FFF) << 5);
    u16 through = (color & k_sprite_color_through) != 0 ? 0x8000 : 0x0000;
    bool swap = (attributes & k_sprite_attribute_swap) != 0;
    u32 flip_x = (attributes & k_sprite_attribute_flip_x) != 0 ? 0x0F : 0x00;
    u32 flip_y = (attributes & k_sprite_attribute_flip_y) != 0 ? 0x0F : 0x00;
    u32 shift_x = (attributes & k_sprite_attribute_half_x) != 0 ? 1 : 0;
    u32 shift_y = (attributes & k_sprite_attribute_half_y) != 0 ? 1 : 0;
    u8* work = GetWorkHalf();

    for (; pixel < end; pixel++)
    {
        u32 px = pixel & 0x0F;
        u32 py = pixel >> 4;
        u32 x = (origin_x + (((swap ? py : px) ^ flip_x) >> shift_x)) & 0x1FF;
        u32 y = (origin_y + (((swap ? px : py) ^ flip_y) >> shift_y)) & 0x1FF;

        if (x >= 256 || y < 2 || y >= 256)
            continue;

        u16 value;
        bool opaque;

        if (indexed)
        {
            u8 data = pattern[(py << 3) + (px >> 1)];
            u32 index = (px & 0x01) != 0 ? data >> 4 : data & 0x0F;
            value = read_u16_le(table + (index << 1));
            opaque = index != 0;
        }
        else
        {
            value = read_u16_le(pattern + (py << 5) + (px << 1));
            opaque = (value & 0x8000) == 0;
        }

        u8* destination = work + (y << 9) + (x << 1);
        u16 previous = read_u16_le(destination);
        write_u16_le(destination, opaque ? (u16)((value & 0x7FFF) | through) : previous);
    }
}

void Sprite::TraceEvent(u8 event, u8 value)
{
    u32 count = k_sprite_entries - m_state.first_index;
    GT_Trace_Entry entry = {};
    entry.type = TRACE_SPRITE;
    entry.event = event;
    entry.sprite.clocks = (m_state.clear_half_clocks + count * m_state.entry_half_clocks + 1) / 2;
    entry.sprite.first = event == TRACE_SPRITE_REGISTER ? GetFirstIndex() : m_state.first_index;
    entry.sprite.count = (u16)count;
    entry.sprite.entry = m_state.entry;
    entry.sprite.reg = m_state.address;
    entry.sprite.value = event == TRACE_SPRITE_REGISTER ? m_state.registers[m_state.address] : value;
    entry.sprite.raw = value;
    entry.sprite.page = m_state.page ? 1 : 0;
    m_trace_logger->TraceLog(entry);
}

void Sprite::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer, GT_SAVESTATE_VERSION);
}

void Sprite::LoadState(std::istream& stream, int version)
{
    StateSerializer serializer(stream);
    Serialize(serializer, version);
    SanitizeState();
}

void Sprite::Serialize(StateSerializer& serializer, int version)
{
    G_SERIALIZE_ARRAY(serializer, m_state.registers, SPRITE_REGISTERS);
    G_SERIALIZE(serializer, m_state.address);
    G_SERIALIZE(serializer, m_state.page);
    G_SERIALIZE(serializer, m_state.busy);
    G_SERIALIZE(serializer, m_state.start_clocks);
    G_SERIALIZE(serializer, m_state.first_index);
    G_SERIALIZE(serializer, m_state.clear_row);
    G_SERIALIZE(serializer, m_state.entry);
    G_SERIALIZE(serializer, m_state.pixel);
    G_SERIALIZE(serializer, m_state.entry_x);
    G_SERIALIZE(serializer, m_state.entry_y);
    G_SERIALIZE(serializer, m_state.entry_attributes);
    G_SERIALIZE(serializer, m_state.entry_color);

    // Older states ran every model at the Data Book's 32 + 75 us
    if (version >= 8)
    {
        G_SERIALIZE(serializer, m_state.clear_half_clocks);
        G_SERIALIZE(serializer, m_state.entry_half_clocks);
    }
    else
    {
        m_state.clear_half_clocks = k_sprite_book_clear_half_clocks;
        m_state.entry_half_clocks = k_sprite_book_entry_half_clocks;
    }
}

void Sprite::SanitizeState()
{
    for (int i = 0; i < SPRITE_REGISTERS; i++)
        m_state.registers[i] &= k_sprite_register_masks[i];

    m_state.address &= 0x07;
    m_state.first_index &= k_sprite_entries - 1;
    m_state.clear_row = (u16)CLAMP((u32)m_state.clear_row, 1U, k_sprite_clear_rows);
    m_state.entry = (u16)MIN((u32)m_state.entry, k_sprite_entries - m_state.first_index);
    m_state.pixel = (u16)MIN((u32)m_state.pixel, k_sprite_pixels - 1);
    m_state.entry_x &= 0x1FF;
    m_state.entry_y &= 0x1FF;
    m_state.clear_half_clocks = (u16)MAX((u32)m_state.clear_half_clocks, 1U);
    m_state.entry_half_clocks = (u16)CLAMP((u32)m_state.entry_half_clocks, 1U, k_sprite_max_entry_half_clocks);
    UpdateEntryReciprocal();
}
