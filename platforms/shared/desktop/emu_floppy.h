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

#ifndef EMU_FLOPPY_H
#define EMU_FLOPPY_H

#include "geartowns.h"

#ifdef EMU_FLOPPY_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

struct Emu_FloppyInfo
{
    bool inserted;
    bool write_protected;
    bool dirty;
    bool state_owned;
    int disk_count;
    int disk_index;
    char path[GT_MAX_PATH];
    char working_path[GT_MAX_PATH];
};

EXTERN void emu_floppy_init(void);
EXTERN bool emu_floppy_is_image(const char* path);
EXTERN bool emu_floppy_insert(int drive, const char* path, bool discard_changes);
EXTERN bool emu_floppy_select_disk(int drive, int index, bool discard_changes);
EXTERN bool emu_floppy_create_blank(int drive, const char* path, int type, bool formatted, bool discard_changes);
EXTERN bool emu_floppy_eject(int drive, bool discard_changes);
EXTERN bool emu_floppy_save(int drive);
EXTERN bool emu_floppy_save_as(int drive, const char* path);
EXTERN bool emu_floppy_discard(int drive);
EXTERN bool emu_floppy_set_write_protected(int drive, bool write_protected);
EXTERN bool emu_floppy_swap(void);
EXTERN bool emu_floppy_flush(void);
EXTERN bool emu_floppy_get_info(int drive, Emu_FloppyInfo* info);
EXTERN const char* emu_floppy_get_disk_name(int drive, int index);
EXTERN const char* emu_floppy_get_content_path(void);
EXTERN void emu_floppy_reconcile(void);
EXTERN void emu_floppy_check_drives(void);

#undef EMU_FLOPPY_IMPORT
#undef EXTERN
#endif /* EMU_FLOPPY_H */
