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
#include "i8251.h"
#include "../common/state_serializer.h"

I8251::I8251()
{
    m_clock_divisor = 1;
    Reset();
}

I8251::~I8251()
{
}

void I8251::Init(u32 clock_divisor)
{
    m_clock_divisor = clock_divisor;
    Reset();
}

void I8251::Reset()
{
    m_state.step = I8251_STEP_MODE;
    m_state.mode = 0;
    m_state.command = 0;
    m_state.holding = 0;
    m_state.holding_full = false;
    m_state.shift = 0;
    m_state.shift_full = false;
    m_state.shift_clocks = 0;
}

// Nothing is wired to the receiver or to DSR
u8 I8251::ReadStatus() const
{
    u8 status = 0;

    if (!m_state.holding_full)
        status |= k_i8251_status_tx_ready;

    if (!m_state.holding_full && !m_state.shift_full)
        status |= k_i8251_status_tx_empty;

    return status;
}

void I8251::WriteData(u8 value, u64 clocks)
{
    m_state.holding = value;
    m_state.holding_full = true;
    LoadShift(clocks);
}

// A mode byte follows a reset, and one or two sync characters a synchronous mode. The rest are commands
void I8251::WriteControl(u8 value, u64 clocks)
{
    switch (m_state.step)
    {
        case I8251_STEP_MODE:
            m_state.mode = value;

            if ((value & 0x03) != 0)
                m_state.step = I8251_STEP_COMMAND;
            else
                m_state.step = (value & 0x80) != 0 ? I8251_STEP_SYNC_2 : I8251_STEP_SYNC_1;

            break;
        case I8251_STEP_SYNC_1:
            m_state.step = I8251_STEP_SYNC_2;
            break;
        case I8251_STEP_SYNC_2:
            m_state.step = I8251_STEP_COMMAND;
            break;
        default:
            if ((value & k_i8251_command_reset) != 0)
            {
                Reset();
                break;
            }

            m_state.command = value;
            LoadShift(clocks);
            break;
    }
}

bool I8251::IsTransmitReady() const
{
    return !m_state.holding_full && ((m_state.command & k_i8251_command_transmit) != 0);
}

// Gives the character that has left the transmitter by the given clock
bool I8251::Shift(u64 clocks, u8& value)
{
    if (!m_state.shift_full || (clocks < m_state.shift_clocks))
        return false;

    value = m_state.shift;
    m_state.shift_full = false;
    LoadShift(m_state.shift_clocks);
    return true;
}

void I8251::LoadShift(u64 clocks)
{
    if (!m_state.holding_full || m_state.shift_full || ((m_state.command & k_i8251_command_transmit) == 0))
        return;

    m_state.shift = m_state.holding;
    m_state.shift_full = true;
    m_state.holding_full = false;
    m_state.shift_clocks = clocks + GetCharacterClocks();
}

// Start, data, parity and stop bits, counted in halves for the one and a half stop bits
u32 I8251::GetCharacterClocks() const
{
    static const u32 k_factors[4] = { 1, 1, 16, 64 };
    static const u32 k_stop_bits[4] = { 2, 2, 3, 4 };

    u32 bits = 5 + ((m_state.mode >> 2) & 0x03) + ((m_state.mode >> 4) & 0x01);
    u32 half_bits = bits * 2;

    if ((m_state.mode & 0x03) != 0)
        half_bits += 2 + k_stop_bits[m_state.mode >> 6];

    return (half_bits * k_factors[m_state.mode & 0x03] * m_clock_divisor) / 2;
}

void I8251::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void I8251::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void I8251::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_state.step);
    G_SERIALIZE(serializer, m_state.mode);
    G_SERIALIZE(serializer, m_state.command);
    G_SERIALIZE(serializer, m_state.holding);
    G_SERIALIZE(serializer, m_state.holding_full);
    G_SERIALIZE(serializer, m_state.shift);
    G_SERIALIZE(serializer, m_state.shift_full);
    G_SERIALIZE(serializer, m_state.shift_clocks);
}

void I8251::SanitizeState()
{
    if (m_state.step > I8251_STEP_COMMAND)
        m_state.step = I8251_STEP_MODE;
}
