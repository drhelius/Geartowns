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

#include "video.h"
#include "../system/towns_pic.h"
#include "../common/state_serializer.h"

Video::Video()
{
    InitPointer(m_pic);
    InitPointer(m_frame_buffer);
    m_pixel_format = GT_PIXEL_RGBA8888;
    m_render = false;
    m_frame_ready = false;
    m_next_event_time = GT_NO_EVENT;
    m_frame_width = GT_FRAME_BUFFER_WIDTH;
    m_frame_height = GT_FRAME_BUFFER_HEIGHT;
    m_render_width = GT_FRAME_BUFFER_WIDTH;
    m_render_height = GT_FRAME_BUFFER_HEIGHT;
    m_rendered_rows = 0;
    m_canvas_h_start = 0;
    m_canvas_v_start = 0;
    m_canvas_h_divider = 1;
    m_canvas_interlaced = false;
}

Video::~Video()
{
}

void Video::Init(TownsPIC* pic, GT_Pixel_Format pixel_format)
{
    m_pic = pic;
    m_pixel_format = pixel_format;
    Reset();
}

void Video::Reset()
{
    memset(&m_state, 0, sizeof(m_state));
    memset(m_state.mask, 0xFF, sizeof(m_state.mask));
    m_frame_ready = false;
    m_rendered_rows = 0;
    m_frame_width = GT_FRAME_BUFFER_WIDTH;
    m_frame_height = GT_FRAME_BUFFER_HEIGHT;
    UpdateColorCaches();
    UpdateGeometry();
    UpdateNextEvent();
    UpdateIRQ();
}

u8 Video::Read(u16 port, u64 time_ns)
{
    Synchronize(time_ns);

    switch (port)
    {
        case 0x0440:
            return m_state.crtc_index;
        case 0x0442:
            return ReadCRTC(false, time_ns);
        case 0x0443:
            return ReadCRTC(true, time_ns);
        case 0x0448:
            return m_state.output_index;
        case 0x044A:
            return m_state.output[m_state.output_index];
        case 0x044C:
        {
            // Sprite busy and page stay clear without the sprite engine
            u8 value = m_state.digital_palette_modified ? 0x80 : 0x00;
            m_state.digital_palette_modified = false;
            return value;
        }
        case 0x0458:
            return m_state.mask_index;
        case 0x045A:
            return m_state.mask[(m_state.mask_index & 0x01) * 2];
        case 0x045B:
            return m_state.mask[(m_state.mask_index & 0x01) * 2 + 1];
        case 0xFD90:
            return m_state.palette_index;
        case 0xFD92:
        case 0xFD94:
        case 0xFD96:
            return ReadPalette((port - 0xFD92) >> 1);
        case 0xFD98:
        case 0xFD99:
        case 0xFD9A:
        case 0xFD9B:
        case 0xFD9C:
        case 0xFD9D:
        case 0xFD9E:
        case 0xFD9F:
            return m_state.digital_palette[port - 0xFD98];
        case 0xFDA0:
        {
            u8 status = GetSyncStatus(time_ns);
            return ((status & 0x04) != 0 ? 0x01 : 0x00) | ((status & 0x02) != 0 ? 0x02 : 0x00);
        }
        default:
            return 0xFF;
    }
}

void Video::Write(u16 port, u8 value, u64 time_ns)
{
    Synchronize(time_ns);

    switch (port)
    {
        case 0x0440:
            m_state.crtc_index = value & 0x1F;
            break;
        case 0x0442:
            WriteCRTC(value, false, time_ns);
            break;
        case 0x0443:
            WriteCRTC(value, true, time_ns);
            break;
        case 0x0448:
            m_state.output_index = value & 0x03;
            break;
        case 0x044A:
            RenderUpTo(time_ns);
            m_state.output[m_state.output_index] = value;
            break;
        case 0x0458:
            m_state.mask_index = value & 0x03;
            break;
        case 0x045A:
            m_state.mask[(m_state.mask_index & 0x01) * 2] = value;
            break;
        case 0x045B:
            m_state.mask[(m_state.mask_index & 0x01) * 2 + 1] = value;
            break;
        case 0x05CA:
            m_state.vsync_irq = false;
            UpdateIRQ();
            break;
        case 0xFD90:
            m_state.palette_index = value;
            break;
        case 0xFD92:
        case 0xFD94:
        case 0xFD96:
            RenderUpTo(time_ns);
            WritePalette((port - 0xFD92) >> 1, value);
            break;
        case 0xFD98:
        case 0xFD99:
        case 0xFD9A:
        case 0xFD9B:
        case 0xFD9C:
        case 0xFD9D:
        case 0xFD9E:
        case 0xFD9F:
            m_state.digital_palette[port - 0xFD98] = value & 0x0F;
            m_state.digital_palette_modified = true;
            break;
        case 0xFDA0:
            RenderUpTo(time_ns);
            m_state.display_enable = value;
            break;
        default:
            break;
    }
}

