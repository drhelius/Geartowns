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

#define GUI_DEBUG_FRAMEBUFFERS_IMPORT
#include "gui_debug_framebuffers.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imgui.h"
#include "geartowns.h"
#include "video/sprite.h"
#include "video/video.h"
#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../gui_colors.h"
#include "../gui_filedialogs.h"
#include "../ogl_renderer.h"
#include "../utils.h"
#include "gui_debug_constants.h"
#include "gui_debug_memory.h"

static const float k_framebuffer_zoom[3] = { 1.0f, 2.0f, 3.0f };
static const char k_framebuffer_zoom_items[] = "1x\0" "2x\0" "3x\0";
static const char k_framebuffer_page_items[] = "DISPLAY\0DRAW\0";
static const char k_framebuffer_format_items[] = "16 COLORS\0" "256 COLORS\0" "32K COLORS\0";
static const char k_framebuffer_palette_items[] = "LAYER 0\0LAYER 1\0" "256\0";
static const char k_sprite_filter_items[] = "ALL\0DRAWN\0VISIBLE\0";

static int selected_sprite = -1;

static void draw_buffer_controls(int tab);
static void draw_control_label(const char* label, bool spaced);
static float combo_width(const char* items);
static void draw_buffer_header(const Emu_Debug_Buffer_Info& info);
static void draw_info_label(const char* label);
static void draw_buffer_image(const Emu_Debug_Buffer_Info& info, bool custom);
static void draw_visible_area(const Emu_Debug_Buffer_Info& info, ImVec2 position, float zoom);
static void draw_visible_rect(ImDrawList* draw_list, ImVec2 position, float zoom, int x, int y, int width, int height);
static u32 buffer_offset(const Emu_Debug_Buffer_Info& info, bool custom, int x, int y);
static void draw_sprite_header(void);
static void draw_sprite_details(int index);
static void draw_sprite_position(int index);
static void draw_sprite_context_menu(int index);
static void goto_region(int region, u32 offset);
static bool sprite_passes_filter(const Emu_Debug_Sprite& sprite);

