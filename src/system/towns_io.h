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

#include "common.h"

typedef u8 (*GT_IO_Read8_Fn)(void* device, u16 port, GT_Bus_Access_Context& context);
typedef void (*GT_IO_Write8_Fn)(void* device, u16 port, u8 value, GT_Bus_Access_Context& context);
typedef u16 (*GT_IO_Read16_Fn)(void* device, u16 port, GT_Bus_Access_Context& context);
typedef void (*GT_IO_Write16_Fn)(void* device, u16 port, u16 value, GT_Bus_Access_Context& context);
typedef u32 (*GT_IO_Read32_Fn)(void* device, u16 port, GT_Bus_Access_Context& context);
typedef void (*GT_IO_Write32_Fn)(void* device, u16 port, u32 value, GT_Bus_Access_Context& context);

struct GT_IO_Handler
{
    void* device;
    GT_IO_Read8_Fn read8;
    GT_IO_Write8_Fn write8;
    GT_IO_Read16_Fn read16;
    GT_IO_Write16_Fn write16;
    GT_IO_Read32_Fn read32;
    GT_IO_Write32_Fn write32;
};

class TownsIO
{
public:
    TownsIO();
    ~TownsIO();
    void Init();
    void Reset();
    bool RegisterPort(u16 port, const GT_IO_Handler& handler);
    bool RegisterRange(u16 start_port, u16 end_port, const GT_IO_Handler& handler);
    u8 Read8(u16 port, GT_Bus_Access_Context& context);
    u16 Read16(u16 port, GT_Bus_Access_Context& context);
    u32 Read32(u16 port, GT_Bus_Access_Context& context);
    void Write8(u16 port, u8 value, GT_Bus_Access_Context& context);
    void Write16(u16 port, u16 value, GT_Bus_Access_Context& context);
    void Write32(u16 port, u32 value, GT_Bus_Access_Context& context);

private:
    bool FindHandler(const GT_IO_Handler& handler, u16& handler_index) const;
    bool AddHandler(const GT_IO_Handler& handler, u16& handler_index);

private:
    u16 m_port_handlers[0x10000];
    GT_IO_Handler m_handlers[GT_IO_MAX_HANDLERS];
    u16 m_handler_count;
};

#endif /* TOWNS_IO_H */
