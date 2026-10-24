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

#ifndef EMU_HARD_DISK_H
#define EMU_HARD_DISK_H

#include "geartowns.h"

#ifdef EMU_HARD_DISK_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

struct Emu_HardDiskInfo
{
    bool attached;
    bool write_protected;
    bool pending;
    bool pending_attached;
    bool pending_write_protected;
    u32 blocks;
    u32 unflushed;
    char path[GT_MAX_PATH];
    char pending_path[GT_MAX_PATH];
};

EXTERN bool emu_hard_disk_is_image(const char* path);
EXTERN int emu_hard_disk_get_image_id(const char* path);
EXTERN bool emu_hard_disk_attach(int id, const char* path, bool write_protected);
EXTERN bool emu_hard_disk_detach(int id);
EXTERN bool emu_hard_disk_create(int id, const char* path, int size_mb, bool formatted, bool write_protected);
EXTERN bool emu_hard_disk_set_write_protected(int id, bool write_protected);
EXTERN bool emu_hard_disk_flush(void);
EXTERN void emu_hard_disk_update(void);
EXTERN bool emu_hard_disk_take_flush_error(void);
EXTERN bool emu_hard_disk_get_info(int id, Emu_HardDiskInfo* info);
EXTERN const char* emu_hard_disk_get_content_path(void);
EXTERN void emu_hard_disk_state_loaded(bool rewind);
EXTERN u32 emu_hard_disk_take_state_changes(void);

#undef EMU_HARD_DISK_IMPORT
#undef EXTERN
#endif /* EMU_HARD_DISK_H */
