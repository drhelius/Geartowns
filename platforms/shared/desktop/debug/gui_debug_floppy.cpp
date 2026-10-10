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

#include <SDL3/SDL.h>
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

static void setup_fdc_columns(void);
static void draw_fdc_register(const char* label, u8 value);
static void draw_fdc_port(const char* port);
static void draw_fdc_detail(void);
static void draw_flag(const char* name, bool value);
static void draw_drive_column(int drive, int row);
static ImVec4 get_track_color(FloppyDisk* disk, int track);
static void goto_floppy_image(int drive, u32 offset);

void gui_debug_window_fdc(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(289, 243), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(265, 545), ImGuiCond_FirstUseEver);
    ImGui::Begin("FDC", &config_debug.show_fdc);

    ImGui::PushFont(gui_default_font);

    GeartownsCore* core = emu_get_core();
    FDC* fdc = core->GetFDC();
    FDC::FDC_State* state = fdc->GetState();
    MB8877::MB8877_State* mb8877 = fdc->GetMB8877()->GetState();
    u64 clocks = core->GetScheduler()->GetClocks();
    u8 status = fdc->Peek(0x0200, clocks);
    u8 control = state->drive_control;
    u8 drive_status = fdc->Peek(0x0208, clocks);
    int selected = fdc->GetSelectedDrive();
    float space = ImGui::CalcTextSize(" ").x;
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    char command[48];
    gui_debug_mb8877_command(mb8877->command, command, sizeof(command));

    // The decoder writes the name and then its flags, which go on their own row
    char* parameters = strchr(command, '=');

    while (IsValidPointer(parameters) && parameters > command && *parameters != ' ')
        parameters--;

    if (IsValidPointer(parameters) && parameters > command)
        *parameters++ = 0;
    else
        parameters = NULL;

    ImGui::TextColored(cyan, "MB8877 REGISTERS"); ImGui::Separator();

    if (ImGui::BeginTable("##fdc_status_register", 3, flags))
    {
        setup_fdc_columns();
        draw_fdc_register("STATUS", status);
        draw_fdc_port("0200");
        ImGui::EndTable();
    }

    if (ImGui::BeginTable("##fdc_status", 3, flags))
    {
        ImGui::TableSetupColumn("");
        ImGui::TableSetupColumn("TYPE I");
        ImGui::TableSetupColumn("TYPE II/III");
        ImGui::TableHeadersRow();

        for (int bit = 7; bit >= 0; bit--)
        {
            bool set = ((status >> bit) & 1) != 0;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(gray, "%d", bit);
            ImGui::TableNextColumn();
            ImGui::TextColored(mb8877->type1 ? (set ? green : white) : gray, "%s", k_debug_mb8877_type1_status[bit]);
            ImGui::TableNextColumn();
            ImGui::TextColored(!mb8877->type1 ? (set ? green : white) : gray, "%s", k_debug_mb8877_type2_status[bit]);
        }

        ImGui::EndTable();
    }

    if (ImGui::BeginTable("##fdc_registers", 3, flags))
    {
        setup_fdc_columns();

        draw_fdc_register("COMMAND", mb8877->command);
        draw_fdc_port("0200");
        draw_fdc_detail();
        ImGui::TextColored(blue, "%s", command);
        draw_fdc_detail();

        if (IsValidPointer(parameters))
            ImGui::TextColored(white, "%s", parameters);

        draw_fdc_register("TRACK", mb8877->track);
        draw_fdc_port("0202");
        draw_fdc_register("SECTOR", mb8877->sector);
        draw_fdc_port("0204");
        draw_fdc_register("DATA", mb8877->data);
        draw_fdc_port("0206");

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "SIGNALS"); ImGui::Separator();

    bool irq = mb8877->intrq && (control & k_fdc_irq_enable) != 0;
    draw_flag("BUSY", (status & 0x01) != 0); ImGui::SameLine(0.0f, space);
    draw_flag("DRQ", mb8877->drq); ImGui::SameLine(0.0f, space);
    draw_flag("INTRQ", mb8877->intrq); ImGui::SameLine(0.0f, space);
    ImGui::TextColored(irq ? yellow : gray, "IRQ6");

    ImGui::NewLine(); ImGui::TextColored(cyan, "DRIVE"); ImGui::Separator();

    if (ImGui::BeginTable("##fdc_drive", 3, flags))
    {
        setup_fdc_columns();

        draw_fdc_register("CONTROL", control);
        draw_fdc_port("0208");
        draw_fdc_detail();
        draw_flag("IRQ", (control & k_fdc_irq_enable) != 0); ImGui::SameLine(0.0f, space);
        draw_flag("MOTOR", (control & k_fdc_motor) != 0); ImGui::SameLine(0.0f, space);
        ImGui::TextColored(white, "SIDE %d", (control & k_fdc_side) ? 1 : 0);
        draw_fdc_detail();
        ImGui::TextColored(blue, "%s", (control & k_fdc_double_density) ? "MFM" : "FM"); ImGui::SameLine(0.0f, space);
        ImGui::TextColored(blue, "%s", (control & k_fdc_slow_clock) ? "SLOW CLOCK" : "FAST CLOCK");

        draw_fdc_register("STATUS", drive_status); ImGui::SameLine(0.0f, space);
        draw_flag("READY", (drive_status & 0x02) != 0);
        draw_fdc_port("0208");

        draw_fdc_register("SELECT", state->drive_select); ImGui::SameLine(0.0f, space);

        if (selected >= 0)
            ImGui::TextColored(orange, "DRIVE %d", selected);
        else
            ImGui::TextColored(gray, "NONE");

        ImGui::SameLine(0.0f, space);
        ImGui::TextColored(blue, "%u RPM", fdc->GetRPM());
        draw_fdc_port("020C");

        draw_fdc_register("SWITCH", state->drive_switch);
        draw_fdc_port("020E");

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_floppy_drives(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(130, 110), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(382, 243), ImGuiCond_FirstUseEver);
    ImGui::Begin("Floppy Drives", &config_debug.show_floppy_drives);

    ImGui::PushFont(gui_default_font);

    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

    if (ImGui::BeginTable("##floppy_drives", 3, flags))
    {
        float character = ImGui::CalcTextSize("0").x;

        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, character * 9);
        ImGui::TableSetupColumn("DRIVE 0", ImGuiTableColumnFlags_WidthFixed, character * 20);
        ImGui::TableSetupColumn("DRIVE 1", ImGuiTableColumnFlags_WidthFixed, character * 20);
        ImGui::TableHeadersRow();

        static const char* rows[11] =
        {
            "IMAGE", "DISK NAME", "MEDIA", "GEOMETRY", "RPM", "PROTECT", "MODIFIED", "HEAD", "MOTOR", "READY", "SELECTED"
        };

        for (int row = 0; row < 11; row++)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(violet, "%s", rows[row]);

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
    ImGui::SetNextWindowPos(ImVec2(171, 177), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(508, 536), ImGuiCond_FirstUseEver);
    ImGui::Begin("Disk Viewer", &config_debug.show_disk_viewer);

    GeartownsCore* core = emu_get_core();
    FDC* fdc = core->GetFDC();
    int max_cylinder = k_floppy_tracks / 2 - 1;
    ImGuiStyle& style = ImGui::GetStyle();
    float arrow = ImGui::GetFrameHeight();
    float steps = (arrow + style.ItemInnerSpacing.x) * 2.0f;
    float padding = style.FramePadding.x * 2.0f;

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Drive"); ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("0").x + padding + arrow);

    if (ImGui::Combo("##disk_drive", &config_debug.disk_viewer_drive, "0\0" "1\0"))
        selected_sector = -1;

    ImGui::SameLine(0.0f, 16.0f);
    ImGui::TextUnformatted("Cylinder"); ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("00").x + padding + steps);

    if (ImGui::InputInt("##disk_cylinder", &config_debug.disk_viewer_cylinder, 1, 10))
        selected_sector = -1;

    ImGui::SameLine(0.0f, 16.0f);
    ImGui::TextUnformatted("Head"); ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("0").x + padding + arrow);

    if (ImGui::Combo("##disk_head", &config_debug.disk_viewer_head, "0\0" "1\0"))
        selected_sector = -1;

    config_debug.disk_viewer_drive = CLAMP(config_debug.disk_viewer_drive, 0, FDC_DRIVES - 1);
    config_debug.disk_viewer_cylinder = CLAMP(config_debug.disk_viewer_cylinder, 0, max_cylinder);
    config_debug.disk_viewer_head = CLAMP(config_debug.disk_viewer_head, 0, 1);
    int drive = config_debug.disk_viewer_drive;
    int cylinder = config_debug.disk_viewer_cylinder;
    int head = config_debug.disk_viewer_head;
    int track = cylinder * 2 + head;
    FloppyDisk* disk = fdc->GetDisk(drive);

    ImGui::PushFont(gui_default_font);

    float space = ImGui::CalcTextSize(" ").x;

    ImGui::NewLine(); ImGui::TextColored(cyan, "TRACK MAP"); ImGui::Separator();

    const float cell = 6.0f;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    int head_cylinder = fdc->GetState()->cylinders[drive];

    for (int t = 0; t < k_floppy_tracks; t++)
    {
        int c = t / 2;
        int h = t & 1;
        ImVec2 cell_min(origin.x + c * cell, origin.y + h * (cell + 2.0f));
        ImVec2 cell_max(cell_min.x + cell - 1.0f, cell_min.y + cell);
        draw_list->AddRectFilled(cell_min, cell_max, ImColor(get_track_color(disk, t)));

        if (t == track)
            draw_list->AddRect(ImVec2(cell_min.x - 1, cell_min.y - 1), ImVec2(cell_max.x + 1, cell_max.y + 1), ImColor(white));
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

    ImGui::TextColored(green, "NORMAL"); ImGui::SameLine(0.0f, space * 2.0f);
    ImGui::TextColored(yellow, "DELETED"); ImGui::SameLine(0.0f, space * 2.0f);
    ImGui::TextColored(red, "ERRORS"); ImGui::SameLine(0.0f, space * 2.0f);
    ImGui::TextColored(gray, "UNFORMATTED"); ImGui::SameLine(0.0f, space * 2.0f);
    ImGui::TextColored(orange, "HEAD");

    ImGui::NewLine(); ImGui::TextColored(cyan, "SECTORS"); ImGui::Separator();

    FloppyDisk_Sector sectors[k_floppy_max_sectors];
    int count = disk->GetSectors(track, sectors);

    if (selected_sector >= count)
        selected_sector = -1;

    ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingFixedFit;

    // Always 16 rows tall so the window keeps its layout, longer tracks scroll
    float row_height = ImGui::GetTextLineHeight() + style.CellPadding.y * 2.0f;
    float table_height = 17 * row_height + 4.0f;

    if (ImGui::BeginTable("##disk_sectors", 10, flags, ImVec2(0, table_height)))
    {
        static const char* headers[10] = { "#", "C", "H", "R", "N", "SIZE", "DENSITY", "DELETED", "STATUS", "OFFSET" };

        ImGui::TableSetupScrollFreeze(0, 1);

        for (int i = 0; i < 10; i++)
            ImGui::TableSetupColumn(headers[i]);

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
            ImGui::TableNextColumn(); ImGui::TextColored(white, "%02X", sector.id[3]);
            ImGui::TableNextColumn(); ImGui::TextColored(white, "%d", sector.size);
            ImGui::TableNextColumn(); ImGui::TextColored(blue, "%s", sector.fm ? "FM" : "MFM");
            ImGui::TableNextColumn(); ImGui::TextColored(sector.deleted ? yellow : gray, "%s", sector.deleted ? "YES" : "NO");
            ImGui::TableNextColumn(); ImGui::TextColored(error ? red : green, "%s", gui_debug_floppy_status_name(sector.status));
            ImGui::TableNextColumn(); ImGui::TextColored(white, "$%06X", sector.header + k_floppy_sector_header_size);
        }

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "SECTOR DATA"); ImGui::Separator();

    bool valid = selected_sector >= 0;
    const FloppyDisk_Sector* sector = valid ? &sectors[selected_sector] : NULL;
    u32 data_offset = valid ? sector->header + k_floppy_sector_header_size : 0;
    const u8* image = disk->GetImage();
    u32 image_size = disk->GetImageSize();
    u32 size = valid ? MIN((u32)sector->size, image_size > data_offset ? image_size - data_offset : 0) : 0;
    static const char* ids[4] = { "C", "H", "R", "N" };

    for (int i = 0; i < 4; i++)
    {
        ImGui::TextColored(violet, "%s", ids[i]); ImGui::SameLine(0.0f, space);

        if (valid)
            ImGui::TextColored(i == 2 ? orange : white, "%02X", sector->id[i]);
        else
            ImGui::TextColored(gray, "--");

        ImGui::SameLine(0.0f, space * 2.0f);
    }

    ImGui::TextColored(violet, "OFFSET"); ImGui::SameLine(0.0f, space);

    if (valid)
    {
        ImGui::TextColored(cyan, "$%06X", data_offset);

        if (ImGui::IsItemClicked())
            goto_floppy_image(drive, data_offset);

        if (ImGui::IsItemHovered())
        {
            ImGui::PushFont(gui_roboto_font);
            ImGui::SetTooltip("Open in Memory Workspace");
            ImGui::PopFont();
        }
    }
    else
        ImGui::TextColored(gray, "--");

    ImGui::BeginChild("##sector_data", ImVec2(0, 0), ImGuiChildFlags_Borders);

    ImGuiListClipper clipper;
    clipper.Begin((int)((size + 15) / 16));

    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
        {
            u32 base = (u32)row * 16;
            char ascii[17];

            ImGui::TextColored(cyan, "%04X", base); ImGui::SameLine(0.0f, space * 2.0f);

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

                ImGui::SameLine(0.0f, i == 15 ? space * 2.0f : space);
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

static void setup_fdc_columns(void)
{
    float character = ImGui::CalcTextSize("0").x;

    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 7);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 20);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 6);
}

