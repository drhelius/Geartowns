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

#ifndef MSM58321_H
#define MSM58321_H

#include <iostream>
#include "../common/common.h"

#define MSM58321_REGISTERS 16

class StateSerializer;
class TraceLogger;

class MSM58321
{
public:
    struct MSM58321_State
    {
        u8 registers[MSM58321_REGISTERS];
        u8 data;
        u8 command;
        u8 address;
        u64 update_clocks;
    };

public:
    MSM58321();
    ~MSM58321();
    void Init();
    void SetTraceLogger(TraceLogger* trace_logger);
    void Reset(u64 clocks);
    u8 Read(u16 port, u64 clocks);
    u8 Peek(u16 port, u64 clocks) const;
    void Write(u16 port, u8 value, u64 clocks);
    void Synchronize(u64 clocks);
    MSM58321_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void WriteRegister(u8 value, u64 clocks);
    u8 ReadRegister(u64 clocks) const;
    void AdvanceSecond();
    int GetValue(int ones, u8 tens_mask) const;
    void SetValue(int ones, u8 tens_mask, int value);
    int GetDaysInMonth() const;
    void TraceEvent(u8 event, u8 value);
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    MSM58321_State m_state;
    TraceLogger* m_trace_logger;
};

static const int k_msm58321_seconds = 0x00;
static const int k_msm58321_minutes = 0x02;
static const int k_msm58321_hours = 0x04;
static const int k_msm58321_weekday = 0x06;
static const int k_msm58321_day = 0x07;
static const int k_msm58321_month = 0x09;
static const int k_msm58321_year = 0x0B;
static const int k_msm58321_divider_reset = 0x0D;
static const u8 k_msm58321_24_hour = 0x08;
static const u8 k_msm58321_pm = 0x04;
static const u64 k_msm58321_second_clocks = GT_CPU_CLOCK_RATE;
static const u64 k_msm58321_busy_clocks = ((u64)GT_CPU_CLOCK_RATE * 14) / 32768;

#include "msm58321_inline.h"

#endif /* MSM58321_H */
