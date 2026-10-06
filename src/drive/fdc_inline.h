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

#ifndef FDC_INLINE_H
#define FDC_INLINE_H

#include "fdc.h"
#include "floppy_image.h"

INLINE void FDC::Synchronize(u64 clocks)
{
    if (m_mb8877.GetEventClocks() <= clocks)
        m_mb8877.Run(clocks);

    UpdateReady(clocks);
}

INLINE FloppyDisk* FDC::GetDisk(int drive)
{
    return (drive >= 0 && drive < FDC_DRIVES) ? &m_disks[drive] : NULL;
}

INLINE MB8877* FDC::GetMB8877()
{
    return &m_mb8877;
}

INLINE FDC::FDC_State* FDC::GetState()
{
    return &m_state;
}

// DSL0-3 select a logical drive and the switch swaps the internal and external pairs
INLINE int FDC::GetSelectedDrive() const
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

INLINE FloppyDisk* FDC::GetSelectedDisk()
{
    int drive = GetSelectedDrive();
    return (drive >= 0 && m_disks[drive].IsInserted()) ? &m_disks[drive] : NULL;
}

// 2D disks have half the tracks, the drive reaches them stepping twice
INLINE int FDC::GetSelectedTrack() const
{
    int drive = GetSelectedDrive();

    if (drive < 0 || !m_disks[drive].IsInserted())
        return -1;

    int cylinder = m_state.cylinders[drive];
    int side = (m_state.drive_control & k_fdc_side) != 0 ? 1 : 0;

    if (m_disks[drive].GetMedia() == FLOPPY_MEDIA_2D)
    {
        if ((cylinder & 1) != 0)
            return -1;

        cylinder >>= 1;
    }

    int track = cylinder * 2 + side;
    return track < k_floppy_tracks ? track : -1;
}

// The drive mode, data rate and rotation must match the ones the disk was written with
// 2HD media needs HISPD, so a two-mode drive never reads a 1.44 MB disk
INLINE bool FDC::IsSelectedReadable() const
{
    int drive = GetSelectedDrive();

    if (drive < 0 || !m_disks[drive].IsInserted())
        return false;

    bool slow = (m_state.drive_control & k_fdc_slow_clock) != 0;

    if (m_disks[drive].GetMedia() == FLOPPY_MEDIA_2HD)
        return !slow && m_state.high_speed && GetRPM() == m_disks[drive].GetRPM();

    return slow && !m_state.high_speed && GetRPM() == m_disks[drive].GetRPM();
}

// 2HD media takes either 2HD mode, 2DD media only the 2DD one
INLINE bool FDC::IsSelectedFormattable() const
{
    int drive = GetSelectedDrive();

    if (drive < 0 || !m_disks[drive].IsInserted())
        return false;

    bool slow = (m_state.drive_control & k_fdc_slow_clock) != 0;

    if (m_disks[drive].GetMedia() == FLOPPY_MEDIA_2HD)
        return !slow && m_state.high_speed;

    return slow && !m_state.high_speed && !m_state.mode_b;
}

INLINE bool FDC::IsSpinning() const
{
    int drive = GetSelectedDrive();
    return drive >= 0 && m_disks[drive].IsInserted() && (m_state.drive_control & k_fdc_motor) != 0;
}

INLINE bool FDC::IsReady(u64 clocks) const
{
    return IsSpinning() && clocks >= m_state.ready_clocks[GetSelectedDrive()];
}

// The index hole passes the sensor during the first 1% of each turn
INLINE bool FDC::IsIndex(u64 clocks) const
{
    return IsSpinning() && ((clocks * GetRPM()) % k_mb8877_turn) < k_mb8877_turn / 100;
}

INLINE bool FDC::IsTrackZero() const
{
    int drive = GetSelectedDrive();
    return drive >= 0 && m_state.cylinders[drive] == 0;
}

INLINE bool FDC::IsWriteProtected() const
{
    int drive = GetSelectedDrive();
    return drive >= 0 && (!m_disks[drive].IsInserted() || m_disks[drive].IsWriteProtected());
}

INLINE bool FDC::IsDoubleDensity() const
{
    return (m_state.drive_control & k_fdc_double_density) != 0;
}

// CLKSEL runs the controller at 2 MHz for 2HD or 1 MHz for 2DD, an MFM byte takes 32 controller cycles
INLINE u32 FDC::GetCycleClocks() const
{
    return (m_state.drive_control & k_fdc_slow_clock) != 0 ? GT_CPU_CLOCK_RATE / 1000000 :
        GT_CPU_CLOCK_RATE / 2000000;
}

INLINE u32 FDC::GetByteClocks() const
{
    return GetCycleClocks() * (IsDoubleDensity() ? 32 : 64);
}

// MODE-B with HISPD is the 1.44 MB mode, without it the unsupported 2ED one
INLINE u32 FDC::GetRPM() const
{
    if (m_state.mode_b)
        return m_state.high_speed ? 300 : 180;

    return m_state.high_speed ? 360 : 300;
}

INLINE void FDC::Step(bool in)
{
    int drive = GetSelectedDrive();

    if (drive < 0)
        return;

    int cylinder = m_state.cylinders[drive] + (in ? 1 : -1);
    m_state.cylinders[drive] = (u8)CLAMP(cylinder, 0, (int)k_fdc_last_cylinder);
}

#endif /* FDC_INLINE_H */
