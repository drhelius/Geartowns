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

#ifndef SYSTEM_CONTROL_H
#define SYSTEM_CONTROL_H

#include <iostream>
#include "../common/common.h"

class StateSerializer;
class TraceLogger;
class Memory;

#define SYSTEM_CONTROL_SERIAL_ROM_SIZE 32

class SystemControl
{
public:
    struct SystemControl_State
    {
        u8 reset_cause;
        bool reset_pending;
        bool write_protect;
        bool power_off;
        u8 serial_rom_control;
        u8 serial_rom_bit;
        bool expansion_nmi;
        u8 port_05e0;
        u8 port_05e2;
        u8 port_05e6;
        bool nmi_mask;
        bool low_speed;
    };

public:
    SystemControl();
    ~SystemControl();
    void Init(Memory* memory);
    void SetTraceLogger(TraceLogger* trace_logger);
    void Reset();
    void SetMachineID(u16 machine_id);
    void SetSerialRom(const u8* serial_rom);
    void SetFeatures(u32 features);
    void SetCPUClockRate(u32 cpu_clock_rate);
    bool Decodes(u16 port) const;
    void Wait(GT_Bus_Access_Context& context) const;
    u8 Read(u16 port);
    u8 Peek(u16 port) const;
    const u8* GetSerialRom() const;
    void Write(u16 port, u8 value);
    void RequestCPUReset(u8 cause);
    bool IsCPUResetPending() const;
    void AcknowledgeCPUReset();
    bool IsPowerOffPending() const;
    SystemControl_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream, int version = GT_SAVESTATE_VERSION);

private:
    void WriteSerialROM(u8 value);
    void Serialize(StateSerializer& serializer, int version);
    void SanitizeState();

private:
    SystemControl_State m_state;
    Memory* m_memory;
    TraceLogger* m_trace_logger;
    u16 m_machine_id;
    u32 m_features;
    u32 m_wait_cycles;
    u8 m_serial_rom[SYSTEM_CONTROL_SERIAL_ROM_SIZE];
};

static const u8 k_system_control_reset_soft = 0x01;
static const u8 k_system_control_reset_shutdown = 0x02;

#include "system_control_inline.h"

#endif /* SYSTEM_CONTROL_H */
