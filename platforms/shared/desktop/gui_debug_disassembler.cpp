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

#define GUI_DEBUG_DISASSEMBLER_IMPORT
#include "gui_debug_disassembler.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "imgui.h"
#include "fonts/IconsMaterialDesign.h"
#include "config.h"
#include "emu.h"
#include "gui.h"
#include "gui_actions.h"
#include "gui_debug.h"
#include "gui_debug_constants.h"

struct DisassemblerLine
{
    const I386_Disassembler_Record* record;
    bool breakpoint;
    bool symbol;
};

static std::vector<DisassemblerLine> disassembler_lines;

static bool selected_address_valid = false;
static u32 selected_address = 0;

static bool goto_address_requested = false;
static bool goto_address_unavailable = false;
static u32 goto_address_target = 0;

static bool goto_back_requested = false;
static float goto_back = 0.0f;
static int pc_position = 0;
static int goto_position = 0;

static char new_breakpoint_buffer[20] = "";
static char goto_address_buffer[9] = "";
static char runto_address_buffer[9] = "";

static bool decode_ahead_valid = false;
static u16 decode_ahead_cs = 0;
static u32 decode_ahead_cs_base = 0;
static u32 decode_ahead_eip = 0;
static u64 decode_ahead_memory_snapshot = 0;
static int decode_ahead_count = 0;

static bool noop_irq_breakpoints[8] = { };
static bool noop_pause_on_brk = false;
static int noop_brk_value = 0x42;
static bool noop_brk_trigger_irq = false;

static void disassembler_menu(void);
static void draw_controls(void);
static void draw_breakpoints_content(void);
static void prepare_drawable_lines(void);
static void draw_disassembly(void);
static void draw_instruction(const I386_Disassembler_Record* record, bool breakpoint, bool current_pc);
static void draw_context_menu(DisassemblerLine* line);
static void split_instruction(const char* instruction, char* prefixes, size_t prefixes_size, char* mnemonic,
    size_t mnemonic_size, char* operands, size_t operands_size);
static void draw_operands(const char* operands, bool address_operand, bool override_color, const ImVec4& color);
static bool replace_jump_target(const I386_Disassembler_Record* record, char* operands, size_t operands_size);
static void unavailable_tooltip(void);
static bool parse_address(const char* text, u32& address);
static bool parse_address_range(const char* text, u32& start, u32& end);
static void request_goto_address(u32 address);

void gui_debug_disassembler_init(void)
{
}

void gui_debug_disassembler_destroy(void)
{
    disassembler_lines.clear();
}

void gui_debug_disassembler_reset(void)
{
    selected_address_valid = false;
    goto_address_requested = false;
    goto_address_unavailable = false;
    goto_back_requested = false;
    decode_ahead_valid = false;
}

void gui_debug_reset_breakpoints(void)
{
    GeartownsCore* core = emu_get_core();

    if (IsValidPointer(core))
        core->GetI386()->ResetBreakpoints();

    new_breakpoint_buffer[0] = 0;
}

void gui_debug_toggle_breakpoint(void)
{
    if (!selected_address_valid)
        return;

    I386* cpu = emu_get_core()->GetI386();

    if (cpu->IsBreakpoint(selected_address))
        cpu->RemoveBreakpoint(selected_address);
    else
        cpu->AddBreakpoint(selected_address);
}

void gui_debug_add_bookmark(void)
{
}

void gui_debug_add_symbol(void)
{
}

void gui_debug_runtocursor(void)
{
    if (selected_address_valid)
        gui_debug_runto_address(selected_address);
}

void gui_debug_runto_address(u32 address)
{
    emu_get_core()->GetI386()->AddRunToBreakpoint(address);
    emu_debug_continue();
}

void gui_debug_go_back(void)
{
    goto_back_requested = true;
}

void gui_debug_window_disassembler(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(330, 26), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(620, 600), ImGuiCond_FirstUseEver);

    ImGui::Begin("Intel 80386 Disassembler", &config_debug.show_disassembler, ImGuiWindowFlags_MenuBar);

    disassembler_menu();

    draw_controls();
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Breakpoints"))
        draw_breakpoints_content();

    draw_disassembly();

    ImGui::End();
    ImGui::PopStyleVar();
}

