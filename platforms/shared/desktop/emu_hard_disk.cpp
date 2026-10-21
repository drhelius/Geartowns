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

#define EMU_HARD_DISK_IMPORT
#include "emu_hard_disk.h"

#include <string>
#include <SDL3/SDL.h>
#include "emu.h"
#include "config.h"
#include "utils.h"
#include "drive/hard_disk_image.h"

static const int k_idle_flush_frames = 120;
static u32 write_count = 0;
static int idle_frames = 0;
static bool flush_failed = false;
static bool flush_error = false;

static bool valid_id(int id);
static void set_config(int id, const char* path, bool write_protected);

void emu_hard_disk_init(void)
{
    for (int i = 0; i < GT_MAX_HARD_DISKS; i++)
    {
        std::string path = config_emulator.hard_disk_paths[i];

        if (path.empty())
            continue;

        if (!emu_get_core()->AttachHardDisk(i, path.c_str(), config_emulator.hard_disk_write_protected[i] != 0))
        {
            Error("Unable to attach hard disk %d: %s", i, path.c_str());
            set_config(i, "", false);
        }
    }
}

bool emu_hard_disk_is_image(const char* path)
{
    if (!IsValidPointer(path))
        return false;

    if (ends_with_no_case(path, ".hdd"))
        return true;

    size_t length = strlen(path);
    return (length > 3) && (path[length - 3] == '.') && ((path[length - 2] | 0x20) == 'h') &&
        (path[length - 1] >= '0') && (path[length - 1] < '0' + GT_MAX_HARD_DISKS);
}

int emu_hard_disk_get_image_id(const char* path)
{
    if (!emu_hard_disk_is_image(path))
        return -1;

    if (!ends_with_no_case(path, ".hdd"))
        return path[strlen(path) - 1] - '0';

    for (int i = 0; i < GT_MAX_HARD_DISKS; i++)
    {
        Emu_HardDiskInfo info;
        emu_hard_disk_get_info(i, &info);

        if (info.pending ? !info.pending_attached : !info.attached)
            return i;
    }

    return -1;
}

bool emu_hard_disk_attach(int id, const char* path, bool write_protected)
{
    if (!valid_id(id) || !IsValidPointer(path) || !emu_get_core()->AttachHardDisk(id, path, write_protected))
        return false;

    set_config(id, path, write_protected);
    return true;
}

bool emu_hard_disk_detach(int id)
{
    if (!valid_id(id))
        return false;

    emu_get_core()->DetachHardDisk(id);
    set_config(id, "", false);
    return true;
}

bool emu_hard_disk_create(int id, const char* path, int size_mb, bool write_protected)
{
    if (!valid_id(id) || !IsValidPointer(path) || (size_mb <= 0) || SDL_GetPathInfo(path, NULL))
        return false;

    if (!HardDiskImage::Create(path, (u64)size_mb * 1024 * 1024))
        return false;

    return emu_hard_disk_attach(id, path, write_protected);
}

bool emu_hard_disk_set_write_protected(int id, bool write_protected)
{
    Emu_HardDiskInfo info;

    if (!emu_hard_disk_get_info(id, &info))
        return false;

    const char* path = info.pending ? info.pending_path : info.path;

    if (path[0] == 0)
        return false;

    return emu_hard_disk_attach(id, path, write_protected);
}

bool emu_hard_disk_flush(void)
{
    bool flushed = emu_get_core()->FlushHardDisks();

    if (!flushed && !flush_failed)
        flush_error = true;

    flush_failed = !flushed;
    idle_frames = 0;
    return flushed;
}

void emu_hard_disk_update(void)
{
    GeartownsCore* core = emu_get_core();
    u32 count = 0;
    u32 unflushed = 0;

    for (int i = 0; i < GT_MAX_HARD_DISKS; i++)
    {
        HardDiskImage* image = core->GetHardDisk(i);
        count += image->GetWriteCount();
        unflushed += image->GetUnflushedBlocks();
    }

    if (count != write_count)
    {
        write_count = count;
        idle_frames = 0;
        return;
    }

    if ((unflushed != 0) && (++idle_frames >= k_idle_flush_frames))
        emu_hard_disk_flush();
}

bool emu_hard_disk_take_flush_error(void)
{
    bool error = flush_error;
    flush_error = false;
    return error;
}

bool emu_hard_disk_get_info(int id, Emu_HardDiskInfo* info)
{
    if (!IsValidPointer(info))
        return false;

    memset(info, 0, sizeof(*info));

    if (!valid_id(id))
        return false;

    GeartownsCore* core = emu_get_core();
    HardDiskImage* image = core->GetHardDisk(id);
    info->attached = image->IsOpen();
    info->write_protected = image->IsWriteProtected();
    info->blocks = image->GetBlocks();
    info->unflushed = image->GetUnflushedBlocks();
    info->pending = core->IsHardDiskPending(id);
    info->pending_attached = info->pending ? core->GetPendingHardDiskPath(id)[0] != 0 : info->attached;
    info->pending_write_protected = info->pending ? core->IsPendingHardDiskWriteProtected(id) : info->write_protected;
    strncpy_fit(info->path, image->GetPath(), sizeof(info->path));
    strncpy_fit(info->pending_path, core->GetPendingHardDiskPath(id), sizeof(info->pending_path));
    return true;
}

const char* emu_hard_disk_get_content_path(void)
{
    for (int i = 0; i < GT_MAX_HARD_DISKS; i++)
    {
        HardDiskImage* image = emu_get_core()->GetHardDisk(i);

        if (image->IsOpen())
            return image->GetPath();
    }

    return "";
}

static bool valid_id(int id)
{
    return (id >= 0) && (id < GT_MAX_HARD_DISKS);
}

static void set_config(int id, const char* path, bool write_protected)
{
    config_emulator.hard_disk_paths[id] = path;
    config_emulator.hard_disk_write_protected[id] = write_protected ? 1 : 0;
}
