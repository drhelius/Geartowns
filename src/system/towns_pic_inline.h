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

#ifndef TOWNS_PIC_INLINE_H
#define TOWNS_PIC_INLINE_H

#include "towns_pic.h"

INLINE void TownsPIC::SetIRQLine(int irq, bool high)
{
    if (irq >= 8)
    {
        m_slave.SetInputLine(irq - 8, high);
        UpdateCascade();
    }
    else if (irq != k_towns_pic_cascade_line)
        m_master.SetInputLine(irq, high);
    else
        Debug("PIC: IRQ 7 is the slave cascade input");
}

INLINE bool TownsPIC::IsInterruptPending() const
{
    return m_master.IsIRQAsserted();
}

INLINE I8259* TownsPIC::GetMaster()
{
    return &m_master;
}

INLINE I8259* TownsPIC::GetSlave()
{
    return &m_slave;
}

INLINE void TownsPIC::UpdateCascade()
{
    m_master.SetInputLine(k_towns_pic_cascade_line, m_slave.IsIRQAsserted());
}

#endif /* TOWNS_PIC_INLINE_H */
