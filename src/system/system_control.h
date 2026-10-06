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
        u8 main_ram_wait;
    };

public:
    SystemControl();
    ~SystemControl();
    void Init();
    void Reset();
    u8 Read(u16 port);
    u8 Peek(u16 port) const;
    void Write(u16 port, u8 value);
    void RequestCPUReset(u8 cause);
    bool IsCPUResetPending() const;
    void AcknowledgeCPUReset();
    SystemControl_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void WriteSerialROM(u8 value);
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    SystemControl_State m_state;
};

static const u8 k_system_control_reset_soft = 0x01;
static const u8 k_system_control_reset_shutdown = 0x02;

#include "system_control_inline.h"

#endif /* SYSTEM_CONTROL_H */
