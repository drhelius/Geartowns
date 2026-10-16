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
#include "../system/memory.h"
#include "../common/state_serializer.h"

Video::Video()
{
    InitPointer(m_sprite);
    InitPointer(m_pic);
    InitPointer(m_trace_logger);
    InitPointer(m_pit);
    InitPointer(m_scheduler);
    InitPointer(m_memory);
    InitPointer(m_font_rom);
    InitPointer(m_frame_buffer);
    m_features = 0;
    m_render = false;
    m_frame_ready = false;
    m_next_event_clocks = GT_NO_EVENT;
    m_next_event_half_line = 0;
    m_sprite_handler_offset = 0;
    m_beam_event_clocks = 0;
    m_beam_middle_clocks = 0;
    m_beam_event_crtc_clocks = 0;
    m_beam_event_dot = 0;
    m_beam_vst1 = 0;
    m_frame_width = GT_FRAME_BUFFER_WIDTH;
    m_frame_height = GT_FRAME_BUFFER_HEIGHT;
    m_display_width = GT_FRAME_BUFFER_WIDTH;
    m_display_height = GT_FRAME_BUFFER_HEIGHT;
    m_render_width = GT_FRAME_BUFFER_WIDTH;
    m_render_height = GT_FRAME_BUFFER_HEIGHT;
    m_rendered_rows = 0;
    m_line_changed = false;
    m_native_resolution = true;
    m_canvas_h_start = 0;
    m_canvas_v_start = 0;
    m_canvas_clocks = GT_FRAME_BUFFER_WIDTH;
    m_canvas_width = GT_FRAME_BUFFER_WIDTH;
    m_canvas_height = GT_FRAME_BUFFER_HEIGHT;
    m_canvas_zoom_x = 1;
    m_canvas_zoom_y = 1;
    m_canvas_zoom_key = 0;
    m_canvas_offset = 0;
    m_canvas_offset_key = 0;
    m_canvas_h_divider = 1;
    m_canvas_v_divider = 1;
    m_canvas_interlaced = false;
    m_canvas_split = false;
}

Video::~Video()
{
    SafeDelete(m_sprite);
}

void Video::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;

    if (IsValidPointer(m_sprite))
        m_sprite->SetTraceLogger(trace_logger);
}

void Video::Init(PIC* pic, PIT* pit, Scheduler* scheduler, Memory* memory, const u8* font_rom)
{
    m_pic = pic;
    m_pit = pit;
    m_scheduler = scheduler;
    m_memory = memory;
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
    m_display_width = GT_FRAME_BUFFER_WIDTH;
    m_display_height = GT_FRAME_BUFFER_HEIGHT;
    UpdateColorCaches();
    UpdateGeometry();
    UpdateNextEvent();
    UpdateIRQ();
}

void Video::SetFeatures(u32 features)
{
    m_features = features;
}

bool Video::Decodes(u16 port) const
{
    switch (port)
    {
        case 0x0022:
            return (m_features & GT_MACHINE_FEATURE_CRT_POWER_OFF) != 0;
        case 0xFDA2:
        case 0xFDA4:
            return (m_features & GT_MACHINE_FEATURE_UG_REGISTERS) != 0;
        default:
            return true;
    }
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
        case 0xFDA2:
        case 0xFDA4:
            return Peek(port, clocks);
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
        case 0xFDA2:
            return (Decodes(port) && m_state.crt_readback) ? (m_state.display_enable & 0x0F) : 0xFF;
        case 0xFDA4:
            return Decodes(port) ? (m_state.crt_readback ? 0x01 : 0x00) : 0xFF;
        default:
            return 0xFF;
    }
}

// After a read only busy changes on its own: the palette flag needs a write and the page flips at a video event
u64 Video::GetStatusStableUntil() const
{
    if (m_state.digital_palette_modified)
        return 0;

    return m_sprite->IsBusy() ? m_sprite->GetFinishClocks() : GT_NO_EVENT;
}

