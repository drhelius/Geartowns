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

#include <string.h>
#include "hires_crtc.h"
#include "../common/state_serializer.h"

// Register offsets from the start of a page's block
static const u16 k_hires_crtc_layer_haj = 0;
static const u16 k_hires_crtc_layer_w = 1;
static const u16 k_hires_crtc_layer_vaj = 2;
static const u16 k_hires_crtc_layer_h = 3;
static const u16 k_hires_crtc_layer_vram_w = 4;
static const u16 k_hires_crtc_layer_vram_h = 5;
static const u16 k_hires_crtc_layer_xoff = 6;
static const u16 k_hires_crtc_layer_yoff = 7;
static const u16 k_hires_crtc_layer_mode = 8;
static const u16 k_hires_crtc_layer_zoom = 9;
static const u16 k_hires_crtc_layer_mask = 11;
static const u16 k_hires_crtc_layer_key = 12;

static const u32 k_hires_crtc_palette_busy = 0x0200;
static const u32 k_hires_crtc_pack_enable = 0x04;
static const u32 k_hires_crtc_clock_rates[3] = { 25175000, 0, 65000000 };

static u32 SetLane(u32 data, u32 lane, u8 value)
{
    u32 shift = lane * 8;
    return (data & ~(0xFFU << shift)) | ((u32)value << shift);
}

HiresCRTC::HiresCRTC()
{
    memset(&m_state, 0, sizeof(m_state));
    InitPointer(m_vram);
    InitPointer(m_direct_colors);
    m_unknown_clock = 0;
}

void HiresCRTC::Init(u8* vram, const u32* direct_colors)
{
    m_vram = vram;
    m_direct_colors = direct_colors;
}

void HiresCRTC::Reset()
{
    memset(&m_state, 0, sizeof(m_state));
    m_state.registers[k_hires_crtc_page0 + k_hires_crtc_layer_mode] = 0x0100;
    m_state.registers[k_hires_crtc_page1 + k_hires_crtc_layer_mode] = 0x0100;
    UpdateColors();
}

u8 HiresCRTC::Read(u16 port, bool vsync) const
{
    switch (port)
    {
        case 0x0472:
            return (u8)m_state.address;
        case 0x0473:
            return (u8)(m_state.address >> 8);
        case 0x0474:
        case 0x0475:
        case 0x0476:
        case 0x0477:
            return (u8)(ReadRegister(vsync) >> ((port - 0x0474) * 8));
        case 0x047A:
            return (u8)m_state.gateway_address;
        case 0x047B:
            return (u8)(m_state.gateway_address >> 8);
        case 0x047C:
        case 0x047D:
        case 0x047E:
        case 0x047F:
            return m_state.gateway_address == 0 ? (u8)(m_state.gateway_register0 >> ((port - 0x047C) * 8)) : 0x00;
        default:
            return 0xFF;
    }
}

void HiresCRTC::Write(u16 port, u8 value)
{
    switch (port)
    {
        case 0x0472:
            m_state.address = (u16)((m_state.address & 0xFF00) | value);
            break;
        case 0x0473:
            m_state.address = (u16)((m_state.address & 0x00FF) | (value << 8));
            break;
        case 0x0474:
        case 0x0475:
        case 0x0476:
        case 0x0477:
            WriteRegister(port - 0x0474, value);
            break;
        case 0x047A:
            m_state.gateway_address = (u16)((m_state.gateway_address & 0xFF00) | value);
            break;
        case 0x047B:
            m_state.gateway_address = (u16)((m_state.gateway_address & 0x00FF) | (value << 8));
            break;
        case 0x047C:
        case 0x047D:
        case 0x047E:
        case 0x047F:
            if (m_state.gateway_address == 0)
                m_state.gateway_register0 = SetLane(m_state.gateway_register0, port - 0x047C, value);
            break;
        default:
            break;
    }
}

// The ROM selects it to program it and never starts it, so the compatibility CRTC keeps the screen until 004h = 1
bool HiresCRTC::IsActive() const
{
    return (m_state.registers[k_hires_crtc_select] & 0x01) != 0 && (m_state.registers[k_hires_crtc_control] & 0x01) != 0;
}

u32 HiresCRTC::GetWord(u16 reg) const
{
    return m_state.registers[reg] & 0xFFFF;
}

