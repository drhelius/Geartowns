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

#ifndef CONFIG_DATA_H
#define CONFIG_DATA_H

#include <SDL3/SDL.h>
#include <string>
#include "geartowns.h"

static const int config_version = 2;
static const int config_minimum_version = 1;
static const int config_max_recent_roms = 15;
static const int config_max_recent_floppies = 5;
static const int config_floppy_drives = 2;

enum config_ShaderMode
{
    config_ShaderMode_PixelPerfect = 0,
    config_ShaderMode_External = 1
};

enum config_Theme
{
    config_Theme_Light = 0,
    config_Theme_Dark = 1,
    config_Theme_Count = 2
};

enum config_VideoSync
{
    config_VideoSync_Disabled = 0,
    config_VideoSync_Fixed = 1,
    config_VideoSync_VRR = 2
};

enum config_KeyboardMode
{
    config_KeyboardMode_Gamepad = 0,
    config_KeyboardMode_Towns = 1
};

struct config_Emulator
{
    bool maximized;
    bool fullscreen;
    int fullscreen_mode;
    bool always_show_menu;
    int theme;
    bool paused;
    int save_slot;
    bool start_paused;
    bool power_on_startup;
    bool pause_when_inactive;
    bool preload_cdrom;
    int cdrom_speed;
    bool ffwd;
    int ffwd_speed;
    int runahead;
    bool show_info;
    std::string recent_roms[config_max_recent_roms];
    int savestates_dir_option;
    std::string savestates_path;
    int savefiles_dir_option;
    std::string savefiles_path;
    int screenshots_dir_option;
    int video_recordings_dir_option;
    std::string bios_path;
    std::string screenshots_path;
    std::string video_recordings_path;
    std::string last_open_path;
    int window_width;
    int window_height;
    bool show_notifications;
    bool allow_screensaver;
    int mcp_tcp_port;
    std::string mcp_http_address;
    bool capture_mouse;
    bool capture_keyboard;
    int mouse_sensitivity;
    bool floppy_persistence;
    bool floppy_write_protected[config_floppy_drives];
    std::string recent_floppies[config_max_recent_floppies];
};

struct config_Machine
{
    int model;
    int custom_cpu;
    int ram_mb[GT_MACHINE_COUNT];
    int floppy_drives[GT_MACHINE_COUNT];
    int cpu_mhz[GT_MACHINE_COUNT];
};

struct config_Video
{
    int scale;
    int scale_manual;
    int ratio;
    bool fps;
    int sync_mode;
    float background_color[config_Theme_Count][3];
    float background_color_debugger[config_Theme_Count][3];
    int shader_mode;
    std::string shader_preset_path;
    int recording_scale;
    int recording_ratio;
    int recording_quality;
};

struct config_Audio
{
    bool enable;
    bool sync;
    float master_volume;
    float fm_volume;
    float pcm_volume;
    float cdda_volume;
    int buffer_count;
};

struct config_Rewind
{
    bool enabled;
    int buffer_seconds;
    int frames_per_snapshot;
    float speed;
};

struct config_Input
{
    bool allow_up_down;
    int controller_type[GT_MAX_GAMEPADS];
    int keyboard_mode[GT_MAX_GAMEPADS];
};

struct config_Input_Keyboard
{
    SDL_Scancode key_left;
    SDL_Scancode key_right;
    SDL_Scancode key_up;
    SDL_Scancode key_down;
    SDL_Scancode key_select;
    SDL_Scancode key_run;
    SDL_Scancode key_A;
    SDL_Scancode key_B;
    SDL_Scancode key_C;
    SDL_Scancode key_X;
    SDL_Scancode key_Y;
    SDL_Scancode key_Z;
    SDL_Scancode key_zoom;
};

struct config_Input_Gamepad
{
    int gamepad_directional;
    bool gamepad_invert_x_axis;
    bool gamepad_invert_y_axis;
    int gamepad_select;
    int gamepad_run;
    int gamepad_A;
    int gamepad_B;
    int gamepad_C;
    int gamepad_X;
    int gamepad_Y;
    int gamepad_Z;
    int gamepad_zoom;
    int gamepad_x_axis;
    int gamepad_y_axis;
};

enum config_HotkeyIndex
{
    config_HotkeyIndex_OpenROM = 0,
    config_HotkeyIndex_ReloadROM,
    config_HotkeyIndex_Quit,
    config_HotkeyIndex_Reset,
    config_HotkeyIndex_Pause,
    config_HotkeyIndex_FFWD,
    config_HotkeyIndex_Rewind,
    config_HotkeyIndex_SaveState,
    config_HotkeyIndex_LoadState,
    config_HotkeyIndex_Screenshot,
    config_HotkeyIndex_Fullscreen,
    config_HotkeyIndex_ShowMainMenu,
    config_HotkeyIndex_DebugStepInto,
    config_HotkeyIndex_DebugStepOver,
    config_HotkeyIndex_DebugStepOut,
    config_HotkeyIndex_DebugStepFrame,
    config_HotkeyIndex_DebugContinue,
    config_HotkeyIndex_DebugBreak,
    config_HotkeyIndex_DebugRunToCursor,
    config_HotkeyIndex_DebugBreakpoint,
    config_HotkeyIndex_DebugGoBack,
    config_HotkeyIndex_SelectSlot1,
    config_HotkeyIndex_SelectSlot2,
    config_HotkeyIndex_SelectSlot3,
    config_HotkeyIndex_SelectSlot4,
    config_HotkeyIndex_SelectSlot5,
    config_HotkeyIndex_CaptureMouse,
    config_HotkeyIndex_Mute,
    config_HotkeyIndex_VideoRecording,
    config_HotkeyIndex_CaptureKeyboard,
    config_HotkeyIndex_COUNT
};

