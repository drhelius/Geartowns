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

#include "fdc_mock.h"
#include "../system/pic.h"
#include "../system/scheduler.h"
#include "../common/state_serializer.h"

// Step rates for r1:r0 with the 1 MHz clock, the 2 MHz clock halves them
static const u8 k_fdc_mock_step_ms[4] = { 6, 12, 20, 30 };

FDCMock::FDCMock()
{
    InitPointer(m_pic);
    InitPointer(m_scheduler);
    memset(&m_state, 0, sizeof(m_state));
    m_internal_drives = 2;
}

FDCMock::~FDCMock()
{
}

void FDCMock::Init(PIC* pic, Scheduler* scheduler)
{
    m_pic = pic;
    m_scheduler = scheduler;
    Reset();
}

void FDCMock::Reset()
{
    memset(&m_state, 0, sizeof(m_state));
    m_state.type1 = true;
    UpdateIRQ();
    UpdateNextEvent();
}

// Drives past the internal ones never reach track zero, as on a board without them
void FDCMock::SetInternalDrives(int drives)
{
    m_internal_drives = CLAMP(drives, 0, 2);
}

u8 FDCMock::Read(u16 port, u64 clocks)
{
    Synchronize(clocks);

    switch (port)
    {
        case 0x0200:
        {
            u8 status = GetStatus();
            m_state.intrq = false;
            UpdateIRQ();
            return status;
        }
        case 0x0202:
            return m_state.track;
        case 0x0204:
            return m_state.sector;
        case 0x0206:
            return m_state.data;
        case 0x0208:
            // Bit 0 always reads one, READY stays clear and the drive type reads 3.5 inch
            return 0x05;
        case 0x020E:
            return m_state.drive_switch;
        default:
            return 0xFF;
    }
}

void FDCMock::Write(u16 port, u8 value, u64 clocks)
{
    Synchronize(clocks);

    switch (port)
    {
        case 0x0200:
            WriteCommand(value, clocks);
            break;
        case 0x0202:
            m_state.track = value;
            break;
        case 0x0204:
            m_state.sector = value;
            break;
        case 0x0206:
            m_state.data = value;
            break;
        case 0x0208:
            m_state.drive_control = value;
            UpdateIRQ();
            break;
        case 0x020C:
            m_state.drive_select = value;
            break;
        case 0x020E:
            m_state.drive_switch = value & 0x01;
            break;
    }
}

// The chip ignores commands while busy, except force interrupt
void FDCMock::WriteCommand(u8 value, u64 clocks)
{
    bool force_interrupt = (value & 0xF0) == 0xD0;

    if (m_state.busy && !force_interrupt)
        return;

    m_state.intrq = false;

    // Only the immediate form interrupts, the ready and index conditions never happen without a disk
    if (force_interrupt)
    {
        if (!m_state.busy)
        {
            m_state.type1 = true;
            m_state.seek_error = false;
        }

        m_state.command = value;
        m_state.busy = false;
        m_state.intrq = (value & 0x08) != 0;
        UpdateIRQ();
        UpdateNextEvent();
        return;
    }

    m_state.command = value;
    m_state.type1 = (value & 0x80) == 0;
    m_state.seek_error = false;

    // Read and write commands end at once because the drive is never ready
    if (!m_state.type1)
    {
        m_state.intrq = true;
        UpdateIRQ();
        return;
    }

    m_state.busy = true;
    m_state.execute_clocks = clocks + GetCommandClocks();
    UpdateIRQ();
    UpdateNextEvent();
}

void FDCMock::CompleteCommand()
{
    int drive = GetSelectedDrive();

    switch (m_state.command >> 4)
    {
        case 0x00:
            // Without TR00 the restore gives up after 255 steps
            if (drive >= 0)
            {
                m_state.cylinders[drive] = 0;
                m_state.track = 0;
            }
            else
                m_state.seek_error = true;
            break;
        case 0x01:
            if (m_state.data != m_state.track)
                m_state.step_in = m_state.data > m_state.track;

            MoveHead(drive, (int)m_state.data - (int)m_state.track);
            m_state.track = m_state.data;
            break;
        default:
            if ((m_state.command & 0x60) == 0x40)
                m_state.step_in = true;
            else if ((m_state.command & 0x60) == 0x60)
                m_state.step_in = false;

            MoveHead(drive, m_state.step_in ? 1 : -1);

            if ((m_state.command & 0x10) != 0)
                m_state.track = (u8)(m_state.track + (m_state.step_in ? 1 : -1));
            break;
    }

    // Verifying finds no ID field without a disk
    if ((m_state.command & 0x04) != 0)
        m_state.seek_error = true;

    m_state.busy = false;
    m_state.intrq = true;
    UpdateIRQ();
    UpdateNextEvent();
}

