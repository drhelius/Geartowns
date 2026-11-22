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

#if defined(GT_ENABLE_PHYSICAL_CDROM) && defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <DiscRecording/DiscRecording.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IOMedia.h>
#include <errno.h>
#include <sys/ioctl.h>

#define CDROM_DRIVE_RAW_SECTOR_SIZE 2352

static bool read_cd_ioctl(int file, u32 lba, u32 sector_count, u8 sector_area, u8 sector_type, u8* buffer, bool* media_unavailable)
{
    if (IsValidPointer(media_unavailable))
        *media_unavailable = false;

    dk_cd_read_t request = {};
    request.offset = (uint64_t)lba * CDROM_DRIVE_RAW_SECTOR_SIZE;
    request.sectorArea = sector_area;
    request.sectorType = sector_type;
    request.bufferLength = sector_count * CDROM_DRIVE_RAW_SECTOR_SIZE;
    request.buffer = buffer;

    if (ioctl(file, DKIOCCDREAD, &request) < 0)
    {
        if (IsValidPointer(media_unavailable))
            *media_unavailable = cdrom_errno_is_media_unavailable(errno);
        return false;
    }

    return request.bufferLength == (sector_count * CDROM_DRIVE_RAW_SECTOR_SIZE);
}

static void get_bsd_name_from_device_id(const char* device_id, char* bsd_name, size_t bsd_name_size)
{
    const char* name = strrchr(device_id, '/');
    name = IsValidPointer(name) ? name + 1 : device_id;

    if ((name[0] == 'r') && (strncmp(name + 1, "disk", 4) == 0))
        name++;

    strncpy_fit(bsd_name, name, bsd_name_size);
}

bool CdRomDrive::ListDrives(std::vector<CdRomDriveInfo>& drives)
{
    drives.clear();
    Debug("macOS physical CD-ROM enumeration started");

    CFMutableDictionaryRef matching = IOServiceMatching(kIOCDMediaClass);
    if (matching == NULL)
    {
        Error("IOServiceMatching failed for kIOCDMediaClass");
        return false;
    }

    CFDictionarySetValue(matching, CFSTR(kIOMediaEjectableKey), kCFBooleanTrue);

    io_iterator_t iterator = IO_OBJECT_NULL;
    kern_return_t result = IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator);
    if (result != KERN_SUCCESS)
    {
        Error("IOServiceGetMatchingServices failed for physical CD-ROM enumeration: 0x%08X", result);
        return false;
    }

    io_object_t media = IOIteratorNext(iterator);
    while (media != IO_OBJECT_NULL)
    {
        CFTypeRef bsd_name_ref = IORegistryEntryCreateCFProperty(media, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
        if (bsd_name_ref != NULL)
        {
            char bsd_name[256] = {};
            if (CFGetTypeID(bsd_name_ref) == CFStringGetTypeID())
            {
                if (CFStringGetCString((CFStringRef)bsd_name_ref, bsd_name, sizeof(bsd_name), kCFStringEncodingUTF8))
                {
                    char id[256];
                    snprintf(id, sizeof(id), "/dev/r%s", bsd_name);
                    CdRomDrive::Probe(id, bsd_name, drives);
                }
            }

            CFRelease(bsd_name_ref);
        }

        IOObjectRelease(media);
        media = IOIteratorNext(iterator);
    }

    IOObjectRelease(iterator);

    Debug("macOS physical CD-ROM enumeration finished: %d drive(s)", (int)drives.size());

    return true;
}

bool CdRomDrive::Eject(const char* device_id)
{
    if (!CheckDeviceId(device_id, true))
        return false;

    char bsd_name[256] = {};
    get_bsd_name_from_device_id(device_id, bsd_name, sizeof(bsd_name));

    Debug("Ejecting physical CD-ROM device %s (BSD name %s)", device_id, bsd_name);

    CFStringRef bsd_name_ref = CFStringCreateWithCString(kCFAllocatorDefault, bsd_name, kCFStringEncodingUTF8);
    if (bsd_name_ref == NULL)
    {
        Error("Failed to create BSD name string for physical CD-ROM eject: %s", bsd_name);
        return false;
    }

    DRDeviceRef device = DRDeviceCopyDeviceForBSDName(bsd_name_ref);
    CFRelease(bsd_name_ref);

    if (device == NULL)
    {
        Error("Failed to find DiscRecording device for physical CD-ROM eject: %s", bsd_name);
        return false;
    }

    if (!DRDeviceIsValid(device))
    {
        Error("DiscRecording device is invalid for physical CD-ROM eject: %s", bsd_name);
        CFRelease(device);
        return false;
    }

    OSStatus status = DRDeviceEjectMedia(device);
    CFRelease(device);

    if (status != noErr)
    {
        Error("Failed to eject physical CD-ROM device %s: OSStatus %d", device_id, (int)status);
        return false;
    }

    Log("Physical CD-ROM ejected: %s", device_id);
    return true;
}

