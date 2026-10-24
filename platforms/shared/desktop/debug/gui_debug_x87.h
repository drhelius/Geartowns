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

#ifndef GUI_DEBUG_X87_H
#define GUI_DEBUG_X87_H

#include <stddef.h>
#include "geartowns.h"

#ifdef GUI_DEBUG_X87_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

EXTERN void gui_debug_window_x87(void);
EXTERN void gui_debug_x87_format_value(u16 sign_exponent, u64 significand, char* text, size_t text_size);
EXTERN const char* gui_debug_x87_tag_name(int tag);
EXTERN const char* gui_debug_x87_precision_name(u16 control);
EXTERN const char* gui_debug_x87_rounding_name(u16 control);

#undef GUI_DEBUG_X87_IMPORT
#undef EXTERN
#endif /* GUI_DEBUG_X87_H */
