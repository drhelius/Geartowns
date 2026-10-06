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

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include "defines.h"

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;

union u16_union
{
    u16 value;
    struct
    {
#ifdef GT_LITTLE_ENDIAN
        u8 low;
        u8 high;
#else
        u8 high;
        u8 low;
#endif
    };
};

union u32_union
{
    u32 value;
    struct
    {
#ifdef GT_LITTLE_ENDIAN
        u16 low;
        u16 high;
#else
        u16 high;
        u16 low;
#endif
    };
    struct
    {
#ifdef GT_LITTLE_ENDIAN
        u8 byte0;
        u8 byte1;
        u8 byte2;
        u8 byte3;
#else
        u8 byte3;
        u8 byte2;
        u8 byte1;
        u8 byte0;
#endif
    };
};

struct GT_Runtime_Info
{
    int screen_width;
    int screen_height;
    int width_scale;
    float frame_time;
    int sample_rate;
    bool media_ready;
    bool bios_ready;
    bool paused;
};

enum GT_Run_Result
{
    GT_RUN_FRAME_READY = 0,
    GT_RUN_PAUSED,
    GT_RUN_NOT_READY
};

enum GT_Bus_Access_Origin
{
    GT_BUS_ORIGIN_CPU = 0,
    GT_BUS_ORIGIN_DMA
};

struct GT_Bus_Access_Context;

typedef void (*GT_Synchronize_Hardware_Fn)(void* core, GT_Bus_Access_Context& context, u32 elapsed_clocks);
typedef void (*GT_Observe_Memory_Write_Fn)(void* observer, u32 bus_address, u8 previous, u8 value);

struct GT_Bus_Access_Context
{
    u64 clocks;
    u32 elapsed_clocks;
    u32 synchronized_clocks;
    u32 wait_clocks;
    GT_Bus_Access_Origin origin;
    GT_Synchronize_Hardware_Fn synchronize;
    void* synchronize_context;
    GT_Observe_Memory_Write_Fn observe_memory_write;
    void* memory_write_context;
    bool end_batch;
};

enum GT_Firmware_Type
{
    GT_FIRMWARE_SYSTEM = 0,
    GT_FIRMWARE_OS,
    GT_FIRMWARE_FONT,
    GT_FIRMWARE_DICTIONARY,
    GT_FIRMWARE_FONT20,
    GT_FIRMWARE_COUNT
};

struct GT_Firmware_Info
{
    char path[GT_MAX_PATH];
    char database_name[128];
    int size;
    u32 crc;
    bool loaded;
    bool recognized;
    bool synthetic;
};

struct GT_Color
{
    u8 red;
    u8 green;
    u8 blue;
};

enum GT_Pixel_Format
{
    GT_PIXEL_RGB565,
    GT_PIXEL_RGBA8888,
};

