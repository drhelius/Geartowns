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

#define EMU_GAME_IMPORT
#include "emu_game.h"

#include <string>
#include <SDL3/SDL.h>
#define MINI_CASE_SENSITIVE
#include "ini.h"
#include "emu.h"
#include "emu_floppy.h"
#include "emu_hard_disk.h"
#include "emu_memory_card.h"
#include "events.h"
#include "gui_notifications.h"
#include "utils.h"
#include "drive/floppy_disk.h"
#include "media/game_db.h"

enum Game_Media
{
    Game_Media_None = 0,
    Game_Media_CDROM,
    Game_Media_Floppy,
    Game_Media_HardDisk
};

struct Game_Content
{
    Game_Media media;
    std::string key;
    std::string name;
    std::string path;
};

struct Game_Disk
{
    std::string path;
    int index;
};

struct Game_State
{
    Game_Content content;
    bool changed[Emu_Game_Setting_Count];
    int values[Emu_Game_Setting_Count];
    Game_Disk disks[Emu_Game_Disk_Count];
};

static const int k_game_max_values = 32;
static const char* const k_game_setting_keys[Emu_Game_Setting_Count] = { "Model", "CPUSpeed", "RAM", "FPU", "SpeedMode",
    "Port1", "Port2", "Port1Keyboard", "Port2Keyboard", "CDROMSpeed", "FastDiskTiming" };

static Game_State game = { };
static Game_Content playlist = { };
static std::string last_key;
static bool last_powered = false;
static bool latch_pending = false;
static bool session_set[Emu_Game_Setting_Count] = { };
static int session_values[Emu_Game_Setting_Count] = { };
static bool locked_disks[Emu_Game_Disk_Count] = { };
static bool applied = false;
static int applied_keyboards[GT_MAX_GAMEPADS] = { };

static Game_Content find_content(void);
static bool is_in_database(const Game_Content& content);
static bool keeps_game(const Game_Content& content, bool restart);
static bool is_attached(const std::string& path);
static std::string get_section_name(const char* file_name);
static bool open_game(const Game_Content& content, bool latch);
static void clear_game(void);
static std::string get_games_path(void);
static bool has_section(const std::string& key);
static void load_game(void);
static bool save_game(void);
static bool write_other_sections(const mINI::INIFile& file, const mINI::INIStructure& ini);
static void save_changes(void);
static int* get_user_value(int setting);
static int get_user(int setting);
static void set_user(int setting, int value);
static int get_values(int setting, int* values);
static std::string get_value_text(int setting, int value);
static std::string get_disk_key(int disk);
static std::string get_disk_label(int disk);
static std::string get_absolute_path(const char* path);
static bool get_inserted_disk(int disk, Game_Disk* inserted);
static bool is_own_disk(int disk);
static bool insert_disk(int disk, const Game_Disk& kept);
static bool restore_disks(bool* floppies_changed);

void emu_game_update(void)
{
    Game_Content content = find_content();
    bool powered = emu_is_powered_on();
    bool changed = content.key != last_key;
    bool stopped = last_powered && !powered;
    last_key = content.key;
    last_powered = powered;

    if (!powered && (changed || stopped) && (content.key != game.content.key) && !keeps_game(content, !changed))
        open_game(content, latch_pending);
}

bool emu_game_open(bool* hard_disks_changed)
{
    Game_Content content = find_content();
    bool powered = emu_is_powered_on();
    bool same = powered && (content.key == game.content.key);
    last_key = content.key;
    latch_pending = powered && !same && emu_game_has_overrides() && !has_section(content.key) && !is_in_database(content);
    *hard_disks_changed = false;

    if (latch_pending)
        return false;

    if (same)
        emu_get_core()->LatchAutoMedia();
    else
        *hard_disks_changed = open_game(content, true);

    return true;
}

void emu_game_restart(void)
{
    Game_Content content = find_content();

    if ((content.key != game.content.key) && !keeps_game(content, true))
        open_game(content, latch_pending);
}

bool emu_game_is_open(void)
{
    return game.content.media != Game_Media_None;
}

const char* emu_game_get_name(void)
{
    return game.content.name.c_str();
}

