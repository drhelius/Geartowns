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

#define GUI_FIRMWARE_IMPORT
#include "gui_firmware.h"

#include <string>
#include "imgui.h"
#include "geartowns.h"
#include "config.h"
#include "emu.h"
#include "gui.h"
#include "gui_colors.h"
#include "gui_filedialogs.h"
#include "gui_menus.h"
#include "utils.h"

static const char* const k_component_labels[GT_FIRMWARE_COUNT] = { "SYSTEM", "OS", "FONT", "DICTIONARY", "FONT 20", "SERIAL", "EXTENSION" };

static GT_Machine_Model selected_model = GT_MACHINE_MODEL1_2;
static bool focus_window = false;
static char model_folder[4096] = "";
static std::string model_folder_shown;
static int model_folder_model = GT_MACHINE_AUTO;
static GT_Firmware_Source inspected_source;
static GT_Firmware_Info inspected_info[GT_FIRMWARE_COUNT];
static bool inspected = false;
static bool inspected_ready = false;

static void draw_controls(void);
static void draw_source(void);
static void draw_components(void);
static void draw_component(GT_Firmware_Type type, const GT_Firmware_Info& info);
static void draw_expected_files(void);
static bool inspect(const GT_Firmware_Source& source);
static bool is_loaded(GT_Machine_Model model);
static bool is_same_source(const GT_Firmware_Source& a, const GT_Firmware_Source& b);
static float get_button_width(const char* label);
static void setup_fixed_columns(const int* widths, int count);
static void draw_grid_label(const char* label);

void gui_firmware_open(void)
{
    config_emulator.show_firmware = true;
    focus_window = true;
    selected_model = emu_get_core()->GetPendingMachineConfig().model;
}

void gui_firmware_window(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    gui_center_next_window();
    ImGui::SetNextWindowSize(ImVec2(656, 430), ImGuiCond_FirstUseEver);

    if (focus_window)
    {
        ImGui::SetNextWindowFocus();
        focus_window = false;
    }

    ImGui::Begin("Firmware", &config_emulator.show_firmware);

    draw_controls();

    ImGui::PushFont(gui_default_font);
    draw_source();
    draw_components();
    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

static void draw_controls(void)
{
    const std::string& folder = config_machine.firmware_paths[selected_model];
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float label_width = ImGui::CalcTextSize("Default Folder").x + spacing * 2.0f;
    float browse_width = get_button_width("Browse...");
    float name_width = 0.0f;
    char status[GT_MAX_PATH];

    if ((selected_model != model_folder_model) || (folder != model_folder_shown))
    {
        strncpy_fit(model_folder, folder.c_str(), sizeof(model_folder));
        model_folder_shown = folder;
        model_folder_model = selected_model;
    }

    for (int i = 0; i < GT_MACHINE_RELEASE_ORDER_COUNT; i++)
        name_width = MAX(name_width, ImGui::CalcTextSize(k_machine_profiles[k_machine_release_order[i]].name).x);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Model");
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - get_button_width("Refresh") - spacing);

    if (ImGui::BeginCombo("##firmware_model", k_machine_profiles[selected_model].name))
    {
        for (int i = 0; i < GT_MACHINE_RELEASE_ORDER_COUNT; i++)
        {
            GT_Machine_Model model = k_machine_release_order[i];
            gui_get_firmware_status(model, status, sizeof(status));

            if (ImGui::Selectable(k_machine_profiles[model].name, selected_model == model))
                selected_model = model;

            ImGui::SameLine(name_width + spacing * 4.0f);
            ImGui::TextColored(gui_get_firmware_status_color(model), "%s", status);
        }

        ImGui::EndCombo();
    }

    ImGui::SameLine();

    if (ImGui::Button("Refresh"))
    {
        emu_apply_firmware_folders();
        inspected = false;
    }

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Search the folders again for firmware added or removed since the last search.");

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Default Folder");
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browse_width - spacing);

    if (ImGui::InputText("##firmware_default", gui_bios_path, IM_ARRAYSIZE(gui_bios_path),
        ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        gui_load_bios(gui_bios_path);
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Searched for every model, with its subfolders and the MAME zips there.");
        ImGui::Text("Press Enter to apply.");
        ImGui::EndTooltip();
    }

    ImGui::SameLine();

    if (ImGui::Button("Browse...##firmware_default"))
        gui_file_dialog_load_bios();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Own Folder");
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browse_width - get_button_width("Use Default") -
        spacing * 2.0f);

    if (ImGui::InputTextWithHint("##firmware_own", "Uses the default folder", model_folder, IM_ARRAYSIZE(model_folder),
        ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        gui_load_model_firmware(selected_model, model_folder);
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Only for the %s, used as it is, even with unknown ROMs.", k_machine_profiles[selected_model].name);
        ImGui::Text("Press Enter to apply.");
        ImGui::EndTooltip();
    }

    ImGui::SameLine();

    if (ImGui::Button("Browse...##firmware_own"))
        gui_file_dialog_load_model_firmware(selected_model);

    ImGui::SameLine();
    ImGui::BeginDisabled(folder.empty());

    if (ImGui::Button("Use Default"))
        gui_load_model_firmware(selected_model, "");

    ImGui::EndDisabled();
}

