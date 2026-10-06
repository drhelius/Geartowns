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

#include "system_control.h"
#include "../common/state_serializer.h"

// 256 bits, bit 255 first: a zero nibble, "FUJITSU", reserved ones, model 0101h and a zero serial number
static const u8 k_system_control_serial_rom[32] =
{
    0x04, 0x65, 0x54, 0xA4, 0x95, 0x45, 0x35, 0x5F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

SystemControl::SystemControl()
{
    m_state.reset_cause = 0;
    m_state.reset_pending = false;
    m_state.write_protect = false;
    m_state.power_off = false;
    m_state.serial_rom_control = 0;
    m_state.serial_rom_bit = 0;
    m_state.main_ram_wait = 0;
}

SystemControl::~SystemControl()
{
}

void SystemControl::Init()
{
    Reset();
}

// Power-on state, a CPU reset keeps the reset causes for the BIOS to read
void SystemControl::Reset()
{
    m_state.reset_cause = 0;
    m_state.reset_pending = false;
    m_state.write_protect = false;
    m_state.power_off = false;
    m_state.serial_rom_control = 0;
    m_state.serial_rom_bit = 0;
    m_state.main_ram_wait = 0;
}

u8 SystemControl::Read(u16 port)
{
    switch (port)
    {
        case 0x0020:
        {
            u8 cause = m_state.reset_cause;
            m_state.reset_cause = 0;
            return cause;
        }
        case 0x0032:
        {
            u8 bit = m_state.serial_rom_bit;
            u8 data = (k_system_control_serial_rom[31 - (bit >> 3)] >> (bit & 0x07)) & 0x01;
            return (m_state.serial_rom_control & 0xC0) | data;
        }
        case 0x05E0:
            return m_state.main_ram_wait;
        default:
            return 0xFF;
    }
}

// The reset cause stays latched
u8 SystemControl::Peek(u16 port) const
{
    switch (port)
    {
        case 0x0020:
            return m_state.reset_cause;
        case 0x0032:
        {
            u8 bit = m_state.serial_rom_bit;
            u8 data = (k_system_control_serial_rom[31 - (bit >> 3)] >> (bit & 0x07)) & 0x01;
            return (m_state.serial_rom_control & 0xC0) | data;
        }
        case 0x05E0:
            return m_state.main_ram_wait;
        default:
            return 0xFF;
    }
}

void SystemControl::Write(u16 port, u8 value)
{
    switch (port)
    {
        case 0x0020:
            m_state.write_protect = (value & 0x80) != 0;

            if ((value & 0x40) != 0)
            {
                m_state.power_off = true;
                Debug("System: power off requested");
            }

            if ((value & 0x01) != 0)
                RequestCPUReset(k_system_control_reset_soft);
            break;
        case 0x0022:
            if ((value & 0x40) != 0)
            {
                m_state.power_off = true;
                Debug("System: power off requested");
            }
            break;
        case 0x0032:
            WriteSerialROM(value);
            break;
        case 0x05E0:
            m_state.main_ram_wait = value;
            break;
    }
}

// While the active low chip select is on, an ID RESET falling edge rewinds the ROM
// and an ID CLK rising edge with ID RESET low advances it one bit
void SystemControl::WriteSerialROM(u8 value)
{
    u8 previous = m_state.serial_rom_control;

    if ((value & 0x20) == 0)
    {
        if ((previous & 0x80) != 0 && (value & 0x80) == 0)
            m_state.serial_rom_bit = 0;
        else if ((value & 0x80) == 0 && (previous & 0x40) == 0 && (value & 0x40) != 0)
            m_state.serial_rom_bit++;
    }

    m_state.serial_rom_control = value;
}

void SystemControl::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void SystemControl::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void SystemControl::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_state.reset_cause);
    G_SERIALIZE(serializer, m_state.reset_pending);
    G_SERIALIZE(serializer, m_state.write_protect);
    G_SERIALIZE(serializer, m_state.power_off);
    G_SERIALIZE(serializer, m_state.serial_rom_control);
    G_SERIALIZE(serializer, m_state.serial_rom_bit);
    G_SERIALIZE(serializer, m_state.main_ram_wait);
}

void SystemControl::SanitizeState()
{
    m_state.reset_cause &= 0x03;
}