int emu_game_get(int setting)
{
    if (session_set[setting])
        return session_values[setting];

    return game.changed[setting] ? game.values[setting] : get_user(setting);
}

void emu_game_set(int setting, int value)
{
    session_set[setting] = false;

    if (!emu_game_is_open())
        set_user(setting, value);
    else
    {
        bool changed = value != get_user(setting);
        bool same = (changed == game.changed[setting]) && (!changed || (value == game.values[setting]));
        game.changed[setting] = changed;
        game.values[setting] = value;

        if (!same)
            save_changes();
    }

    emu_game_apply();
}

void emu_game_set_session(int setting, int value)
{
    session_set[setting] = true;
    session_values[setting] = value;

    if (IsValidPointer(emu_get_core()))
        emu_game_apply();
}

void emu_game_clear_session(void)
{
    for (int i = 0; i < Emu_Game_Setting_Count; i++)
        session_set[i] = false;
}

int emu_game_get_source(int setting)
{
    if (session_set[setting])
        return Emu_Game_Source_Session;

    return game.changed[setting] ? Emu_Game_Source_Change : Emu_Game_Source_User;
}

bool emu_game_parse(int setting, const char* text, int* value)
{
    int values[k_game_max_values];
    int count = get_values(setting, values);

    for (int i = 0; i < count; i++)
    {
        if (get_value_text(setting, values[i]) == text)
        {
            *value = values[i];
            return true;
        }
    }

    return false;
}

void emu_game_apply(void)
{
    int cdrom_speed = emu_game_get(Emu_Game_Setting_CDROMSpeed);
    bool sync_input = false;

    emu_apply_machine_settings();
    emu_update_pad_types();
    emu_set_cdrom_speed(cdrom_speed > 0 ? 1 << (cdrom_speed - 1) : 0);
    emu_set_fast_hard_disk_timing(emu_game_get(Emu_Game_Setting_FastDiskTiming) != 0);

    for (int i = 0; i < GT_MAX_GAMEPADS; i++)
    {
        int keyboard = emu_game_get(Emu_Game_Setting_Keyboard1 + i);
        sync_input = sync_input || (applied && (keyboard != applied_keyboards[i]));
        applied_keyboards[i] = keyboard;
    }

    applied = true;

    if (sync_input)
        events_sync_input();
}

bool emu_game_has_overrides(void)
{
    for (int i = 0; i < Emu_Game_Setting_Count; i++)
    {
        if (game.changed[i])
            return true;
    }

    for (int i = 0; i < Emu_Game_Disk_Count; i++)
    {
        if (!game.disks[i].path.empty())
            return true;
    }

    return false;
}

bool emu_game_reset_settings(void)
{
    if (!emu_game_is_open())
        return false;

    clear_game();

    if (!save_game())
        return false;

    emu_game_apply();
    return true;
}

const char* emu_game_get_kept_disk(int disk)
{
    return game.disks[disk].path.c_str();
}

bool emu_game_is_disk_kept(int disk)
{
    const Game_Disk& kept = game.disks[disk];
    Game_Disk inserted;

    if (kept.path.empty())
        return false;

    return !get_inserted_disk(disk, &inserted) || ((inserted.path == kept.path) && (inserted.index == kept.index));
}

bool emu_game_can_keep_disk(int disk)
{
    Game_Disk inserted;
    return emu_game_is_open() && !is_own_disk(disk) && (!game.disks[disk].path.empty() || get_inserted_disk(disk, &inserted));
}

void emu_game_keep_disk(int disk, bool keep)
{
    if (!emu_game_can_keep_disk(disk))
        return;

    if (!keep || !get_inserted_disk(disk, &game.disks[disk]))
    {
        game.disks[disk].path.clear();
        game.disks[disk].index = 0;
    }

    save_changes();
}

void emu_game_lock_disk(int disk)
{
    locked_disks[disk] = true;
}

