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

#define GUI_DEBUG_I386_IMPORT
#include "gui_debug_i386.h"

#include <stdio.h>
#include <string.h>
#include "imgui.h"
#include "geartowns.h"
#include "../gui_colors.h"
#include "gui_debug_memory.h"
#include "gui_debug_widgets.h"
#include "../gui.h"
#include "../config.h"
#include "../emu.h"
#include "../utils.h"

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

    I386RegId_DR0,
    I386RegId_DR1,
    I386RegId_DR2,
    I386RegId_DR3,
    I386RegId_DR6,
    I386RegId_DR7,

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

    I386RegId_SegmentSelectorBase = 0x100,
    I386RegId_SegmentBaseBase = 0x110,
    I386RegId_SegmentLimitBase = 0x120,
    I386RegId_SegmentAccessBase = 0x130,
    I386RegId_SegmentDplBase = 0x140
};

struct I386FlagDefinition
{
    const char* name;
    u8 bit;
};

struct I386RegisterDefinition
{
    const char* name;
    u16 id;
    int index;
};

static const I386FlagDefinition k_eflags[] =
{
    { "VM", 17 }, { "RF", 16 }, { "NT", 14 }, { "I1", 13 }, { "I0", 12 }, { "OF", 11 }, { "DF", 10 },
    { "IF", 9 }, { "TF", 8 }, { "SF", 7 }, { "ZF", 6 }, { "AF", 4 }, { "PF", 2 }, { "CF", 0 }
};

static const I386FlagDefinition k_cr0_flags[] =
{
    { "PG", 31 }, { "ET", 4 }, { "TS", 3 }, { "EM", 2 }, { "MP", 1 }, { "PE", 0 }
};

static const I386RegisterDefinition k_registers[] =
{
    { "EAX", I386RegId_EAX, I386_REG_EAX },
    { "EBX", I386RegId_EBX, I386_REG_EBX },
    { "ECX", I386RegId_ECX, I386_REG_ECX },
    { "EDX", I386RegId_EDX, I386_REG_EDX },
    { "ESI", I386RegId_ESI, I386_REG_ESI },
    { "EDI", I386RegId_EDI, I386_REG_EDI },
    { "EBP", I386RegId_EBP, I386_REG_EBP },
    { "ESP", I386RegId_ESP, I386_REG_ESP }
};

static const int k_segment_order[I386_SEGMENT_COUNT] =
{
    I386_SEGMENT_CS, I386_SEGMENT_DS, I386_SEGMENT_ES,
    I386_SEGMENT_SS, I386_SEGMENT_FS, I386_SEGMENT_GS
};

static const char* k_segment_names[I386_SEGMENT_COUNT] =
{
    "ES", "CS", "SS", "DS", "FS", "GS"
};

static void write_state(I386* cpu, I386_State& state, bool pc_changed)
{
    state.repeat.active = false;
    cpu->SetState(state);

    if (pc_changed)
        emu_debug_pc_changed = true;
}

static void I386WriteCallback1(u16 reg_id, u8 bit_index, bool value, void* user_data)
{
    I386* cpu = (I386*)user_data;
    I386_State state;
    u32* reg = NULL;

    cpu->CopyState(state);

    if (reg_id == I386RegId_EFLAGS)
        reg = &state.eflags;
    else if (reg_id == I386RegId_CR0)
        reg = &state.cr0;

    if (!IsValidPointer(reg))
        return;

    if (value)
        *reg |= 1U << bit_index;
    else
        *reg &= ~(1U << bit_index);

    state.eflags |= I386_FLAG_FIXED;
    write_state(cpu, state, reg_id == I386RegId_CR0 || (reg_id == I386RegId_EFLAGS && bit_index == 17));
}

static void I386WriteCallback8(u16 reg_id, u8 value, void* user_data)
{
    I386* cpu = (I386*)user_data;
    I386_State state;

    cpu->CopyState(state);

    if (reg_id >= I386RegId_SegmentDplBase && reg_id < I386RegId_SegmentDplBase + I386_SEGMENT_COUNT)
    {
        int segment = reg_id - I386RegId_SegmentDplBase;
        state.segments[segment].dpl = value & 3;
    }
    else if (reg_id == I386RegId_LDTRDpl)
        state.ldtr.dpl = value & 3;
    else if (reg_id == I386RegId_TRDpl)
        state.task_register.dpl = value & 3;
    else
        return;

    write_state(cpu, state, false);
}

