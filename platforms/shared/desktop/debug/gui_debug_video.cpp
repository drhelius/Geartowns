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

#define GUI_DEBUG_VIDEO_IMPORT
#include "gui_debug_video.h"

#include "imgui.h"
#include "geartowns.h"
#include "system/memory.h"
#include "system/scheduler.h"
#include "video/sprite.h"
#include "video/video.h"
#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../gui_colors.h"
#include "../utils.h"
#include "gui_debug_constants.h"
#include "gui_debug_memory.h"
#include "gui_debug_widgets.h"

static void crtc_write_callback(u16 index, u16 value, void* user_data);
static void setup_grid_columns(bool layers);
static void draw_grid_label(const char* label);
static void draw_layer_column(int layer, int row);
static void draw_palette_title(const char* title, bool in_use);
static void setup_palette_columns(void);
static void draw_palette_indices(int count);
static void draw_palette_swatches(const u8 (*colors)[3], int first, bool nibbles);
static void draw_centered_digit(const ImVec4& color, int value);
static void draw_color_tooltip(int index, const u8* color, bool nibbles);
static void goto_vram(u32 offset, bool single_page);

void gui_debug_window_crtc(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(156, 172), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(387, 490), ImGuiCond_FirstUseEver);
    ImGui::Begin("CRTC", &config_debug.show_crtc);

    ImGui::PushFont(gui_default_font);

    GeartownsCore* core = emu_get_core();
    Video* video = core->GetVideo();
    Video::Video_State* state = video->GetState();
    const u16* crtc = state->crtc;
    double clock = k_debug_crtc_clocks[crtc[k_video_crtc_cr1] & 0x03];
    u32 line_clocks = (u32)crtc[k_video_crtc_hst] + 1;
    u32 half_lines = (u32)crtc[k_video_crtc_vst] + 1;
    bool interlaced = (half_lines & 1) != 0;
    bool running = state->running;
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

    ImGui::TextColored(cyan, "DISPLAY"); ImGui::Separator();

    if (ImGui::BeginTable("##crtc_display", 4, flags))
    {
        setup_grid_columns(false);

        ImGui::TableNextRow();
        draw_grid_label("DOT CLOCK");
        ImGui::TextColored(orange, "%.3f MHz", clock);
        draw_grid_label("RUNNING");
        ImGui::TextColored(running ? green : gray, "%s", running ? "ON" : "OFF");

        ImGui::TableNextRow();
        draw_grid_label("LINE");
        ImGui::TextColored(white, "%u DOTS", line_clocks);
        draw_grid_label("LINE RATE");

        if (running)
            ImGui::TextColored(orange, "%.2f kHz", clock * 1000.0 / line_clocks);
        else
            ImGui::TextColored(gray, "--");

        ImGui::TableNextRow();
        draw_grid_label("FRAME");
        ImGui::TextColored(white, "%u HALF-LINES", half_lines);
        draw_grid_label("REFRESH");

        if (running)
            ImGui::TextColored(orange, "%.2f Hz", (clock * 1000000.0 * 2.0) / ((double)half_lines * line_clocks));
        else
            ImGui::TextColored(gray, "--");

        ImGui::TableNextRow();
        draw_grid_label("INTERLACE");
        ImGui::TextColored(interlaced ? green : gray, "%s", interlaced ? "ON" : "OFF");

        ImGui::TableNextRow();
        draw_grid_label("HSW1");
        ImGui::TextColored(white, "%u", crtc[0x00]);
        draw_grid_label("HSW2");
        ImGui::TextColored(white, "%u", crtc[0x01]);

        ImGui::TableNextRow();
        draw_grid_label("VST1");
        ImGui::TextColored(white, "%u", crtc[k_video_crtc_vst1]);
        draw_grid_label("VST2");
        ImGui::TextColored(white, "%u", crtc[k_video_crtc_vst2]);

        ImGui::TableNextRow();
        draw_grid_label("EET");
        ImGui::TextColored(white, "%u", crtc[0x07]);

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "RASTER"); ImGui::Separator();

    if (ImGui::BeginTable("##crtc_raster", 4, flags))
    {
        setup_grid_columns(false);

        u64 clocks = core->GetScheduler()->GetClocks();
        u8 status = running ? video->GetSyncStatus(clocks) : 0;

        ImGui::TableNextRow();
        draw_grid_label("LINE");

        if (running)
            ImGui::TextColored(white, "%u", video->GetBeamHalfLine(clocks) / 2);
        else
            ImGui::TextColored(gray, "--");

        draw_grid_label("DOT");

        if (running)
            ImGui::TextColored(white, "%u", video->GetBeamClock(clocks));
        else
            ImGui::TextColored(gray, "--");

        ImGui::TableNextRow();
        draw_grid_label("H STATE");

        if (running)
            ImGui::TextColored(blue, "%s", (status & 0x02) ? "SYNC" : (status & 0x30) ? "DISPLAY" : "BLANK");
        else
            ImGui::TextColored(gray, "--");

        draw_grid_label("V STATE");

        if (running)
            ImGui::TextColored(blue, "%s", (status & 0x04) ? "SYNC" : (status & 0xC0) ? "DISPLAY" : "BLANK");
        else
            ImGui::TextColored(gray, "--");

        ImGui::TableNextRow();
        draw_grid_label("FIELD");

        if (running)
            ImGui::TextColored(white, "%d", (status & 0x08) ? 1 : 0);
        else
            ImGui::TextColored(gray, "--");

        draw_grid_label("VSYNC IRQ");
        ImGui::TextColored(state->vsync_irq ? yellow : gray, "%-8s", state->vsync_irq ? "PENDING" : "CLEAR");
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextColored(gray, "(05CA)");

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "LAYERS"); ImGui::Separator();

    if (ImGui::BeginTable("##crtc_layers", 3, flags | ImGuiTableFlags_RowBg))
    {
        setup_grid_columns(true);
        ImGui::TableHeadersRow();

        static const char* rows[] = { "FORMAT", "H WINDOW", "V WINDOW", "VRAM START", "STRIDE", "HAJ", "FIELD OFFSET",
            "ZOOM X/Y", "VISIBLE SIZE" };

        for (int row = 0; row < (int)(sizeof(rows) / sizeof(rows[0])); row++)
        {
            ImGui::TableNextRow();
            draw_grid_label(rows[row]);
            draw_layer_column(0, row);
            ImGui::TableNextColumn();
            draw_layer_column(1, row);
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_crtc_registers(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(197, 239), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(536, 334), ImGuiCond_FirstUseEver);
    ImGui::Begin("CRTC Registers", &config_debug.show_crtc_registers);

    ImGui::PushFont(gui_default_font);

    Video* video = emu_get_core()->GetVideo();
    Video::Video_State* state = video->GetState();

    ImGui::TextColored(magenta, "INDEX"); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", state->crtc_index); ImGui::SameLine();
    ImGui::TextColored(gray, "(0440)");
    ImGui::Separator();

    if (ImGui::BeginTable("##crtc_registers", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_NoHostExtendX))
    {
        for (int row = 0; row < 16; row++)
        {
            ImGui::TableNextRow();

            for (int column = 0; column < 2; column++)
            {
                int index = column * 16 + row;
                char address[8];
                snprintf(address, sizeof(address), "R%02X", index);

                ImGui::TableNextColumn();
                ImGui::TextColored(cyan, "%s", address); ImGui::SameLine();
                ImGui::TextColored(violet, "%-4s", k_debug_crtc_register_names[index]);

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Edits go through 0440/0442 like a CPU write\n"
                        "Timing updates and the trace logger records them");

                ImGui::SameLine();
                EditableRegister16(NULL, NULL, (u16)index, state->crtc[index], crtc_write_callback, video,
                    EditableRegisterFlags_ShowBinary);
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_video_output(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(238, 106), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(240, 557), ImGuiCond_FirstUseEver);
    ImGui::Begin("Output Control", &config_debug.show_video_output);

    ImGui::PushFont(gui_default_font);

    GeartownsCore* core = emu_get_core();
    Video* video = core->GetVideo();
    Video::Video_State* state = video->GetState();
    Sprite* sprite = video->GetSprite();
    Emu_Debug_Buffer_Info layer0;
    Emu_Debug_Buffer_Info layer1;
    emu_debug_get_buffer_info(Emu_Debug_Buffer_Layer0, NULL, layer0);
    emu_debug_get_buffer_info(Emu_Debug_Buffer_Layer1, NULL, layer1);
    bool two_page = video->IsTwoPage();
    int palette = (state->output[1] >> 4) & 0x03;

    ImGui::TextColored(cyan, "OUTPUT CONTROLLER (0448/044A)"); ImGui::Separator();

    ImGui::TextColored(violet, "INDEX      "); ImGui::SameLine();
    ImGui::TextColored(white, "%d", state->output_index & 3);

    for (int i = 0; i < 4; i++)
    {
        ImGui::TextColored(cyan, "REG %d      ", i); ImGui::SameLine();
        ImGui::TextColored(white, "$%02X ", state->output[i]); ImGui::SameLine(0, 0);
        ImGui::TextColored(gray, "(" BYTE_TO_BINARY_PATTERN_SPACED ")", BYTE_TO_BINARY(state->output[i]));
    }

    ImGui::TextColored(violet, "MODE       "); ImGui::SameLine();
    ImGui::TextColored(blue, "%s", two_page ? "TWO PAGE   " : "SINGLE PAGE");
    ImGui::TextColored(violet, "LAYER 0    "); ImGui::SameLine();
    ImGui::TextColored(layer0.format ? white : gray, "%-10s", k_debug_layer_format_names[layer0.format & 3]);
    ImGui::TextColored(violet, "LAYER 1    "); ImGui::SameLine();
    ImGui::TextColored(layer1.format ? white : gray, "%-10s", k_debug_layer_format_names[layer1.format & 3]);
    ImGui::TextColored(violet, "FRONT      "); ImGui::SameLine();
    ImGui::TextColored(two_page ? white : gray, "LAYER %d", state->output[1] & 0x01);
    ImGui::TextColored(violet, "PALETTE    "); ImGui::SameLine();
    ImGui::TextColored(white, "%s", palette == 0 ? "LAYER 0 (16)" : palette == 2 ? "LAYER 1 (16)" : "256 COLORS  ");

    ImGui::NewLine(); ImGui::TextColored(cyan, "DISPLAY (FDA0)"); ImGui::Separator();

    bool enable0 = (state->display_enable & 0x0C) != 0;
    bool enable1 = two_page && (state->display_enable & 0x03) != 0;
    ImGui::TextColored(violet, "LAYER 0    "); ImGui::SameLine();
    ImGui::TextColored(enable0 ? green : gray, "%s", enable0 ? "ON " : "OFF"); ImGui::SameLine();
    ImGui::TextColored(violet, " LAYER 1"); ImGui::SameLine();
    ImGui::TextColored(enable1 ? green : gray, "%s", enable1 ? "ON " : "OFF");

    ImGui::NewLine(); ImGui::TextColored(cyan, "STATUS (044C)"); ImGui::Separator();

    ImGui::TextColored(violet, "DPMD       "); ImGui::SameLine();
    ImGui::TextColored(state->digital_palette_modified ? yellow : gray, "%s", state->digital_palette_modified ? "ON " : "OFF");
    ImGui::TextColored(violet, "SPRITE     "); ImGui::SameLine();
    ImGui::TextColored(sprite->IsBusy() ? yellow : gray, "%s", sprite->IsBusy() ? "BUSY" : "IDLE"); ImGui::SameLine();
    ImGui::TextColored(violet, " PAGE"); ImGui::SameLine();
    ImGui::TextColored(white, "%d", sprite->GetPage() ? 1 : 0);

    ImGui::NewLine(); ImGui::TextColored(cyan, "VRAM WRITE MASK (0458/045A)"); ImGui::Separator();

    ImGui::TextColored(violet, "INDEX      "); ImGui::SameLine();
    ImGui::TextColored(white, "%d", state->mask_index & 1); ImGui::SameLine();
    ImGui::TextColored(violet, " MASK"); ImGui::SameLine();
    ImGui::TextColored(white, "%02X %02X %02X %02X", state->mask[0], state->mask[1], state->mask[2], state->mask[3]);

    ImGui::NewLine(); ImGui::TextColored(cyan, "FM-R"); ImGui::Separator();

    ImGui::TextColored(violet, "LOW WINDOW "); ImGui::SameLine();
    ImGui::TextColored(blue, "%s", core->GetMemory()->GetState()->main_memory ? "MAIN RAM" : "FM-R    ");
    ImGui::TextColored(violet, "ACCESS MASK"); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", state->fmr_mask); ImGui::SameLine();
    ImGui::TextColored(gray, "READ %d WRITE %X", (state->fmr_mask >> 6) & 3, state->fmr_mask & 0x0F);
    ImGui::TextColored(violet, "DISPLAY    "); ImGui::SameLine();
    ImGui::TextColored(white, "PLANES %X PAGE %d", state->fmr_display_planes & 0x0F, state->fmr_display_page ? 1 : 0);
    ImGui::TextColored(violet, "ACCESS PAGE"); ImGui::SameLine();
    ImGui::TextColored(white, "%d", state->fmr_page ? 1 : 0); ImGui::SameLine();
    ImGui::TextColored(violet, " ANK"); ImGui::SameLine();
    ImGui::TextColored(state->fmr_ank ? green : gray, "%s", state->fmr_ank ? "ON " : "OFF");
    ImGui::TextColored(violet, "KANJI ROM  "); ImGui::SameLine();
    ImGui::TextColored(white, "JIS %02X%02X ROW %2d", state->kanji_high, state->kanji_low, state->kanji_row);
    ImGui::TextColored(violet, "TVRAM WRITE"); ImGui::SameLine();
    ImGui::TextColored(state->fmr_text_written ? yellow : gray, "%s", state->fmr_text_written ? "YES" : "NO ");

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_palettes(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(279, 173), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 491), ImGuiCond_FirstUseEver);
    ImGui::Begin("Palettes", &config_debug.show_palettes);

    ImGui::PushFont(gui_default_font);

    Video* video = emu_get_core()->GetVideo();
    Video::Video_State* state = video->GetState();
    int palette = (state->output[1] >> 4) & 0x03;
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

    ImGui::TextColored(violet, "INDEX"); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", state->palette_index); ImGui::SameLine();
    ImGui::TextColored(gray, "(FD90)"); ImGui::SameLine();
    ImGui::TextColored(violet, " SELECTED"); ImGui::SameLine();
    ImGui::TextColored(white, "%s", palette == 0 ? "LAYER 0" : palette == 2 ? "LAYER 1" : "256 COLORS");

    ImGui::PopFont();

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(2.0f, 2.0f));

    if (ImGui::BeginTabBar("##palette_tabs"))
    {
        if (ImGui::BeginTabItem("16 COLORS"))
        {
            ImGui::PushFont(gui_default_font);

            for (int bank = 0; bank < 2; bank++)
            {
                if (bank == 1)
                    ImGui::NewLine();

                draw_palette_title(bank == 0 ? "LAYER 0" : "LAYER 1",
                    video->GetLayerFormat(bank) == Video::VIDEO_LAYER_4BPP);

                if (ImGui::BeginTable(bank == 0 ? "##palette16_0" : "##palette16_1", 17, flags))
                {
                    const u8 (*colors)[3] = state->palette16[bank];
                    setup_palette_columns();
                    draw_palette_indices(16);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    draw_palette_swatches(colors, 0, true);

                    for (int component = 0; component < 3; component++)
                    {
                        ImVec4 tint = component == 0 ? blue : component == 1 ? red : green;
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextColored(violet, "%c", "BRG"[component]);

                        for (int i = 0; i < 16; i++)
                        {
                            ImGui::TableNextColumn();
                            draw_centered_digit(tint, colors[i][component] >> 4);
                        }
                    }

                    ImGui::EndTable();
                }
            }

            ImGui::PopFont();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("256 COLORS"))
        {
            ImGui::PushFont(gui_default_font);

            draw_palette_title("256 COLORS", video->GetLayerFormat(0) == Video::VIDEO_LAYER_8BPP);

            if (ImGui::BeginTable("##palette256", 17, flags))
            {
                setup_palette_columns();
                draw_palette_indices(16);

                for (int row = 0; row < 16; row++)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextColored(orange, "%02X", row * 16);
                    draw_palette_swatches(state->palette256, row * 16, false);
                }

                ImGui::EndTable();
            }

            ImGui::PopFont();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("DIGITAL"))
        {
            ImGui::PushFont(gui_default_font);

            bool modified = state->digital_palette_modified;
            ImGui::TextColored(cyan, "DIGITAL"); ImGui::SameLine();
            ImGui::TextColored(violet, "DPMD"); ImGui::SameLine();
            ImGui::TextColored(modified ? yellow : gray, "%s", modified ? "ON" : "OFF");
            ImGui::Separator();

            if (ImGui::BeginTable("##palette_digital", 17, flags))
            {
                float size = ImGui::GetFrameHeight();
                setup_palette_columns();
                draw_palette_indices(8);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                for (int i = 0; i < 8; i++)
                {
                    u8 value = state->digital_palette[i] & 0x0F;
                    float level = (value & 0x08) ? 1.0f : 0.5f;
                    ImVec4 color = ImVec4((value & 0x02) ? level : 0.0f, (value & 0x04) ? level : 0.0f,
                        (value & 0x01) ? level : 0.0f, 1.0f);

                    ImGui::TableNextColumn();
                    ImGui::PushID(i);
                    ImGui::ColorButton("##color", color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                        ImVec2(size, size));

                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("FD%02X $%X", 0x98 + i, value);

                    ImGui::PopID();
                }

                for (int bit = 3; bit >= 0; bit--)
                {
                    ImVec4 tint = bit == 3 ? white : bit == 2 ? green : bit == 1 ? red : blue;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(violet, "%c", "BRGI"[bit]);

                    for (int i = 0; i < 8; i++)
                    {
                        int set = (state->digital_palette[i] >> bit) & 0x01;
                        ImGui::TableNextColumn();
                        draw_centered_digit(set ? tint : (ImVec4)gray, set);
                    }
                }

                ImGui::EndTable();
            }

            ImGui::PopFont();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::PopStyleVar();

    ImGui::End();
    ImGui::PopStyleVar();
}

static void crtc_write_callback(u16 index, u16 value, void* user_data)
{
    Video* video = (Video*)user_data;
    u64 clocks = emu_get_core()->GetScheduler()->GetClocks();
    u8 saved = video->GetState()->crtc_index;

    video->Write(0x0440, (u8)index, clocks);
    video->Write(0x0442, (u8)value, clocks);
    video->Write(0x0443, (u8)(value >> 8), clocks);
    video->Write(0x0440, saved, clocks);
}

// The sections share the second label column and the right edge, so LAYER 1 starts under the second labels
// The layer labels are longer, which takes two characters from the LAYER 0 column
static void setup_grid_columns(bool layers)
{
    float character = ImGui::CalcTextSize("0").x;
    float spacing = ImGui::GetStyle().CellPadding.x * 2.0f;

    if (layers)
    {
        ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed, character * 12);
        ImGui::TableSetupColumn("LAYER 0", ImGuiTableColumnFlags_WidthFixed, character * 14);
        ImGui::TableSetupColumn("LAYER 1", ImGuiTableColumnFlags_WidthFixed, character * 23 + spacing);
    }
    else
    {
        ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed, character * 10);
        ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed, character * 16);
        ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed, character * 9);
        ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed, character * 14);
    }
}

