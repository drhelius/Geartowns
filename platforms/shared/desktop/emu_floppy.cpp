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

#define EMU_FLOPPY_IMPORT
#include "emu_floppy.h"

#include <fstream>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include "emu.h"
#include "config.h"
#include "rewind.h"
#include "utils.h"
#include "drive/floppy_disk.h"
#include "drive/floppy_image.h"
#include "media/crc.h"

struct Floppy_Entry
{
    std::string path;
    int disk;
    std::string name;
};

// What the host knows about each drive: the file behind the disk, where its changes go and the other disks
// of the same image or playlist
struct Floppy_Host
{
    std::string source_path;
    std::string working_path;
    u32 base_crc;
    bool state_owned;
    std::vector<Floppy_Entry> entries;
    int entry;
};

static Floppy_Host hosts[config_floppy_drives];

static bool valid_drive(int drive);
static void clear_host(int drive);
static bool read_file(const char* path, std::vector<u8>& data);
static bool read_playlist(const char* path, std::vector<std::string>& entries);
static bool load_source(const char* path, std::vector<u8>& data);
static bool build_entries(const char* path, std::vector<Floppy_Entry>& entries);
static bool add_file_entries(const char* path, std::vector<Floppy_Entry>& entries);
static std::string make_working_path(const char* source_path, u32 base_crc);
static bool is_raw_name(const char* path);
static bool write_disk(FloppyDisk* disk, const char* path);
static bool write_working_copy(int drive);
static bool mount(int drive, int index, bool discard_changes);

void emu_floppy_init(void)
{
    for (int i = 0; i < config_floppy_drives; i++)
        clear_host(i);
}

bool emu_floppy_is_image(const char* path)
{
    if (!IsValidPointer(path))
        return false;

    if (FloppyImage::IsImageName(path))
        return true;

    if (ends_with_no_case(path, ".m3u"))
    {
        std::vector<std::string> entries;

        if (!read_playlist(path, entries))
            return true;

        for (size_t i = 0; i < entries.size(); i++)
        {
            if (!ends_with_no_case(entries[i].c_str(), ".m3u") && !emu_floppy_is_image(entries[i].c_str()))
                return false;
        }

        return true;
    }

    if (!ends_with_no_case(path, ".zip"))
        return false;

    std::vector<u8> archive;
    u8* data = NULL;
    u32 size = 0;

    if (!read_file(path, archive) || !FloppyImage::ExtractFromZip(archive.data(), archive.size(), &data, &size, NULL, 0))
        return false;

    SafeDeleteArray(data);
    return true;
}

bool emu_floppy_insert(int drive, const char* path, bool discard_changes)
{
    if (!valid_drive(drive) || !IsValidPointer(path))
        return false;

    std::vector<Floppy_Entry> entries;

    if (!build_entries(path, entries))
    {
        Error("Not a floppy disk image: %s", path);
        return false;
    }

    std::vector<Floppy_Entry> previous_entries = hosts[drive].entries;
    int previous_entry = hosts[drive].entry;
    hosts[drive].entries = entries;

    if (!mount(drive, 0, discard_changes))
    {
        hosts[drive].entries = previous_entries;
        hosts[drive].entry = previous_entry;
        return false;
    }

    config_push_recent_floppy(drive, path);
    Log("Floppy %d: %s", drive + 1, path);
    return true;
}

bool emu_floppy_select_disk(int drive, int index, bool discard_changes)
{
    if (!valid_drive(drive) || index < 0 || index >= (int)hosts[drive].entries.size())
        return false;

    return index == hosts[drive].entry || mount(drive, index, discard_changes);
}

// A stale working copy of an earlier blank with the same name would come back, so it goes first
bool emu_floppy_create_blank(int drive, const char* path, int type, bool formatted, bool discard_changes)
{
    if (!valid_drive(drive) || !IsValidPointer(path))
        return false;

    u32 size = 0;
    u8* image = FloppyImage::CreateBlank((FloppyImage_Blank)type, formatted, &size);

    if (!IsValidPointer(image))
        return false;

    FloppyDisk disk;
    bool created = disk.Insert(image, size, false, 0);
    u32 base_crc = 0;

    if (created && is_raw_name(path) && FloppyImage::GetRawSize(image, size) == 0)
    {
        Error("An unformatted disk can only be saved as D77: %s", path);
        created = false;
    }

    if (created)
        created = write_disk(&disk, path);

    if (created)
    {
        std::vector<u8> written;
        read_file(path, written);
        base_crc = CalculateCRC32(0, written.data(), (int)written.size());
    }

    SafeDeleteArray(image);

    if (!created)
        return false;

    std::string working = make_working_path(path, base_crc);
    SDL_RemovePath(working.c_str());

    return emu_floppy_insert(drive, path, discard_changes);
}