// The display window applies from the frame that starts next, and a state saved in between has to keep that
void HiresCRTC::LatchWindow()
{
    m_state.window[0] = (u16)GetWord(k_hires_crtc_hds);
    m_state.window[1] = (u16)GetWord(k_hires_crtc_hde);
    m_state.window[2] = (u16)GetWord(k_hires_crtc_vds);
    m_state.window[3] = (u16)GetWord(k_hires_crtc_vde);
}

u32 HiresCRTC::PeekClockRate() const
{
    u32 index = (m_state.registers[k_hires_crtc_clock] >> 4) & 0x0F;
    return index == 2 ? k_hires_crtc_clock_rates[2] : k_hires_crtc_clock_rates[0];
}

// Fujitsu's software only uses clocks 0 and 2
u32 HiresCRTC::GetClockRate()
{
    u32 index = (m_state.registers[k_hires_crtc_clock] >> 4) & 0x0F;

    if ((index == 0) || (index == 2))
    {
        m_unknown_clock = 0;
        return k_hires_crtc_clock_rates[index];
    }

    if (index != m_unknown_clock)
    {
        Log("High-res CRTC clock %u is unknown, using 25.175 MHz", index);
        m_unknown_clock = index;
    }

    return k_hires_crtc_clock_rates[0];
}

u32 HiresCRTC::ReadRegister(bool vsync) const
{
    u16 address = m_state.address;

    switch (address)
    {
        case k_hires_crtc_status:
            return vsync ? 0x02 : 0x00;
        case k_hires_crtc_palette_select:
            return m_state.registers[address] & ~k_hires_crtc_palette_busy;
        case k_hires_crtc_palette_data:
            return m_state.palette[GetPaletteSelect()][m_state.registers[k_hires_crtc_palette_index] & 0xFF];
        case k_hires_crtc_cursor_data:
            return m_state.cursor_pattern[m_state.registers[k_hires_crtc_cursor_address] &
                (HIRES_CRTC_CURSOR_PATTERN_SIZE - 1)];
        default:
            return address < HIRES_CRTC_REGISTER_COUNT ? m_state.registers[address] : 0;
    }
}

// The port only sees bytes, whatever the width of the OUT: wider writes to these registers arrive lane by lane
void HiresCRTC::WriteRegister(u32 lane, u8 value)
{
    u16 address = m_state.address;

    switch (address)
    {
        case k_hires_crtc_control:
            if (lane == 0)
                WriteControl(value);
            break;
        case k_hires_crtc_status:
            break;
        case k_hires_crtc_palette_data:
            WritePalette(lane, value);
            break;
        case k_hires_crtc_cursor_data:
            if (lane == 0)
                WriteCursorPattern(value);
            break;
        default:
            if (address < HIRES_CRTC_REGISTER_COUNT)
                m_state.registers[address] = SetLane(m_state.registers[address], lane, value);
            break;
    }
}

void HiresCRTC::WriteControl(u8 value)
{
    if ((value & 0x02) != 0)
        m_state.registers[k_hires_crtc_control] = 0;
    else if ((value & 0x01) != 0)
        m_state.registers[k_hires_crtc_control] = 1;
}

// DISPT8 writes a color as a word at 0474h, selects 133h again and writes green at 0476h, so the low half outlives
// any write to 0472h. Green completes the color, and a dword's last byte comes after it
void HiresCRTC::WritePalette(u32 lane, u8 value)
{
    m_state.palette_latch = SetLane(m_state.palette_latch, lane, value);

    if (lane != 2)
        return;

    u32 index = m_state.registers[k_hires_crtc_palette_index] & 0xFF;
    m_state.palette[GetPaletteSelect()][index] = m_state.palette_latch & 0x00FFFFFF;
    m_state.registers[k_hires_crtc_palette_index] = (index + 1) & 0xFF;
    UpdateColor(GetPaletteSelect(), index);
}

void HiresCRTC::WriteCursorPattern(u8 value)
{
    u32 address = m_state.registers[k_hires_crtc_cursor_address] & (HIRES_CRTC_CURSOR_PATTERN_SIZE - 1);
    m_state.cursor_pattern[address] = value;
    m_state.registers[k_hires_crtc_cursor_address] = (address + 1) & (HIRES_CRTC_CURSOR_PATTERN_SIZE - 1);
}

u32 HiresCRTC::GetPaletteSelect() const
{
    return m_state.registers[k_hires_crtc_palette_select] & (HIRES_CRTC_PALETTE_COUNT - 1);
}

