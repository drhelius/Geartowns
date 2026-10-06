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
static void draw_layer_column(int layer, int row);
static void draw_palette_row(const char* id, const u8 (*colors)[3], int first, int count, bool nibbles);
static void draw_color_tooltip(int index, const u8* color, bool nibbles);
static void goto_vram(u32 offset);

void gui_debug_window_crtc(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(70, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(440, 500), ImGuiCond_FirstUseEver);
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

    ImGui::TextColored(cyan, "DISPLAY"); ImGui::Separator();

    ImGui::TextColored(violet, "DOT CLOCK  "); ImGui::SameLine();
    ImGui::TextColored(orange, "%.3f MHz", clock); ImGui::SameLine();
    ImGui::TextColored(violet, " RUNNING"); ImGui::SameLine();
    ImGui::TextColored(state->running ? green : gray, "%s", state->running ? "ON " : "OFF");

    ImGui::TextColored(violet, "LINE       "); ImGui::SameLine();
    ImGui::TextColored(white, "%4u DOTS", line_clocks); ImGui::SameLine();
    ImGui::TextColored(orange, "%6.2f kHz", clock * 1000.0 / line_clocks);

    ImGui::TextColored(violet, "FRAME      "); ImGui::SameLine();
    ImGui::TextColored(white, "%4u HALF-LINES", half_lines); ImGui::SameLine();
    ImGui::TextColored(orange, "%5.2f Hz", (clock * 1000000.0 * 2.0) / ((double)half_lines * line_clocks));

    ImGui::TextColored(violet, "INTERLACE  "); ImGui::SameLine();
    ImGui::TextColored(interlaced ? green : gray, "%s", interlaced ? "ON " : "OFF");

    ImGui::TextColored(violet, "HSYNC      "); ImGui::SameLine();
    ImGui::TextColored(white, "HSW1 %4u  HSW2 %4u", crtc[0x00], crtc[0x01]);

    ImGui::TextColored(violet, "VSYNC      "); ImGui::SameLine();
    ImGui::TextColored(white, "VST1 %4u  VST2 %4u  EET %4u", crtc[k_video_crtc_vst1], crtc[k_video_crtc_vst2], crtc[0x07]);

    ImGui::NewLine(); ImGui::TextColored(cyan, "RASTER"); ImGui::Separator();

    u64 clocks = core->GetScheduler()->GetClocks();

    if (state->running)
    {
        u32 beam = video->GetBeamHalfLine(clocks);
        u8 status = video->GetSyncStatus(clocks);

        ImGui::TextColored(violet, "LINE       "); ImGui::SameLine();
        ImGui::TextColored(white, "%4u", beam / 2); ImGui::SameLine();
        ImGui::TextColored(violet, " DOT"); ImGui::SameLine();
        ImGui::TextColored(white, "%4u", video->GetBeamClock(clocks)); ImGui::SameLine();
        ImGui::TextColored(violet, " FIELD"); ImGui::SameLine();
        ImGui::TextColored(white, "%d", (status & 0x08) ? 1 : 0);

        const char* h_state = (status & 0x02) ? "SYNC   " : (status & 0x30) ? "DISPLAY" : "BLANK  ";
        const char* v_state = (status & 0x04) ? "SYNC   " : (status & 0xC0) ? "DISPLAY" : "BLANK  ";
        ImGui::TextColored(violet, "H STATE    "); ImGui::SameLine();
        ImGui::TextColored(blue, "%s", h_state); ImGui::SameLine();
        ImGui::TextColored(violet, " V STATE"); ImGui::SameLine();
        ImGui::TextColored(blue, "%s", v_state);
    }
    else
    {
        ImGui::TextColored(violet, "LINE       "); ImGui::SameLine();
        ImGui::TextColored(gray, "--  "); ImGui::SameLine();
        ImGui::TextColored(violet, " DOT"); ImGui::SameLine();
        ImGui::TextColored(gray, "--  "); ImGui::SameLine();
        ImGui::TextColored(violet, " FIELD"); ImGui::SameLine();
        ImGui::TextColored(gray, "-");
        ImGui::TextColored(violet, "H STATE    "); ImGui::SameLine();
        ImGui::TextColored(gray, "--     "); ImGui::SameLine();
        ImGui::TextColored(violet, " V STATE"); ImGui::SameLine();
        ImGui::TextColored(gray, "--     ");
    }

    ImGui::TextColored(violet, "VSYNC IRQ  "); ImGui::SameLine();
    ImGui::TextColored(state->vsync_irq ? yellow : gray, "%s", state->vsync_irq ? "PENDING" : "CLEAR  "); ImGui::SameLine();
    ImGui::TextColored(gray, "(05CA)");

    ImGui::NewLine(); ImGui::TextColored(cyan, "LAYERS"); ImGui::Separator();

    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

    if (ImGui::BeginTable("##crtc_layers", 3, flags))
    {
        ImGui::TableSetupColumn(" ");
        ImGui::TableSetupColumn("LAYER 0");
        ImGui::TableSetupColumn("LAYER 1");
        ImGui::TableHeadersRow();

        static const char* rows[] = { "FORMAT", "H WINDOW", "V WINDOW", "START", "STRIDE", "HAJ", "FIELD OFFSET", "ZOOM X/Y",
            "VISIBLE SIZE" };

        for (int row = 0; row < (int)(sizeof(rows) / sizeof(rows[0])); row++)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(violet, "%-12s", rows[row]);
            ImGui::TableNextColumn();
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
    ImGui::SetNextWindowPos(ImVec2(100, 80), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520, 330), ImGuiCond_FirstUseEver);
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
                ImGui::TextColored(violet, "%-4s", k_debug_crtc_register_names[index]); ImGui::SameLine();
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
    ImGui::SetNextWindowPos(ImVec2(130, 110), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 480), ImGuiCond_FirstUseEver);
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
    ImGui::SetNextWindowPos(ImVec2(160, 140), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(526, 420), ImGuiCond_FirstUseEver);
    ImGui::Begin("Palettes", &config_debug.show_palettes);

    ImGui::PushFont(gui_default_font);

    Video::Video_State* state = emu_get_core()->GetVideo()->GetState();
    int palette = (state->output[1] >> 4) & 0x03;

    ImGui::TextColored(violet, "INDEX"); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", state->palette_index); ImGui::SameLine();
    ImGui::TextColored(gray, "(FD90)"); ImGui::SameLine();
    ImGui::TextColored(violet, "  SELECTED"); ImGui::SameLine();
    ImGui::TextColored(white, "%s", palette == 0 ? "LAYER 0" : palette == 2 ? "LAYER 1" : "256 COLORS");

    ImGui::PopFont();

    if (ImGui::BeginTabBar("##palette_tabs"))
    {
        for (int bank = 0; bank < 2; bank++)
        {
            if (ImGui::BeginTabItem(bank == 0 ? "LAYER 0" : "LAYER 1"))
            {
                ImGui::PushFont(gui_default_font);
                ImGui::NewLine();
                draw_palette_row(bank == 0 ? "##pal0" : "##pal1", state->palette16[bank], 0, 16, true);
                ImGui::NewLine();

                if (ImGui::BeginTable("##palette16", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit |
                    ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_ScrollY, ImVec2(0, 0)))
                {
                    ImGui::TableSetupColumn("INDEX");
                    ImGui::TableSetupColumn("BLUE");
                    ImGui::TableSetupColumn("RED");
                    ImGui::TableSetupColumn("GREEN");
                    ImGui::TableHeadersRow();

                    for (int i = 0; i < 16; i++)
                    {
                        const u8* color = state->palette16[bank][i];
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextColored(orange, "%X", i);
                        ImGui::TableNextColumn();
                        ImGui::TextColored(blue, "%X", color[0] >> 4);
                        ImGui::TableNextColumn();
                        ImGui::TextColored(red, "%X", color[1] >> 4);
                        ImGui::TableNextColumn();
                        ImGui::TextColored(green, "%X", color[2] >> 4);
                    }

                    ImGui::EndTable();
                }

                ImGui::PopFont();
                ImGui::EndTabItem();
            }
        }

        if (ImGui::BeginTabItem("256 COLORS"))
        {
            ImGui::BeginChild("##palette256", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::PushFont(gui_default_font);
            ImGui::NewLine();

            for (int row = 0; row < 16; row++)
            {
                char id[16];
                snprintf(id, sizeof(id), "##p256_%d", row);
                ImGui::TextColored(white, "%02X:", row * 16); ImGui::SameLine();
                draw_palette_row(id, state->palette256, row * 16, 16, false);
            }

            ImGui::PopFont();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("DIGITAL"))
        {
            ImGui::PushFont(gui_default_font);
            ImGui::NewLine();

            for (int i = 0; i < 8; i++)
            {
                u8 value = state->digital_palette[i] & 0x0F;
                ImVec4 color = ImVec4((value & 0x02) ? (value & 0x08 ? 1.0f : 0.5f) : 0.0f,
                    (value & 0x04) ? (value & 0x08 ? 1.0f : 0.5f) : 0.0f, (value & 0x01) ? (value & 0x08 ? 1.0f : 0.5f) : 0.0f,
                    1.0f);
                char id[16];
                snprintf(id, sizeof(id), "##dpal_%d", i);

                ImGui::TextColored(cyan, "FD%02X", 0x98 + i); ImGui::SameLine();
                ImGui::ColorEdit3(id, (float*)&color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker); ImGui::SameLine();
                ImGui::TextColored(white, "$%X ", value); ImGui::SameLine(0, 0);
                ImGui::TextColored(gray, "(I%dG%dR%dB%d)", (value >> 3) & 1, (value >> 2) & 1, (value >> 1) & 1, value & 1);
            }

            ImGui::PopFont();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

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
            ImGui::TextColored(active && info.format ? blue : gray, "%-10s", k_debug_layer_format_names[info.format & 3]);
            break;
        case 1:
            ImGui::TextColored(color, "%4u-%4u %4u", hds, hde, hde > hds ? hde - hds : 0);
            break;
        case 2:
            ImGui::TextColored(color, "%4u-%4u %4u", vds, vde, vde > vds ? (vde - vds) / 2 : 0);
            break;
        case 3:
            ImGui::TextColored(active ? cyan : gray, "$%05X", info.start);

            if (active && ImGui::IsItemClicked())
                goto_vram(info.page_base + info.start);

            if (active && ImGui::IsItemHovered())
                ImGui::SetTooltip("FA%d $%04X", layer, crtc[k_video_crtc_fa0 + layer * 4]);

            break;
        case 4:
            ImGui::TextColored(color, "%5u BYTES", info.stride);
            break;
        case 5:
            ImGui::TextColored(color, "%4u", crtc[k_video_crtc_haj0 + layer * 4]);
            break;
        case 6:
            ImGui::TextColored(color, "$%04X", crtc[k_video_crtc_fo0 + layer * 4]);
            break;
        case 7:
            ImGui::TextColored(color, "%2u x %2u", (zoom & 0x0F) + 1, ((zoom >> 4) & 0x0F) + 1);
            break;
        default:
            if (info.window && active)
                ImGui::TextColored(white, "%4d x %4d", info.window_width, info.window_height);
            else
                ImGui::TextColored(gray, "--         ");

            break;
    }
}

static void draw_palette_row(const char* id, const u8 (*colors)[3], int first, int count, bool nibbles)
{
    for (int i = 0; i < count; i++)
    {
        const u8* color = colors[first + i];
        ImVec4 value = ImVec4(color[1] / 255.0f, color[2] / 255.0f, color[0] / 255.0f, 1.0f);

        if (nibbles)
            value = ImVec4((color[1] | (color[1] >> 4)) / 255.0f, (color[2] | (color[2] >> 4)) / 255.0f,
                (color[0] | (color[0] >> 4)) / 255.0f, 1.0f);

        char item_id[32];
        snprintf(item_id, sizeof(item_id), "%s_%d", id, i);
        ImGui::ColorEdit3(item_id, (float*)&value, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker |
            ImGuiColorEditFlags_NoTooltip);

        if (ImGui::IsItemHovered())
            draw_color_tooltip(first + i, color, nibbles);

        if (i != count - 1)
            ImGui::SameLine(0, 10);
    }
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

static void goto_vram(u32 offset)
{
    GT_Debug_Memory_Address target = { };
    target.space = GT_DEBUG_MEMORY_REGION;
    target.region = GT_DEBUG_REGION_VRAM;
    target.address = offset;
    target.segment_register = -1;
    gui_debug_memory_goto(target);
}
