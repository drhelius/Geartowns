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

#ifndef VIDEO_INLINE_H
#define VIDEO_INLINE_H

#include "video.h"
#include "../system/scheduler.h"

INLINE void Video::Synchronize(u64 clocks)
{
    while (clocks >= m_next_event_clocks)
        CompleteFrame();
}

INLINE void Video::HandleEvent(u64 clocks)
{
    Synchronize(clocks);
    m_scheduler->Schedule(SCHEDULER_EVENT_VIDEO, m_next_event_clocks);
}

INLINE bool Video::IsFrameReady() const
{
    return m_frame_ready;
}

INLINE bool Video::IsRunning() const
{
    return m_state.running;
}

INLINE int Video::GetFrameWidth() const
{
    return m_frame_width;
}

INLINE int Video::GetFrameHeight() const
{
    return m_frame_height;
}

// Milliseconds between VSYNCs with the timing latched at the start of the frame
INLINE float Video::GetFrameTime() const
{
    double clocks = ((double)m_state.frame_half_lines * m_state.frame_line_clocks) / 2.0;
    return (float)((clocks * 1000.0) / m_state.frame_clock_rate);
}

INLINE u8* Video::GetVRAM()
{
    return m_state.vram;
}

INLINE u8* Video::GetSpriteRAM()
{
    return m_state.sprite_ram;
}

INLINE Video::Video_State* Video::GetState()
{
    return &m_state;
}

INLINE bool Video::IsTwoPage() const
{
    return (m_state.output[0] & 0x10) != 0;
}

INLINE Video::Video_Layer_Format Video::GetLayerFormat(int layer) const
{
    u8 output = m_state.output[0];
    u8 enable = m_state.display_enable;

    if (IsTwoPage())
    {
        // FDA0 enables layer 0 with bits 3-2 and layer 1 with bits 1-0
        if (((enable >> (layer == 0 ? 2 : 0)) & 0x03) == 0)
            return VIDEO_LAYER_OFF;

        switch ((output >> (layer * 2)) & 0x03)
        {
            case 0x01:
                return VIDEO_LAYER_4BPP;
            case 0x03:
                return VIDEO_LAYER_16BPP;
            default:
                return VIDEO_LAYER_OFF;
        }
    }

    if (layer != 0 || (enable & 0x0C) == 0)
        return VIDEO_LAYER_OFF;

    switch (output & 0x0F)
    {
        case 0x0A:
            return VIDEO_LAYER_8BPP;
        case 0x0F:
            return VIDEO_LAYER_16BPP;
        default:
            return VIDEO_LAYER_OFF;
    }
}

INLINE u8 Video::ReadVRAM(u32 offset, bool two_page) const
{
    return m_state.vram[two_page ? offset : SinglePageToCanonical(offset)];
}

// The single page view interleaves four byte groups between the two 256 KiB halves
INLINE u32 Video::SinglePageToCanonical(u32 offset) const
{
    offset &= VIDEO_VRAM_SIZE - 1;
    return ((offset & 0x00004) << 16) | ((offset & 0x7FFF8) >> 1) | (offset & 0x00003);
}

// Each FM-R byte covers eight pixels, four packed 4 bpp bytes of layer 0
INLINE u32 Video::FMRToCanonical(u32 offset) const
{
    return ((offset & 0x7FFF) << 2) + (m_state.fmr_page ? 0x20000 : 0);
}

// Unsigned arithmetic like the reference, so any JIS code lands inside the 256 KiB font ROM
INLINE u32 Video::GetKanjiOffset() const
{
    u32 high = m_state.kanji_high;
    u32 low = m_state.kanji_low;
    u32 glyph;

    if (high < 0x28)
    {
        // 32x8 blocks with the second and third swapped
        u32 block = (low - 0x20) >> 5;

        if (block == 1)
            block = 2;
        else if (block == 2)
            block = 1;

        glyph = block * 256 + (high & 0x07) * 32 + (low & 0x1F);
    }
    else
    {
        u32 block = ((high - 0x30) >> 4) * 3 + ((low - 0x20) >> 5);
        glyph = 0x400 + block * 512 + (high & 0x0F) * 32 + (low & 0x1F);
    }

    return (glyph & 0x1FFF) * 32 + m_state.kanji_row * 2;
}

INLINE u32 Video::GetBeamHalfLine(u64 clocks) const
{
    u64 elapsed = ((clocks - m_state.frame_start_clocks) * m_state.frame_clock_rate) / GT_CPU_CLOCK_RATE;
    return m_state.crtc[k_video_crtc_vst1] + (u32)((elapsed * 2) / m_state.frame_line_clocks);
}

INLINE u32 Video::GetBeamClock(u64 clocks) const
{
    u64 elapsed = ((clocks - m_state.frame_start_clocks) * m_state.frame_clock_rate) / GT_CPU_CLOCK_RATE;
    u64 origin = ((u64)m_state.crtc[k_video_crtc_vst1] * m_state.frame_line_clocks) / 2;
    return (u32)((origin + elapsed) % m_state.frame_line_clocks);
}

INLINE u32 Video::MakeColor(u8 red, u8 green, u8 blue) const
{
    if (m_pixel_format == GT_PIXEL_RGB565)
        return ((u32)(red >> 3) << 11) | ((u32)(green >> 2) << 5) | (blue >> 3);

#if defined(GT_LITTLE_ENDIAN)
    return (u32)red | ((u32)green << 8) | ((u32)blue << 16) | 0xFF000000U;
#else
    return ((u32)red << 24) | ((u32)green << 16) | ((u32)blue << 8) | 0xFFU;
#endif
}

INLINE u8 Video::Expand5(u32 value) const
{
    return (u8)((value << 3) | (value >> 2));
}

#endif /* VIDEO_INLINE_H */