static void I386WriteCallback16(u16 reg_id, u16 value, void* user_data)
{
    I386* cpu = (I386*)user_data;
    I386_State state;
    bool pc_changed = false;

    cpu->CopyState(state);

    if (reg_id >= I386RegId_SegmentSelectorBase && reg_id < I386RegId_SegmentSelectorBase + I386_SEGMENT_COUNT)
    {
        int segment = reg_id - I386RegId_SegmentSelectorBase;
        state.segments[segment].selector = value;

        if (state.execution_mode == I386_MODE_REAL || state.execution_mode == I386_MODE_VM86)
        {
            state.segments[segment].base = (u32)value << 4;
            state.segments[segment].limit = 0xFFFF;
        }

        pc_changed = segment == I386_SEGMENT_CS;
    }
    else if (reg_id >= I386RegId_SegmentAccessBase && reg_id < I386RegId_SegmentAccessBase + I386_SEGMENT_COUNT)
    {
        int segment = reg_id - I386RegId_SegmentAccessBase;
        state.segments[segment].attributes = value;
        pc_changed = segment == I386_SEGMENT_CS;
    }
    else
    {
        switch (reg_id)
        {
            case I386RegId_GDTRLimit: state.gdtr.limit = value; break;
            case I386RegId_IDTRLimit: state.idtr.limit = value; break;
            case I386RegId_LDTRSelector: state.ldtr.selector = value; break;
            case I386RegId_LDTRAccess: state.ldtr.attributes = value; break;
            case I386RegId_TRSelector: state.task_register.selector = value; break;
            case I386RegId_TRAccess: state.task_register.attributes = value; break;
            default: return;
        }
    }

    write_state(cpu, state, pc_changed);
}

static void I386WriteCallback32(u16 reg_id, u32 value, void* user_data)
{
    I386* cpu = (I386*)user_data;
    I386_State state;
    bool pc_changed = false;

    cpu->CopyState(state);

    if (reg_id >= I386RegId_SegmentBaseBase && reg_id < I386RegId_SegmentBaseBase + I386_SEGMENT_COUNT)
    {
        int segment = reg_id - I386RegId_SegmentBaseBase;
        state.segments[segment].base = value;
        pc_changed = segment == I386_SEGMENT_CS;
    }
    else if (reg_id >= I386RegId_SegmentLimitBase && reg_id < I386RegId_SegmentLimitBase + I386_SEGMENT_COUNT)
    {
        int segment = reg_id - I386RegId_SegmentLimitBase;
        state.segments[segment].limit = value;
        pc_changed = segment == I386_SEGMENT_CS;
    }
    else
    {
        switch (reg_id)
        {
            case I386RegId_EAX: state.registers[I386_REG_EAX].value = value; break;
            case I386RegId_ECX: state.registers[I386_REG_ECX].value = value; break;
            case I386RegId_EDX: state.registers[I386_REG_EDX].value = value; break;
            case I386RegId_EBX: state.registers[I386_REG_EBX].value = value; break;
            case I386RegId_ESP: state.registers[I386_REG_ESP].value = value; break;
            case I386RegId_EBP: state.registers[I386_REG_EBP].value = value; break;
            case I386RegId_ESI: state.registers[I386_REG_ESI].value = value; break;
            case I386RegId_EDI: state.registers[I386_REG_EDI].value = value; break;
            case I386RegId_EIP:
                state.eip = value;
                state.halted = false;
                pc_changed = true;
                break;
            case I386RegId_EFLAGS:
                state.eflags = (value & 0x00037FD5U) | I386_FLAG_FIXED;
                pc_changed = true;
                break;
            case I386RegId_CR0:
                state.cr0 = value;
                pc_changed = true;
                break;
            case I386RegId_CR2: state.cr2 = value; break;
            case I386RegId_CR3:
                state.cr3 = value;
                pc_changed = true;
                break;
            case I386RegId_DR0: state.debug_registers[0] = value; break;
            case I386RegId_DR1: state.debug_registers[1] = value; break;
            case I386RegId_DR2: state.debug_registers[2] = value; break;
            case I386RegId_DR3: state.debug_registers[3] = value; break;
            case I386RegId_DR6: state.debug_registers[6] = value; break;
            case I386RegId_DR7: state.debug_registers[7] = value; break;
            case I386RegId_TR6: state.test_registers[0] = value; break;
            case I386RegId_TR7: state.test_registers[1] = value; break;
            case I386RegId_GDTRBase: state.gdtr.base = value; break;
            case I386RegId_IDTRBase: state.idtr.base = value; break;
            case I386RegId_LDTRBase: state.ldtr.base = value; break;
            case I386RegId_LDTRLimit: state.ldtr.limit = value; break;
            case I386RegId_TRBase: state.task_register.base = value; break;
            case I386RegId_TRLimit: state.task_register.limit = value; break;
            default: return;
        }
    }

    write_state(cpu, state, pc_changed);
}

