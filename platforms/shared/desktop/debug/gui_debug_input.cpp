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

#define GUI_DEBUG_INPUT_IMPORT
#include "gui_debug_input.h"

#include "imgui.h"
#include "geartowns.h"
#include "input/input.h"
#include "input/keyboard.h"
#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../gui_colors.h"
#include "gui_debug_constants.h"

static void draw_grid_label(const char* label);
static void draw_game_port(Input* input, int port);
static void draw_button(const char* name, u16 mask, u16 buttons, u16 present);

void gui_debug_window_keyboard(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(217, 179), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(216, 332), ImGuiCond_FirstUseEver);
    ImGui::Begin("Keyboard", &config_debug.show_keyboard);

    ImGui::PushFont(gui_default_font);

    Keyboard* keyboard = emu_get_core()->GetKeyboard();
    Keyboard::Keyboard_State* state = keyboard->GetState();
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    float character = ImGui::CalcTextSize("0").x;
    float space = ImGui::CalcTextSize(" ").x;

    ImGui::TextColored(cyan, "INTERFACE (0600-0604)"); ImGui::Separator();

    if (ImGui::BeginTable("##keyboard_interface", 4, flags))
    {
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 12);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 4);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 6);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 3);

        ImGui::TableNextRow();
        draw_grid_label("DATA");
        ImGui::TextColored(state->fifo_count ? white : gray, "$%02X", keyboard->Peek(0x0600));
        draw_grid_label("STATUS");
        ImGui::TextColored(white, "$%02X", keyboard->Peek(0x0602));

        ImGui::TableNextRow();
        draw_grid_label("IRQ ENABLE");
        ImGui::TextColored(state->irq_enabled ? green : gray, "%s", state->irq_enabled ? "ON" : "OFF");
        draw_grid_label("KBINT");
        ImGui::TextColored(state->kbint ? yellow : gray, "%s", state->kbint ? "ON" : "OFF");

        ImGui::TableNextRow();
        draw_grid_label("LAST COMMAND");
        ImGui::TextColored(white, "$%02X", state->last_command);

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "QUEUE"); ImGui::Separator();

    for (int i = 0; i < KEYBOARD_FIFO_SIZE; i++)
    {
        if ((i & 7) != 0)
            ImGui::SameLine(0.0f, space);

        if (i < state->fifo_count)
        {
            u8 value = state->fifo[(state->fifo_read + i) & (KEYBOARD_FIFO_SIZE - 1)];
            bool event = (value & 0x80) != 0;

            ImGui::TextColored(event ? orange : white, "%02X", value);

            if (ImGui::IsItemHovered())
            {
                if (event)
                    ImGui::SetTooltip("%s%s%s", (value & 0x10) ? "BREAK" : "MAKE", (value & 0x08) ? " +CTRL" : "",
                        (value & 0x04) ? " +SHIFT" : "");
                else
                {
                    const char* name = gui_debug_key_name(value);
                    ImGui::SetTooltip("%s", IsValidPointer(name) ? name : "UNKNOWN");
                }
            }
        }
        else
            ImGui::TextColored(gray, "--");
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "PRESSED"); ImGui::Separator();

    if (ImGui::BeginTable("##keyboard_pressed", 4, flags))
    {
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 2);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 17);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 2);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 16);

        int pressed = 0;

        for (int key = GT_KEY_NONE + 1; key < GT_KEY_COUNT && pressed < 8; key++)
        {
            if (!state->keys[key])
                continue;

            const char* name = gui_debug_key_name(key);

            if ((pressed & 1) == 0)
                ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextColored(white, "%02X", key);
            ImGui::TableNextColumn();
            ImGui::TextColored(green, "%s", IsValidPointer(name) ? name : "UNKNOWN");
            pressed++;
        }

        for (; pressed < 8; pressed++)
        {
            if ((pressed & 1) == 0)
                ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextColored(gray, "--");
            ImGui::TableNextColumn();
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_game_ports(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(240, 179), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(221, 436), ImGuiCond_FirstUseEver);
    ImGui::Begin("Game Ports", &config_debug.show_game_ports);

    ImGui::PushFont(gui_default_font);

    Input* input = emu_get_core()->GetInput();
    Input::Input_State* state = input->GetState();
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    float character = ImGui::CalcTextSize("0").x;

    ImGui::TextColored(cyan, "OUTPUT (04D6)"); ImGui::Separator();

    if (ImGui::BeginTable("##game_ports_output", 2, flags))
    {
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 8);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 22);

        ImGui::TableNextRow();
        draw_grid_label("OUTPUT");
        ImGui::TextColored(white, "$%02X", state->output);

        ImGui::EndTable();
    }

    for (int port = 0; port < GT_MAX_GAMEPADS; port++)
        draw_game_port(input, port);

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

