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

#include "fdc.h"
#include "../system/pic.h"
#include "../system/scheduler.h"
#include "../system/upd71071.h"
#include "../common/state_serializer.h"

FDC::FDC()
{
    InitPointer(m_pic);
    InitPointer(m_scheduler);
    InitPointer(m_dma);
    memset(&m_state, 0, sizeof(m_state));
    m_internal_drives = FDC_DRIVES;
}

FDC::~FDC()
{
}

void FDC::Init(PIC* pic, Scheduler* scheduler, UPD71071* dma)
{
    m_pic = pic;
    m_scheduler = scheduler;
    m_dma = dma;
    m_mb8877.Init(this);

    GT_DMA_Endpoint endpoint = { this, DMAReadCallback, DMAWriteCallback, NULL };
    m_dma->SetEndpoint(k_fdc_dma_channel, endpoint);

    Reset();
}

// Disks stay in their drives, a disk present at power on is ready at once
void FDC::Reset()
{
    memset(&m_state, 0, sizeof(m_state));
    m_mb8877.Reset();
    UpdateIRQ();
    UpdateNextEvent(0);
}

// Drives past the internal ones are not fitted, so they never reach track zero or get ready
void FDC::SetInternalDrives(int drives)
{
    m_internal_drives = CLAMP(drives, 0, FDC_DRIVES);
}

u8 FDC::Read(u16 port, u64 clocks)
{
    Synchronize(clocks);

    switch (port)
    {
        case 0x0200:
            return m_mb8877.ReadStatus(clocks);
        case 0x0202:
            return m_mb8877.ReadTrackRegister();
        case 0x0204:
            return m_mb8877.ReadSectorRegister();
        case 0x0206:
            return m_mb8877.ReadData();
        case 0x0208:
            // Bit 0 always reads one and the drive type reads 3.5 inch
            return IsReady(clocks) ? 0x07 : 0x05;
        case 0x020E:
            return m_state.drive_switch;
        default:
            return 0xFF;
    }
}

void FDC::Write(u16 port, u8 value, u64 clocks)
{
    Synchronize(clocks);

    switch (port)
    {
        case 0x0200:
            m_mb8877.WriteCommand(value, clocks);
            break;
        case 0x0202:
            m_mb8877.WriteTrackRegister(value, clocks);
            break;
        case 0x0204:
            m_mb8877.WriteSectorRegister(value, clocks);
            break;
        case 0x0206:
            m_mb8877.WriteData(value);
            break;
        case 0x0208:
            m_state.drive_control = value;
            UpdateIRQ();
            UpdateReady(clocks);
            break;
        case 0x020C:
        {
            int previous = GetSelectedDrive();

            // Speed and in-use are latched when a drive gets selected
            if ((m_state.drive_select & 0x0F) == 0 && (value & 0x0F) != 0)
            {
                m_state.high_speed = (value & 0x40) != 0;
                m_state.mode_b = (value & 0x80) != 0;
                m_state.in_use = (value & 0x10) != 0;
            }

            m_state.drive_select = value;
            ChangeSelection(previous, clocks);
            break;
        }
        case 0x020E:
        {
            int previous = GetSelectedDrive();
            m_state.drive_switch = value & 0x01;
            ChangeSelection(previous, clocks);
            break;
        }
    }

    UpdateNextEvent(clocks);
}

void FDC::HandleEvent(u64 clocks)
{
    Synchronize(clocks);
    UpdateNextEvent(clocks);
}

bool FDC::InsertDisk(int drive, const u8* data, u32 size, bool write_protected, u32 base_crc)
{
    if (drive < 0 || drive >= FDC_DRIVES || !m_disks[drive].Insert(data, size, write_protected, base_crc))
        return false;

    ChangeDisk(drive);
    return true;
}

void FDC::EjectDisk(int drive)
{
    if (drive < 0 || drive >= FDC_DRIVES)
        return;

    m_disks[drive].Eject();
    ChangeDisk(drive);
}