bool emu_floppy_eject(int drive, bool discard_changes)
{
    if (!valid_drive(drive))
        return false;

    FloppyDisk* disk = emu_get_core()->GetFloppy(drive);

    if (disk->IsDirty() && !discard_changes && !write_working_copy(drive))
        return false;

    emu_get_core()->EjectFloppy(drive);
    clear_host(drive);
    rewind_reset();
    return true;
}

bool emu_floppy_save(int drive)
{
    if (!valid_drive(drive) || !write_working_copy(drive))
        return false;

    rewind_reset();
    return true;
}

// The new file becomes the source and the place where later changes go
bool emu_floppy_save_as(int drive, const char* path)
{
    if (!valid_drive(drive) || !IsValidPointer(path))
        return false;

    FloppyDisk* disk = emu_get_core()->GetFloppy(drive);

    if (!disk->IsInserted() || !write_disk(disk, path))
        return false;

    Floppy_Host& host = hosts[drive];
    Floppy_Entry entry;
    entry.path = path;
    entry.disk = 0;
    entry.name = get_filename(path);
    host.entries.clear();
    host.entries.push_back(entry);
    host.entry = 0;
    host.source_path = path;
    host.working_path = path;
    host.state_owned = false;
    disk->ClearDirty();
    config_push_recent_floppy(drive, path);
    rewind_reset();
    return true;
}

// Discarding reloads the disk as it was last saved
bool emu_floppy_discard(int drive)
{
    if (!valid_drive(drive) || hosts[drive].state_owned || hosts[drive].entries.empty())
        return false;

    return mount(drive, hosts[drive].entry, true);
}

// A disk restored from a save state has no file to save into, so it stays protected until saved as one
bool emu_floppy_set_write_protected(int drive, bool write_protected)
{
    if (!valid_drive(drive))
        return false;

    FloppyDisk* disk = emu_get_core()->GetFloppy(drive);

    if (!disk->IsInserted() || (hosts[drive].state_owned && !write_protected))
        return false;

    disk->SetWriteProtected(write_protected);
    config_emulator.floppy_write_protected[drive] = write_protected;
    rewind_reset();
    return true;
}

// Working copies are named after the disk, not the drive, so they follow their disks
bool emu_floppy_swap(void)
{
    emu_get_core()->SwapFloppies();
    Floppy_Host host = hosts[0];
    hosts[0] = hosts[1];
    hosts[1] = host;
    bool write_protected = config_emulator.floppy_write_protected[0];
    config_emulator.floppy_write_protected[0] = config_emulator.floppy_write_protected[1];
    config_emulator.floppy_write_protected[1] = write_protected;
    rewind_reset();
    return true;
}

bool emu_floppy_flush(void)
{
    bool flushed = true;

    for (int i = 0; i < config_floppy_drives; i++)
    {
        FloppyDisk* disk = emu_get_core()->GetFloppy(i);

        if (disk->IsInserted() && disk->IsDirty() && !write_working_copy(i))
            flushed = false;
    }

    return flushed;
}

bool emu_floppy_get_info(int drive, Emu_FloppyInfo* info)
{
    if (!IsValidPointer(info))
        return false;

    memset(info, 0, sizeof(*info));

    if (!valid_drive(drive))
        return false;

    FloppyDisk* disk = emu_get_core()->GetFloppy(drive);
    const Floppy_Host& host = hosts[drive];

    if (!disk->IsInserted())
        return true;

    info->inserted = true;
    info->write_protected = disk->IsWriteProtected();
    info->dirty = disk->IsDirty();
    info->state_owned = host.state_owned;
    info->disk_count = (int)host.entries.size();
    info->disk_index = host.entry;
    strncpy_fit(info->path, host.source_path.c_str(), sizeof(info->path));
    strncpy_fit(info->working_path, host.working_path.c_str(), sizeof(info->working_path));
    return true;
}

const char* emu_floppy_get_disk_name(int drive, int index)
{
    if (!valid_drive(drive) || index < 0 || index >= (int)hosts[drive].entries.size())
        return "";

    return hosts[drive].entries[index].name.c_str();
}

// Names save states when no CD-ROM is in the drive
const char* emu_floppy_get_content_path(void)
{
    for (int i = 0; i < config_floppy_drives; i++)
    {
        if (!hosts[i].source_path.empty())
            return hosts[i].source_path.c_str();
    }

    return "";
}

