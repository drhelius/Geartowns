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

#ifndef GUI_DEBUG_CDROM_H
#define GUI_DEBUG_CDROM_H

#include "geartowns.h"

#ifdef GUI_DEBUG_CDROM_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

EXTERN void gui_debug_window_cdrom(void);
EXTERN void gui_debug_window_cdrom_toc(void);
EXTERN const char* gui_debug_cdrom_drive_state(void);
EXTERN bool gui_debug_cdrom_reading(void);
EXTERN u32 gui_debug_cdrom_head(void);

#undef GUI_DEBUG_CDROM_IMPORT
#undef EXTERN
#endif /* GUI_DEBUG_CDROM_H */