// Palette entries and the cursor's colors hold blue, red and green from the low byte up
static u32 ToXRGB(u32 color)
{
    u32 blue = color & 0xFF;
    u32 red = (color >> 8) & 0xFF;
    u32 green = (color >> 16) & 0xFF;
    return 0xFF000000U | (red << 16) | (green << 8) | blue;
}

void HiresCRTC::UpdateColor(u32 select, u32 index)
{
    m_colors[select][index] = ToXRGB(m_state.palette[select][index]);
}

void HiresCRTC::UpdateColors()
{
    for (u32 select = 0; select < HIRES_CRTC_PALETTE_COUNT; select++)
    {
        for (u32 index = 0; index < HIRES_CRTC_PALETTE_SIZE; index++)
            UpdateColor(select, index);
    }
}

bool HiresCRTC::IsTwoPage() const
{
    return (m_state.registers[k_hires_crtc_clock] & 0x02) != 0;
}

bool HiresCRTC::IsPackMode() const
{
    return (m_state.gateway_register0 & k_hires_crtc_pack_enable) != 0;
}

// Pack mode's 4 CPU bytes to 3 VRAM bytes is inferred from EGB_HR.BIN's strides for mode 27, 2560 for 1920
// No code shows the fourth byte, so reading it as 0 is a guess
u8 HiresCRTC::ReadOnePage(u32 offset) const
{
    if (!IsPackMode())
        return m_vram[offset & (k_hires_crtc_vram_size - 1)];

    u32 packed = (offset >> 2) * 3 + (offset & 0x03);
    return (offset & 0x03) == 0x03 || packed >= k_hires_crtc_vram_size ? 0x00 : m_vram[packed];
}

void HiresCRTC::WriteOnePage(u32 offset, u8 value)
{
    if (!IsPackMode())
    {
        m_vram[offset & (k_hires_crtc_vram_size - 1)] = value;
        return;
    }

    u32 packed = (offset >> 2) * 3 + (offset & 0x03);

    if ((offset & 0x03) != 0x03 && packed < k_hires_crtc_vram_size)
        m_vram[packed] = value;
}

// The back page shows whole and the front one lets its key through, as on the compatibility CRTC
void HiresCRTC::RenderRow(u32* destination, int row, int width) const
{
    if (IsTwoPage())
    {
        int front = m_state.registers[k_hires_crtc_priority] & 0x01;
        RenderLayer(destination, front ^ 1, row, width, false);
        RenderLayer(destination, front, row, width, true);
    }
    else
        RenderLayer(destination, 0, row, width, false);

    if ((m_state.registers[k_hires_crtc_cursor_on] & 0x01) != 0)
        RenderCursor(destination, row, width);
}

// 64 by 64 with 8 bytes a row, the leftmost pixel in the top bit, as DISPT8.DRV sends a Windows cursor unchanged: its AND
// mask first, where 1 is transparent, then its XOR mask, 0 for the back color and 1 for the front one
// Both set inverts the screen: the driver doesn't convert them and Windows's I-beam is made of them, nothing else shows it
// The position counts zoomed pixels from the display window's corner, and 202h and 203h name the pattern pixel shown there
void HiresCRTC::RenderCursor(u32* destination, int row, int width) const
{
    u32 zoom = m_state.registers[k_hires_crtc_cursor_zoom];
    int zoom_x = (int)(zoom & 0xFF) + 1;
    int zoom_y = (int)((zoom >> 8) & 0xFF) + 1;
    int top = ((int)GetWord(k_hires_crtc_cursor_y) - (int)GetWord(k_hires_crtc_cursor_hot_y)) * zoom_y;

    if (row < top)
        return;

    int y = (row - top) / zoom_y;

    if (y >= 64)
        return;

    const u8* mask = &m_state.cursor_pattern[y * 8];
    const u8* pattern = mask + (HIRES_CRTC_CURSOR_PATTERN_SIZE / 2);
    u32 colors[2] = { ToXRGB(m_state.registers[k_hires_crtc_cursor_back]),
        ToXRGB(m_state.registers[k_hires_crtc_cursor_front]) };
    int start = ((int)GetWord(k_hires_crtc_cursor_x) - (int)GetWord(k_hires_crtc_cursor_hot_x)) * zoom_x;

    for (int x = 0; (x < 64) && (start < width); x++, start += zoom_x)
    {
        u32 bit = 0x80U >> (x & 0x07);
        bool clear = (mask[x >> 3] & bit) != 0;
        bool set = (pattern[x >> 3] & bit) != 0;

        if (clear && !set)
            continue;

        int end = MIN(start + zoom_x, width);

        for (int i = MAX(start, 0); i < end; i++)
            destination[i] = clear ? destination[i] ^ 0x00FFFFFF : colors[set ? 1 : 0];
    }
}