static Game_Content find_content(void)
{
    Game_Content content;
    content.media = Game_Media_None;

    Media* media = emu_get_core()->GetMedia();

    if (media->IsReady())
    {
        int index = emu_cdrom_playlist_get_index();

        if ((index > 0) && (playlist.media != Game_Media_None))
            return playlist;

        char crc[16];
        snprintf(crc, sizeof(crc), "%08X", media->GetCRC());
        const GT_DB_Entry* entry = media->GetGameDBEntry();
        content.media = Game_Media_CDROM;
        content.key = media->GetCRC() != 0 ? std::string(crc) : get_section_name(media->GetFileName());
        content.name = IsValidPointer(entry) ? std::string(entry->title) : path_strip_extension(media->GetFileName());
        content.path = media->GetFilePath();
        playlist = content;

        if (index != 0)
            playlist.media = Game_Media_None;

        return content;
    }

    content.path = emu_floppy_get_content_path();

    if (!content.path.empty())
        content.media = Game_Media_Floppy;

    for (int i = 0; (i < GT_MAX_HARD_DISKS) && content.path.empty(); i++)
    {
        Emu_HardDiskInfo info;
        emu_hard_disk_get_info(i, &info);
        content.path = info.pending ? info.pending_path : info.path;

        if (!content.path.empty())
            content.media = Game_Media_HardDisk;
    }

    if (content.media != Game_Media_None)
    {
        content.key = get_section_name(get_filename(content.path.c_str()));
        content.name = path_strip_extension(path_filename(content.path));
    }

    return content;
}

static bool is_in_database(const Game_Content& content)
{
    GeartownsCore* core = emu_get_core();

    if (content.media == Game_Media_CDROM)
        return IsValidPointer(core->GetMedia()->GetGameDBEntry());

    for (int i = 0; (content.media == Game_Media_Floppy) && (i < config_floppy_drives); i++)
    {
        Emu_FloppyInfo info;
        emu_floppy_get_info(i, &info);

        if (content.path == info.path)
        {
            u32 crc = core->GetFloppy(i)->GetDataCRC();
            return (crc != 0) && IsValidPointer(Media::FindFloppyEntry(crc));
        }
    }

    return false;
}

static bool keeps_game(const Game_Content& content, bool restart)
{
    // Another of its floppies, a floppy in a machine that boots its hard disk, or the disk it was installed to
    if (content.media == Game_Media_Floppy)
        return (game.content.media == Game_Media_HardDisk) ? is_attached(game.content.path) :
            restart && (game.content.media == Game_Media_Floppy);

    return restart && (content.media == Game_Media_HardDisk) && emu_game_is_open() && !has_section(content.key);
}

static bool is_attached(const std::string& path)
{
    for (int i = 0; i < GT_MAX_HARD_DISKS; i++)
    {
        Emu_HardDiskInfo info;
        emu_hard_disk_get_info(i, &info);

        if (path == (info.pending ? info.pending_path : info.path))
            return true;
    }

    return false;
}

static std::string get_section_name(const char* file_name)
{
    std::string name = file_name;

    for (size_t i = 0; i < name.size(); i++)
    {
        if ((name[i] == '[') || (name[i] == ']') || (name[i] == ';'))
            name[i] = '_';
    }

    return name;
}

static bool open_game(const Game_Content& content, bool latch)
{
    bool floppies_changed = false;

    clear_game();
    game.content = content;
    latch_pending = false;
    load_game();
    emu_game_apply();

    if (latch)
        emu_get_core()->LatchAutoMedia();

    bool hard_disks_changed = restore_disks(&floppies_changed);

    if (latch && floppies_changed)
        emu_get_core()->LatchAutoMedia();

    return hard_disks_changed;
}

static void clear_game(void)
{
    for (int i = 0; i < Emu_Game_Setting_Count; i++)
        game.changed[i] = false;

    for (int i = 0; i < Emu_Game_Disk_Count; i++)
    {
        game.disks[i].path.clear();
        game.disks[i].index = 0;
    }
}

static std::string get_games_path(void)
{
    std::string path = config_root_path;
    append_path_component(path, "games.ini");
    return path;
}

static bool has_section(const std::string& key)
{
    mINI::INIFile file(get_games_path());
    mINI::INIStructure ini;
    return file.read(ini) && ini.has(key);
}

