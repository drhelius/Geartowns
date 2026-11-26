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

#ifndef EMU_GAME_H
#define EMU_GAME_H

#include "geartowns.h"
#include "config.h"

#ifdef EMU_GAME_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

enum Emu_Game_Setting
{
    Emu_Game_Setting_Model = 0,
    Emu_Game_Setting_CPUSpeed,
    Emu_Game_Setting_RAM,
    Emu_Game_Setting_FPU,
    Emu_Game_Setting_SpeedMode,
    Emu_Game_Setting_MidiCard,
    Emu_Game_Setting_Port1,
    Emu_Game_Setting_Port2,
    Emu_Game_Setting_Keyboard1,
    Emu_Game_Setting_Keyboard2,
    Emu_Game_Setting_CDROMSpeed,
    Emu_Game_Setting_FastDiskTiming,
    Emu_Game_Setting_MidiMusic,
    Emu_Game_Setting_Count
};

enum Emu_Game_Disk
{
    Emu_Game_Disk_HardDisk = 0,
    Emu_Game_Disk_Floppy = GT_MAX_HARD_DISKS,
    Emu_Game_Disk_MemoryCard = Emu_Game_Disk_Floppy + config_floppy_drives,
    Emu_Game_Disk_Count
};

enum Emu_Game_Source
{
    Emu_Game_Source_User = 0,
    Emu_Game_Source_Change,
    Emu_Game_Source_Session
};

EXTERN void emu_game_update(void);
EXTERN bool emu_game_open(bool* hard_disks_changed);
EXTERN void emu_game_restart(void);
EXTERN bool emu_game_is_open(void);
EXTERN const char* emu_game_get_name(void);
EXTERN int emu_game_get(int setting);
EXTERN void emu_game_set(int setting, int value);
EXTERN void emu_game_set_session(int setting, int value);
EXTERN void emu_game_clear_session(void);
EXTERN int emu_game_get_source(int setting);
EXTERN bool emu_game_parse(int setting, const char* text, int* value);
EXTERN void emu_game_apply(void);
EXTERN bool emu_game_has_overrides(void);
EXTERN bool emu_game_reset_settings(void);
EXTERN const char* emu_game_get_kept_disk(int disk);
EXTERN bool emu_game_is_disk_kept(int disk);
EXTERN bool emu_game_can_keep_disk(int disk);
EXTERN void emu_game_keep_disk(int disk, bool keep);
EXTERN void emu_game_lock_disk(int disk);
EXTERN void emu_game_lock_cdrom(void);
EXTERN void emu_game_remember_drives(void);
EXTERN bool emu_game_restore_drives(bool cdrom, std::string* cdrom_path, std::string* failed);

#undef EMU_GAME_IMPORT
#undef EXTERN
#endif /* EMU_GAME_H */
