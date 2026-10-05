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

#ifndef TOWNS_IO_H
#define TOWNS_IO_H

#include "../common/common.h"

class Audio;
class CdRom;
class FDCMock;
class TownsKeyboard;
class TownsRTC;
class UPD71071;
class YM3438;
class RF5C68;
class Memory;
class TownsPIC;
class TownsPIT;
class TownsSystem;
class Video;

class TownsIO
{
public:
    TownsIO();
    ~TownsIO();
    void Init(Audio* audio, TownsPIC* pic, TownsPIT* pit, Video* video, Memory* memory, TownsSystem* system,
        CdRom* cdrom, FDCMock* fdc, TownsKeyboard* keyboard, TownsRTC* rtc, UPD71071* dma);
    void Reset();
    u8 Read8(u16 port, GT_Bus_Access_Context& context);
    u16 Read16(u16 port, GT_Bus_Access_Context& context);
    u32 Read32(u16 port, GT_Bus_Access_Context& context);
    void Write8(u16 port, u8 value, GT_Bus_Access_Context& context);
    void Write16(u16 port, u16 value, GT_Bus_Access_Context& context);
    void Write32(u16 port, u32 value, GT_Bus_Access_Context& context);

private:
    Audio* m_audio;
    YM3438* m_ym3438;
    RF5C68* m_rf5c68;
    TownsPIC* m_pic;
    TownsPIT* m_pit;
    Video* m_video;
    Memory* m_memory;
    TownsSystem* m_system;
    CdRom* m_cdrom;
    FDCMock* m_fdc;
    TownsKeyboard* m_keyboard;
    TownsRTC* m_rtc;
    UPD71071* m_dma;
};

#include "towns_io_inline.h"

#endif /* TOWNS_IO_H */