enum GT_Keys
{
    GT_KEY_NONE             = 0x00,
    GT_KEY_ESCAPE           = 0x01,
    GT_KEY_1                = 0x02,
    GT_KEY_2                = 0x03,
    GT_KEY_3                = 0x04,
    GT_KEY_4                = 0x05,
    GT_KEY_5                = 0x06,
    GT_KEY_6                = 0x07,
    GT_KEY_7                = 0x08,
    GT_KEY_8                = 0x09,
    GT_KEY_9                = 0x0A,
    GT_KEY_0                = 0x0B,
    GT_KEY_MINUS            = 0x0C,
    GT_KEY_CARET            = 0x0D,
    GT_KEY_YEN              = 0x0E,
    GT_KEY_BACKSPACE        = 0x0F,
    GT_KEY_TAB              = 0x10,
    GT_KEY_Q                = 0x11,
    GT_KEY_W                = 0x12,
    GT_KEY_E                = 0x13,
    GT_KEY_R                = 0x14,
    GT_KEY_T                = 0x15,
    GT_KEY_Y                = 0x16,
    GT_KEY_U                = 0x17,
    GT_KEY_I                = 0x18,
    GT_KEY_O                = 0x19,
    GT_KEY_P                = 0x1A,
    GT_KEY_AT               = 0x1B,
    GT_KEY_LEFT_BRACKET     = 0x1C,
    GT_KEY_RETURN           = 0x1D,
    GT_KEY_A                = 0x1E,
    GT_KEY_S                = 0x1F,
    GT_KEY_D                = 0x20,
    GT_KEY_F                = 0x21,
    GT_KEY_G                = 0x22,
    GT_KEY_H                = 0x23,
    GT_KEY_J                = 0x24,
    GT_KEY_K                = 0x25,
    GT_KEY_L                = 0x26,
    GT_KEY_SEMICOLON        = 0x27,
    GT_KEY_COLON            = 0x28,
    GT_KEY_RIGHT_BRACKET    = 0x29,
    GT_KEY_Z                = 0x2A,
    GT_KEY_X                = 0x2B,
    GT_KEY_C                = 0x2C,
    GT_KEY_V                = 0x2D,
    GT_KEY_B                = 0x2E,
    GT_KEY_N                = 0x2F,
    GT_KEY_M                = 0x30,
    GT_KEY_COMMA            = 0x31,
    GT_KEY_PERIOD           = 0x32,
    GT_KEY_SLASH            = 0x33,
    GT_KEY_UNDERSCORE       = 0x34,
    GT_KEY_SPACE            = 0x35,
    GT_KEY_KP_MULTIPLY      = 0x36,
    GT_KEY_KP_DIVIDE        = 0x37,
    GT_KEY_KP_PLUS          = 0x38,
    GT_KEY_KP_MINUS         = 0x39,
    GT_KEY_KP_7             = 0x3A,
    GT_KEY_KP_8             = 0x3B,
    GT_KEY_KP_9             = 0x3C,
    GT_KEY_KP_EQUALS        = 0x3D,
    GT_KEY_KP_4             = 0x3E,
    GT_KEY_KP_5             = 0x3F,
    GT_KEY_KP_6             = 0x40,
    GT_KEY_KP_1             = 0x42,
    GT_KEY_KP_2             = 0x43,
    GT_KEY_KP_3             = 0x44,
    GT_KEY_KP_ENTER         = 0x45,
    GT_KEY_KP_0             = 0x46,
    GT_KEY_KP_PERIOD        = 0x47,
    GT_KEY_INSERT           = 0x48,
    GT_KEY_KP_000           = 0x4A,
    GT_KEY_DELETE           = 0x4B,
    GT_KEY_UP               = 0x4D,
    GT_KEY_HOME             = 0x4E,
    GT_KEY_LEFT             = 0x4F,
    GT_KEY_DOWN             = 0x50,
    GT_KEY_RIGHT            = 0x51,
    GT_KEY_CTRL             = 0x52,
    GT_KEY_SHIFT            = 0x53,
    GT_KEY_CAPS             = 0x55,
    GT_KEY_HIRAGANA         = 0x56,
    GT_KEY_NO_CONVERT       = 0x57,
    GT_KEY_CONVERT          = 0x58,
    GT_KEY_KANA_KANJI       = 0x59,
    GT_KEY_KATAKANA         = 0x5A,
    GT_KEY_PF12             = 0x5B,
    GT_KEY_ALT              = 0x5C,
    GT_KEY_PF1              = 0x5D,
    GT_KEY_PF2              = 0x5E,
    GT_KEY_PF3              = 0x5F,
    GT_KEY_PF4              = 0x60,
    GT_KEY_PF5              = 0x61,
    GT_KEY_PF6              = 0x62,
    GT_KEY_PF7              = 0x63,
    GT_KEY_PF8              = 0x64,
    GT_KEY_PF9              = 0x65,
    GT_KEY_PF10             = 0x66,
    GT_KEY_PF11             = 0x69,
    GT_KEY_KANJI_DICTIONARY = 0x6B,
    GT_KEY_DELETE_WORD      = 0x6C,
    GT_KEY_ADD_WORD         = 0x6D,
    GT_KEY_PREVIOUS         = 0x6E,
    GT_KEY_NEXT             = 0x70,
    GT_KEY_HALF_FULL        = 0x71,
    GT_KEY_CANCEL           = 0x72,
    GT_KEY_EXECUTE          = 0x73,
    GT_KEY_PF13             = 0x74,
    GT_KEY_PF14             = 0x75,
    GT_KEY_PF15             = 0x76,
    GT_KEY_PF16             = 0x77,
    GT_KEY_PF17             = 0x78,
    GT_KEY_PF18             = 0x79,
    GT_KEY_PF19             = 0x7A,
    GT_KEY_PF20             = 0x7B,
    GT_KEY_BREAK            = 0x7C,
    GT_KEY_COPY             = 0x7D,
    GT_KEY_COUNT            = 0x80
};

