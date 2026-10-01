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

#include "towns_io.h"

TownsIO::TownsIO()
{
    Reset();
}

TownsIO::~TownsIO()
{
}

void TownsIO::Init()
{
    Reset();
}

void TownsIO::Reset()
{
    memset(m_port_handlers, 0, sizeof(m_port_handlers));
    memset(m_handlers, 0, sizeof(m_handlers));
    m_handler_count = 1;
}

bool TownsIO::RegisterPort(u16 port, const GT_IO_Handler& handler)
{
    return RegisterRange(port, port, handler);
}

bool TownsIO::RegisterRange(u16 start_port, u16 end_port, const GT_IO_Handler& handler)
{
    if (end_port < start_port)
        return false;

    for (u32 port = start_port; port <= end_port; port++)
    {
        if (m_port_handlers[port] != 0)
            return false;
    }

    u16 handler_index = 0;

    if (!FindHandler(handler, handler_index) && !AddHandler(handler, handler_index))
        return false;

    for (u32 port = start_port; port <= end_port; port++)
        m_port_handlers[port] = handler_index;

    return true;
}

u8 TownsIO::Read8(u16 port, GT_Bus_Access_Context& context)
{
    GT_IO_Handler& handler = m_handlers[m_port_handlers[port]];
    return IsValidPointer(handler.read8) ? handler.read8(handler.device, port, context) : 0xFF;
}

u16 TownsIO::Read16(u16 port, GT_Bus_Access_Context& context)
{
    GT_IO_Handler& handler = m_handlers[m_port_handlers[port]];

    if (IsValidPointer(handler.read16))
        return handler.read16(handler.device, port, context);

    u16 value = Read8(port, context);
    value |= (u16)Read8((u16)(port + 1), context) << 8;
    return value;
}

u32 TownsIO::Read32(u16 port, GT_Bus_Access_Context& context)
{
    GT_IO_Handler& handler = m_handlers[m_port_handlers[port]];

    if (IsValidPointer(handler.read32))
        return handler.read32(handler.device, port, context);

    u32 value = Read16(port, context);
    value |= (u32)Read16((u16)(port + 2), context) << 16;
    return value;
}

void TownsIO::Write8(u16 port, u8 value, GT_Bus_Access_Context& context)
{
    GT_IO_Handler& handler = m_handlers[m_port_handlers[port]];

    if (IsValidPointer(handler.write8))
        handler.write8(handler.device, port, value, context);
}

void TownsIO::Write16(u16 port, u16 value, GT_Bus_Access_Context& context)
{
    GT_IO_Handler& handler = m_handlers[m_port_handlers[port]];

    if (IsValidPointer(handler.write16))
        handler.write16(handler.device, port, value, context);
    else
    {
        Write8(port, (u8)value, context);
        Write8((u16)(port + 1), (u8)(value >> 8), context);
    }
}

void TownsIO::Write32(u16 port, u32 value, GT_Bus_Access_Context& context)
{
    GT_IO_Handler& handler = m_handlers[m_port_handlers[port]];

    if (IsValidPointer(handler.write32))
        handler.write32(handler.device, port, value, context);
    else
    {
        Write16(port, (u16)value, context);
        Write16((u16)(port + 2), (u16)(value >> 16), context);
    }
}

bool TownsIO::FindHandler(const GT_IO_Handler& handler, u16& handler_index) const
{
    for (u16 i = 1; i < m_handler_count; i++)
    {
        const GT_IO_Handler& current = m_handlers[i];

        if (current.device == handler.device &&
            current.read8 == handler.read8 && current.write8 == handler.write8 &&
            current.read16 == handler.read16 && current.write16 == handler.write16 &&
            current.read32 == handler.read32 && current.write32 == handler.write32)
        {
            handler_index = i;
            return true;
        }
    }

    return false;
}

bool TownsIO::AddHandler(const GT_IO_Handler& handler, u16& handler_index)
{
    if (m_handler_count >= GT_IO_MAX_HANDLERS)
        return false;

    handler_index = m_handler_count++;
    m_handlers[handler_index] = handler;
    return true;
}