void gui_debug_window_framebuffers(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(120, 240), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(548, 483), ImGuiCond_FirstUseEver);
    ImGui::Begin("Framebuffers", &config_debug.show_framebuffers);

    if (ImGui::BeginTabBar("##framebuffer_tabs"))
    {
        static const char* tabs[] = { "LAYER 0", "LAYER 1", "SPRITES", "CUSTOM" };

        for (int tab = 0; tab < 4; tab++)
        {
            if (!ImGui::BeginTabItem(tabs[tab]))
                continue;

            if (config_debug.framebuffer_tab != tab)
            {
                config_debug.framebuffer_tab = tab;
                emu_debug_update();
            }

            draw_buffer_controls(tab);
            ImGui::Separator();

            ImGui::PushFont(gui_default_font);
            draw_buffer_header(emu_debug_framebuffer_info);
            draw_buffer_image(emu_debug_framebuffer_info, tab == 3);
            ImGui::PopFont();

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_sprites(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(161, 107), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(583, 418), ImGuiCond_FirstUseEver);
    ImGui::Begin("Sprites", &config_debug.show_sprites);

    ImGui::PushFont(gui_default_font);
    draw_sprite_header();
    ImGui::PopFont();
    ImGui::Separator();

    float cell = 32.0f;
    float spacing = 2.0f;
    float grid_width = cell * 8 + spacing * 7 + ImGui::GetStyle().ScrollbarSize;

    if (ImGui::BeginTable("##sprites_layout", 2, ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("##grid", ImGuiTableColumnFlags_WidthFixed, grid_width);
        ImGui::TableSetupColumn("##details", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Filter");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(combo_width(k_sprite_filter_items));
        ImGui::Combo("##sprite_filter", &config_debug.sprite_filter, k_sprite_filter_items);

        ImGui::BeginChild("##sprite_grid", ImVec2(0, 0), ImGuiChildFlags_None);
        int hovered = -1;
        int column = 0;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        for (int i = 0; i < (int)k_sprite_entries; i++)
        {
            Emu_Debug_Sprite item;
            emu_debug_get_sprite(i, item);

            if (!sprite_passes_filter(item))
                continue;

            if (column > 0)
                ImGui::SameLine(0, spacing);

            ImVec2 position = ImGui::GetCursorScreenPos();
            float u = (float)((i & 31) * 16) / EMU_DEBUG_SPRITE_ATLAS_SIZE;
            float v = (float)((i >> 5) * 16) / EMU_DEBUG_SPRITE_ATLAS_SIZE;
            float size = 16.0f / EMU_DEBUG_SPRITE_ATLAS_SIZE;
            ImVec4 tint = item.drawn && !item.hide ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.4f, 0.4f, 0.4f, 1.0f);

            ImGui::PushID(i);
            ImGui::ImageWithBg((ImTextureID)(intptr_t)ogl_renderer_emu_debug_sprite_atlas, ImVec2(cell, cell), ImVec2(u, v),
                ImVec2(u + size, v + size), ImVec4(0, 0, 0, 0), tint);

            if (ImGui::IsItemHovered())
            {
                hovered = i;

                if (ImGui::IsMouseClicked(0))
                    selected_sprite = selected_sprite == i ? -1 : i;

                if (selected_sprite != i)
                    draw_list->AddRect(position, ImVec2(position.x + cell, position.y + cell), ImColor(cyan), 2.0f,
                        ImDrawFlags_RoundCornersAll, 2.0f);
            }

            if (selected_sprite == i)
            {
                float t = (float)(0.5 + 0.5 * sin(ImGui::GetTime() * 4.0));
                draw_list->AddRect(position, ImVec2(position.x + cell, position.y + cell),
                    ImColor(gui_lerp_color(red, white, t)), 2.0f, ImDrawFlags_RoundCornersAll, 2.0f);
            }

            draw_sprite_context_menu(i);
            ImGui::PopID();
            column = (column + 1) % 8;
        }

        ImGui::EndChild();
        ImGui::TableNextColumn();

        int display = hovered >= 0 ? hovered : selected_sprite;

        if (ImGui::BeginTabBar("##sprite_detail_tabs"))
        {
            if (ImGui::BeginTabItem("DETAILS"))
            {
                ImGui::PushFont(gui_default_font);
                draw_sprite_details(display);
                ImGui::PopFont();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("POSITION"))
            {
                draw_sprite_position(display);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::EndTable();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

bool gui_debug_save_sprite(const char* file_path, int index)
{
    unsigned char* png = NULL;
    int size = emu_get_sprite_png(index, 1, &png);

    if (size <= 0 || !IsValidPointer(png))
        return false;

    FILE* file = fopen_utf8(file_path, "wb");
    bool ok = IsValidPointer(file) && fwrite(png, 1, size, file) == (size_t)size;

    if (IsValidPointer(file))
        fclose(file);

    free(png);
    return ok;
}

bool gui_debug_save_all_sprites(const char* folder)
{
    bool ok = true;

    for (int i = 0; i < (int)k_sprite_entries; i++)
    {
        char name[32];
        snprintf(name, sizeof(name), "sprite_%04d.png", i);
        std::string path = folder;
        append_path_component(path, name);
        ok = gui_debug_save_sprite(path.c_str(), i) && ok;
    }

    return ok;
}

static void draw_buffer_controls(int tab)
{
    ImGuiStyle& style = ImGui::GetStyle();
    int columns = tab < 2 ? 3 : tab == 2 ? 4 : 6;

    if (!ImGui::BeginTable("##framebuffer_controls", columns, ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_NoHostExtendX))
        return;

    ImGui::TableNextRow();
    draw_control_label("Zoom", false);
    ImGui::SetNextItemWidth(combo_width(k_framebuffer_zoom_items));
    ImGui::Combo("##framebuffer_zoom", &config_debug.framebuffer_zoom, k_framebuffer_zoom_items);

    if (tab < 2)
    {
        ImGui::TableNextColumn();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + style.ItemSpacing.x);
        ImGui::Checkbox("Show Visible Area", &config_debug.framebuffer_show_window);
    }
    else if (tab == 2)
    {
        draw_control_label("Page", true);
        ImGui::SetNextItemWidth(combo_width(k_framebuffer_page_items));
        ImGui::Combo("##sprite_page", &config_debug.framebuffer_sprite_page, k_framebuffer_page_items);
    }
    else
    {
        float number = ImGui::CalcTextSize("0000").x + style.FramePadding.x * 2.0f;
        float steps = (ImGui::GetFrameHeight() + style.ItemInnerSpacing.x) * 2.0f;

        draw_control_label("Format", true);
        ImGui::SetNextItemWidth(combo_width(k_framebuffer_format_items));
        ImGui::Combo("##custom_format", &config_debug.framebuffer_custom_format, k_framebuffer_format_items);

        ImGui::BeginDisabled(config_debug.framebuffer_custom_format != 0);
        draw_control_label("Palette", true);
        ImGui::SetNextItemWidth(combo_width(k_framebuffer_palette_items));
        ImGui::Combo("##custom_palette", &config_debug.framebuffer_custom_palette, k_framebuffer_palette_items);
        ImGui::EndDisabled();

        ImGui::TableNextRow();
        draw_control_label("Offset", false);
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("00000").x + style.FramePadding.x * 2.0f);
        ImGui::InputScalar("##custom_offset", ImGuiDataType_S32, &config_debug.framebuffer_custom_offset, NULL, NULL,
            "%05X", ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
        config_debug.framebuffer_custom_offset &= 0x7FFFF;

        draw_control_label("Width", true);
        ImGui::SetNextItemWidth(number + steps);
        ImGui::InputInt("##custom_width", &config_debug.framebuffer_custom_width, 8, 64);
        config_debug.framebuffer_custom_width = CLAMP(config_debug.framebuffer_custom_width, 1, EMU_DEBUG_FRAMEBUFFER_WIDTH);

        draw_control_label("Height", true);
        ImGui::SetNextItemWidth(number + steps);
        ImGui::InputInt("##custom_height", &config_debug.framebuffer_custom_height, 8, 64);
        config_debug.framebuffer_custom_height = CLAMP(config_debug.framebuffer_custom_height, 1,
            EMU_DEBUG_FRAMEBUFFER_HEIGHT);
    }

    ImGui::EndTable();
}

static void draw_control_label(const char* label, bool spaced)
{
    ImGui::TableNextColumn();

    if (spaced)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
}

static float combo_width(const char* items)
{
    float width = 0.0f;

    for (const char* item = items; *item; item += strlen(item) + 1)
        width = MAX(width, ImGui::CalcTextSize(item).x);

    return width + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight();
}

static void draw_buffer_header(const Emu_Debug_Buffer_Info& info)
{
    if (!ImGui::BeginTable("##framebuffer_info", 6, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX))
        return;

    float character = ImGui::CalcTextSize("0").x;
    u32 start = (info.page_base + info.start) & 0x7FFFF;

    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 6);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 14);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 5);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 9);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 7);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 10);

    ImGui::TableNextRow();
    draw_info_label("FORMAT");
    ImGui::TextColored(info.format ? blue : gray, "%s", k_debug_layer_format_names[info.format & 3]);
    draw_info_label("START");
    ImGui::TextColored(cyan, "$%05X", start);

    if (ImGui::IsItemClicked())
        goto_region(info.single_page ? GT_DEBUG_REGION_VRAM_SINGLE_PAGE : GT_DEBUG_REGION_VRAM, start);

    draw_info_label("STRIDE");
    ImGui::TextColored(white, "%u BYTES", info.stride);

    ImGui::TableNextRow();
    draw_info_label("PAGE");

    if (info.single_page)
        ImGui::TextColored(blue, "SINGLE PAGE");
    else
        ImGui::TextColored(white, "$%05X-$%05X", info.page_base, info.page_base + info.page_size - 1);

    draw_info_label("SIZE");

    if (info.width > 0 && info.height > 0)
        ImGui::TextColored(white, "%dx%d", info.width, info.height);
    else
        ImGui::TextColored(gray, "--");

    draw_info_label("VISIBLE");

    if (info.window)
        ImGui::TextColored(yellow, "%dx%d", info.window_width, info.window_height);
    else
        ImGui::TextColored(gray, "--");

    ImGui::EndTable();
}