void HiresCRTC::RenderLayer(u32* destination, int page, int row, int width, bool keyed) const
{
    if ((m_state.registers[k_hires_crtc_priority] & (0x100U << page)) == 0)
        return;

    const u32* registers = &m_state.registers[page == 0 ? k_hires_crtc_page0 : k_hires_crtc_page1];
    u32 format = registers[k_hires_crtc_layer_mode] & 0x07;
    u32 zoom = registers[k_hires_crtc_layer_zoom];
    u32 zoom_x = (zoom & 0xFF) + 1;
    u32 zoom_y = ((zoom >> 8) & 0xFF) + 1;
    u32 haj = registers[k_hires_crtc_layer_haj] & 0xFFFF;
    u32 vaj = registers[k_hires_crtc_layer_vaj] & 0xFFFF;
    u32 w = registers[k_hires_crtc_layer_w] & 0xFFFF;
    u32 h = registers[k_hires_crtc_layer_h] & 0xFFFF;
    u32 end = MIN(haj + w, (u32)width);

    if (format < 1 || format > 4 || (u32)row < vaj || (u32)row >= vaj + h || haj >= end)
        return;

    u32 vram_width = MAX(registers[k_hires_crtc_layer_vram_w] & 0xFFFF, 1U);
    u32 vram_height = MAX(registers[k_hires_crtc_layer_vram_h] & 0xFFFF, 1U);
    u32 y = (((u32)row - vaj) / zoom_y + (registers[k_hires_crtc_layer_yoff] & 0xFFFF)) % vram_height;
    u32 bits = format == 1 ? 4 : format * 8 - 8;
    bool two_page = IsTwoPage();

    Layer layer;
    layer.page = m_vram + (two_page ? (u32)page * k_hires_crtc_page_size : 0);
    layer.page_mask = (two_page ? k_hires_crtc_page_size : k_hires_crtc_vram_size) - 1;
    layer.palette = format <= 2 ? m_colors[(format - 1) * 2 + (u32)page] : NULL;
    layer.row_offset = y * ((vram_width * bits) / 8);
    layer.x = (registers[k_hires_crtc_layer_xoff] & 0xFFFF) % vram_width;
    layer.vram_width = vram_width;
    layer.zoom = zoom_x;
    layer.key_mask = registers[k_hires_crtc_layer_mask];
    layer.key = registers[k_hires_crtc_layer_key];

    u32* start = destination + haj;
    int count = (int)(end - haj);

    switch (format)
    {
        case 1:
            keyed ? RenderPixels<4, true>(start, count, layer) : RenderPixels<4, false>(start, count, layer);
            break;
        case 2:
            keyed ? RenderPixels<8, true>(start, count, layer) : RenderPixels<8, false>(start, count, layer);
            break;
        case 3:
            keyed ? RenderPixels<16, true>(start, count, layer) : RenderPixels<16, false>(start, count, layer);
            break;
        default:
            keyed ? RenderPixels<24, true>(start, count, layer) : RenderPixels<24, false>(start, count, layer);
            break;
    }
}

// Unzoomed rows go in runs that neither wrap around the page's width nor its memory, the rest pixel by pixel
template<int bits, bool keyed>
void HiresCRTC::RenderPixels(u32* destination, int count, const Layer& layer) const
{
    u32 x = layer.x;
    int i = 0;

    if (layer.zoom != 1)
    {
        RenderZoomedPixels<bits, keyed>(destination, count, layer);
        return;
    }

    while (i < count)
    {
        u32 run = MIN((u32)(count - i), layer.vram_width - x);
        u32 first = (layer.row_offset + (x * bits) / 8) & layer.page_mask;
        bool aligned = bits != 4 || (x & 0x01) == 0;

        if (aligned && first + (run * bits + 7) / 8 <= layer.page_mask + 1)
            RenderRun<bits, keyed>(destination + i, run, layer.page + first, layer);
        else
        {
            for (u32 j = 0; j < run; j++)
            {
                u32 pixel = FetchPixel<bits>(layer, x + j);
                StorePixel<keyed>(destination + i + j, pixel, GetPixelColor<bits>(layer, pixel), layer);
            }
        }

        i += (int)run;
        x += run;

        if (x == layer.vram_width)
            x = 0;
    }
}

