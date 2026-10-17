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

class Memory;
class PIC;
class TraceLogger;
class PIT;
class Scheduler;
class Sprite;
class StateSerializer;

class Video
{
public:
    enum Video_Layer_Format
    {
        VIDEO_LAYER_OFF = 0,
        VIDEO_LAYER_4BPP,
        VIDEO_LAYER_8BPP,
        VIDEO_LAYER_16BPP
    };

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
        u64 frame_start_clocks;
        u32 frame_line_clocks;
        u32 frame_half_lines;
        u32 frame_vsync_half_lines;
        u32 frame_clock_rate;
        u32 frame_count;
        u32 event_half_line;
        u8 fmr_mask;
        bool fmr_page;
        u8 fmr_display_planes;
        bool fmr_display_page;
        bool fmr_ank;
        bool fmr_text_written;
        u8 kanji_high;
        u8 kanji_low;
        u8 kanji_row;
        bool crt_readback;
        bool crt_power_off;
    };

public:
    Video();
    ~Video();
    void Init(PIC* pic, PIT* pit, Scheduler* scheduler, Memory* memory, const u8* font_rom);
    void SetTraceLogger(TraceLogger* trace_logger);
    void Reset();
    void SetFeatures(u32 features);
    bool Decodes(u16 port) const;
    void ResetFMRView();
    u8 Read(u16 port, u64 clocks);
    u8 Peek(u16 port, u64 clocks) const;
    u64 GetStatusStableUntil() const;
    u64 GetSyncStableUntil(u64 clocks) const;
    void Write(u16 port, u8 value, u64 clocks);
    void Synchronize(u64 clocks);
    void HandleEvent(u64 clocks);
    void BeginFrame(u8* frame_buffer, bool render);
    void EndFrame();
    bool IsFrameReady() const;
    bool IsRunning() const;
    int GetFrameWidth() const;
    int GetFrameHeight() const;
    int GetDisplayWidth() const;
    int GetDisplayHeight() const;
    float GetFrameTime() const;
    void EnableNativeResolution(bool enable);
    bool IsNativeResolutionEnabled() const;
    u8* GetVRAM();
    u8* GetSpriteRAM();
    Sprite* GetSprite();
    Video_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream, int version = GT_SAVESTATE_VERSION);

    u8 ReadFMRRegister(u32 offset, bool peek);
    void WriteFMRRegister(u32 offset, u8 value);
    Video_Layer_Format GetLayerFormat(int layer) const;
    bool IsTwoPage() const;
    void UpdateVRAMMapping();
    void UpdateSpriteMapping();
    u8 GetSyncStatus(u64 clocks) const;
    u32 GetBeamHalfLine(u64 clocks) const;
    u32 GetBeamClock(u64 clocks) const;

    static u8 ReadVRAMTwoPageCallback(void* device, u32 offset);
    static u8 PeekVRAMTwoPageCallback(void* device, u32 offset);
    static void WriteVRAMTwoPageCallback(void* device, u32 offset, u8 value);
    static void WriteVRAMTwoPage16Callback(void* device, u32 offset, u16 value);
    static void WriteVRAMTwoPage32Callback(void* device, u32 offset, u32 value);
    static u8 ReadVRAMSinglePageCallback(void* device, u32 offset);
    static u8 PeekVRAMSinglePageCallback(void* device, u32 offset);
    static void WriteVRAMSinglePageCallback(void* device, u32 offset, u8 value);
    static void WriteVRAMSinglePage16Callback(void* device, u32 offset, u16 value);
    static void WriteVRAMSinglePage32Callback(void* device, u32 offset, u32 value);
    static u8 ReadSpriteRAMCallback(void* device, u32 offset);
    static void WriteSpriteRAMCallback(void* device, u32 offset, u8 value);
    static void WriteSpriteRAM16Callback(void* device, u32 offset, u16 value);
    static void WriteSpriteRAM32Callback(void* device, u32 offset, u32 value);
    static u8 ReadFMRPlanesCallback(void* device, u32 offset);
    static u8 PeekFMRPlanesCallback(void* device, u32 offset);
    static void WriteFMRPlanesCallback(void* device, u32 offset, u8 value);
    static u8 ReadFMRTextCallback(void* device, u32 offset);
    static u8 PeekFMRTextCallback(void* device, u32 offset);
    static void WriteFMRTextCallback(void* device, u32 offset, u8 value);
    static u8 ReadFMRRegisterCallback(void* device, u32 offset);
    static u8 PeekFMRRegisterCallback(void* device, u32 offset);
    static void WriteFMRRegisterCallback(void* device, u32 offset, u8 value);

