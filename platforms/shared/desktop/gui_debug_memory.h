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

#ifndef GUI_DEBUG_MEMORY_H
#define GUI_DEBUG_MEMORY_H

#include <iosfwd>
#include "geartowns.h"

void gui_debug_memory_init(void);
void gui_debug_memory_destroy(void);
void gui_debug_memory_reset(void);
void gui_debug_memory_update(void);
void gui_debug_window_memory(void);
void gui_debug_memory_auxiliary_windows(void);
void gui_debug_memory_copy(void);
void gui_debug_memory_paste(void);
void gui_debug_memory_save_dump(const char* file_path);
void gui_debug_memory_load_dump(const char* file_path);
void gui_debug_memory_save_settings(std::ostream& stream);
bool gui_debug_memory_load_settings(std::istream& stream);

#endif /* GUI_DEBUG_MEMORY_H */
