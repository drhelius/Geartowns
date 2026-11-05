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

#ifndef HIRES_CRTC_H
#define HIRES_CRTC_H

#include <iostream>
#include "../common/common.h"

#define HIRES_CRTC_REGISTER_COUNT 0x300
#define HIRES_CRTC_PALETTE_COUNT 4
#define HIRES_CRTC_PALETTE_SIZE 256
#define HIRES_CRTC_CURSOR_PATTERN_SIZE 1024

static const u16 k_hires_crtc_select = 0x000;
static const u16 k_hires_crtc_clock = 0x001;
static const u16 k_hires_crtc_control = 0x004;
static const u16 k_hires_crtc_priority = 0x005;
static const u16 k_hires_crtc_status = 0x006;
static const u16 k_hires_crtc_hsw1 = 0x100;
static const u16 k_hires_crtc_hst = 0x101;
static const u16 k_hires_crtc_vsw1 = 0x102;
static const u16 k_hires_crtc_vst = 0x103;
static const u16 k_hires_crtc_hds = 0x104;
static const u16 k_hires_crtc_hde = 0x105;
static const u16 k_hires_crtc_vds = 0x106;
static const u16 k_hires_crtc_vde = 0x107;
static const u16 k_hires_crtc_page0 = 0x110;
static const u16 k_hires_crtc_page1 = 0x120;
static const u16 k_hires_crtc_palette_select = 0x130;
static const u16 k_hires_crtc_palette_index = 0x132;
static const u16 k_hires_crtc_palette_data = 0x133;
static const u16 k_hires_crtc_cursor_x = 0x200;
static const u16 k_hires_crtc_cursor_y = 0x201;
static const u16 k_hires_crtc_cursor_hidden_x = 0x202;
static const u16 k_hires_crtc_cursor_hidden_y = 0x203;
static const u16 k_hires_crtc_cursor_back = 0x204;
static const u16 k_hires_crtc_cursor_front = 0x205;
static const u16 k_hires_crtc_cursor_on = 0x206;
static const u16 k_hires_crtc_cursor_address = 0x208;
static const u16 k_hires_crtc_cursor_data = 0x209;
static const u16 k_hires_crtc_cursor_zoom = 0x20A;

static const u32 k_hires_crtc_vram_size = 0x100000;
static const u32 k_hires_crtc_page_size = 0x80000;

// The CPU's view in pack mode: 3 bytes of the whole VRAM as 4
static const u32 k_hires_crtc_pack_view_size = 0x156000;

class StateSerializer;

class HiresCRTC
{
public:
    struct HiresCRTC_State
    {
        u32 registers[HIRES_CRTC_REGISTER_COUNT];
        u32 palette[HIRES_CRTC_PALETTE_COUNT][HIRES_CRTC_PALETTE_SIZE];
        u32 palette_latch;
        u8 cursor_pattern[HIRES_CRTC_CURSOR_PATTERN_SIZE];
        u32 gateway_register0;
        u16 address;
        u16 gateway_address;
        u16 window[4];
    };

private:
    struct Layer
    {
        const u8* page;
        const u32* palette;
        u32 page_mask;
        u32 row_offset;
        u32 x;
        u32 vram_width;
        u32 zoom;
        u32 key_mask;
        u32 key;
    };

public:
    HiresCRTC();
    void Init(u8* vram, const u32* direct_colors);
    void Reset();
    u8 Read(u16 port, bool vsync) const;
    void Write(u16 port, u8 value);
    bool IsActive() const;
    u32 GetWord(u16 reg) const;
    void LatchWindow();
    u32 GetClockRate();
    u32 PeekClockRate() const;
    bool IsTwoPage() const;
    bool IsPackMode() const;
    u8 ReadOnePage(u32 offset) const;
    void WriteOnePage(u32 offset, u8 value);
    void RenderRow(u32* destination, int row, int width) const;
    HiresCRTC_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream, int version = GT_SAVESTATE_VERSION);

private:
    u32 ReadRegister(bool vsync) const;
    void WriteRegister(u32 lane, u8 value);
    void WriteControl(u8 value);
    void WritePalette(u32 lane, u8 value);
    void WriteCursorPattern(u8 value);
    u32 GetPaletteSelect() const;
    void UpdateColor(u32 select, u32 index);
    void UpdateColors();
    void RenderLayer(u32* destination, int page, int row, int width, bool keyed) const;
    void RenderCursor(u32* destination, int row, int width) const;
    template<int bits, bool keyed>
    void RenderPixels(u32* destination, int count, const Layer& layer) const;
    template<int bits, bool keyed>
    void RenderZoomedPixels(u32* destination, int count, const Layer& layer) const;
    template<int bits, bool keyed>
    void RenderRun(u32* destination, u32 count, const u8* source, const Layer& layer) const;
    template<bool keyed>
    void StorePixel(u32* destination, u32 pixel, u32 color, const Layer& layer) const;
    template<int bits>
    u32 FetchPixel(const Layer& layer, u32 x) const;
    template<int bits>
    u32 GetPixelColor(const Layer& layer, u32 pixel) const;
    void Serialize(StateSerializer& serializer, int version);

private:
    HiresCRTC_State m_state;
    u8* m_vram;
    const u32* m_direct_colors;
    u32 m_colors[HIRES_CRTC_PALETTE_COUNT][HIRES_CRTC_PALETTE_SIZE];
    u32 m_unknown_clock;
};

#endif /* HIRES_CRTC_H */
