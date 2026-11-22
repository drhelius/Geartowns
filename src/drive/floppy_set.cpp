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

#include "floppy_set.h"
#include "../media/crc.h"
#include "../media/media_file.h"

bool FloppySet::LoadSource(const char* path, int image, std::vector<u8>& data, char* name, size_t name_size)
{
    if (!MediaFile::ReadFile(path, k_floppy_max_file_size, &data))
    {
        Error("Unable to read %s", path);
        return false;
    }

    if (!ends_with_no_case(path, ".zip"))
        return true;

    u8* extracted = NULL;
    u32 size = 0;

    if (!FloppyImage::ExtractFromZip(data.data(), data.size(), image, &extracted, &size, name, name_size))
    {
        Error("ZIP holds no floppy image: %s", path);
        return false;
    }

    data.assign(extracted, extracted + size);
    SafeDeleteArray(extracted);
    return true;
}

bool FloppySet::ListDisks(const char* path, std::vector<FloppySet_Disk>& disks)
{
    size_t first = disks.size();
    int images = 1;

    if (ends_with_no_case(path, ".zip"))
    {
        std::vector<u8> archive;
        images = MediaFile::ReadFile(path, k_floppy_max_file_size, &archive) ?
            FloppyImage::GetZipImageCount(archive.data(), archive.size()) : 0;
    }

    for (int image = 0; image < images; image++)
    {
        std::vector<u8> data;
        char member[GT_MAX_PATH];
        member[0] = 0;

        if (!LoadSource(path, image, data, member, sizeof(member)))
            return false;

        int count = FloppyImage::GetDiskCount(data.data(), (u32)data.size());

        for (int i = 0; i < count; i++)
        {
            u32 offset = 0;
            u32 size = 0;
            char name[64];

            if (!FloppyImage::GetDisk(data.data(), (u32)data.size(), i, &offset, &size))
                return false;

            FloppyImage::GetDiskName(data.data() + offset, size, name, sizeof(name));

            FloppySet_Disk disk;
            disk.image = image;
            disk.images = images;
            disk.disk = i;
            disk.count = count;
            disk.crc = CalculateCRC32(0, data.data() + offset, size);
            disk.member = member;
            disk.name = name;
            disks.push_back(disk);
        }
    }

    return disks.size() > first;
}

std::string FloppySet::GetDiskLabel(const FloppySet_Disk& disk)
{
    if (disk.count <= 1)
        return "";

    char label[96];
    snprintf(label, sizeof(label), disk.name.empty() ? " (Disk %d)" : " (Disk %d: %s)", disk.disk + 1, disk.name.c_str());
    return label;
}

bool FloppySet::IsBootDisk(const char* path, int image, int disk)
{
    std::vector<u8> data;
    return LoadSource(path, image, data) && FloppyImage::IsBootDisk(data.data(), (u32)data.size(), disk);
}

bool FloppySet::ReadDisk(const char* path, int image, int disk, std::vector<u8>& file, u32* offset, u32* size)
{
    return LoadSource(path, image, file) && FloppyImage::GetDisk(file.data(), (u32)file.size(), disk, offset, size);
}

bool FloppySet::ReadWorkingCopy(const char* path, std::vector<u8>& copy)
{
    return MediaFile::ReadFile(path, k_floppy_max_file_size, &copy) && FloppyImage::IsD77(copy.data(), (u32)copy.size());
}

bool FloppySet::IsFloppyFile(const char* path)
{
    std::vector<u8> data;

    if (!IsValidPointer(path))
        return false;

    if (FloppyImage::IsImageName(path))
        return true;

    if (ends_with_no_case(path, ".bin"))
        return MediaFile::ReadFile(path, k_floppy_max_file_size, &data) && FloppyImage::IsRaw(data.data(), (u32)data.size());

    if (ends_with_no_case(path, ".zip"))
        return MediaFile::ReadFile(path, k_floppy_max_file_size, &data) &&
            (FloppyImage::GetZipImageCount(data.data(), data.size()) > 0);

    return false;
}
