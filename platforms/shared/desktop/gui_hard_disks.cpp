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

#define GUI_HARD_DISKS_IMPORT
#include "gui_hard_disks.h"

#include <string>
#include "imgui.h"
#include "gui.h"
#include "gui_actions.h"
#include "gui_colors.h"
#include "gui_filedialogs.h"
#include "gui_notifications.h"
#include "application.h"
#include "config.h"
#include "emu.h"
#include "emu_floppy.h"
#include "emu_hard_disk.h"
#include "utils.h"

static const int k_create_sizes_mb[5] = { 40, 100, 127, 250, 500 };
static const char* create_sizes = "40 MB\0" "100 MB\0" "127 MB\0" "250 MB\0" "500 MB\0" "Custom\0\0";

static bool open_create = false;
static bool open_quit = false;
static int dialog_id = 0;
static int create_choice = 1;
static int create_custom_mb = 100;
static int create_size_mb = 100;
static bool create_formatted = true;

static void disk_menu(int id);
static void attach(int id, const char* path);
static void draw_create_popup(void);
static void draw_quit_popup(void);
static void notify_attached(int id, const char* path, const char* message);
static GT_Machine_Model current_model(void);

void gui_hard_disks_menu(void)
{
    if (!ImGui::BeginMenu("Hard Disks"))
        return;

    for (int i = 0; i < GT_MAX_HARD_DISKS; i++)
        disk_menu(i);

    ImGui::Separator();

    if (ImGui::MenuItem("Assign Drive Letters", "", &config_emulator.hard_disk_drive_letters))
    {
        emu_set_drive_letter_assignment(config_emulator.hard_disk_drive_letters);
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Towns OS keeps hard disk drive letters in the CMOS, not on the disk.");
        ImGui::Text("When enabled, each reset gives free letters to partitions it doesn't list yet.");
        ImGui::Text("When disabled, register them yourself in Towns OS.");
        ImGui::EndTooltip();
    }

    ImGui::Separator();

    GT_Machine_Model model = current_model();

    if ((k_machine_profiles[model].features & GT_MACHINE_FEATURE_SCSI) != 0)
        ImGui::TextDisabled("Built-in SCSI interface");
    else if (model == GT_MACHINE_MODEL1_2)
        ImGui::TextDisabled("SCSI card");
    else
        ImGui::TextDisabled("No SCSI interface");

    if (!emu_is_empty() && gui_hard_disks_is_pending() && ImGui::MenuItem("Reset to Apply Changes"))
        gui_action_reset();

    ImGui::EndMenu();
}

void gui_hard_disks_popups(void)
{
    if (open_create)
    {
        open_create = false;
        gui_dialog_in_use = true;
        ImGui::OpenPopup("New Blank Hard Disk");
    }

    if (open_quit)
    {
        open_quit = false;
        gui_dialog_in_use = true;
        ImGui::OpenPopup("Unsaved Hard Disk Changes on Quit");
    }

    draw_create_popup();
    draw_quit_popup();
}

void gui_hard_disks_update(void)
{
    if (emu_hard_disk_take_flush_error())
        gui_notify(gui_NotificationError, NULL, "Unable to save the hard disk changes", "They are kept and retried");

    u32 changes = emu_hard_disk_take_state_changes();

    if (changes == 0)
        return;

    std::string ids;

    for (int i = 0; i < GT_MAX_HARD_DISKS; i++)
    {
        if ((changes & (1 << i)) == 0)
            continue;

        ids += ids.empty() ? "SCSI ID " : ", ";
        ids += (char)('0' + i);
    }

    gui_notify(gui_NotificationWarning, ICON_MD_STORAGE, "The hard disk changed since this state", ids.c_str());
}

