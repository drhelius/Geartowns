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

#include "cdrom_drive.h"
#include <algorithm>

#if defined(GT_ENABLE_PHYSICAL_CDROM)

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#if defined(__linux__)
#include <linux/cdrom.h>
#include <sys/ioctl.h>
#endif

#if !defined(_WIN32)
#define CDROM_DRIVE_DATA_SECTOR_SIZE 2048
#define CDROM_DRIVE_DATA_SECTOR_OFFSET 16
#define CDROM_DRIVE_RAW_SECTOR_SIZE 2352
#endif

static bool drive_track_compare(const CdRomDriveTrackInfo& a, const CdRomDriveTrackInfo& b)
{
    return a.start_lba < b.start_lba;
}

CdRomDrive::CdRomDrive()
{
#if defined(_WIN32)
    m_file = INVALID_HANDLE_VALUE;
#else
    m_file = -1;
#endif
    m_device_id[0] = 0;
}

CdRomDrive::~CdRomDrive()
{
    Close();
}

bool CdRomDrive::IsOpen() const
{
#if defined(_WIN32)
    return m_file != INVALID_HANDLE_VALUE;
#else
    return m_file >= 0;
#endif
}

void CdRomDrive::Probe(const char* device_id, const char* name, std::vector<CdRomDriveInfo>& drives)
{
    CdRomDriveInfo info;
    strncpy_fit(info.id, device_id, sizeof(info.id));
    strncpy_fit(info.name, name, sizeof(info.name));
    info.has_disc = false;

    CdRomDrive drive;
    std::vector<CdRomDriveTrackInfo> tracks;
    u32 lead_out = 0;
    if (drive.Open(info.id))
        info.has_disc = drive.ReadTOC(tracks, &lead_out);

    Debug("Physical CD-ROM candidate: id=%s name=%s has_disc=%s", info.id, info.name, info.has_disc ? "true" : "false");
    drives.push_back(info);
}

bool CdRomDrive::CheckDeviceId(const char* device_id, bool eject)
{
    if (IsValidPointer(device_id) && (device_id[0] != 0))
        return true;

    if (eject)
        Error("Invalid physical CD-ROM device id for eject");
    else
        Error("Invalid physical CD-ROM device id");

    return false;
}

bool CdRomDrive::ValidateToc(std::vector<CdRomDriveTrackInfo>& tracks, u32* lead_out_lba)
{
    std::sort(tracks.begin(), tracks.end(), drive_track_compare);

    if (tracks.empty() || (*lead_out_lba == 0))
    {
        Error("Invalid physical CD-ROM TOC for %s", m_device_id);
        tracks.clear();
        *lead_out_lba = 0;
        return false;
    }

    Debug("Physical CD-ROM TOC read complete for %s: tracks=%d lead_out=%u", m_device_id, (int)tracks.size(), *lead_out_lba);
    return true;
}

bool CdRomDrive::ReadRawSector2352(u32 lba, u8* buffer, bool audio, bool report_errors)
{
    return ReadRawSectors2352(lba, 1, buffer, audio, report_errors);
}

#if !defined(_WIN32)
bool cdrom_errno_is_media_unavailable(int error)
{
    if ((error == ENODEV) || (error == ENXIO))
        return true;

#if defined(ENOMEDIUM)
    if (error == ENOMEDIUM)
        return true;
#endif

    return false;
}

bool CdRomDrive::Open(const char* device_id)
{
    if (!CheckDeviceId(device_id, false))
        return false;

    Close();

    Debug("Opening physical CD-ROM device %s", device_id);
    m_file = open(device_id, O_RDONLY | O_NONBLOCK);
    if (m_file < 0)
    {
        Error("Failed to open physical CD-ROM device %s: %s", device_id, strerror(errno));
        return false;
    }

#if defined(__linux__)
    if (ioctl(m_file, CDROM_GET_CAPABILITY, 0) < 0)
    {
        Error("Physical CD-ROM capability query failed for %s: %s", device_id, strerror(errno));
        Close();
        return false;
    }
#endif

    strncpy_fit(m_device_id, device_id, sizeof(m_device_id));
    Debug("Physical CD-ROM device opened: %s", m_device_id);
    return true;
}

void CdRomDrive::Close()
{
    if (m_file >= 0)
    {
        close(m_file);
        m_file = -1;
    }

    m_device_id[0] = 0;
}

bool CdRomDrive::ReadDataSector2048(u32 lba, u8* buffer, bool report_errors)
{
    if (!IsValidPointer(buffer))
        return false;

    u8 raw[CDROM_DRIVE_RAW_SECTOR_SIZE];
    if (!ReadRawSector2352(lba, raw, false, report_errors))
        return false;

    memcpy(buffer, raw + CDROM_DRIVE_DATA_SECTOR_OFFSET, CDROM_DRIVE_DATA_SECTOR_SIZE);
    return true;
}
#endif

const char* CdRomDrive::GetDeviceId() const
{
    return m_device_id;
}

#endif /* GT_ENABLE_PHYSICAL_CDROM */