static void draw_fdc_register(const char* label, u8 value)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(violet, "%s", label);
    ImGui::TableNextColumn();
    ImGui::TextColored(white, "$%02X", value);
}

static void draw_fdc_port(const char* port)
{
    ImGui::TableNextColumn();
    ImGui::TextColored(gray, "(%s)", port);
}

static void draw_fdc_detail(void)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TableNextColumn();
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
        ImGui::TextColored(gray, "%s", row == 0 ? "NOT PRESENT" : "--");
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
            char name[64] = { };

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
            u32 total_size = 0;
            bool mixed = false;

            if (inserted && gui_debug_floppy_geometry(disk, cylinders, heads, sectors, sector_size, total_size, mixed))
            {
                ImGui::TextColored(mixed ? yellow : white, "%dx%dx%d %uK", cylinders, heads, sectors, total_size / 1024);

                if (mixed && ImGui::IsItemHovered())
                    ImGui::SetTooltip("Tracks use different layouts\n"
                        "Sectors per track are track 0's, the size counts every track");
            }
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
                ImGui::TextColored(disk->IsWriteProtected() ? yellow : gray, "%s", disk->IsWriteProtected() ? "YES" : "NO");
            else
                ImGui::TextColored(gray, "--");

            break;
        case 6:
            if (inserted)
                ImGui::TextColored(disk->IsDirty() ? yellow : gray, "%s", disk->IsDirty() ? "YES" : "NO");
            else
                ImGui::TextColored(gray, "--");

            break;
        case 7:
            ImGui::TextColored(white, "CYLINDER %d", fdc->GetState()->cylinders[drive]);
            break;
        case 8:
            draw_flag(selected && fdc->IsSpinning() ? "ON" : "OFF", selected && fdc->IsSpinning());
            break;
        case 9:
        {
            bool ready = selected && fdc->IsReady(core->GetScheduler()->GetClocks());
            draw_flag(ready ? "YES" : "NO", ready);
            break;
        }
        default:
            draw_flag(selected ? "YES" : "NO", selected);
            break;
    }
}