void FDC::SwapDisks()
{
    m_disks[0].Swap(m_disks[1]);
    ChangeDisk(0);
    ChangeDisk(1);
}

void FDC::SetINTRQ(bool active)
{
    m_state.intrq = active;
    UpdateIRQ();
}

void FDC::SetDRQ(bool active)
{
    m_dma->SetRequest(k_fdc_dma_channel, active);
}

// A disk change aborts a transfer on that drive and keeps it not ready for a moment
void FDC::ChangeDisk(int drive)
{
    u64 clocks = m_scheduler->GetClocks();
    m_state.ready_clocks[drive] = clocks + k_fdc_disk_change_clocks;

    if (drive == GetSelectedDrive())
        m_mb8877.MediaChanged();

    UpdateReady(clocks);
    UpdateNextEvent(clocks);
}

void FDC::ChangeSelection(int previous, u64 clocks)
{
    if (GetSelectedDrive() != previous)
        m_mb8877.MediaChanged();

    UpdateReady(clocks);
}

void FDC::UpdateIRQ()
{
    m_pic->SetIRQLine(k_fdc_irq, m_state.intrq && (m_state.drive_control & k_fdc_irq_enable) != 0);
}

void FDC::UpdateReady(u64 clocks)
{
    bool ready = IsReady(clocks);

    if (ready == m_state.ready)
        return;

    m_state.ready = ready;
    m_mb8877.ReadyChanged(ready);
}

// Besides the controller events, the end of a disk change wakes the board to report the drive ready
void FDC::UpdateNextEvent(u64 clocks)
{
    u64 next = m_mb8877.GetEventClocks();
    int drive = GetSelectedDrive();

    if (drive >= 0 && !m_state.ready && IsSpinning() && m_state.ready_clocks[drive] > clocks)
        next = MIN(next, m_state.ready_clocks[drive]);

    m_scheduler->Schedule(SCHEDULER_EVENT_FDC, next);
}

// The controller is 8 bit, a word unit only carries the low byte
bool FDC::DMAReadCallback(void* device, u16& value, bool word)
{
    UNUSED(word);
    FDC* fdc = (FDC*)device;
    u8 data = 0;

    if (!fdc->m_mb8877.ReadDMA(data))
        return false;

    value = data;
    return true;
}

bool FDC::DMAWriteCallback(void* device, u16 value, bool word)
{
    UNUSED(word);
    FDC* fdc = (FDC*)device;
    return fdc->m_mb8877.WriteDMA((u8)value);
}

void FDC::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    m_mb8877.SaveState(stream);

    for (int i = 0; i < FDC_DRIVES; i++)
        m_disks[i].SaveState(stream);
}

void FDC::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    m_mb8877.LoadState(stream);

    for (int i = 0; i < FDC_DRIVES; i++)
        m_disks[i].LoadState(stream);

    SanitizeState();
}

void FDC::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_state.drive_control);
    G_SERIALIZE(serializer, m_state.drive_select);
    G_SERIALIZE(serializer, m_state.drive_switch);
    G_SERIALIZE(serializer, m_state.high_speed);
    G_SERIALIZE(serializer, m_state.mode_b);
    G_SERIALIZE(serializer, m_state.in_use);
    G_SERIALIZE(serializer, m_state.ready);
    G_SERIALIZE(serializer, m_state.intrq);
    G_SERIALIZE_ARRAY(serializer, m_state.cylinders, FDC_DRIVES);
    G_SERIALIZE_ARRAY(serializer, m_state.ready_clocks, FDC_DRIVES);
}

void FDC::SanitizeState()
{
    for (int i = 0; i < FDC_DRIVES; i++)
        m_state.cylinders[i] = (u8)MIN(m_state.cylinders[i], k_fdc_last_cylinder);

    m_state.drive_switch &= 0x01;
    UpdateIRQ();
    UpdateNextEvent(m_scheduler->GetClocks());
}
