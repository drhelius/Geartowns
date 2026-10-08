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
#include "../common/trace_logger.h"
#include "../system/pic.h"
#include "../system/pit.h"
#include "../system/scheduler.h"
#include "../common/state_serializer.h"

Video::Video()
{
    InitPointer(m_sprite);
    InitPointer(m_pic);
    InitPointer(m_trace_logger);
    InitPointer(m_pit);
    InitPointer(m_scheduler);
    InitPointer(m_font_rom);
    InitPointer(m_frame_buffer);
    m_render = false;
    m_frame_ready = false;
    m_next_event_clocks = GT_NO_EVENT;
    m_next_event_half_line = 0;
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
    SafeDelete(m_sprite);
}

void Video::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

void Video::Init(PIC* pic, PIT* pit, Scheduler* scheduler, const u8* font_rom)
{
    m_pic = pic;
    m_pit = pit;
    m_scheduler = scheduler;
    m_font_rom = font_rom;

    if (!IsValidPointer(m_sprite))
        m_sprite = new Sprite();

    m_sprite->Init(m_state.vram, m_state.sprite_ram);

    Reset();
}

void Video::Reset()
{
    memset(&m_state, 0, sizeof(m_state));
    memset(m_state.mask, 0xFF, sizeof(m_state.mask));
    m_state.fmr_display_planes = 0x0F;
    m_sprite->Reset();
    ResetFMRView();
    m_frame_ready = false;
    m_rendered_rows = 0;
    m_frame_width = GT_FRAME_BUFFER_WIDTH;
    m_frame_height = GT_FRAME_BUFFER_HEIGHT;
    UpdateColorCaches();
    UpdateGeometry();
    UpdateNextEvent();
    UpdateIRQ();
}

// The latches that shape what the CPU sees at C0000-CFFFF
// restored with the memory map on a CPU reset
void Video::ResetFMRView()
{
    m_state.fmr_mask = 0x0F;
    m_state.fmr_page = false;
    m_state.fmr_ank = false;
}

u8 Video::Read(u16 port, u64 clocks)
{
    Synchronize(clocks);

    switch (port)
    {
        case 0x0440:
            return m_state.crtc_index;
        case 0x0442:
            return ReadCRTC(false, clocks);
        case 0x0443:
            return ReadCRTC(true, clocks);
        case 0x0448:
            return m_state.output_index;
        case 0x044A:
            return m_state.output[m_state.output_index];
        case 0x044C:
        {
            u8 value = (m_state.digital_palette_modified ? 0x80 : 0x00) | (m_sprite->IsBusy() ? 0x02 : 0x00) |
                (m_sprite->GetPage() ? 0x01 : 0x00);
            m_state.digital_palette_modified = false;
            return value;
        }
        case 0x0450:
        case 0x0452:
            return m_sprite->Read(port);
        case 0x0458:
            return m_state.mask_index;
        case 0x045A:
            return m_state.mask[(m_state.mask_index & 0x01) * 2];
        case 0x045B:
            return m_state.mask[(m_state.mask_index & 0x01) * 2 + 1];
        case 0x05C8:
        {
            u8 value = m_state.fmr_text_written ? 0xFF : 0x00;
            m_state.fmr_text_written = false;
            return value;
        }
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
            u8 status = GetSyncStatus(clocks);
            return ((status & 0x04) != 0 ? 0x01 : 0x00) | ((status & 0x02) != 0 ? 0x02 : 0x00);
        }
        default:
            return 0xFF;
    }
}

// Port reads without catching up and without clearing the read-once status flags
u8 Video::Peek(u16 port, u64 clocks) const
{
    switch (port)
    {
        case 0x0440:
            return m_state.crtc_index;
        case 0x0442:
            return ReadCRTC(false, clocks);
        case 0x0443:
            return ReadCRTC(true, clocks);
        case 0x0448:
            return m_state.output_index;
        case 0x044A:
            return m_state.output[m_state.output_index];
        case 0x044C:
            return (m_state.digital_palette_modified ? 0x80 : 0x00) | (m_sprite->IsBusy() ? 0x02 : 0x00) |
                (m_sprite->GetPage() ? 0x01 : 0x00);
        case 0x0450:
        case 0x0452:
            return m_sprite->Read(port);
        case 0x0458:
            return m_state.mask_index;
        case 0x045A:
            return m_state.mask[(m_state.mask_index & 0x01) * 2];
        case 0x045B:
            return m_state.mask[(m_state.mask_index & 0x01) * 2 + 1];
        case 0x05C8:
            return m_state.fmr_text_written ? 0xFF : 0x00;
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
            u8 status = GetSyncStatus(clocks);
            return ((status & 0x04) != 0 ? 0x01 : 0x00) | ((status & 0x02) != 0 ? 0x02 : 0x00);
        }
        default:
            return 0xFF;
    }
}

