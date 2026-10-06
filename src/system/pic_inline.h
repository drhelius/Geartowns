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

#ifndef PIC_INLINE_H
#define PIC_INLINE_H

#include "pic.h"
#include "../common/trace_logger.h"

INLINE void PIC::SetIRQLine(int irq, bool high)
{
    if (unlikely(high && IsValidPointer(m_trace_logger) && m_trace_logger->IsEnabled(TRACE_INTERRUPT)))
        TraceRequest(irq);

    if (irq >= 8)
    {
        m_slave.SetInputLine(irq - 8, high);
        UpdateCascade();
    }
    else if (irq != k_pic_cascade_line)
        m_master.SetInputLine(irq, high);
    else
        Debug("PIC: IRQ 7 is the slave cascade input");
}

INLINE u8 PIC::Peek(u16 port) const
{
    int a0 = (port >> 1) & 0x01;
    return (port & 0x10) == 0 ? m_master.Peek(a0) : m_slave.Peek(a0);
}

INLINE bool PIC::IsInterruptPending() const
{
    return m_master.IsIRQAsserted();
}

INLINE I8259* PIC::GetMaster()
{
    return &m_master;
}

INLINE I8259* PIC::GetSlave()
{
    return &m_slave;
}

INLINE void PIC::UpdateCascade()
{
    m_master.SetInputLine(k_pic_cascade_line, m_slave.IsIRQAsserted());
}

#endif /* PIC_INLINE_H */