// FDA0 changes at the next HSYNC edge or at the half-line that ends VSYNC, new frames start at events
u64 Video::GetSyncStableUntil(u64 clocks) const
{
    if (!m_state.running)
        return GT_NO_EVENT;

    const u16* crtc = m_state.crtc;
    u32 line = m_state.frame_line_clocks;
    u64 rate = m_state.frame_clock_rate;
    u64 until = GT_NO_EVENT;
    u64 half_line = ((clocks - m_state.frame_start_clocks) * rate * 2) / ((u64)GT_MACHINE_CLOCK_RATE * line);
    s64 vsync_half_lines = (s64)crtc[k_video_crtc_vst2] - (s64)crtc[k_video_crtc_vst1];

    if ((s64)half_line < vsync_half_lines)
        until = GetHalfLineClocks((u32)vsync_half_lines);

    if (crtc[k_video_crtc_hsw1] != 0)
    {
        u64 crtc_clocks = GetElapsedCRTCClocks(clocks);
        u64 origin = ((u64)crtc[k_video_crtc_vst1] * line) / 2;
        u32 clock = (u32)((origin + crtc_clocks) % line);
        u64 edge = crtc_clocks + (clock < crtc[k_video_crtc_hsw1] ? crtc[k_video_crtc_hsw1] - clock : line - clock);
        until = MIN(until, m_state.frame_start_clocks + (edge * GT_MACHINE_CLOCK_RATE + rate - 1) / rate);
    }

    return until;
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
        {
            // PLT, YS and YM don't change the picture
            u8 mask = m_state.output_index == 1 ? 0x01 : 0xFF;

            if (((m_state.output[m_state.output_index] ^ value) & mask) != 0)
                m_line_changed = true;

            m_state.output[m_state.output_index] = value;
            break;
        }
        case 0x0450:
        case 0x0452:
        {
            u32 offset = m_sprite->GetDisplayOffset();
            m_sprite->Write(port, value);

            if (m_sprite->GetDisplayOffset() != offset)
                m_line_changed = true;

            break;
        }
        case 0x0458:
            m_state.mask_index = value & 0x03;
            break;
        case 0x045A:
            m_state.mask[(m_state.mask_index & 0x01) * 2] = value;
            UpdateVRAMMapping();
            break;
        case 0x045B:
            m_state.mask[(m_state.mask_index & 0x01) * 2 + 1] = value;
            UpdateVRAMMapping();
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
            if (m_state.display_enable != value)
                m_line_changed = true;

            m_state.display_enable = value;
            break;
        case 0xFDA4:
            if (Decodes(port))
                m_state.crt_readback = (value & 0x01) != 0;
            break;
        case 0x0022:
            if (Decodes(port) && m_state.crt_power_off != ((value & 0x80) != 0))
            {
                m_state.crt_power_off = (value & 0x80) != 0;
                m_line_changed = true;
            }
            break;
        default:
            break;
    }

    if (unlikely(IsValidPointer(m_trace_logger) && m_trace_logger->IsEnabled(TRACE_VIDEO)))
        TraceWrite(port, value, clocks);
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
    m_display_width = GT_FRAME_BUFFER_WIDTH;
    m_display_height = GT_FRAME_BUFFER_HEIGHT;

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
    video->SynchronizeSpriteWork(offset & (VIDEO_VRAM_SIZE - 1), 1);
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

void Video::WriteVRAMTwoPage16Callback(void* device, u32 offset, u16 value)
{
    Video* video = (Video*)device;
    video->WriteVRAMTwoPage16(offset, value);
}

void Video::WriteVRAMTwoPage32Callback(void* device, u32 offset, u32 value)
{
    Video* video = (Video*)device;
    video->WriteVRAMTwoPage32(offset, value);
}