void Video::Write(u16 port, u8 value, u64 clocks)
{
    Synchronize(clocks);

    switch (port)
    {
        case 0x0440:
            m_state.crtc_index = value & 0x1F;
            break;
        case 0x0442:
            WriteCRTC(value, false, clocks);
            break;
        case 0x0443:
            WriteCRTC(value, true, clocks);
            break;
        case 0x0448:
            m_state.output_index = value & 0x03;
            break;
        case 0x044A:
            m_state.output[m_state.output_index] = value;
            break;
        case 0x0450:
        case 0x0452:
            m_sprite->Write(port, value);
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

    m_frame_width = GT_FRAME_BUFFER_WIDTH;
    m_frame_height = GT_FRAME_BUFFER_HEIGHT;

    if (!m_render || !IsValidPointer(m_frame_buffer))
        return;

    int pixels = m_frame_width * m_frame_height;
    u32* buffer = (u32*)m_frame_buffer;
    u32 black = ToXRGB(0, 0, 0);

    for (int i = 0; i < pixels; i++)
        buffer[i] = black;
}

u8 Video::ReadVRAMTwoPageCallback(void* device, u32 offset)
{
    Video* video = (Video*)device;
    return video->ReadVRAMTwoPage(offset);
}

u8 Video::PeekVRAMTwoPageCallback(void* device, u32 offset)
{
    const Video* video = (const Video*)device;
    return video->ReadVRAMTwoPage(offset);
}

void Video::WriteVRAMTwoPageCallback(void* device, u32 offset, u8 value)
{
    Video* video = (Video*)device;
    video->WriteVRAMTwoPage(offset, value);
}

u8 Video::ReadVRAMSinglePageCallback(void* device, u32 offset)
{
    Video* video = (Video*)device;
    return video->ReadVRAMSinglePage(offset);
}

u8 Video::PeekVRAMSinglePageCallback(void* device, u32 offset)
{
    const Video* video = (const Video*)device;
    return video->ReadVRAMSinglePage(offset);
}

void Video::WriteVRAMSinglePageCallback(void* device, u32 offset, u8 value)
{
    Video* video = (Video*)device;
    video->WriteVRAMSinglePage(offset, value);
}

u8 Video::ReadFMRPlanesCallback(void* device, u32 offset)
{
    Video* video = (Video*)device;
    return video->ReadFMRPlanes(offset);
}

u8 Video::PeekFMRPlanesCallback(void* device, u32 offset)
{
    const Video* video = (const Video*)device;
    return video->ReadFMRPlanes(offset);
}

void Video::WriteFMRPlanesCallback(void* device, u32 offset, u8 value)
{
    Video* video = (Video*)device;
    video->WriteFMRPlanes(offset, value);
}

u8 Video::ReadFMRTextCallback(void* device, u32 offset)
{
    Video* video = (Video*)device;
    return video->ReadFMRText(offset);
}

u8 Video::PeekFMRTextCallback(void* device, u32 offset)
{
    const Video* video = (const Video*)device;
    return video->ReadFMRText(offset);
}

void Video::WriteFMRTextCallback(void* device, u32 offset, u8 value)
{
    Video* video = (Video*)device;
    video->WriteFMRText(offset, value);
}

u8 Video::ReadFMRRegisterCallback(void* device, u32 offset)
{
    Video* video = (Video*)device;
    return video->ReadFMRRegister(offset, false);
}

u8 Video::PeekFMRRegisterCallback(void* device, u32 offset)
{
    Video* video = (Video*)device;
    return video->ReadFMRRegister(offset, true);
}

void Video::WriteFMRRegisterCallback(void* device, u32 offset, u8 value)
{
    Video* video = (Video*)device;
    video->WriteFMRRegister(offset, value);
}

u8 Video::ReadVRAMTwoPage(u32 offset) const
{
    return m_state.vram[offset & (VIDEO_VRAM_SIZE - 1)];
}

void Video::WriteVRAMTwoPage(u32 offset, u8 value)
{
    u8 mask = m_state.mask[offset & 0x03];
    u8& data = m_state.vram[offset & (VIDEO_VRAM_SIZE - 1)];
    data = (u8)((data & ~mask) | (value & mask));
}

u8 Video::ReadVRAMSinglePage(u32 offset) const
{
    return m_state.vram[SinglePageToCanonical(offset)];
}

void Video::WriteVRAMSinglePage(u32 offset, u8 value)
{
    u8 mask = m_state.mask[offset & 0x03];
    u8& data = m_state.vram[SinglePageToCanonical(offset)];
    data = (u8)((data & ~mask) | (value & mask));
}

// C0000-C7FFF reads the plane picked by mask bits 7-6
// one bit per pixel with the first pixel in bit 7
u8 Video::ReadFMRPlanes(u32 offset) const
{
    const u8* bytes = &m_state.vram[FMRToCanonical(offset)];
    int plane = (m_state.fmr_mask >> 6) & 0x03;
    u8 result = 0;

    for (int i = 0; i < 4; i++)
    {
        if ((bytes[i] & (0x01 << plane)) != 0)
            result |= (u8)(0x80 >> (i * 2));

        if ((bytes[i] & (0x10 << plane)) != 0)
            result |= (u8)(0x40 >> (i * 2));
    }

    return result;
}

// Writes set or clear every plane enabled in mask bits 3-0 and keep the others
void Video::WriteFMRPlanes(u32 offset, u8 value)
{
    u8* bytes = &m_state.vram[FMRToCanonical(offset)];
    u8 planes = m_state.fmr_mask & 0x0F;
    u8 enabled = (u8)(planes | (planes << 4));

    for (int i = 0; i < 4; i++)
    {
        u8 data = (u8)(bytes[i] & ~enabled);

        if ((value & (0x80 >> (i * 2))) != 0)
            data |= planes;

        if ((value & (0x40 >> (i * 2))) != 0)
            data |= (u8)(planes << 4);

        bytes[i] = data;
    }
}

// C8000-CAFFF is text RAM at the start of sprite RAM
// the ANK font covers CA000-CBFFF and hides C9000-C9FFF
u8 Video::ReadFMRText(u32 offset) const
{
    if (offset < 0x1000)
        return m_state.sprite_ram[offset];

    if (m_state.fmr_ank)
    {
        if (offset >= 0x2000 && offset < 0x4000)
            return m_font_rom[(offset < 0x3000 ? 0x3D000 : 0x3D800) + (offset & 0x0FFF)];

        return 0xFF;
    }

    return offset < 0x3000 ? m_state.sprite_ram[offset] : 0xFF;
}

void Video::WriteFMRText(u32 offset, u8 value)
{
    if (offset >= 0x3000)
        return;

    m_state.sprite_ram[offset] = value;
    m_state.fmr_text_written = true;
}

// CF000-CFFFF, the registers sit at CFF80-CFFA0
void Video::WriteFMRRegister(u32 offset, u8 value)
{
    switch (offset)
    {
        case 0x0F81:
            m_state.fmr_mask = value;
            break;
        case 0x0F82:
            Synchronize(m_scheduler->GetClocks());
            m_state.fmr_display_planes = (u8)((value & 0x07) | ((value >> 2) & 0x08));
            m_state.fmr_display_page = (value & 0x10) != 0;
            break;
        case 0x0F83:
            m_state.fmr_page = (value & 0x10) != 0;
            break;
        case 0x0F94:
            m_state.kanji_high = value & 0x7F;
            break;
        case 0x0F95:
            m_state.kanji_low = value;
            m_state.kanji_row = 0;
            break;
        case 0x0F97:
            m_state.kanji_row = (m_state.kanji_row + 1) & 0x0F;
            break;
        case 0x0F98:
            m_pit->SetMemoryBuzzer(false);
            break;
        case 0x0F99:
            m_state.fmr_ank = (value & 0x01) != 0;
            break;
        default:
            break;
    }
}

// A peek skips the glyph row advance and the buzzer strobe
u8 Video::ReadFMRRegister(u32 offset, bool peek)
{
    switch (offset)
    {
        case 0x0F81:
            return m_state.fmr_mask;
        case 0x0F83:
            return m_state.fmr_page ? 0x10 : 0x00;
        case 0x0F84:
            // No light pen, so FIRQ stays clear
            return 0x00;
        case 0x0F86:
        {
            u8 status = GetSyncStatus(m_scheduler->GetClocks());
            return 0x10 | ((status & 0x04) != 0 ? 0x04 : 0x00) | ((status & 0x02) != 0 ? 0x80 : 0x00);
        }
        case 0x0F94:
            // Level 2 kanji are present
            return 0x80;
        case 0x0F96:
            return m_font_rom[GetKanjiOffset()];
        case 0x0F97:
        {
            u8 value = m_font_rom[GetKanjiOffset() + 1];

            if (!peek)
                m_state.kanji_row = (m_state.kanji_row + 1) & 0x0F;

            return value;
        }
        case 0x0F98:
            if (!peek)
                m_pit->SetMemoryBuzzer(true);

            return 0xFF;
        case 0x0FA0:
            // No logical operation unit, so ESTART stays clear
            return 0x00;
        default:
            return 0xFF;
    }
}

void Video::WriteCRTC(u8 value, bool high, u64 clocks)
{
    int index = m_state.crtc_index;
    u16 previous = m_state.crtc[index];

    if (high)
        m_state.crtc[index] = (u16)((previous & 0x00FF) | (value << 8));
    else
        m_state.crtc[index] = (u16)((previous & 0xFF00) | value);

    if (index != k_video_crtc_cr0)
        return;

    bool start = (m_state.crtc[index] & 0x8000) != 0;

    if (start && !m_state.running)
        StartFrame(clocks);
    else if (!start && m_state.running)
    {
        m_state.running = false;
        UpdateNextEvent();
    }
}

u8 Video::ReadCRTC(bool high, u64 clocks) const
{
    int index = m_state.crtc_index;

    // FR reads the live sync and display status in its high byte
    if (index == k_video_crtc_fr && high)
        return GetSyncStatus(clocks);

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

// The sprite engine is brought to each event first
// so the rows drawn there see its VRAM writes
void Video::RunNextEvent()
{
    u64 clocks = m_next_event_clocks;
    u32 half_line = m_next_event_half_line;

    m_sprite->Synchronize(clocks);

    if (half_line >= m_state.frame_half_lines)
    {
        CompleteFrame(clocks);
        return;
    }

    m_state.event_half_line = half_line;

    if (half_line == m_state.frame_vsync_half_lines && m_sprite->IsEnabled() && !m_sprite->IsBusy())
        m_sprite->StartTransfer(clocks);

    if ((half_line & 0x01) == 0)
        RenderRows(half_line);

    UpdateNextEvent();
}

// Timing registers are latched at each frame start
// The vertical period counts VST + 1 half-lines
// 525 lines on the VGA preset and 262.5 on 15 kHz ones
void Video::StartFrame(u64 clocks)
{
    u32 vst1 = m_state.crtc[k_video_crtc_vst1];
    u32 vst2 = m_state.crtc[k_video_crtc_vst2];

    m_state.running = true;
    m_state.frame_start_clocks = clocks;
    m_state.frame_line_clocks = MAX((u32)m_state.crtc[k_video_crtc_hst] + 1, k_video_min_line_clocks);
    m_state.frame_half_lines = MAX((u32)m_state.crtc[k_video_crtc_vst] + 1, k_video_min_half_lines);
    m_state.frame_vsync_half_lines = CLAMP(vst2 > vst1 ? vst2 - vst1 : 1, 1U, m_state.frame_half_lines - 1);
    m_state.frame_clock_rate = k_video_clock_rates[m_state.crtc[k_video_crtc_cr1] & 0x03];
    m_state.event_half_line = 0;
    m_rendered_rows = 0;
    UpdateGeometry();
    UpdateNextEvent();
}

void Video::CompleteFrame(u64 clocks)
{
    for (; m_rendered_rows < m_render_height; m_rendered_rows++)
        RenderRow(m_rendered_rows);

    m_frame_width = m_render_width;
    m_frame_height = m_render_height;
    m_frame_ready = true;
    m_state.frame_count++;
    m_state.vsync_irq = true;
    UpdateIRQ();

    if (IsValidPointer(m_trace_logger) && m_trace_logger->IsEnabled(TRACE_VIDEO))
        m_trace_logger->Record(TRACE_VIDEO, TRACE_VIDEO_VSYNC)->video.frame = m_state.frame_count;
    StartFrame(clocks);
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
        m_next_event_clocks = GT_NO_EVENT;
        m_scheduler->Schedule(SCHEDULER_EVENT_VIDEO, m_next_event_clocks);
        return;
    }

    // Every line end, the end of VSYNC and the end of the frame
    u32 half_line = m_state.event_half_line;
    u32 next = (half_line + 2) & ~0x01U;

    if (half_line < m_state.frame_vsync_half_lines)
        next = MIN(next, m_state.frame_vsync_half_lines);

    m_next_event_half_line = MIN(next, m_state.frame_half_lines);
    m_next_event_clocks = GetHalfLineClocks(m_next_event_half_line);
    m_scheduler->Schedule(SCHEDULER_EVENT_VIDEO, m_next_event_clocks);
}

void Video::UpdateIRQ()
{
    m_pic->SetIRQLine(11, m_state.vsync_irq);
}

// Same bit layout as the FR status byte
u8 Video::GetSyncStatus(u64 clocks) const
{
    if (!m_state.running)
        return 0x00;

    const u16* crtc = m_state.crtc;
    u32 half_line = GetBeamHalfLine(clocks);
    u32 clock = GetBeamClock(clocks);
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

void Video::RenderRows(u32 half_line)
{
    u32 beam = m_state.crtc[k_video_crtc_vst1] + half_line;

    if (beam <= m_canvas_v_start)
        return;

    int rows = (int)((beam - m_canvas_v_start) / 2);

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

    int width = m_render_width;
    u32* destination = (u32*)m_frame_buffer + row * width;
    u32 black = ToXRGB(0, 0, 0);

    for (int x = 0; x < width; x++)
        destination[x] = black;

    if (IsTwoPage())
    {
        // The back layer is drawn opaque and the front one lets transparent pixels through
        int front = m_state.output[1] & 0x01;
        RenderLayerRow(destination, front ^ 1, row, true);
        RenderLayerRow(destination, front, row, false);
    }
    else
        RenderLayerRow(destination, 0, row, true);
}

void Video::RenderLayerRow(u32* destination, int layer, int row, bool opaque)
{
    if (GetLayerFormat(layer) == VIDEO_LAYER_OFF)
        return;

    const u16* crtc = m_state.crtc;
    u32 field_line = m_canvas_interlaced ? (u32)row >> 1 : (u32)row;
    u32 half_line = m_canvas_v_start + field_line * 2;
    u32 vds = crtc[k_video_crtc_vds0 + layer * 2];
    u32 vde = crtc[k_video_crtc_vde0 + layer * 2];

    if (half_line < vds || half_line >= vde)
        return;

    // Fetching starts at HAJ, so data before HDS has already been consumed when the window opens
    u32 hds = crtc[k_video_crtc_hds0 + layer * 2];
    u32 hde = crtc[k_video_crtc_hde0 + layer * 2];
    u32 haj = crtc[k_video_crtc_haj0 + layer * 4];
    u32 first = MAX(hds, haj);
    u32 h_start = m_canvas_h_start;
    u32 divider = m_canvas_h_divider;
    int x = first > h_start ? (int)((first - h_start + divider - 1) / divider) : 0;
    int end = hde > h_start ? (int)MIN((hde - h_start + divider - 1) / divider, (u32)m_render_width) : 0;

    if (x >= end)
        return;

    u32 zoom = (u32)crtc[k_video_crtc_zoom] >> (layer * 8);
    u32 zoom_x = (zoom & 0x0F) + 1;
    u32 zoom_y = ((zoom >> 4) & 0x0F) + 1;
    u32 unit = IsTwoPage() ? 4 : 8;
    u32 line = ((half_line - vds) / 2) / zoom_y;
    u32 start = ((u32)crtc[k_video_crtc_fa0 + layer * 4] + line * crtc[k_video_crtc_lo0 + layer * 4]) * unit;

    // The second field of an interlaced frame starts FO further on
    if (m_canvas_interlaced && (row & 0x01) != 0)
        start += (u32)crtc[k_video_crtc_fo0 + layer * 4] * unit;

    // The FM-R display mode picks the page and hides planes of layer 0
    u8 planes = 0x0F;

    if (layer == 0)
    {
        planes = m_state.fmr_display_planes;

        if (m_state.fmr_display_page)
            start += 0x20000;
    }
    else
        start += m_sprite->GetDisplayOffset();

    u32 position = h_start + (u32)x * divider - haj;
    u32 colors[GT_MAX_FRAME_BUFFER_WIDTH + 4];

    if (divider != 1)
    {
        for (; x < end; x++, position += divider)
        {
            u32 pixel = position / zoom_x;
            DecodeLayerPixels(colors, layer, start, pixel, 1, planes, opaque);
            destination[x] = BlendLayerColor(colors[pixel & 0x01], destination[x]);
        }

        return;
    }

    u32 pixel = position / zoom_x;
    u32 count = (position + (u32)(end - x) - 1) / zoom_x - pixel + 1;
    DecodeLayerPixels(colors, layer, start, pixel, count, planes, opaque);
    const u32* source = colors + (pixel & 0x01);

    if (zoom_x == 1)
    {
        for (; x < end; x++, source++)
            destination[x] = BlendLayerColor(*source, destination[x]);

        return;
    }

    // Each pixel covers ZOOM outputs
    u32 phase = position % zoom_x;

    if (zoom_x == 2)
    {
        if (phase != 0)
        {
            destination[x] = BlendLayerColor(*source, destination[x]);
            x++;
            source++;
        }

        for (; x + 1 < end; x += 2, source++)
        {
            destination[x] = BlendLayerColor(*source, destination[x]);
            destination[x + 1] = BlendLayerColor(*source, destination[x + 1]);
        }

        if (x < end)
            destination[x] = BlendLayerColor(*source, destination[x]);

        return;
    }

    for (; x < end; x++)
    {
        destination[x] = BlendLayerColor(*source, destination[x]);

        if (++phase == zoom_x)
        {
            phase = 0;
            source++;
        }
    }
}

// Decoding starts at an even pixel so 4 bpp layers read whole bytes
// Pixels the layer lets the back one through are k_video_transparent
void Video::DecodeLayerPixels(u32* colors, int layer, u32 start, u32 pixel, u32 count, u8 planes, bool opaque)
{
    u32 even = pixel & ~0x01U;
    count += pixel & 0x01;

    switch (GetLayerFormat(layer))
    {
        case VIDEO_LAYER_4BPP:
            DecodeLayerPixelsTemplate<VIDEO_LAYER_4BPP, true>(colors, layer, start, even, count, planes, opaque);
            break;
        case VIDEO_LAYER_8BPP:
            DecodeLayerPixelsTemplate<VIDEO_LAYER_8BPP, false>(colors, layer, start, even, count, planes, opaque);
            break;
        default:
            if (IsTwoPage())
                DecodeLayerPixelsTemplate<VIDEO_LAYER_16BPP, true>(colors, layer, start, even, count, planes, opaque);
            else
                DecodeLayerPixelsTemplate<VIDEO_LAYER_16BPP, false>(colors, layer, start, even, count, planes, opaque);
            break;
    }
}

// 4 bpp layers only exist in two page mode and 8 bpp ones in single page mode
template<Video::Video_Layer_Format format, bool two_page>
void Video::DecodeLayerPixelsTemplate(u32* colors, int layer, u32 start, u32 pixel, u32 count, u8 planes, bool opaque)
{
    const u8* page = m_state.vram + (two_page ? (u32)layer << 18 : 0);

    if (format == VIDEO_LAYER_4BPP)
    {
        const u32* palette = m_palette16_colors[layer];

        for (u32 i = 0; i < count; i += 2, pixel += 2)
        {
            u8 data = ReadLayerVRAM<two_page>(page, start + (pixel >> 1));
            u8 low = data & planes;
            u8 high = (data >> 4) & planes;
            u32 low_color = palette[low];
            u32 high_color = palette[high];
            colors[i] = opaque || low != 0 ? low_color : k_video_transparent;
            colors[i + 1] = opaque || high != 0 ? high_color : k_video_transparent;
        }
    }
    else if (format == VIDEO_LAYER_8BPP)
    {
        for (u32 i = 0; i < count; i++, pixel++)
        {
            u8 index = ReadLayerVRAM<two_page>(page, start + pixel);
            u32 color = m_palette256_colors[index];
            colors[i] = opaque || index != 0 ? color : k_video_transparent;
        }
    }
    else
    {
        for (u32 i = 0; i < count; i++, pixel++)
        {
            u32 address = start + pixel * 2;
            u16 value = (u16)(ReadLayerVRAM<two_page>(page, address) |
                (ReadLayerVRAM<two_page>(page, address + 1) << 8));
            u32 color = m_direct_colors[value & 0x7FFF];
            colors[i] = opaque || (value & 0x8000) == 0 ? color : k_video_transparent;
        }
    }
}

// Palette components are kept in port order: blue, red, green
void Video::UpdatePaletteColor(int bank, int index)
{
    if (bank < 2)
    {
        const u8* color = m_state.palette16[bank][index];
        m_palette16_colors[bank][index] = ToXRGB(color[1] | (color[1] >> 4), color[2] | (color[2] >> 4),
            color[0] | (color[0] >> 4));
    }
    else
    {
        const u8* color = m_state.palette256[index];
        m_palette256_colors[index] = ToXRGB(color[1], color[2], color[0]);
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
        m_direct_colors[i] = ToXRGB(Expand5((i >> 5) & 0x1F), Expand5((i >> 10) & 0x1F), Expand5(i & 0x1F));
}

void Video::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    m_sprite->SaveState(stream);
}

void Video::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    m_sprite->LoadState(stream);
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
    G_SERIALIZE(serializer, m_state.frame_start_clocks);
    G_SERIALIZE(serializer, m_state.frame_line_clocks);
    G_SERIALIZE(serializer, m_state.frame_half_lines);
    G_SERIALIZE(serializer, m_state.frame_vsync_half_lines);
    G_SERIALIZE(serializer, m_state.frame_clock_rate);
    G_SERIALIZE(serializer, m_state.frame_count);
    G_SERIALIZE(serializer, m_state.event_half_line);
    G_SERIALIZE(serializer, m_state.fmr_mask);
    G_SERIALIZE(serializer, m_state.fmr_page);
    G_SERIALIZE(serializer, m_state.fmr_display_planes);
    G_SERIALIZE(serializer, m_state.fmr_display_page);
    G_SERIALIZE(serializer, m_state.fmr_ank);
    G_SERIALIZE(serializer, m_state.fmr_text_written);
    G_SERIALIZE(serializer, m_state.kanji_high);
    G_SERIALIZE(serializer, m_state.kanji_low);
    G_SERIALIZE(serializer, m_state.kanji_row);
}

void Video::SanitizeState()
{
    m_state.crtc_index &= 0x1F;
    m_state.output_index &= 0x03;
    m_state.mask_index &= 0x03;
    m_state.fmr_display_planes &= 0x0F;
    m_state.kanji_high &= 0x7F;
    m_state.kanji_row &= 0x0F;
    m_state.frame_line_clocks = MAX(m_state.frame_line_clocks, k_video_min_line_clocks);
    m_state.frame_half_lines = MAX(m_state.frame_half_lines, k_video_min_half_lines);
    m_state.frame_vsync_half_lines = CLAMP(m_state.frame_vsync_half_lines, 1U, m_state.frame_half_lines - 1);
    m_state.event_half_line = MIN(m_state.event_half_line, m_state.frame_half_lines - 1);

    if (m_state.frame_clock_rate == 0)
        m_state.frame_clock_rate = k_video_clock_rates[m_state.crtc[k_video_crtc_cr1] & 0x03];

    // The rows drawn before saving are not part of the state, the rest of the frame redraws them
    m_rendered_rows = 0;
    UpdateColorCaches();
    UpdateGeometry();
    UpdateNextEvent();
    UpdateIRQ();
}
