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

#ifndef GUI_DEBUG_DISASSEMBLER_H
#define GUI_DEBUG_DISASSEMBLER_H

#include <map>
#include <string>
#include <vector>
#include "geartowns.h"

struct DisassemblerBookmark
{
    u32 address;
    char name[32];
};

#ifdef GUI_DEBUG_DISASSEMBLER_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

EXTERN void gui_debug_disassembler_init(void);
EXTERN void gui_debug_disassembler_destroy(void);
EXTERN void gui_debug_disassembler_reset(void);
EXTERN void gui_debug_reset_breakpoints(void);
EXTERN void gui_debug_toggle_breakpoint(void);
EXTERN void gui_debug_add_bookmark(void);
EXTERN void gui_debug_add_symbol(void);
EXTERN void gui_debug_runtocursor(void);
EXTERN void gui_debug_runto_address(u32 address);
EXTERN void gui_debug_go_back(void);
EXTERN void gui_debug_goto_address(u32 address);
EXTERN void gui_debug_add_disassembler_bookmark(u32 address, const char* name);
EXTERN bool gui_debug_remove_disassembler_bookmark(u32 address);
EXTERN void gui_debug_reset_disassembler_bookmarks(void);
EXTERN std::vector<DisassemblerBookmark>* gui_debug_get_disassembler_bookmarks(void);
EXTERN void gui_debug_window_disassembler(void);
EXTERN void gui_debug_window_call_stack(void);
EXTERN void gui_debug_window_breakpoints(void);
EXTERN void gui_debug_window_symbols(void);
EXTERN bool gui_debug_add_user_symbol(u32 linear, const char* name);
EXTERN bool gui_debug_remove_user_symbol(u32 linear);
EXTERN const char* gui_debug_get_user_symbol(u32 linear);
EXTERN const std::map<u32, std::string>& gui_debug_get_user_symbols(void);
EXTERN const char* gui_debug_get_symbol(u32 linear);
EXTERN int gui_debug_load_symbols(const char* file_path);

#undef GUI_DEBUG_DISASSEMBLER_IMPORT
#undef EXTERN
#endif /* GUI_DEBUG_DISASSEMBLER_H */
