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

#ifndef IO_H
#define IO_H

#include "../common/common.h"

class Audio;
class CdRom;
class FDC;
class Input;
class Keyboard;
class MSM58321;
class UPD71071;
class YM3438;
class RF5C68;
class Memory;
class PIC;
class PIT;
class SystemControl;
class Video;

class IO
{
public:
    IO();
    ~IO();
    void Init(Audio* audio, PIC* pic, PIT* pit, Video* video, Memory* memory, SystemControl* system_control,
        CdRom* cdrom, FDC* fdc, Keyboard* keyboard, Input* input, MSM58321* rtc, UPD71071* dma);
    void Reset();
    u8 Read8(u16 port, GT_Bus_Access_Context& context);
    u16 Read16(u16 port, GT_Bus_Access_Context& context);
    u32 Read32(u16 port, GT_Bus_Access_Context& context);
    void Write8(u16 port, u8 value, GT_Bus_Access_Context& context);
    void Write16(u16 port, u16 value, GT_Bus_Access_Context& context);
    void Write32(u16 port, u32 value, GT_Bus_Access_Context& context);
    bool Peek(u16 port, u64 clocks, u8& value) const;

private:
    Audio* m_audio;
    YM3438* m_ym3438;
    RF5C68* m_rf5c68;
    PIC* m_pic;
    PIT* m_pit;
    Video* m_video;
    Memory* m_memory;
    SystemControl* m_system_control;
    CdRom* m_cdrom;
    FDC* m_fdc;
    Keyboard* m_keyboard;
    Input* m_input;
    MSM58321* m_rtc;
    UPD71071* m_dma;
};

#include "io_inline.h"

#endif /* IO_H */
