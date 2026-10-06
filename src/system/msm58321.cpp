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

#include <time.h>
#include "msm58321.h"
#include "../common/state_serializer.h"

MSM58321::MSM58321()
{
    memset(&m_state, 0, sizeof(m_state));
}

MSM58321::~MSM58321()
{
}

void MSM58321::Init()
{
    Reset(0);
}

// Power on starts from the host local time in 24 hour mode, the year counter holds two digits
void MSM58321::Reset(u64 clocks)
{
    memset(&m_state, 0, sizeof(m_state));
    m_state.registers[k_msm58321_hours + 1] = k_msm58321_24_hour;
    m_state.update_clocks = clocks + k_msm58321_second_clocks;

    time_t now = time(NULL);
    struct tm* local = localtime(&now);

    if (!IsValidPointer(local))
    {
        SetValue(k_msm58321_day, 0x03, 1);
        SetValue(k_msm58321_month, 0x01, 1);
        return;
    }

    SetValue(k_msm58321_seconds, 0x07, MIN(local->tm_sec, 59));
    SetValue(k_msm58321_minutes, 0x07, local->tm_min);
    SetValue(k_msm58321_hours, 0x03, local->tm_hour);
    m_state.registers[k_msm58321_weekday] = (u8)local->tm_wday;
    SetValue(k_msm58321_day, 0x03, local->tm_mday);
    SetValue(k_msm58321_month, 0x01, local->tm_mon + 1);
    SetValue(k_msm58321_year, 0x0F, local->tm_year % 100);
}

u8 MSM58321::Read(u16 port, u64 clocks)
{
    Synchronize(clocks);

    switch (port)
    {
        case 0x0070:
        {
            // READY drops for the 427 us before each counter update
            u8 value = clocks + k_msm58321_busy_clocks < m_state.update_clocks ? 0x80 : 0x00;

            if ((m_state.command & 0x84) == 0x84)
                value |= ReadRegister(clocks);

            return value;
        }
        default:
            return 0xFF;
    }
}

// The strobes are levels, so a nibble written while one is held takes effect at once
void MSM58321::Write(u16 port, u8 value, u64 clocks)
{
    Synchronize(clocks);

    switch (port)
    {
        case 0x0070:
            m_state.data = value & 0x0F;
            break;
        case 0x0080:
            m_state.command = value;
            break;
        default:
            return;
    }

    if ((m_state.command & 0x80) == 0)
        return;

    if ((m_state.command & 0x01) != 0)
        m_state.address = m_state.data;

    if ((m_state.command & 0x02) != 0)
        WriteRegister(m_state.data, clocks);
}

void MSM58321::WriteRegister(u8 value, u64 clocks)
{
    switch (m_state.address)
    {
        case 0x01:
        case 0x03:
        case 0x06:
            m_state.registers[m_state.address] = value & 0x07;
            break;
        case 0x05:
            // Selecting 24 hour mode clears PM
            m_state.registers[m_state.address] = (value & k_msm58321_24_hour) != 0 ? value & ~k_msm58321_pm : value;
            break;
        case 0x0A:
            m_state.registers[m_state.address] = value & 0x01;
            break;
        case k_msm58321_divider_reset:
            // The divider restarts, so the next update comes a full second later
            m_state.update_clocks = clocks + k_msm58321_second_clocks;
            break;
        case 0x0E:
        case 0x0F:
            break;
        default:
            m_state.registers[m_state.address] = value;
            break;
    }
}

u8 MSM58321::ReadRegister(u64 clocks) const
{
    switch (m_state.address)
    {
        case k_msm58321_divider_reset:
            return 0x00;
        case 0x0E:
        case 0x0F:
            // Reference outputs, the 1024 Hz signal on D0 and the slower ones idle
            return 0x0E | (u8)(((clocks * 2048) / GT_CPU_CLOCK_RATE) & 0x01);
        default:
            return m_state.registers[m_state.address];
    }
}

void MSM58321::AdvanceSecond()
{
    int second = GetValue(k_msm58321_seconds, 0x07) + 1;

    if (second < 60)
    {
        SetValue(k_msm58321_seconds, 0x07, second);
        return;
    }

    SetValue(k_msm58321_seconds, 0x07, 0);
    int minute = GetValue(k_msm58321_minutes, 0x07) + 1;

    if (minute < 60)
    {
        SetValue(k_msm58321_minutes, 0x07, minute);
        return;
    }

    SetValue(k_msm58321_minutes, 0x07, 0);
    int hour = GetValue(k_msm58321_hours, 0x03) + 1;
    u8& hour_flags = m_state.registers[k_msm58321_hours + 1];

    // 12 hour mode counts 0 to 11 and toggles PM at noon and midnight
    if ((hour_flags & k_msm58321_24_hour) != 0)
    {
        if (hour < 24)
        {
            SetValue(k_msm58321_hours, 0x03, hour);
            return;
        }

        SetValue(k_msm58321_hours, 0x03, 0);
    }
    else
    {
        if (hour < 12)
        {
            SetValue(k_msm58321_hours, 0x03, hour);
            return;
        }

        SetValue(k_msm58321_hours, 0x03, 0);
        hour_flags ^= k_msm58321_pm;

        if ((hour_flags & k_msm58321_pm) != 0)
            return;
    }

    m_state.registers[k_msm58321_weekday] = (u8)((m_state.registers[k_msm58321_weekday] + 1) % 7);
    int day = GetValue(k_msm58321_day, 0x03) + 1;

    if (day <= GetDaysInMonth())
    {
        SetValue(k_msm58321_day, 0x03, day);
        return;
    }

    SetValue(k_msm58321_day, 0x03, 1);
    int month = GetValue(k_msm58321_month, 0x01) + 1;

    if (month <= 12)
    {
        SetValue(k_msm58321_month, 0x01, month);
        return;
    }

    SetValue(k_msm58321_month, 0x01, 1);
    SetValue(k_msm58321_year, 0x0F, (GetValue(k_msm58321_year, 0x0F) + 1) % 100);
}

// February is leap when the year modulo 4 matches the phase in bits 2-3 of the day tens
int MSM58321::GetDaysInMonth() const
{
    int month = GetValue(k_msm58321_month, 0x01);

    if (month == 2)
    {
        int phase = (m_state.registers[k_msm58321_day + 1] >> 2) & 0x03;
        return (GetValue(k_msm58321_year, 0x0F) % 4) == phase ? 29 : 28;
    }

    if (month == 4 || month == 6 || month == 9 || month == 11)
        return 30;

    return 31;
}

void MSM58321::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void MSM58321::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void MSM58321::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE_ARRAY(serializer, m_state.registers, MSM58321_REGISTERS);
    G_SERIALIZE(serializer, m_state.data);
    G_SERIALIZE(serializer, m_state.command);
    G_SERIALIZE(serializer, m_state.address);
    G_SERIALIZE(serializer, m_state.update_clocks);
}

void MSM58321::SanitizeState()
{
    for (int i = 0; i < MSM58321_REGISTERS; i++)
        m_state.registers[i] &= 0x0F;

    m_state.data &= 0x0F;
    m_state.address &= 0x0F;
}