bool gui_hard_disks_load(const char* path)
{
    int id = emu_hard_disk_get_image_id(path);

    if (id < 0)
    {
        gui_set_error_message("Every SCSI ID already has a hard disk.");
        return false;
    }

    if (!emu_hard_disk_attach(id, path, config_emulator.hard_disk_write_protected[id] != 0))
    {
        std::string message("Unable to attach the hard disk image:\n");
        message += path;
        gui_set_error_message(message.c_str());
        return false;
    }

    notify_attached(id, path, "Hard disk attached");

    if (emu_is_empty() && emu_get_core()->GetFirmware()->IsReady())
        gui_action_power_on();

    return true;
}

bool gui_hard_disks_is_pending(void)
{
    for (int i = 0; i < GT_MAX_HARD_DISKS; i++)
    {
        Emu_HardDiskInfo info;
        emu_hard_disk_get_info(i, &info);

        if (info.pending)
            return true;
    }

    return false;
}

void gui_hard_disks_dialog_attach(int id, const char* path)
{
    std::string full_path(path);
    size_t separator = full_path.find_last_of("/\\");
    config_emulator.last_open_path = separator == std::string::npos ? "" : full_path.substr(0, separator + 1);
    attach(id, path);
}

void gui_hard_disks_dialog_create(int id, const char* path)
{
    if (emu_hard_disk_create(id, path, create_size_mb, create_formatted,
        config_emulator.hard_disk_write_protected[id] != 0))
        notify_attached(id, path, "Hard disk created");
    else
    {
        std::string message("Unable to create the hard disk image.\nAn existing file is never replaced:\n");
        message += path;
        gui_set_error_message(message.c_str());
    }
}

void gui_hard_disks_open_quit_confirmation(void)
{
    open_quit = true;
}