void Video::BeginFrame(u8* frame_buffer, bool render)
{
    m_frame_buffer = frame_buffer;
    m_render = render;
    m_frame_ready = false;
}

void Video::EndFrame()
{
    if (m_frame_ready || m_state.running)
        return;

    // A stopped CRTC sends no picture
    m_frame_width = GT_FRAME_BUFFER_WIDTH;
    m_frame_height = GT_FRAME_BUFFER_HEIGHT;

    if (!m_render || !IsValidPointer(m_frame_buffer))
        return;

    int pixels = m_frame_width * m_frame_height;
    u32 black = MakeColor(0, 0, 0);

    if (m_pixel_format == GT_PIXEL_RGB565)
        memset(m_frame_buffer, 0, pixels * 2);
    else
    {
        u32* buffer = (u32*)m_frame_buffer;

        for (int i = 0; i < pixels; i++)
            buffer[i] = black;
    }
}

u8 Video::ReadVRAMTwoPage(void* device, u32 offset)
{
    Video* video = (Video*)device;
    return video->m_state.vram[offset & (VIDEO_VRAM_SIZE - 1)];
}

void Video::WriteVRAMTwoPage(void* device, u32 offset, u8 value)
{
    Video* video = (Video*)device;
    u8 mask = video->m_state.mask[offset & 0x03];
    u8& data = video->m_state.vram[offset & (VIDEO_VRAM_SIZE - 1)];
    data = (u8)((data & ~mask) | (value & mask));
}

u8 Video::ReadVRAMSinglePage(void* device, u32 offset)
{
    Video* video = (Video*)device;
    return video->m_state.vram[video->SinglePageToCanonical(offset)];
}

void Video::WriteVRAMSinglePage(void* device, u32 offset, u8 value)
{
    Video* video = (Video*)device;
    u8 mask = video->m_state.mask[offset & 0x03];
    u8& data = video->m_state.vram[video->SinglePageToCanonical(offset)];
    data = (u8)((data & ~mask) | (value & mask));
}

void Video::WriteCRTC(u8 value, bool high, u64 time_ns)
{
    int index = m_state.crtc_index;
    u16 previous = m_state.crtc[index];

    RenderUpTo(time_ns);

    if (high)
        m_state.crtc[index] = (u16)((previous & 0x00FF) | (value << 8));
    else
        m_state.crtc[index] = (u16)((previous & 0xFF00) | value);

    if (index != k_video_crtc_cr0)
        return;

    bool start = (m_state.crtc[index] & 0x8000) != 0;

    if (start && !m_state.running)
        StartFrame(time_ns);
    else if (!start && m_state.running)
    {
        m_state.running = false;
        UpdateNextEvent();
    }
}

u8 Video::ReadCRTC(bool high, u64 time_ns)
{
    int index = m_state.crtc_index;

    // FR reads the live sync and display status in its high byte
    if (index == k_video_crtc_fr && high)
        return GetSyncStatus(time_ns);

    u16 value = m_state.crtc[index];
    return high ? (u8)(value >> 8) : (u8)value;
}

void Video::WritePalette(int component, u8 value)
{
    u8 index = m_state.palette_index;

    switch ((m_state.output[1] >> 4) & 0x03)
    {
        case 0:
            m_state.palette16[0][index & 0x0F][component] = value & 0xF0;
            UpdatePaletteColor(0, index & 0x0F);
            break;
        case 2:
            m_state.palette16[1][index & 0x0F][component] = value & 0xF0;
            UpdatePaletteColor(1, index & 0x0F);
            break;
        default:
            m_state.palette256[index][component] = value;
            UpdatePaletteColor(2, index);
            break;
    }
}

u8 Video::ReadPalette(int component) const
{
    u8 index = m_state.palette_index;

    switch ((m_state.output[1] >> 4) & 0x03)
    {
        case 0:
            return m_state.palette16[0][index & 0x0F][component];
        case 2:
            return m_state.palette16[1][index & 0x0F][component];
        default:
            return m_state.palette256[index][component];
    }
}

