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

#ifndef RTC_H
#define RTC_H

#include <iostream>
#include "../common/common.h"

#define RTC_REGISTERS 16

class StateSerializer;

class RTC
{
public:
    struct RTC_State
    {
        u8 registers[RTC_REGISTERS];
        u8 data;
        u8 command;
        u8 address;
        u64 update_clocks;
    };

public:
    RTC();
    ~RTC();
    void Init();
    void Reset(u64 clocks);
    u8 Read(u16 port, u64 clocks);
    void Write(u16 port, u8 value, u64 clocks);
    void Synchronize(u64 clocks);
    RTC_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void WriteRegister(u8 value, u64 clocks);
    u8 ReadRegister(u64 clocks) const;
    void AdvanceSecond();
    int GetValue(int ones, u8 tens_mask) const;
    void SetValue(int ones, u8 tens_mask, int value);
    int GetDaysInMonth() const;
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    RTC_State m_state;
};

static const int k_rtc_seconds = 0x00;
static const int k_rtc_minutes = 0x02;
static const int k_rtc_hours = 0x04;
static const int k_rtc_weekday = 0x06;
static const int k_rtc_day = 0x07;
static const int k_rtc_month = 0x09;
static const int k_rtc_year = 0x0B;
static const int k_rtc_divider_reset = 0x0D;
static const u8 k_rtc_24_hour = 0x08;
static const u8 k_rtc_pm = 0x04;
static const u64 k_rtc_second_clocks = GT_CPU_CLOCK_RATE;
static const u64 k_rtc_busy_clocks = ((u64)GT_CPU_CLOCK_RATE * 14) / 32768;

#include "rtc_inline.h"

#endif /* RTC_H */