static void draw_grid_label(const char* label)
{
    ImGui::TableNextColumn();
    ImGui::TextColored(violet, "%s", label);
    ImGui::TableNextColumn();
}

static void draw_layer_column(int layer, int row)
{
    Video* video = emu_get_core()->GetVideo();
    const u16* crtc = video->GetState()->crtc;
    Emu_Debug_Buffer_Info info;
    emu_debug_get_buffer_info(layer == 0 ? Emu_Debug_Buffer_Layer0 : Emu_Debug_Buffer_Layer1, NULL, info);
    bool active = layer == 0 || video->IsTwoPage();
    ImVec4 color = active ? white : gray;
    u32 hds = crtc[k_video_crtc_hds0 + layer * 2];
    u32 hde = crtc[k_video_crtc_hde0 + layer * 2];
    u32 vds = crtc[k_video_crtc_vds0 + layer * 2];
    u32 vde = crtc[k_video_crtc_vde0 + layer * 2];
    u32 zoom = (u32)crtc[k_video_crtc_zoom] >> (layer * 8);

    switch (row)
    {
        case 0:
            ImGui::TextColored(active && info.format ? blue : gray, "%s", k_debug_layer_format_names[info.format & 3]);
            break;
        case 1:
            ImGui::TextColored(color, "%u-%u", hds, hde); ImGui::SameLine();
            ImGui::TextColored(gray, "(%u)", hde > hds ? hde - hds : 0);
            break;
        case 2:
            ImGui::TextColored(color, "%u-%u", vds, vde); ImGui::SameLine();
            ImGui::TextColored(gray, "(%u)", vde > vds ? (vde - vds) / 2 : 0);
            break;
        case 3:
            ImGui::TextColored(active ? cyan : gray, "$%05X", info.page_base + info.start);

            if (active && ImGui::IsItemClicked())
                goto_vram(info.page_base + info.start, info.single_page);

            if (active && ImGui::IsItemHovered())
                ImGui::SetTooltip("FA%d $%04X", layer, crtc[k_video_crtc_fa0 + layer * 4]);

            break;
        case 4:
            ImGui::TextColored(color, "%u BYTES", info.stride);
            break;
        case 5:
            ImGui::TextColored(color, "%u", crtc[k_video_crtc_haj0 + layer * 4]);
            break;
        case 6:
            ImGui::TextColored(color, "$%04X", crtc[k_video_crtc_fo0 + layer * 4]);
            break;
        case 7:
            ImGui::TextColored(color, "%u x %u", (zoom & 0x0F) + 1, ((zoom >> 4) & 0x0F) + 1);
            break;
        default:
            if (info.window && active)
                ImGui::TextColored(white, "%d x %d", info.window_width, info.window_height);
            else
                ImGui::TextColored(gray, "--");

            break;
    }
}

