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

#include "towns_pic.h"

void TownsPIC::Init()
{
    m_master.Init(true);
    m_slave.Init(false);
}

void TownsPIC::Reset()
{
    m_master.Reset();
    m_slave.Reset();
}

u8 TownsPIC::Read(u16 port)
{
    int a0 = (port >> 1) & 0x01;

    if ((port & 0x10) == 0)
        return m_master.Read(a0);

    u8 value = m_slave.Read(a0);
    UpdateCascade();
    return value;
}

void TownsPIC::Write(u16 port, u8 value)
{
    int a0 = (port >> 1) & 0x01;

    if ((port & 0x10) == 0)
        m_master.Write(a0, value);
    else
    {
        m_slave.Write(a0, value);
        UpdateCascade();
    }
}

u8 TownsPIC::AcknowledgeInterrupt()
{
    int line = m_master.Acknowledge();

    // A default IR7 drives cascade address 7, so the slave supplies the vector without any service bit
    if (line < 0)
    {
        Debug("PIC: spurious interrupt acknowledge");
        line = k_towns_pic_cascade_line;
        return m_master.IsCascadeLine(line) ? m_slave.GetVector(7) : m_master.GetVector(line);
    }

    if (!m_master.IsCascadeLine(line))
        return m_master.GetVector(line);

    int slave_line = m_slave.Acknowledge();
    UpdateCascade();

    if (slave_line < 0)
    {
        Debug("PIC: spurious slave interrupt acknowledge");
        slave_line = 7;
    }

    return m_slave.GetVector(slave_line);
}

void TownsPIC::SaveState(std::ostream& stream)
{
    m_master.SaveState(stream);
    m_slave.SaveState(stream);
}

void TownsPIC::LoadState(std::istream& stream)
{
    m_master.LoadState(stream);
    m_slave.LoadState(stream);
}
