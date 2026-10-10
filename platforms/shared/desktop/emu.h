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

#ifndef EMU_H
#define EMU_H

#include "geartowns.h"

#ifdef EMU_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

enum Debug_Command
{
    Debug_Command_Continue,
    Debug_Command_Step,
    Debug_Command_StepOver,
    Debug_Command_StepFrame,
    Debug_Command_None
};

#define EMU_DEBUG_FRAMEBUFFER_WIDTH 1024
#define EMU_DEBUG_FRAMEBUFFER_HEIGHT 512
#define EMU_DEBUG_SPRITE_ATLAS_SIZE 512
#define EMU_DEBUG_SPRITE_PAGE_SIZE 256

enum Emu_Debug_Buffer
{
    Emu_Debug_Buffer_Layer0 = 0,
    Emu_Debug_Buffer_Layer1,
    Emu_Debug_Buffer_SpriteDisplay,
    Emu_Debug_Buffer_SpriteDraw,
    Emu_Debug_Buffer_Custom,
    Emu_Debug_Buffer_Count
};

struct Emu_Debug_Buffer_Request
{
    u32 offset;
    int format;
    int width;
    int height;
    int palette;
};

struct Emu_Debug_Buffer_Info
{
    int format;
    bool single_page;
    u32 page_base;
    u32 page_size;
    u32 start;
    u32 stride;
    int width;
    int height;
    bool window;
    int window_x;
    int window_y;
    int window_width;
    int window_height;
    int window_wrap;
};

struct Emu_Debug_Sprite
{
    int index;
    u32 address;
    u16 x;
    u16 y;
    u16 attributes;
    u16 color;
    int screen_x;
    int screen_y;
    bool offset;
    bool swap;
    bool flip_x;
    bool flip_y;
    bool half_x;
    bool half_y;
    bool table;
    bool through;
    bool hide;
    u16 pattern;
    u32 pattern_address;
    u16 color_table;
    u32 color_table_address;
    bool drawn;
    bool visible;
};

enum Directory_Location
{
    Directory_Location_Default = 0,
    Directory_Location_ROM = 1,
    Directory_Location_Custom = 2
};

EXTERN u8* emu_frame_buffer;
EXTERN u8* emu_debug_framebuffer;
EXTERN u8* emu_debug_sprite_atlas;
EXTERN u8* emu_debug_sprite_page;
EXTERN Emu_Debug_Buffer_Info emu_debug_framebuffer_info;
EXTERN GT_SaveState_Header emu_savestates[5];
EXTERN GT_SaveState_Screenshot emu_savestates_screenshots[5];
EXTERN u32 emu_savestates_generation;
EXTERN Debug_Command emu_debug_command;
EXTERN bool emu_debug_pc_changed;
EXTERN int emu_debug_step_frames_pending;
EXTERN bool emu_debug_disable_breakpoints;
EXTERN u64 emu_frame_counter;
EXTERN bool emu_audio_sync;

EXTERN bool emu_init(void);
EXTERN void emu_destroy(void);
EXTERN void emu_update(void);
EXTERN void emu_load_media_async(const char* file_path);
EXTERN void emu_load_physical_cdrom_async(const char* device_id);
EXTERN bool emu_is_media_loading(void);
EXTERN bool emu_finish_media_loading(void);
EXTERN void emu_set_gamepad_state(GT_Controllers controller, const GT_GamePad_State& state);
EXTERN void emu_key_pressed(GT_Keys key);
EXTERN void emu_key_released(GT_Keys key);
EXTERN void emu_release_all_keys(void);
EXTERN void emu_set_mouse_delta(GT_Controllers controller, int x, int y);
EXTERN void emu_clear_mouse(GT_Controllers controller);
EXTERN void emu_pause(void);
EXTERN void emu_resume(void);
EXTERN bool emu_is_paused(void);
EXTERN bool emu_is_debug_idle(void);
EXTERN bool emu_is_empty(void);
EXTERN bool emu_is_powered_on(void);
EXTERN bool emu_power_on(void);
EXTERN void emu_power_off(void);
EXTERN void emu_reset(void);
EXTERN bool emu_eject_media(void);
EXTERN bool emu_cdrom_playlist_load(const char* path, bool* floppies);
EXTERN void emu_cdrom_playlist_clear(void);
EXTERN bool emu_cdrom_playlist_select(const char* path);
EXTERN int emu_cdrom_playlist_get_count(void);
EXTERN int emu_cdrom_playlist_get_index(void);
EXTERN const char* emu_cdrom_playlist_get_path(int index);
EXTERN const char* emu_cdrom_playlist_get_name(int index);
EXTERN void emu_set_preload_cdrom(bool enabled);
EXTERN void emu_set_cdrom_speed(int speed);
EXTERN void emu_apply_machine_settings(void);

