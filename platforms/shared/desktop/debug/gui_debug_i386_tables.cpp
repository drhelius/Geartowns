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

#define GUI_DEBUG_I386_TABLES_IMPORT
#include "gui_debug_i386_tables.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "imgui.h"
#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../gui_colors.h"
#include "gui_debug_constants.h"
#include "gui_debug_disassembler.h"
#include "gui_debug_memory.h"
#include "system/pic.h"

static int selected_directory_entry = -1;

static bool read_linear32(I386* cpu, u32 address, u32& value);
static void decode_descriptor(u32 address, u32 low, u32 high, GuiDebugDescriptor& descriptor);
static void draw_descriptor_table(GuiDebugDescriptorTable table);
static void draw_interrupt_table(void);
static void draw_table_header(GuiDebugDescriptorTable table);
static int get_selector_references(u16 selector, bool local, char* text, size_t text_size);
static bool parse_translator_address(const char* text, GT_Debug_Memory_Address& address);
static void goto_linear(u32 address);
static void goto_physical(u32 address);

u32 gui_debug_i386_table_entry_count(GuiDebugDescriptorTable table)
{
    GeartownsCore* core = emu_get_core();

    if (!IsValidPointer(core))
        return 0;

    I386_State* state = core->GetI386()->GetState();

    switch (table)
    {
        case GuiDebugDescriptorTable_GDT:
            return ((u32)state->gdtr.limit + 1) / 8;
        case GuiDebugDescriptorTable_LDT:
            if ((state->ldtr.attributes & I386_SEGMENT_PRESENT) == 0 || (state->ldtr.selector & 0xFFFC) == 0)
                return 0;

            return MIN((state->ldtr.limit / 8) + 1, 8192U);
        default:
        {
            u32 size = state->execution_mode == I386_MODE_PROTECTED ? 8 : 4;
            return MIN(((u32)state->idtr.limit + 1) / size, 256U);
        }
    }
}

bool gui_debug_i386_read_table_entry(GuiDebugDescriptorTable table, u32 index, GuiDebugDescriptor& descriptor)
{
    memset(&descriptor, 0, sizeof(descriptor));
    GeartownsCore* core = emu_get_core();

    if (!IsValidPointer(core) || index >= gui_debug_i386_table_entry_count(table))
        return false;

    I386* cpu = core->GetI386();
    I386_State* state = cpu->GetState();

    if (table == GuiDebugDescriptorTable_IDT && state->execution_mode != I386_MODE_PROTECTED)
    {
        u32 address = state->idtr.base + index * 4;
        u32 vector = 0;

        descriptor.address = address;

        if (!read_linear32(cpu, address, vector))
            return false;

        descriptor.readable = true;
        descriptor.low = vector;
        descriptor.present = true;
        descriptor.gate = true;
        descriptor.gate_offset = vector & 0xFFFF;
        descriptor.gate_selector = (u16)(vector >> 16);
        descriptor.base = ((u32)descriptor.gate_selector << 4) + descriptor.gate_offset;
        return true;
    }

    u32 base = table == GuiDebugDescriptorTable_GDT ? state->gdtr.base :
        table == GuiDebugDescriptorTable_LDT ? state->ldtr.base : state->idtr.base;
    u32 address = base + index * 8;
    u32 low = 0;
    u32 high = 0;

    descriptor.address = address;

    if (!read_linear32(cpu, address, low) || !read_linear32(cpu, address + 4, high))
        return false;

    decode_descriptor(address, low, high, descriptor);
    return true;
}

bool gui_debug_i386_read_descriptor(u16 selector, GuiDebugDescriptor& descriptor)
{
    bool local = (selector & 0x0004) != 0;
    return gui_debug_i386_read_table_entry(local ? GuiDebugDescriptorTable_LDT : GuiDebugDescriptorTable_GDT,
        selector >> 3, descriptor);
}