bool CdRomDrive::ReadTOC(std::vector<CdRomDriveTrackInfo>& tracks, u32* lead_out_lba)
{
    if (!IsOpen() || !IsValidPointer(lead_out_lba))
    {
        Error("ReadTOC failed - invalid physical CD-ROM drive state");
        return false;
    }

    tracks.clear();
    *lead_out_lba = 0;

    u8 toc_buffer[4096] = {};
    dk_cd_read_toc_t request = {};
    request.format = kCDTOCFormatTOC;
    request.formatAsTime = 1;
    request.address.session = 0;
    request.bufferLength = sizeof(toc_buffer);
    request.buffer = toc_buffer;

    if (ioctl(m_file, DKIOCCDREADTOC, &request) < 0)
    {
        Error("DKIOCCDREADTOC failed for %s: %s", m_device_id, strerror(errno));
        return false;
    }

    CDTOC* toc = (CDTOC*)toc_buffer;
    u32 descriptor_count = CDTOCGetDescriptorCount(toc);
    Debug("Physical CD-ROM TOC descriptors for %s: %u", m_device_id, descriptor_count);

    for (u32 i = 0; i < descriptor_count; i++)
    {
        CDTOCDescriptor& descriptor = toc->descriptors[i];

        if ((descriptor.point >= 1) && (descriptor.point <= 99))
        {
            CdRomDriveTrackInfo track;
            track.number = descriptor.point;
            track.data = (descriptor.control & 0x04) != 0;
            track.control = descriptor.control;
            track.start_lba = CDConvertMSFToClippedLBA(descriptor.p);
            tracks.push_back(track);
            Debug("Physical CD-ROM TOC track %u: start_lba=%u type=%s", track.number, track.start_lba, track.data ? "data" : "audio");
        }
        else if (descriptor.point == 0xA2)
        {
            *lead_out_lba = CDConvertMSFToClippedLBA(descriptor.p);
            Debug("Physical CD-ROM TOC lead-out: lba=%u", *lead_out_lba);
        }
    }

    return ValidateToc(tracks, lead_out_lba);
}

bool CdRomDrive::ReadRawSectors2352(u32 lba, u32 sector_count, u8* buffer, bool audio, bool report_errors)
{
    if (!IsOpen() || !IsValidPointer(buffer) || (sector_count == 0))
        return false;

    u8 raw_area = kCDSectorAreaSync | kCDSectorAreaHeader | kCDSectorAreaSubHeader | kCDSectorAreaUser | kCDSectorAreaAuxiliary;
    bool media_unavailable = false;

    if (audio)
    {
        if (read_cd_ioctl(m_file, lba, sector_count, kCDSectorAreaUser, kCDSectorTypeCDDA, buffer, &media_unavailable))
            return true;
    }
    else
    {
        if (read_cd_ioctl(m_file, lba, sector_count, raw_area, kCDSectorTypeUnknown, buffer, &media_unavailable))
            return true;
    }

    if (media_unavailable)
    {
        if (report_errors)
            Error("Physical CD-ROM media unavailable for %s at LBA %u", m_device_id, lba);
        return false;
    }

    if (report_errors)
        Debug("Physical CD-ROM block read fallback for %s at LBA %u (%u sectors)", m_device_id, lba, sector_count);

    for (u32 i = 0; i < sector_count; i++)
    {
        u32 sector_lba = lba + i;
        u8* sector_buffer = buffer + (i * CDROM_DRIVE_RAW_SECTOR_SIZE);

        if (audio)
        {
            if (read_cd_ioctl(m_file, sector_lba, 1, kCDSectorAreaUser, kCDSectorTypeCDDA, sector_buffer, &media_unavailable))
                continue;

            if (media_unavailable)
            {
                if (report_errors)
                    Error("Physical CD-ROM media unavailable for %s at LBA %u", m_device_id, sector_lba);
                return false;
            }
        }

        if (!audio)
        {
            if (read_cd_ioctl(m_file, sector_lba, 1, raw_area, kCDSectorTypeMode1, sector_buffer, &media_unavailable))
                continue;

            if (media_unavailable)
            {
                if (report_errors)
                    Error("Physical CD-ROM media unavailable for %s at LBA %u", m_device_id, sector_lba);
                return false;
            }

            if (read_cd_ioctl(m_file, sector_lba, 1, raw_area, kCDSectorTypeUnknown, sector_buffer, &media_unavailable))
                continue;

            if (media_unavailable)
            {
                if (report_errors)
                    Error("Physical CD-ROM media unavailable for %s at LBA %u", m_device_id, sector_lba);
                return false;
            }
        }

        if (report_errors)
            Error("DKIOCCDREAD failed for %s at LBA %u", m_device_id, sector_lba);
        return false;
    }

    return true;
}

bool CdRomDrive::SetSpeed(u16 speed)
{
    if (!IsOpen())
        return false;

    bool ok = ioctl(m_file, DKIOCCDSETSPEED, &speed) == 0;
    Debug("Physical CD-ROM set speed %u for %s: %s", speed, m_device_id, ok ? "ok" : "failed");
    return ok;
}

#endif /* GT_ENABLE_PHYSICAL_CDROM && __APPLE__ */