static void draw_game_port(Input* input, int port)
{
    static const char* k_types[5] = { "NONE", "ORIGINAL PAD", "MARTY PAD", "6-BUTTON PAD", "MOUSE" };
    static const char* k_phases[4] = { "X HIGH", "X LOW", "Y HIGH", "Y LOW" };
    Input::Input_State* state = input->GetState();
    GT_Controller_Type type = input->GetControllerType(port);
    bool mouse = type == GT_CONTROLLER_MOUSE;
    u16 buttons = state->gamepads[port].buttons;
    u8 output = state->output;
    float character = ImGui::CalcTextSize("0").x;
    float space = ImGui::CalcTextSize(" ").x;
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    char table_id[32];
    u16 address = port == 0 ? 0x04D0 : 0x04D2;

    ImGui::NewLine(); ImGui::TextColored(cyan, "PORT %d (%04X)", port + 1, address); ImGui::Separator();
    snprintf(table_id, sizeof(table_id), "##game_port_%d", port);

    if (!ImGui::BeginTable(table_id, 2, flags))
        return;

    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 8);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 22);

    ImGui::TableNextRow();
    draw_grid_label("TYPE");
    ImGui::TextColored(type == GT_CONTROLLER_NONE ? gray : orange, "%s", k_types[type % 5]);

    ImGui::TableNextRow();
    draw_grid_label("READ");
    ImGui::TextColored(white, "$%02X", input->Peek(address));

    ImGui::TableNextRow();
    draw_grid_label("LINES");
    ImGui::TextColored((output & (k_input_com << port)) ? green : gray, "COM"); ImGui::SameLine(0.0f, space);
    ImGui::TextColored((output & (0x01 << (port * 2))) ? green : gray, "TRIG1"); ImGui::SameLine(0.0f, space);
    ImGui::TextColored((output & (0x02 << (port * 2))) ? green : gray, "TRIG2");

    u16 pad = GT_GAMEPAD_UP | GT_GAMEPAD_DOWN | GT_GAMEPAD_LEFT | GT_GAMEPAD_RIGHT | GT_GAMEPAD_SELECT |
        GT_GAMEPAD_RUN | GT_GAMEPAD_A | GT_GAMEPAD_B;
    u16 present = 0;

    if (type == GT_CONTROLLER_ORIGINAL_GAMEPAD || type == GT_CONTROLLER_MARTY_GAMEPAD)
        present = pad;
    else if (type == GT_CONTROLLER_6_BUTTON_GAMEPAD)
        present = pad | GT_GAMEPAD_C | GT_GAMEPAD_X | GT_GAMEPAD_Y | GT_GAMEPAD_Z;
    else if (mouse)
        present = GT_GAMEPAD_A | GT_GAMEPAD_B;

    ImGui::TableNextRow();
    draw_grid_label("BUTTONS");
    draw_button("UP", GT_GAMEPAD_UP, buttons, present); ImGui::SameLine(0.0f, space);
    draw_button("DOWN", GT_GAMEPAD_DOWN, buttons, present); ImGui::SameLine(0.0f, space);
    draw_button("LEFT", GT_GAMEPAD_LEFT, buttons, present); ImGui::SameLine(0.0f, space);
    draw_button("RIGHT", GT_GAMEPAD_RIGHT, buttons, present);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TableNextColumn();
    draw_button("SEL", GT_GAMEPAD_SELECT, buttons, present); ImGui::SameLine(0.0f, space);
    draw_button("RUN", GT_GAMEPAD_RUN, buttons, present); ImGui::SameLine(0.0f, space);
    draw_button("A", GT_GAMEPAD_A, buttons, present); ImGui::SameLine(0.0f, space);
    draw_button("B", GT_GAMEPAD_B, buttons, present); ImGui::SameLine(0.0f, space);
    draw_button("C", GT_GAMEPAD_C, buttons, present); ImGui::SameLine(0.0f, space);
    draw_button("X", GT_GAMEPAD_X, buttons, present); ImGui::SameLine(0.0f, space);
    draw_button("Y", GT_GAMEPAD_Y, buttons, present); ImGui::SameLine(0.0f, space);
    draw_button("Z", GT_GAMEPAD_Z, buttons, present);

    ImGui::TableNextRow();
    draw_grid_label("PHASE");

    if (mouse)
        ImGui::TextColored(white, "%s", k_phases[state->mouse_phase[port] & 3]);
    else
        ImGui::TextColored(gray, "--");

    ImGui::TableNextRow();
    draw_grid_label("SAMPLE");

    if (mouse)
    {
        ImGui::TextColored(violet, "X"); ImGui::SameLine(0.0f, space);
        ImGui::TextColored(white, "%+4d", (s8)state->mouse_sample_x[port]); ImGui::SameLine(0.0f, space);
        ImGui::TextColored(violet, "Y"); ImGui::SameLine(0.0f, space);
        ImGui::TextColored(white, "%+4d", (s8)state->mouse_sample_y[port]);
    }
    else
        ImGui::TextColored(gray, "--");

    ImGui::TableNextRow();
    draw_grid_label("PENDING");

    if (mouse)
    {
        ImGui::TextColored(violet, "X"); ImGui::SameLine(0.0f, space);
        ImGui::TextColored(white, "%+4d", state->mouse_x[port]); ImGui::SameLine(0.0f, space);
        ImGui::TextColored(violet, "Y"); ImGui::SameLine(0.0f, space);
        ImGui::TextColored(white, "%+4d", state->mouse_y[port]);
    }
    else
        ImGui::TextColored(gray, "--");

    ImGui::EndTable();
}

static void draw_button(const char* name, u16 mask, u16 buttons, u16 present)
{
    bool available = (present & mask) != 0;
    bool pressed = available && (buttons & mask) != 0;

    ImGui::TextColored(pressed ? green : (available ? white : gray), "%s", name);
}

static void draw_grid_label(const char* label)
{
    ImGui::TableNextColumn();
    ImGui::TextColored(violet, "%s", label);
    ImGui::TableNextColumn();
}