static void draw_info_label(const char* label)
{
    ImGui::TableNextColumn();
    ImGui::TextColored(violet, "%s", label);
    ImGui::TableNextColumn();
}

static void draw_buffer_image(const Emu_Debug_Buffer_Info& info, bool custom)
{
    ImGui::BeginChild("##framebuffer_image", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);

    if (info.width <= 0 || info.height <= 0)
    {
        ImGui::TextColored(gray, "LAYER OFF");
        ImGui::EndChild();
        return;
    }

    float zoom = k_framebuffer_zoom[CLAMP(config_debug.framebuffer_zoom, 0, 2)];
    ImVec2 position = ImGui::GetCursorScreenPos();
    ImVec2 size((float)info.width * zoom, (float)info.height * zoom);

    ImGui::Image((ImTextureID)(intptr_t)ogl_renderer_emu_debug_framebuffer, size, ImVec2(0, 0),
        ImVec2((float)info.width / EMU_DEBUG_FRAMEBUFFER_WIDTH, (float)info.height / EMU_DEBUG_FRAMEBUFFER_HEIGHT));

    if (info.window && config_debug.framebuffer_show_window && config_debug.framebuffer_tab < 2)
        draw_visible_area(info, position, zoom);

    if (ImGui::IsItemHovered())
    {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        int x = CLAMP((int)((mouse.x - position.x) / zoom), 0, info.width - 1);
        int y = CLAMP((int)((mouse.y - position.y) / zoom), 0, info.height - 1);
        u32 offset = buffer_offset(info, custom, x, y);
        const u8* vram = emu_get_core()->GetVideo()->GetVRAM();
        u32 color = ((u32*)emu_debug_framebuffer)[y * EMU_DEBUG_FRAMEBUFFER_WIDTH + x];

        float swatch = ImGui::GetTextLineHeight();
        ImVec4 pixel = ImVec4((color & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f, ((color >> 16) & 0xFF) / 255.0f,
            1.0f);

        ImGui::BeginTooltip();
        ImGui::TextColored(violet, "X,Y  "); ImGui::SameLine();
        ImGui::TextColored(white, "%d,%d", x, y);
        ImGui::TextColored(violet, "VRAM "); ImGui::SameLine();
        ImGui::TextColored(cyan, "$%05X", offset);

        if (info.format == Video::VIDEO_LAYER_4BPP)
        {
            ImGui::TextColored(violet, "INDEX"); ImGui::SameLine();
            ImGui::TextColored(white, "$%X", (x & 1) ? vram[offset] >> 4 : vram[offset] & 0x0F);
        }
        else if (info.format == Video::VIDEO_LAYER_8BPP)
        {
            ImGui::TextColored(violet, "INDEX"); ImGui::SameLine();
            ImGui::TextColored(white, "$%02X", vram[offset]);
        }
        else
        {
            ImGui::TextColored(violet, "VALUE"); ImGui::SameLine();
            ImGui::TextColored(white, "$%04X", vram[offset] | (vram[(offset + 1) & 0x7FFFF] << 8));
        }

        ImGui::TextColored(violet, "COLOR"); ImGui::SameLine();
        ImGui::ColorButton("##pixel", pixel, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
            ImVec2(swatch, swatch)); ImGui::SameLine();
        ImGui::TextColored(white, "#%02X%02X%02X", color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF);
        ImGui::EndTooltip();

        if (ImGui::IsMouseClicked(0))
            goto_region(GT_DEBUG_REGION_VRAM, offset);
    }

    ImGui::EndChild();
}

static void draw_visible_area(const Emu_Debug_Buffer_Info& info, ImVec2 position, float zoom)
{
    int bits = info.format == Video::VIDEO_LAYER_4BPP ? 4 : info.format == Video::VIDEO_LAYER_8BPP ? 8 : 16;
    int row_width = (int)(info.stride * 8 / bits);
    int rows = (int)(info.page_size / info.stride);

    if (row_width <= 0 || rows <= 0)
        return;

    int first = info.window_y * row_width + info.window_x;
    int height = MIN(info.window_height, rows);
    int segments[2][2] = { { first, first + info.window_width }, { 0, 0 } };

    if (info.window_wrap > 0)
    {
        int block = info.window_wrap;
        int base = first - first % block;
        int end = first + MIN(info.window_width, block);
        segments[0][1] = MIN(end, base + block);
        segments[1][0] = base;
        segments[1][1] = MAX(end - block, base);
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRect(position, ImVec2(position.x + info.width * zoom, position.y + info.height * zoom), true);

    for (int i = 0; i < 2; i++)
    {
        int start = segments[i][0];

        while (start < segments[i][1])
        {
            int column = start % row_width;
            int span = MIN(segments[i][1] - start, row_width - column);
            int top = (start / row_width) % rows;

            draw_visible_rect(draw_list, position, zoom, column, top, span, MIN(height, rows - top));

            if (top + height > rows)
                draw_visible_rect(draw_list, position, zoom, column, 0, span, top + height - rows);

            start += span;
        }
    }

    draw_list->PopClipRect();
}

static void draw_visible_rect(ImDrawList* draw_list, ImVec2 position, float zoom, int x, int y, int width, int height)
{
    ImVec2 min(position.x + x * zoom, position.y + y * zoom);
    ImVec2 max(min.x + width * zoom, min.y + height * zoom);
    draw_list->AddRect(min, max, ImColor(yellow), 0.0f, ImDrawFlags_None, 2.0f);
}

static u32 buffer_offset(const Emu_Debug_Buffer_Info& info, bool custom, int x, int y)
{
    u32 offset = (custom ? info.start : 0) + (u32)y * info.stride;

    if (info.format == Video::VIDEO_LAYER_4BPP)
        offset += (u32)x >> 1;
    else if (info.format == Video::VIDEO_LAYER_8BPP)
        offset += (u32)x;
    else
        offset += (u32)x * 2;

    offset &= info.page_size - 1;

    if (info.single_page)
        return ((offset & 0x00004) << 16) | ((offset & 0x7FFF8) >> 1) | (offset & 0x00003);

    return info.page_base + offset;
}

static void draw_sprite_header(void)
{
    Sprite* sprite = emu_get_core()->GetVideo()->GetSprite();
    Sprite::Sprite_State* state = sprite->GetState();
    u16 first = (u16)(((state->registers[k_sprite_control1] & 0x03) << 8) | state->registers[k_sprite_control0]);
    int offset_x = ((state->registers[k_sprite_offset_x + 1] & 0x01) << 8) | state->registers[k_sprite_offset_x];
    int offset_y = ((state->registers[k_sprite_offset_y + 1] & 0x01) << 8) | state->registers[k_sprite_offset_y];
    bool enabled = sprite->IsEnabled();
    bool busy = sprite->IsBusy();

    if (!ImGui::BeginTable("##sprite_info", 6, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX))
        return;

    float character = ImGui::CalcTextSize("0").x;

    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 7);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 4);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 12);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 2);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 6);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 16);

    ImGui::TableNextRow();
    draw_info_label("ENABLED");
    ImGui::TextColored(enabled ? green : gray, "%s", enabled ? "ON" : "OFF");
    draw_info_label("DISPLAY PAGE");
    ImGui::TextColored(white, "%d", sprite->GetDisplayOffset() != 0 ? 1 : 0);
    draw_info_label("DRAWN");
    ImGui::TextColored(white, "%u", k_sprite_entries - first); ImGui::SameLine();
    ImGui::TextColored(gray, "(FROM %u)", first);

    ImGui::TableNextRow();
    draw_info_label("BUSY");
    ImGui::TextColored(busy ? yellow : gray, "%s", busy ? "YES" : "NO");
    draw_info_label("DRAW PAGE");
    ImGui::TextColored(white, "%d", sprite->GetPage() ? 1 : 0);
    draw_info_label("OFFSET");
    ImGui::TextColored(white, "%d,%d", offset_x, offset_y);

    ImGui::EndTable();
}

