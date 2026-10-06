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

#define GUI_FLOPPY_IMPORT
#include "gui_floppy.h"

#include <string>
#include "imgui.h"
#include "gui.h"
#include "gui_colors.h"
#include "gui_filedialogs.h"
#include "application.h"
#include "config.h"
#include "emu.h"
#include "emu_floppy.h"
#include "utils.h"

enum Floppy_Action
{
    Floppy_Action_None = 0,
    Floppy_Action_Insert,
    Floppy_Action_Select,
    Floppy_Action_Blank,
    Floppy_Action_Eject
};

static const char* blank_types = "2HD 1.23 MB (FM Towns)\0" "2DD 640 KB\0" "2DD 720 KB\0" "2HD 1.44 MB\0\0";

static Floppy_Action pending_action = Floppy_Action_None;
static int pending_drive = -1;
static int pending_index = 0;
static std::string pending_path;
static int dialog_drive = 0;
static int blank_type = 0;
static bool blank_formatted = true;
static bool open_dirty = false;
static bool open_discard = false;
static bool open_blank = false;
static bool open_quit = false;

static void request(int drive, Floppy_Action action, const char* path, int index);
static bool perform(bool discard_changes);
static void draw_dirty_popup(void);
static void draw_discard_popup(void);
static void draw_blank_popup(void);
static void draw_quit_popup(void);

void gui_floppy_menu(int drive, const char* label, int drives)
{
    if (!ImGui::BeginMenu(label))
        return;

    Emu_FloppyInfo info;
    emu_floppy_get_info(drive, &info);
    bool loading = gui_is_rom_loading() || emu_is_media_loading();

    if (ImGui::MenuItem("Insert...", NULL, false, !loading))
        gui_file_dialog_open_floppy(drive);

    if (ImGui::MenuItem("New Blank Disk...", NULL, false, !loading))
    {
        dialog_drive = drive;
        open_blank = true;
    }

    if (ImGui::MenuItem("Eject", NULL, false, info.inserted))
        request(drive, Floppy_Action_Eject, NULL, 0);

    ImGui::Separator();

    if (!info.inserted)
        ImGui::TextDisabled("Empty");
    else
    {
        const char* name = info.state_owned ? "Saved-state image" :
            emu_floppy_get_disk_name(drive, info.disk_index);
        ImGui::Text("%.40s%s", name, strlen(name) > 40 ? "..." : "");

        if (ImGui::IsItemHovered() && info.path[0])
            ImGui::SetTooltip("%s", info.path);

        if (info.dirty)
            ImGui::TextColored(orange, "Unsaved changes");
    }

    ImGui::Separator();

    if (ImGui::BeginMenu("Disk in Image", info.disk_count > 1))
    {
        for (int i = 0; i < info.disk_count; i++)
        {
            ImGui::PushID(i);

            if (ImGui::MenuItem(emu_floppy_get_disk_name(drive, i), NULL, i == info.disk_index) && i != info.disk_index)
                request(drive, Floppy_Action_Select, NULL, i);

            ImGui::PopID();
        }

        ImGui::EndMenu();
    }

    bool write_protected = info.inserted ? info.write_protected : config_emulator.floppy_write_protected[drive];

    if (ImGui::MenuItem("Write Protected", NULL, &write_protected, !info.state_owned))
    {
        if (!info.inserted)
            config_emulator.floppy_write_protected[drive] = write_protected;
        else if (!emu_floppy_set_write_protected(drive, write_protected))
            gui_set_error_message("Unable to change the write protection of this disk.");
    }

    if (ImGui::MenuItem("Save Changes", NULL, false, info.dirty && info.working_path[0]))
    {
        if (!emu_floppy_save(drive))
            gui_set_error_message("Unable to save the disk changes.\nUse Save As to write them somewhere else.");
    }

    if (ImGui::IsItemHovered() && info.working_path[0])
        ImGui::SetTooltip("%s", info.working_path);

    if (ImGui::MenuItem("Save As...", NULL, false, info.inserted))
        gui_file_dialog_save_floppy(drive);

    if (ImGui::MenuItem("Discard Changes...", NULL, false, info.dirty && !info.state_owned))
    {
        dialog_drive = drive;
        open_discard = true;
    }

    ImGui::Separator();

    if (ImGui::BeginMenu("Recent"))
    {
        for (int i = 0; i < config_max_recent_floppies; i++)
        {
            const std::string& recent = config_emulator.recent_floppies[drive][i];

            if (recent.empty())
                continue;

            ImGui::PushID(i);

            if (ImGui::MenuItem(get_filename(recent.c_str()), NULL, false, !loading))
                request(drive, Floppy_Action_Insert, recent.c_str(), 0);

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", recent.c_str());

            ImGui::PopID();
        }

        ImGui::EndMenu();
    }

    ImGui::Separator();

    if (drives > 1 && ImGui::MenuItem("Swap Floppy 1 and 2"))
        emu_floppy_swap();

    ImGui::MenuItem("Remember Changes", NULL, &config_emulator.floppy_persistence);

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Applies to disks inserted from now on.");
        ImGui::Text("Changes go to a working copy in the save files folder, the original image is never modified.");
        ImGui::Text("Disabled, disks are inserted write protected.");
        ImGui::EndTooltip();
    }

    ImGui::EndMenu();
}