enum GT_Controller_Type
{
    GT_CONTROLLER_NONE = 0,
    GT_CONTROLLER_ORIGINAL_GAMEPAD,
    GT_CONTROLLER_MARTY_GAMEPAD,
    GT_CONTROLLER_6_BUTTON_GAMEPAD,
    GT_CONTROLLER_MOUSE
};

enum GT_Controllers
{
    GT_CONTROLLER_1 = 0,
    GT_CONTROLLER_2
};

enum GT_GamePad_Buttons
{
    GT_GAMEPAD_UP       = 0x0001,
    GT_GAMEPAD_DOWN     = 0x0002,
    GT_GAMEPAD_LEFT     = 0x0004,
    GT_GAMEPAD_RIGHT    = 0x0008,
    GT_GAMEPAD_SELECT   = 0x0010,
    GT_GAMEPAD_RUN      = 0x0020,
    GT_GAMEPAD_A        = 0x0040,
    GT_GAMEPAD_B        = 0x0080,
    GT_GAMEPAD_C        = 0x0100,
    GT_GAMEPAD_X        = 0x0200,
    GT_GAMEPAD_Y        = 0x0400,
    GT_GAMEPAD_Z        = 0x0800,
    GT_GAMEPAD_ZOOM     = 0x1000
};

struct GT_GamePad_State
{
    u16 buttons;
};

enum GT_Machine_Model
{
    GT_MACHINE_MODEL1_2 = 0,
    GT_MACHINE_1F_2F,
    GT_MACHINE_10F_20F,
    GT_MACHINE_CX,
    GT_MACHINE_UX,
    GT_MACHINE_UG,
    GT_MACHINE_HG,
    GT_MACHINE_HR,
    GT_MACHINE_UR,
    GT_MACHINE_ME,
    GT_MACHINE_MA,
    GT_MACHINE_MX,
    GT_MACHINE_MF_FRESH,
    GT_MACHINE_HC,
    GT_MACHINE_MARTY,
    GT_MACHINE_CUSTOM,
    GT_MACHINE_COUNT
};

enum GT_Machine_CPU
{
    GT_MACHINE_CPU_80386DX = 0,
    GT_MACHINE_CPU_80386SX,
    GT_MACHINE_CPU_80486SX,
    GT_MACHINE_CPU_80486DX2,
    GT_MACHINE_CPU_PENTIUM,
    GT_MACHINE_CPU_COUNT
};

struct GT_Machine_CPU_Info
{
    const char* name;
    bool emulated;
};

struct GT_Machine_Profile
{
    const char* name;
    const char* models;
    bool emulated;
    GT_Machine_CPU cpu;
    u32 cpu_clock_rate;
    int ram_min_mb;
    int ram_default_mb;
    int ram_max_mb;
    int floppy_min;
    int floppy_default;
    int floppy_max;
    bool three_mode_floppy;
};

struct GT_Machine_Config
{
    GT_Machine_Model model;
    GT_Machine_CPU cpu;
    u32 ram_size;
    int floppy_drives;
    u32 cpu_clock_rate;
};

struct GT_SaveState_Header
{
    u32 magic;
    u32 version;
    u32 size;
    s64 timestamp;
    char rom_name[128];
    u32 rom_crc;
    u32 screenshot_size;
    u16 screenshot_width;
    u16 screenshot_height;
    char emu_build[32];
};

struct GT_SaveState_Header_Libretro
{
    u32 magic;
    u32 version;
};

struct GT_SaveState_Screenshot
{
    u32 width;
    u32 height;
    u32 size;
    u8* data;
};

#endif /* TYPES_H */
