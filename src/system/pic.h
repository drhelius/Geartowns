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

#ifndef PIC_H
#define PIC_H

#include <iostream>
#include "../common/common.h"
#include "i8259.h"

class TraceLogger;

class PIC
{
public:
    void Init();
    void Reset();
    u8 Read(u16 port);
    u8 Peek(u16 port) const;
    void Write(u16 port, u8 value);
    void SetIRQLine(int irq, bool high);
    void SetTraceLogger(TraceLogger* trace_logger);
    bool IsInterruptPending() const;
    u8 AcknowledgeInterrupt();
    u8 AcknowledgeInterrupt(int& line);
    I8259* GetMaster();
    I8259* GetSlave();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void UpdateCascade();
    void TraceRequest(int irq);
    void TraceWrite(int chip, int a0, u8 value, u8 previous_mask, u8 previous_step);

private:
    I8259 m_master;
    I8259 m_slave;
    TraceLogger* m_trace_logger;
};

static const int k_pic_cascade_line = 7;

#include "pic_inline.h"

#endif /* PIC_H */