void gui_floppy_popups(void)
{
    if (open_dirty)
    {
        open_dirty = false;
        gui_dialog_in_use = true;
        ImGui::OpenPopup("Unsaved Floppy Changes");
    }

    if (open_discard)
    {
        open_discard = false;
        gui_dialog_in_use = true;
        ImGui::OpenPopup("Discard Floppy Changes");
    }

    if (open_blank)
    {
        open_blank = false;
        gui_dialog_in_use = true;
        ImGui::OpenPopup("New Blank Disk");
    }

    if (open_quit)
    {
        open_quit = false;
        gui_dialog_in_use = true;
        ImGui::OpenPopup("Unsaved Floppy Changes on Quit");
    }

    draw_dirty_popup();
    draw_discard_popup();
    draw_blank_popup();
    draw_quit_popup();
}

void gui_floppy_insert(int drive, const char* path)
{
    request(drive, Floppy_Action_Insert, path, 0);
}

void gui_floppy_dialog_insert(int drive, const char* path)
{
    std::string full_path(path);
    size_t separator = full_path.find_last_of("/\\");
    config_emulator.last_open_path = separator == std::string::npos ? "" : full_path.substr(0, separator + 1);
    request(drive, Floppy_Action_Insert, path, 0);
}

void gui_floppy_dialog_save_as(int drive, const char* path)
{
    if (emu_floppy_save_as(drive, path))
        gui_set_status_message("Disk saved", 3000);
    else
        gui_set_error_message("Unable to save the disk.\nRaw images need a standard 1.23 MB, 1.44 MB, 720 KB or "
            "640 KB layout, D77 takes any disk.");
}

void gui_floppy_dialog_new_blank(int drive, const char* path)
{
    request(drive, Floppy_Action_Blank, path, 0);
}

void gui_floppy_open_quit_confirmation(void)
{
    open_quit = true;
}

static void request(int drive, Floppy_Action action, const char* path, int index)
{
    pending_action = action;
    pending_drive = drive;
    pending_index = index;
    pending_path = IsValidPointer(path) ? path : "";

    Emu_FloppyInfo info;
    emu_floppy_get_info(drive, &info);

    if (info.dirty)
    {
        open_dirty = true;
        return;
    }

    perform(false);
}

static bool perform(bool discard_changes)
{
    bool done = false;
    const char* error = "Unable to load the floppy image.";

    switch (pending_action)
    {
        case Floppy_Action_Insert:
            done = emu_floppy_insert(pending_drive, pending_path.c_str(), discard_changes);
            break;
        case Floppy_Action_Select:
            done = emu_floppy_select_disk(pending_drive, pending_index, discard_changes);
            break;
        case Floppy_Action_Blank:
            done = emu_floppy_create_blank(pending_drive, pending_path.c_str(), blank_type, blank_formatted,
                discard_changes);
            error = "Unable to create the blank disk.";
            break;
        case Floppy_Action_Eject:
            done = emu_floppy_eject(pending_drive, discard_changes);
            error = "Unable to save the disk changes before ejecting.";
            break;
        default:
            return true;
    }

    if (!done)
    {
        std::string message(error);

        if (!pending_path.empty())
            message += "\n" + pending_path;

        gui_set_error_message(message.c_str());
    }
    else if (pending_action != Floppy_Action_Eject)
        gui_set_status_message("Floppy inserted", 3000);

    pending_action = Floppy_Action_None;
    return done;
}

