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

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "imgui.h"
#include "fonts/IconsMaterialDesign.h"
#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../gui_actions.h"
#include "../gui_filedialogs.h"
#include "gui_debug.h"
#include "gui_debug_constants.h"
#include "gui_debug_i386_tables.h"
#include "../gui_colors.h"

struct DisassemblerLine
{
    const I386_Disassembler_Record* record;
    bool breakpoint;
    bool symbol;
};

static std::vector<DisassemblerLine> disassembler_lines;
static std::vector<DisassemblerBookmark> bookmarks;
static std::map<u32, std::string> user_symbols;
static bool add_symbol_open = false;
static bool add_bookmark_open = false;

static bool selected_address_valid = false;
static u32 selected_address = 0;

static bool goto_address_requested = false;
static u32 goto_address_target = 0;

static bool goto_back_requested = false;
static float goto_back = 0.0f;
static int pc_position = 0;
static int goto_position = 0;

static char new_breakpoint_buffer[20] = "";
static char new_interrupt_buffer[4] = "";
static int new_breakpoint_space = I386_BREAKPOINT_LINEAR;
static int new_interrupt_source = I386_INTERRUPT_ANY;
static int new_irq_line = 0;
static bool new_breakpoint_read = false;
static bool new_breakpoint_write = false;
static bool new_breakpoint_execute = true;
static char goto_address_buffer[9] = "";
static char runto_address_buffer[9] = "";

static bool decode_ahead_valid = false;
static u16 decode_ahead_cs = 0;
static u32 decode_ahead_cs_base = 0;
static u32 decode_ahead_eip = 0;
static u64 decode_ahead_memory_snapshot = 0;
static int decode_ahead_count = 0;


static void disassembler_menu(void);
static void draw_controls(void);
static void draw_breakpoints_content(void);
static void prepare_drawable_lines(void);
static void draw_disassembly(void);
static void draw_instruction(const I386_Disassembler_Record* record, bool breakpoint, bool current_pc);
static void draw_context_menu(DisassemblerLine* line);
static void split_instruction(const char* instruction, char* prefixes, size_t prefixes_size, char* mnemonic,
    size_t mnemonic_size, char* operands, size_t operands_size);
static void draw_operands(const char* operands, bool address_operand, bool override_color, const ImVec4& color,
    const char* label);
static bool replace_jump_target(const I386_Disassembler_Record* record, char* operands, size_t operands_size);
static const char* replace_operand_labels(const I386_Disassembler_Record* record, char* operands, size_t operands_size,
    char* vector_name, size_t vector_name_size, bool& replaced_jump_target, int& port, int& vector);
static void save_full_disassembler(FILE* file);
static void save_current_disassembler(FILE* file);
static void save_instruction(FILE* file, const I386_Disassembler_Record* record, bool segment, bool bytes, bool labels);
static bool parse_address(const char* text, u32& address);
static bool parse_address_range(const char* text, u32& start, u32& end);
static void request_goto_address(u32 address);
static void add_bookmark_popup(void);
static void add_symbol_popup(void);
static bool parse_symbol_line(char* line, u32& address, char* name, size_t name_size);
static bool parse_symbol_address(const char* text, u32& address);
static bool is_symbol_name(const char* text);
static int opcode_index(const I386_Disassembler_Record* record);
static int get_port_operand(const I386_Disassembler_Record* record, bool& immediate);
static void port_tooltip(const I386_Disassembler_Record* record);
static int get_interrupt_vector(const I386_Disassembler_Record* record);
static void interrupt_tooltip(const I386_Disassembler_Record* record, int vector);

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
    add_bookmark_open = true;
}

void gui_debug_add_symbol(void)
{
    add_symbol_open = true;
}

void gui_debug_reset_symbols(void)
{
    user_symbols.clear();
}

bool gui_debug_load_symbols_file(const char* file_path)
{
    return gui_debug_load_symbols(file_path) >= 0;
}

bool gui_debug_add_user_symbol(u32 linear, const char* name)
{
    if (!is_symbol_name(name))
        return false;

    user_symbols[linear] = name;
    return true;
}

bool gui_debug_remove_user_symbol(u32 linear)
{
    return user_symbols.erase(linear) != 0;
}

const char* gui_debug_get_user_symbol(u32 linear)
{
    std::map<u32, std::string>::const_iterator it = user_symbols.find(linear);
    return it != user_symbols.end() ? it->second.c_str() : NULL;
}

const std::map<u32, std::string>& gui_debug_get_user_symbols(void)
{
    return user_symbols;
}

const char* gui_debug_get_symbol(u32 linear)
{
    bool is_manual = false;
    return gui_debug_get_symbol_name(linear, &is_manual);
}

const char* gui_debug_get_symbol_name(u32 linear, bool* is_manual)
{
    const char* user = gui_debug_get_user_symbol(linear);
    *is_manual = IsValidPointer(user);

    if (*is_manual)
        return user;

    I386_Disassembler_Record* record = emu_get_core()->GetI386()->GetDisassemblerRecord(linear);
    return IsValidPointer(record) && record->auto_symbol[0] != 0 ? record->auto_symbol : NULL;
}

bool gui_debug_save_disassembler(const char* file_path, bool full)
{
    FILE* file = fopen_utf8(file_path, "w");

    if (!IsValidPointer(file))
        return false;

    if (full)
        save_full_disassembler(file);
    else
        save_current_disassembler(file);

    fclose(file);
    return true;
}

