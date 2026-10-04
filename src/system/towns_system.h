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

#ifndef TOWNS_SYSTEM_H
#define TOWNS_SYSTEM_H

#include <iostream>
#include "../common/common.h"

class StateSerializer;

class TownsSystem
{
public:
    struct TownsSystem_State
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
    TownsSystem();
    ~TownsSystem();
    void Init();
    void Reset();
    u8 Read(u16 port);
    void Write(u16 port, u8 value);
    void RequestCPUReset(u8 cause);
    bool IsCPUResetPending() const;
    void AcknowledgeCPUReset();
    TownsSystem_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void WriteSerialROM(u8 value);
    void Serialize(StateSerializer& serializer);

private:
    TownsSystem_State m_state;
};

static const u8 k_towns_system_reset_soft = 0x01;
static const u8 k_towns_system_reset_shutdown = 0x02;

#include "towns_system_inline.h"

#endif /* TOWNS_SYSTEM_H */