static void draw_binary32(u32 value)
{
    ImGui::TextColored(gray, BYTE_TO_BINARY_PATTERN_SPACED " " BYTE_TO_BINARY_PATTERN_SPACED " "
        BYTE_TO_BINARY_PATTERN_SPACED " " BYTE_TO_BINARY_PATTERN_SPACED,
        BYTE_TO_BINARY((value >> 24) & 0xFF), BYTE_TO_BINARY((value >> 16) & 0xFF),
        BYTE_TO_BINARY((value >> 8) & 0xFF), BYTE_TO_BINARY(value & 0xFF));
}

static void draw_register_tooltip(const char* name, u32 value)
{
    char ascii[5];

    for (int i = 0; i < 4; i++)
    {
        u8 character = (u8)(value >> ((3 - i) * 8));
        ascii[i] = character >= 32 && character < 127 ? (char)character : '.';
    }

    ascii[4] = 0;

    ImGui::BeginTooltip();
    ImGui::TextColored(cyan, "%s", name);
    ImGui::TextColored(cyan, "Hex: $%08X", value);
    ImGui::TextColored(cyan, "Dec: %u (%d)", value, (s32)value);
    ImGui::TextColored(cyan, "ASCII: %s", ascii);
    draw_binary32(value);
    ImGui::EndTooltip();
}

static void draw_register(I386* cpu, const char* name, u16 reg_id, u32 value, ImVec4 color)
{
    ImGui::TableNextColumn();
    ImGui::BeginGroup();
    ImGui::TextColored(color, " %s", name);
    ImGui::SameLine();
    EditableRegister32(NULL, NULL, reg_id, value, I386WriteCallback32, cpu, EditableRegisterFlags_None);
    ImGui::EndGroup();

    if (ImGui::IsItemHovered())
        draw_register_tooltip(name, value);
}

static void draw_flags(u16 reg_id, u32 value, const I386FlagDefinition* flags, int count, I386* cpu)
{
    float width = ImGui::GetStyle().ItemSpacing.x * (count - 1);

    for (int i = 0; i < count; i++)
        width += ImGui::CalcTextSize(flags[i].name).x;

    float offset = (ImGui::GetContentRegionAvail().x - width) * 0.5f;

    if (offset > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (int)offset);

    for (int i = 0; i < count; i++)
    {
        if (i > 0)
            ImGui::SameLine();

        ImGui::BeginGroup();

        float value_x = ImGui::GetCursorPosX() +
            (int)((ImGui::CalcTextSize(flags[i].name).x - ImGui::CalcTextSize("0").x) * 0.5f);

        ImGui::TextColored(orange, "%s", flags[i].name);
        ImGui::SetCursorPosX(value_x);
        EditableRegister1(reg_id, flags[i].bit, (value & (1U << flags[i].bit)) != 0, I386WriteCallback1, cpu);
        ImGui::EndGroup();
    }
}

static const char* execution_mode_name(u8 mode)
{
    if (mode == I386_MODE_PROTECTED)
        return "PROTECTED";

    if (mode == I386_MODE_VM86)
        return "VM86";

    return "REAL";
}