template<int bits, bool keyed>
void HiresCRTC::RenderZoomedPixels(u32* destination, int count, const Layer& layer) const
{
    u32 x = layer.x;
    int zoom = (int)layer.zoom;

    for (int i = 0; i < count; i += zoom)
    {
        u32 pixel = FetchPixel<bits>(layer, x);

        if (++x == layer.vram_width)
            x = 0;

        if (keyed && (pixel & layer.key_mask) == layer.key)
            continue;

        u32 color = GetPixelColor<bits>(layer, pixel);
        int run = MIN(zoom, count - i);

        for (int j = 0; j < run; j++)
            destination[i + j] = color;
    }
}

template<int bits, bool keyed>
void HiresCRTC::RenderRun(u32* destination, u32 count, const u8* source, const Layer& layer) const
{
    u32 i = 0;

    if (bits == 4)
    {
        for (; i + 1 < count; i += 2)
        {
            u32 data = *source++;
            u32 low = data & 0x0F;
            u32 high = data >> 4;
            StorePixel<keyed>(destination + i, low, layer.palette[low], layer);
            StorePixel<keyed>(destination + i + 1, high, layer.palette[high], layer);
        }

        if (i < count)
        {
            u32 low = *source & 0x0F;
            StorePixel<keyed>(destination + i, low, layer.palette[low], layer);
        }

        return;
    }

    for (; i < count; i++)
    {
        u32 pixel = bits == 8 ? source[i] : bits == 16 ? read_u16_le(source + i * 2) :
            source[i * 3] | ((u32)source[i * 3 + 1] << 8) | ((u32)source[i * 3 + 2] << 16);
        StorePixel<keyed>(destination + i, pixel, GetPixelColor<bits>(layer, pixel), layer);
    }
}

template<bool keyed>
INLINE void HiresCRTC::StorePixel(u32* destination, u32 pixel, u32 color, const Layer& layer) const
{
    *destination = keyed && (pixel & layer.key_mask) == layer.key ? *destination : color;
}

template<int bits>
u32 HiresCRTC::FetchPixel(const Layer& layer, u32 x) const
{
    const u8* page = layer.page;
    u32 mask = layer.page_mask;

    if (bits == 4)
    {
        u8 data = page[(layer.row_offset + (x >> 1)) & mask];
        return (x & 0x01) != 0 ? data >> 4 : data & 0x0F;
    }

    if (bits == 8)
        return page[(layer.row_offset + x) & mask];

    if (bits == 16)
    {
        u32 offset = layer.row_offset + x * 2;
        return page[offset & mask] | ((u32)page[(offset + 1) & mask] << 8);
    }

    u32 offset = layer.row_offset + x * 3;
    return page[offset & mask] | ((u32)page[(offset + 1) & mask] << 8) | ((u32)page[(offset + 2) & mask] << 16);
}

// 24 bpp is blue, green and red from the low byte up, by DISPT24M's physical color and tdisp.drv's DIB engine
template<int bits>
u32 HiresCRTC::GetPixelColor(const Layer& layer, u32 pixel) const
{
    if (bits == 16)
        return m_direct_colors[pixel & 0x7FFF];

    if (bits == 24)
        return 0xFF000000U | pixel;

    return layer.palette[pixel];
}

HiresCRTC::HiresCRTC_State* HiresCRTC::GetState()
{
    return &m_state;
}

void HiresCRTC::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer, GT_SAVESTATE_VERSION);
}

void HiresCRTC::LoadState(std::istream& stream, int version)
{
    StateSerializer serializer(stream);
    Serialize(serializer, version);
    UpdateColors();
}

void HiresCRTC::Serialize(StateSerializer& serializer, int version)
{
    UNUSED(version);
    G_SERIALIZE_ARRAY(serializer, m_state.registers, HIRES_CRTC_REGISTER_COUNT);
    G_SERIALIZE_ARRAY(serializer, &m_state.palette[0][0], HIRES_CRTC_PALETTE_COUNT * HIRES_CRTC_PALETTE_SIZE);
    G_SERIALIZE(serializer, m_state.palette_latch);
    G_SERIALIZE_ARRAY(serializer, m_state.cursor_pattern, HIRES_CRTC_CURSOR_PATTERN_SIZE);
    G_SERIALIZE(serializer, m_state.gateway_register0);
    G_SERIALIZE(serializer, m_state.address);
    G_SERIALIZE(serializer, m_state.gateway_address);
    G_SERIALIZE_ARRAY(serializer, m_state.window, 4);
}