// Timing registers are latched at each frame start
// The vertical period counts VST + 1 half-lines, 525 lines on the VGA preset and 262.5 on 15 kHz ones
void Video::StartFrame(u64 time_ns)
{
    m_state.running = true;
    m_state.frame_start_time = time_ns;
    m_state.frame_line_clocks = MAX((u32)m_state.crtc[k_video_crtc_hst] + 1, k_video_min_line_clocks);
    m_state.frame_half_lines = MAX((u32)m_state.crtc[k_video_crtc_vst] + 1, k_video_min_half_lines);
    m_state.frame_clock_rate = k_video_clock_rates[m_state.crtc[k_video_crtc_cr1] & 0x03];
    m_rendered_rows = 0;
    UpdateGeometry();
    UpdateNextEvent();
}

// Frames run from VSYNC to VSYNC, so every displayed line has been scanned when the next one starts
void Video::CompleteFrame()
{
    u64 vsync_time = m_next_event_time;

    for (; m_rendered_rows < m_render_height; m_rendered_rows++)
        RenderRow(m_rendered_rows);

    m_frame_width = m_render_width;
    m_frame_height = m_render_height;
    m_frame_ready = true;
    m_state.frame_count++;
    m_state.vsync_irq = true;
    UpdateIRQ();
    StartFrame(vsync_time);
}

void Video::UpdateGeometry()
{
    m_canvas_interlaced = (m_state.frame_half_lines & 0x01) != 0;

    u32 h_start = 0xFFFFFFFF;
    u32 h_end = 0;
    u32 v_start = 0xFFFFFFFF;
    u32 v_end = 0;
    int layers = IsTwoPage() ? 2 : 1;

    for (int layer = 0; layer < layers; layer++)
    {
        u32 hds = m_state.crtc[k_video_crtc_hds0 + layer * 2];
        u32 hde = m_state.crtc[k_video_crtc_hde0 + layer * 2];
        u32 vds = m_state.crtc[k_video_crtc_vds0 + layer * 2];
        u32 vde = m_state.crtc[k_video_crtc_vde0 + layer * 2];

        if (hde <= hds || vde <= vds)
            continue;

        h_start = MIN(h_start, hds);
        h_end = MAX(h_end, hde);
        v_start = MIN(v_start, vds);
        v_end = MAX(v_end, vde);
    }

    if (!m_state.running || h_end <= h_start || v_end <= v_start)
    {
        m_canvas_h_start = 0;
        m_canvas_v_start = 0;
        m_canvas_h_divider = 1;
        m_render_width = GT_FRAME_BUFFER_WIDTH;
        m_render_height = GT_FRAME_BUFFER_HEIGHT;
        return;
    }

    // The canvas covers both display windows, sampled at a whole number of CRTC clocks per output pixel
    u32 clocks = h_end - h_start;
    u32 rows = (v_end - v_start) / 2;

    if (m_canvas_interlaced)
        rows *= 2;

    m_canvas_h_start = h_start;
    m_canvas_v_start = v_start;
    m_canvas_h_divider = (clocks + GT_MAX_FRAME_BUFFER_WIDTH - 1) / GT_MAX_FRAME_BUFFER_WIDTH;
    m_render_width = MAX((int)(clocks / m_canvas_h_divider), 1);
    m_render_height = CLAMP((int)rows, 1, GT_MAX_FRAME_BUFFER_HEIGHT);
}

void Video::UpdateNextEvent()
{
    if (!m_state.running)
    {
        m_next_event_time = GT_NO_EVENT;
        return;
    }

    u64 frame_clocks = ((u64)m_state.frame_half_lines * m_state.frame_line_clocks) / 2;
    u64 frame_ns = (frame_clocks * 1000000000ULL + m_state.frame_clock_rate - 1) / m_state.frame_clock_rate;
    m_next_event_time = m_state.frame_start_time + frame_ns;
}

void Video::UpdateIRQ()
{
    m_pic->SetIRQLine(11, m_state.vsync_irq);
}