// D77 names are Shift-JIS, converted to UTF-8, with ASCII as the fallback
void gui_debug_floppy_disk_name(FloppyDisk* disk, char* name, size_t size)
{
    const u8* image = disk->GetImage();
    char raw[18] = { };
    size_t length = 0;

    name[0] = 0;

    if (!IsValidPointer(image) || disk->GetImageSize() < 17 || size == 0)
        return;

    while (length < 17 && image[length] != 0)
    {
        raw[length] = (char)image[length];
        length++;
    }

    char* converted = SDL_iconv_string("UTF-8", "SHIFT-JIS", raw, length + 1);

    if (IsValidPointer(converted))
    {
        snprintf(name, size, "%s", converted);
        SDL_free(converted);
        return;
    }

    for (size_t i = 0; i < length && i + 1 < size; i++)
    {
        u8 c = (u8)raw[i];
        name[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
        name[i + 1] = 0;
    }
}

// Cylinders and heads cover every formatted track, sectors per track come from track 0
// A disk whose tracks differ from track 0 is mixed, and its size adds up every sector
bool gui_debug_floppy_geometry(FloppyDisk* disk, int& cylinders, int& heads, int& sectors, int& sector_size,
    u32& total_size, bool& mixed)
{
    FloppyDisk_Sector list[k_floppy_max_sectors];
    int last = -1;
    bool second_head = false;

    sectors = disk->GetSectors(0, list);
    sector_size = sectors > 0 ? list[0].size : 0;
    total_size = 0;
    mixed = false;

    for (int t = 0; t < k_floppy_tracks; t++)
    {
        int count = disk->GetSectors(t, list);

        if (count == 0)
            continue;

        last = t;
        second_head = second_head || (t & 1) != 0;
        mixed = mixed || count != sectors;

        for (int i = 0; i < count; i++)
        {
            total_size += list[i].size;
            mixed = mixed || list[i].size != sector_size;
        }
    }

    if (last < 0 || sectors == 0)
        return false;

    cylinders = last / 2 + 1;
    heads = second_head ? 2 : 1;
    return true;
}

static ImVec4 get_track_color(FloppyDisk* disk, int track)
{
    FloppyDisk_Sector sectors[k_floppy_max_sectors];
    int count = disk->GetSectors(track, sectors);
    bool deleted = false;

    if (count == 0)
        return gray;

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
