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
    I8259::I8259_State* state = irq >= 8 ? m_slave.GetState() : m_master.GetState();
    u8 bit = (u8)(1 << (irq & 7));

    if ((state->input_levels & bit) != 0)
        return;

    GT_Trace_Entry entry = {};
    entry.type = TRACE_PIC;
    entry.event = TRACE_PIC_REQUEST;
    entry.pic.chip = irq >= 8 ? 1 : 0;
    entry.pic.line = (u8)irq;
    entry.pic.vector = (u8)((state->icw2 & 0xF8) | (irq & 7));
    entry.pic.irr = state->irr;
    entry.pic.isr = state->isr;
    entry.pic.imr = state->imr;
    m_trace_logger->TraceLog(entry);
}

void PIC::TraceWrite(int chip, int a0, u8 value, u8 previous_mask, u8 previous_step)
{
    u8 event = TRACE_PIC_COMMAND;

    if ((a0 == 0 && (value & k_i8259_icw1_init) != 0) || (a0 != 0 && previous_step != I8259::I8259_INIT_READY))
        event = TRACE_PIC_INIT;
    else if (a0 != 0)
        event = TRACE_PIC_MASK;

    if (!m_trace_logger->IsEventEnabled(TRACE_PIC, event))
        return;

    I8259::I8259_State* state = chip != 0 ? m_slave.GetState() : m_master.GetState();
    GT_Trace_Entry entry = {};
    entry.type = TRACE_PIC;
    entry.event = event;
    entry.pic.chip = (u8)chip;
    entry.pic.value = value;
    entry.pic.previous = previous_mask;
    entry.pic.irr = state->irr;
    entry.pic.isr = state->isr;
    entry.pic.imr = state->imr;
    entry.pic.step = a0 == 0 ? (u8)I8259::I8259_INIT_READY : previous_step;
    m_trace_logger->TraceLog(entry);
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
    int chip = (port >> 4) & 0x01;
    I8259::I8259_State* state = chip != 0 ? m_slave.GetState() : m_master.GetState();
    u8 previous_mask = state->imr;
    u8 previous_step = (u8)state->init_step;

    if (chip == 0)
        m_master.Write(a0, value);
    else
    {
        m_slave.Write(a0, value);
        UpdateCascade();
    }

    if (unlikely(IsValidPointer(m_trace_logger) && m_trace_logger->IsEnabled(TRACE_PIC)))
        TraceWrite(chip, a0, value, previous_mask, previous_step);
}

u8 PIC::AcknowledgeInterrupt()
{
    int line = 0;
    return AcknowledgeInterrupt(line);
}

// Also returns the IRQ line 0-15 that supplied the vector
u8 PIC::AcknowledgeInterrupt(int& line)
{
    line = m_master.Acknowledge();

    // A default IR7 drives cascade address 7, so the slave supplies the vector without any service bit
    if (line < 0)
    {
        Debug("PIC: spurious interrupt acknowledge");
        line = k_pic_cascade_line;

        if (!m_master.IsCascadeLine(line))
            return m_master.GetVector(line);

        line = 8 + 7;
        return m_slave.GetVector(7);
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

    line = 8 + slave_line;
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