EXTERN void emu_audio_mute(bool mute);
EXTERN void emu_audio_set_master_volume(float volume);
EXTERN void emu_audio_set_source_volume(int source, float volume);
EXTERN void emu_audio_set_source_mute(int source, bool mute);
EXTERN bool emu_audio_is_source_muted(int source);
EXTERN void emu_audio_reset(void);
EXTERN bool emu_is_audio_enabled(void);
EXTERN bool emu_is_audio_open(void);

EXTERN bool emu_save_state_slot(int index);
EXTERN bool emu_load_state_slot(int index);
EXTERN bool emu_save_state_file(const char* file_path);
EXTERN bool emu_load_state_file(const char* file_path);
EXTERN void update_savestates_data(void);

EXTERN void emu_get_runtime(GT_Runtime_Info& runtime);
EXTERN double emu_get_frame_rate(void);
EXTERN void emu_get_info(char* info, int buffer_size);
EXTERN GeartownsCore* emu_get_core(void);

EXTERN void emu_debug_step_over(void);
EXTERN void emu_debug_step_into(void);
EXTERN void emu_debug_step_out(void);
EXTERN void emu_debug_step_frame(void);
EXTERN void emu_debug_step_frames(int frames);
EXTERN void emu_debug_break(void);
EXTERN void emu_debug_continue(void);
EXTERN void emu_debug_state_restored(void);

EXTERN void emu_debug_update(void);
EXTERN void emu_debug_get_buffer_info(int buffer, const Emu_Debug_Buffer_Request* request, Emu_Debug_Buffer_Info& info);
EXTERN void emu_debug_decode_buffer(int buffer, const Emu_Debug_Buffer_Request* request, u8* output,
    Emu_Debug_Buffer_Info& info);
EXTERN void emu_debug_get_sprite(int index, Emu_Debug_Sprite& sprite);
EXTERN void emu_debug_decode_sprite(int index, u8* output, int stride);
EXTERN int emu_get_debug_buffer_png(int buffer, const Emu_Debug_Buffer_Request* request, unsigned char** out_buffer);
EXTERN int emu_get_sprite_png(int index, int scale, unsigned char** out_buffer);

EXTERN void emu_set_pad_type(GT_Controllers controller, GT_Controller_Type type);
EXTERN GT_Controller_Type emu_get_pad_type(GT_Controllers controller);
EXTERN bool emu_save_screenshot(const char* file_path);
EXTERN int emu_get_screenshot_png(unsigned char** out_buffer);
EXTERN bool emu_start_video_recording(const char* file_path);
EXTERN void emu_stop_video_recording(void);
EXTERN bool emu_is_video_recording(void);
EXTERN bool emu_load_bios(const char* path);

EXTERN void emu_mcp_set_transport(int mode, int tcp_port, const char* tcp_address);
EXTERN void emu_mcp_start(void);
EXTERN void emu_mcp_stop(void);
EXTERN bool emu_mcp_is_running(void);
EXTERN int emu_mcp_get_transport_mode(void);
EXTERN const char* emu_mcp_get_http_address(void);
EXTERN int emu_mcp_get_http_port(void);
EXTERN void emu_mcp_pump_commands(void);
EXTERN void emu_reset_rewind_timing(void);

#undef EMU_IMPORT
#undef EXTERN
#endif /* EMU_H */