// A state brings back its own disks, a disk the host does not know becomes a protected saved-state image
void emu_floppy_reconcile(void)
{
    for (int i = 0; i < config_floppy_drives; i++)
    {
        FloppyDisk* disk = emu_get_core()->GetFloppy(i);

        if (!disk->IsInserted())
        {
            clear_host(i);
            continue;
        }

        if (!hosts[i].source_path.empty() && hosts[i].base_crc == disk->GetBaseCRC())
            continue;

        clear_host(i);
        hosts[i].state_owned = true;
        hosts[i].base_crc = disk->GetBaseCRC();
        disk->SetWriteProtected(true);
        disk->ClearDirty();
    }
}

// A machine with fewer drives than before ejects the disks it no longer reaches
void emu_floppy_check_drives(void)
{
    int drives = emu_get_core()->GetMachineConfig().floppy_drives;

    for (int i = drives; i < config_floppy_drives; i++)
    {
        if (emu_get_core()->GetFloppy(i)->IsInserted() && !emu_floppy_eject(i, false))
            Error("Unable to save the disk in floppy drive %d", i + 1);
    }
}

static bool valid_drive(int drive)
{
    return drive >= 0 && drive < config_floppy_drives && IsValidPointer(emu_get_core());
}

static void clear_host(int drive)
{
    Floppy_Host& host = hosts[drive];
    host.source_path.clear();
    host.working_path.clear();
    host.base_crc = 0;
    host.state_owned = false;
    host.entries.clear();
    host.entry = 0;
}

static bool read_playlist(const char* path, std::vector<std::string>& entries)
{
    std::vector<u8> data;
    return read_file(path, data) &&
        Media::ParsePlaylist(path, reinterpret_cast<const char*>(data.data()), data.size(), entries);
}

static bool read_file(const char* path, std::vector<u8>& data)
{
    std::ifstream file;
    open_ifstream_utf8(file, path, std::ios::in | std::ios::binary | std::ios::ate);

    if (!file.is_open())
        return false;

    std::streamoff size = file.tellg();

    if (size <= 0 || size > (std::streamoff)k_floppy_max_image_size * 4)
        return false;

    data.resize((size_t)size);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return file.gcount() == size;
}

static bool load_source(const char* path, std::vector<u8>& data)
{
    if (!read_file(path, data))
    {
        Error("Unable to read %s", path);
        return false;
    }

    if (!ends_with_no_case(path, ".zip"))
        return true;

    u8* extracted = NULL;
    u32 size = 0;

    if (!FloppyImage::ExtractFromZip(data.data(), data.size(), &extracted, &size, NULL, 0))
    {
        Error("ZIP holds no single floppy image: %s", path);
        return false;
    }

    data.assign(extracted, extracted + size);
    SafeDeleteArray(extracted);
    return true;
}

// Every disk of a multi-disk image gets an entry, a playlist adds the disks of each listed file
static bool build_entries(const char* path, std::vector<Floppy_Entry>& entries)
{
    entries.clear();

    if (!ends_with_no_case(path, ".m3u"))
        return add_file_entries(path, entries);

    std::vector<std::string> paths;

    if (!read_playlist(path, paths))
        return false;

    for (size_t i = 0; i < paths.size(); i++)
    {
        const char* entry = paths[i].c_str();

        if (!ends_with_no_case(entry, ".m3u") && !emu_floppy_is_image(entry))
            continue;

        if (ends_with_no_case(entry, ".m3u") || !add_file_entries(entry, entries))
        {
            Error("Playlist entry is missing or not a floppy image: %s", entry);
            return false;
        }
    }

    return !entries.empty();
}

static bool add_file_entries(const char* path, std::vector<Floppy_Entry>& entries)
{
    std::vector<u8> data;

    if (!load_source(path, data))
        return false;

    int count = FloppyImage::GetDiskCount(data.data(), (u32)data.size());

    for (int i = 0; i < count; i++)
    {
        u32 offset = 0;
        u32 size = 0;
        char name[64];
        FloppyImage::GetDisk(data.data(), (u32)data.size(), i, &offset, &size);
        FloppyImage::GetDiskName(data.data() + offset, size, name, sizeof(name));

        Floppy_Entry entry;
        entry.path = path;
        entry.disk = i;
        entry.name = get_filename(path);

        if (count > 1)
        {
            char label[96];
            snprintf(label, sizeof(label), name[0] ? " (Disk %d: %s)" : " (Disk %d)", i + 1, name);
            entry.name += label;
        }

        entries.push_back(entry);
    }

    return count > 0;
}