static const char* interrupt_shadow_name(u8 shadow)
{
    if (shadow == I386_SHADOW_STI)
        return "STI";

    if (shadow == I386_SHADOW_MOV_SS)
        return "MOV SS";

    return "NONE";
}

static void format_segment_attributes(const I386_Segment& segment, char* text, size_t text_size)
{
    snprintf(text, text_size, "%c %c%c%c %s %c%c%c%c%c",
        (segment.attributes & I386_SEGMENT_PRESENT) != 0 ? 'P' : '-',
        (segment.attributes & I386_SEGMENT_EXECUTABLE) != 0 ? 'X' : 'D',
        (segment.attributes & I386_SEGMENT_READABLE) != 0 ? 'R' : '-',
        (segment.attributes & I386_SEGMENT_WRITABLE) != 0 ? 'W' : '-',
        (segment.attributes & I386_SEGMENT_DEFAULT_32) != 0 ? "32" : "16",
        (segment.attributes & I386_SEGMENT_GRANULAR) != 0 ? 'G' : '-',
        (segment.attributes & I386_SEGMENT_CONFORMING) != 0 ? 'C' : '-',
        (segment.attributes & I386_SEGMENT_ACCESSED) != 0 ? 'A' : '-',
        (segment.attributes & I386_SEGMENT_SYSTEM) != 0 ? 'S' : '-',
        (segment.attributes & I386_SEGMENT_EXPAND_DOWN) != 0 ? 'E' : '-');
}

static void goto_memory(GT_Debug_Memory_Space space, u32 address, int segment_register)
{
    GT_Debug_Memory_Address target = { };

    target.space = space;
    target.address = address;
    target.segment_register = (s8)segment_register;
    target.region = -1;

    gui_debug_memory_goto(target);
}

static void draw_address(bool valid, u32 address, GT_Debug_Memory_Space space)
{
    if (!valid)
    {
        ImGui::TextDisabled("UNAVAILABLE");
        return;
    }

    ImGui::Text("$%08X", address);

    if (ImGui::IsItemClicked())
        goto_memory(space, address, -1);
}

static void draw_section_title(const char* title)
{
    ImGui::Spacing();
    ImGui::TextColored(blue, " %s", title);
    ImGui::Separator();
}