static void draw_sprite_details(int index)
{
    static const char* rows[] = { "ENTRY", "X,Y", "SCREEN X,Y", "PATTERN", "COLORS", "COLOR TABLE", "FLAGS", "", "" };
    Emu_Debug_Sprite sprite;
    bool valid = index >= 0;
    emu_debug_get_sprite(valid ? index : 0, sprite);

    if (!ImGui::BeginTable("##sprite_details", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX))
        return;

    float character = ImGui::CalcTextSize("0").x;

    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 11);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 19);

    for (int row = 0; row < (int)(sizeof(rows) / sizeof(rows[0])); row++)
    {
        ImGui::TableNextRow();
        draw_info_label(rows[row]);

        if (!valid)
        {
            if (row < 7)
                ImGui::TextColored(gray, "--");

            continue;
        }

        switch (row)
        {
            case 0:
                ImGui::TextColored(white, "%-4d", sprite.index); ImGui::SameLine();
                ImGui::TextColored(cyan, "$%05X", sprite.address);

                if (ImGui::IsItemClicked())
                    goto_region(GT_DEBUG_REGION_SPRITE_RAM, sprite.address);

                break;
            case 1:
                ImGui::TextColored(white, "$%04X,$%04X", sprite.x, sprite.y);
                break;
            case 2:
                ImGui::TextColored(sprite.visible ? white : gray, "%d,%d", sprite.screen_x, sprite.screen_y);
                break;
            case 3:
                ImGui::TextColored(white, "$%03X", sprite.pattern); ImGui::SameLine();
                ImGui::TextColored(cyan, "$%05X", sprite.pattern_address);

                if (ImGui::IsItemClicked())
                    goto_region(GT_DEBUG_REGION_SPRITE_RAM, sprite.pattern_address);

                break;
            case 4:
                ImGui::TextColored(blue, "%s", sprite.table ? "16 (TABLE)" : "32K DIRECT");
                break;
            case 5:
                ImGui::TextColored(sprite.table ? white : gray, "$%03X", sprite.color_table); ImGui::SameLine();
                ImGui::TextColored(sprite.table ? cyan : gray, "$%05X", sprite.color_table_address);

                if (sprite.table && ImGui::IsItemClicked())
                    goto_region(GT_DEBUG_REGION_SPRITE_RAM, sprite.color_table_address);

                break;
            case 6:
                ImGui::TextColored(sprite.offset ? green : gray, "%-7s", "OFFSET"); ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextColored(sprite.swap ? green : gray, "%-7s", "ROTATE"); ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextColored(sprite.flip_x ? green : gray, "FLIPX");
                break;
            case 7:
                ImGui::TextColored(sprite.flip_y ? green : gray, "%-7s", "FLIPY"); ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextColored(sprite.half_x ? green : gray, "%-7s", "HALFX"); ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextColored(sprite.half_y ? green : gray, "HALFY");
                break;
            default:
                ImGui::TextColored(sprite.table ? green : gray, "%-7s", "TABLE"); ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextColored(sprite.through ? green : gray, "%-7s", "THRU"); ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextColored(sprite.hide ? yellow : gray, "HIDE");
                break;
        }
    }

    ImGui::EndTable();

    if (!valid)
        return;

    float u = (float)((index & 31) * 16) / EMU_DEBUG_SPRITE_ATLAS_SIZE;
    float v = (float)((index >> 5) * 16) / EMU_DEBUG_SPRITE_ATLAS_SIZE;
    float size = 16.0f / EMU_DEBUG_SPRITE_ATLAS_SIZE;
    ImGui::NewLine();
    ImGui::Image((ImTextureID)(intptr_t)ogl_renderer_emu_debug_sprite_atlas, ImVec2(128, 128), ImVec2(u, v),
        ImVec2(u + size, v + size));
}

