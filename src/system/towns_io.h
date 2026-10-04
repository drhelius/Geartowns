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
class YM3438;
class RF5C68;
class TownsPIC;
class TownsPIT;
class Video;

class TownsIO
{
public:
    TownsIO();
    ~TownsIO();
    void Init(Audio* audio, TownsPIC* pic, TownsPIT* pit, Video* video);
    void Reset();
    u8 Read8(u16 port, GT_Bus_Access_Context& context);
    u16 Read16(u16 port, GT_Bus_Access_Context& context);
    u32 Read32(u16 port, GT_Bus_Access_Context& context);
    void Write8(u16 port, u8 value, GT_Bus_Access_Context& context);
    void Write16(u16 port, u16 value, GT_Bus_Access_Context& context);
    void Write32(u16 port, u32 value, GT_Bus_Access_Context& context);

private:
    YM3438* m_ym3438;
    RF5C68* m_rf5c68;
    TownsPIC* m_pic;
    TownsPIT* m_pit;
    Video* m_video;
};

#include "towns_io_inline.h"

#endif /* TOWNS_IO_H */
