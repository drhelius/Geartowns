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

static const float k_framebuffer_zoom[3] = { 0.5f, 1.0f, 2.0f };

static int selected_sprite = -1;

static void draw_buffer_header(const Emu_Debug_Buffer_Info& info);
static void draw_buffer_image(const Emu_Debug_Buffer_Info& info, bool custom);
static u32 buffer_offset(const Emu_Debug_Buffer_Info& info, bool custom, int x, int y);
static void draw_sprite_details(int index);
static void draw_sprite_position(int index);
static void draw_sprite_context_menu(int index);
static void goto_region(int region, u32 offset);
static bool sprite_passes_filter(const Emu_Debug_Sprite& sprite);

void gui_debug_window_framebuffers(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(80, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 520), ImGuiCond_FirstUseEver);
    ImGui::Begin("Framebuffers", &config_debug.show_framebuffers);

    if (ImGui::BeginTabBar("##framebuffer_tabs"))
    {
        static const char* tabs[] = { "LAYER 0", "LAYER 1", "SPRITES", "CUSTOM" };

        for (int tab = 0; tab < 4; tab++)
        {
            if (!ImGui::BeginTabItem(tabs[tab]))
                continue;

            ImGui::PushFont(gui_default_font);

            if (config_debug.framebuffer_tab != tab)
            {
                config_debug.framebuffer_tab = tab;
                emu_debug_update();
            }

            if (tab == 2)
            {
                ImGui::PushItemWidth(90.0f);
                ImGui::Combo("PAGE##sprite_page", &config_debug.framebuffer_sprite_page, "DISPLAY\0DRAW\0\0");
                ImGui::PopItemWidth();
                ImGui::SameLine();
            }
            else if (tab == 3)
            {
                ImGui::PushItemWidth(70.0f);
                ImGui::InputScalar("OFFSET##custom_offset", ImGuiDataType_S32, &config_debug.framebuffer_custom_offset, NULL,
                    NULL, "%05X", ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
                config_debug.framebuffer_custom_offset &= 0x7FFFF;
                ImGui::SameLine();
                ImGui::PushItemWidth(100.0f);
                ImGui::Combo("FORMAT##custom_format", &config_debug.framebuffer_custom_format,
                    "16 COLORS\0256 COLORS\0" "32K COLORS\0\0");
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::PushItemWidth(80.0f);
                ImGui::Combo("PALETTE##custom_palette", &config_debug.framebuffer_custom_palette, "LAYER 0\0LAYER 1\0" "256\0\0");
                ImGui::PopItemWidth();
                ImGui::InputInt("WIDTH##custom_width", &config_debug.framebuffer_custom_width, 8, 64);
                ImGui::SameLine();
                ImGui::InputInt("HEIGHT##custom_height", &config_debug.framebuffer_custom_height, 8, 64);
                config_debug.framebuffer_custom_width = CLAMP(config_debug.framebuffer_custom_width, 1, EMU_DEBUG_FRAMEBUFFER_WIDTH);
                config_debug.framebuffer_custom_height = CLAMP(config_debug.framebuffer_custom_height, 1,
                    EMU_DEBUG_FRAMEBUFFER_HEIGHT);
                ImGui::PopItemWidth();
            }

            ImGui::PushItemWidth(60.0f);
            ImGui::Combo("ZOOM##framebuffer_zoom", &config_debug.framebuffer_zoom, "0.5x\0" "1x\0" "2x\0\0");
            ImGui::PopItemWidth();

            if (tab < 2)
            {
                ImGui::SameLine();
                ImGui::Checkbox("Show Visible Area", &config_debug.framebuffer_show_window);
            }

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
    ImGui::SetNextWindowPos(ImVec2(110, 90), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("Sprites", &config_debug.show_sprites);

    ImGui::PushFont(gui_default_font);

    Video* video = emu_get_core()->GetVideo();
    Sprite* sprite = video->GetSprite();
    Sprite::Sprite_State* state = sprite->GetState();
    u16 first = (u16)(((state->registers[k_sprite_control1] & 0x03) << 8) | state->registers[k_sprite_control0]);
    int offset_x = ((state->registers[k_sprite_offset_x + 1] & 0x01) << 8) | state->registers[k_sprite_offset_x];
    int offset_y = ((state->registers[k_sprite_offset_y + 1] & 0x01) << 8) | state->registers[k_sprite_offset_y];

    ImGui::TextColored(violet, "ENABLED"); ImGui::SameLine();
    ImGui::TextColored(sprite->IsEnabled() ? green : gray, "%s", sprite->IsEnabled() ? "ON " : "OFF"); ImGui::SameLine();
    ImGui::TextColored(violet, " BUSY"); ImGui::SameLine();
    ImGui::TextColored(sprite->IsBusy() ? yellow : gray, "%s", sprite->IsBusy() ? "YES" : "NO "); ImGui::SameLine();
    ImGui::TextColored(violet, " DRAWN"); ImGui::SameLine();
    ImGui::TextColored(white, "%4u (FROM %4u)", k_sprite_entries - first, first);

    ImGui::TextColored(violet, "OFFSET "); ImGui::SameLine();
    ImGui::TextColored(white, "%3d,%3d", offset_x, offset_y); ImGui::SameLine();
    ImGui::TextColored(violet, " DISPLAY PAGE"); ImGui::SameLine();
    ImGui::TextColored(white, "%d", sprite->GetDisplayOffset() != 0 ? 1 : 0); ImGui::SameLine();
    ImGui::TextColored(violet, " "); ImGui::SameLine();
    ImGui::PushItemWidth(90.0f);
    ImGui::Combo("FILTER##sprite_filter", &config_debug.sprite_filter, "ALL\0DRAWN\0VISIBLE\0\0");
    ImGui::PopItemWidth();
    ImGui::Separator();

    if (ImGui::BeginTable("##sprites_layout", 2, ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("##grid", ImGuiTableColumnFlags_WidthFixed, 284.0f);
        ImGui::TableSetupColumn("##details", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::BeginChild("##sprite_grid", ImVec2(0, 0), ImGuiChildFlags_None);
        int hovered = -1;
        int column = 0;
        float cell = 32.0f;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        for (int i = 0; i < (int)k_sprite_entries; i++)
        {
            Emu_Debug_Sprite item;
            emu_debug_get_sprite(i, item);

            if (!sprite_passes_filter(item))
                continue;

            if (column > 0)
                ImGui::SameLine(0, 2);

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
                draw_sprite_details(display);
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

    ImGui::PopFont();

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

static void draw_buffer_header(const Emu_Debug_Buffer_Info& info)
{
    ImGui::TextColored(violet, "FORMAT"); ImGui::SameLine();
    ImGui::TextColored(info.format ? blue : gray, "%-10s", k_debug_layer_format_names[info.format & 3]); ImGui::SameLine();
    ImGui::TextColored(violet, " START"); ImGui::SameLine();
    ImGui::TextColored(cyan, "$%05X", (info.page_base + info.start) & 0x7FFFF);

    if (ImGui::IsItemClicked())
        goto_region(GT_DEBUG_REGION_VRAM, (info.page_base + info.start) & 0x7FFFF);

    ImGui::SameLine();
    ImGui::TextColored(violet, " STRIDE"); ImGui::SameLine();
    ImGui::TextColored(white, "%4u", info.stride); ImGui::SameLine();
    ImGui::TextColored(violet, " PAGE"); ImGui::SameLine();
    ImGui::TextColored(white, "$%05X %s", info.page_base, info.single_page ? "SINGLE" : "      "); ImGui::SameLine();
    ImGui::TextColored(violet, " SIZE"); ImGui::SameLine();
    ImGui::TextColored(white, "%4dx%-3d", info.width, info.height);
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
    {
        ImVec2 min(position.x + info.window_x * zoom, position.y + info.window_y * zoom);
        ImVec2 max(min.x + info.window_width * zoom, min.y + info.window_height * zoom);
        ImGui::GetWindowDrawList()->AddRect(min, max, ImColor(yellow), 0.0f, ImDrawFlags_None, 2.0f);
    }

    if (ImGui::IsItemHovered())
    {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        int x = CLAMP((int)((mouse.x - position.x) / zoom), 0, info.width - 1);
        int y = CLAMP((int)((mouse.y - position.y) / zoom), 0, info.height - 1);
        u32 offset = buffer_offset(info, custom, x, y);
        const u8* vram = emu_get_core()->GetVideo()->GetVRAM();
        u32 color = ((u32*)emu_debug_framebuffer)[y * EMU_DEBUG_FRAMEBUFFER_WIDTH + x];

        ImGui::BeginTooltip();
        ImGui::TextColored(cyan, "X %d  Y %d", x, y);
        ImGui::Text("VRAM $%05X", offset);

        if (info.format == Video::VIDEO_LAYER_4BPP)
            ImGui::Text("INDEX $%X", (x & 1) ? vram[offset] >> 4 : vram[offset] & 0x0F);
        else if (info.format == Video::VIDEO_LAYER_8BPP)
            ImGui::Text("INDEX $%02X", vram[offset]);
        else
            ImGui::Text("VALUE $%04X", vram[offset] | (vram[(offset + 1) & 0x7FFFF] << 8));

        ImGui::Text("RGB #%02X%02X%02X", color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF);
        ImGui::EndTooltip();

        if (ImGui::IsMouseClicked(0))
            goto_region(GT_DEBUG_REGION_VRAM, offset);
    }

    ImGui::EndChild();
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

static void draw_sprite_details(int index)
{
    Emu_Debug_Sprite sprite;
    bool valid = index >= 0;
    emu_debug_get_sprite(valid ? index : 0, sprite);

    if (!valid)
    {
        static const char* rows[] = { "ENTRY", "X,Y", "SCREEN X,Y", "PATTERN", "COLORS", "COLOR TABLE", "FLAGS", " ", " " };

        for (int i = 0; i < (int)(sizeof(rows) / sizeof(rows[0])); i++)
        {
            ImGui::TextColored(violet, "%-11s", rows[i]); ImGui::SameLine();
            ImGui::TextColored(gray, "--");
        }

        return;
    }

    ImGui::TextColored(violet, "ENTRY      "); ImGui::SameLine();
    ImGui::TextColored(white, "%4d", sprite.index); ImGui::SameLine();
    ImGui::TextColored(cyan, "$%04X", sprite.address);

    if (ImGui::IsItemClicked())
        goto_region(GT_DEBUG_REGION_SPRITE_RAM, sprite.address);

    ImGui::TextColored(violet, "X,Y        "); ImGui::SameLine();
    ImGui::TextColored(white, "$%04X,$%04X", sprite.x, sprite.y);
    ImGui::TextColored(violet, "SCREEN X,Y "); ImGui::SameLine();
    ImGui::TextColored(sprite.visible ? white : gray, "%3d,%3d", sprite.screen_x, sprite.screen_y);
    ImGui::TextColored(violet, "PATTERN    "); ImGui::SameLine();
    ImGui::TextColored(white, "$%03X", sprite.pattern); ImGui::SameLine();
    ImGui::TextColored(cyan, "$%05X", sprite.pattern_address);

    if (ImGui::IsItemClicked())
        goto_region(GT_DEBUG_REGION_SPRITE_RAM, sprite.pattern_address);

    ImGui::TextColored(violet, "COLORS     "); ImGui::SameLine();
    ImGui::TextColored(blue, "%s", sprite.table ? "16 (TABLE)" : "32K DIRECT");
    ImGui::TextColored(violet, "COLOR TABLE"); ImGui::SameLine();
    ImGui::TextColored(sprite.table ? white : gray, "$%03X", sprite.color_table); ImGui::SameLine();
    ImGui::TextColored(sprite.table ? cyan : gray, "$%05X", sprite.color_table_address);

    if (sprite.table && ImGui::IsItemClicked())
        goto_region(GT_DEBUG_REGION_SPRITE_RAM, sprite.color_table_address);

    ImGui::TextColored(violet, "FLAGS      "); ImGui::SameLine();
    ImGui::TextColored(sprite.offset ? green : gray, "OFFSET"); ImGui::SameLine();
    ImGui::TextColored(sprite.swap ? green : gray, "ROTATE"); ImGui::SameLine();
    ImGui::TextColored(sprite.flip_x ? green : gray, "FLIPX");
    ImGui::TextColored(violet, "           "); ImGui::SameLine();
    ImGui::TextColored(sprite.flip_y ? green : gray, "FLIPY "); ImGui::SameLine();
    ImGui::TextColored(sprite.half_x ? green : gray, "HALFX "); ImGui::SameLine();
    ImGui::TextColored(sprite.half_y ? green : gray, "HALFY");
    ImGui::TextColored(violet, "           "); ImGui::SameLine();
    ImGui::TextColored(sprite.table ? green : gray, "TABLE "); ImGui::SameLine();
    ImGui::TextColored(sprite.through ? green : gray, "THRU  "); ImGui::SameLine();
    ImGui::TextColored(sprite.hide ? yellow : gray, "HIDE");

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
    ImGui::PopFont();

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::Selectable("Save Sprite As..."))
            gui_file_dialog_save_sprite(index);

        if (ImGui::Selectable("Save All Sprites To Folder..."))
            gui_file_dialog_save_all_sprites();

        ImGui::EndPopup();
    }

    ImGui::PushFont(gui_default_font);
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
