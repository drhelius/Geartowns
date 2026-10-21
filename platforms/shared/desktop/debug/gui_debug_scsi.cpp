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

#define GUI_DEBUG_SCSI_IMPORT
#include "gui_debug_scsi.h"

#include "imgui.h"
#include "geartowns.h"
#include "drive/scsi_controller.h"
#include "drive/scsi_disk.h"
#include "system/machine_profiles.h"
#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../gui_colors.h"
#include "gui_debug_constants.h"

static const char* const k_control_bits[8] = { "RST", "DMAE", "SEL", NULL, "ATN", NULL, "IMSK", "WEN" };
static const char* const k_status_bits[8] = { NULL, "INT", NULL, "BUSY", "C/D", "MSG", "I/O", "REQ" };

static void setup_scsi_columns(void);
static void draw_scsi_label(const char* label);
static void draw_scsi_register(const char* label, u8 value, bool fitted);
static void draw_scsi_port(const char* port);
static void draw_scsi_detail(void);
static void draw_scsi_bits(u8 value, const char* const* names, bool fitted);
static void draw_scsi_disk_row(int id);

void gui_debug_window_scsi(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(289, 243), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(292, 423), ImGuiCond_FirstUseEver);
    ImGui::Begin("SCSI", &config_debug.show_scsi);

    ImGui::PushFont(gui_default_font);

    ScsiController* scsi = emu_get_core()->GetSCSI();
    ScsiController::ScsiController_State* state = scsi->GetState();
    bool fitted = scsi->IsFitted();
    bool selected = fitted && (state->target >= 0);
    ScsiDisk::ScsiDisk_State* disk = selected ? scsi->GetDisk(state->target)->GetState() : NULL;
    u8 status = scsi->Peek(0x0C32);
    float space = ImGui::CalcTextSize(" ").x;
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

    ImGui::TextColored(cyan, "HOST INTERFACE"); ImGui::Separator();

    if (ImGui::BeginTable("##scsi_registers", 3, flags))
    {
        setup_scsi_columns();

        draw_scsi_label("FITTED");
        ImGui::TextColored(fitted ? green : gray, "%s", gui_debug_scsi_interface());

        draw_scsi_register("CONTROL", state->control, fitted);
        draw_scsi_port("0C32");
        draw_scsi_detail();
        draw_scsi_bits(state->control, k_control_bits, fitted);

        draw_scsi_register("STATUS", status, fitted);
        draw_scsi_port("0C32");
        draw_scsi_detail();
        draw_scsi_bits(status, k_status_bits, fitted);

        draw_scsi_register("DATA", state->data, fitted);
        draw_scsi_port("0C30");

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "BUS"); ImGui::Separator();

    if (ImGui::BeginTable("##scsi_bus", 3, flags))
    {
        setup_scsi_columns();

        draw_scsi_label("PHASE");

        if (fitted)
            ImGui::TextColored(blue, "%s", k_debug_scsi_phase_names[state->phase % SCSI_PHASE_COUNT]);
        else
            ImGui::TextColored(gray, "--");

        draw_scsi_label("TARGET");

        if (selected)
            ImGui::TextColored(orange, "ID %d  LUN %d", state->target, state->lun);
        else
            ImGui::TextColored(gray, "--");

        draw_scsi_label("MESSAGE");

        if (fitted)
            ImGui::TextColored(white, "$%02X %s", state->message, gui_debug_scsi_message_name(state->message));
        else
            ImGui::TextColored(gray, "--");

        ImGui::EndTable();
    }

    bool irq = fitted && state->interrupt && ((state->control & k_scsi_control_imsk) != 0);
    ImGui::TextColored(fitted && state->drq ? green : gray, "DRQ"); ImGui::SameLine(0.0f, space);
    ImGui::TextColored(irq ? yellow : gray, "IRQ8");

    ImGui::NewLine(); ImGui::TextColored(cyan, "COMMAND"); ImGui::Separator();

    if (ImGui::BeginTable("##scsi_command", 3, flags))
    {
        setup_scsi_columns();

        for (int row = 0; row < 2; row++)
        {
            draw_scsi_label(row == 0 ? "CDB" : "");

            for (int i = row * 8; i < (row + 1) * 8; i++)
            {
                if (i > row * 8)
                    ImGui::SameLine(0.0f, space);

                if (fitted && (i < state->command_count))
                    ImGui::TextColored(white, "%02X", state->command[i]);
                else
                    ImGui::TextColored(gray, "--");
            }
        }

        draw_scsi_detail();

        if (fitted && (state->command_count != 0))
        {
            char command[64];
            gui_debug_scsi_received_command(state->command, state->command_count, command, sizeof(command));
            ImGui::TextColored(blue, "%s", command);
        }
        else
            ImGui::TextColored(gray, "--");

        draw_scsi_label("STATUS");

        if (IsValidPointer(disk))
            ImGui::TextColored(disk->status == 0 ? green : red, "$%02X %s", disk->status,
                gui_debug_scsi_status_name(disk->status));
        else
            ImGui::TextColored(gray, "--");

        draw_scsi_label("DATA");

        if (IsValidPointer(disk))
            ImGui::TextColored(white, "%u / %u BYTES", disk->position, disk->length);
        else
            ImGui::TextColored(gray, "--");

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_scsi_disks(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(130, 110), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(398, 179), ImGuiCond_FirstUseEver);
    ImGui::Begin("SCSI Disks", &config_debug.show_scsi_disks);

    ImGui::PushFont(gui_default_font);

    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_NoHostExtendX;

    if (ImGui::BeginTable("##scsi_disks", 6, flags))
    {
        float character = ImGui::CalcTextSize("0").x;

        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, character * 2);
        ImGui::TableSetupColumn("IMAGE", ImGuiTableColumnFlags_WidthFixed, character * 16);
        ImGui::TableSetupColumn("BLOCKS", ImGuiTableColumnFlags_WidthFixed, character * 8);
        ImGui::TableSetupColumn("WP", ImGuiTableColumnFlags_WidthFixed, character * 3);
        ImGui::TableSetupColumn("SENSE KEY", ImGuiTableColumnFlags_WidthFixed, character * 15);
        ImGui::TableSetupColumn("ASC", ImGuiTableColumnFlags_WidthFixed, character * 3);
        ImGui::TableHeadersRow();

        for (int id = 0; id < GT_MAX_HARD_DISKS; id++)
            draw_scsi_disk_row(id);

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

const char* gui_debug_scsi_interface(void)
{
    GeartownsCore* core = emu_get_core();
    GT_Machine_Model model = core->GetMachineConfig().model;

    if (!core->GetSCSI()->IsFitted())
        return "NONE";

    return (k_machine_profiles[model].features & GT_MACHINE_FEATURE_SCSI) != 0 ? "BUILT IN" : "CARD";
}

const char* gui_debug_scsi_status_name(u8 status)
{
    switch (status)
    {
        case 0x00: return "GOOD";
        case 0x02: return "CHECK CONDITION";
        case 0x08: return "BUSY";
        default: return "";
    }
}

void gui_debug_scsi_received_command(const u8* command, int count, char* text, size_t size)
{
    static const int k_group_lengths[8] = { 6, 10, 10, 6, 16, 12, 6, 6 };

    if (count >= k_group_lengths[command[0] >> 5])
        gui_debug_scsi_command(command, text, size);
    else
        snprintf(text, size, "%s", gui_debug_scsi_command_name(command[0]));
}

static void setup_scsi_columns(void)
{
    float character = ImGui::CalcTextSize("0").x;

    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 7);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 24);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 6);
}