void FDCMock::MoveHead(int drive, int steps)
{
    if (drive < 0)
        return;

    int cylinder = m_state.cylinders[drive] + steps;
    m_state.cylinders[drive] = (u8)CLAMP(cylinder, 0, k_fdc_mock_last_cylinder);
}

// DSL0-3 select a logical drive and the switch swaps the internal and external pairs
int FDCMock::GetSelectedDrive() const
{
    for (int i = 0; i < 4; i++)
    {
        if ((m_state.drive_select & (1 << i)) == 0)
            continue;

        int drive = (m_state.drive_switch & 0x01) != 0 ? i ^ 0x02 : i;
        return drive < m_internal_drives ? drive : -1;
    }

    return -1;
}

// Type I commands step once per cylinder at the programmed rate, verifying adds the head settle time
u64 FDCMock::GetCommandClocks() const
{
    int drive = GetSelectedDrive();
    int steps = 1;

    switch (m_state.command >> 4)
    {
        case 0x00:
            steps = drive >= 0 ? m_state.cylinders[drive] : k_fdc_mock_restore_steps;
            break;
        case 0x01:
            steps = m_state.data > m_state.track ? m_state.data - m_state.track : m_state.track - m_state.data;
            break;
    }

    u64 step_clocks = ((u64)k_fdc_mock_step_ms[m_state.command & 0x03] * GT_CPU_CLOCK_RATE) / 1000;

    if ((m_state.drive_control & 0x20) == 0)
        step_clocks /= 2;

    u64 clocks = k_fdc_mock_command_delay + steps * step_clocks;

    if ((m_state.command & 0x04) != 0)
        clocks += k_fdc_mock_settle_delay;

    return clocks;
}

// No drive ever holds a disk, so NOT READY stays set
u8 FDCMock::GetStatus() const
{
    u8 status = 0x80;

    if (m_state.type1)
    {
        int drive = GetSelectedDrive();

        if (m_state.seek_error)
            status |= 0x10;

        if (drive >= 0 && m_state.cylinders[drive] == 0)
            status |= 0x04;
    }

    if (m_state.busy)
        status |= 0x01;

    return status;
}

void FDCMock::UpdateIRQ()
{
    m_pic->SetIRQLine(k_fdc_mock_irq, m_state.intrq && (m_state.drive_control & 0x01) != 0);
}

void FDCMock::UpdateNextEvent()
{
    m_scheduler->Schedule(SCHEDULER_EVENT_FDC, m_state.busy ? m_state.execute_clocks : GT_NO_EVENT);
}

void FDCMock::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void FDCMock::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void FDCMock::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_state.command);
    G_SERIALIZE(serializer, m_state.track);
    G_SERIALIZE(serializer, m_state.sector);
    G_SERIALIZE(serializer, m_state.data);
    G_SERIALIZE(serializer, m_state.busy);
    G_SERIALIZE(serializer, m_state.type1);
    G_SERIALIZE(serializer, m_state.seek_error);
    G_SERIALIZE(serializer, m_state.intrq);
    G_SERIALIZE(serializer, m_state.step_in);
    G_SERIALIZE(serializer, m_state.execute_clocks);
    G_SERIALIZE(serializer, m_state.drive_control);
    G_SERIALIZE(serializer, m_state.drive_select);
    G_SERIALIZE(serializer, m_state.drive_switch);
    G_SERIALIZE_ARRAY(serializer, m_state.cylinders, FDC_MOCK_DRIVES);
}

void FDCMock::SanitizeState()
{
    for (int i = 0; i < FDC_MOCK_DRIVES; i++)
        m_state.cylinders[i] = (u8)MIN(m_state.cylinders[i], k_fdc_mock_last_cylinder);

    m_state.drive_switch &= 0x01;
    UpdateIRQ();
    UpdateNextEvent();
}