u8 Video::ReadVRAMSinglePageCallback(void* device, u32 offset)
{
    Video* video = (Video*)device;
    video->SynchronizeSpriteWork(video->SinglePageToCanonical(offset), 1);
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

void Video::WriteVRAMSinglePage16Callback(void* device, u32 offset, u16 value)
{
    Video* video = (Video*)device;
    video->WriteVRAMSinglePage16(offset, value);
}

void Video::WriteVRAMSinglePage32Callback(void* device, u32 offset, u32 value)
{
    Video* video = (Video*)device;
    video->WriteVRAMSinglePage32(offset, value);
}

u8 Video::ReadSpriteRAMCallback(void* device, u32 offset)
{
    const Video* video = (const Video*)device;
    return video->m_state.sprite_ram[offset];
}

// The engine only reads sprite RAM, so a write that keeps the data can't change what it draws
void Video::WriteSpriteRAMCallback(void* device, u32 offset, u8 value)
{
    Video* video = (Video*)device;
    u8* data = &video->m_state.sprite_ram[offset];

    if (*data != value)
    {
        video->SynchronizeSpriteRAM();
        *data = value;
    }
}

void Video::WriteSpriteRAM16Callback(void* device, u32 offset, u16 value)
{
    Video* video = (Video*)device;
    u8* data = &video->m_state.sprite_ram[offset];

    if (read_u16_le(data) != value)
    {
        video->SynchronizeSpriteRAM();
        write_u16_le(data, value);
    }
}

void Video::WriteSpriteRAM32Callback(void* device, u32 offset, u32 value)
{
    Video* video = (Video*)device;
    u8* data = &video->m_state.sprite_ram[offset];

    if (read_u32_le(data) != value)
    {
        video->SynchronizeSpriteRAM();
        write_u32_le(data, value);
    }
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

void Video::UpdateVRAMMapping()
{
    u8* write_data = read_u32_le(m_state.mask) == 0xFFFFFFFF ? m_state.vram : NULL;
    m_memory->SetRegionData(GT_DEBUG_REGION_VRAM_TWO_PAGE, m_state.vram, write_data);
    UpdateSpriteMapping();
}

// While a transfer runs, CPU writes to sprite RAM and accesses to the half being drawn reach the handlers
void Video::UpdateSpriteMapping()
{
    u32 work = GetSpriteHandlerOffset();
    bool busy = work != 0;
    m_sprite_handler_offset = work;
    m_memory->SetRegionHandlerRange(GT_DEBUG_REGION_SPRITE_RAM, 0, busy ? VIDEO_SPRITE_RAM_SIZE : 0, false);
    m_memory->SetRegionHandlerRange(GT_DEBUG_REGION_VRAM_TWO_PAGE, work, busy ? k_sprite_half_size : 0, true);
}

// Machine time moves up to the access first, then the engine catches up with it
NO_INLINE void Video::SynchronizeSpriteAccess()
{
    m_memory->SynchronizeAccess();
    Synchronize(m_scheduler->GetClocks());
}

u8 Video::ReadVRAMTwoPage(u32 offset) const
{
    return m_state.vram[offset & (VIDEO_VRAM_SIZE - 1)];
}

void Video::WriteVRAMTwoPage(u32 offset, u8 value)
{
    SynchronizeSpriteWork(offset & (VIDEO_VRAM_SIZE - 1), 1);
    u8 mask = m_state.mask[offset & 0x03];
    u8& data = m_state.vram[offset & (VIDEO_VRAM_SIZE - 1)];
    data = (u8)((data & ~mask) | (value & mask));
}

void Video::WriteVRAMTwoPage16(u32 offset, u16 value)
{
    SynchronizeSpriteWork(offset & (VIDEO_VRAM_SIZE - 1), 2);
    u16 mask = (u16)GetVRAMMask(offset);
    u8* data = &m_state.vram[offset & (VIDEO_VRAM_SIZE - 1)];
    write_u16_le(data, (u16)((read_u16_le(data) & ~mask) | (value & mask)));
}

void Video::WriteVRAMTwoPage32(u32 offset, u32 value)
{
    SynchronizeSpriteWork(offset & (VIDEO_VRAM_SIZE - 1), 4);
    u32 mask = GetVRAMMask(offset);
    u8* data = &m_state.vram[offset & (VIDEO_VRAM_SIZE - 1)];
    write_u32_le(data, (read_u32_le(data) & ~mask) | (value & mask));
}

u8 Video::ReadVRAMSinglePage(u32 offset) const
{
    return m_state.vram[SinglePageToCanonical(offset)];
}

void Video::WriteVRAMSinglePage(u32 offset, u8 value)
{
    u32 canonical = SinglePageToCanonical(offset);
    SynchronizeSpriteWork(canonical, 1);
    u8 mask = m_state.mask[offset & 0x03];
    u8& data = m_state.vram[canonical];
    data = (u8)((data & ~mask) | (value & mask));
}

void Video::WriteVRAMSinglePage16(u32 offset, u16 value)
{
    if ((offset & 0x03) == 0x03)
    {
        WriteVRAMSinglePage(offset, (u8)value);
        WriteVRAMSinglePage(offset + 1, (u8)(value >> 8));
        return;
    }

    u32 canonical = SinglePageToCanonical(offset);
    SynchronizeSpriteWork(canonical, 2);
    u16 mask = (u16)GetVRAMMask(offset);
    u8* data = &m_state.vram[canonical];
    write_u16_le(data, (u16)((read_u16_le(data) & ~mask) | (value & mask)));
}

void Video::WriteVRAMSinglePage32(u32 offset, u32 value)
{
    if ((offset & 0x03) != 0)
    {
        WriteVRAMSinglePage16(offset, (u16)value);
        WriteVRAMSinglePage16(offset + 2, (u16)(value >> 16));
        return;
    }

    u32 canonical = SinglePageToCanonical(offset);
    SynchronizeSpriteWork(canonical, 4);
    u32 mask = read_u32_le(m_state.mask);
    u8* data = &m_state.vram[canonical];
    write_u32_le(data, (read_u32_le(data) & ~mask) | (value & mask));
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

    SynchronizeSpriteRAM();
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
        {
            Synchronize(m_scheduler->GetClocks());
            u8 planes = (u8)((value & 0x07) | ((value >> 2) & 0x08));
            bool page = (value & 0x10) != 0;

            if (planes != m_state.fmr_display_planes || page != m_state.fmr_display_page)
                m_line_changed = true;

            m_state.fmr_display_planes = planes;
            m_state.fmr_display_page = page;
            break;
        }
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
        case 0x0F9E:
            if (m_state.crt_readback)
                m_state.kanji_row = value & 0x0F;
            break;
        default:
            break;
    }

    bool fmr = offset == 0x0F81 || offset == 0x0F82 || offset == 0x0F83 || offset == 0x0F99;

    if (unlikely(fmr && IsValidPointer(m_trace_logger) && m_trace_logger->IsEventEnabled(TRACE_VIDEO, TRACE_VIDEO_FMR)))
        TraceEvent(TRACE_VIDEO_FMR, (u8)offset, value, value, 0, 0, m_scheduler->GetClocks());
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
        case 0x0F88:
            if (!m_state.crt_readback)
                return 0xFF;

            return 0x40 | (m_state.fmr_display_planes & 0x07) | ((m_state.fmr_display_planes & 0x08) << 2) |
                (m_state.fmr_display_page ? 0x10 : 0x00);
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
        case 0x0F99:
            return m_state.crt_readback ? (m_state.fmr_ank ? 0x01 : 0x00) : 0xFF;
        case 0x0F9C:
            return m_state.crt_readback ? m_state.kanji_high : 0xFF;
        case 0x0F9D:
            return m_state.crt_readback ? m_state.kanji_low : 0xFF;
        case 0x0F9E:
            return m_state.crt_readback ? m_state.kanji_row : 0xFF;
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

    if (m_state.crtc[index] != previous && index >= k_video_crtc_hds0 && index <= k_video_crtc_cr0)
        m_line_changed = true;

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
    u8 previous = ReadPalette(component);

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

    if (ReadPalette(component) != previous)
        m_line_changed = true;
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

    if (half_line == m_state.frame_vsync_half_lines && m_sprite->IsEnabled())
    {
        if (!m_sprite->IsBusy())
            m_sprite->StartTransfer(clocks);
        else
            m_sprite->TraceBusyAtVSync();
    }

    if (GetSpriteHandlerOffset() != m_sprite_handler_offset)
        UpdateSpriteMapping();

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
    m_line_changed = false;
    m_canvas_split = false;
    UpdateGeometry();
    UpdateNextEvent();
}

void Video::CompleteFrame(u64 clocks)
{
    UpdateCanvasZoom();

    for (; m_rendered_rows < m_render_height; m_rendered_rows++)
        RenderRow(m_rendered_rows);

    m_frame_width = m_render_width;
    m_frame_height = m_render_height;
    m_display_width = m_canvas_width;
    m_display_height = m_canvas_height;
    m_frame_ready = true;
    m_state.frame_count++;
    m_state.vsync_irq = true;

    if (unlikely(IsValidPointer(m_trace_logger) && m_trace_logger->IsEventEnabled(TRACE_VIDEO, TRACE_VIDEO_VSYNC)))
        TraceEvent(TRACE_VIDEO_VSYNC, 0, 0, 0, 0, m_state.frame_count, clocks);

    UpdateIRQ();
    StartFrame(clocks);
}

// Index writes only select, so the events are the data writes with what the register holds afterwards
void Video::TraceWrite(u16 port, u8 value, u64 clocks)
{
    switch (port)
    {
        case 0x0442:
        case 0x0443:
        {
            u8 reg = m_state.crtc_index;
            TraceEvent(TRACE_VIDEO_CRTC, reg, m_state.crtc[reg], value, port & 0x01, 0, clocks);
            break;
        }
        case 0x044A:
            TraceEvent(TRACE_VIDEO_OUTPUT, m_state.output_index, value, value, 0, 0, clocks);
            break;
        case 0x045A:
        case 0x045B:
        {
            const u8* mask = m_state.mask;
            u32 full = mask[0] | (mask[1] << 8) | (mask[2] << 16) | ((u32)mask[3] << 24);
            TraceEvent(TRACE_VIDEO_MASK, m_state.mask_index, value, value, port & 0x01, full, clocks);
            break;
        }
        case 0x05CA:
            TraceEvent(TRACE_VIDEO_VSYNC_CLEAR, 0, value, value, 0, 0, clocks);
            break;
        case 0xFD92:
        case 0xFD94:
        case 0xFD96:
        {
            u8 index = m_state.palette_index;
            u8 bank = (m_state.output[1] >> 4) & 0x03;
            const u8* color = bank == 0 ? m_state.palette16[0][index & 0x0F] : bank == 2 ?
                m_state.palette16[1][index & 0x0F] : m_state.palette256[index];
            u32 rgb = ((u32)color[1] << 16) | ((u32)color[2] << 8) | color[0];
            TraceEvent(TRACE_VIDEO_PALETTE, index, (port - 0xFD92) >> 1, value, bank, rgb, clocks);
            break;
        }
        case 0xFD98:
        case 0xFD99:
        case 0xFD9A:
        case 0xFD9B:
        case 0xFD9C:
        case 0xFD9D:
        case 0xFD9E:
        case 0xFD9F:
            TraceEvent(TRACE_VIDEO_DIGITAL_PALETTE, (u8)(port - 0xFD98), value & 0x0F, value, 0, 0, clocks);
            break;
        case 0xFDA0:
            TraceEvent(TRACE_VIDEO_DISPLAY, 0, value, value, 0, 0, clocks);
            break;
        default:
            break;
    }
}

void Video::TraceEvent(u8 event, u8 reg, u16 value, u8 raw, u8 bank, u32 param, u64 clocks)
{
    if (!m_trace_logger->IsEventEnabled(TRACE_VIDEO, event))
        return;

    GT_Trace_Entry entry = {};
    entry.type = TRACE_VIDEO;
    entry.event = event;
    entry.video.param = param;
    entry.video.value = value;
    entry.video.reg = reg;
    entry.video.raw = raw;
    entry.video.bank = bank;

    entry.video.line = 0xFFFF;

    if (m_state.running)
    {
        entry.video.line = (u16)(GetBeamHalfLine(clocks) / 2);
        entry.video.dot = (u16)GetBeamClock(clocks);
    }

    m_trace_logger->TraceLog(entry);
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

    u32 zoom_x = m_canvas_zoom_x;
    u32 zoom_y = m_canvas_zoom_y;
    m_canvas_zoom_key = GetLayerZoomKey();

    if (!m_state.running || h_end <= h_start || v_end <= v_start)
    {
        m_canvas_h_start = 0;
        m_canvas_v_start = 0;
        m_canvas_clocks = GT_FRAME_BUFFER_WIDTH;
        m_canvas_width = GT_FRAME_BUFFER_WIDTH;
        m_canvas_height = GT_FRAME_BUFFER_HEIGHT;
        SetCanvasZoom(1, 1);
        return;
    }

    // The canvas covers both display windows, sampled at a whole number of CRTC clocks per output pixel
    u32 rows = (v_end - v_start) / 2;

    if (m_canvas_interlaced)
        rows *= 2;

    m_canvas_h_start = h_start;
    m_canvas_v_start = v_start;
    m_canvas_clocks = h_end - h_start;
    m_canvas_height = CLAMP((int)rows, 1, GT_MAX_FRAME_BUFFER_HEIGHT);
    SetCanvasZoom(1, 1);
    m_canvas_width = m_render_width;

    if (!m_native_resolution)
        return;

    GetLayerZoom(&zoom_x, &zoom_y);
    SetCanvasZoom(zoom_x, zoom_y);
}

// A layer whose HAJ is part of a pixel away from the canvas start needs a finer zoom
// The finest one stays until the zoom, formats or enables change, so scrolling doesn't flip the size
void Video::UpdateCanvasOffset()
{
    if (!m_native_resolution)
        return;

    if (m_canvas_zoom_key != m_canvas_offset_key)
    {
        m_canvas_offset_key = m_canvas_zoom_key;
        m_canvas_offset = 0;
    }

    m_canvas_offset = Gcd(m_canvas_offset, GetLayerOffset());
    SetCanvasZoom(Gcd(m_canvas_zoom_x, m_canvas_offset), m_canvas_zoom_y);
}

void Video::UpdateCanvasZoom()
{
    if (!m_native_resolution)
        return;

    u32 key = GetLayerZoomKey();

    if (key == m_canvas_zoom_key)
        return;

    m_canvas_zoom_key = key;

    u32 zoom_x = m_canvas_zoom_x;
    u32 zoom_y = m_canvas_zoom_y;

    if (!GetLayerZoom(&zoom_x, &zoom_y))
        return;

    if (m_rendered_rows == 0)
        SetCanvasZoom(zoom_x, zoom_y);
    else
        ResizeCanvas(CommonZoom(zoom_x, m_canvas_zoom_x), CommonZoom(zoom_y, m_canvas_zoom_y));
}

// The rows already drawn are widened in place to the new dividers
void Video::ResizeCanvas(u32 zoom_x, u32 zoom_y)
{
    u32 h_divider = m_canvas_h_divider;
    u32 v_divider = m_canvas_v_divider;
    int width = m_render_width;
    int rows = m_rendered_rows;

    SetCanvasZoom(zoom_x, zoom_y);

    if (m_canvas_h_divider == h_divider && m_canvas_v_divider == v_divider)
        return;

    u32 new_h_divider = m_canvas_h_divider;
    u32 new_v_divider = m_canvas_v_divider;
    int new_width = m_render_width;
    m_rendered_rows = MIN((int)(((u32)rows * v_divider + new_v_divider - 1) / new_v_divider), m_render_height);

    if (!m_render || !IsValidPointer(m_frame_buffer))
        return;

    u32* buffer = (u32*)m_frame_buffer;

    for (int y = m_rendered_rows - 1; y >= 0; y--)
    {
        const u32* source = buffer + (int)(((u32)y * new_v_divider) / v_divider) * width;
        u32* destination = buffer + y * new_width;

        for (int x = new_width - 1; x >= 0; x--)
            destination[x] = source[MIN((int)(((u32)x * new_h_divider) / h_divider), width - 1)];
    }
}

void Video::SetCanvasZoom(u32 zoom_x, u32 zoom_y)
{
    u32 fit = (m_canvas_clocks + GT_MAX_FRAME_BUFFER_WIDTH - 1) / GT_MAX_FRAME_BUFFER_WIDTH;

    m_canvas_zoom_x = zoom_x;
    m_canvas_zoom_y = zoom_y;
    m_canvas_h_divider = (zoom_x % fit) == 0 ? zoom_x : fit;
    m_canvas_v_divider = m_canvas_interlaced || m_canvas_split ? 1 : zoom_y;
    m_render_width = MAX((int)(m_canvas_clocks / m_canvas_h_divider), 1);
    m_render_height = MAX(m_canvas_height / (int)m_canvas_v_divider, 1);
}

bool Video::GetLayerZoom(u32* zoom_x, u32* zoom_y) const
{
    u32 zoom = m_state.crtc[k_video_crtc_zoom];
    int layers = IsTwoPage() ? 2 : 1;
    bool found = false;

    for (int layer = 0; layer < layers; layer++)
    {
        if (GetLayerFormat(layer) == VIDEO_LAYER_OFF)
            continue;

        u32 layer_zoom = zoom >> (layer * 8);
        u32 x = (layer_zoom & 0x0F) + 1;
        u32 y = ((layer_zoom >> 4) & 0x0F) + 1;

        *zoom_x = found ? CommonZoom(*zoom_x, x) : x;
        *zoom_y = found ? CommonZoom(*zoom_y, y) : y;
        found = true;
    }

    return found;
}

// Clocks from the canvas start to the pixel grid of the visible layers
u32 Video::GetLayerOffset() const
{
    int layers = IsTwoPage() ? 2 : 1;
    u32 offset = 0;

    for (int layer = 0; layer < layers; layer++)
    {
        if (GetLayerFormat(layer) == VIDEO_LAYER_OFF)
            continue;

        u32 haj = m_state.crtc[k_video_crtc_haj0 + layer * 4];
        offset = Gcd(offset, haj > m_canvas_h_start ? haj - m_canvas_h_start : m_canvas_h_start - haj);
    }

    return offset;
}

u32 Video::CommonZoom(u32 a, u32 b)
{
    if ((b % a) == 0)
        return a;

    if ((a % b) == 0)
        return b;

    return 1;
}

u32 Video::Gcd(u32 a, u32 b)
{
    while (b != 0)
    {
        u32 remainder = a % b;
        a = b;
        b = remainder;
    }

    return a;
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

    m_beam_event_clocks = GetHalfLineClocks(half_line);
    m_beam_middle_clocks = half_line + 1 < m_next_event_half_line ? GetHalfLineClocks(half_line + 1) : m_next_event_clocks;
    m_beam_event_crtc_clocks = GetElapsedCRTCClocks(m_beam_event_clocks);
    m_beam_event_dot = GetBeamClock(m_beam_event_clocks);
    m_beam_vst1 = m_state.crtc[k_video_crtc_vst1];
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
    u32 half_line;
    u32 clock;

    // Polling between two events starts from the position cached at the first one instead of dividing
    if (clocks >= m_beam_event_clocks && clocks < m_next_event_clocks && crtc[k_video_crtc_vst1] == m_beam_vst1)
    {
        half_line = m_beam_vst1 + m_state.event_half_line + (clocks >= m_beam_middle_clocks ? 1 : 0);
        clock = m_beam_event_dot + (u32)(GetElapsedCRTCClocks(clocks) - m_beam_event_crtc_clocks);

        while (clock >= m_state.frame_line_clocks)
            clock -= m_state.frame_line_clocks;
    }
    else
    {
        half_line = GetBeamHalfLine(clocks);
        clock = GetBeamClock(clocks);
    }

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
    bool changed = m_line_changed;
    m_line_changed = false;

    if (beam <= m_canvas_v_start)
        return;

    u32 lines = (beam - m_canvas_v_start) / 2;
    u32 divider = m_canvas_v_divider;

    // Rows are drawn at the end of their first line, so a change during a later one splits them from there on
    if (changed && lines > 0 && ((lines - 1) % divider) != 0 && (lines - 1) / divider < (u32)m_rendered_rows)
    {
        m_canvas_split = true;
        ResizeCanvas(m_canvas_zoom_x, m_canvas_zoom_y);
        m_rendered_rows = (int)lines - 1;
    }

    UpdateCanvasZoom();

    // HAJ is often set after VSYNC, so it is checked when the first row is drawn
    if (m_rendered_rows == 0)
        UpdateCanvasOffset();

    int rows = m_canvas_interlaced ? (int)(lines * 2) : (int)((lines + m_canvas_v_divider - 1) / m_canvas_v_divider);
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

    if (unlikely(m_state.crt_power_off))
        return;

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
    u32 field_line = m_canvas_interlaced ? (u32)row >> 1 : (u32)row * m_canvas_v_divider;
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

    // Without CEN the low 8 bits of the address counter don't carry, so rows turn around within 256 units
    u32 skip = 0;
    u32 wrap = k_video_no_wrap;

    if ((crtc[k_video_crtc_cr0] & (0x10 << layer)) == 0)
    {
        Video_Layer_Format format = GetLayerFormat(layer);
        u32 bits = format == VIDEO_LAYER_4BPP ? 4 : (format == VIDEO_LAYER_8BPP ? 8 : 16);
        u32 block = unit << 8;
        skip = ((start & (block - 1)) * 8) / bits;
        wrap = (block * 8) / bits - 1;
        start &= ~(block - 1);
    }

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
    u32 colors[GT_MAX_FRAME_BUFFER_WIDTH];

    if ((zoom_x % divider) != 0)
    {
        for (; x < end; x++, position += divider)
        {
            u32 pixel = (position / zoom_x + skip) & wrap;
            DecodeLayerPixels(colors, layer, start, pixel, 1, planes, opaque);
            destination[x] = BlendLayerColor(colors[0], destination[x]);
        }

        return;
    }

    position /= divider;
    zoom_x /= divider;

    u32 pixel = position / zoom_x;
    u32 count = (position + (u32)(end - x) - 1) / zoom_x - pixel + 1;
    pixel = (pixel + skip) & wrap;

    if (zoom_x == 1 && opaque)
    {
        DecodeLayerRun(destination + x, layer, start, pixel, count, wrap, planes, true);
        return;
    }

    DecodeLayerRun(colors, layer, start, pixel, count, wrap, planes, opaque);
    const u32* source = colors;

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

// A run that reaches the end of the block goes on from its beginning
void Video::DecodeLayerRun(u32* colors, int layer, u32 start, u32 pixel, u32 count, u32 wrap, u8 planes, bool opaque)
{
    while (pixel + count > wrap + 1)
    {
        u32 part = wrap + 1 - pixel;
        DecodeLayerPixels(colors, layer, start, pixel, part, planes, opaque);
        colors += part;
        count -= part;
        pixel = 0;
    }

    DecodeLayerPixels(colors, layer, start, pixel, count, planes, opaque);
}

// Pixels the layer lets the back one through are k_video_transparent
void Video::DecodeLayerPixels(u32* colors, int layer, u32 start, u32 pixel, u32 count, u8 planes, bool opaque)
{
    switch (GetLayerFormat(layer))
    {
        case VIDEO_LAYER_4BPP:
            DecodeLayerPixelsTemplate<VIDEO_LAYER_4BPP, true>(colors, layer, start, pixel, count, planes, opaque);
            break;
        case VIDEO_LAYER_8BPP:
            if (unlikely(layer == 0 && m_state.fmr_display_page))
                DecodeFMRPagePixels(colors, start, pixel, count, planes, opaque);
            else
                DecodeLayerPixelsTemplate<VIDEO_LAYER_8BPP, false>(colors, layer, start, pixel, count, planes, opaque);
            break;
        default:
            if (IsTwoPage())
                DecodeLayerPixelsTemplate<VIDEO_LAYER_16BPP, true>(colors, layer, start, pixel, count, planes, opaque);
            else if (unlikely(layer == 0 && m_state.fmr_display_page))
                DecodeFMRPagePixels(colors, start, pixel, count, planes, opaque);
            else
                DecodeLayerPixelsTemplate<VIDEO_LAYER_16BPP, false>(colors, layer, start, pixel, count, planes, opaque);
            break;
    }
}

// FM-R page 1 adds 20000h to the first VRAM half only
// Single page mode interleaves the halves every four bytes, so only the groups from the first half move
NO_INLINE void Video::DecodeFMRPagePixels(u32* colors, u32 start, u32 pixel, u32 count, u8 planes, bool opaque)
{
    u32 size = GetLayerFormat(0) == VIDEO_LAYER_8BPP ? 1 : 2;

    // The row start comes with the two page offset already added
    start -= 0x20000;

    for (u32 i = 0; i < count; i++, pixel++)
    {
        u32 flip = ((start + pixel * size) & 0x04) == 0 ? 0x40000 : 0;

        if (size == 1)
            DecodeLayerPixelsTemplate<VIDEO_LAYER_8BPP, false>(colors + i, 0, start ^ flip, pixel, 1, planes, opaque);
        else
            DecodeLayerPixelsTemplate<VIDEO_LAYER_16BPP, false>(colors + i, 0, start ^ flip, pixel, 1, planes, opaque);
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
        u32 i = 0;

        if ((pixel & 0x01) != 0 && count > 0)
        {
            u8 data = ReadLayerVRAM<two_page>(page, start + (pixel >> 1));
            u8 index = (data >> 4) & planes;
            colors[i++] = opaque || index != 0 ? palette[index] : k_video_transparent;
            pixel++;
        }

        for (; i + 1 < count; i += 2, pixel += 2)
        {
            u8 data = ReadLayerVRAM<two_page>(page, start + (pixel >> 1));
            u8 low = data & planes;
            u8 high = (data >> 4) & planes;
            u32 low_color = palette[low];
            u32 high_color = palette[high];
            colors[i] = opaque || low != 0 ? low_color : k_video_transparent;
            colors[i + 1] = opaque || high != 0 ? high_color : k_video_transparent;
        }

        if (i < count)
        {
            u8 data = ReadLayerVRAM<two_page>(page, start + (pixel >> 1));
            u8 index = data & planes;
            colors[i] = opaque || index != 0 ? palette[index] : k_video_transparent;
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
            u32 offset = two_page ? address & 0x3FFFF : SinglePageToCanonical(address);
            u16 value = read_u16_le(page + offset);
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
    Serialize(serializer, GT_SAVESTATE_VERSION);
    m_sprite->SaveState(stream);
}

void Video::LoadState(std::istream& stream, int version)
{
    StateSerializer serializer(stream);
    Serialize(serializer, version);
    m_sprite->LoadState(stream);
    SanitizeState();
}

void Video::Serialize(StateSerializer& serializer, int version)
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
    G_SERIALIZE(serializer, m_canvas_zoom_x);
    G_SERIALIZE(serializer, m_canvas_zoom_y);
    G_SERIALIZE(serializer, m_canvas_offset);
    G_SERIALIZE(serializer, m_canvas_offset_key);

    if (version >= 5)
    {
        G_SERIALIZE(serializer, m_state.crt_readback);
        G_SERIALIZE(serializer, m_state.crt_power_off);
    }
    else
    {
        m_state.crt_readback = false;
        m_state.crt_power_off = false;
    }
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

    m_canvas_zoom_x = CLAMP(m_canvas_zoom_x, 1U, 16U);
    m_canvas_zoom_y = CLAMP(m_canvas_zoom_y, 1U, 16U);

    // The rows drawn before saving are not part of the state, the rest of the frame redraws them
    m_rendered_rows = 0;
    m_canvas_split = false;
    UpdateColorCaches();
    UpdateGeometry();
    UpdateNextEvent();
    UpdateIRQ();
    UpdateVRAMMapping();
}