static void draw_palette_title(const char* title, bool in_use)
{
    ImGui::TextColored(cyan, "%s", title); ImGui::SameLine();
    ImGui::TextColored(in_use ? green : gray, "%s", in_use ? "IN USE" : "UNUSED");
    ImGui::Separator();
}

static void setup_palette_columns(void)
{
    float size = ImGui::GetFrameHeight();

    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("00").x);

    for (int i = 0; i < 16; i++)
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, size);
}

static void draw_palette_indices(int count)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    for (int i = 0; i < count; i++)
    {
        ImGui::TableNextColumn();
        draw_centered_digit(orange, i);
    }
}

static void draw_palette_swatches(const u8 (*colors)[3], int first, bool nibbles)
{
    float size = ImGui::GetFrameHeight();

    for (int i = 0; i < 16; i++)
    {
        const u8* color = colors[first + i];
        ImVec4 value = ImVec4(color[1] / 255.0f, color[2] / 255.0f, color[0] / 255.0f, 1.0f);

        if (nibbles)
            value = ImVec4((color[1] | (color[1] >> 4)) / 255.0f, (color[2] | (color[2] >> 4)) / 255.0f,
                (color[0] | (color[0] >> 4)) / 255.0f, 1.0f);

        ImGui::TableNextColumn();
        ImGui::PushID(first + i);
        ImGui::ColorButton("##color", value, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
            ImVec2(size, size));

        if (ImGui::IsItemHovered())
            draw_color_tooltip(first + i, color, nibbles);

        ImGui::PopID();
    }
}

