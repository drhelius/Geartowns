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
#include "memory.h"
#include "../common/trace_logger.h"
#include "../common/state_serializer.h"

// 256 bits, bit 255 first: a zero nibble, "FUJITSU", reserved ones, the machine ID and a zero serial number
static const u8 k_system_control_serial_rom[SYSTEM_CONTROL_SERIAL_ROM_SIZE] =
{
    0x04, 0x65, 0x54, 0xA4, 0x95, 0x45, 0x35, 0x5F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

SystemControl::SystemControl()
{
    InitPointer(m_memory);
    InitPointer(m_trace_logger);
    m_machine_id = 0x0101;
    m_features = 0;
    m_wait_cycles = 16;
    m_write_wait_cycles = 16;
    InitPointer(m_bus_mode_context);
    InitPointer(m_bus_mode_callback);
    memcpy(m_serial_rom, k_system_control_serial_rom, sizeof(m_serial_rom));
    m_state.reset_cause = 0;
    m_state.reset_pending = false;
    m_state.write_protect = false;
    m_state.power_off = false;
    m_state.serial_rom_control = 0;
    m_state.serial_rom_bit = 0;
    m_state.expansion_nmi = false;
    m_state.port_05e0 = 0;
    m_state.port_05e2 = 0;
    m_state.port_05e6 = 0;
    m_state.nmi_mask = false;
    m_state.low_speed = false;
    m_state.high_speed = false;
    m_state.port_0486 = 0;
    m_state.port_0487 = 0;
    m_state.port_0488 = 0;
    m_state.port_0494 = 0;
    m_state.port_0496 = 0;
}

SystemControl::~SystemControl()
{
}

void SystemControl::Init(Memory* memory)
{
    m_memory = memory;
    Reset();
}

void SystemControl::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

void SystemControl::SetMachineID(u16 machine_id)
{
    m_machine_id = machine_id;
    m_serial_rom[23] = (u8)(machine_id >> 8);
    m_serial_rom[24] = (u8)(machine_id & 0xFF);
}

void SystemControl::SetSerialRom(const u8* serial_rom)
{
    memcpy(m_serial_rom, IsValidPointer(serial_rom) ? serial_rom : k_system_control_serial_rom, sizeof(m_serial_rom));
    SetMachineID(m_machine_id);
}

void SystemControl::SetFeatures(u32 features)
{
    m_features = features;
}

void SystemControl::SetCPUClockRate(u32 cpu_clock_rate)
{
    m_wait_cycles = cpu_clock_rate / 1000000;
}

void SystemControl::SetWriteWait(u32 cycles)
{
    m_write_wait_cycles = cycles;
}

// Writes that change the bus waits: 05E2h, 05E6h and 05ECh
void SystemControl::SetBusModeCallback(void* context, GT_Bus_Mode_Callback callback)
{
    m_bus_mode_context = context;
    m_bus_mode_callback = callback;
}

bool SystemControl::Decodes(u16 port) const
{
    switch (port)
    {
        case 0x0024:
        case 0x05ED:
            return (m_features & GT_MACHINE_FEATURE_UG_REGISTERS) != 0;
        case 0x0025:
        case 0x0028:
            return (m_features & GT_MACHINE_FEATURE_NMI_MASK) != 0;
        case 0x006C:
            return (m_features & GT_MACHINE_FEATURE_WAIT_1US) != 0;
        case 0x05E2:
        case 0x05E6:
            return (m_features & GT_MACHINE_FEATURE_MEMORY_WAIT) != 0;
        case 0x05EC:
            return (m_features & GT_MACHINE_FEATURE_SPEED_CONTROL) != 0;
        case 0x0486:
        case 0x0487:
        case 0x0488:
        case 0x0494:
        case 0x0496:
            return (m_features & GT_MACHINE_FEATURE_MARTY_PORTS) != 0;
        // The first-generation wait register, gone since the 10F brought 006Ch and 05E2h/05E6h
        case 0x05E0:
            return (m_features & GT_MACHINE_FEATURE_WAIT_1US) == 0;
        // Not on the Marty's board, its RAM card answers it
        case 0x05E8:
            if ((m_features & GT_MACHINE_FEATURE_RAM_CARD) != 0)
                return m_memory->GetMainRAMSize() > k_system_control_board_ram;

            return (m_features & GT_MACHINE_FEATURE_RAM_SIZE) != 0;
        default:
            return true;
    }
}

const u8* SystemControl::GetSerialRom() const
{
    return m_serial_rom;
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
    m_state.expansion_nmi = false;
    m_state.port_05e0 = 0;
    m_state.port_05e2 = 0;
    m_state.port_05e6 = 0;
    m_state.nmi_mask = false;
    m_state.low_speed = false;
    m_state.high_speed = false;
    m_state.port_0486 = 0;
    m_state.port_0487 = 0;
    m_state.port_0488 = 0;
    m_state.port_0494 = 0;
    m_state.port_0496 = 0;
    m_memory->SetNMIVectorProtect(false);
    m_memory->SetBackupRAMProtect(0);
}

u8 SystemControl::Read(u16 port, u64 clocks)
{
    switch (port)
    {
        case 0x0020:
        {
            u8 cause = m_state.reset_cause;
            m_state.reset_cause = 0;
            return cause;
        }
        case 0x0030:
        case 0x0031:
            return Peek(port, clocks);
        case 0x0032:
        {
            u8 bit = m_state.serial_rom_bit;
            u8 data = (m_serial_rom[31 - (bit >> 3)] >> (bit & 0x07)) & 0x01;
            return (m_state.serial_rom_control & 0xC0) | data;
        }
        case 0x05C0:
            // Only BNMI latches, the undefined bits read 0
            return m_state.expansion_nmi ? 0x08 : 0x00;
        case 0x05C2:
            // No expansion board, so the expansion bus never raised the NMI
            return 0x00;
        case 0x0024:
        case 0x0025:
        case 0x0028:
        case 0x006C:
        case 0x0486:
        case 0x0487:
        case 0x0488:
        case 0x0494:
        case 0x0496:
        case 0x05E0:
        case 0x05E2:
        case 0x05E6:
        case 0x05E8:
        case 0x05EC:
        case 0x05ED:
            return Peek(port, clocks);
        default:
            return 0xFF;
    }
}

// The reset cause stays latched
u8 SystemControl::Peek(u16 port, u64 clocks) const
{
    switch (port)
    {
        case 0x0020:
            return m_state.reset_cause;
        case 0x0030:
            return (u8)(m_machine_id & 0xFF);
        case 0x0031:
        {
            bool settling = (m_features & GT_MACHINE_FEATURE_UG_REGISTERS) != 0 &&
                clocks < k_system_control_id_settle_clocks;
            return (u8)(m_machine_id >> 8) | (settling ? 0x80 : 0x00);
        }
        case 0x0032:
        {
            u8 bit = m_state.serial_rom_bit;
            u8 data = (m_serial_rom[31 - (bit >> 3)] >> (bit & 0x07)) & 0x01;
            return (m_state.serial_rom_control & 0xC0) | data;
        }
        case 0x05C0:
            return m_state.expansion_nmi ? 0x08 : 0x00;
        case 0x05C2:
            return 0x00;
        case 0x05E0:
            return Decodes(port) ? m_state.port_05e0 : 0xFF;
        case 0x0024:
            return Decodes(port) ? GetCPUMisc3() : 0xFF;
        case 0x0025:
            return Decodes(port) ? 0x7F : 0xFF;
        case 0x0028:
            return Decodes(port) ? (m_state.nmi_mask ? 0xFF : 0xFE) : 0xFF;
        case 0x006C:
            return Decodes(port) ? 0x00 : 0xFF;
        case 0x0486:
            return Decodes(port) ? m_state.port_0486 : 0xFF;
        case 0x0487:
            return Decodes(port) ? m_state.port_0487 : 0xFF;
        case 0x0488:
            return Decodes(port) ? m_state.port_0488 : 0xFF;
        case 0x0494:
            return Decodes(port) ? m_state.port_0494 : 0xFF;
        case 0x0496:
            return Decodes(port) ? m_state.port_0496 : 0xFF;
        case 0x05E2:
            return Decodes(port) ? m_state.port_05e2 : 0xFF;
        case 0x05E6:
            return Decodes(port) ? m_state.port_05e6 : 0xFF;
        case 0x05E8:
            return Decodes(port) ? (u8)((m_memory->GetMainRAMSize() >> 20) & 0x1F) : 0xFF;
        case 0x05EC:
            return Decodes(port) ? (m_state.high_speed ? 0x01 : 0x00) : 0xFF;
        case 0x05ED:
            return Decodes(port) ? (u8)(m_wait_cycles & 0x7F) : 0xFF;
        default:
            return 0xFF;
    }
}

// Active-low capabilities: the HG chassis can power off its CRT and itself, the UG neither
u8 SystemControl::GetCPUMisc3() const
{
    u8 value = 0x07;

    if ((m_features & GT_MACHINE_FEATURE_FREE_RUN_TIMER) == 0)
        value |= 0x10;

    if ((m_features & GT_MACHINE_FEATURE_CRT_POWER_OFF) == 0)
        value |= 0x28;

    return value;
}

void SystemControl::Write(u16 port, u8 value)
{
    switch (port)
    {
        case 0x0020:
            m_state.write_protect = (value & 0x80) != 0;
            m_memory->SetNMIVectorProtect(m_state.write_protect);

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
        case 0x0028:
            if (Decodes(port))
                m_state.nmi_mask = (value & 0x01) != 0;
            break;
        case 0x0030:
            if ((m_features & GT_MACHINE_FEATURE_LSPEED) != 0)
                m_state.low_speed = (value & 0x01) != 0;
            break;
        case 0x0032:
            WriteSerialROM(value);
            break;
        case 0x05C0:
            m_state.expansion_nmi = (value & 0x08) != 0;
            break;
        // Unknown Marty latches, as wide as the ROM's reset writes mask them
        case 0x0486:
            if (Decodes(port))
            {
                m_state.port_0486 = value & 0x0E;
                m_memory->SetBackupRAMProtect(m_state.port_0486);
            }
            break;
        case 0x0487:
            if (Decodes(port))
                m_state.port_0487 = value & 0x0F;
            break;
        case 0x0488:
            if (Decodes(port))
                m_state.port_0488 = value & 0x01;
            break;
        case 0x0494:
            if (Decodes(port))
                m_state.port_0494 = value & 0x03;
            break;
        case 0x0496:
            if (Decodes(port))
                m_state.port_0496 = value & 0xF0;
            break;
        case 0x05E0:
            if (Decodes(port))
                m_state.port_05e0 = value;
            break;
        case 0x05E2:
            if (Decodes(port) && m_state.port_05e2 != value)
            {
                m_state.port_05e2 = value;
                NotifyBusMode();
            }
            break;
        case 0x05E6:
            if (Decodes(port) && m_state.port_05e6 != value)
            {
                m_state.port_05e6 = value;
                NotifyBusMode();
            }
            break;
        case 0x05EC:
            if (Decodes(port) && m_state.high_speed != ((value & 0x01) != 0))
            {
                m_state.high_speed = (value & 0x01) != 0;
                NotifyBusMode();
            }
            break;
    }

    if (unlikely((port == 0x0020 || port == 0x0022) && IsValidPointer(m_trace_logger) &&
        m_trace_logger->IsEventEnabled(TRACE_SYSTEM, TRACE_SYSTEM_RESET)))
    {
        GT_Trace_Entry entry = {};
        entry.type = TRACE_SYSTEM;
        entry.event = TRACE_SYSTEM_RESET;
        entry.system.port = port;
        entry.system.value = value;
        entry.system.flags = (m_state.reset_pending ? 0x01 : 0x00) | (m_state.write_protect ? 0x02 : 0x00) |
            (m_state.power_off ? 0x04 : 0x00);
        m_trace_logger->TraceLog(entry);
    }
}

void SystemControl::NotifyBusMode()
{
    if (IsValidPointer(m_bus_mode_callback))
        m_bus_mode_callback(m_bus_mode_context);
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
    Serialize(serializer, GT_SAVESTATE_VERSION);
}

void SystemControl::LoadState(std::istream& stream, int version)
{
    StateSerializer serializer(stream);
    Serialize(serializer, version);
    SanitizeState();
}

void SystemControl::Serialize(StateSerializer& serializer, int version)
{
    G_SERIALIZE(serializer, m_state.reset_cause);
    G_SERIALIZE(serializer, m_state.reset_pending);
    G_SERIALIZE(serializer, m_state.write_protect);
    G_SERIALIZE(serializer, m_state.power_off);
    G_SERIALIZE(serializer, m_state.serial_rom_control);
    G_SERIALIZE(serializer, m_state.serial_rom_bit);
    G_SERIALIZE(serializer, m_state.expansion_nmi);
    G_SERIALIZE(serializer, m_state.port_05e0);

    if (version >= 3)
    {
        G_SERIALIZE(serializer, m_state.port_05e2);
        G_SERIALIZE(serializer, m_state.port_05e6);
        G_SERIALIZE(serializer, m_state.nmi_mask);
        G_SERIALIZE(serializer, m_state.low_speed);
    }
    else
    {
        m_state.port_05e2 = 0;
        m_state.port_05e6 = 0;
        m_state.nmi_mask = false;
        m_state.low_speed = false;
    }

    if (version >= 4)
        G_SERIALIZE(serializer, m_state.high_speed);
    else
        m_state.high_speed = false;

    if (version >= 11)
    {
        G_SERIALIZE(serializer, m_state.port_0486);
        G_SERIALIZE(serializer, m_state.port_0487);
        G_SERIALIZE(serializer, m_state.port_0488);
        G_SERIALIZE(serializer, m_state.port_0494);
        G_SERIALIZE(serializer, m_state.port_0496);
    }
    else
    {
        m_state.port_0486 = 0;
        m_state.port_0487 = 0;
        m_state.port_0488 = 0;
        m_state.port_0494 = 0;
        m_state.port_0496 = 0;
    }
}

void SystemControl::SanitizeState()
{
    m_state.reset_cause &= 0x03;
    m_state.port_0486 &= 0x0E;
    m_memory->SetNMIVectorProtect(m_state.write_protect);
    m_memory->SetBackupRAMProtect(m_state.port_0486);
}