static void draw_dirty_popup(void)
{
    if (!ImGui::BeginPopupModal("Unsaved Floppy Changes", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::Text("The disk in floppy drive %d has unsaved changes.\n\n", pending_drive + 1);
    ImGui::Separator();

    if (ImGui::Button("Save and Continue", ImVec2(150, 0)))
    {
        ImGui::CloseCurrentPopup();
        gui_dialog_in_use = false;
        perform(false);
    }

    ImGui::SameLine();

    if (ImGui::Button("Discard", ImVec2(100, 0)))
    {
        ImGui::CloseCurrentPopup();
        gui_dialog_in_use = false;
        perform(true);
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(100, 0)))
    {
        ImGui::CloseCurrentPopup();
        gui_dialog_in_use = false;
        pending_action = Floppy_Action_None;
    }

    ImGui::EndPopup();
}

static void draw_discard_popup(void)
{
    if (!ImGui::BeginPopupModal("Discard Floppy Changes", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::Text("Discard the unsaved changes in floppy drive %d?\n\n", dialog_drive + 1);
    ImGui::Text("The disk goes back to its last saved contents.\n\n");
    ImGui::Separator();

    if (ImGui::Button("Discard", ImVec2(120, 0)))
    {
        ImGui::CloseCurrentPopup();
        gui_dialog_in_use = false;

        if (!emu_floppy_discard(dialog_drive))
            gui_set_error_message("Unable to reload the floppy image.");
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120, 0)))
    {
        ImGui::CloseCurrentPopup();
        gui_dialog_in_use = false;
    }

    ImGui::SetItemDefaultFocus();
    ImGui::EndPopup();
}

static void draw_blank_popup(void)
{
    if (!ImGui::BeginPopupModal("New Blank Disk", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::PushItemWidth(220.0f);
    ImGui::Combo("Type", &blank_type, blank_types);
    ImGui::PopItemWidth();
    ImGui::Checkbox("Formatted (empty FAT12 data disk)", &blank_formatted);

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Unformatted disks need formatting by Towns OS or the game before use.");

    ImGui::Separator();

    if (ImGui::Button("Create...", ImVec2(120, 0)))
    {
        ImGui::CloseCurrentPopup();
        gui_dialog_in_use = false;
        gui_file_dialog_new_floppy(dialog_drive);
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120, 0)))
    {
        ImGui::CloseCurrentPopup();
        gui_dialog_in_use = false;
    }

    ImGui::EndPopup();
}

static void draw_quit_popup(void)
{
    if (!ImGui::BeginPopupModal("Unsaved Floppy Changes on Quit", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::Text("Some floppy changes could not be saved.\n");
    ImGui::Text("Use Save As to write them to another file, or quit without them.\n\n");
    ImGui::Separator();

    for (int i = 0; i < config_floppy_drives; i++)
    {
        Emu_FloppyInfo info;
        emu_floppy_get_info(i, &info);

        if (!info.dirty)
            continue;

        ImGui::PushID(i);
        ImGui::Text("Floppy %d: %s", i + 1, info.path[0] ? get_filename(info.path) : "Saved-state image");
        ImGui::SameLine();

        if (ImGui::Button("Save As..."))
            gui_file_dialog_save_floppy(i);

        ImGui::PopID();
    }

    ImGui::Separator();

    if (ImGui::Button("Retry", ImVec2(100, 0)))
    {
        if (emu_floppy_flush())
        {
            ImGui::CloseCurrentPopup();
            gui_dialog_in_use = false;
            application_confirm_quit();
        }
        else
            gui_set_error_message("Unable to save the floppy changes.");
    }

    ImGui::SameLine();

    if (ImGui::Button("Quit Without Saving", ImVec2(160, 0)))
    {
        ImGui::CloseCurrentPopup();
        gui_dialog_in_use = false;
        application_confirm_quit();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(100, 0)))
    {
        ImGui::CloseCurrentPopup();
        gui_dialog_in_use = false;
    }

    ImGui::EndPopup();
}