static void load_game(void)
{
    mINI::INIFile file(get_games_path());
    mINI::INIStructure ini;

    if (!emu_game_is_open() || !file.read(ini) || !ini.has(game.content.key))
        return;

    mINI::INIMap<std::string>& section = ini[game.content.key];

    for (int i = 0; i < Emu_Game_Setting_Count; i++)
    {
        if (section.has(k_game_setting_keys[i]))
            game.changed[i] = emu_game_parse(i, section[k_game_setting_keys[i]].c_str(), &game.values[i]);
    }

    for (int i = 0; i < Emu_Game_Disk_Count; i++)
    {
        std::string key = get_disk_key(i);
        game.disks[i].path = section.get(key);
        game.disks[i].index = atoi(section.get(key + "Disk").c_str());
    }

    Log("Settings loaded for %s", game.content.name.c_str());
}

static bool save_game(void)
{
    std::string path = get_games_path();
    mINI::INIFile file(path);
    mINI::INIStructure ini;

    if (!file.read(ini) && path_exists(path.c_str()))
        return false;

    if (!emu_game_has_overrides())
        return write_other_sections(file, ini);

    mINI::INIMap<std::string>& section = ini[game.content.key];
    section["Name"] = game.content.name;

    for (int i = 0; i < Emu_Game_Setting_Count; i++)
    {
        if (game.changed[i])
            section[k_game_setting_keys[i]] = get_value_text(i, game.values[i]);
        else
            section.remove(k_game_setting_keys[i]);
    }

    for (int i = 0; i < Emu_Game_Disk_Count; i++)
    {
        std::string key = get_disk_key(i);
        section.remove(key);
        section.remove(key + "Disk");

        if (!game.disks[i].path.empty())
            section[key] = game.disks[i].path;

        if (game.disks[i].index > 0)
            section[key + "Disk"] = std::to_string(game.disks[i].index);
    }

    return file.write(ini, true);
}

static bool write_other_sections(const mINI::INIFile& file, const mINI::INIStructure& ini)
{
    mINI::INIStructure others;

    for (mINI::INIStructure::const_iterator section = ini.begin(); section != ini.end(); ++section)
    {
        if (section->first == game.content.key)
            continue;

        mINI::INIMap<std::string>& other = others[section->first];

        for (mINI::INIMap<std::string>::const_iterator key = section->second.begin(); key != section->second.end(); ++key)
            other[key->first] = key->second;
    }

    return file.write(others, true);
}

static void save_changes(void)
{
    if (!save_game())
    {
        gui_notify(gui_NotificationError, NULL, "Unable to save the game's settings", get_games_path().c_str(), "game_settings");
        return;
    }

    if (emu_game_has_overrides())
        gui_notify(gui_NotificationInfo, ICON_MD_VIDEOGAME_ASSET, ("Saved for " + game.content.name + " only").c_str(),
            "Forget it in Machine", "game_settings");
    else
        gui_notify(gui_NotificationInfo, ICON_MD_VIDEOGAME_ASSET,
            (game.content.name + " uses the general settings again").c_str(), NULL, "game_settings");
}

static int* get_user_value(int setting)
{
    switch (setting)
    {
        case Emu_Game_Setting_Model:
            return &config_machine.model;
        case Emu_Game_Setting_CPUSpeed:
            return &config_machine.cpu_mhz;
        case Emu_Game_Setting_RAM:
            return &config_machine.ram_mb;
        case Emu_Game_Setting_FPU:
            return &config_machine.fpu;
        case Emu_Game_Setting_SpeedMode:
            return &config_machine.speed_mode;
        case Emu_Game_Setting_Port1:
        case Emu_Game_Setting_Port2:
            return &config_input.controller_type[setting - Emu_Game_Setting_Port1];
        case Emu_Game_Setting_Keyboard1:
        case Emu_Game_Setting_Keyboard2:
            return &config_input.keyboard_mode[setting - Emu_Game_Setting_Keyboard1];
        case Emu_Game_Setting_CDROMSpeed:
            return &config_emulator.cdrom_speed;
        default:
            return NULL;
    }
}