static void draw_scsi_label(const char* label)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(violet, "%s", label);
    ImGui::TableNextColumn();
}

static void draw_scsi_register(const char* label, u8 value, bool fitted)
{
    draw_scsi_label(label);

    if (fitted)
        ImGui::TextColored(white, "$%02X", value);
    else
        ImGui::TextColored(gray, "--");
}

static void draw_scsi_port(const char* port)
{
    ImGui::TableNextColumn();
    ImGui::TextColored(gray, "(%s)", port);
}

static void draw_scsi_detail(void)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TableNextColumn();
}

static void draw_scsi_bits(u8 value, const char* const* names, bool fitted)
{
    float space = ImGui::CalcTextSize(" ").x;
    bool first = true;

    for (int bit = 7; bit >= 0; bit--)
    {
        if (!IsValidPointer(names[bit]))
            continue;

        if (!first)
            ImGui::SameLine(0.0f, space);

        ImGui::TextColored(fitted && ((value >> bit) & 1) ? green : gray, "%s", names[bit]);
        first = false;
    }
}

static void draw_scsi_disk_row(int id)
{
    ScsiDisk* disk = emu_get_core()->GetSCSI()->GetDisk(id);
    ScsiDisk::ScsiDisk_State* state = disk->GetState();
    HardDiskImage* image = disk->GetImage();
    bool attached = disk->IsAttached();

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(orange, "%d", id);

    ImGui::TableNextColumn();

    if (attached)
    {
        ImGui::TextColored(white, "%.16s", image->GetFileName());

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", image->GetPath());
    }
    else
        ImGui::TextColored(gray, "--");

    ImGui::TableNextColumn();

    if (attached)
        ImGui::TextColored(white, "%u", image->GetBlocks());
    else
        ImGui::TextColored(gray, "--");

    ImGui::TableNextColumn();

    if (attached)
        ImGui::TextColored(image->IsWriteProtected() ? yellow : gray, "%s", image->IsWriteProtected() ? "YES" : "NO");
    else
        ImGui::TextColored(gray, "--");

    ImGui::TableNextColumn();

    if (attached)
        ImGui::TextColored(state->sense_key != 0 ? red : gray, "%s", k_debug_scsi_sense_keys[state->sense_key & 0x0F]);
    else
        ImGui::TextColored(gray, "--");

    ImGui::TableNextColumn();

    if (attached)
    {
        ImGui::TextColored(state->sense_code != 0 ? red : gray, "$%02X", state->sense_code);
        const char* name = gui_debug_scsi_sense_code_name(state->sense_code);

        if ((name[0] != 0) && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", name);
    }
    else
        ImGui::TextColored(gray, "--");
}