static void draw_source(void)
{
    GeartownsCore* core = emu_get_core();
    const GT_Firmware_Source& source = core->GetFirmwareSource(selected_model);
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit;
    static const int widths[2] = { 11, 0 };
    float space = ImGui::CalcTextSize(" ").x;
    char status[GT_MAX_PATH];
    gui_get_firmware_status(selected_model, status, sizeof(status));

    ImGui::NewLine(); ImGui::TextColored(cyan, "FIRMWARE"); ImGui::Separator();

    if (!ImGui::BeginTable("##firmware_source", 2, flags))
        return;

    setup_fixed_columns(widths, 2);

    ImGui::TableNextRow();
    draw_grid_label("STATUS");
    ImGui::TextColored(gui_get_firmware_status_color(selected_model), "%s", status);

    if (source.type != GT_FIRMWARE_SOURCE_NONE)
    {
        static const char* const sources[] =
        {
            "", "Own folder", "Default folder, set made for this model", "Default folder, compatible set",
            "Default folder, unknown ROMs", "Default folder, set made for another model"
        };

        ImGui::TableNextRow();
        draw_grid_label("SOURCE");
        ImGui::TextColored(white, "%s", sources[source.type]);

        ImGui::TableNextRow();
        draw_grid_label(source.zip ? "ZIP" : "FOLDER");
        ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)white);
        ImGui::TextWrapped("%s", source.path);
        ImGui::PopStyleColor();

        ImGui::TableNextRow();
        draw_grid_label("SET");

        if (source.set >= 0)
        {
            ImGui::TextColored(white, "%s", k_firmware_sets[source.set].description); ImGui::SameLine(0.0f, space);
            ImGui::TextColored(gray, "(%s)", k_firmware_sets[source.set].name);
        }
        else
            ImGui::TextColored(gray, "Unknown");

        ImGui::TableNextRow();
        draw_grid_label("SYSTEM CRC");
        ImGui::TextColored(source.crc != 0 ? orange : gray, "%08X", source.crc);

        ImGui::TableNextRow();
        draw_grid_label("LOADED");
        bool loaded = is_loaded(selected_model);
        ImGui::TextColored(loaded ? green : gray, "%s", loaded ? "YES" : "NO");
    }

    ImGui::EndTable();
}

static void draw_components(void)
{
    GeartownsCore* core = emu_get_core();
    const GT_Firmware_Source& source = core->GetFirmwareSource(selected_model);
    bool loaded = is_loaded(selected_model);

    ImGui::NewLine(); ImGui::TextColored(cyan, "COMPONENTS"); ImGui::Separator();

    if (source.type == GT_FIRMWARE_SOURCE_NONE)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)gray);
        ImGui::TextWrapped("No firmware for this model. Add a set to the default folder, or choose a folder for it.");
        ImGui::PopStyleColor();
        draw_expected_files();
        return;
    }

    if (!loaded && !inspect(source))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)red);
        ImGui::TextWrapped("Required ROMs are missing or have the wrong size.");
        ImGui::PopStyleColor();
        draw_expected_files();
        return;
    }

    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit;
    static const int widths[4] = { 11, 11, 9, 0 };

    if (!ImGui::BeginTable("##firmware_components", 4, flags))
        return;

    setup_fixed_columns(widths, 4);

    for (int i = 0; i < GT_FIRMWARE_COUNT; i++)
    {
        GT_Firmware_Type type = (GT_Firmware_Type)i;
        ImGui::TableNextRow();
        draw_component(type, loaded ? core->GetFirmware()->GetInfo(type) : inspected_info[i]);
    }

    ImGui::EndTable();
}

