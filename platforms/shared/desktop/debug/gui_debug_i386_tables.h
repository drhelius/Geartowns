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

#ifndef GUI_DEBUG_I386_TABLES_H
#define GUI_DEBUG_I386_TABLES_H

#include "geartowns.h"

#ifdef GUI_DEBUG_I386_TABLES_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

enum GuiDebugDescriptorTable
{
    GuiDebugDescriptorTable_GDT = 0,
    GuiDebugDescriptorTable_LDT,
    GuiDebugDescriptorTable_IDT
};

struct GuiDebugDescriptor
{
    bool readable;
    u32 address;
    u32 low;
    u32 high;
    u32 base;
    u32 limit;
    u8 access;
    u8 type;
    u8 dpl;
    bool present;
    bool system;
    bool gate;
    bool default32;
    bool granular;
    u16 gate_selector;
    u32 gate_offset;
    u8 gate_parameters;
};

EXTERN u32 gui_debug_i386_table_entry_count(GuiDebugDescriptorTable table);
EXTERN bool gui_debug_i386_read_table_entry(GuiDebugDescriptorTable table, u32 index, GuiDebugDescriptor& descriptor);
EXTERN bool gui_debug_i386_read_descriptor(u16 selector, GuiDebugDescriptor& descriptor);
EXTERN void gui_debug_i386_descriptor_type(const GuiDebugDescriptor& descriptor, char* text, size_t text_size);
EXTERN void gui_debug_i386_descriptor_flags(const GuiDebugDescriptor& descriptor, char* text, size_t text_size);
EXTERN void gui_debug_i386_vector_name(u8 vector, char* name, size_t name_size, char* description,
    size_t description_size);
EXTERN const char* gui_debug_i386_interrupt_function(u8 vector, u32 eax);
EXTERN u16 gui_debug_i386_segment_attributes(const GuiDebugDescriptor& descriptor);
EXTERN bool gui_debug_i386_selector_base(u16 selector, u32& base, u32& limit, char* reason, size_t reason_size);
EXTERN bool gui_debug_i386_read_page_entry(u32 table, u32 index, u32& entry);
EXTERN void gui_debug_i386_page_flags(u32 entry, char* text, size_t text_size);
EXTERN void gui_debug_window_descriptor_tables(void);
EXTERN void gui_debug_window_paging(void);

#undef GUI_DEBUG_I386_TABLES_IMPORT
#undef EXTERN
#endif /* GUI_DEBUG_I386_TABLES_H */
