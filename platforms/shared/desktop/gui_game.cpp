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

#define GUI_GAME_IMPORT
#include "gui_game.h"

#include "imgui.h"
#include "gui_colors.h"
#include "gui_notifications.h"
#include "emu_game.h"

static const GuiColor& game_change_color = orange;

bool gui_push_game_source_color(int setting)
{
    if (emu_game_get_source(setting) != Emu_Game_Source_Change)
        return false;

    ImGui::PushStyleColor(ImGuiCol_Text, game_change_color);
    return true;
}

void gui_draw_game_source_tooltip(int setting)
{
    int source = emu_game_get_source(setting);

    if (source == Emu_Game_Source_Change)
        ImGui::TextColored(game_change_color, "Changed for this game only.");
    else if (source == Emu_Game_Source_Session)
        ImGui::TextDisabled("Set for this session only.");
}

bool gui_begin_game_menu(const char* label, int setting)
{
    bool pushed = gui_push_game_source_color(setting);
    bool open = ImGui::BeginMenu(label);

    if (pushed)
        ImGui::PopStyleColor();

    return open;
}

bool gui_begin_game_combo(const char* id, const char* preview, int setting)
{
    bool pushed = gui_push_game_source_color(setting);
    bool open = ImGui::BeginCombo(id, preview);

    if (pushed)
        ImGui::PopStyleColor();

    return open;
}

void gui_game_keep_disk_item(int disk)
{
    bool kept = emu_game_is_disk_kept(disk);

    if (ImGui::MenuItem("Keep with This Game", NULL, kept, emu_game_can_keep_disk(disk)))
        emu_game_keep_disk(disk, !kept);

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        const char* path = emu_game_get_kept_disk(disk);
        ImGui::BeginTooltip();

        if (emu_game_is_open())
            ImGui::Text("Goes in this drive each time %s is opened.", emu_game_get_name());
        else
            ImGui::Text("Goes in this drive each time the game is opened. No game is open.");

        if (path[0] != 0)
            ImGui::TextColored(game_change_color, "Kept: %s", path);

        ImGui::EndTooltip();
    }
}

void gui_game_forget_item(void)
{
    if (ImGui::MenuItem("Forget This Game's Changes", NULL, false, emu_game_has_overrides()))
    {
        if (emu_game_reset_settings())
            gui_notify(gui_NotificationInfo, ICON_MD_VIDEOGAME_ASSET, "This game's changes were forgotten", NULL, "game_settings");
        else
            gui_notify(gui_NotificationError, NULL, "Unable to save the game's settings", NULL, "game_settings");
    }

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Forgets the machine, FM Towns Ports, CD-ROM read speed, Fast Disk Timing\n"
            "and disks kept for this game. The key map is kept.");
}