// Same bit layout as the FR status byte
u8 Video::GetSyncStatus(u64 time_ns) const
{
    if (!m_state.running)
        return 0x00;

    const u16* crtc = m_state.crtc;
    u32 half_line = GetBeamHalfLine(time_ns);
    u32 clock = GetBeamClock(time_ns);
    u8 status = 0x00;

    if (clock < crtc[k_video_crtc_hsw1])
        status |= 0x02;

    if (half_line >= crtc[k_video_crtc_vst1] && half_line < crtc[k_video_crtc_vst2])
        status |= 0x04;

    if (m_canvas_interlaced && (m_state.frame_count & 0x01) != 0)
        status |= 0x08;

    for (int layer = 0; layer < 2; layer++)
    {
        if (clock >= crtc[k_video_crtc_hds0 + layer * 2] && clock < crtc[k_video_crtc_hde0 + layer * 2])
            status |= (u8)(0x10 << layer);

        if (half_line >= crtc[k_video_crtc_vds0 + layer * 2] && half_line < crtc[k_video_crtc_vde0 + layer * 2])
            status |= (u8)(0x40 << layer);
    }

    return status;
}

// Rows already scanned are drawn before a register or palette change, so the change only affects later rows
// VRAM writes are not tracked, they show up in every row not drawn yet
void Video::RenderUpTo(u64 time_ns)
{
    if (!m_state.running)
        return;

    u32 half_line = GetBeamHalfLine(time_ns);

    if (half_line <= m_canvas_v_start)
        return;

    int rows = (int)((half_line - m_canvas_v_start) / 2);

    if (m_canvas_interlaced)
        rows *= 2;

    rows = MIN(rows, m_render_height);

    for (; m_rendered_rows < rows; m_rendered_rows++)
        RenderRow(m_rendered_rows);
}

void Video::RenderRow(int row)
{
    if (!m_render || !IsValidPointer(m_frame_buffer))
        return;

    u32 black = MakeColor(0, 0, 0);

    for (int x = 0; x < m_render_width; x++)
        m_line[x] = black;

    if (IsTwoPage())
    {
        // The back layer is drawn opaque and the front one lets transparent pixels through
        int front = m_state.output[1] & 0x01;
        RenderLayerRow(front ^ 1, row, true);
        RenderLayerRow(front, row, false);
    }
    else
        RenderLayerRow(0, row, true);

    if (m_pixel_format == GT_PIXEL_RGB565)
    {
        u16* buffer = (u16*)m_frame_buffer + row * m_render_width;

        for (int x = 0; x < m_render_width; x++)
            buffer[x] = (u16)m_line[x];
    }
    else
        memcpy(m_frame_buffer + row * m_render_width * 4, m_line, m_render_width * 4);
}

void Video::RenderLayerRow(int layer, int row, bool opaque)
{
    Video_Layer_Format format = GetLayerFormat(layer);

    if (format == VIDEO_LAYER_OFF)
        return;

    const u16* crtc = m_state.crtc;
    u32 field_line = m_canvas_interlaced ? (u32)row >> 1 : (u32)row;
    u32 half_line = m_canvas_v_start + field_line * 2;
    u32 vds = crtc[k_video_crtc_vds0 + layer * 2];
    u32 vde = crtc[k_video_crtc_vde0 + layer * 2];

    if (half_line < vds || half_line >= vde)
        return;

    u32 zoom = (u32)crtc[k_video_crtc_zoom] >> (layer * 8);
    u32 zoom_x = (zoom & 0x0F) + 1;
    u32 zoom_y = ((zoom >> 4) & 0x0F) + 1;
    bool two_page = IsTwoPage();
    u32 unit = two_page ? 4 : 8;
    u32 page_mask = two_page ? 0x3FFFF : 0x7FFFF;
    u32 page_base = two_page ? (u32)layer << 18 : 0;
    u32 line = ((half_line - vds) / 2) / zoom_y;
    u32 start = ((u32)crtc[k_video_crtc_fa0 + layer * 4] + line * crtc[k_video_crtc_lo0 + layer * 4]) * unit;

    // The second field of an interlaced frame starts FO further on
    if (m_canvas_interlaced && (row & 0x01) != 0)
        start += (u32)crtc[k_video_crtc_fo0 + layer * 4] * unit;

    // Fetching starts at HAJ, so data before HDS has already been consumed when the window opens
    u32 hds = crtc[k_video_crtc_hds0 + layer * 2];
    u32 hde = crtc[k_video_crtc_hde0 + layer * 2];
    u32 haj = crtc[k_video_crtc_haj0 + layer * 4];
    u32 first = MAX(hds, haj);

    for (int x = 0; x < m_render_width; x++)
    {
        u32 clock = m_canvas_h_start + (u32)x * m_canvas_h_divider;

        if (clock < first || clock >= hde)
            continue;

        u32 pixel = (clock - haj) / zoom_x;

        switch (format)
        {
            case VIDEO_LAYER_4BPP:
            {
                u8 data = ReadVRAM(page_base + ((start + (pixel >> 1)) & page_mask), two_page);
                u8 index = (pixel & 0x01) != 0 ? data >> 4 : data & 0x0F;

                if (opaque || index != 0)
                    m_line[x] = m_palette16_colors[layer][index];

                break;
            }
            case VIDEO_LAYER_8BPP:
            {
                u8 index = ReadVRAM(page_base + ((start + pixel) & page_mask), two_page);

                if (opaque || index != 0)
                    m_line[x] = m_palette256_colors[index];

                break;
            }
            default:
            {
                u32 address = start + pixel * 2;
                u16 value = (u16)(ReadVRAM(page_base + (address & page_mask), two_page) |
                    (ReadVRAM(page_base + ((address + 1) & page_mask), two_page) << 8));

                if (opaque || (value & 0x8000) == 0)
                    m_line[x] = m_direct_colors[value & 0x7FFF];

                break;
            }
        }
    }
}