static void disk_menu(int id)
{
    Emu_HardDiskInfo info;
    emu_hard_disk_get_info(id, &info);
    const char* path = info.pending ? info.pending_path : info.path;
    bool present = path[0] != 0;
    const char* name = present ? get_filename(path) : "Empty";
    char label[64];
    snprintf(label, sizeof(label), "ID %d: %.32s%s###hard_disk_%d", id, name, strlen(name) > 32 ? "..." : "", id);

    if (!ImGui::BeginMenu(label))
        return;

    bool loading = gui_is_rom_loading() || emu_is_media_loading();

    if (ImGui::MenuItem("Attach...", NULL, false, !loading))
        gui_file_dialog_attach_hard_disk(id);

    if (ImGui::MenuItem("New Blank Disk...", NULL, false, !loading))
    {
        dialog_id = id;
        open_create = true;
    }

    if (ImGui::MenuItem("Detach", NULL, false, present))
    {
        emu_hard_disk_detach(id);
        Emu_HardDiskInfo detached;
        emu_hard_disk_get_info(id, &detached);
        gui_notify(gui_NotificationInfo, ICON_MD_EJECT, "Hard disk detached",
            detached.pending ? "At the next reset" : name);
    }

    ImGui::Separator();

    if (!present)
        ImGui::TextDisabled("Empty");
    else
    {
        if (info.attached && !info.pending)
            ImGui::Text("%.40s%s, %u MB", name, strlen(name) > 40 ? "..." : "", (info.blocks + 1024) / 2048);
        else
            ImGui::Text("%.40s%s", name, strlen(name) > 40 ? "..." : "");

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", path);
    }

    if (info.pending && info.pending_attached)
        ImGui::TextColored(orange, "Attaches at the next reset");
    else if (info.pending)
        ImGui::TextColored(orange, "%.32s detaches at the next reset", get_filename(info.path));

    ImGui::Separator();

    bool write_protected = present ? info.pending_write_protected : config_emulator.hard_disk_write_protected[id] != 0;

    if (ImGui::MenuItem("Write Protected", NULL, &write_protected) &&
        !emu_hard_disk_set_write_protected(id, write_protected))
        gui_notify(gui_NotificationError, NULL, "Unable to change the write protection of this disk");

    ImGui::Separator();

    if (ImGui::BeginMenu("Recent", !loading))
    {
        for (int i = 0; i < config_max_recent_hard_disks; i++)
        {
            const std::string& recent = config_emulator.recent_hard_disks[i];

            if (recent.empty())
                continue;

            ImGui::PushID(i);

            if (ImGui::MenuItem(get_filename(recent.c_str())))
            {
                std::string path = recent;
                attach(id, path.c_str());
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", recent.c_str());

            ImGui::PopID();
        }

        ImGui::EndMenu();
    }

    ImGui::EndMenu();
}

static void attach(int id, const char* path)
{
    if (emu_hard_disk_attach(id, path, config_emulator.hard_disk_write_protected[id] != 0))
        notify_attached(id, path, "Hard disk attached");
    else
    {
        std::string message("Unable to attach the hard disk image:\n");
        message += path;
        gui_set_error_message(message.c_str());
    }
}

static void draw_create_popup(void)
{
    gui_center_next_window();

    if (!ImGui::BeginPopupModal("New Blank Hard Disk", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::Text("Create a blank hard disk on SCSI ID %d.\n\n", dialog_id);

    ImGui::Text("Size:");
    ImGui::PushItemWidth(248.0f);
    ImGui::Combo("##create_size", &create_choice, create_sizes);

    if (create_choice == 5)
    {
        ImGui::InputInt("MB##create_custom", &create_custom_mb);
        create_custom_mb = CLAMP(create_custom_mb, 1, 2047);
    }

    ImGui::PopItemWidth();

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Towns OS makes drives of up to 127 MB, up to ten per disk.");
        ImGui::EndTooltip();
    }

    ImGui::Checkbox("Formatted", &create_formatted);

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Formatted disks have empty drives, ready for game installs.");
        ImGui::Text("Unformatted disks need HD Install from the Towns OS CD before use.");
        ImGui::EndTooltip();
    }

    ImGui::NewLine();
    ImGui::Separator();

    if (ImGui::Button("Create...", ImVec2(120, 0)))
    {
        create_size_mb = create_choice < 5 ? k_create_sizes_mb[create_choice] : create_custom_mb;
        ImGui::CloseCurrentPopup();
        gui_dialog_in_use = false;
        gui_file_dialog_create_hard_disk(dialog_id);
    }

    ImGui::SetItemDefaultFocus();
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
    gui_center_next_window();

    if (!ImGui::BeginPopupModal("Unsaved Hard Disk Changes on Quit", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::Text("Some hard disk changes could not be saved.\n");
    ImGui::Text("Free some space or fix the file permissions and retry, or quit without them.\n\n");
    ImGui::Separator();

    for (int i = 0; i < GT_MAX_HARD_DISKS; i++)
    {
        Emu_HardDiskInfo info;
        emu_hard_disk_get_info(i, &info);

        if (info.unflushed != 0)
            ImGui::Text("ID %d: %s", i, get_filename(info.path));
    }

    ImGui::Separator();

    if (ImGui::Button("Retry", ImVec2(100, 0)))
    {
        if (emu_hard_disk_flush() && emu_floppy_flush())
        {
            ImGui::CloseCurrentPopup();
            gui_dialog_in_use = false;
            application_confirm_quit();
        }
        else
            gui_set_error_message("Unable to save the hard disk changes.");
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

static void notify_attached(int id, const char* path, const char* message)
{
    Emu_HardDiskInfo info;
    emu_hard_disk_get_info(id, &info);
    char text[64];
    snprintf(text, sizeof(text), "%s on ID %d", message, id);
    gui_notify(gui_NotificationSuccess, ICON_MD_STORAGE, text, info.pending ? "At the next reset" : get_filename(path));
}

static GT_Machine_Model current_model(void)
{
    GeartownsCore* core = emu_get_core();
    return emu_is_empty() ? core->GetPendingMachineConfig().model : core->GetMachineConfig().model;
}
