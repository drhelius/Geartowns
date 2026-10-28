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

#define EMU_MEMORY_CARD_IMPORT
#include "emu_memory_card.h"

#include <fstream>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include "emu.h"
#include "config.h"
#include "rewind.h"
#include "utils.h"
#include "drive/memory_card.h"

static const int k_idle_flush_frames = 180;
static std::string card_path;
static u32 write_count = 0;
static int idle_frames = 0;
static bool flush_failed = false;
static bool flush_error = false;
static bool state_change = false;
static bool rewind_changed = false;

static bool read_card(const char* path, std::vector<u8>& data);
static bool write_card(const char* path, const u8* data, u32 size, bool report_errors);
static bool is_writable(const char* path);

bool emu_memory_card_is_image(const char* path)
{
    return IsValidPointer(path) && ends_with_no_case(path, ".icm");
}

bool emu_memory_card_insert(const char* path, bool write_protected)
{
    std::vector<u8> data;

    if (!IsValidPointer(path) || !emu_memory_card_flush() || !read_card(path, data))
        return false;

    MemoryCard* card = emu_get_core()->GetMemoryCard();

    if (!emu_get_core()->InsertMemoryCard(data.data(), (u32)data.size(), write_protected || !is_writable(path)))
        return false;

    card_path = path;
    write_count = card->GetWriteCount();
    idle_frames = 0;
    flush_failed = false;
    config_push_recent_memory_card(path);
    rewind_reset();
    return true;
}

bool emu_memory_card_create(const char* path, u32 size, bool write_protected)
{
    if (!IsValidPointer(path) || !MemoryCard::IsValidSize(size) || SDL_GetPathInfo(path, NULL) ||
        !emu_memory_card_flush())
        return false;

    std::vector<u8> blank(size, 0xFF);

    if (!write_card(path, blank.data(), size, true))
        return false;

    return emu_memory_card_insert(path, write_protected);
}

bool emu_memory_card_eject(bool discard_changes)
{
    MemoryCard* card = emu_get_core()->GetMemoryCard();

    if (!card->IsInserted())
    {
        card_path.clear();
        return true;
    }

    if (!discard_changes && !emu_memory_card_flush())
        return false;

    emu_get_core()->EjectMemoryCard();
    card_path.clear();
    flush_failed = false;
    rewind_reset();
    return true;
}

bool emu_memory_card_set_write_protected(bool write_protected)
{
    MemoryCard* card = emu_get_core()->GetMemoryCard();

    if (card->IsInserted())
    {
        if (!write_protected && !is_writable(card_path.c_str()))
            return false;

        card->SetWriteProtected(write_protected);
    }

    config_emulator.memory_card_write_protected = write_protected;
    return true;
}

bool emu_memory_card_flush(void)
{
    MemoryCard* card = emu_get_core()->GetMemoryCard();
    idle_frames = 0;

    if (!card->IsInserted() || !card->IsDirty())
        return true;

    if (card_path.empty() || !write_card(card_path.c_str(), card->GetImage(), card->GetSize(), !flush_failed))
    {
        if (!flush_failed)
            flush_error = true;

        flush_failed = true;
        return false;
    }

    card->ClearDirty();
    flush_failed = false;
    Log("IC memory card changes saved to %s", card_path.c_str());
    return true;
}

void emu_memory_card_update(void)
{
    MemoryCard* card = emu_get_core()->GetMemoryCard();

    if (!card->IsInserted() || !card->IsDirty())
        return;

    if (card->GetWriteCount() != write_count)
    {
        write_count = card->GetWriteCount();
        idle_frames = 0;
        return;
    }

    if (++idle_frames >= k_idle_flush_frames)
        emu_memory_card_flush();
}

bool emu_memory_card_take_flush_error(void)
{
    bool error = flush_error;
    flush_error = false;
    return error;
}

bool emu_memory_card_get_info(Emu_MemoryCardInfo* info)
{
    if (!IsValidPointer(info))
        return false;

    memset(info, 0, sizeof(*info));
    MemoryCard* card = emu_get_core()->GetMemoryCard();
    info->inserted = card->IsInserted();
    info->write_protected = card->IsWriteProtected();
    info->dirty = card->IsDirty();
    info->size = card->GetSize();

    if (info->inserted)
        strncpy_fit(info->path, card_path.c_str(), sizeof(info->path));

    return true;
}

void emu_memory_card_state_loaded(bool rewind)
{
    bool changed = emu_get_core()->IsMemoryCardStateChanged();

    if (!rewind || !rewind_changed)
        state_change = state_change || changed;

    rewind_changed = rewind && changed;
}

bool emu_memory_card_take_state_change(void)
{
    bool change = state_change;
    state_change = false;
    return change;
}

static bool read_card(const char* path, std::vector<u8>& data)
{
    std::ifstream file;
    open_ifstream_utf8(file, path, std::ios::in | std::ios::binary | std::ios::ate);

    if (!file.is_open())
    {
        Error("Unable to read %s", path);
        return false;
    }

    std::streamoff size = file.tellg();

    if ((size <= 0) || !MemoryCard::IsValidSize((u64)size))
    {
        Error("An IC memory card image is a whole number of KB up to 64 MB: %s", path);
        return false;
    }

    data.resize((size_t)size);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(data.data()), size);

    if (file.gcount() != size)
    {
        Error("Unable to read %s", path);
        return false;
    }

    return true;
}

static bool write_card(const char* path, const u8* data, u32 size, bool report_errors)
{
    std::string temporary(path);
    temporary += ".tmp";
    std::ofstream file;
    open_ofstream_utf8(file, temporary.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);

    if (!file.is_open())
    {
        if (report_errors)
            Error("Unable to write IC memory card image: %s", path);

        return false;
    }

    file.write(reinterpret_cast<const char*>(data), size);
    file.close();

    if (!file.good() || !SDL_RenamePath(temporary.c_str(), path))
    {
        SDL_RemovePath(temporary.c_str());

        if (report_errors)
            Error("Unable to write IC memory card image: %s", path);

        return false;
    }

    return true;
}

static bool is_writable(const char* path)
{
    FILE* file = fopen_utf8(path, "r+b");

    if (!IsValidPointer(file))
        return false;

    fclose(file);
    return true;
}