// Palette components are kept in port order: blue, red, green
void Video::UpdatePaletteColor(int bank, int index)
{
    if (bank < 2)
    {
        const u8* color = m_state.palette16[bank][index];
        m_palette16_colors[bank][index] = MakeColor(color[1] | (color[1] >> 4), color[2] | (color[2] >> 4),
            color[0] | (color[0] >> 4));
    }
    else
    {
        const u8* color = m_state.palette256[index];
        m_palette256_colors[index] = MakeColor(color[1], color[2], color[0]);
    }
}

// Direct color is GRB555, green on top
void Video::UpdateColorCaches()
{
    for (int bank = 0; bank < 2; bank++)
    {
        for (int i = 0; i < 16; i++)
            UpdatePaletteColor(bank, i);
    }

    for (int i = 0; i < 256; i++)
        UpdatePaletteColor(2, i);

    for (u32 i = 0; i < 0x8000; i++)
        m_direct_colors[i] = MakeColor(Expand5((i >> 5) & 0x1F), Expand5((i >> 10) & 0x1F), Expand5(i & 0x1F));
}

void Video::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void Video::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void Video::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE_ARRAY(serializer, m_state.vram, VIDEO_VRAM_SIZE);
    G_SERIALIZE_ARRAY(serializer, m_state.sprite_ram, VIDEO_SPRITE_RAM_SIZE);
    G_SERIALIZE_ARRAY(serializer, m_state.crtc, VIDEO_CRTC_REGISTER_COUNT);
    G_SERIALIZE(serializer, m_state.crtc_index);
    G_SERIALIZE_ARRAY(serializer, m_state.output, 4);
    G_SERIALIZE(serializer, m_state.output_index);
    G_SERIALIZE_ARRAY(serializer, m_state.mask, 4);
    G_SERIALIZE(serializer, m_state.mask_index);
    G_SERIALIZE(serializer, m_state.palette_index);
    G_SERIALIZE_ARRAY(serializer, &m_state.palette16[0][0][0], sizeof(m_state.palette16));
    G_SERIALIZE_ARRAY(serializer, &m_state.palette256[0][0], sizeof(m_state.palette256));
    G_SERIALIZE_ARRAY(serializer, m_state.digital_palette, 8);
    G_SERIALIZE(serializer, m_state.digital_palette_modified);
    G_SERIALIZE(serializer, m_state.display_enable);
    G_SERIALIZE(serializer, m_state.vsync_irq);
    G_SERIALIZE(serializer, m_state.running);
    G_SERIALIZE(serializer, m_state.frame_start_time);
    G_SERIALIZE(serializer, m_state.frame_line_clocks);
    G_SERIALIZE(serializer, m_state.frame_half_lines);
    G_SERIALIZE(serializer, m_state.frame_clock_rate);
    G_SERIALIZE(serializer, m_state.frame_count);
}

void Video::SanitizeState()
{
    m_state.crtc_index &= 0x1F;
    m_state.output_index &= 0x03;
    m_state.mask_index &= 0x03;
    m_state.frame_line_clocks = MAX(m_state.frame_line_clocks, k_video_min_line_clocks);
    m_state.frame_half_lines = MAX(m_state.frame_half_lines, k_video_min_half_lines);

    if (m_state.frame_clock_rate == 0)
        m_state.frame_clock_rate = k_video_clock_rates[m_state.crtc[k_video_crtc_cr1] & 0x03];

    // The rows drawn before saving are not part of the state, the rest of the frame redraws them
    m_rendered_rows = 0;
    UpdateColorCaches();
    UpdateGeometry();
    UpdateNextEvent();
    UpdateIRQ();
}
