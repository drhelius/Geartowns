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

#ifndef EMU_MEMORY_CARD_H
#define EMU_MEMORY_CARD_H

#include "geartowns.h"

#ifdef EMU_MEMORY_CARD_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

struct Emu_MemoryCardInfo
{
    bool inserted;
    bool write_protected;
    bool dirty;
    u32 size;
    char path[GT_MAX_PATH];
};

EXTERN bool emu_memory_card_is_image(const char* path);
EXTERN bool emu_memory_card_insert(const char* path, bool write_protected);
EXTERN bool emu_memory_card_create(const char* path, u32 size, bool write_protected);
EXTERN bool emu_memory_card_eject(bool discard_changes);
EXTERN bool emu_memory_card_set_write_protected(bool write_protected);
EXTERN bool emu_memory_card_flush(void);
EXTERN void emu_memory_card_update(void);
EXTERN bool emu_memory_card_take_flush_error(void);
EXTERN bool emu_memory_card_get_info(Emu_MemoryCardInfo* info);
EXTERN void emu_memory_card_state_loaded(bool rewind);
EXTERN bool emu_memory_card_take_state_change(void);

#undef EMU_MEMORY_CARD_IMPORT
#undef EXTERN
#endif /* EMU_MEMORY_CARD_H */
