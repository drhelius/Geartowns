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
#include "input/keyboard.h"
#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../gui_colors.h"
#include "gui_debug_constants.h"

void gui_debug_window_keyboard(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(210, 90), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 320), ImGuiCond_FirstUseEver);
    ImGui::Begin("Keyboard", &config_debug.show_keyboard);

    ImGui::PushFont(gui_default_font);

    Keyboard* keyboard = emu_get_core()->GetKeyboard();
    Keyboard::Keyboard_State* state = keyboard->GetState();

    ImGui::TextColored(cyan, "INTERFACE (0600-0604)"); ImGui::Separator();

    ImGui::TextColored(violet, "DATA        "); ImGui::SameLine();
    ImGui::TextColored(state->fifo_count ? white : gray, "$%02X", keyboard->Peek(0x0600)); ImGui::SameLine();
    ImGui::TextColored(violet, " STATUS"); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", keyboard->Peek(0x0602));

    ImGui::TextColored(violet, "IRQ ENABLE  "); ImGui::SameLine();
    ImGui::TextColored(state->irq_enabled ? green : gray, "%s", state->irq_enabled ? "ON " : "OFF"); ImGui::SameLine();
    ImGui::TextColored(violet, " KBINT "); ImGui::SameLine();
    ImGui::TextColored(state->kbint ? yellow : gray, "%s", state->kbint ? "ON " : "OFF");

    ImGui::TextColored(violet, "LAST COMMAND"); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", state->last_command);

    ImGui::NewLine(); ImGui::TextColored(cyan, "QUEUE"); ImGui::Separator();

    for (int i = 0; i < KEYBOARD_FIFO_SIZE; i++)
    {
        if ((i & 7) != 0)
            ImGui::SameLine();

        if (i < state->fifo_count)
        {
            u8 value = state->fifo[(state->fifo_read + i) & (KEYBOARD_FIFO_SIZE - 1)];
            bool flags = (value & 0x80) != 0;

            ImGui::TextColored(flags ? orange : white, "%02X", value);

            if (ImGui::IsItemHovered())
            {
                if (flags)
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

    int pressed = 0;

    for (int key = GT_KEY_NONE + 1; key < GT_KEY_COUNT && pressed < 8; key++)
    {
        if (!state->keys[key])
            continue;

        const char* name = gui_debug_key_name(key);

        if ((pressed & 1) != 0)
            ImGui::SameLine(150);

        ImGui::TextColored(white, "%02X", key); ImGui::SameLine();
        ImGui::TextColored(green, "%-12s", IsValidPointer(name) ? name : "UNKNOWN");
        pressed++;
    }

    for (; pressed < 8; pressed++)
    {
        if ((pressed & 1) != 0)
            ImGui::SameLine(150);

        ImGui::TextColored(gray, "--");
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}
