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

struct GT_Runtime_Info
{
    int screen_width;
    int screen_height;
    int width_scale;
    int sample_rate;
    bool media_ready;
    bool cdrom_ready;
    bool paused;
    u64 towns_time_ns;
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
    GT_KEY_NONE = 0,
    GT_KEY_ESCAPE,
    GT_KEY_F1,
    GT_KEY_F2,
    GT_KEY_A,
    GT_KEY_B,
    GT_KEY_Z,
    GT_KEY_SPACE,
    GT_KEY_RETURN,
    GT_KEY_KANA,
    GT_KEY_GRAPH,
    GT_KEY_COUNT
};

enum GT_Controller_Type
{
    GT_CONTROLLER_NONE = 0,
    GT_CONTROLLER_ORIGINAL_GAMEPAD,
    GT_CONTROLLER_6_BUTTON_GAMEPAD
};

enum GT_GamePad_Buttons
{
    GT_GAMEPAD_UP       = 0x0001,
    GT_GAMEPAD_DOWN     = 0x0002,
    GT_GAMEPAD_LEFT     = 0x0004,
    GT_GAMEPAD_RIGHT    = 0x0008,
    GT_GAMEPAD_START    = 0x0010,
    GT_GAMEPAD_RUN      = 0x0020,
    GT_GAMEPAD_A        = 0x0040,
    GT_GAMEPAD_B        = 0x0080,
    GT_GAMEPAD_C        = 0x0100,
    GT_GAMEPAD_X        = 0x0200,
    GT_GAMEPAD_Y        = 0x0400,
    GT_GAMEPAD_Z        = 0x0800
};

struct GT_GamePad_State
{
    u16 buttons;
    s16 axis_x;
    s16 axis_y;
};

#endif /* TYPES_H */