static int get_user(int setting)
{
    if (setting == Emu_Game_Setting_FastDiskTiming)
        return config_emulator.hard_disk_fast_timing ? 1 : 0;

    return *get_user_value(setting);
}

static void set_user(int setting, int value)
{
    if (setting == Emu_Game_Setting_FastDiskTiming)
        config_emulator.hard_disk_fast_timing = value != 0;
    else
        *get_user_value(setting) = value;
}

static int get_values(int setting, int* values)
{
    int count = 0;
    int first = 0;
    int last = 1;

    switch (setting)
    {
        case Emu_Game_Setting_Model:
            values[count++] = GT_MACHINE_AUTO;

            for (int i = 0; i < GT_MACHINE_RELEASE_ORDER_COUNT; i++)
            {
                if (k_machine_profiles[k_machine_release_order[i]].emulated)
                    values[count++] = k_machine_release_order[i];
            }

            return count;
        case Emu_Game_Setting_CPUSpeed:
            values[count++] = GT_MACHINE_AUTO;

            for (int i = 0; i < GT_MACHINE_CPU_SPEED_COUNT; i++)
                values[count++] = k_machine_cpu_speeds_mhz[i];

            return count;
        case Emu_Game_Setting_RAM:
            values[count++] = GT_MACHINE_AUTO;

            for (int i = 0; i < GT_MACHINE_RAM_SIZE_COUNT; i++)
                values[count++] = k_machine_ram_sizes_mb[i];

            return count;
        case Emu_Game_Setting_FPU:
        case Emu_Game_Setting_SpeedMode:
            first = GT_MACHINE_AUTO;
            break;
        case Emu_Game_Setting_Port1:
        case Emu_Game_Setting_Port2:
            first = config_ControllerType_Auto;
            last = GT_CONTROLLER_MOUSE;
            break;
        case Emu_Game_Setting_CDROMSpeed:
            last = 4;
            break;
        default:
            break;
    }

    for (int value = first; value <= last; value++)
        values[count++] = value;

    return count;
}

static std::string get_value_text(int setting, int value)
{
    static const char* const k_ports[] = { "none", "original_gamepad", "marty_gamepad", "six_button_gamepad", "mouse" };

    switch (setting)
    {
        case Emu_Game_Setting_Model:
            return value < 0 ? "auto" : k_machine_profiles[value].key;
        case Emu_Game_Setting_CPUSpeed:
        case Emu_Game_Setting_RAM:
            return value <= 0 ? "auto" : std::to_string(value);
        case Emu_Game_Setting_FPU:
            return value < 0 ? "auto" : value == 0 ? "none" : "80387";
        case Emu_Game_Setting_SpeedMode:
            return value < 0 ? "auto" : value == 0 ? "compatible" : "high";
        case Emu_Game_Setting_Port1:
        case Emu_Game_Setting_Port2:
            return value < 0 ? "auto" : k_ports[value];
        case Emu_Game_Setting_Keyboard1:
        case Emu_Game_Setting_Keyboard2:
            return value == config_KeyboardMode_Towns ? "towns" : "gamepad";
        case Emu_Game_Setting_CDROMSpeed:
            return value <= 0 ? "auto" : std::to_string(1 << (value - 1));
        default:
            return value != 0 ? "true" : "false";
    }
}

static std::string get_disk_key(int disk)
{
    if (disk < Emu_Game_Disk_Floppy)
        return "HardDisk" + std::to_string(disk);

    return disk < Emu_Game_Disk_MemoryCard ? "Floppy" + std::to_string(disk - Emu_Game_Disk_Floppy + 1) : "MemoryCard";
}

static std::string get_disk_label(int disk)
{
    if (disk < Emu_Game_Disk_Floppy)
        return "SCSI ID " + std::to_string(disk);

    return disk < Emu_Game_Disk_MemoryCard ? "Floppy " + std::to_string(disk - Emu_Game_Disk_Floppy + 1) : "IC memory card";
}

static std::string get_absolute_path(const char* path)
{
    if ((path[0] == 0) || is_absolute_path(path))
        return path;

    char* directory = SDL_GetCurrentDirectory();
    std::string absolute = IsValidPointer(directory) ? directory : "";
    SDL_free(directory);
    return absolute + path;
}

