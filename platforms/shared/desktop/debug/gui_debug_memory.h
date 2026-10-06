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
#include <vector>
#include "geartowns.h"

#define GUI_DEBUG_MEMORY_AREA_LINEAR 0
#define GUI_DEBUG_MEMORY_AREA_PHYSICAL 1
#define GUI_DEBUG_MEMORY_AREA_IO 2
#define GUI_DEBUG_MEMORY_AREA_REGIONS 3

struct GuiDebugMemoryArea
{
    int id;
    GT_Debug_Memory_Address source;
    char name[GT_DEBUG_MEMORY_REGION_NAME_SIZE];
    u64 size;
    u32 flags;
    u32 physical_base;
};

struct GuiDebugMemoryBookmark
{
    GT_Debug_Memory_Address address;
    u32 end;
    char name[64];
};

struct GuiDebugMemoryWatch
{
    GT_Debug_Memory_Address address;
    char name[64];
    int size;
    u64 value;
    bool valid;
    bool freeze;
};

struct GuiDebugMemorySearchResult
{
    u32 address;
    u64 current;
    u64 previous;
};

void gui_debug_memory_init(void);
void gui_debug_memory_destroy(void);
void gui_debug_memory_reset(void);
void gui_debug_memory_update(void);
void gui_debug_memory_goto(const GT_Debug_Memory_Address& address);
void gui_debug_window_memory(void);
void gui_debug_memory_auxiliary_windows(void);
void gui_debug_memory_copy(void);
void gui_debug_memory_paste(void);
void gui_debug_memory_save_dump(const char* file_path);
void gui_debug_memory_load_dump(const char* file_path);
void gui_debug_memory_save_settings(std::ostream& stream);
bool gui_debug_memory_load_settings(std::istream& stream);

int gui_debug_memory_get_area_count(void);
bool gui_debug_memory_get_area(int id, GuiDebugMemoryArea& area);
bool gui_debug_memory_same_source(const GT_Debug_Memory_Address& a, const GT_Debug_Memory_Address& b);
void gui_debug_memory_read(const GT_Debug_Memory_Address& address, u8* data, GT_Debug_Memory_Status* status, u32 size);
bool gui_debug_memory_write(const GT_Debug_Memory_Address& address, const u8* data, u32 size);
bool gui_debug_memory_translate(const GT_Debug_Memory_Address& address, GT_Debug_Memory_Translation& translation);
bool gui_debug_memory_select_range(const GT_Debug_Memory_Address& source, u32 start, u32 end);
bool gui_debug_memory_get_selection(const GT_Debug_Memory_Address& source, u32& start, u32& end);
int gui_debug_memory_set_selection_value(const GT_Debug_Memory_Address& source, u8 value);
void gui_debug_memory_add_bookmark(const GT_Debug_Memory_Address& address, u32 end, const char* name);
bool gui_debug_memory_remove_bookmark(const GT_Debug_Memory_Address& address);
void gui_debug_memory_get_bookmarks(const GT_Debug_Memory_Address& source, std::vector<GuiDebugMemoryBookmark>& bookmarks);
bool gui_debug_memory_add_watch(const GT_Debug_Memory_Address& address, const char* name, int size);
bool gui_debug_memory_remove_watch(const GT_Debug_Memory_Address& address);
void gui_debug_memory_get_watches(const GT_Debug_Memory_Address& source, std::vector<GuiDebugMemoryWatch>& watches);
bool gui_debug_memory_search_capture(const GT_Debug_Memory_Address& source, u32 start, u32 size, int width);
int gui_debug_memory_search(const GT_Debug_Memory_Address& source, int comparison, bool previous, u64 value,
    bool signed_values, std::vector<GuiDebugMemorySearchResult>& results);
int gui_debug_memory_find(const GT_Debug_Memory_Address& source, u32 start, u32 size, const std::vector<u8>& pattern,
    bool case_sensitive, std::vector<u32>& addresses, int max);

#endif /* GUI_DEBUG_MEMORY_H */