static void draw_sprite_position(int index)
{
    ImVec2 position = ImGui::GetCursorScreenPos();
    ImGui::Image((ImTextureID)(intptr_t)ogl_renderer_emu_debug_sprite_page, ImVec2(256, 256));

    if (index < 0)
        return;

    Emu_Debug_Sprite sprite;
    emu_debug_get_sprite(index, sprite);

    float x = (float)(sprite.screen_x >= 256 ? sprite.screen_x - 512 : sprite.screen_x);
    float y = (float)(sprite.screen_y >= 256 ? sprite.screen_y - 512 : sprite.screen_y);
    float width = sprite.half_x ? 8.0f : 16.0f;
    float height = sprite.half_y ? 8.0f : 16.0f;
    float t = (float)(0.5 + 0.5 * sin(ImGui::GetTime() * 4.0));
    ImGui::GetWindowDrawList()->AddRect(ImVec2(position.x + x, position.y + y),
        ImVec2(position.x + x + width, position.y + y + height), ImColor(gui_lerp_color(red, white, t)), 0.0f,
        ImDrawFlags_None, 2.0f);
}

static void draw_sprite_context_menu(int index)
{
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::Selectable("Save Sprite As..."))
            gui_file_dialog_save_sprite(index);

        if (ImGui::Selectable("Save All Sprites To Folder..."))
            gui_file_dialog_save_all_sprites();

        ImGui::EndPopup();
    }
}

static void goto_region(int region, u32 offset)
{
    GT_Debug_Memory_Address target = { };
    target.space = GT_DEBUG_MEMORY_REGION;
    target.region = region;
    target.address = offset;
    target.segment_register = -1;
    gui_debug_memory_goto(target);
}

static bool sprite_passes_filter(const Emu_Debug_Sprite& sprite)
{
    if (config_debug.sprite_filter == 1)
        return sprite.drawn;

    if (config_debug.sprite_filter == 2)
        return sprite.visible;

    return true;
}
