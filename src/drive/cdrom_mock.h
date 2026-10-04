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

#ifndef CDROM_MOCK_H
#define CDROM_MOCK_H

#include <iostream>
#include "../common/common.h"

#define CDROM_MOCK_QUEUE_SIZE 64

class TownsPIC;
class Scheduler;
class StateSerializer;

// Temporary stand-in for the CD-ROM controller: a drive with its lid closed and no disc
class CDROMMock
{
public:
    struct CDROMMock_State
    {
        u8 command;
        bool command_received;
        u8 params[8];
        u8 param_count;
        bool busy;
        u64 execute_clocks;
        u8 queue[CDROM_MOCK_QUEUE_SIZE];
        u8 queue_count;
        bool sirq;
        bool enable_sirq;
        bool enable_dei;
    };

public:
    CDROMMock();
    ~CDROMMock();
    void Init(TownsPIC* pic, Scheduler* scheduler);
    void Reset();
    u8 Read(u16 port, u64 clocks);
    void Write(u16 port, u8 value, u64 clocks);
    void Synchronize(u64 clocks);
    void HandleEvent(u64 clocks);
    CDROMMock_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void ResetController();
    void CheckCommand(u64 clocks);
    void ExecuteCommand();
    void PushStatus(u8 status0, u8 status1);
    void SetSIRQ();
    void UpdateIRQ();
    void UpdateNextEvent();
    void Serialize(StateSerializer& serializer);

private:
    TownsPIC* m_pic;
    Scheduler* m_scheduler;
    CDROMMock_State m_state;
};

static const int k_cdrom_mock_irq = 9;
static const u64 k_cdrom_mock_command_delay = GT_CPU_CLOCK_RATE / 20000;

#include "cdrom_mock_inline.h"

#endif /* CDROM_MOCK_H */