static void draw_component(GT_Firmware_Type type, const GT_Firmware_Info& info)
{
    bool needed = Firmware::IsRequired(type) ||
        ((type == GT_FIRMWARE_EXTENSION) && (k_machine_maps[k_machine_profiles[selected_model].map].extension_rom != 0));

    draw_grid_label(k_component_labels[type]);

    if (info.synthetic && needed)
        ImGui::TextColored(red, "MISSING");
    else if (info.synthetic)
        ImGui::TextColored(gray, "%s", (type == GT_FIRMWARE_SERIAL) ? "GENERATED" : "BLANK");
    else if (info.recognized)
        ImGui::TextColored(green, "RECOGNIZED");
    else if (info.loaded)
        ImGui::TextColored(amber, "UNKNOWN");
    else if (needed)
        ImGui::TextColored(red, "MISSING");
    else
        ImGui::TextColored(gray, "OPTIONAL");

    bool hovered = ImGui::IsItemHovered();

    ImGui::TableNextColumn();
    ImGui::TextColored(orange, "%08X", info.crc);
    hovered = hovered || ImGui::IsItemHovered();

    ImGui::TableNextColumn();

    if (info.loaded)
        ImGui::TextColored(white, "%s", get_filename(info.path));
    else
        ImGui::TextColored(gray, "%s", Firmware::GetFileName(type));

    hovered = hovered || ImGui::IsItemHovered();

    if (!hovered)
        return;

    int size = Firmware::GetExpectedSize(type);

    ImGui::BeginTooltip();
    ImGui::Text("%s, %d %s", Firmware::GetComponentName(type), (size >= 1024) ? size / 1024 : size,
        (size >= 1024) ? "KB" : "bytes");

    if (info.synthetic)
        ImGui::Text("%s", (type == GT_FIRMWARE_SERIAL) ? "Not present, generated from the machine ID" :
            "Not present, reads return FFh");
    else if (info.loaded)
    {
        ImGui::Text("%s", info.recognized ? info.database_name : "Custom or unknown ROM");
        ImGui::Text("%s", info.path);
    }

    ImGui::EndTooltip();
}

static void draw_expected_files(void)
{
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit;
    static const int widths[3] = { 11, 11, 0 };

    ImGui::NewLine();

    if (!ImGui::BeginTable("##firmware_expected", 3, flags))
        return;

    setup_fixed_columns(widths, 3);

    for (int i = 0; i < GT_FIRMWARE_COUNT; i++)
    {
        GT_Firmware_Type type = (GT_Firmware_Type)i;
        ImGui::TableNextRow();
        draw_grid_label(k_component_labels[type]);
        ImGui::TextColored(white, "%s", Firmware::GetFileName(type));
        ImGui::TableNextColumn();
        int size = Firmware::GetExpectedSize(type);
        ImGui::TextColored(gray, "%d %s%s", (size >= 1024) ? size / 1024 : size, (size >= 1024) ? "KB" : "bytes",
            Firmware::IsRequired(type) ? "" : ", optional");
    }

    ImGui::EndTable();
}

static bool inspect(const GT_Firmware_Source& source)
{
    if (inspected && is_same_source(source, inspected_source))
        return inspected_ready;

    Firmware* firmware = new Firmware();
    inspected_ready = firmware->LoadSource(source, true);

    for (int i = 0; i < GT_FIRMWARE_COUNT; i++)
        inspected_info[i] = firmware->GetInfo((GT_Firmware_Type)i);

    SafeDelete(firmware);
    inspected_source = source;
    inspected = true;
    return inspected_ready;
}

static bool is_loaded(GT_Machine_Model model)
{
    GeartownsCore* core = emu_get_core();
    Firmware* firmware = core->GetFirmware();
    GT_Machine_Model loaded_model = emu_is_empty() ? core->GetPendingMachineConfig().model :
        core->GetMachineConfig().model;
    const GT_Firmware_Source& source = core->GetFirmwareSource(model);
    const GT_Firmware_Source& loaded = firmware->GetSource();

    return (model == loaded_model) && firmware->IsReady() && (source.type != GT_FIRMWARE_SOURCE_NONE) &&
        (loaded.zip == source.zip) && (strcmp(loaded.path, source.path) == 0);
}

static bool is_same_source(const GT_Firmware_Source& a, const GT_Firmware_Source& b)
{
    return (a.type == b.type) && (a.zip == b.zip) && (strcmp(a.path, b.path) == 0);
}

static float get_button_width(const char* label)
{
    return ImGui::CalcTextSize(label, NULL, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

static void setup_fixed_columns(const int* widths, int count)
{
    float character = ImGui::CalcTextSize("0").x;

    for (int i = 0; i < count; i++)
    {
        if (widths[i] > 0)
            ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * widths[i]);
        else
            ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthStretch);
    }
}

static void draw_grid_label(const char* label)
{
    ImGui::TableNextColumn();
    ImGui::TextColored(violet, "%s", label);
    ImGui::TableNextColumn();
}