void gui_debug_i386_descriptor_type(const GuiDebugDescriptor& descriptor, char* text, size_t text_size)
{
    static const char* k_system_types[16] =
    {
        "RESERVED", "TSS16 AVAIL", "LDT", "TSS16 BUSY", "CALL GATE16", "TASK GATE", "INT GATE16", "TRAP GATE16",
        "RESERVED", "TSS32 AVAIL", "RESERVED", "TSS32 BUSY", "CALL GATE32", "RESERVED", "INT GATE32", "TRAP GATE32"
    };

    if (descriptor.system)
        snprintf(text, text_size, "%s", k_system_types[descriptor.type & 0x0F]);
    else if ((descriptor.type & 0x08) != 0)
        snprintf(text, text_size, "CODE%s", descriptor.default32 ? "32" : "16");
    else
        snprintf(text, text_size, "DATA%s", descriptor.default32 ? "32" : "16");
}

void gui_debug_i386_descriptor_flags(const GuiDebugDescriptor& descriptor, char* text, size_t text_size)
{
    if (descriptor.system)
    {
        snprintf(text, text_size, "%c %c", descriptor.present ? 'P' : '-', descriptor.granular ? 'G' : '-');
        return;
    }

    bool code = (descriptor.type & 0x08) != 0;
    snprintf(text, text_size, "%c %c %c %c%c%c", descriptor.present ? 'P' : '-', descriptor.default32 ? 'D' : '-',
        descriptor.granular ? 'G' : '-', code ? ((descriptor.type & 0x04) ? 'C' : '-') : ((descriptor.type & 0x04) ? 'E' : '-'),
        code ? ((descriptor.type & 0x02) ? 'R' : '-') : ((descriptor.type & 0x02) ? 'W' : '-'),
        (descriptor.type & 0x01) ? 'A' : '-');
}

void gui_debug_i386_vector_name(u8 vector, char* name, size_t name_size, char* description, size_t description_size)
{
    GeartownsCore* core = emu_get_core();
    PIC* pic = IsValidPointer(core) ? core->GetPIC() : NULL;
    int irq = -1;

    if (IsValidPointer(pic))
    {
        u8 master = pic->GetMaster()->GetState()->icw2 & 0xF8;
        u8 slave = pic->GetSlave()->GetState()->icw2 & 0xF8;

        if (master >= 32 && (vector & 0xF8) == master)
            irq = vector & 7;
        else if (slave >= 32 && (vector & 0xF8) == slave)
            irq = 8 + (vector & 7);
    }

    if (vector < 32)
    {
        snprintf(name, name_size, "%s", k_debug_exception_names[vector]);
        snprintf(description, description_size, "%s", k_debug_exception_descriptions[vector]);
    }
    else if (irq >= 0)
    {
        snprintf(name, name_size, "IRQ%d", irq);
        snprintf(description, description_size, "IRQ%d %s", irq, k_debug_irq_sources[irq]);
    }
    else
    {
        for (int i = 0; i < k_debug_interrupt_name_count; i++)
        {
            if (k_debug_interrupt_names[i].vector == vector)
            {
                snprintf(name, name_size, "%s", k_debug_interrupt_names[i].label);
                snprintf(description, description_size, "%s", k_debug_interrupt_names[i].description);
                return;
            }
        }

        snprintf(name, name_size, "INT");
        snprintf(description, description_size, "Software interrupt");
    }
}

const char* gui_debug_i386_interrupt_function(u8 vector, u32 eax)
{
    u16 ax = (u16)eax;

    for (int i = 0; i < k_debug_interrupt_function_count; i++)
    {
        const stDebugInterruptFunction& function = k_debug_interrupt_functions[i];

        if (function.vector == vector && (ax & function.mask) == function.value)
            return function.name;
    }

    return NULL;
}