static void disassembler_menu(void)
{
    if (!ImGui::BeginMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        ImGui::MenuItem("Save All Disassembled Code As...");
        unavailable_tooltip();
        ImGui::MenuItem("Save Current View As...");
        unavailable_tooltip();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("Opcodes", NULL, &config_debug.dis_show_bytes);
        ImGui::MenuItem("Symbols", NULL, &config_debug.dis_show_symbols);
        ImGui::MenuItem("Segment", NULL, &config_debug.dis_show_segment);

        ImGui::Separator();

        if (ImGui::BeginMenu("Syntax"))
        {
            if (ImGui::MenuItem("Intel", NULL, config_debug.dis_syntax == 0))
            {
                config_debug.dis_syntax = 0;
                emu_set_disassembler_syntax(0);
            }

            ImGui::BeginDisabled();
            ImGui::MenuItem("AT&T");
            ImGui::EndDisabled();
            unavailable_tooltip();

            ImGui::Separator();
            ImGui::TextDisabled("Intel syntax is currently available");
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Decode Ahead"))
        {
            ImGui::PushItemWidth(220.0f);
            ImGui::SliderInt("##lookahead", &config_debug.dis_look_ahead_count, 0, 100, "%d instructions");
            ImGui::PopItemWidth();
            ImGui::TextDisabled("Decoded when this window opens or execution stops");
            ImGui::TextDisabled("Previously decoded rows are retained");
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Go"))
    {
        if (ImGui::MenuItem("Back", config_hotkeys[config_HotkeyIndex_DebugGoBack].str))
        {
            gui_debug_go_back();
        }

        if (ImGui::MenuItem("Go To PC"))
        {
            request_goto_address(emu_get_core()->GetI386()->GetCurrentLinearPC());
        }

        if (ImGui::BeginMenu("Go To Address..."))
        {
            bool go = false;
            ImGui::PushItemWidth(82.0f);

            if (ImGui::InputTextWithHint("##menu_goto_address", "XXXXXXXX", goto_address_buffer,
                IM_ARRAYSIZE(goto_address_buffer), ImGuiInputTextFlags_AutoSelectAll |
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsHexadecimal |
                ImGuiInputTextFlags_CharsUppercase))
            {
                go = true;
            }

            ImGui::PopItemWidth();
            ImGui::SameLine();

            if (ImGui::Button("Go!", ImVec2(40.0f, 0.0f)))
                go = true;

            if (go)
            {
                u32 address = 0;

                if (parse_address(goto_address_buffer, address))
                    request_goto_address(address);

                goto_address_buffer[0] = 0;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Run"))
    {
        if (ImGui::MenuItem("Start", config_hotkeys[config_HotkeyIndex_DebugContinue].str))
        {
            emu_debug_continue();
        }

        if (ImGui::MenuItem("Stop", config_hotkeys[config_HotkeyIndex_DebugBreak].str))
        {
            emu_debug_break();
        }

        if (ImGui::MenuItem("Step Over", config_hotkeys[config_HotkeyIndex_DebugStepOver].str))
        {
            emu_debug_step_over();
        }

        if (ImGui::MenuItem("Step Into", config_hotkeys[config_HotkeyIndex_DebugStepInto].str))
        {
            emu_debug_step_into();
        }

        if (ImGui::MenuItem("Step Out", config_hotkeys[config_HotkeyIndex_DebugStepOut].str))
        {
            emu_debug_step_out();
        }

        if (ImGui::MenuItem("Step Frame", config_hotkeys[config_HotkeyIndex_DebugStepFrame].str))
        {
            emu_debug_step_frame();
        }

        if (ImGui::MenuItem("Run to Cursor", config_hotkeys[config_HotkeyIndex_DebugRunToCursor].str, false,
            selected_address_valid))
        {
            gui_debug_runtocursor();
        }

        if (ImGui::MenuItem("Reset", config_hotkeys[config_HotkeyIndex_Reset].str))
        {
            gui_action_reset();
        }

        ImGui::Separator();

        ImGui::MenuItem("Skip IRQs on Step Into", NULL, &config_debug.step_skip_interrupts);
        unavailable_tooltip();

        ImGui::Separator();

        if (ImGui::BeginMenu("Run To Address..."))
        {
            bool run = false;
            ImGui::PushItemWidth(82.0f);

            if (ImGui::InputTextWithHint("##menu_runto_address", "XXXXXXXX", runto_address_buffer,
                IM_ARRAYSIZE(runto_address_buffer), ImGuiInputTextFlags_AutoSelectAll |
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsHexadecimal |
                ImGuiInputTextFlags_CharsUppercase))
            {
                run = true;
            }

            ImGui::PopItemWidth();
            ImGui::SameLine();

            if (ImGui::Button("Run!", ImVec2(50.0f, 0.0f)))
                run = true;

            if (run)
            {
                u32 address = 0;

                if (parse_address(runto_address_buffer, address))
                    gui_debug_runto_address(address);

                runto_address_buffer[0] = 0;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Breakpoints"))
    {
        ImGui::MenuItem("Breakpoints Window", NULL, &config_debug.show_breakpoints);

        ImGui::Separator();

        if (ImGui::MenuItem("Toggle Selected Line", config_hotkeys[config_HotkeyIndex_DebugBreakpoint].str, false,
            selected_address_valid))
        {
            gui_debug_toggle_breakpoint();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("IRQs"))
        {
            for (int i = 0; i < 8; i++)
            {
                char label[32];

                snprintf(label, sizeof(label), "Break on IRQ %d", i);
                ImGui::MenuItem(label, NULL, &noop_irq_breakpoints[i]);
                unavailable_tooltip();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("BRK #n"))
        {
            ImGui::MenuItem("Pause on BRK #n", NULL, &noop_pause_on_brk);
            unavailable_tooltip();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("#n");
            ImGui::SameLine();
            ImGui::PushItemWidth(80.0f);
            ImGui::InputInt("##brk_value", &noop_brk_value, 1, 16,
                ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
            ImGui::PopItemWidth();

            if (noop_brk_value < 0)
                noop_brk_value = 0;
            else if (noop_brk_value > 0xFF)
                noop_brk_value = 0xFF;

            unavailable_tooltip();

            ImGui::MenuItem("Trigger IRQ", NULL, &noop_brk_trigger_irq);
            unavailable_tooltip();
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Remove All"))
            gui_debug_reset_breakpoints();

        ImGui::MenuItem("Disable All", NULL, &emu_debug_disable_breakpoints);

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Bookmarks"))
    {
        if (ImGui::MenuItem("Add Bookmark...", NULL, false, selected_address_valid))
        {
            gui_debug_add_bookmark();
        }

        unavailable_tooltip();

        ImGui::MenuItem("Remove All");
        unavailable_tooltip();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Symbols"))
    {
        ImGui::MenuItem("Symbols Window", NULL, &config_debug.show_symbols);

        ImGui::Separator();

        ImGui::MenuItem("Hardware Labels", NULL, &config_debug.dis_replace_labels);
        unavailable_tooltip();

        ImGui::MenuItem("Automatic Symbols", NULL, &config_debug.dis_show_auto_symbols);

        if (!config_debug.dis_show_auto_symbols)
            ImGui::BeginDisabled();

        ImGui::MenuItem("Dim Automatic Symbols", NULL, &config_debug.dis_dim_auto_symbols);

        if (!config_debug.dis_show_auto_symbols)
            ImGui::EndDisabled();

        ImGui::MenuItem("Replace Address With Symbol", NULL, &config_debug.dis_replace_symbols);

        ImGui::Separator();

        if (ImGui::MenuItem("Add Symbol...", NULL, false, selected_address_valid))
        {
            gui_debug_add_symbol();
        }

        unavailable_tooltip();

        ImGui::MenuItem("Load Symbols...");
        unavailable_tooltip();

        if (ImGui::MenuItem("Clear Symbols"))
            gui_debug_reset_symbols();

        unavailable_tooltip();

        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

static void draw_controls(void)
{
    ImGui::PushFont(gui_material_icons_font);

    if (ImGui::Button(ICON_MD_PLAY_ARROW))
        emu_debug_continue();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Start / Continue (%s)", config_hotkeys[config_HotkeyIndex_DebugContinue].str);

    ImGui::SameLine();

    if (ImGui::Button(ICON_MD_STOP))
        emu_debug_break();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Stop (%s)", config_hotkeys[config_HotkeyIndex_DebugBreak].str);

    ImGui::SameLine();

    if (ImGui::Button(ICON_MD_REDO))
        emu_debug_step_over();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Step Over (%s)", config_hotkeys[config_HotkeyIndex_DebugStepOver].str);

    ImGui::SameLine();

    if (ImGui::Button(ICON_MD_FILE_DOWNLOAD))
        emu_debug_step_into();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Step Into (%s)", config_hotkeys[config_HotkeyIndex_DebugStepInto].str);

    ImGui::SameLine();

    if (ImGui::Button(ICON_MD_FILE_UPLOAD))
        emu_debug_step_out();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Step Out (%s)", config_hotkeys[config_HotkeyIndex_DebugStepOut].str);

    ImGui::SameLine();

    if (ImGui::Button(ICON_MD_INPUT))
        emu_debug_step_frame();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Step Frame (%s)", config_hotkeys[config_HotkeyIndex_DebugStepFrame].str);

    ImGui::SameLine();

    if (ImGui::Button(ICON_MD_KEYBOARD_TAB))
        gui_debug_runtocursor();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Run to Cursor (%s)", config_hotkeys[config_HotkeyIndex_DebugRunToCursor].str);

    ImGui::SameLine();

    if (ImGui::Button(ICON_MD_REPLAY))
        gui_action_reset();

    ImGui::PopFont();

    ImGui::SameLine();
    ImGui::TextColored(emu_is_debug_idle() ? red : green, emu_is_debug_idle() ? "   PAUSED" : "   RUNNING");

    ImGui::PushItemWidth(92.0f);
    ImGui::InputTextWithHint("##goto_address", "Go to linear", goto_address_buffer, IM_ARRAYSIZE(goto_address_buffer),
        ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();

    if (ImGui::Button("Go"))
    {
        u32 address = 0;

        if (parse_address(goto_address_buffer, address))
            request_goto_address(address);
    }

    ImGui::SameLine();
    ImGui::InputTextWithHint("##runto_address", "Run to linear", runto_address_buffer,
        IM_ARRAYSIZE(runto_address_buffer), ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();

    if (ImGui::Button("Run To"))
    {
        u32 address = 0;

        if (parse_address(runto_address_buffer, address))
            gui_debug_runto_address(address);
    }

    ImGui::PopItemWidth();

    if (goto_address_unavailable)
        ImGui::TextColored(violet, "Disassembly unavailable at %08X", goto_address_target);
}

static void draw_breakpoints_content(void)
{
    ImGui::Checkbox("Disable All##disable_exec", &emu_debug_disable_breakpoints);
    ImGui::SameLine();

    if (ImGui::Button("Remove All##clear_exec", ImVec2(85, 0)))
        gui_debug_reset_breakpoints();

    ImGui::Separator();
    ImGui::Columns(2, "execution_breakpoints");
    ImGui::SetColumnOffset(1, 175);

    ImGui::PushItemWidth(145);
    bool add = ImGui::InputTextWithHint("##add_exec_breakpoint", "XXXXXXXX-XXXXXXXX", new_breakpoint_buffer,
        IM_ARRAYSIZE(new_breakpoint_buffer), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();
    ImGui::TextDisabled("Execution only");

    if (ImGui::Button("Add##add_exec", ImVec2(85, 0)))
        add = true;

    if (add)
    {
        u32 start = 0;
        u32 end = 0;

        if (parse_address_range(new_breakpoint_buffer, start, end))
        {
            if (start == end)
                emu_get_core()->GetI386()->AddBreakpoint(start);
            else
                emu_get_core()->GetI386()->AddBreakpoint(start, end);

            new_breakpoint_buffer[0] = 0;
        }
    }

    ImGui::NextColumn();
    ImGui::BeginChild("execution_breakpoint_list", ImVec2(0, 130), false);
    ImGui::PushFont(gui_default_font);

    std::vector<I386_Breakpoint>* breakpoints = emu_get_core()->GetI386()->GetBreakpoints();
    int remove = -1;

    for (size_t i = 0; i < breakpoints->size(); i++)
    {
        I386_Breakpoint& breakpoint = (*breakpoints)[i];

        ImGui::PushID((int)i);

        if (ImGui::SmallButton("X"))
            remove = (int)i;

        ImGui::SameLine();

        if (ImGui::SmallButton(breakpoint.enabled ? "-" : "+"))
            breakpoint.enabled = !breakpoint.enabled;

        ImGui::SameLine();

        if (breakpoint.range)
            ImGui::TextColored(breakpoint.enabled ? cyan : gray, "%08X-%08X X", breakpoint.address1, breakpoint.address2);
        else
        {
            I386_Disassembler_Record* record = emu_get_core()->GetI386()->GetDisassemblerRecord(breakpoint.address1);

            ImGui::TextColored(breakpoint.enabled ? cyan : gray, "%08X X", breakpoint.address1);

            if (IsValidPointer(record) && record->auto_symbol[0] != 0)
            {
                ImGui::SameLine();
                ImGui::TextColored(breakpoint.enabled ? green : gray, "%s", record->auto_symbol);
            }
        }

        ImGui::PopID();
    }

    if (remove >= 0)
        breakpoints->erase(breakpoints->begin() + remove);

    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::Columns(1);
    ImGui::Separator();
}

void gui_debug_window_breakpoints(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(795, 26), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(430, 264), ImGuiCond_FirstUseEver);
    ImGui::Begin("Execution Breakpoints", &config_debug.show_breakpoints);

    draw_breakpoints_content();

    ImGui::End();
    ImGui::PopStyleVar();
}

static void prepare_drawable_lines(void)
{
    I386* cpu = emu_get_core()->GetI386();
    I386_Debug_State state;
    cpu->CopyDebugState(state);

    u32 pc = cpu->GetCurrentLinearPC();
    u64 memory_snapshot = emu_get_core()->GetMemory()->GetDebugSnapshotId();
    I386_Disassembler_Record* pc_record = cpu->GetDisassemblerRecord(pc);
    bool pc_missing = !IsValidPointer(pc_record) || pc_record->name[0] == 0;
    bool decode = emu_is_debug_idle() &&
        (!decode_ahead_valid || emu_debug_pc_changed || decode_ahead_cs != state.segment[I386_SEGMENT_CS].selector ||
        decode_ahead_cs_base != state.segment[I386_SEGMENT_CS].base || decode_ahead_eip != state.eip ||
        decode_ahead_memory_snapshot != memory_snapshot || decode_ahead_count != config_debug.dis_look_ahead_count ||
        pc_missing);

    if (decode)
    {
        if (config_debug.dis_look_ahead_count > 0)
            cpu->DisassembleAhead(config_debug.dis_look_ahead_count);
        else
            cpu->Disassemble(state.eip);

        decode_ahead_valid = true;
        decode_ahead_cs = state.segment[I386_SEGMENT_CS].selector;
        decode_ahead_cs_base = state.segment[I386_SEGMENT_CS].base;
        decode_ahead_eip = state.eip;
        decode_ahead_memory_snapshot = memory_snapshot;
        decode_ahead_count = config_debug.dis_look_ahead_count;
    }

    const std::map<u32, I386_Disassembler_Record>& records = cpu->GetDisassemblerRecords();
    std::map<u32, I386_Disassembler_Record>::const_iterator record;

    if (goto_address_requested)
    {
        bool found = false;

        for (record = records.begin(); record != records.end(); record++)
        {
            if (record->second.name[0] != 0 && (u32)(goto_address_target - record->first) < (u32)record->second.size)
            {
                found = true;
                break;
            }
        }

        u32 eip = goto_address_target - state.segment[I386_SEGMENT_CS].base;

        if (!found && emu_is_debug_idle() && eip <= state.segment[I386_SEGMENT_CS].limit)
            cpu->Disassemble(eip);
    }

    disassembler_lines.clear();
    pc_position = 0;
    goto_position = -1;

    for (record = records.begin(); record != records.end(); record++)
    {
        if (record->second.name[0] == 0)
            continue;

        int first_position = (int)disassembler_lines.size();

        if (config_debug.dis_show_symbols && config_debug.dis_show_auto_symbols && record->second.auto_symbol[0] != 0)
        {
            DisassemblerLine symbol_line;

            symbol_line.record = &record->second;
            symbol_line.breakpoint = false;
            symbol_line.symbol = true;
            disassembler_lines.push_back(symbol_line);
        }

        DisassemblerLine line;

        line.record = &record->second;
        line.breakpoint = cpu->IsBreakpoint(record->first);
        line.symbol = false;

        if (record->first == pc)
            pc_position = (int)disassembler_lines.size();

        if (goto_address_requested && (u32)(goto_address_target - record->first) < (u32)record->second.size)
            goto_position = first_position;

        disassembler_lines.push_back(line);
    }
}

static void draw_disassembly(void)
{
    bool light_theme = config_emulator.theme == config_Theme_Light;
    ImVec4 hover_color = light_theme ? ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered] : (ImVec4)mid_gray;

    ImGui::PushFont(gui_default_font);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hover_color);

    bool visible = ImGui::BeginChild("##disassembly", ImVec2(ImGui::GetContentRegionAvail().x, 0), true,
        ImGuiWindowFlags_HorizontalScrollbar);

    if (visible)
    {
        I386* cpu = emu_get_core()->GetI386();
        u32 pc = cpu->GetCurrentLinearPC();

        prepare_drawable_lines();

        if (emu_debug_pc_changed)
        {
            emu_debug_pc_changed = false;

            float offset = ImGui::GetWindowHeight() / 2.0f - ImGui::GetTextLineHeightWithSpacing();
            ImGui::SetScrollY(pc_position * ImGui::GetTextLineHeightWithSpacing() - offset);
        }

        if (goto_address_requested)
        {
            goto_address_requested = false;
            goto_address_unavailable = goto_position < 0;

            if (!goto_address_unavailable)
            {
                goto_back = ImGui::GetScrollY();
                ImGui::SetScrollY(goto_position * ImGui::GetTextLineHeightWithSpacing() + 2.0f);
            }
        }

        if (goto_back_requested)
        {
            goto_back_requested = false;
            ImGui::SetScrollY(goto_back);
        }

        ImGuiListClipper clipper;
        clipper.Begin((int)disassembler_lines.size(), ImGui::GetTextLineHeightWithSpacing());

        while (clipper.Step())
        {
            for (int item = clipper.DisplayStart; item < clipper.DisplayEnd; item++)
            {
                DisassemblerLine& line = disassembler_lines[item];
                const I386_Disassembler_Record* record = line.record;

                if (line.symbol)
                {
                    ImGui::TextColored(config_debug.dis_dim_auto_symbols ? dim_green : green, "%s:", record->auto_symbol);
                    continue;
                }

                ImGui::PushID(item);

                bool selected = selected_address_valid && selected_address == record->linear;

                if (ImGui::Selectable("", selected, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    if (ImGui::IsMouseDoubleClicked(0) && record->jump && record->jump_target_known)
                        request_goto_address(record->jump_linear);
                    else if (selected)
                        selected_address_valid = false;
                    else
                    {
                        selected_address_valid = true;
                        selected_address = record->linear;
                    }
                }

                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                ImVec2 row_min = ImGui::GetItemRectMin();
                ImVec2 row_max = ImGui::GetItemRectMax();
                bool hovered = ImGui::IsItemHovered();

                if (line.breakpoint && !hovered)
                    draw_list->AddRectFilled(row_min, row_max, ImGui::GetColorU32(dark_red));
                else if (record->linear == pc && !hovered)
                    draw_list->AddRectFilled(row_min, row_max, ImGui::GetColorU32(dark_yellow));
                else if (record->subroutine && !hovered)
                {
                    ImVec4 subroutine_color = light_theme ? ImGui::GetStyle().Colors[ImGuiCol_Header] : (ImVec4)dark_gray;
                    draw_list->AddRectFilled(row_min, row_max, ImGui::GetColorU32(subroutine_color));
                }

                draw_context_menu(&line);

                if (config_debug.dis_show_segment)
                {
                    ImGui::SameLine();
                    ImGui::TextColored(line.breakpoint ? red : magenta, "%s:%08X", record->segment, record->eip);
                }

                ImGui::SameLine();
                ImGui::TextColored(line.breakpoint ? red : cyan, "%08X", record->linear);
                ImGui::SameLine();
                ImGui::TextColored(yellow, record->linear == pc ? "->" : "  ");
                ImGui::SameLine();
                draw_instruction(record, line.breakpoint, record->linear == pc);

                if (record->returns)
                {
                    // Keep the clipper's row height unchanged
                    ImVec4 separator_color = light_theme ? ImGui::GetStyle().Colors[ImGuiCol_Separator] : (ImVec4)dark_green;
                    ImVec2 separator_start = ImGui::GetCursorScreenPos();
                    separator_start.y -= ImGui::GetStyle().ItemSpacing.y * 0.5f + 1.0f;
                    ImVec2 separator_end(separator_start.x + ImGui::GetContentRegionAvail().x, separator_start.y + 1.0f);

                    draw_list->AddRectFilled(separator_start, separator_end, ImGui::GetColorU32(separator_color));
                }

                ImGui::PopID();
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

static void draw_instruction(const I386_Disassembler_Record* record, bool breakpoint, bool current_pc)
{
    const int bytes_column = 25;
    char prefixes[24];
    char mnemonic[32];
    char operands[128];

    split_instruction(record->name, prefixes, sizeof(prefixes), mnemonic, sizeof(mnemonic), operands, sizeof(operands));

    bool replaced_jump_target = replace_jump_target(record, operands, sizeof(operands));
    bool light_theme = config_emulator.theme == config_Theme_Light;
    ImVec4 mnemonic_color = current_pc ? (ImVec4)yellow : (breakpoint ? (ImVec4)red : (ImVec4)white);
    ImVec4 bytes_color = breakpoint ? (ImVec4)red : (light_theme ? (ImVec4)gray : (ImVec4)mid_gray);
    bool override_color = current_pc || breakpoint;
    float space_width = ImGui::CalcTextSize(" ").x;
    int instruction_length = (int)strlen(mnemonic);

    if (prefixes[0] != 0)
    {
        ImGui::TextColored(override_color ? mnemonic_color : (ImVec4)blue, "%s", prefixes);
        ImGui::SameLine(0.0f, space_width);
        instruction_length += (int)strlen(prefixes) + 1;
    }

    ImGui::TextColored(mnemonic_color, "%s", mnemonic);

    if (operands[0] != 0)
    {
        bool address_operand = record->jump && record->jump_target_known && !replaced_jump_target;

        ImGui::SameLine(0.0f, space_width);
        draw_operands(operands, address_operand, override_color, mnemonic_color);
        instruction_length += 1 + (int)strlen(operands);
    }

    if (config_debug.dis_show_bytes)
    {
        int spacing = bytes_column - instruction_length;

        if (spacing < 2)
            spacing = 2;

        ImGui::SameLine(0.0f, space_width * spacing);
        ImGui::TextColored(bytes_color, "; %s", record->bytes);
    }
}

static bool text_equals(const char* start, const char* end, const char* word)
{
    size_t length = (size_t)(end - start);

    return strlen(word) == length && strncmp(start, word, length) == 0;
}

static bool is_prefix(const char* start, const char* end)
{
    return text_equals(start, end, "LOCK") || text_equals(start, end, "REP") || text_equals(start, end, "REPE") ||
        text_equals(start, end, "REPNE") || text_equals(start, end, "ADDR16") || text_equals(start, end, "ADDR32") ||
        text_equals(start, end, "ES") || text_equals(start, end, "CS") || text_equals(start, end, "SS") ||
        text_equals(start, end, "DS") || text_equals(start, end, "FS") || text_equals(start, end, "GS");
}

static bool is_identifier_character(char value)
{
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') ||
        value == '_' || value == '?';
}

static bool is_register(const char* start, const char* end)
{
    static const char* k_register_names[] =
    {
        "AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH",
        "AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI",
        "EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI",
        "ES", "CS", "SS", "DS", "FS", "GS", "ST",
        "EIP", "EFLAGS"
    };

    for (size_t i = 0; i < sizeof(k_register_names) / sizeof(k_register_names[0]); i++)
    {
        if (text_equals(start, end, k_register_names[i]))
            return true;
    }

    size_t length = (size_t)(end - start);

    // CRn, DRn and TRn
    if (length == 3 && start[2] >= '0' && start[2] <= '7')
        return (start[0] == 'C' || start[0] == 'D' || start[0] == 'T') && start[1] == 'R';

    return false;
}

static bool is_qualifier(const char* start, const char* end)
{
    static const char* k_qualifier_names[] =
    {
        "BYTE", "WORD", "DWORD", "FWORD", "QWORD", "TBYTE",
        "PTR", "FAR", "NEAR", "SHORT"
    };

    for (size_t i = 0; i < sizeof(k_qualifier_names) / sizeof(k_qualifier_names[0]); i++)
    {
        if (text_equals(start, end, k_qualifier_names[i]))
            return true;
    }

    return false;
}

static bool is_automatic_symbol(const char* start, const char* end)
{
    size_t length = (size_t)(end - start);

    if (length <= 4)
        return false;

    return strncmp(start, "LOC_", 4) == 0 || strncmp(start, "SUB_", 4) == 0 || strncmp(start, "INT_", 4) == 0;
}

static void draw_operands(const char* operands, bool address_operand, bool override_color, const ImVec4& color)
{
    const char* cursor = operands;
    bool first = true;

    while (*cursor != 0)
    {
        const char* start = cursor;
        ImVec4 token_color = blue;

        if (*cursor == ' ')
        {
            while (*cursor == ' ')
                cursor++;
        }
        else if (*cursor >= '0' && *cursor <= '9')
        {
            cursor++;

            while (is_hex_digit(*cursor) || *cursor == 'x' || *cursor == 'X')
                cursor++;

            token_color = address_operand ? (ImVec4)cyan : (ImVec4)blue;
        }
        else if (is_identifier_character(*cursor))
        {
            while (is_identifier_character(*cursor))
                cursor++;

            bool stack_register = text_equals(start, cursor, "ST") && *cursor == '(' && cursor[1] >= '0' &&
                cursor[1] <= '7' && cursor[2] == ')';

            if (stack_register)
            {
                cursor += 3;
                token_color = cyan;
            }
            else if (is_register(start, cursor))
                token_color = cyan;
            else if (is_qualifier(start, cursor))
                token_color = brown;
            else if (is_automatic_symbol(start, cursor))
                token_color = config_debug.dis_dim_auto_symbols ? (ImVec4)dim_green : (ImVec4)green;
        }
        else
        {
            cursor++;

            if (*start == '[' || *start == ']')
                token_color = brown;
            else
                token_color = gray;
        }

        if (!first)
            ImGui::SameLine(0.0f, 0.0f);

        ImGui::TextColored(override_color ? color : token_color, "%.*s", (int)(cursor - start), start);
        first = false;
    }
}

static void split_instruction(const char* instruction, char* prefixes, size_t prefixes_size, char* mnemonic,
    size_t mnemonic_size, char* operands, size_t operands_size)
{
    prefixes[0] = 0;
    mnemonic[0] = 0;
    operands[0] = 0;

    if (!IsValidPointer(instruction) || instruction[0] == 0)
        return;

    const char* start = instruction;

    while (*start == ' ')
        start++;

    const char* word_start = start;
    const char* word_end = start;
    size_t prefix_length = 0;

    while (true)
    {
        while (*word_end != 0 && *word_end != ' ')
            word_end++;

        if (!is_prefix(word_start, word_end))
            break;

        size_t word_length = (size_t)(word_end - word_start);

        if (prefix_length != 0 && prefix_length + 1 < prefixes_size)
            prefixes[prefix_length++] = ' ';

        size_t remaining = prefixes_size - prefix_length - 1;

        if (word_length > remaining)
            word_length = remaining;

        memcpy(prefixes + prefix_length, word_start, word_length);
        prefix_length += word_length;
        prefixes[prefix_length] = 0;

        word_start = word_end;

        while (*word_start == ' ')
            word_start++;

        word_end = word_start;
    }

    const char* mnemonic_end = word_end;
    const char* operand_start = word_end;

    while (*operand_start == ' ')
        operand_start++;

    size_t mnemonic_length = (size_t)(mnemonic_end - word_start);

    if (mnemonic_length >= mnemonic_size)
        mnemonic_length = mnemonic_size - 1;

    memcpy(mnemonic, word_start, mnemonic_length);
    mnemonic[mnemonic_length] = 0;
    snprintf(operands, operands_size, "%s", operand_start);
}

static bool replace_jump_target(const I386_Disassembler_Record* record, char* operands, size_t operands_size)
{
    if (!config_debug.dis_replace_symbols || !config_debug.dis_show_auto_symbols || !record->jump ||
        !record->jump_target_known)
        return false;

    I386_Disassembler_Record* target = emu_get_core()->GetI386()->GetDisassemblerRecord(record->jump_linear);

    if (!IsValidPointer(target) || target->auto_symbol[0] == 0)
        return false;

    const char* space = strchr(operands, ' ');

    if (IsValidPointer(space) && text_equals(operands, space, "FAR"))
        snprintf(operands, operands_size, "FAR %s", target->auto_symbol);
    else if (IsValidPointer(space) && text_equals(operands, space, "NEAR"))
        snprintf(operands, operands_size, "NEAR %s", target->auto_symbol);
    else if (IsValidPointer(space) && text_equals(operands, space, "SHORT"))
        snprintf(operands, operands_size, "SHORT %s", target->auto_symbol);
    else
        snprintf(operands, operands_size, "%s", target->auto_symbol);

    return true;
}

static void draw_context_menu(DisassemblerLine* line)
{
    if (!ImGui::BeginPopupContextItem())
        return;

    I386* cpu = emu_get_core()->GetI386();

    if (ImGui::Selectable(line->breakpoint ? "Remove Breakpoint" : "Add Breakpoint"))
    {
        if (line->breakpoint)
            cpu->RemoveBreakpoint(line->record->linear);
        else
            cpu->AddBreakpoint(line->record->linear);
    }

    if (ImGui::Selectable("Run to Cursor"))
        gui_debug_runto_address(line->record->linear);

    if (line->record->jump && line->record->jump_target_known && ImGui::Selectable("Go to Target"))
        request_goto_address(line->record->jump_linear);

    ImGui::Separator();

    if (ImGui::Selectable("Add Bookmark"))
        gui_debug_add_bookmark();

    if (ImGui::Selectable("Add Symbol"))
        gui_debug_add_symbol();

    ImGui::EndPopup();
}

void gui_debug_window_call_stack(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(795, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(510, 250), ImGuiCond_FirstUseEver);
    ImGui::Begin("Intel 80386 Call Stack", &config_debug.show_call_stack);

    I386* cpu = emu_get_core()->GetI386();
    const std::vector<I386_CallStackEntry>& stack = cpu->GetDisassemblerCallStack();
    ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("call_stack", 3, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Subroutine", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Return", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableHeadersRow();
        ImGui::PushFont(gui_default_font);

        int row = 0;

        for (int i = (int)stack.size() - 1; i >= 0; i--)
        {
            const I386_CallStackEntry* entry = &stack[i];
            I386_Disassembler_Record* record = cpu->GetDisassemblerRecord(entry->dest_linear);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID(row++);

            if (ImGui::Selectable("##call", false, ImGuiSelectableFlags_SpanAllColumns))
                request_goto_address(entry->dest_linear);

            ImGui::SameLine(0, 0);
            ImGui::TextColored(cyan, "%04X:%08X", entry->dest_cs, entry->dest);

            if (IsValidPointer(record) && record->auto_symbol[0] != 0)
            {
                ImGui::SameLine();
                ImGui::TextColored(green, "%s", record->auto_symbol);
            }

            ImGui::PopID();
            ImGui::TableNextColumn();
            ImGui::TextColored(cyan, "%04X:%08X", entry->src_cs, entry->src);
            ImGui::TableNextColumn();
            ImGui::TextColored(cyan, "%04X:%08X", entry->back_cs, entry->back);
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(gray, "----- Bottom of Stack");
        ImGui::TableNextColumn();
        ImGui::TextColored(gray, "-----");
        ImGui::TableNextColumn();
        ImGui::TextColored(gray, "-----");
        ImGui::PopFont();
        ImGui::EndTable();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_symbols(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(795, 560), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(390, 360), ImGuiCond_FirstUseEver);
    ImGui::Begin("Symbols", &config_debug.show_symbols);

    static char symbol_filter[64] = "";

    ImGui::Checkbox("Automatic Symbols", &config_debug.dis_show_auto_symbols);
    ImGui::SameLine();

    if (!config_debug.dis_show_auto_symbols)
        ImGui::BeginDisabled();

    ImGui::Checkbox("Dim", &config_debug.dis_dim_auto_symbols);

    if (!config_debug.dis_show_auto_symbols)
        ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::PushItemWidth(-1);
    ImGui::InputTextWithHint("##symbol_filter", "Filter...", symbol_filter, IM_ARRAYSIZE(symbol_filter));
    ImGui::PopItemWidth();
    ImGui::Separator();

    I386* cpu = emu_get_core()->GetI386();
    const std::map<u32, I386_Disassembler_Record>& records = cpu->GetDisassemblerRecords();
    ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("symbols_table", 3, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 82.0f);
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 48.0f);
        ImGui::TableHeadersRow();
        ImGui::PushFont(gui_default_font);

        int row = 0;
        std::map<u32, I386_Disassembler_Record>::const_iterator record;

        for (record = records.begin(); record != records.end(); record++)
        {
            if (record->second.auto_symbol[0] == 0)
                continue;

            if (symbol_filter[0] != 0 && strstr(record->second.auto_symbol, symbol_filter) == NULL)
                continue;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID(row++);

            if (ImGui::Selectable("##symbol", false, ImGuiSelectableFlags_SpanAllColumns))
                request_goto_address(record->first);

            ImGui::SameLine(0, 0);
            ImGui::TextColored(cyan, "%08X", record->first);
            ImGui::PopID();
            ImGui::TableNextColumn();
            ImGui::TextColored(config_debug.dis_dim_auto_symbols ? dim_green : green, "%s", record->second.auto_symbol);
            ImGui::TableNextColumn();
            ImGui::TextColored(brown, "Auto");
        }

        ImGui::PopFont();
        ImGui::EndTable();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

static bool parse_address(const char* text, u32& address)
{
    if (!IsValidPointer(text) || text[0] == 0)
        return false;

    char* end = NULL;

    errno = 0;
    unsigned long value = strtoul(text, &end, 16);

    if (errno == ERANGE || end == text || *end != 0 || value > 0xFFFFFFFFUL)
        return false;

    address = (u32)value;
    return true;
}

static void unavailable_tooltip(void)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Not implemented yet in " GT_TITLE);
}

static bool parse_address_range(const char* text, u32& start, u32& end)
{
    if (!IsValidPointer(text))
        return false;

    const char* separator = strchr(text, '-');

    if (!IsValidPointer(separator))
    {
        if (!parse_address(text, start))
            return false;

        end = start;
        return true;
    }

    char first[9];
    size_t length = (size_t)(separator - text);

    if (length == 0 || length >= sizeof(first))
        return false;

    memcpy(first, text, length);
    first[length] = 0;

    return parse_address(first, start) && parse_address(separator + 1, end);
}

static void request_goto_address(u32 address)
{
    goto_address_target = address;
    goto_address_requested = true;
    goto_address_unavailable = false;
}
