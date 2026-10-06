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

#include "pic.h"

void PIC::Init()
{
    InitPointer(m_trace_logger);
    m_master.Init(true);
    m_slave.Init(false);
}

void PIC::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

// Only a rising input is a new request
void PIC::TraceRequest(int irq)
{
    I8259* chip = irq >= 8 ? &m_slave : &m_master;
    u8 bit = (u8)(1 << (irq & 7));

    if ((chip->GetState()->input_levels & bit) != 0)
        return;

    GT_Trace_Entry* entry = m_trace_logger->Record(TRACE_INTERRUPT, TRACE_INTERRUPT_REQUEST);
    entry->interrupt.from = 0;
    entry->interrupt.to = 0;
    entry->interrupt.error_code = 0;
    entry->interrupt.vector = (u8)((chip->GetState()->icw2 & 0xF8) | (irq & 7));
    entry->interrupt.source = 0;
    entry->interrupt.line = (u8)irq;
    entry->interrupt.has_error_code = 0;
}

void PIC::Reset()
{
    m_master.Reset();
    m_slave.Reset();
}

u8 PIC::Read(u16 port)
{
    int a0 = (port >> 1) & 0x01;

    if ((port & 0x10) == 0)
        return m_master.Read(a0);

    u8 value = m_slave.Read(a0);
    UpdateCascade();
    return value;
}

void PIC::Write(u16 port, u8 value)
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

u8 PIC::AcknowledgeInterrupt()
{
    int line = m_master.Acknowledge();

    // A default IR7 drives cascade address 7, so the slave supplies the vector without any service bit
    if (line < 0)
    {
        Debug("PIC: spurious interrupt acknowledge");
        line = k_pic_cascade_line;
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

void PIC::SaveState(std::ostream& stream)
{
    m_master.SaveState(stream);
    m_slave.SaveState(stream);
}

void PIC::LoadState(std::istream& stream)
{
    m_master.LoadState(stream);
    m_slave.LoadState(stream);
}