private:
    void WriteCRTC(u8 value, bool high, u64 clocks);
    u8 ReadCRTC(bool high, u64 clocks) const;
    void WritePalette(int component, u8 value);
    u8 ReadPalette(int component) const;
    void RunNextEvent();
    void StartFrame(u64 clocks);
    void CompleteFrame(u64 clocks);
    void UpdateGeometry();
    void UpdateCanvasZoom();
    void UpdateCanvasOffset();
    void ResizeCanvas(u32 zoom_x, u32 zoom_y);
    void SetCanvasZoom(u32 zoom_x, u32 zoom_y);
    bool GetLayerZoom(u32* zoom_x, u32* zoom_y) const;
    u32 GetLayerOffset() const;
    u32 GetLayerZoomKey() const;
    static u32 CommonZoom(u32 a, u32 b);
    static u32 Gcd(u32 a, u32 b);
    void UpdateNextEvent();
    void UpdateIRQ();
    bool HasVSync() const;
    u64 GetHalfLineClocks(u32 half_line) const;
    u64 GetElapsedCRTCClocks(u64 clocks) const;
    void RenderRows(u32 half_line);
    void RenderRow(int row);
    void RenderLayerRow(u32* destination, int layer, int row, bool opaque);
    void DecodeLayerRun(u32* colors, int layer, u32 start, u32 pixel, u32 count, u32 wrap, u8 planes, bool opaque);
    void DecodeLayerPixels(u32* colors, int layer, u32 start, u32 pixel, u32 count, u8 planes, bool opaque);
    void DecodeFMRPagePixels(u32* colors, u32 start, u32 pixel, u32 count, u8 planes, bool opaque);
    template<Video_Layer_Format format, bool two_page>
    void DecodeLayerPixelsTemplate(u32* colors, int layer, u32 start, u32 pixel, u32 count, u8 planes, bool opaque);
    template<bool two_page>
    u8 ReadLayerVRAM(const u8* page, u32 offset) const;
    u32 BlendLayerColor(u32 color, u32 back) const;
    u32 SinglePageToCanonical(u32 offset) const;
    u32 FMRToCanonical(u32 offset) const;
    u32 GetVRAMMask(u32 offset) const;
    u32 GetSpriteHandlerOffset() const;
    void SynchronizeSpriteWork(u32 canonical, u32 size);
    void SynchronizeSpriteRAM();
    void SynchronizeSpriteAccess();
    u8 ReadVRAMTwoPage(u32 offset) const;
    void WriteVRAMTwoPage(u32 offset, u8 value);
    void WriteVRAMTwoPage16(u32 offset, u16 value);
    void WriteVRAMTwoPage32(u32 offset, u32 value);
    u8 ReadVRAMSinglePage(u32 offset) const;
    void WriteVRAMSinglePage(u32 offset, u8 value);
    void WriteVRAMSinglePage16(u32 offset, u16 value);
    void WriteVRAMSinglePage32(u32 offset, u32 value);
    u8 ReadFMRPlanes(u32 offset) const;
    void WriteFMRPlanes(u32 offset, u8 value);
    u8 ReadFMRText(u32 offset) const;
    void WriteFMRText(u32 offset, u8 value);
    u32 GetKanjiOffset() const;
    u32 ToXRGB(u8 red, u8 green, u8 blue) const;
    u8 Expand5(u32 value) const;
    void UpdatePaletteColor(int bank, int index);
    void UpdateColorCaches();
    void TraceWrite(u16 port, u8 value, u64 clocks);
    void TraceEvent(u8 event, u8 reg, u16 value, u8 raw, u8 bank, u32 param, u64 clocks);
    void Serialize(StateSerializer& serializer, int version);
    void SanitizeState();

private:
    Video_State m_state;
    Sprite* m_sprite;
    PIC* m_pic;
    TraceLogger* m_trace_logger;
    PIT* m_pit;
    Scheduler* m_scheduler;
    Memory* m_memory;
    const u8* m_font_rom;
    u32 m_features;
    u8* m_frame_buffer;
    bool m_render;
    bool m_frame_ready;
    u64 m_next_event_clocks;
    u32 m_next_event_half_line;
    u32 m_sprite_handler_offset;
    u64 m_beam_event_clocks;
    u64 m_beam_middle_clocks;
    u64 m_beam_event_crtc_clocks;
    u32 m_beam_event_dot;
    u16 m_beam_vst1;
    int m_frame_width;
    int m_frame_height;
    int m_display_width;
    int m_display_height;
    int m_render_width;
    int m_render_height;
    int m_rendered_rows;
    bool m_line_changed;
    bool m_native_resolution;
    u32 m_canvas_h_start;
    u32 m_canvas_v_start;
    u32 m_canvas_clocks;
    int m_canvas_width;
    int m_canvas_height;
    u32 m_canvas_zoom_x;
    u32 m_canvas_zoom_y;
    u32 m_canvas_zoom_key;
    u32 m_canvas_offset;
    u32 m_canvas_offset_key;
    u32 m_canvas_h_divider;
    u32 m_canvas_v_divider;
    bool m_canvas_interlaced;
    bool m_canvas_split;
    u32 m_palette16_colors[2][16];
    u32 m_palette256_colors[256];
    u32 m_direct_colors[0x8000];
};

static const u32 k_video_clock_rates[4] = { 28636364, 24545455, 25175000, 21052500 };
static const u32 k_video_min_line_clocks = 64;
static const u32 k_video_min_half_lines = 16;
static const u32 k_video_transparent = 0x00010000;
static const u32 k_video_no_wrap = 0x7FFFFFFF;

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
