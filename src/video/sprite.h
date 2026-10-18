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

#ifndef SPRITE_H
#define SPRITE_H

#include <iostream>
#include "../common/common.h"

#define SPRITE_REGISTERS 8

class StateSerializer;
class TraceLogger;

class Sprite
{
public:
    struct Sprite_State
    {
        u8 registers[SPRITE_REGISTERS];
        u8 address;
        bool page;
        bool busy;
        u64 start_clocks;
        u16 clear_half_clocks;
        u16 entry_half_clocks;
        u16 first_index;
        u16 clear_row;
        u16 entry;
        u16 pixel;
        u16 entry_x;
        u16 entry_y;
        u16 entry_attributes;
        u16 entry_color;
    };

public:
    Sprite();
    ~Sprite();
    void Init(u8* vram, const u8* sprite_ram, const GT_Machine_Video_Timing* timing);
    void SetTraceLogger(TraceLogger* trace_logger);
    void Reset();
    u8 Read(u16 port) const;
    void Write(u16 port, u8 value);
    void Synchronize(u64 clocks);
    void StartTransfer(u64 clocks, bool high_line_rate);
    void TraceBusyAtVSync();
    bool IsEnabled() const;
    bool IsBusy() const;
    u64 GetFinishClocks() const;
    bool GetPage() const;
    u32 GetDisplayOffset() const;
    u32 GetWorkOffset() const;
    Sprite_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream, int version = GT_SAVESTATE_VERSION);

private:
    void Run(u64 clocks);
    void UpdateEntryReciprocal();
    void LoadEntry();
    void DrawPixels(u32 end);
    template<bool indexed>
    void DrawPattern(u32 pixel, u32 end);
    u16 GetFirstIndex() const;
    u8* GetWorkHalf() const;
    void TraceEvent(u8 event, u8 value);
    void Serialize(StateSerializer& serializer, int version);
    void SanitizeState();

private:
    Sprite_State m_state;
    u8* m_vram;
    const u8* m_sprite_ram;
    const GT_Machine_Video_Timing* m_timing;
    TraceLogger* m_trace_logger;
    u64 m_entry_reciprocal;
};

static const u8 k_sprite_register_masks[SPRITE_REGISTERS] = { 0xFF, 0x83, 0xFF, 0x01, 0xFF, 0x01, 0x88, 0x00 };
static const int k_sprite_control0 = 0x00;
static const int k_sprite_control1 = 0x01;
static const int k_sprite_offset_x = 0x02;
static const int k_sprite_offset_y = 0x04;
static const int k_sprite_display_page = 0x06;
static const u16 k_sprite_attribute_offset = 0x8000;
static const u16 k_sprite_attribute_swap = 0x4000;
static const u16 k_sprite_attribute_flip_x = 0x2000;
static const u16 k_sprite_attribute_flip_y = 0x1000;
static const u16 k_sprite_attribute_half_y = 0x0800;
static const u16 k_sprite_attribute_half_x = 0x0400;
static const u16 k_sprite_color_table = 0x8000;
static const u16 k_sprite_color_through = 0x4000;
static const u16 k_sprite_color_hide = 0x2000;
static const u32 k_sprite_entries = 1024;
static const u32 k_sprite_pixels = 256;
static const u32 k_sprite_work_base = 0x40000;
static const u32 k_sprite_half_size = 0x20000;
static const u32 k_sprite_clear_rows = 128;
static const u32 k_sprite_clear_row_size = k_sprite_half_size / k_sprite_clear_rows;
static const u32 k_sprite_max_entry_half_clocks = 4096;
static const u16 k_sprite_book_clear_half_clocks = (GT_MACHINE_CLOCK_RATE / 500000) * 32;
static const u16 k_sprite_book_entry_half_clocks = (GT_MACHINE_CLOCK_RATE / 500000) * 75;

#include "sprite_inline.h"

#endif /* SPRITE_H */
