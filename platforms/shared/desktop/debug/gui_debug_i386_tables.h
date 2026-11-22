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

enum I386RegId
{
    I386RegId_EAX = 1,
    I386RegId_ECX,
    I386RegId_EDX,
    I386RegId_EBX,
    I386RegId_ESP,
    I386RegId_EBP,
    I386RegId_ESI,
    I386RegId_EDI,

    I386RegId_EIP,
    I386RegId_EFLAGS,

    I386RegId_CR0,
    I386RegId_CR2,
    I386RegId_CR3,
    I386RegId_CR4,

    I386RegId_DR0,
    I386RegId_DR1,
    I386RegId_DR2,
    I386RegId_DR3,
    I386RegId_DR6,
    I386RegId_DR7,

    I386RegId_TR3,
    I386RegId_TR4,
    I386RegId_TR5,
    I386RegId_TR6,
    I386RegId_TR7,

    I386RegId_GDTRBase,
    I386RegId_IDTRBase,
    I386RegId_LDTRBase,
    I386RegId_LDTRLimit,
    I386RegId_TRBase,
    I386RegId_TRLimit,
    I386RegId_GDTRLimit,
    I386RegId_IDTRLimit,
    I386RegId_LDTRSelector,
    I386RegId_LDTRAccess,
    I386RegId_LDTRDpl,
    I386RegId_TRSelector,
    I386RegId_TRAccess,
    I386RegId_TRDpl,
    I386RegId_DR4,
    I386RegId_DR5,

    I386RegId_SegmentSelectorBase = 0x100,
    I386RegId_SegmentBaseBase = 0x110,
    I386RegId_SegmentLimitBase = 0x120,
    I386RegId_SegmentAccessBase = 0x130,
    I386RegId_SegmentDplBase = 0x140
};

enum GuiDebugAddressError
{
    GuiDebugAddressError_None = 0,
    GuiDebugAddressError_Value,
    GuiDebugAddressError_Offset,
    GuiDebugAddressError_Prefix
};

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
    size_t description_size, bool pic_bases = true);
EXTERN const char* gui_debug_i386_interrupt_function(u8 vector, u32 eax);
EXTERN u16 gui_debug_i386_segment_attributes(const GuiDebugDescriptor& descriptor);
EXTERN bool gui_debug_i386_selector_base(u16 selector, u32& base, u32& limit, char* reason, size_t reason_size);
EXTERN bool gui_debug_i386_descriptor_base(u16 selector, u32& base, u32& limit, char* reason, size_t reason_size);
EXTERN bool gui_debug_i386_idt_is_ivt(void);
EXTERN bool gui_debug_parse_hex(const char* text, u32& value);
EXTERN bool gui_debug_parse_address_prefix(const char* prefix, bool spaces, GT_Debug_Memory_Address& address);
EXTERN GuiDebugAddressError gui_debug_parse_address(const char* text, bool spaces, GT_Debug_Memory_Address& address);
EXTERN bool gui_debug_i386_parse_linear(const char* text, u32& linear);
EXTERN bool gui_debug_i386_load_segment(const I386_State& state, int index, u32 selector, I386_Segment& segment,
    char* error, size_t error_size);
EXTERN bool gui_debug_i386_load_system_segment(const I386_State& state, bool task, u32 selector, I386_Segment& segment,
    char* error, size_t error_size);
EXTERN bool gui_debug_i386_write_register(u16 reg_id, u32 value, char* error, size_t error_size);
EXTERN u8 gui_debug_i386_access_byte(const I386_Segment& segment);
EXTERN u16 gui_debug_i386_access_attributes(u8 access, u16 attributes);
EXTERN bool gui_debug_i386_read_page_entry(u32 table, u32 index, u32& entry);
EXTERN void gui_debug_i386_page_flags(u32 entry, char* text, size_t text_size);
EXTERN void gui_debug_window_descriptor_tables(void);
EXTERN void gui_debug_window_paging(void);

#undef GUI_DEBUG_I386_TABLES_IMPORT
#undef EXTERN
#endif /* GUI_DEBUG_I386_TABLES_H */
