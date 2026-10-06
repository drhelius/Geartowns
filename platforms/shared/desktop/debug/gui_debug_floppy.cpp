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

#define GUI_DEBUG_FLOPPY_IMPORT
#include "gui_debug_floppy.h"

#include "imgui.h"
#include "geartowns.h"
#include "drive/fdc.h"
#include "drive/floppy_disk.h"
#include "drive/floppy_image.h"
#include "drive/mb8877.h"
#include "system/scheduler.h"
#include "../config.h"
#include "../emu.h"
#include "../emu_floppy.h"
#include "../gui.h"
#include "../gui_colors.h"
#include "../utils.h"
#include "gui_debug_constants.h"
#include "gui_debug_memory.h"

static int selected_sector = -1;

static void draw_flag(const char* name, bool value);
static void draw_drive_column(int drive, int row);
static ImVec4 get_track_color(FloppyDisk* disk, int track);
static void goto_floppy_image(int drive, u32 offset);

void gui_debug_window_fdc(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(80, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("FDC", &config_debug.show_fdc);

    ImGui::PushFont(gui_default_font);

    GeartownsCore* core = emu_get_core();
    FDC* fdc = core->GetFDC();
    FDC::FDC_State* state = fdc->GetState();
    MB8877::MB8877_State* mb8877 = fdc->GetMB8877()->GetState();
    u64 clocks = core->GetScheduler()->GetClocks();
    u8 status = fdc->Peek(0x0200, clocks);
    char command[48];
    gui_debug_mb8877_command(mb8877->command, command, sizeof(command));

    ImGui::TextColored(cyan, "MB8877 REGISTERS"); ImGui::Separator();

    ImGui::TextColored(violet, "STATUS  "); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", status); ImGui::SameLine();
    ImGui::TextColored(gray, "0200");

    if (ImGui::BeginTable("##fdc_status", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX))
    {
        for (int bit = 7; bit >= 0; bit--)
        {
            bool set = ((status >> bit) & 1) != 0;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(gray, "%d", bit);
            ImGui::TableNextColumn();
            ImGui::TextColored(mb8877->type1 ? (set ? green : white) : dark_gray, "%s", k_debug_mb8877_type1_status[bit]);
            ImGui::TableNextColumn();
            ImGui::TextColored(!mb8877->type1 ? (set ? green : white) : dark_gray, "%s", k_debug_mb8877_type2_status[bit]);
        }

        ImGui::EndTable();
    }

    ImGui::TextColored(violet, "COMMAND "); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", mb8877->command);
    ImGui::TextColored(blue, "%-32s", command);
    ImGui::TextColored(violet, "TRACK   "); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", mb8877->track); ImGui::SameLine();
    ImGui::TextColored(gray, "0202");
    ImGui::TextColored(violet, "SECTOR  "); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", mb8877->sector); ImGui::SameLine();
    ImGui::TextColored(gray, "0204");
    ImGui::TextColored(violet, "DATA    "); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", mb8877->data); ImGui::SameLine();
    ImGui::TextColored(gray, "0206");

    ImGui::NewLine(); ImGui::TextColored(cyan, "SIGNALS"); ImGui::Separator();

    bool irq = mb8877->intrq && (state->drive_control & k_fdc_irq_enable) != 0;
    draw_flag("BUSY", (status & 0x01) != 0); ImGui::SameLine();
    draw_flag("DRQ", mb8877->drq); ImGui::SameLine();
    draw_flag("INTRQ", mb8877->intrq); ImGui::SameLine();
    ImGui::TextColored(irq ? yellow : gray, "IRQ6");

    ImGui::NewLine(); ImGui::TextColored(cyan, "DRIVE"); ImGui::Separator();

    u8 control = state->drive_control;
    ImGui::TextColored(violet, "CONTROL "); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", control); ImGui::SameLine();
    ImGui::TextColored(gray, "0208");
    ImGui::TextColored(violet, "        "); ImGui::SameLine();
    draw_flag("IRQ", (control & k_fdc_irq_enable) != 0); ImGui::SameLine();
    draw_flag("MOTOR", (control & k_fdc_motor) != 0); ImGui::SameLine();
    ImGui::TextColored(white, "SIDE %d", (control & k_fdc_side) ? 1 : 0);
    ImGui::TextColored(violet, "        "); ImGui::SameLine();
    ImGui::TextColored(blue, "%s", (control & k_fdc_double_density) ? "MFM" : "FM "); ImGui::SameLine();
    ImGui::TextColored(blue, "%s", (control & k_fdc_slow_clock) ? "SLOW CLOCK" : "FAST CLOCK");

    u8 drive_status = fdc->Peek(0x0208, clocks);
    ImGui::TextColored(violet, "STATUS  "); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", drive_status); ImGui::SameLine();
    draw_flag("READY", (drive_status & 0x02) != 0);

    int selected = fdc->GetSelectedDrive();
    ImGui::TextColored(violet, "SELECT  "); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", state->drive_select); ImGui::SameLine();
    ImGui::TextColored(gray, "020C");
    ImGui::TextColored(violet, "        "); ImGui::SameLine();

    if (selected >= 0)
        ImGui::TextColored(orange, "DRIVE %d", selected);
    else
        ImGui::TextColored(gray, "NONE   ");

    ImGui::SameLine();
    ImGui::TextColored(blue, "%u RPM", fdc->GetRPM());
    ImGui::TextColored(violet, "SWITCH  "); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", state->drive_switch); ImGui::SameLine();
    ImGui::TextColored(gray, "020E");

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_floppy_drives(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(110, 90), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(460, 260), ImGuiCond_FirstUseEver);
    ImGui::Begin("Floppy Drives", &config_debug.show_floppy_drives);

    ImGui::PushFont(gui_default_font);

    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

    if (ImGui::BeginTable("##floppy_drives", 3, flags))
    {
        ImGui::TableSetupColumn(" ");
        ImGui::TableSetupColumn("DRIVE 0");
        ImGui::TableSetupColumn("DRIVE 1");
        ImGui::TableHeadersRow();

        static const char* rows[11] =
        {
            "IMAGE", "DISK NAME", "MEDIA", "GEOMETRY", "RPM", "PROTECT", "MODIFIED", "HEAD", "MOTOR", "READY", "SELECTED"
        };

        for (int row = 0; row < 11; row++)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(violet, "%-9s", rows[row]);

            for (int drive = 0; drive < FDC_DRIVES; drive++)
            {
                ImGui::TableNextColumn();
                draw_drive_column(drive, row);
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_disk_viewer(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(140, 70), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 560), ImGuiCond_FirstUseEver);
    ImGui::Begin("Disk Viewer", &config_debug.show_disk_viewer);

    ImGui::PushFont(gui_default_font);

    GeartownsCore* core = emu_get_core();
    FDC* fdc = core->GetFDC();
    int drive = CLAMP(config_debug.disk_viewer_drive, 0, FDC_DRIVES - 1);
    FloppyDisk* disk = fdc->GetDisk(drive);
    int max_cylinder = k_floppy_tracks / 2 - 1;

    ImGui::PushItemWidth(80.0f);

    if (ImGui::Combo("DRIVE##disk_drive", &config_debug.disk_viewer_drive, "DRIVE 0\0DRIVE 1\0\0"))
        selected_sector = -1;

    ImGui::SameLine();

    if (ImGui::InputInt("CYLINDER##disk_cylinder", &config_debug.disk_viewer_cylinder))
        selected_sector = -1;

    ImGui::SameLine();

    if (ImGui::Combo("HEAD##disk_head", &config_debug.disk_viewer_head, "0\0" "1\0\0"))
        selected_sector = -1;

    ImGui::PopItemWidth();

    config_debug.disk_viewer_cylinder = CLAMP(config_debug.disk_viewer_cylinder, 0, max_cylinder);
    config_debug.disk_viewer_head = CLAMP(config_debug.disk_viewer_head, 0, 1);
    int cylinder = config_debug.disk_viewer_cylinder;
    int head = config_debug.disk_viewer_head;
    int track = cylinder * 2 + head;

    if (!disk->IsInserted())
    {
        ImGui::TextColored(gray, "No disk in drive %d", drive);
        ImGui::PopFont();
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "TRACK MAP"); ImGui::Separator();

    const float cell = 6.0f;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    int head_cylinder = fdc->GetState()->cylinders[drive];

    for (int t = 0; t < k_floppy_tracks; t++)
    {
        int c = t / 2;
        int h = t & 1;
        ImVec2 min(origin.x + c * cell, origin.y + h * (cell + 2.0f));
        ImVec2 max(min.x + cell - 1.0f, min.y + cell);
        draw_list->AddRectFilled(min, max, ImColor(get_track_color(disk, t)));

        if (t == track)
            draw_list->AddRect(ImVec2(min.x - 1, min.y - 1), ImVec2(max.x + 1, max.y + 1), ImColor(white));
    }

    ImVec2 head_min(origin.x + head_cylinder * cell - 1.0f, origin.y - 3.0f);
    draw_list->AddRect(head_min, ImVec2(head_min.x + cell + 1.0f, head_min.y + 2 * cell + 7.0f), ImColor(orange));

    ImGui::InvisibleButton("##track_map", ImVec2(k_floppy_tracks / 2 * cell, 2 * cell + 2.0f));

    if (ImGui::IsItemHovered())
    {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        int c = CLAMP((int)((mouse.x - origin.x) / cell), 0, max_cylinder);
        int h = CLAMP((int)((mouse.y - origin.y) / (cell + 2.0f)), 0, 1);
        FloppyDisk_Sector sectors[k_floppy_max_sectors];
        int count = disk->GetSectors(c * 2 + h, sectors);
        ImGui::SetTooltip("CYLINDER %d HEAD %d\n%d SECTORS", c, h, count);

        if (ImGui::IsMouseClicked(0))
        {
            config_debug.disk_viewer_cylinder = c;
            config_debug.disk_viewer_head = h;
            selected_sector = -1;
        }
    }

    ImGui::TextColored(green, "NORMAL"); ImGui::SameLine();
    ImGui::TextColored(yellow, " DELETED"); ImGui::SameLine();
    ImGui::TextColored(red, " ERRORS"); ImGui::SameLine();
    ImGui::TextColored(gray, " UNFORMATTED"); ImGui::SameLine();
    ImGui::TextColored(orange, " HEAD");

    ImGui::NewLine(); ImGui::TextColored(cyan, "SECTORS"); ImGui::Separator();

    FloppyDisk_Sector sectors[k_floppy_max_sectors];
    int count = disk->GetSectors(track, sectors);

    if (selected_sector >= count)
        selected_sector = -1;

    ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("##disk_sectors", 9, flags, ImVec2(0, 150)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn("C");
        ImGui::TableSetupColumn("H");
        ImGui::TableSetupColumn("R");
        ImGui::TableSetupColumn("N");
        ImGui::TableSetupColumn("DENSITY");
        ImGui::TableSetupColumn("DELETED");
        ImGui::TableSetupColumn("STATUS");
        ImGui::TableSetupColumn("OFFSET");
        ImGui::TableHeadersRow();

        for (int i = 0; i < count; i++)
        {
            const FloppyDisk_Sector& sector = sectors[i];
            bool error = sector.status != 0x00 && sector.status != 0x10;
            char label[16];
            snprintf(label, sizeof(label), "%02d##sector%d", i + 1, i);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            if (ImGui::Selectable(label, selected_sector == i, ImGuiSelectableFlags_SpanAllColumns))
                selected_sector = i;

            ImGui::TableNextColumn(); ImGui::TextColored(white, "%02X", sector.id[0]);
            ImGui::TableNextColumn(); ImGui::TextColored(white, "%02X", sector.id[1]);
            ImGui::TableNextColumn(); ImGui::TextColored(orange, "%02X", sector.id[2]);
            ImGui::TableNextColumn(); ImGui::TextColored(white, "%02X %4d", sector.id[3], sector.size);
            ImGui::TableNextColumn(); ImGui::TextColored(blue, "%s", sector.fm ? "FM " : "MFM");
            ImGui::TableNextColumn(); ImGui::TextColored(sector.deleted ? yellow : gray, "%s", sector.deleted ? "YES" : "NO ");
            ImGui::TableNextColumn(); ImGui::TextColored(error ? red : green, "%s", gui_debug_floppy_status_name(sector.status));
            ImGui::TableNextColumn(); ImGui::TextColored(white, "$%06X", sector.header + k_floppy_sector_header_size);
        }

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "SECTOR DATA"); ImGui::Separator();

    if (selected_sector < 0)
    {
        ImGui::TextColored(gray, "Select a sector");
        ImGui::PopFont();
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    const FloppyDisk_Sector& sector = sectors[selected_sector];
    u32 data_offset = sector.header + k_floppy_sector_header_size;
    const u8* image = disk->GetImage();
    u32 image_size = disk->GetImageSize();
    u32 size = MIN((u32)sector.size, image_size > data_offset ? image_size - data_offset : 0);

    ImGui::TextColored(violet, "C %02X H %02X R %02X N %02X", sector.id[0], sector.id[1], sector.id[2], sector.id[3]);
    ImGui::SameLine();
    ImGui::PopFont();

    if (ImGui::SmallButton("Open in Memory Workspace"))
        goto_floppy_image(drive, data_offset);

    ImGui::PushFont(gui_default_font);
    ImGui::BeginChild("##sector_data", ImVec2(0, 0), ImGuiChildFlags_Borders);

    ImGuiListClipper clipper;
    clipper.Begin((int)((size + 15) / 16));

    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
        {
            u32 base = (u32)row * 16;
            char ascii[17];

            ImGui::TextColored(cyan, "%04X", base); ImGui::SameLine();

            for (int i = 0; i < 16; i++)
            {
                u32 offset = base + i;

                if (offset < size)
                {
                    u8 value = image[data_offset + offset];
                    ImGui::TextColored(value ? white : gray, "%02X", value);
                    ascii[i] = (value >= 0x20 && value < 0x7F) ? (char)value : '.';
                }
                else
                {
                    ImGui::TextColored(gray, "  ");
                    ascii[i] = ' ';
                }

                ImGui::SameLine();
            }

            ascii[16] = 0;
            ImGui::TextColored(gray, "%s", ascii);
        }
    }

    ImGui::EndChild();

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

static void draw_flag(const char* name, bool value)
{
    ImGui::TextColored(value ? green : gray, "%s", name);
}

static void draw_drive_column(int drive, int row)
{
    GeartownsCore* core = emu_get_core();
    FDC* fdc = core->GetFDC();
    FloppyDisk* disk = fdc->GetDisk(drive);
    bool present = drive < core->GetMachineConfig().floppy_drives;
    bool inserted = present && disk->IsInserted();
    bool selected = present && fdc->GetSelectedDrive() == drive;
    Emu_FloppyInfo info;

    if (!present)
    {
        ImGui::TextColored(gray, "NOT PRESENT");
        return;
    }

    switch (row)
    {
        case 0:
        {
            if (inserted && emu_floppy_get_info(drive, &info) && info.path[0] != 0)
            {
                const char* name = strrchr(info.path, '/');
                const char* windows_name = strrchr(info.path, '\\');
                name = (IsValidPointer(windows_name) && windows_name > name) ? windows_name : name;
                ImGui::TextColored(white, "%.20s", IsValidPointer(name) ? name + 1 : info.path);

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", info.path);
            }
            else
                ImGui::TextColored(gray, "--");

            break;
        }
        case 1:
        {
            char name[18] = { };

            if (inserted)
                gui_debug_floppy_disk_name(disk, name, sizeof(name));

            if (name[0] != 0)
                ImGui::TextColored(white, "%s", name);
            else
                ImGui::TextColored(gray, "--");

            break;
        }
        case 2:
            if (inserted)
                ImGui::TextColored(blue, "%s", k_debug_floppy_media_names[disk->GetMedia() % 3]);
            else
                ImGui::TextColored(gray, "--");

            break;
        case 3:
        {
            int cylinders = 0;
            int heads = 0;
            int sectors = 0;
            int sector_size = 0;

            if (inserted && gui_debug_floppy_geometry(disk, cylinders, heads, sectors, sector_size))
                ImGui::TextColored(white, "%dx%dx%d %dK", cylinders, heads, sectors,
                    cylinders * heads * sectors * sector_size / 1024);
            else
                ImGui::TextColored(gray, "--");

            break;
        }
        case 4:
            if (inserted)
                ImGui::TextColored(white, "%u", disk->GetRPM());
            else
                ImGui::TextColored(gray, "--");

            break;
        case 5:
            if (inserted)
                ImGui::TextColored(disk->IsWriteProtected() ? yellow : gray, "%s", disk->IsWriteProtected() ? "YES" : "NO ");
            else
                ImGui::TextColored(gray, "--");

            break;
        case 6:
            if (inserted)
                ImGui::TextColored(disk->IsDirty() ? yellow : gray, "%s", disk->IsDirty() ? "YES" : "NO ");
            else
                ImGui::TextColored(gray, "--");

            break;
        case 7:
            ImGui::TextColored(white, "CYLINDER %d", fdc->GetState()->cylinders[drive]);
            break;
        case 8:
            draw_flag(selected && fdc->IsSpinning() ? "ON " : "OFF", selected && fdc->IsSpinning());
            break;
        case 9:
        {
            bool ready = selected && fdc->IsReady(core->GetScheduler()->GetClocks());
            draw_flag(ready ? "YES" : "NO ", ready);
            break;
        }
        default:
            draw_flag(selected ? "YES" : "NO ", selected);
            break;
    }
}

void gui_debug_floppy_disk_name(FloppyDisk* disk, char* name, size_t size)
{
    const u8* image = disk->GetImage();
    size_t length = 0;

    if (IsValidPointer(image) && disk->GetImageSize() >= 17)
    {
        while (length < 17 && length + 1 < size && image[length] != 0)
        {
            u8 c = image[length];
            name[length] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
            length++;
        }
    }

    name[length] = 0;
}

bool gui_debug_floppy_geometry(FloppyDisk* disk, int& cylinders, int& heads, int& sectors, int& sector_size)
{
    FloppyDisk_Sector list[k_floppy_max_sectors];
    int last = -1;
    bool second_head = false;

    for (int t = 0; t < k_floppy_tracks; t++)
    {
        if (disk->GetSectors(t, list) == 0)
            continue;

        last = t;
        second_head = second_head || (t & 1) != 0;
    }

    sectors = disk->GetSectors(0, list);

    if (last < 0 || sectors == 0)
        return false;

    cylinders = last / 2 + 1;
    heads = second_head ? 2 : 1;
    sector_size = list[0].size;
    return true;
}

static ImVec4 get_track_color(FloppyDisk* disk, int track)
{
    FloppyDisk_Sector sectors[k_floppy_max_sectors];
    int count = disk->GetSectors(track, sectors);
    bool deleted = false;

    if (count == 0)
        return dark_gray;

    for (int i = 0; i < count; i++)
    {
        if (sectors[i].status != 0x00 && sectors[i].status != 0x10)
            return red;

        deleted = deleted || sectors[i].deleted;
    }

    return deleted ? yellow : green;
}

static void goto_floppy_image(int drive, u32 offset)
{
    GT_Debug_Memory_Address target = { };
    target.space = GT_DEBUG_MEMORY_REGION;
    target.region = GT_DEBUG_REGION_FLOPPY_IMAGE + drive;
    target.address = offset;
    target.segment_register = -1;
    gui_debug_memory_goto(target);
}