struct config_Input_Gamepad_Shortcuts
{
    int gamepad_shortcuts[config_HotkeyIndex_COUNT];
};

struct config_Hotkey
{
    SDL_Scancode key;
    SDL_Keymod mod;
    char str[64];
};

struct config_Debug
{
    bool debug;
    bool show_screen;
    bool show_memory;
    bool show_disassembler;
    bool show_processor;
    bool show_processor_details;
    bool show_call_stack;
    bool show_i386_descriptors;
    bool show_i386_paging;
    bool show_trace_logger;
    bool show_profiler;
    bool trace_counter;
    bool trace_cycles;
    bool trace_linear;
    bool trace_registers;
    bool trace_segments;
    bool trace_flags;
    bool trace_bytes;
    bool trace_cpu_enabled;
    bool trace_cpu;
    bool trace_io;
    bool trace_pic;
    bool trace_timer;
    bool trace_dma;
    bool trace_video;
    bool trace_sprite;
    bool trace_fm;
    bool trace_pcm;
    bool trace_mixer;
    bool trace_cdrom;
    bool trace_fdc;
    bool trace_keyboard;
    bool trace_input;
    bool trace_system;
    int trace_cpu_interrupt_events;
    int trace_io_events;
    int trace_pic_events;
    int trace_timer_events;
    int trace_dma_events;
    int trace_video_events;
    int trace_sprite_events;
    int trace_fm_events;
    int trace_pcm_events;
    int trace_mixer_events;
    int trace_cdrom_events;
    int trace_fdc_events;
    int trace_keyboard_events;
    int trace_input_events;
    int trace_system_events;
    int trace_vblank_watch_address;
    int trace_vblank_watch_operation;
    int trace_output;
    int trace_capacity;
    int trace_disk_dir_option;
    int trace_disk_size;
    std::string trace_disk_path;
    bool show_breakpoints;
    bool show_symbols;
    bool show_rewind;
    bool show_pic;
    bool show_pit;
    bool show_dma;
    bool show_rtc;
    bool show_system_control;
    bool show_keyboard;
    bool show_game_ports;
    bool show_crtc;
    bool show_crtc_registers;
    bool show_video_output;
    bool show_palettes;
    bool show_framebuffers;
    bool show_sprites;
    int framebuffer_tab;
    int framebuffer_zoom;
    bool framebuffer_show_window;
    int framebuffer_sprite_page;
    int framebuffer_custom_offset;
    int framebuffer_custom_format;
    int framebuffer_custom_width;
    int framebuffer_custom_height;
    int framebuffer_custom_palette;
    int sprite_filter;
    bool show_ym3438;
    bool show_ym3438_registers;
    bool show_rf5c68;
    bool show_sound_control;
    int rf5c68_wave_bank;
    bool show_cdrom;
    bool show_cdrom_toc;
    bool show_cdrom_audio;
    bool show_fdc;
    bool show_floppy_drives;
    bool show_disk_viewer;
    int disk_viewer_drive;
    int disk_viewer_cylinder;
    int disk_viewer_head;
    bool auto_debug_settings;
    bool dis_show_bytes;
    bool dis_show_symbols;
    bool dis_show_segment;
    bool dis_show_auto_symbols;
    bool dis_dim_auto_symbols;
    bool dis_replace_symbols;
    bool dis_replace_labels;
    int dis_look_ahead_count;
    bool step_skip_interrupts;
    int font_size;
    int scale;
    int ratio;
    bool multi_viewport;
    bool single_instance;
};

EXTERN config_Emulator config_emulator;
EXTERN config_Machine config_machine;
EXTERN config_Video config_video;
EXTERN config_Audio config_audio;
EXTERN config_Rewind config_rewind;
EXTERN config_Input config_input;
EXTERN config_Input_Keyboard config_input_keyboard[GT_MAX_GAMEPADS];
EXTERN config_Input_Gamepad config_input_gamepad[GT_MAX_GAMEPADS];
EXTERN config_Input_Gamepad_Shortcuts config_input_gamepad_shortcuts[GT_MAX_GAMEPADS];
EXTERN config_Hotkey config_hotkeys[config_HotkeyIndex_COUNT];
EXTERN config_Debug config_debug;

#endif /* CONFIG_DATA_H */
