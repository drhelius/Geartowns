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

#ifndef FDC_H
#define FDC_H

#include <iostream>
#include "../common/common.h"
#include "mb8877.h"
#include "floppy_disk.h"

#define FDC_DRIVES 2

class PIC;
class Scheduler;
class UPD71071;
class StateSerializer;

class FDC
{
public:
    struct FDC_State
    {
        u8 drive_control;
        u8 drive_select;
        u8 drive_switch;
        bool high_speed;
        bool mode_b;
        bool in_use;
        bool ready;
        bool intrq;
        u8 cylinders[FDC_DRIVES];
        u64 ready_clocks[FDC_DRIVES];
    };

public:
    FDC();
    ~FDC();
    void Init(PIC* pic, Scheduler* scheduler, UPD71071* dma);
    void Reset();
    void SetInternalDrives(int drives);
    u8 Read(u16 port, u64 clocks);
    void Write(u16 port, u8 value, u64 clocks);
    void Synchronize(u64 clocks);
    void HandleEvent(u64 clocks);
    bool InsertDisk(int drive, const u8* data, u32 size, bool write_protected, u32 base_crc);
    void EjectDisk(int drive);
    void SwapDisks();
    FloppyDisk* GetDisk(int drive);
    MB8877* GetMB8877();
    FDC_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

    FloppyDisk* GetSelectedDisk();
    int GetSelectedTrack() const;
    bool IsSelectedReadable() const;
    bool IsSelectedFormattable() const;
    bool IsReady(u64 clocks) const;
    bool IsSpinning() const;
    bool IsIndex(u64 clocks) const;
    bool IsTrackZero() const;
    bool IsWriteProtected() const;
    bool IsDoubleDensity() const;
    u32 GetCycleClocks() const;
    u32 GetByteClocks() const;
    u32 GetRPM() const;
    void Step(bool in);
    void SetINTRQ(bool active);
    void SetDRQ(bool active);

private:
    int GetSelectedDrive() const;
    void ChangeDisk(int drive);
    void ChangeSelection(int previous, u64 clocks);
    void UpdateIRQ();
    void UpdateReady(u64 clocks);
    void UpdateNextEvent(u64 clocks);
    static bool DMAReadCallback(void* device, u16& value, bool word);
    static bool DMAWriteCallback(void* device, u16 value, bool word);
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    PIC* m_pic;
    Scheduler* m_scheduler;
    UPD71071* m_dma;
    MB8877 m_mb8877;
    FloppyDisk m_disks[FDC_DRIVES];
    FDC_State m_state;
    int m_internal_drives;
};

static const int k_fdc_irq = 6;
static const int k_fdc_dma_channel = 0;
static const u8 k_fdc_last_cylinder = 82;
static const u8 k_fdc_irq_enable = 0x01;
static const u8 k_fdc_double_density = 0x02;
static const u8 k_fdc_side = 0x04;
static const u8 k_fdc_motor = 0x10;
static const u8 k_fdc_slow_clock = 0x20;

// The drive stays not ready for a moment after a disk change, so the BIOS sees the change
static const u64 k_fdc_disk_change_clocks = GT_CPU_CLOCK_RATE / 20;

#include "fdc_inline.h"

#endif /* FDC_H */