u16 gui_debug_i386_segment_attributes(const GuiDebugDescriptor& descriptor)
{
    u16 attributes = (u16)descriptor.type << I386_SEGMENT_TYPE_SHIFT;

    if (descriptor.present)
        attributes |= I386_SEGMENT_PRESENT;

    if (descriptor.system)
        attributes |= I386_SEGMENT_SYSTEM;

    if (descriptor.default32)
        attributes |= I386_SEGMENT_DEFAULT_32;

    if (descriptor.granular)
        attributes |= I386_SEGMENT_GRANULAR;

    if (descriptor.system)
        return attributes;

    if ((descriptor.type & 8) != 0)
    {
        attributes |= I386_SEGMENT_EXECUTABLE;

        if ((descriptor.type & 2) != 0)
            attributes |= I386_SEGMENT_READABLE;

        if ((descriptor.type & 4) != 0)
            attributes |= I386_SEGMENT_CONFORMING;
    }
    else
    {
        attributes |= I386_SEGMENT_READABLE;

        if ((descriptor.type & 2) != 0)
            attributes |= I386_SEGMENT_WRITABLE;

        if ((descriptor.type & 4) != 0)
            attributes |= I386_SEGMENT_EXPAND_DOWN;
    }

    if ((descriptor.type & 1) != 0)
        attributes |= I386_SEGMENT_ACCESSED;

    return attributes;
}

bool gui_debug_i386_selector_base(u16 selector, u32& base, u32& limit, char* reason, size_t reason_size)
{
    GeartownsCore* core = emu_get_core();

    if (!IsValidPointer(core))
        return false;

    I386_State* state = core->GetI386()->GetState();

    if (state->execution_mode != I386_MODE_PROTECTED)
    {
        base = (u32)selector << 4;
        limit = 0xFFFF;
        return true;
    }

    if ((selector & 0xFFFC) == 0)
    {
        snprintf(reason, reason_size, "Null selector");
        return false;
    }

    GuiDebugDescriptor descriptor;

    if (!gui_debug_i386_read_descriptor(selector, descriptor))
    {
        snprintf(reason, reason_size, "Selector %04X is outside the descriptor table or unreadable", selector);
        return false;
    }

    if (descriptor.system || !descriptor.present)
    {
        snprintf(reason, reason_size, "Selector %04X is not a present code or data segment", selector);
        return false;
    }

    base = descriptor.base;
    limit = descriptor.limit;
    return true;
}

static bool read_linear32(I386* cpu, u32 address, u32& value)
{
    value = 0;

    for (int i = 0; i < 4; i++)
    {
        u8 byte = 0;

        if (!cpu->TryPeekLinear(address + i, byte))
            return false;

        value |= (u32)byte << (i * 8);
    }

    return true;
}

static void decode_descriptor(u32 address, u32 low, u32 high, GuiDebugDescriptor& descriptor)
{
    descriptor.readable = true;
    descriptor.address = address;
    descriptor.low = low;
    descriptor.high = high;
    descriptor.access = (u8)(high >> 8);
    descriptor.type = descriptor.access & 0x0F;
    descriptor.dpl = (descriptor.access >> 5) & 0x03;
    descriptor.present = (descriptor.access & 0x80) != 0;
    descriptor.system = (descriptor.access & 0x10) == 0;
    descriptor.granular = (high & 0x00800000) != 0;
    descriptor.default32 = (high & 0x00400000) != 0;
    descriptor.base = (low >> 16) | ((high & 0xFF) << 16) | (high & 0xFF000000);

    u32 limit = (low & 0xFFFF) | (high & 0x000F0000);
    descriptor.limit = descriptor.granular ? (limit << 12) | 0xFFF : limit;

    if (!descriptor.system)
        return;

    switch (descriptor.type)
    {
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x0C:
        case 0x0E:
        case 0x0F:
            descriptor.gate = true;
            descriptor.gate_selector = (u16)(low >> 16);
            descriptor.gate_offset = (descriptor.type & 0x08) != 0 ? (low & 0xFFFF) | (high & 0xFFFF0000) : low & 0xFFFF;
            descriptor.gate_parameters = (u8)(high & 0x1F);
            descriptor.base = 0;
            descriptor.limit = 0;
            break;
        default:
            break;
    }
}

