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

#ifndef VIDEO_H
#define VIDEO_H

#include <iostream>
#include "../common/common.h"

#define VIDEO_VRAM_SIZE 0x80000
#define VIDEO_SPRITE_RAM_SIZE 0x20000
#define VIDEO_CRTC_REGISTER_COUNT 32

class TownsPIC;
class StateSerializer;

class Video
{
public:
    struct Video_State
    {
        u8 vram[VIDEO_VRAM_SIZE];
        u8 sprite_ram[VIDEO_SPRITE_RAM_SIZE];
        u16 crtc[VIDEO_CRTC_REGISTER_COUNT];
        u8 crtc_index;
        u8 output[4];
        u8 output_index;
        u8 mask[4];
        u8 mask_index;
        u8 palette_index;
        u8 palette16[2][16][3];
        u8 palette256[256][3];
        u8 digital_palette[8];
        bool digital_palette_modified;
        u8 display_enable;
        bool vsync_irq;
        bool running;
        u64 frame_start_time;
        u32 frame_line_clocks;
        u32 frame_half_lines;
        u32 frame_clock_rate;
        u32 frame_count;
    };

public:
    Video();
    ~Video();
    void Init(TownsPIC* pic, GT_Pixel_Format pixel_format);
    void Reset();
    u8 Read(u16 port, u64 time_ns);
    void Write(u16 port, u8 value, u64 time_ns);
    void Synchronize(u64 time_ns);
    u64 GetNextEventTime() const;
    void BeginFrame(u8* frame_buffer, bool render);
    void EndFrame();
    bool IsFrameReady() const;
    bool IsRunning() const;
    int GetFrameWidth() const;
    int GetFrameHeight() const;
    u8* GetVRAM();
    u8* GetSpriteRAM();
    Video_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

    static u8 ReadVRAMTwoPage(void* device, u32 offset);
    static void WriteVRAMTwoPage(void* device, u32 offset, u8 value);
    static u8 ReadVRAMSinglePage(void* device, u32 offset);
    static void WriteVRAMSinglePage(void* device, u32 offset, u8 value);

private:
    enum Video_Layer_Format
    {
        VIDEO_LAYER_OFF = 0,
        VIDEO_LAYER_4BPP,
        VIDEO_LAYER_8BPP,
        VIDEO_LAYER_16BPP
    };

private:
    void WriteCRTC(u8 value, bool high, u64 time_ns);
    u8 ReadCRTC(bool high, u64 time_ns);
    void WritePalette(int component, u8 value);
    u8 ReadPalette(int component) const;
    void StartFrame(u64 time_ns);
    void CompleteFrame();
    void UpdateGeometry();
    void UpdateNextEvent();
    void UpdateIRQ();
    u8 GetSyncStatus(u64 time_ns) const;
    u32 GetBeamHalfLine(u64 time_ns) const;
    u32 GetBeamClock(u64 time_ns) const;
    void RenderUpTo(u64 time_ns);
    void RenderRow(int row);
    void RenderLayerRow(int layer, int row, bool opaque);
    Video_Layer_Format GetLayerFormat(int layer) const;
    bool IsTwoPage() const;
    u8 ReadVRAM(u32 offset, bool two_page) const;
    u32 SinglePageToCanonical(u32 offset) const;
    u32 MakeColor(u8 red, u8 green, u8 blue) const;
    u8 Expand5(u32 value) const;
    void UpdatePaletteColor(int bank, int index);
    void UpdateColorCaches();
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    Video_State m_state;
    TownsPIC* m_pic;
    GT_Pixel_Format m_pixel_format;
    u8* m_frame_buffer;
    bool m_render;
    bool m_frame_ready;
    u64 m_next_event_time;
    int m_frame_width;
    int m_frame_height;
    int m_render_width;
    int m_render_height;
    int m_rendered_rows;
    u32 m_canvas_h_start;
    u32 m_canvas_v_start;
    u32 m_canvas_h_divider;
    bool m_canvas_interlaced;
    u32 m_line[GT_MAX_FRAME_BUFFER_WIDTH];
    u32 m_palette16_colors[2][16];
    u32 m_palette256_colors[256];
    u32 m_direct_colors[0x8000];
};

static const u32 k_video_clock_rates[4] = { 28636364, 24545455, 25175000, 21052500 };
static const u32 k_video_min_line_clocks = 64;
static const u32 k_video_min_half_lines = 16;

static const int k_video_crtc_hsw1 = 0x00;
static const int k_video_crtc_hst = 0x04;
static const int k_video_crtc_vst1 = 0x05;
static const int k_video_crtc_vst2 = 0x06;
static const int k_video_crtc_vst = 0x08;
static const int k_video_crtc_hds0 = 0x09;
static const int k_video_crtc_hde0 = 0x0A;
static const int k_video_crtc_vds0 = 0x0D;
static const int k_video_crtc_vde0 = 0x0E;
static const int k_video_crtc_fa0 = 0x11;
static const int k_video_crtc_haj0 = 0x12;
static const int k_video_crtc_fo0 = 0x13;
static const int k_video_crtc_lo0 = 0x14;
static const int k_video_crtc_zoom = 0x1B;
static const int k_video_crtc_cr0 = 0x1C;
static const int k_video_crtc_cr1 = 0x1D;
static const int k_video_crtc_fr = 0x1E;

#include "video_inline.h"

#endif /* VIDEO_H */