int gui_debug_load_symbols(const char* file_path)
{
    if (!IsValidPointer(file_path) || file_path[0] == 0)
        return -1;

    FILE* file = fopen_utf8(file_path, "r");

    if (!IsValidPointer(file))
        return -1;

    char line[512];
    int count = 0;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        u32 address = 0;
        char name[64];

        if (parse_symbol_line(line, address, name, sizeof(name)) && gui_debug_add_user_symbol(address, name))
            count++;
    }

    fclose(file);
    Log("Loaded %d symbols from %s", count, file_path);
    return count;
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

void gui_debug_goto_address(u32 address)
{
    config_debug.show_disassembler = true;
    request_goto_address(address);
}

void gui_debug_add_disassembler_bookmark(u32 address, const char* name)
{
    DisassemblerBookmark bookmark;
    bookmark.address = address;

    if (IsValidPointer(name) && name[0] != 0)
        snprintf(bookmark.name, sizeof(bookmark.name), "%s", name);
    else
    {
        I386_Disassembler_Record* record = emu_get_core()->GetI386()->GetDisassemblerRecord(address);

        if (IsValidPointer(record) && record->name[0] != 0)
            snprintf(bookmark.name, sizeof(bookmark.name), "%s", record->name);
        else
            snprintf(bookmark.name, sizeof(bookmark.name), "Bookmark_%08X", address);
    }

    bookmarks.push_back(bookmark);
}

bool gui_debug_remove_disassembler_bookmark(u32 address)
{
    for (std::vector<DisassemblerBookmark>::iterator it = bookmarks.begin(); it != bookmarks.end(); ++it)
    {
        if (it->address == address)
        {
            bookmarks.erase(it);
            return true;
        }
    }

    return false;
}

void gui_debug_reset_disassembler_bookmarks(void)
{
    bookmarks.clear();
}

std::vector<DisassemblerBookmark>* gui_debug_get_disassembler_bookmarks(void)
{
    return &bookmarks;
}