bool gui_debug_i386_read_page_entry(u32 table, u32 index, u32& entry)
{
    GT_Debug_Memory_Address address = { };
    address.space = GT_DEBUG_MEMORY_PHYSICAL;
    address.address = (table & 0xFFFFF000U) + (index & 0x3FF) * 4;
    address.segment_register = -1;

    u8 data[4];
    GT_Debug_Memory_Status status[4];
    gui_debug_memory_read(address, data, status, 4);

    for (int i = 0; i < 4; i++)
    {
        if (status[i] != GT_DEBUG_MEMORY_VALID && status[i] != GT_DEBUG_MEMORY_READ_ONLY)
            return false;
    }

    entry = read_u32_le(data);
    return true;
}

void gui_debug_i386_page_flags(u32 entry, char* text, size_t text_size)
{
    snprintf(text, text_size, "%c %c %c %c %c", (entry & 0x01) ? 'P' : '-', (entry & 0x02) ? 'W' : 'R',
        (entry & 0x04) ? 'U' : 'S', (entry & 0x20) ? 'A' : '-', (entry & 0x40) ? 'D' : '-');
}

void gui_debug_window_descriptor_tables(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(146, 102), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(389, 281), ImGuiCond_FirstUseEver);
    ImGui::Begin("Descriptor Tables", &config_debug.show_i386_descriptors);

    if (ImGui::BeginTabBar("##descriptor_tabs"))
    {
        if (ImGui::BeginTabItem("GDT"))
        {
            draw_descriptor_table(GuiDebugDescriptorTable_GDT);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("LDT"))
        {
            draw_descriptor_table(GuiDebugDescriptorTable_LDT);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("IDT"))
        {
            draw_interrupt_table();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_paging(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(187, 169), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(550, 478), ImGuiCond_FirstUseEver);
    ImGui::Begin("Paging", &config_debug.show_i386_paging);

    ImGui::PushFont(gui_default_font);

    I386_State* state = emu_get_core()->GetI386()->GetState();
    bool paging = (state->cr0 & 0x80000000U) != 0;
    u32 directory = state->cr3 & 0xFFFFF000U;
    ImVec4 color = paging ? white : gray;

    ImGui::TextColored(violet, "PAGING"); ImGui::SameLine();
    ImGui::TextColored(paging ? green : gray, "%s", paging ? "ON " : "OFF"); ImGui::SameLine();
    ImGui::TextColored(violet, "  CR3"); ImGui::SameLine();
    ImGui::TextColored(color, "%08X", state->cr3); ImGui::SameLine();
    ImGui::TextColored(violet, "  CR2"); ImGui::SameLine();
    ImGui::TextColored(color, "%08X", state->cr2);

    ImGui::NewLine(); ImGui::TextColored(cyan, "TRANSLATOR"); ImGui::Separator();

    static char input[32] = "";
    ImGui::PopFont();
    ImGui::PushItemWidth(160);
    ImGui::InputTextWithHint("##translate", "1234ABCD, DS:1234 or 0008:1234", input, IM_ARRAYSIZE(input),
        ImGuiInputTextFlags_CharsUppercase);
    ImGui::PopItemWidth();
    ImGui::PushFont(gui_default_font);

    GT_Debug_Memory_Address address;
    GT_Debug_Memory_Translation translation;
    memset(&translation, 0, sizeof(translation));
    bool parsed = input[0] != 0 && parse_translator_address(input, address);
    bool translated = parsed && gui_debug_memory_translate(address, translation) && translation.linear_valid;

    ImGui::TextColored(violet, "LINEAR  "); ImGui::SameLine();

    if (translated)
    {
        ImGui::TextColored(cyan, "%08X", translation.linear);

        if (ImGui::IsItemClicked())
            goto_linear(translation.linear);
    }
    else
        ImGui::TextColored(gray, "--      ");

    ImGui::SameLine();
    ImGui::TextColored(violet, " PDE"); ImGui::SameLine();

    if (translated && paging)
        ImGui::TextColored(white, "%03X %08X", translation.linear >> 22, translation.page_directory_entry);
    else
        ImGui::TextColored(gray, "--           ");

    ImGui::SameLine();
    ImGui::TextColored(violet, " PTE"); ImGui::SameLine();

    if (translated && paging && translation.physical_valid)
        ImGui::TextColored(white, "%03X %08X", (translation.linear >> 12) & 0x3FF, translation.page_table_entry);
    else
        ImGui::TextColored(gray, "--");

    ImGui::TextColored(violet, "PHYSICAL"); ImGui::SameLine();

    if (translated && translation.physical_valid)
    {
        ImGui::TextColored(cyan, "%08X", translation.physical);

        if (ImGui::IsItemClicked())
            goto_physical(translation.physical);

        ImGui::SameLine();
        ImGui::TextColored(green, "%s", translation.region_valid ? translation.region_name : "");
    }
    else if (translated)
        ImGui::TextColored(red, "%s", translation.reason);
    else
        ImGui::TextColored(gray, "--");

    ImGui::TextColored(violet, "FLAGS   "); ImGui::SameLine();

    if (translated && paging && translation.physical_valid)
    {
        char flags[16];
        gui_debug_i386_page_flags(translation.page_table_entry, flags, sizeof(flags));
        ImGui::TextColored(orange, "%s", flags);
    }
    else
        ImGui::TextColored(gray, "--");

    ImGui::NewLine(); ImGui::TextColored(cyan, "PAGE DIRECTORY"); ImGui::Separator();

    ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingFixedFit;
    float height = (ImGui::GetContentRegionAvail().y - 4.0f) * 0.5f;

    if (ImGui::BeginTable("##page_directory", 5, flags, ImVec2(0, height)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("INDEX");
        ImGui::TableSetupColumn("LINEAR RANGE");
        ImGui::TableSetupColumn("PDE");
        ImGui::TableSetupColumn("TABLE");
        ImGui::TableSetupColumn("FLAGS");
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(1024);

        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                u32 entry = 0;
                bool readable = paging && gui_debug_i386_read_page_entry(directory, (u32)i, entry);
                bool present = readable && (entry & 1) != 0;
                ImVec4 row = present ? white : gray;
                char label[16];
                char page_flags[16];
                snprintf(label, sizeof(label), "%03X##pde%d", i, i);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                if (ImGui::Selectable(label, selected_directory_entry == i, ImGuiSelectableFlags_SpanAllColumns))
                    selected_directory_entry = i;

                ImGui::TableNextColumn();
                ImGui::TextColored(row, "%08X-%08X", (u32)i << 22, ((u32)i << 22) | 0x3FFFFF);
                ImGui::TableNextColumn();

                if (readable)
                    ImGui::TextColored(row, "%08X", entry);
                else
                    ImGui::TextColored(gray, "--      ");

                ImGui::TableNextColumn();

                if (present)
                    ImGui::TextColored(cyan, "%08X", entry & 0xFFFFF000U);
                else
                    ImGui::TextColored(gray, "--      ");

                ImGui::TableNextColumn();
                gui_debug_i386_page_flags(entry, page_flags, sizeof(page_flags));
                ImGui::TextColored(present ? orange : gray, "%s", readable ? page_flags : "--");
            }
        }

        ImGui::EndTable();
    }

    u32 directory_entry = 0;
    bool table_present = paging && selected_directory_entry >= 0 &&
        gui_debug_i386_read_page_entry(directory, (u32)selected_directory_entry, directory_entry) && (directory_entry & 1) != 0;

    if (ImGui::BeginTable("##page_table", 4, flags, ImVec2(0, 0)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("INDEX");
        ImGui::TableSetupColumn("LINEAR");
        ImGui::TableSetupColumn("PHYSICAL");
        ImGui::TableSetupColumn("FLAGS");
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(table_present ? 1024 : 0);

        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                u32 entry = 0;
                bool readable = gui_debug_i386_read_page_entry(directory_entry, (u32)i, entry);
                bool present = readable && (entry & 1) != 0;
                u32 linear = ((u32)selected_directory_entry << 22) | ((u32)i << 12);
                char page_flags[16];
                gui_debug_i386_page_flags(entry, page_flags, sizeof(page_flags));

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(present ? white : gray, "%03X", i);
                ImGui::TableNextColumn();
                ImGui::TextColored(present ? cyan : gray, "%08X", linear);

                if (present && ImGui::IsItemClicked())
                    goto_linear(linear);

                ImGui::TableNextColumn();

                if (present)
                {
                    ImGui::TextColored(cyan, "%08X", entry & 0xFFFFF000U);

                    if (ImGui::IsItemClicked())
                        goto_physical(entry & 0xFFFFF000U);
                }
                else
                    ImGui::TextColored(gray, "--      ");

                ImGui::TableNextColumn();
                ImGui::TextColored(present ? orange : gray, "%s", readable ? page_flags : "--");
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

static void draw_table_header(GuiDebugDescriptorTable table)
{
    I386_State* state = emu_get_core()->GetI386()->GetState();
    u32 base = table == GuiDebugDescriptorTable_GDT ? state->gdtr.base :
        table == GuiDebugDescriptorTable_LDT ? state->ldtr.base : state->idtr.base;
    u32 limit = table == GuiDebugDescriptorTable_GDT ? state->gdtr.limit :
        table == GuiDebugDescriptorTable_LDT ? state->ldtr.limit : state->idtr.limit;
    bool present = table != GuiDebugDescriptorTable_LDT || gui_debug_i386_table_entry_count(table) > 0;

    ImGui::TextColored(violet, "BASE"); ImGui::SameLine();

    if (present)
    {
        ImGui::TextColored(cyan, "%08X", base);

        if (ImGui::IsItemClicked())
            goto_linear(base);
    }
    else
        ImGui::TextColored(gray, "--      ");

    ImGui::SameLine();
    ImGui::TextColored(violet, " LIMIT"); ImGui::SameLine();
    ImGui::TextColored(present ? white : gray, "%08X", present ? limit : 0); ImGui::SameLine();
    ImGui::TextColored(violet, " ENTRIES"); ImGui::SameLine();
    ImGui::TextColored(present ? white : gray, "%u", gui_debug_i386_table_entry_count(table));

    if (table == GuiDebugDescriptorTable_LDT)
    {
        ImGui::SameLine();
        ImGui::TextColored(violet, " LDTR"); ImGui::SameLine();
        ImGui::TextColored(white, "%04X", state->ldtr.selector);
    }
}

static void draw_descriptor_table(GuiDebugDescriptorTable table)
{
    ImGui::PushFont(gui_default_font);
    draw_table_header(table);
    ImGui::Separator();

    ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("##descriptors", 7, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("INDEX");
        ImGui::TableSetupColumn("SEL");
        ImGui::TableSetupColumn("BASE");
        ImGui::TableSetupColumn("LIMIT");
        ImGui::TableSetupColumn("TYPE");
        ImGui::TableSetupColumn("DPL");
        ImGui::TableSetupColumn("FLAGS");
        ImGui::TableHeadersRow();

        bool local = table == GuiDebugDescriptorTable_LDT;
        ImGuiListClipper clipper;
        clipper.Begin((int)gui_debug_i386_table_entry_count(table));

        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                GuiDebugDescriptor descriptor;
                bool readable = gui_debug_i386_read_table_entry(table, (u32)i, descriptor);
                u16 selector = (u16)((i << 3) | (local ? 4 : 0));
                char references[64];
                int reference_count = get_selector_references(selector, local, references, sizeof(references));
                bool used = readable && descriptor.present;
                ImVec4 color = used ? white : gray;

                ImGui::TableNextRow();

                if (reference_count > 0)
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(dark_blue));

                ImGui::TableNextColumn();
                ImGui::TextColored(orange, "%04X", i);

                if (reference_count > 0 && ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", references);

                ImGui::TableNextColumn();
                ImGui::TextColored(color, "%04X", selector);

                if (!readable)
                {
                    ImGui::TableNextColumn();
                    ImGui::TextColored(gray, "UNAVAILABLE");
                    continue;
                }

                char type[16];
                char descriptor_flags[16];
                gui_debug_i386_descriptor_type(descriptor, type, sizeof(type));
                gui_debug_i386_descriptor_flags(descriptor, descriptor_flags, sizeof(descriptor_flags));

                ImGui::TableNextColumn();

                if (descriptor.gate)
                    ImGui::TextColored(color, "%04X:%08X", descriptor.gate_selector, descriptor.gate_offset);
                else
                {
                    ImGui::TextColored(used ? cyan : gray, "%08X", descriptor.base);

                    if (used && ImGui::IsItemClicked())
                        goto_linear(descriptor.base);
                }

                ImGui::TableNextColumn();

                if (descriptor.gate)
                    ImGui::TextColored(gray, "--      ");
                else
                    ImGui::TextColored(color, "%08X", descriptor.limit);

                ImGui::TableNextColumn();
                ImGui::TextColored(used ? blue : gray, "%-11s", (descriptor.low | descriptor.high) == 0 ? "NULL" : type);
                ImGui::TableNextColumn();
                ImGui::TextColored(color, "%d", descriptor.dpl);
                ImGui::TableNextColumn();
                ImGui::TextColored(used ? green : gray, "%s", descriptor_flags);
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();
}

static void draw_interrupt_table(void)
{
    ImGui::PushFont(gui_default_font);
    draw_table_header(GuiDebugDescriptorTable_IDT);
    ImGui::Separator();

    I386_State* state = emu_get_core()->GetI386()->GetState();
    bool protected_mode = state->execution_mode == I386_MODE_PROTECTED;
    ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("##interrupts", protected_mode ? 7 : 4, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("VEC");
        ImGui::TableSetupColumn("NAME");

        if (protected_mode)
        {
            ImGui::TableSetupColumn("GATE");
            ImGui::TableSetupColumn("TARGET");
            ImGui::TableSetupColumn("DPL");
            ImGui::TableSetupColumn("P");
            ImGui::TableSetupColumn("SYMBOL");
        }
        else
        {
            ImGui::TableSetupColumn("SEG:OFF");
            ImGui::TableSetupColumn("LINEAR");
        }

        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin((int)gui_debug_i386_table_entry_count(GuiDebugDescriptorTable_IDT));

        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                GuiDebugDescriptor descriptor;
                bool readable = gui_debug_i386_read_table_entry(GuiDebugDescriptorTable_IDT, (u32)i, descriptor);
                char name[16];
                char description[64];
                gui_debug_i386_vector_name((u8)i, name, sizeof(name), description, sizeof(description));

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(orange, "%02X", i);
                ImGui::TableNextColumn();
                ImGui::TextColored(i < 32 ? red : (name[0] == 'I' && name[1] == 'R') ? yellow : white, "%-5s", name);

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", description);

                ImGui::TableNextColumn();

                if (!readable)
                {
                    ImGui::TextColored(gray, "UNAVAILABLE");
                    continue;
                }

                if (!protected_mode)
                {
                    ImGui::TextColored(white, "%04X:%04X", descriptor.gate_selector, descriptor.gate_offset);
                    ImGui::TableNextColumn();
                    ImGui::TextColored(cyan, "%08X", descriptor.base);

                    if (ImGui::IsItemClicked())
                        gui_debug_goto_address(descriptor.base);

                    continue;
                }

                bool gate = descriptor.gate && descriptor.present;
                const char* gate_name = !descriptor.gate ? "--" : descriptor.type == 0x05 ? "TASK" :
                    (descriptor.type & 0x01) != 0 ? ((descriptor.type & 0x08) ? "TRAP32" : "TRAP16") :
                    ((descriptor.type & 0x08) ? "INT32" : "INT16");

                ImGui::TextColored(gate ? blue : gray, "%-6s", gate_name);
                ImGui::TableNextColumn();

                u32 base = 0;
                u32 limit = 0;
                char reason[GT_DEBUG_MEMORY_REASON_SIZE];
                bool target = gate && descriptor.type != 0x05 &&
                    gui_debug_i386_selector_base(descriptor.gate_selector, base, limit, reason, sizeof(reason));

                ImGui::TextColored(gate ? (target ? cyan : white) : gray, "%04X:%08X", descriptor.gate_selector,
                    descriptor.gate_offset);

                if (target && ImGui::IsItemClicked())
                    gui_debug_goto_address(base + descriptor.gate_offset);

                ImGui::TableNextColumn();
                ImGui::TextColored(gate ? white : gray, "%d", descriptor.dpl);
                ImGui::TableNextColumn();
                ImGui::TextColored(descriptor.present ? green : gray, "%s", descriptor.present ? "P" : "-");
                ImGui::TableNextColumn();

                const char* symbol = target ? gui_debug_get_symbol(base + descriptor.gate_offset) : NULL;
                ImGui::TextColored(green, "%s", IsValidPointer(symbol) ? symbol : "");
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();
}

static int get_selector_references(u16 selector, bool local, char* text, size_t text_size)
{
    static const char* k_registers[I386_SEGMENT_COUNT] = { "ES", "CS", "SS", "DS", "FS", "GS" };
    I386_State* state = emu_get_core()->GetI386()->GetState();
    int count = 0;
    text[0] = 0;

    if (state->execution_mode != I386_MODE_PROTECTED || (selector & 0xFFF8) == 0)
        return 0;

    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
    {
        if ((state->segments[i].selector & 0xFFFC) == selector)
        {
            size_t length = strlen(text);
            snprintf(text + length, text_size - length, "%s%s", count > 0 ? " " : "", k_registers[i]);
            count++;
        }
    }

    if (!local && (state->ldtr.selector & 0xFFFC) == selector)
    {
        size_t length = strlen(text);
        snprintf(text + length, text_size - length, "%sLDTR", count > 0 ? " " : "");
        count++;
    }

    if (!local && (state->task_register.selector & 0xFFFC) == selector)
    {
        size_t length = strlen(text);
        snprintf(text + length, text_size - length, "%sTR", count > 0 ? " " : "");
        count++;
    }

    return count;
}

static bool parse_translator_address(const char* text, GT_Debug_Memory_Address& address)
{
    static const char* k_registers[I386_SEGMENT_COUNT] = { "ES", "CS", "SS", "DS", "FS", "GS" };
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%s", text);
    char* colon = strchr(buffer, ':');
    char* end = NULL;

    memset(&address, 0, sizeof(address));
    address.segment_register = -1;

    if (!IsValidPointer(colon))
    {
        unsigned long value = strtoul(buffer, &end, 16);

        if (end == buffer || *end != 0)
            return false;

        address.space = GT_DEBUG_MEMORY_LINEAR;
        address.address = (u32)value;
        return true;
    }

    *colon = 0;
    unsigned long offset = strtoul(colon + 1, &end, 16);

    if (end == colon + 1 || *end != 0)
        return false;

    address.space = GT_DEBUG_MEMORY_LOGICAL;
    address.address = (u32)offset;

    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
    {
        if (strcmp(buffer, k_registers[i]) == 0)
        {
            address.segment_register = (s8)i;
            address.segment = emu_get_core()->GetI386()->GetState()->segments[i].selector;
            return true;
        }
    }

    unsigned long selector = strtoul(buffer, &end, 16);

    if (end == buffer || *end != 0 || selector > 0xFFFF)
        return false;

    address.segment = (u16)selector;
    return true;
}

static void goto_linear(u32 address)
{
    GT_Debug_Memory_Address target = { };
    target.space = GT_DEBUG_MEMORY_LINEAR;
    target.address = address;
    target.segment_register = -1;
    gui_debug_memory_goto(target);
}

static void goto_physical(u32 address)
{
    GT_Debug_Memory_Address target = { };
    target.space = GT_DEBUG_MEMORY_PHYSICAL;
    target.address = address;
    target.segment_register = -1;
    gui_debug_memory_goto(target);
}