static void draw_centered_digit(const ImVec4& color, int value)
{
    char text[4];
    snprintf(text, sizeof(text), "%X", value);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text).x) * 0.5f);
    ImGui::TextColored(color, "%s", text);
}

static void draw_color_tooltip(int index, const u8* color, bool nibbles)
{
    u8 red = nibbles ? (u8)(color[1] | (color[1] >> 4)) : color[1];
    u8 green_value = nibbles ? (u8)(color[2] | (color[2] >> 4)) : color[2];
    u8 blue_value = nibbles ? (u8)(color[0] | (color[0] >> 4)) : color[0];

    ImGui::BeginTooltip();
    ImGui::TextColored(cyan, "INDEX $%02X", index);
    ImGui::Text("B $%02X  R $%02X  G $%02X", color[0], color[1], color[2]);
    ImGui::Text("RGB888 #%02X%02X%02X", red, green_value, blue_value);
    ImGui::EndTooltip();
}

static void goto_vram(u32 offset, bool single_page)
{
    GT_Debug_Memory_Address target = { };
    target.space = GT_DEBUG_MEMORY_REGION;
    target.region = single_page ? GT_DEBUG_REGION_VRAM_SINGLE_PAGE : GT_DEBUG_REGION_VRAM;
    target.address = offset;
    target.segment_register = -1;
    gui_debug_memory_goto(target);
}
