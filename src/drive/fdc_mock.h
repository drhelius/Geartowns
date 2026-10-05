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

#ifndef FDC_MOCK_H
#define FDC_MOCK_H

#include <iostream>
#include "../common/common.h"

#define FDC_MOCK_DRIVES 4

class PIC;
class Scheduler;
class StateSerializer;

class FDCMock
{
public:
    struct FDCMock_State
    {
        u8 command;
        u8 track;
        u8 sector;
        u8 data;
        bool busy;
        bool type1;
        bool seek_error;
        bool intrq;
        bool step_in;
        u64 execute_clocks;
        u8 drive_control;
        u8 drive_select;
        u8 drive_switch;
        u8 cylinders[FDC_MOCK_DRIVES];
    };

public:
    FDCMock();
    ~FDCMock();
    void Init(PIC* pic, Scheduler* scheduler);
    void Reset();
    void SetInternalDrives(int drives);
    u8 Read(u16 port, u64 clocks);
    void Write(u16 port, u8 value, u64 clocks);
    void Synchronize(u64 clocks);
    void HandleEvent(u64 clocks);
    FDCMock_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void WriteCommand(u8 value, u64 clocks);
    void CompleteCommand();
    void MoveHead(int drive, int steps);
    int GetSelectedDrive() const;
    u64 GetCommandClocks() const;
    u8 GetStatus() const;
    void UpdateIRQ();
    void UpdateNextEvent();
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    PIC* m_pic;
    Scheduler* m_scheduler;
    FDCMock_State m_state;
    int m_internal_drives;
};

static const int k_fdc_mock_irq = 6;
static const int k_fdc_mock_last_cylinder = 80;
static const int k_fdc_mock_restore_steps = 255;
static const u64 k_fdc_mock_command_delay = GT_CPU_CLOCK_RATE / 20000;
static const u64 k_fdc_mock_settle_delay = (GT_CPU_CLOCK_RATE * 15) / 1000;

#include "fdc_mock_inline.h"

#endif /* FDC_MOCK_H */
