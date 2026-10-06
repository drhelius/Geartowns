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

#ifndef GUI_DEBUG_TRACE_H
#define GUI_DEBUG_TRACE_H

#include "geartowns.h"

#define GUI_DEBUG_TRACE_TEXT_SIZE 256

#ifdef GUI_DEBUG_TRACE_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

EXTERN void gui_debug_window_trace_logger(void);
EXTERN void gui_debug_trace_update(void);
EXTERN bool gui_debug_trace_start(void);
EXTERN void gui_debug_trace_stop(void);
EXTERN bool gui_debug_trace_is_running(void);
EXTERN const char* gui_debug_trace_get_disk_path(void);
EXTERN u64 gui_debug_trace_get_disk_bytes(void);
EXTERN void gui_debug_trace_format(const GT_Trace_Entry& entry, char* text, size_t size, bool registers, bool cycles);
EXTERN bool gui_debug_trace_save(const char* file_path);
EXTERN const char* gui_debug_trace_type_name(int type);

#undef GUI_DEBUG_TRACE_IMPORT
#undef EXTERN
#endif /* GUI_DEBUG_TRACE_H */