static bool get_inserted_disk(int disk, Game_Disk* inserted)
{
    inserted->path.clear();
    inserted->index = 0;

    if (disk < Emu_Game_Disk_Floppy)
    {
        Emu_HardDiskInfo info;
        emu_hard_disk_get_info(disk, &info);
        inserted->path = get_absolute_path(info.pending ? info.pending_path : info.path);
    }
    else if (disk < Emu_Game_Disk_MemoryCard)
    {
        Emu_FloppyInfo info;
        emu_floppy_get_info(disk - Emu_Game_Disk_Floppy, &info);
        inserted->path = get_absolute_path(info.path);
        inserted->index = info.file_index;
    }
    else
    {
        Emu_MemoryCardInfo info;
        emu_memory_card_get_info(&info);
        inserted->path = get_absolute_path(info.path);
    }

    return !inserted->path.empty();
}

static bool is_own_disk(int disk)
{
    if (disk < Emu_Game_Disk_Floppy)
    {
        Emu_HardDiskInfo info;
        emu_hard_disk_get_info(disk, &info);
        return (game.content.media == Game_Media_HardDisk) && (game.content.path == (info.pending ? info.pending_path : info.path));
    }

    if ((disk == Emu_Game_Disk_MemoryCard) || (game.content.media != Game_Media_Floppy))
        return false;

    Emu_FloppyInfo info;
    emu_floppy_get_info(disk - Emu_Game_Disk_Floppy, &info);
    return (disk == Emu_Game_Disk_Floppy) || (game.content.path == info.path);
}

static bool insert_disk(int disk, const Game_Disk& kept)
{
    const char* path = kept.path.c_str();

    if (!path_exists(path))
        return false;

    if (disk < Emu_Game_Disk_Floppy)
        return emu_hard_disk_attach(disk, path, config_emulator.hard_disk_write_protected[disk] != 0);

    if (disk < Emu_Game_Disk_MemoryCard)
        return emu_floppy_insert(disk - Emu_Game_Disk_Floppy, path, false) &&
            emu_floppy_select_disk(disk - Emu_Game_Disk_Floppy, kept.index, false);

    return emu_memory_card_insert(path, config_emulator.memory_card_write_protected);
}

static bool restore_disks(bool* floppies_changed)
{
    int floppy_drives = emu_get_core()->GetPendingMachineConfig().floppy_drives;
    std::string inserted;
    std::string failed;
    bool pending = false;

    for (int i = 0; emu_game_is_open() && (i < Emu_Game_Disk_Count); i++)
    {
        const Game_Disk& kept = game.disks[i];
        bool missing_drive = (i >= Emu_Game_Disk_Floppy) && (i < Emu_Game_Disk_MemoryCard) &&
            (i - Emu_Game_Disk_Floppy >= floppy_drives);

        if (kept.path.empty() || locked_disks[i] || missing_drive || is_own_disk(i))
            continue;

        Game_Disk current;
        bool ready = get_inserted_disk(i, &current) && (current.path == kept.path) && (current.index == kept.index);

        if (!ready)
        {
            ready = insert_disk(i, kept);
            std::string& list = ready ? inserted : failed;
            list += (list.empty() ? "" : "\n") + get_disk_label(i) + ": " + get_filename(kept.path.c_str());
            *floppies_changed = *floppies_changed || (i >= Emu_Game_Disk_Floppy);
        }

        Emu_HardDiskInfo info;
        pending = pending || (ready && (i < Emu_Game_Disk_Floppy) && emu_hard_disk_get_info(i, &info) && info.pending);
    }

    if (!inserted.empty())
        gui_notify(gui_NotificationInfo, ICON_MD_VIDEOGAME_ASSET, ("Disks kept with " + game.content.name).c_str(),
            inserted.c_str());

    if (!failed.empty())
        gui_notify(gui_NotificationError, NULL, ("Unable to insert the disks kept with " + game.content.name).c_str(),
            failed.c_str());

    return pending;
}
