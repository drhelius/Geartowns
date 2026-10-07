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

#ifndef CDROM_COMMON_H
#define CDROM_COMMON_H

#include "../common/common.h"

static const u32 k_cdrom_track_type_size[3] = { 2352, 2048, 2352};
static const char* const k_cdrom_track_type_name[3] = { "AUDIO", "MODE1/2048", "MODE1/2352" };

static const u8 k_cdrom_control_pre_emphasis = 0x01;
static const u8 k_cdrom_control_copy_permitted = 0x02;
static const u8 k_cdrom_control_data = 0x04;
static const u8 k_cdrom_control_four_channels = 0x08;

enum GT_CdRomTrackType
{
    GT_CDROM_AUDIO_TRACK,
    GT_CDROM_DATA_TRACK_MODE1_2048,
    GT_CDROM_DATA_TRACK_MODE1_2352
};

INLINE u32 TrackTypeSectorSize(GT_CdRomTrackType type)
{
    return k_cdrom_track_type_size[type];
}

INLINE const char* TrackTypeName(GT_CdRomTrackType type)
{
    return k_cdrom_track_type_name[type];
}

struct GT_CdRomMSF
{
    u8 minutes;
    u8 seconds;
    u8 frames;
};

INLINE void LbaToMsf(u32 lba, GT_CdRomMSF* msf)
{
    msf->minutes = (u8)(lba / 75 / 60);
    msf->seconds = (u8)(lba / 75 % 60);
    msf->frames = (u8)(lba % 75);
}

INLINE u32 MsfToLba(GT_CdRomMSF* msf)
{
    return (msf->minutes * 60 + msf->seconds) * 75 + msf->frames;
}

INLINE u8 DecToBcd(u8 val)
{
    return ((val / 10) << 4) | (val % 10);
}

INLINE u8 BcdToDec(u8 bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

INLINE bool IsValidBcd(u8 bcd)
{
    return ((bcd >> 4) <= 9) && ((bcd & 0x0F) <= 9);
}

#endif /* CDROM_COMMON_H */