void gui_debug_window_disassembler(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    // The default layout lines up the 80386 window (227 wide), the disassembler and the output 6 pixels apart
    ImGui::SetNextWindowPos(ImVec2(239.0f, gui_main_menu_height + 6.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(534, 575), ImGuiCond_FirstUseEver);

    ImGui::Begin("Intel 80386 Disassembler", &config_debug.show_disassembler, ImGuiWindowFlags_MenuBar);

    disassembler_menu();

    draw_controls();
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Breakpoints"))
        draw_breakpoints_content();

    draw_disassembly();
    add_bookmark_popup();
    add_symbol_popup();

    ImGui::End();
    ImGui::PopStyleVar();
}

static void disassembler_menu(void)
{
    if (!ImGui::BeginMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Save All Disassembled Code As..."))
            gui_file_dialog_save_disassembler(true);

        if (ImGui::MenuItem("Save Current View As..."))
            gui_file_dialog_save_disassembler(false);

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("Opcodes", NULL, &config_debug.dis_show_bytes);
        ImGui::MenuItem("Symbols", NULL, &config_debug.dis_show_symbols);
        ImGui::MenuItem("Segment", NULL, &config_debug.dis_show_segment);

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

        if (ImGui::BeginMenu("Go To Linear Address..."))
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

        ImGui::Separator();

        if (ImGui::BeginMenu("Run To Linear Address..."))
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
            I386* cpu = emu_get_core()->GetI386();

            for (int i = 0; i < 16; i++)
            {
                char label[40];
                bool enabled = cpu->IsIRQBreakpointEnabled(i);

                snprintf(label, sizeof(label), "Break on IRQ %d (%s)", i, k_debug_irq_sources[i]);

                if (ImGui::MenuItem(label, NULL, enabled))
                    cpu->SetIRQBreakpoint(i, !enabled);
            }

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
        if (ImGui::MenuItem("Add Bookmark..."))
        {
            gui_debug_add_bookmark();
        }

        if (ImGui::MenuItem("Remove All"))
        {
            bookmarks.clear();
        }

        if (bookmarks.size() > 0)
            ImGui::Separator();

        for (size_t i = 0; i < bookmarks.size(); i++)
        {
            char label[64];
            snprintf(label, sizeof(label), "$%08X: %s##bookmark%d", bookmarks[i].address, bookmarks[i].name, (int)i);

            if (ImGui::MenuItem(label))
                request_goto_address(bookmarks[i].address);
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Symbols"))
    {
        ImGui::MenuItem("Symbols Window", NULL, &config_debug.show_symbols);

        ImGui::Separator();

        ImGui::MenuItem("Hardware Labels", NULL, &config_debug.dis_replace_labels);

        ImGui::MenuItem("Automatic Symbols", NULL, &config_debug.dis_show_auto_symbols);

        if (!config_debug.dis_show_auto_symbols)
            ImGui::BeginDisabled();

        ImGui::MenuItem("Dim Automatic Symbols", NULL, &config_debug.dis_dim_auto_symbols);

        if (!config_debug.dis_show_auto_symbols)
            ImGui::EndDisabled();

        ImGui::MenuItem("Replace Address With Symbol", NULL, &config_debug.dis_replace_symbols);

        ImGui::Separator();

        if (ImGui::MenuItem("Add Symbol..."))
            gui_debug_add_symbol();

        if (ImGui::MenuItem("Load Symbols..."))
            gui_file_dialog_load_symbols();

        if (ImGui::MenuItem("Clear Symbols"))
            gui_debug_reset_symbols();

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

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Reset (%s)", config_hotkeys[config_HotkeyIndex_Reset].str);

    ImGui::PopFont();

    ImGui::SameLine();
    ImGui::TextColored(emu_is_debug_idle() ? red : green, emu_is_debug_idle() ? "   PAUSED" : "   RUNNING");
}

static void draw_breakpoints_content(void)
{
    static const char* k_spaces[I386_BREAKPOINT_SPACE_COUNT] = { "LINEAR", "PHYSICAL", "I/O" };
    static const char* k_sources[I386_INTERRUPT_SOURCE_COUNT] = { "ANY", "EXCEPTION", "HARDWARE", "SOFTWARE" };
    I386* cpu = emu_get_core()->GetI386();
    I386_Breakpoint_Hit hit;

    ImGui::Checkbox("Disable All##disable_breakpoints", &emu_debug_disable_breakpoints);
    ImGui::SameLine();

    if (ImGui::Button("Remove All##clear_breakpoints", ImVec2(85, 0)))
        gui_debug_reset_breakpoints();

    ImGui::SameLine();
    ImGui::PushFont(gui_default_font);
    ImGui::TextColored(violet, " LAST HIT"); ImGui::SameLine();

    if (cpu->GetBreakpointHit(hit) && hit.interrupt && hit.line < 16)
        ImGui::TextColored(yellow, "IRQ%d %s INT $%02X", hit.line, k_debug_irq_sources[hit.line], hit.vector);
    else if (cpu->GetBreakpointHit(hit) && hit.interrupt)
    {
        char name[16];
        char description[64];
        gui_debug_i386_vector_name(hit.vector, name, sizeof(name), description, sizeof(description));
        ImGui::TextColored(yellow, "INT $%02X %s %s", hit.vector, name, k_sources[hit.source % I386_INTERRUPT_SOURCE_COUNT]);

        const char* function = hit.source == I386_INTERRUPT_SOFTWARE ?
            gui_debug_i386_interrupt_function(hit.vector, hit.ax) : NULL;

        if (IsValidPointer(function))
        {
            ImGui::SameLine();
            ImGui::TextColored(gray, "%s", function);
        }
    }
    else if (cpu->GetBreakpointHit(hit))
        ImGui::TextColored(yellow, "%s %s %0*X", hit.type == I386_BREAKPOINT_EXECUTE ? "EXEC" :
            hit.type == I386_BREAKPOINT_WRITE ? "WRITE" : "READ", k_spaces[hit.space % I386_BREAKPOINT_SPACE_COUNT],
            hit.space == I386_BREAKPOINT_IO ? 4 : 8, hit.address);
    else
        ImGui::TextColored(gray, "--");

    ImGui::PopFont();

    ImGui::Separator();
    ImGui::Columns(2, "breakpoints");
    ImGui::SetColumnOffset(1, 175);

    ImGui::PushItemWidth(145);

    if (ImGui::Combo("##breakpoint_space", &new_breakpoint_space, "LINEAR\0PHYSICAL\0I/O\0\0"))
    {
        new_breakpoint_read = new_breakpoint_space != I386_BREAKPOINT_LINEAR;
        new_breakpoint_write = false;
        new_breakpoint_execute = new_breakpoint_space == I386_BREAKPOINT_LINEAR;
    }

    bool add = ImGui::InputTextWithHint("##add_breakpoint", "ADDR[-ADDR]", new_breakpoint_buffer,
        IM_ARRAYSIZE(new_breakpoint_buffer), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Hex address or range: 1234ABCD, 8000-8FFF\nI/O ports: 0000-FFFF");

    ImGui::Checkbox("R##breakpoint_read", &new_breakpoint_read); ImGui::SameLine();
    ImGui::Checkbox("W##breakpoint_write", &new_breakpoint_write);

    if (new_breakpoint_space == I386_BREAKPOINT_LINEAR)
    {
        ImGui::SameLine();
        ImGui::Checkbox("X##breakpoint_execute", &new_breakpoint_execute);
    }

    u8 data = (new_breakpoint_read ? I386_BREAKPOINT_READ : 0) | (new_breakpoint_write ? I386_BREAKPOINT_WRITE : 0);
    bool execute = new_breakpoint_execute && new_breakpoint_space == I386_BREAKPOINT_LINEAR;

    ImGui::BeginDisabled(data == 0 && !execute);

    if (ImGui::Button("Add##add_breakpoint_button", ImVec2(85, 0)))
        add = true;

    ImGui::EndDisabled();

    if (add)
    {
        u32 start = 0;
        u32 end = 0;

        if (parse_address_range(new_breakpoint_buffer, start, end) && (data != 0 || execute))
        {
            bool ok = true;

            if (execute)
                ok = cpu->AddBreakpoint(start, end, I386_BREAKPOINT_EXECUTE, I386_BREAKPOINT_LINEAR);

            if (data != 0)
                ok = cpu->AddBreakpoint(start, end, data, (u8)new_breakpoint_space) && ok;

            if (ok)
                new_breakpoint_buffer[0] = 0;
        }
    }

    ImGui::NextColumn();
    ImGui::BeginChild("breakpoint_list", ImVec2(0, 130), false);
    ImGui::PushFont(gui_default_font);

    std::vector<I386_Breakpoint>* breakpoints = cpu->GetBreakpoints();
    int remove = -1;

    for (size_t i = 0; i < breakpoints->size(); i++)
    {
        I386_Breakpoint& breakpoint = (*breakpoints)[i];
        ImVec4 color = breakpoint.enabled ? cyan : gray;
        int digits = breakpoint.space == I386_BREAKPOINT_IO ? 4 : 8;

        ImGui::PushID((int)i);

        if (ImGui::SmallButton("X"))
            remove = (int)i;

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Remove breakpoint");

        ImGui::SameLine();

        if (ImGui::SmallButton(breakpoint.enabled ? "-" : "+"))
            breakpoint.enabled = !breakpoint.enabled;

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(breakpoint.enabled ? "Disable breakpoint" : "Enable breakpoint");

        ImGui::SameLine();
        ImGui::TextColored(breakpoint.enabled ? violet : gray, "%-8s", k_spaces[breakpoint.space % I386_BREAKPOINT_SPACE_COUNT]);
        ImGui::SameLine();
        ImGui::TextColored(breakpoint.enabled ? orange : gray, "%c%c%c", (breakpoint.type & I386_BREAKPOINT_READ) ? 'R' : '-',
            (breakpoint.type & I386_BREAKPOINT_WRITE) ? 'W' : '-', (breakpoint.type & I386_BREAKPOINT_EXECUTE) ? 'X' : '-');
        ImGui::SameLine();

        if (breakpoint.range)
            ImGui::TextColored(color, "%0*X-%0*X", digits, breakpoint.address1, digits, breakpoint.address2);
        else
        {
            char address[9];
            snprintf(address, sizeof(address), "%0*X", digits, breakpoint.address1);
            ImGui::TextColored(color, "%-8s", address);

            if (breakpoint.space == I386_BREAKPOINT_IO)
            {
                const char* label = gui_debug_port_label((u16)breakpoint.address1);

                if (IsValidPointer(label))
                {
                    ImGui::SameLine();
                    ImGui::TextColored(breakpoint.enabled ? green : gray, "%s", label);
                }
            }
            else if (breakpoint.type == I386_BREAKPOINT_EXECUTE)
            {
                const char* symbol = gui_debug_get_symbol(breakpoint.address1);

                if (IsValidPointer(symbol))
                {
                    ImGui::SameLine();
                    ImGui::TextColored(breakpoint.enabled ? green : gray, "%s", symbol);
                }
            }
        }

        ImGui::PopID();
    }

    if (remove >= 0)
    {
        I386_Breakpoint breakpoint = (*breakpoints)[remove];
        cpu->RemoveBreakpoint(breakpoint.address1, breakpoint.address2, breakpoint.type, breakpoint.space);
    }

    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::Columns(1);
    ImGui::Separator();

    ImGui::Columns(2, "interrupt_breakpoints");
    ImGui::SetColumnOffset(1, 175);

    ImGui::PushItemWidth(145);
    ImGui::Combo("##interrupt_source", &new_interrupt_source, "ANY\0EXCEPTION\0HARDWARE\0SOFTWARE\0\0");
    bool add_interrupt = ImGui::InputTextWithHint("##add_interrupt", "VECTOR", new_interrupt_buffer,
        IM_ARRAYSIZE(new_interrupt_buffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::PopItemWidth();

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Hex vector 00-FF, stops before the handler's first instruction");

    if (ImGui::Button("Add##add_interrupt_button", ImVec2(85, 0)))
        add_interrupt = true;

    if (add_interrupt && new_interrupt_buffer[0] != 0)
    {
        char* end = NULL;
        unsigned long vector = strtoul(new_interrupt_buffer, &end, 16);

        if (IsValidPointer(end) && *end == 0 && vector <= 0xFF &&
            cpu->AddInterruptBreakpoint((u8)vector, (u8)new_interrupt_source))
            new_interrupt_buffer[0] = 0;
    }

    char irq_text[32];
    snprintf(irq_text, sizeof(irq_text), "IRQ%d %s", new_irq_line, k_debug_irq_sources[new_irq_line]);
    ImGui::PushItemWidth(145);

    if (ImGui::BeginCombo("##irq_line", irq_text))
    {
        for (int i = 0; i < 16; i++)
        {
            snprintf(irq_text, sizeof(irq_text), "IRQ%d %s", i, k_debug_irq_sources[i]);

            if (ImGui::Selectable(irq_text, i == new_irq_line))
                new_irq_line = i;
        }

        ImGui::EndCombo();
    }

    ImGui::PopItemWidth();

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("IRQ line, whatever vector the PIC gives it");

    if (ImGui::Button("Add##add_irq_button", ImVec2(85, 0)))
        cpu->SetIRQBreakpoint(new_irq_line, true);

    ImGui::NextColumn();
    ImGui::BeginChild("interrupt_breakpoint_list", ImVec2(0, 115), false);
    ImGui::PushFont(gui_default_font);

    std::vector<I386_Interrupt_Breakpoint>* interrupts = cpu->GetInterruptBreakpoints();
    int remove_interrupt = -1;

    for (size_t i = 0; i < interrupts->size(); i++)
    {
        I386_Interrupt_Breakpoint& breakpoint = (*interrupts)[i];
        char name[16];
        char description[64];
        gui_debug_i386_vector_name(breakpoint.vector, name, sizeof(name), description, sizeof(description));

        ImGui::PushID((int)i + 0x1000);

        if (ImGui::SmallButton("X"))
            remove_interrupt = (int)i;

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Remove breakpoint");

        ImGui::SameLine();

        if (ImGui::SmallButton(breakpoint.enabled ? "-" : "+"))
            breakpoint.enabled = !breakpoint.enabled;

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(breakpoint.enabled ? "Disable breakpoint" : "Enable breakpoint");

        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextColored(breakpoint.enabled ? cyan : gray, "INT $%02X", breakpoint.vector); ImGui::SameLine();
        ImGui::TextColored(breakpoint.enabled ? orange : gray, "%-14s", name); ImGui::SameLine();
        ImGui::TextColored(breakpoint.enabled ? violet : gray, "%s", k_sources[breakpoint.source % I386_INTERRUPT_SOURCE_COUNT]);
        ImGui::EndGroup();

        if (description[0] != 0 && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", description);

        ImGui::PopID();
    }

    if (remove_interrupt >= 0)
    {
        I386_Interrupt_Breakpoint breakpoint = (*interrupts)[remove_interrupt];
        cpu->RemoveInterruptBreakpoint(breakpoint.vector, breakpoint.source);
    }

    int remove_irq = -1;

    for (int line = 0; line < 16; line++)
    {
        if (!cpu->IsIRQBreakpoint(line))
            continue;

        char line_text[8];
        snprintf(line_text, sizeof(line_text), "IRQ%d", line);

        bool enabled = cpu->IsIRQBreakpointEnabled(line);

        ImGui::PushID(line + 0x2000);

        if (ImGui::SmallButton("X"))
            remove_irq = line;

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Remove breakpoint");

        ImGui::SameLine();

        if (ImGui::SmallButton(enabled ? "-" : "+"))
            cpu->EnableIRQBreakpoint(line, !enabled);

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(enabled ? "Disable breakpoint" : "Enable breakpoint");

        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextColored(enabled ? cyan : gray, "%-7s", line_text); ImGui::SameLine();
        ImGui::TextColored(enabled ? orange : gray, "%-14s", k_debug_irq_sources[line]); ImGui::SameLine();
        ImGui::TextColored(enabled ? violet : gray, "LINE");
        ImGui::EndGroup();

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Breaks on this IRQ line whatever vector the PIC gives it");

        ImGui::PopID();
    }

    if (remove_irq >= 0)
        cpu->SetIRQBreakpoint(remove_irq, false);

    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::Columns(1);
    ImGui::Separator();
}

void gui_debug_window_breakpoints(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(182, 234), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(594, 346), ImGuiCond_FirstUseEver);
    ImGui::Begin("Breakpoints", &config_debug.show_breakpoints);

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

        bool user_symbol = IsValidPointer(gui_debug_get_user_symbol(record->first));

        if (config_debug.dis_show_symbols &&
            (user_symbol || (config_debug.dis_show_auto_symbols && record->second.auto_symbol[0] != 0)))
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

            if (goto_position >= 0)
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
                    const char* user = gui_debug_get_user_symbol(record->linear);

                    if (IsValidPointer(user))
                        ImGui::TextColored(green, "%s:", user);
                    else
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
    const int bytes_column = 29;
    char prefixes[24];
    char mnemonic[32];
    char operands[128];
    char vector_name[16];
    bool replaced_jump_target = false;
    int port = -1;
    int vector = -1;

    split_instruction(record->name, prefixes, sizeof(prefixes), mnemonic, sizeof(mnemonic), operands, sizeof(operands));

    const char* operand_label = replace_operand_labels(record, operands, sizeof(operands), vector_name,
        sizeof(vector_name), replaced_jump_target, port, vector);

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

        // One group, so the tooltips below see the whole operand text and not only its last token
        ImGui::SameLine(0.0f, space_width);
        ImGui::BeginGroup();
        draw_operands(operands, address_operand, override_color, mnemonic_color, operand_label);
        ImGui::EndGroup();
        instruction_length += 1 + (int)strlen(operands);
    }

    if (port >= 0 && ImGui::IsItemHovered())
        port_tooltip(record);

    if (vector >= 0 && ImGui::IsItemHovered())
        interrupt_tooltip(record, vector);

    if (config_debug.dis_show_bytes)
    {
        int spacing = bytes_column - instruction_length;

        if (spacing < 2)
            spacing = 2;

        ImGui::SameLine(0.0f, space_width * spacing);
        ImGui::TextColored(bytes_color, "; %s", record->bytes);
    }
}

// Instruction text in the window colors, for other views that show it without its record
void gui_debug_disassembler_draw_text(const char* instruction)
{
    char prefixes[24];
    char mnemonic[32];
    char operands[128];
    float space_width = ImGui::CalcTextSize(" ").x;

    split_instruction(instruction, prefixes, sizeof(prefixes), mnemonic, sizeof(mnemonic), operands, sizeof(operands));

    if (prefixes[0] != 0)
    {
        ImGui::TextColored(blue, "%s", prefixes);
        ImGui::SameLine(0.0f, space_width);
    }

    ImGui::TextColored(white, "%s", mnemonic);

    if (operands[0] != 0)
    {
        ImGui::SameLine(0.0f, space_width);
        ImGui::BeginGroup();
        draw_operands(operands, false, false, white, NULL);
        ImGui::EndGroup();
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

static void draw_operands(const char* operands, bool address_operand, bool override_color, const ImVec4& color,
    const char* label)
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
            else if (IsValidPointer(label) && text_equals(start, cursor, label))
                token_color = orange;
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

// Jump targets, ports and interrupt vectors by name, as the window shows them
static const char* replace_operand_labels(const I386_Disassembler_Record* record, char* operands, size_t operands_size,
    char* vector_name, size_t vector_name_size, bool& replaced_jump_target, int& port, int& vector)
{
    replaced_jump_target = replace_jump_target(record, operands, operands_size);
    bool immediate_port = false;
    port = get_port_operand(record, immediate_port);
    const char* operand_label = port >= 0 && immediate_port ? gui_debug_port_label((u16)port) : NULL;

    if (IsValidPointer(operand_label) && config_debug.dis_replace_labels)
    {
        char port_text[8];
        snprintf(port_text, sizeof(port_text), "0x%02X", port);
        char* found = strstr(operands, port_text);

        if (IsValidPointer(found))
        {
            char replaced[128];
            snprintf(replaced, sizeof(replaced), "%.*s%s%s", (int)(found - operands), operands, operand_label,
                found + strlen(port_text));
            snprintf(operands, operands_size, "%s", replaced);
        }
    }
    else
        operand_label = NULL;

    vector = get_interrupt_vector(record);
    vector_name[0] = 0;
    char vector_description[64];

    if (vector >= 0)
        gui_debug_i386_vector_name((u8)vector, vector_name, vector_name_size, vector_description,
            sizeof(vector_description));

    if (vector >= 0 && config_debug.dis_replace_labels && strcmp(vector_name, "INT") != 0 &&
        record->opcodes[opcode_index(record)] == 0xCD)
    {
        char vector_text[8];
        snprintf(vector_text, sizeof(vector_text), "0x%02X", vector);
        char* found = strstr(operands, vector_text);

        if (IsValidPointer(found))
        {
            char replaced[128];
            snprintf(replaced, sizeof(replaced), "%.*s%s%s", (int)(found - operands), operands, vector_name,
                found + strlen(vector_text));
            snprintf(operands, operands_size, "%s", replaced);
            operand_label = vector_name;
        }
    }

    return operand_label;
}

static bool replace_jump_target(const I386_Disassembler_Record* record, char* operands, size_t operands_size)
{
    if (!config_debug.dis_replace_symbols || !record->jump || !record->jump_target_known)
        return false;

    const char* name = gui_debug_get_user_symbol(record->jump_linear);

    if (!IsValidPointer(name) && config_debug.dis_show_auto_symbols)
    {
        I386_Disassembler_Record* target = emu_get_core()->GetI386()->GetDisassemblerRecord(record->jump_linear);

        if (IsValidPointer(target) && target->auto_symbol[0] != 0)
            name = target->auto_symbol;
    }

    if (!IsValidPointer(name))
        return false;

    char symbol[64];
    snprintf(symbol, sizeof(symbol), "%s", name);
    const char* space = strchr(operands, ' ');

    if (IsValidPointer(space) && text_equals(operands, space, "FAR"))
        snprintf(operands, operands_size, "FAR %s", symbol);
    else if (IsValidPointer(space) && text_equals(operands, space, "NEAR"))
        snprintf(operands, operands_size, "NEAR %s", symbol);
    else if (IsValidPointer(space) && text_equals(operands, space, "SHORT"))
        snprintf(operands, operands_size, "SHORT %s", symbol);
    else
        snprintf(operands, operands_size, "%s", symbol);

    return true;
}

static void draw_context_menu(DisassemblerLine* line)
{
    if (!ImGui::BeginPopupContextItem())
        return;

    selected_address_valid = true;
    selected_address = line->record->linear;

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
    ImGui::SetNextWindowPos(ImVec2(223, 101), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(377, 157), ImGuiCond_FirstUseEver);
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
    ImGui::SetNextWindowPos(ImVec2(264, 168), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(389, 336), ImGuiCond_FirstUseEver);
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
        u32 remove_user = 0;
        bool remove_user_requested = false;
        std::map<u32, std::string>::const_iterator user;

        for (user = user_symbols.begin(); user != user_symbols.end(); user++)
        {
            if (symbol_filter[0] != 0 && strstr(user->second.c_str(), symbol_filter) == NULL)
                continue;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID(row++);

            if (ImGui::Selectable("##symbol", false, ImGuiSelectableFlags_SpanAllColumns))
                request_goto_address(user->first);

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Remove Symbol"))
                {
                    remove_user = user->first;
                    remove_user_requested = true;
                }

                ImGui::EndPopup();
            }

            ImGui::SameLine(0, 0);
            ImGui::TextColored(cyan, "%08X", user->first);
            ImGui::PopID();
            ImGui::TableNextColumn();
            ImGui::TextColored(green, "%s", user->second.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(orange, "User");
        }

        if (remove_user_requested)
            gui_debug_remove_user_symbol(remove_user);

        std::map<u32, I386_Disassembler_Record>::const_iterator record;

        for (record = records.begin(); record != records.end(); record++)
        {
            if (record->second.auto_symbol[0] == 0 || !config_debug.dis_show_auto_symbols)
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

static void save_full_disassembler(FILE* file)
{
    const std::map<u32, I386_Disassembler_Record>& records = emu_get_core()->GetI386()->GetDisassemblerRecords();
    std::map<u32, I386_Disassembler_Record>::const_iterator record;

    for (record = records.begin(); record != records.end(); record++)
    {
        if (record->second.name[0] == 0)
            continue;

        const char* symbol = gui_debug_get_symbol(record->first);

        if (IsValidPointer(symbol))
            fprintf(file, "%s:\n", symbol);

        save_instruction(file, &record->second, true, true, false);
    }
}

static void save_current_disassembler(FILE* file)
{
    prepare_drawable_lines();

    for (size_t i = 0; i < disassembler_lines.size(); i++)
    {
        const DisassemblerLine& line = disassembler_lines[i];

        if (line.symbol)
        {
            const char* user = gui_debug_get_user_symbol(line.record->linear);
            fprintf(file, "%s:\n", IsValidPointer(user) ? user : line.record->auto_symbol);
            continue;
        }

        save_instruction(file, line.record, config_debug.dis_show_segment, config_debug.dis_show_bytes, true);
    }
}

static void save_instruction(FILE* file, const I386_Disassembler_Record* record, bool segment, bool bytes, bool labels)
{
    const int bytes_column = 29;
    char prefixes[24];
    char mnemonic[32];
    char operands[128];
    char vector_name[16];
    bool replaced_jump_target = false;
    int port = -1;
    int vector = -1;

    split_instruction(record->name, prefixes, sizeof(prefixes), mnemonic, sizeof(mnemonic), operands, sizeof(operands));

    if (labels)
        replace_operand_labels(record, operands, sizeof(operands), vector_name, sizeof(vector_name),
            replaced_jump_target, port, vector);

    char instruction[192];
    snprintf(instruction, sizeof(instruction), "%s%s%s%s%s", prefixes, prefixes[0] != 0 ? " " : "", mnemonic,
        operands[0] != 0 ? " " : "", operands);

    if (segment)
        fprintf(file, "%s:%08X ", record->segment, record->eip);

    fprintf(file, "%08X  %s", record->linear, instruction);

    if (bytes)
    {
        int spacing = bytes_column - (int)strlen(instruction);

        if (spacing < 2)
            spacing = 2;

        fprintf(file, "%*s; %s", spacing, "", record->bytes);
    }

    fputs(record->returns ? "\n\n" : "\n", file);
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

static int opcode_index(const I386_Disassembler_Record* record)
{
    int index = 0;

    while (index < record->size)
    {
        u8 byte = record->opcodes[index];

        if (byte != 0x26 && byte != 0x2E && byte != 0x36 && byte != 0x3E && byte != 0x64 && byte != 0x65 &&
            byte != 0x66 && byte != 0x67 && byte != 0xF0 && byte != 0xF2 && byte != 0xF3)
            break;

        index++;
    }

    return index;
}

static int get_port_operand(const I386_Disassembler_Record* record, bool& immediate)
{
    int index = opcode_index(record);

    if (index >= record->size)
        return -1;

    u8 opcode = record->opcodes[index];
    immediate = opcode >= 0xE4 && opcode <= 0xE7;

    if (immediate)
        return index + 1 < record->size ? record->opcodes[index + 1] : -1;

    bool string_io = opcode == 0x6C || opcode == 0x6D || opcode == 0x6E || opcode == 0x6F;

    if ((opcode < 0xEC || opcode > 0xEF) && !string_io)
        return -1;

    I386* cpu = emu_get_core()->GetI386();

    if (record->linear != cpu->GetCurrentLinearPC())
        return -2;

    I386_Debug_State state;
    cpu->CopyDebugState(state);
    return (int)(state.edx & 0xFFFF);
}

// INT n, INT3, INTO and ICEBP, -1 for anything else
static int get_interrupt_vector(const I386_Disassembler_Record* record)
{
    int index = opcode_index(record);

    if (index >= record->size)
        return -1;

    switch (record->opcodes[index])
    {
        case 0xCD:
            return index + 1 < record->size ? record->opcodes[index + 1] : -1;
        case 0xCC:
            return 3;
        case 0xCE:
            return 4;
        case 0xF1:
            return 1;
        default:
            return -1;
    }
}

// The function comes from AX, so it only shows on the current instruction
static void interrupt_tooltip(const I386_Disassembler_Record* record, int vector)
{
    char name[16];
    char description[64];
    gui_debug_i386_vector_name((u8)vector, name, sizeof(name), description, sizeof(description));

    ImGui::BeginTooltip();
    ImGui::TextColored(cyan, "INT $%02X", vector);
    ImGui::TextColored(orange, "%s", name);
    ImGui::Text("%s", description);

    I386* cpu = emu_get_core()->GetI386();

    if (record->linear == cpu->GetCurrentLinearPC())
    {
        I386_Debug_State state;
        cpu->CopyDebugState(state);
        const char* function = gui_debug_i386_interrupt_function((u8)vector, state.eax);

        if (IsValidPointer(function))
            ImGui::TextColored(green, "AX=%04X %s", state.eax & 0xFFFF, function);
    }

    ImGui::EndTooltip();
}

static void port_tooltip(const I386_Disassembler_Record* record)
{
    bool immediate = false;
    int port = get_port_operand(record, immediate);

    if (port < 0)
        return;

    const stDebugPortLabel* label = gui_debug_find_port_label((u16)port);

    ImGui::BeginTooltip();
    ImGui::TextColored(cyan, "PORT $%04X", port);

    if (IsValidPointer(label))
    {
        ImGui::TextColored(orange, "%s", label->label);
        ImGui::Text("%s", label->description);
    }
    else if (IsValidPointer(gui_debug_port_label((u16)port)))
        ImGui::TextColored(orange, "%s", gui_debug_port_label((u16)port));
    else
        ImGui::TextColored(gray, "Not decoded by the board");

    ImGui::EndTooltip();
}

static void add_bookmark_popup(void)
{
    if (add_bookmark_open)
    {
        ImGui::OpenPopup("Add Bookmark");
        add_bookmark_open = false;
    }

    if (ImGui::BeginPopupModal("Add Bookmark", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char address_bookmark[9] = "";
        static char name_bookmark[32] = "";
        static bool bookmark_modified = false;

        if (!bookmark_modified && selected_address_valid)
            snprintf(address_bookmark, sizeof(address_bookmark), "%08X", selected_address);

        ImGui::Text("Name:");
        ImGui::PushItemWidth(200);
        ImGui::SetItemDefaultFocus();
        ImGui::InputText("##name", name_bookmark, IM_ARRAYSIZE(name_bookmark));
        ImGui::PopItemWidth();

        ImGui::Text("Linear Address:");
        ImGui::PushItemWidth(80);

        if (ImGui::InputTextWithHint("##bookaddr", "XXXXXXXX", address_bookmark, IM_ARRAYSIZE(address_bookmark),
            ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase))
        {
            bookmark_modified = true;
        }

        ImGui::PopItemWidth();

        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(90, 0)))
        {
            u32 address = 0;

            if (parse_address(address_bookmark, address))
            {
                gui_debug_add_disassembler_bookmark(address, name_bookmark);
                ImGui::CloseCurrentPopup();
                address_bookmark[0] = 0;
                name_bookmark[0] = 0;
                bookmark_modified = false;
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(90, 0)))
        {
            ImGui::CloseCurrentPopup();
            address_bookmark[0] = 0;
            name_bookmark[0] = 0;
            bookmark_modified = false;
        }

        ImGui::EndPopup();
    }
}

static void request_goto_address(u32 address)
{
    goto_address_target = address;
    goto_address_requested = true;
}

static void add_symbol_popup(void)
{
    if (add_symbol_open)
    {
        ImGui::OpenPopup("Add Symbol");
        add_symbol_open = false;
    }

    if (ImGui::BeginPopupModal("Add Symbol", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char address_symbol[9] = "";
        static char name_symbol[64] = "";
        static bool symbol_modified = false;

        if (!symbol_modified && selected_address_valid)
            snprintf(address_symbol, sizeof(address_symbol), "%08X", selected_address);

        ImGui::Text("Name:");
        ImGui::PushItemWidth(200);
        ImGui::SetItemDefaultFocus();
        ImGui::InputText("##symbol_name", name_symbol, IM_ARRAYSIZE(name_symbol));
        ImGui::PopItemWidth();

        ImGui::Text("Linear Address:");
        ImGui::PushItemWidth(80);

        if (ImGui::InputTextWithHint("##symbol_address", "XXXXXXXX", address_symbol, IM_ARRAYSIZE(address_symbol),
            ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase))
        {
            symbol_modified = true;
        }

        ImGui::PopItemWidth();
        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(90, 0)))
        {
            u32 address = 0;

            if (parse_address(address_symbol, address) && gui_debug_add_user_symbol(address, name_symbol))
            {
                ImGui::CloseCurrentPopup();
                address_symbol[0] = 0;
                name_symbol[0] = 0;
                symbol_modified = false;
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(90, 0)))
        {
            ImGui::CloseCurrentPopup();
            address_symbol[0] = 0;
            name_symbol[0] = 0;
            symbol_modified = false;
        }

        ImGui::EndPopup();
    }
}

static bool parse_symbol_line(char* line, u32& address, char* name, size_t name_size)
{
    char* comment = strpbrk(line, ";#");

    if (IsValidPointer(comment))
        *comment = 0;

    char* tokens[3];
    int count = 0;
    char* cursor = line;

    while (count < 3)
    {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')
            cursor++;

        if (*cursor == 0)
            break;

        tokens[count++] = cursor;

        while (*cursor != 0 && *cursor != ' ' && *cursor != '\t' && *cursor != '\r' && *cursor != '\n')
            cursor++;

        if (*cursor != 0)
            *cursor++ = 0;
    }

    const char* symbol = NULL;
    const char* value = NULL;

    if (count == 3 && (strcmp(tokens[1], "=") == 0 || (strlen(tokens[1]) == 3 && ends_with_no_case(tokens[1], "EQU"))))
    {
        symbol = tokens[0];
        value = tokens[2];
    }
    else if (count == 2)
    {
        symbol = tokens[1];
        value = tokens[0];
    }

    if (!IsValidPointer(symbol) || !is_symbol_name(symbol) || !parse_symbol_address(value, address))
        return false;

    snprintf(name, name_size, "%s", symbol);
    return true;
}

static bool parse_symbol_address(const char* text, u32& address)
{
    char value[32];
    snprintf(value, sizeof(value), "%s", text);
    char* colon = strchr(value, ':');

    if (IsValidPointer(colon))
    {
        *colon = 0;
        u32 selector = 0;
        u32 offset = 0;
        u32 base = 0;
        u32 limit = 0;
        char reason[GT_DEBUG_MEMORY_REASON_SIZE];

        if (!parse_address(value, selector) || selector > 0xFFFF || !parse_address(colon + 1, offset) ||
            !gui_debug_i386_selector_base((u16)selector, base, limit, reason, sizeof(reason)))
            return false;

        address = base + offset;
        return true;
    }

    char* start = value;
    size_t length = strlen(start);

    if (start[0] == '$')
        start++;
    else if (start[0] == '0' && (start[1] == 'x' || start[1] == 'X'))
        start += 2;
    else if (length > 1 && (start[length - 1] == 'h' || start[length - 1] == 'H'))
        start[length - 1] = 0;

    return parse_address(start, address);
}

static bool is_symbol_name(const char* text)
{
    if (!IsValidPointer(text) || text[0] == 0 || strlen(text) >= 64 || isdigit((unsigned char)text[0]))
        return false;

    for (const char* c = text; *c != 0; c++)
    {
        if (!isalnum((unsigned char)*c) && *c != '_' && *c != '.' && *c != '@' && *c != '?' && *c != '$')
            return false;
    }

    return true;
}