static std::string make_working_path(const char* source_path, u32 base_crc)
{
    char directory[GT_MAX_PATH];

    switch ((Directory_Location)config_emulator.savefiles_dir_option)
    {
        case Directory_Location_ROM:
            get_directory(source_path, directory, sizeof(directory));
            break;
        case Directory_Location_Custom:
            strncpy_fit(directory, config_emulator.savefiles_path.c_str(), sizeof(directory));
            break;
        default:
            strncpy_fit(directory, config_root_path, sizeof(directory));
            break;
    }

    char name[GT_MAX_PATH];
    char file_name[GT_MAX_PATH];
    char path[GT_MAX_PATH];
    get_filename_without_extension(source_path, name, sizeof(name));
    snprintf(file_name, sizeof(file_name), "%s.%08X.geartowns.d77", name, base_crc);
    join_path(directory, file_name, path, sizeof(path));
    return path;
}

static bool is_raw_name(const char* path)
{
    return ends_with_no_case(path, ".hdm") || ends_with_no_case(path, ".xdf") || ends_with_no_case(path, ".img") ||
        ends_with_no_case(path, ".bin");
}

// Raw names get a raw image when every track has the standard layout, anything else is written as D77
// The file is replaced only once it is fully written
static bool write_disk(FloppyDisk* disk, const char* path)
{
    const u8* data = disk->GetImage();
    u32 size = disk->GetImageSize();
    std::vector<u8> raw;

    if (is_raw_name(path))
    {
        u32 raw_size = FloppyImage::GetRawSize(data, size);

        if (raw_size == 0)
        {
            Error("This disk does not have a standard layout, save it as D77: %s", path);
            return false;
        }

        raw.resize(raw_size);
        FloppyImage::ExportRaw(data, size, raw.data(), raw_size);
        data = raw.data();
        size = raw_size;
    }

    std::string temporary(path);
    temporary += ".tmp";
    std::ofstream file;
    open_ofstream_utf8(file, temporary.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);

    if (!file.is_open())
    {
        Error("Unable to write floppy image: %s", path);
        return false;
    }

    file.write(reinterpret_cast<const char*>(data), size);
    file.close();

    if (!file.good() || !SDL_RenamePath(temporary.c_str(), path))
    {
        SDL_RemovePath(temporary.c_str());
        Error("Unable to write floppy image: %s", path);
        return false;
    }

    return true;
}

static bool write_working_copy(int drive)
{
    FloppyDisk* disk = emu_get_core()->GetFloppy(drive);

    if (!disk->IsInserted() || !disk->IsDirty())
        return true;

    if (hosts[drive].working_path.empty() || !write_disk(disk, hosts[drive].working_path.c_str()))
        return false;

    disk->ClearDirty();
    Log("Floppy %d changes saved to %s", drive + 1, hosts[drive].working_path.c_str());
    return true;
}

// The working copy of a disk, when there is one, holds its changes and loads in place of the original
// Without remembered changes the disk is write protected, so nothing can be lost
static bool mount(int drive, int index, bool discard_changes)
{
    Floppy_Host& host = hosts[drive];
    FloppyDisk* current = emu_get_core()->GetFloppy(drive);

    if (current->IsDirty() && !discard_changes && !write_working_copy(drive))
        return false;

    const Floppy_Entry& entry = host.entries[index];
    std::vector<u8> file;
    u32 offset = 0;
    u32 size = 0;

    if (!load_source(entry.path.c_str(), file) ||
        !FloppyImage::GetDisk(file.data(), (u32)file.size(), entry.disk, &offset, &size))
    {
        Error("Not a floppy disk image: %s", entry.path.c_str());
        return false;
    }

    u32 base_crc = CalculateCRC32(0, file.data() + offset, (int)size);
    bool persistence = config_emulator.floppy_persistence;
    std::string working = persistence ? make_working_path(entry.path.c_str(), base_crc) : std::string();
    std::vector<u8> copy;
    const u8* data = file.data() + offset;

    if (persistence && read_file(working.c_str(), copy) && FloppyImage::IsD77(copy.data(), (u32)copy.size()))
    {
        Log("Loading floppy working copy: %s", working.c_str());
        data = copy.data();
        size = (u32)copy.size();
    }

    bool write_protected = !persistence || config_emulator.floppy_write_protected[drive];

    if (!emu_get_core()->InsertFloppy(drive, data, size, write_protected, base_crc))
    {
        Error("Unable to insert floppy image: %s", entry.path.c_str());
        return false;
    }

    host.source_path = entry.path;
    host.working_path = working;
    host.base_crc = base_crc;
    host.state_owned = false;
    host.entry = index;
    rewind_reset();
    return true;
}