static void draw_segments(I386* cpu, const I386_State& state)
{
    draw_section_title("SEGMENTS");

    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoPadOuterX;

    if (!ImGui::BeginTable("i386_segments", 7, flags))
        return;

    ImGui::TableSetupColumn("Reg", ImGuiTableColumnFlags_WidthFixed, 30.0f);
    ImGui::TableSetupColumn("Selector", ImGuiTableColumnFlags_WidthFixed, 72.0f);
    ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthFixed, 96.0f);
    ImGui::TableSetupColumn("Limit", ImGuiTableColumnFlags_WidthFixed, 96.0f);
    ImGui::TableSetupColumn("Access", ImGuiTableColumnFlags_WidthFixed, 72.0f);
    ImGui::TableSetupColumn("DPL", ImGuiTableColumnFlags_WidthFixed, 28.0f);
    ImGui::TableSetupColumn("Attributes", ImGuiTableColumnFlags_WidthFixed, 112.0f);
    ImGui::TableHeadersRow();

    for (int row = 0; row < I386_SEGMENT_COUNT; row++)
    {
        int segment_index = k_segment_order[row];
        const I386_Segment& segment = state.segments[segment_index];
        char attributes[32];

        format_segment_attributes(segment, attributes, sizeof(attributes));

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(magenta, "%s", k_segment_names[segment_index]);
        ImGui::TableNextColumn();
        EditableRegister16(NULL, NULL, I386RegId_SegmentSelectorBase + segment_index, segment.selector,
            I386WriteCallback16, cpu, EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        EditableRegister32(NULL, NULL, I386RegId_SegmentBaseBase + segment_index, segment.base, I386WriteCallback32,
            cpu, EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        EditableRegister32(NULL, NULL, I386RegId_SegmentLimitBase + segment_index, segment.limit, I386WriteCallback32,
            cpu, EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        EditableRegister16(NULL, NULL, I386RegId_SegmentAccessBase + segment_index, segment.attributes,
            I386WriteCallback16, cpu, EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        EditableRegister8(NULL, NULL, I386RegId_SegmentDplBase + segment_index, segment.dpl, I386WriteCallback8, cpu,
            EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        ImGui::TextColored(brown, "%s", attributes);
    }

    ImGui::EndTable();
}

static void draw_descriptor_tables(I386* cpu, const I386_State& state)
{
    draw_section_title("DESCRIPTOR TABLES");

    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoPadOuterX;

    if (ImGui::BeginTable("i386_descriptors", 6, flags))
    {
        ImGui::TableSetupColumn("Table", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Selector", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthFixed, 96.0f);
        ImGui::TableSetupColumn("Limit", ImGuiTableColumnFlags_WidthFixed, 96.0f);
        ImGui::TableSetupColumn("Access", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("DPL", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(magenta, "GDTR");
        ImGui::TableNextColumn();
        ImGui::TextDisabled("-");
        ImGui::TableNextColumn();
        EditableRegister32(NULL, NULL, I386RegId_GDTRBase, state.gdtr.base, I386WriteCallback32, cpu,
            EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        EditableRegister16(NULL, NULL, I386RegId_GDTRLimit, state.gdtr.limit, I386WriteCallback16, cpu,
            EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        ImGui::TextDisabled("-");
        ImGui::TableNextColumn();
        ImGui::TextDisabled("-");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(magenta, "IDTR");
        ImGui::TableNextColumn();
        ImGui::TextDisabled("-");
        ImGui::TableNextColumn();
        EditableRegister32(NULL, NULL, I386RegId_IDTRBase, state.idtr.base, I386WriteCallback32, cpu,
            EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        EditableRegister16(NULL, NULL, I386RegId_IDTRLimit, state.idtr.limit, I386WriteCallback16, cpu,
            EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        ImGui::TextDisabled("-");
        ImGui::TableNextColumn();
        ImGui::TextDisabled("-");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(magenta, "LDTR");
        ImGui::TableNextColumn();
        EditableRegister16(NULL, NULL, I386RegId_LDTRSelector, state.ldtr.selector, I386WriteCallback16, cpu,
            EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        EditableRegister32(NULL, NULL, I386RegId_LDTRBase, state.ldtr.base, I386WriteCallback32, cpu,
            EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        EditableRegister32(NULL, NULL, I386RegId_LDTRLimit, state.ldtr.limit, I386WriteCallback32, cpu,
            EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        EditableRegister16(NULL, NULL, I386RegId_LDTRAccess, state.ldtr.attributes, I386WriteCallback16, cpu,
            EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        EditableRegister8(NULL, NULL, I386RegId_LDTRDpl, state.ldtr.dpl, I386WriteCallback8, cpu,
            EditableRegisterFlags_None);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(magenta, "TR");
        ImGui::TableNextColumn();
        EditableRegister16(NULL, NULL, I386RegId_TRSelector, state.task_register.selector, I386WriteCallback16, cpu,
            EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        EditableRegister32(NULL, NULL, I386RegId_TRBase, state.task_register.base, I386WriteCallback32, cpu,
            EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        EditableRegister32(NULL, NULL, I386RegId_TRLimit, state.task_register.limit, I386WriteCallback32, cpu,
            EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        EditableRegister16(NULL, NULL, I386RegId_TRAccess, state.task_register.attributes, I386WriteCallback16, cpu,
            EditableRegisterFlags_None);
        ImGui::TableNextColumn();
        EditableRegister8(NULL, NULL, I386RegId_TRDpl, state.task_register.dpl, I386WriteCallback8, cpu,
            EditableRegisterFlags_None);

        ImGui::EndTable();
    }
}

static void draw_debug_registers(I386* cpu, const I386_State& state)
{
    draw_section_title("DEBUG AND TEST REGISTERS");

    static const char* k_names[] = { "DR0", "DR1", "DR2", "DR3", "DR6", "DR7", "TR6", "TR7" };

    static const u16 k_ids[] =
    {
        I386RegId_DR0, I386RegId_DR1, I386RegId_DR2, I386RegId_DR3,
        I386RegId_DR6, I386RegId_DR7, I386RegId_TR6, I386RegId_TR7
    };

    const u32 values[] =
    {
        state.debug_registers[0], state.debug_registers[1], state.debug_registers[2], state.debug_registers[3],
        state.debug_registers[6], state.debug_registers[7], state.test_registers[0], state.test_registers[1]
    };

    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoPadOuterX;

    if (ImGui::BeginTable("i386_debug_registers", 2, flags))
    {
        for (int i = 0; i < 8; i++)
        {
            ImGui::TableNextColumn();
            ImGui::TextColored(violet, "%s", k_names[i]);
            ImGui::SameLine();
            EditableRegister32(NULL, NULL, k_ids[i], values[i], I386WriteCallback32, cpu, EditableRegisterFlags_None);
        }

        ImGui::EndTable();
    }
}

void gui_debug_window_i386(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(3, 26), ImGuiCond_FirstUseEver);

    ImGui::Begin("Intel 80386", &config_debug.show_processor, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize);

    ImGui::PushFont(gui_default_font);

    GeartownsCore* core = emu_get_core();
    I386* cpu = core->GetI386();
    I386_State state;

    cpu->CopyState(state);

    const I386_Segment& cs = state.segments[I386_SEGMENT_CS];
    const I386_Segment& ss = state.segments[I386_SEGMENT_SS];
    bool code_32 = (cs.attributes & I386_SEGMENT_DEFAULT_32) != 0;
    bool stack_32 = (ss.attributes & I386_SEGMENT_DEFAULT_32) != 0;

    if (ImGui::BeginTable("i386", 1, ImGuiTableFlags_BordersInnerH))
    {
        ImGui::TableNextColumn();
        draw_flags(I386RegId_EFLAGS, state.eflags, k_eflags, 7, cpu);
        draw_flags(I386RegId_EFLAGS, state.eflags, k_eflags + 7, 7, cpu);

        ImGui::TableNextColumn();
        ImGui::TextColored(yellow, "   CS:EIP");
        ImGui::SameLine();
        EditableRegister16(NULL, NULL, I386RegId_SegmentSelectorBase + I386_SEGMENT_CS, cs.selector, I386WriteCallback16,
            cpu, EditableRegisterFlags_None);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextUnformatted(":");
        ImGui::SameLine(0.0f, 0.0f);
        EditableRegister32(NULL, NULL, I386RegId_EIP, state.eip, I386WriteCallback32, cpu, EditableRegisterFlags_None);

        if (ImGui::IsItemClicked())
            goto_memory(GT_DEBUG_MEMORY_LOGICAL, state.eip, I386_SEGMENT_CS);

        if (ImGui::IsItemHovered())
            draw_register_tooltip("EIP", state.eip);

        GT_Debug_Memory_Translation pc = { };
        cpu->DebugTranslateLogical(I386_SEGMENT_CS, state.eip, pc);

        ImGui::TextColored(yellow, "   LINEAR");
        ImGui::SameLine();
        draw_address(pc.linear_valid, pc.linear, GT_DEBUG_MEMORY_LINEAR);
        ImGui::TextColored(yellow, " PHYSICAL");
        ImGui::SameLine();
        draw_address(pc.physical_valid, pc.physical, GT_DEBUG_MEMORY_PHYSICAL);

        ImGui::TableNextColumn();
        ImGui::TextColored(yellow, "   SS:ESP");
        ImGui::SameLine();
        EditableRegister16(NULL, NULL, I386RegId_SegmentSelectorBase + I386_SEGMENT_SS, ss.selector, I386WriteCallback16,
            cpu, EditableRegisterFlags_None);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextUnformatted(":");
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::PushID("stack_esp");
        EditableRegister32(NULL, NULL, I386RegId_ESP, state.registers[I386_REG_ESP].value, I386WriteCallback32, cpu,
            EditableRegisterFlags_None);

        if (ImGui::IsItemClicked())
            goto_memory(GT_DEBUG_MEMORY_LOGICAL,
                stack_32 ? state.registers[I386_REG_ESP].value : (u16)state.registers[I386_REG_ESP].value, I386_SEGMENT_SS);

        if (ImGui::IsItemHovered())
            draw_register_tooltip("ESP", state.registers[I386_REG_ESP].value);

        ImGui::PopID();

        ImGui::TableNextColumn();
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(2.0f, 2.0f));

        if (ImGui::BeginTable("regs", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoPadOuterX |
            ImGuiTableFlags_NoHostExtendX))
        {
            for (int i = 0; i < (int)(sizeof(k_registers) / sizeof(k_registers[0])); i++)
                draw_register(cpu, k_registers[i].name, k_registers[i].id, state.registers[k_registers[i].index].value, cyan);

            draw_register(cpu, "EFL", I386RegId_EFLAGS, state.eflags, orange);
            draw_register(cpu, "CR0", I386RegId_CR0, state.cr0, violet);
            draw_register(cpu, "CR2", I386RegId_CR2, state.cr2, violet);
            draw_register(cpu, "CR3", I386RegId_CR3, state.cr3, violet);

            ImGui::EndTable();
        }

        ImGui::PopStyleVar();

        ImGui::TableNextColumn();
        draw_flags(I386RegId_CR0, state.cr0, k_cr0_flags, (int)(sizeof(k_cr0_flags) / sizeof(k_cr0_flags[0])), cpu);

        ImGui::TableNextColumn();
        ImGui::TextColored(violet, " MODE:");
        ImGui::SameLine();
        ImGui::TextColored(green, "%-9s", execution_mode_name(state.execution_mode));
        ImGui::SameLine();
        ImGui::TextColored(violet, "  CPL:");
        ImGui::SameLine();
        ImGui::Text("%u", state.current_privilege_level);

        ImGui::TextColored(violet, " CODE:");
        ImGui::SameLine();
        ImGui::Text("%-9s", code_32 ? "32" : "16");
        ImGui::SameLine();
        ImGui::TextColored(violet, "STACK:");
        ImGui::SameLine();
        ImGui::Text("%s", stack_32 ? "32" : "16");

        ImGui::TableNextColumn();
        ImGui::TextColored(violet, " EXCEPTION:");
        ImGui::SameLine();

        if (state.last_exception_vector == 0xFF)
            ImGui::TextColored(gray, "--");
        else
            ImGui::TextColored(red, "$%02X", state.last_exception_vector);

        ImGui::TextColored(violet, "    REPEAT:");
        ImGui::SameLine();

        if (state.repeat.active)
        {
            ImGui::BeginGroup();
            ImGui::TextColored(green, "ON");
            ImGui::SameLine();
            ImGui::Text("$%02X", state.repeat.opcode);
            ImGui::EndGroup();

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::TextColored(cyan, "Opcode: $%02X", state.repeat.opcode);
                ImGui::TextColored(cyan, "Start: $%08X", state.repeat.start_eip);
                ImGui::TextColored(cyan, "Next: $%08X", state.repeat.next_eip);
                ImGui::EndTooltip();
            }
        }
        else
            ImGui::TextColored(gray, "OFF");

        ImGui::TextColored(violet, "    SHADOW:");
        ImGui::SameLine();

        if (state.interrupt_shadow == I386_SHADOW_NONE)
            ImGui::TextColored(gray, "NONE");
        else
            ImGui::TextColored(green, "%s (%u)", interrupt_shadow_name(state.interrupt_shadow),
                state.interrupt_shadow_steps);

        ImGui::TableNextColumn();
        ImGui::TextColored(state.halted ? yellow : gray, " HALTED");
        ImGui::SameLine();
        ImGui::TextColored(state.shutdown ? red : gray, "SHUTDOWN");
        ImGui::SameLine();
        ImGui::TextColored(state.nmi_blocked ? yellow : gray, "NMI BLOCKED");

        ImGui::EndTable();
    }

    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_i386_details(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(130, 26), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(660, 500), ImGuiCond_FirstUseEver);

    bool visible = ImGui::Begin("Intel 80386 System State", &config_debug.show_processor_details,
        ImGuiWindowFlags_HorizontalScrollbar);

    if (visible)
    {
        ImGui::PushFont(gui_default_font);

        GeartownsCore* core = emu_get_core();
        I386* cpu = core->GetI386();
        I386_State state;

        cpu->CopyState(state);

        draw_segments(cpu, state);
        draw_descriptor_tables(cpu, state);
        draw_debug_registers(cpu, state);

        ImGui::PopFont();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
