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

#define GUI_MEMORY_CARD_IMPORT
#include "gui_memory_card.h"

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
#include "emu_memory_card.h"
#include "utils.h"

static const u32 k_create_sizes_kb[11] = { 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 };
static const char* create_sizes = "64 KB\0" "128 KB\0" "256 KB\0" "512 KB\0" "1 MB\0" "2 MB\0" "4 MB\0" "8 MB\0"
    "16 MB\0" "32 MB\0" "64 MB\0\0";

static bool open_create = false;
static bool open_unsaved = false;
static bool unsaved_quit = false;
static int create_choice = 4;
static u32 create_size = 1024 * 1024;

static void eject(void);
static void draw_create_popup(void);
static void draw_unsaved_popup(void);
static void format_size(u32 size, char* text, size_t text_size);
static GT_Machine_Model current_model(void);

void gui_memory_card_menu(void)
{
    if (!ImGui::BeginMenu("IC Memory Card"))
        return;

    Emu_MemoryCardInfo info;
    emu_memory_card_get_info(&info);
    bool loading = gui_is_rom_loading() || emu_is_media_loading();

    if (ImGui::MenuItem("Insert...", NULL, false, !loading))
        gui_file_dialog_insert_memory_card();

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("A card that starts with an IPL4 boot sector boots ahead of the CD at reset");

    if (ImGui::MenuItem("New Blank Card...", NULL, false, !loading))
        open_create = true;

    if (ImGui::MenuItem("Eject", NULL, false, info.inserted))
        eject();

    ImGui::Separator();

    if (!info.inserted)
        ImGui::TextDisabled("Empty");
    else
    {
        const char* name = get_filename(info.path);
        char size[16];
        format_size(info.size, size, sizeof(size));
        ImGui::Text("%.40s%s, %s", name, strlen(name) > 40 ? "..." : "", size);

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", info.path);

        if (info.dirty)
            ImGui::TextColored(orange, "Unsaved changes");
    }

    ImGui::Separator();

    bool write_protected = info.inserted ? info.write_protected : config_emulator.memory_card_write_protected;

    if (ImGui::MenuItem("Write Protected", NULL, &write_protected) &&
        !emu_memory_card_set_write_protected(write_protected))
        gui_notify(gui_NotificationError, NULL, "Unable to change the write protection of this card");

    if (ImGui::MenuItem("Save Changes", NULL, false, info.dirty))
    {
        if (emu_memory_card_flush())
            gui_notify(gui_NotificationSuccess, ICON_MD_SAVE, "IC memory card saved", info.path);
        else
            gui_set_error_message("Unable to save the IC memory card changes.");
    }

    ImGui::Separator();

    if (ImGui::BeginMenu("Recent", !loading))
    {
        for (int i = 0; i < config_max_recent_memory_cards; i++)
        {
            const std::string& recent = config_emulator.recent_memory_cards[i];

            if (recent.empty())
                continue;

            ImGui::PushID(i);

            if (ImGui::MenuItem(get_filename(recent.c_str())))
            {
                std::string path = recent;
                gui_memory_card_insert(path.c_str());
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", recent.c_str());

            ImGui::PopID();
        }

        ImGui::EndMenu();
    }

    ImGui::Separator();

    u32 features = k_machine_profiles[current_model()].features;

    if ((features & GT_MACHINE_FEATURE_JEIDA4) != 0)
        ImGui::TextDisabled("JEIDA 4 slot, cards up to 64 MB");
    else if (k_machine_profiles[current_model()].map == GT_MACHINE_MAP_32BIT)
        ImGui::TextDisabled("Cards up to 16 MB");
    else
        ImGui::TextDisabled("Cards up to 1 MB");

    ImGui::EndMenu();
}

void gui_memory_card_popups(void)
{
    if (open_create)
    {
        open_create = false;
        gui_dialog_in_use = true;
        ImGui::OpenPopup("New Blank IC Memory Card");
    }

    if (open_unsaved)
    {
        open_unsaved = false;
        gui_dialog_in_use = true;
        ImGui::OpenPopup("Unsaved IC Memory Card Changes");
    }

    draw_create_popup();
    draw_unsaved_popup();
}

void gui_memory_card_update(void)
{
    if (emu_memory_card_take_flush_error())
        gui_notify(gui_NotificationError, NULL, "Unable to save the IC memory card changes",
            "They are kept and retried");

    if (emu_memory_card_take_state_change())
        gui_notify(gui_NotificationWarning, ICON_MD_SD_CARD, "The IC memory card changed since this state");
}

bool gui_memory_card_load(const char* path)
{
    if (!gui_memory_card_insert(path))
        return false;

    if (emu_is_empty() && emu_get_core()->GetFirmware()->IsReady())
        gui_action_power_on();

    return true;
}

bool gui_memory_card_insert(const char* path)
{
    if (!emu_memory_card_insert(path, config_emulator.memory_card_write_protected))
    {
        std::string message("Unable to insert the IC memory card image:\n");
        message += path;
        gui_set_error_message(message.c_str());
        return false;
    }

    gui_notify(gui_NotificationSuccess, ICON_MD_SD_CARD, "IC memory card inserted", get_filename(path));
    return true;
}

void gui_memory_card_dialog_insert(const char* path)
{
    gui_memory_card_insert(path);
}

void gui_memory_card_dialog_create(const char* path)
{
    if (emu_memory_card_create(path, create_size, config_emulator.memory_card_write_protected))
        gui_notify(gui_NotificationSuccess, ICON_MD_SD_CARD, "IC memory card created", get_filename(path));
    else
    {
        std::string message("Unable to create the IC memory card image.\nAn existing file is never replaced:\n");
        message += path;
        gui_set_error_message(message.c_str());
    }
}

void gui_memory_card_open_quit_confirmation(void)
{
    unsaved_quit = true;
    open_unsaved = true;
}

static void eject(void)
{
    Emu_MemoryCardInfo info;
    emu_memory_card_get_info(&info);

    if (!emu_memory_card_eject(false))
    {
        unsaved_quit = false;
        open_unsaved = true;
        return;
    }

    gui_notify(gui_NotificationInfo, ICON_MD_EJECT, "IC memory card ejected", get_filename(info.path));
}

static void draw_create_popup(void)
{
    gui_center_next_window();

    if (!ImGui::BeginPopupModal("New Blank IC Memory Card", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::Text("Create a blank card in the card slot. \n\n");

    ImGui::Text("Size:");
    ImGui::PushItemWidth(248.0f);
    ImGui::Combo("##create_size", &create_choice, create_sizes);
    ImGui::PopItemWidth();

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("The UG reaches the first 1 MB of a card, the Model 1/2, 1F/2F and 10F/20F the first 16 MB.");
        ImGui::Text("Towns OS formats the card with TICMFMT once CONFIG.SYS loads TICM.SYS.");
        ImGui::EndTooltip();
    }

    ImGui::NewLine();
    ImGui::Separator();

    if (ImGui::Button("Create...", ImVec2(120, 0)))
    {
        create_size = k_create_sizes_kb[create_choice] * 1024;
        ImGui::CloseCurrentPopup();
        gui_dialog_in_use = false;
        gui_file_dialog_create_memory_card();
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

static void draw_unsaved_popup(void)
{
    gui_center_next_window();

    if (!ImGui::BeginPopupModal("Unsaved IC Memory Card Changes", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    Emu_MemoryCardInfo info;
    emu_memory_card_get_info(&info);

    ImGui::Text("The IC memory card changes could not be saved.\n");
    ImGui::Text("Free some space or fix the file permissions and retry, or %s without them.\n\n",
        unsaved_quit ? "quit" : "eject the card");
    ImGui::Separator();
    ImGui::Text("%s", get_filename(info.path));
    ImGui::Separator();

    if (ImGui::Button("Retry", ImVec2(100, 0)))
    {
        bool saved = unsaved_quit ? emu_memory_card_flush() && emu_hard_disk_flush() && emu_floppy_flush() :
            emu_memory_card_eject(false);

        if (saved)
        {
            ImGui::CloseCurrentPopup();
            gui_dialog_in_use = false;

            if (unsaved_quit)
                application_confirm_quit();
            else
                gui_notify(gui_NotificationInfo, ICON_MD_EJECT, "IC memory card ejected", get_filename(info.path));
        }
        else
            gui_set_error_message("Unable to save the IC memory card changes.");
    }

    ImGui::SameLine();

    if (ImGui::Button(unsaved_quit ? "Quit Without Saving" : "Eject Without Saving", ImVec2(160, 0)))
    {
        ImGui::CloseCurrentPopup();
        gui_dialog_in_use = false;

        if (unsaved_quit)
            application_confirm_quit();
        else
        {
            emu_memory_card_eject(true);
            gui_notify(gui_NotificationInfo, ICON_MD_EJECT, "IC memory card ejected", get_filename(info.path));
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(100, 0)))
    {
        ImGui::CloseCurrentPopup();
        gui_dialog_in_use = false;
    }

    ImGui::EndPopup();
}

static void format_size(u32 size, char* text, size_t text_size)
{
    if ((size % (1024 * 1024)) == 0)
        snprintf(text, text_size, "%u MB", size / (1024 * 1024));
    else
        snprintf(text, text_size, "%u KB", size / 1024);
}

static GT_Machine_Model current_model(void)
{
    GeartownsCore* core = emu_get_core();
    return emu_is_empty() ? core->GetPendingMachineConfig().model : core->GetMachineConfig().model;
}
