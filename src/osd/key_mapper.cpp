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

#include "key_mapper.h"
#include "key_mapper_layout.h"
#include "../input/host_keys.h"

static const u8 k_color_background = 0;
static const u8 k_color_title = 1;
static const u8 k_color_key = 2;
static const u8 k_color_border = 3;
static const u8 k_color_text = 4;
static const u8 k_color_dim = 5;
static const u8 k_color_remapped = 6;
static const u8 k_color_unreachable = 7;
static const u8 k_color_selected = 8;
static const u8 k_color_pressed = 9;
static const u8 k_color_hover = 10;
static const u8 k_color_dark = 11;
static const u8 k_color_button = 12;
static const u8 k_color_warning = 13;
static const u8 k_color_game = 14;
static const u8 k_color_black = 15;
static const u8 k_color_global = 16;

static const u8 k_key_flag_reachable = 0x01;
static const u8 k_key_flag_remapped = 0x02;
static const u8 k_key_flag_pressed = 0x04;

static const int k_margin = 16;
static const int k_title_height = 22;
static const int k_line_height = 20;
static const int k_chip_height = 18;
static const int k_chip_close_width = 16;
static const int k_button_width = 104;
static const int k_button_height = 22;

static bool is_inside(int px, int py, int x, int y, int width, int height)
{
    return (px >= x) && (py >= y) && (px < x + width) && (py < y + height);
}

static std::string level_prefix(int level)
{
    static const char* const k_prefixes[GT_KEY_MAP_LEVELS] = { "", "Shift+", GT_HOST_LABEL_LEVEL "+",
        GT_HOST_LABEL_LEVEL "+Shift+" };
    return k_prefixes[level];
}

static bool drives(const GT_Key_Map_Entry& entry, GT_Keys key)
{
    for (int i = 0; i < entry.count; i++)
    {
        if (entry.keys[i] == (u8)key)
            return true;
    }

    return false;
}

KeyMapper::KeyMapper()
{
    InitPointer(m_key_map);
    m_open = false;
    m_mapper_hotkey = GT_HOST_HOTKEY_LABEL("F1");
    m_mapping_hotkey = GT_HOST_HOTKEY_LABEL("F2");
    m_pointer_x = k_key_mapper_width / 2;
    m_pointer_y = k_key_mapper_height / 2;
    m_buttons = 0;
    m_selected = GT_KEY_NONE;
    memset(m_held, 0, sizeof(m_held));
    m_held_count = 0;
    m_tap_usage = 0;
    m_tap_valid = false;
    m_query_usage = 0;
    m_reset_armed = false;
    memset(m_key_flags, 0, sizeof(m_key_flags));
    memset(m_chips, 0, sizeof(m_chips));
    m_chip_count = 0;
}

void KeyMapper::Init(KeyMap* key_map)
{
    m_key_map = key_map;
    m_canvas.Init(k_key_mapper_width, k_key_mapper_height);
    m_canvas.SetColor(k_color_background, 22, 26, 38);
    m_canvas.SetColor(k_color_title, 10, 12, 20);
    m_canvas.SetColor(k_color_key, 84, 90, 108);
    m_canvas.SetColor(k_color_border, 8, 8, 12);
    m_canvas.SetColor(k_color_text, 236, 236, 240);
    m_canvas.SetColor(k_color_dim, 140, 146, 162);
    m_canvas.SetColor(k_color_remapped, 40, 92, 196);
    m_canvas.SetColor(k_color_unreachable, 178, 44, 44);
    m_canvas.SetColor(k_color_selected, 228, 196, 40);
    m_canvas.SetColor(k_color_pressed, 52, 168, 76);
    m_canvas.SetColor(k_color_hover, 255, 255, 255);
    m_canvas.SetColor(k_color_dark, 16, 16, 16);
    m_canvas.SetColor(k_color_button, 62, 68, 88);
    m_canvas.SetColor(k_color_warning, 250, 196, 72);
    m_canvas.SetColor(k_color_game, 30, 132, 140);
    m_canvas.SetColor(k_color_black, 0, 0, 0);
    m_canvas.SetColor(k_color_global, 116, 72, 168);
}

void KeyMapper::SetTitle(const char* title)
{
    m_title = IsValidPointer(title) ? title : "";

    if (m_open && !HasGame())
        m_key_map->SetEditLayer(GT_KEY_MAP_GLOBAL);
}

void KeyMapper::Open()
{
    if (m_open)
        return;

    m_open = true;
    m_buttons = 0xFF;
    m_selected = GT_KEY_NONE;
    memset(m_held, 0, sizeof(m_held));
    m_held_count = 0;
    m_tap_valid = false;
    m_query_usage = 0;
    m_reset_armed = false;
    m_message.clear();
    m_chip_count = 0;
    m_key_map->SetEditLayer(HasGame() ? GT_KEY_MAP_USER : GT_KEY_MAP_GLOBAL);
    m_key_map->SetBlocked(true);
}

void KeyMapper::Close()
{
    if (!m_open)
        return;

    m_open = false;
    m_key_map->SetEditLayer(GT_KEY_MAP_USER);
    m_key_map->SetBlocked(false);
}

bool KeyMapper::IsOpen() const
{
    return m_open;
}

void KeyMapper::SetHotkeyNames(const char* mapper, const char* mapping)
{
    m_mapper_hotkey = IsValidPointer(mapper) ? mapper : "";
    m_mapping_hotkey = IsValidPointer(mapping) ? mapping : "";
}

void KeyMapper::HostKey(u16 usage, bool pressed)
{
    if (!m_open || !KeyMap::IsHostKey(usage))
        return;

    if (pressed)
    {
        if (m_held[usage])
            return;

        m_held[usage] = true;
        m_tap_valid = m_held_count == 0;
        m_tap_usage = usage;
        m_held_count++;
        if (m_selected == GT_KEY_NONE)
        {
            m_message = DescribeHostKeyAction(usage);
            m_query_usage = usage;
        }

        return;
    }

    if (!m_held[usage])
        return;

    m_held[usage] = false;
    m_held_count--;
    bool tap = m_tap_valid && (m_tap_usage == usage) && (m_held_count == 0);
    m_tap_valid = false;

    if (usage == m_query_usage)
    {
        m_message.clear();
        m_query_usage = 0;
    }

    if (tap && (m_selected != GT_KEY_NONE))
        BindHostKey(usage);
}

void KeyMapper::ReleaseHostKeys()
{
    memset(m_held, 0, sizeof(m_held));
    m_held_count = 0;
    m_tap_valid = false;
}

void KeyMapper::SetPointer(int x, int y)
{
    m_pointer_x = CLAMP(x, 0, k_key_mapper_width - 1);
    m_pointer_y = CLAMP(y, 0, k_key_mapper_height - 1);
}

void KeyMapper::MovePointer(int dx, int dy)
{
    SetPointer(m_pointer_x + dx, m_pointer_y + dy);
}

void KeyMapper::SetPointerButtons(u8 buttons)
{
    u8 pressed = buttons & (u8)~m_buttons;
    m_buttons = buttons;

    if (!m_open)
        return;

    if ((pressed & KEY_MAPPER_BUTTON_LEFT) != 0)
        Click(m_pointer_x, m_pointer_y);
    else if ((pressed & KEY_MAPPER_BUTTON_RIGHT) != 0)
        RightClick(m_pointer_x, m_pointer_y);
}

void KeyMapper::Render(u8* frame_buffer)
{
    UpdateKeyFlags();
    m_canvas.Clear(k_color_background);
    DrawTitle();
    DrawKeyboard();
    DrawLegend();
    DrawInfo();
    DrawButtons();
    m_canvas.DrawCursor(m_pointer_x, m_pointer_y, k_color_hover, k_color_black);

    if (IsValidPointer(frame_buffer))
        m_canvas.Present(frame_buffer);
}

int KeyMapper::GetWidth() const
{
    return k_key_mapper_width;
}

int KeyMapper::GetHeight() const
{
    return k_key_mapper_height;
}

int KeyMapper::GetDisplayWidth() const
{
    return k_key_mapper_display_width;
}

int KeyMapper::GetDisplayHeight() const
{
    return k_key_mapper_display_height;
}

void KeyMapper::Click(int x, int y)
{
    int bx, by, bw, bh;
    GetResetRect(&bx, &by, &bw, &bh);
    m_query_usage = 0;
    m_tap_valid = false;

    if (is_inside(x, y, bx, by, bw, bh))
    {
        if (m_reset_armed)
        {
            GT_Key_Map_Layer layer = m_key_map->GetEditLayer();
            m_key_map->ClearLayer(layer);
            m_key_map->SetChanged(layer, true);
            m_message = layer == GT_KEY_MAP_GLOBAL ? "Reset the keys of all games" : "Reset to defaults";
        }

        m_reset_armed = !m_reset_armed;
        return;
    }

    m_reset_armed = false;

    for (int i = 0; i < 2; i++)
    {
        GetLayerRect(i, &bx, &by, &bw, &bh);

        if (is_inside(x, y, bx, by, bw, bh))
        {
            SetEditLayer(i == 0 ? GT_KEY_MAP_USER : GT_KEY_MAP_GLOBAL);
            return;
        }
    }

    GetCloseRect(&bx, &by, &bw, &bh);

    if (is_inside(x, y, bx, by, bw, bh))
    {
        Close();
        return;
    }

    for (int i = 0; i < m_chip_count; i++)
    {
        const KeyMapper_Chip& chip = m_chips[i];

        if (!is_inside(x, y, chip.x + chip.width - k_chip_close_width, chip.y, k_chip_close_width, k_chip_height))
            continue;

        if (chip.layout ? m_key_map->Bind(chip.usage, NULL, 0) : m_key_map->Unbind(chip.usage, m_selected))
            m_message = "Removed " + DescribeHostKey(chip.usage);

        return;
    }

    int index = FindLayoutKey(x, y);

    if (index >= 0)
    {
        GT_Keys key = k_key_mapper_layout[index].key;
        m_selected = key == m_selected ? GT_KEY_NONE : key;
        m_message.clear();
    }
}

void KeyMapper::RightClick(int x, int y)
{
    m_reset_armed = false;
    m_query_usage = 0;
    int index = FindLayoutKey(x, y);

    if (index < 0)
        return;

    GT_Keys key = k_key_mapper_layout[index].key;
    ClearKey(key);

    if (key == m_selected)
        m_selected = GT_KEY_NONE;
}

void KeyMapper::ClearKey(GT_Keys key)
{
    bool cleared = false;

    for (int usage = 0; usage < GT_HOST_KEY_COUNT; usage++)
    {
        GT_Key_Map_Layer layer = GT_KEY_MAP_BUILT_IN;
        const GT_Key_Map_Entry& entry = m_key_map->GetEntry((u16)usage, &layer);

        if ((layer != GT_KEY_MAP_BUILT_IN) && drives(entry, key))
            cleared = m_key_map->RestoreBuiltIn((u16)usage) || cleared;
    }

    m_message = cleared ? std::string("Cleared ") + KeyMap::GetKeyLabel(key) : "Nothing to clear";
}

void KeyMapper::BindHostKey(u16 usage)
{
    if (!m_key_map->Bind(usage, &m_selected, 1))
    {
        m_message = DescribeHostKey(usage) + " can't be bound";
        return;
    }

    m_message = "Bound " + DescribeHostKey(usage) + " \x1A " + KeyMap::GetKeyLabel(m_selected);
}

int KeyMapper::FindLayoutKey(int x, int y) const
{
    for (int i = 0; i < k_key_mapper_key_count; i++)
    {
        int kx, ky, kw, kh;
        GetKeyRect(i, &kx, &ky, &kw, &kh);

        if (is_inside(x, y, kx, ky, kw, kh))
            return i;
    }

    return -1;
}

void KeyMapper::GetKeyRect(int index, int* x, int* y, int* width, int* height) const
{
    const GT_Key_Mapper_Key& key = k_key_mapper_layout[index];
    int x0 = k_key_mapper_origin_x + ((key.x * k_key_mapper_unit_x) / k_key_mapper_grid);
    int x1 = k_key_mapper_origin_x + (((key.x + key.width) * k_key_mapper_unit_x) / k_key_mapper_grid);
    int y0 = k_key_mapper_origin_y + ((key.y * k_key_mapper_unit_y) / k_key_mapper_grid);
    int y1 = k_key_mapper_origin_y + (((key.y + key.height) * k_key_mapper_unit_y) / k_key_mapper_grid);
    *x = x0;
    *y = y0;
    *width = x1 - x0 - 2;
    *height = y1 - y0 - 2;
}

int KeyMapper::GetKeyboardBottom() const
{
    int bottom = k_key_mapper_origin_y;

    for (int i = 0; i < k_key_mapper_key_count; i++)
    {
        int x, y, width, height;
        GetKeyRect(i, &x, &y, &width, &height);
        bottom = MAX(bottom, y + height);
    }

    return bottom;
}

void KeyMapper::UpdateKeyFlags()
{
    bool reached[GT_KEY_COUNT];
    int level = m_key_map->GetHostLevel();
    m_key_map->GetReachable(reached);

    for (int i = 0; i < GT_KEY_COUNT; i++)
        m_key_flags[i] = reached[i] ? k_key_flag_reachable : 0;

    for (int usage = 0; usage < GT_HOST_KEY_COUNT; usage++)
    {
        GT_Key_Map_Layer layer = GT_KEY_MAP_BUILT_IN;
        const GT_Key_Map_Entry& entry = m_key_map->GetEntry((u16)usage, &layer);
        bool pressed = m_held[usage] && (m_selected == GT_KEY_NONE);
        GT_Key_Map_Char target;

        if (pressed && m_key_map->GetActiveLayoutTarget((u16)usage, level, &target))
        {
            m_key_flags[target.key] |= k_key_flag_pressed;
            continue;
        }

        u8 flags = (layer != GT_KEY_MAP_BUILT_IN) ? k_key_flag_remapped : 0;

        if (pressed)
            flags |= k_key_flag_pressed;

        for (int i = 0; i < entry.count; i++)
            m_key_flags[entry.keys[i]] |= flags;
    }
}

void KeyMapper::DrawTitle()
{
    std::string hint = m_mapper_hotkey.empty() ? "" : m_mapper_hotkey + " closes";
    int hint_width = OsdCanvas::GetTextWidth(hint.c_str());
    std::string title = m_key_map->GetEditLayer() == GT_KEY_MAP_GLOBAL ? std::string("All games") : m_title;
    size_t fit = (size_t)MAX(0, (k_key_mapper_width - (k_margin * 2) - hint_width - 16) / 8);

    if (title.size() > fit)
        title = title.substr(0, fit);

    m_canvas.FillRect(0, 0, k_key_mapper_width, k_title_height, k_color_title);
    m_canvas.DrawText(8, 3, title.c_str(), k_color_text, true);
    m_canvas.DrawText(k_key_mapper_width - 8 - hint_width, 3, hint.c_str(), k_color_dim, true);
}

void KeyMapper::DrawKeyboard()
{
    for (int i = 0; i < k_key_mapper_key_count; i++)
    {
        const GT_Key_Mapper_Key& key = k_key_mapper_layout[i];
        int x, y, width, height;
        GetKeyRect(i, &x, &y, &width, &height);

        bool selected = key.key == m_selected;
        u8 flags = m_key_flags[key.key];
        u8 fill = k_color_key;
        u8 text = k_color_text;
        u8 frame = k_color_border;

        if ((flags & k_key_flag_pressed) != 0)
            fill = k_color_pressed;
        else if (selected)
        {
            fill = k_color_selected;
            text = k_color_dark;
        }
        else if ((flags & k_key_flag_reachable) == 0)
            fill = k_color_unreachable;
        else if ((flags & k_key_flag_remapped) != 0)
            fill = k_color_remapped;

        if (IsPointerIn(x, y, width, height))
            frame = k_color_hover;

        m_canvas.FillRect(x, y, width, height, fill);
        m_canvas.FrameRect(x, y, width, height, frame);

        std::string label = key.label;
        size_t split = label.find('\n');
        std::string first = label.substr(0, split);
        std::string second = split == std::string::npos ? "" : label.substr(split + 1);
        int lines = second.empty() ? 1 : 2;
        bool small = (OsdCanvas::GetTextWidth(first.c_str()) > width - 4) ||
            (OsdCanvas::GetTextWidth(second.c_str()) > width - 4) || (((lines * 8) + ((lines - 1) * 2)) > height - 4);
        int line_height = small ? 6 : 8;
        int top = y + ((height - ((lines * line_height) + ((lines - 1) * 2))) / 2);

        DrawKeyLabel(x, top, width, first.c_str(), text, small);

        if (lines == 2)
            DrawKeyLabel(x, top + line_height + 2, width, second.c_str(), text, small);
    }
}

void KeyMapper::DrawKeyLabel(int x, int y, int width, const char* label, u8 color, bool small)
{
    if (small)
        m_canvas.DrawSmallText(x + ((width - OsdCanvas::GetSmallTextWidth(label)) / 2), y, label, color);
    else
        m_canvas.DrawText(x + ((width - OsdCanvas::GetTextWidth(label) + 1) / 2), y, label, color, false);
}

void KeyMapper::DrawLegend()
{
    static const u8 k_colors[5] = { k_color_key, k_color_remapped, k_color_unreachable, k_color_selected,
        k_color_pressed };
    static const char* const k_names[5] = { "Built-in", "Remapped", "No host key", "Selected", "Pressed" };
    int x = k_key_mapper_origin_x;
    int y = GetKeyboardBottom() + 8;

    for (int i = 0; i < 5; i++)
    {
        m_canvas.FillRect(x, y, 8, 8, k_colors[i]);
        m_canvas.FrameRect(x, y, 8, 8, k_color_border);
        m_canvas.DrawText(x + 12, y, k_names[i], k_color_dim, false);
        x += 12 + OsdCanvas::GetTextWidth(k_names[i]) + 20;
    }
}

void KeyMapper::DrawInfo()
{
    int y = GetKeyboardBottom() + 26;
    m_chip_count = 0;

    if (m_selected == GT_KEY_NONE)
        m_canvas.DrawText(k_margin, y, "Select a key to show its mapping", k_color_text, true);
    else
    {
        const char* label = KeyMap::GetKeyLabel(m_selected);
        int x = k_margin + OsdCanvas::GetTextWidth(label) + 24;
        size_t fit = (size_t)MAX(0, (k_key_mapper_width - k_margin - x) / 8);
        m_canvas.DrawText(k_margin, y, label, k_color_text, true);
        m_canvas.DrawText(x, y, DescribeOwnHostKeys(m_selected).substr(0, fit).c_str(), k_color_text, true);
        DrawChips(y + k_line_height);
    }

    y += (k_line_height * 3) + 4;
    m_canvas.DrawText(k_margin, y, "Click a key, press a host key to bind it. Right click clears.", k_color_dim, true);
    y += k_line_height;
    bool host = m_key_map->GetLayout() == GT_KEY_LAYOUT_CHARACTER;
    m_canvas.DrawText(k_margin, y, host ? "Layout: Host layout" : "Layout: Japanese positions", k_color_dim, true);
    y += k_line_height;
    m_canvas.DrawText(k_margin, y, m_message.c_str(), k_color_text, true);

    std::string warning;

    if (m_reset_armed)
        warning = "Click Reset All again to confirm";
    else if (!m_key_map->IsEnabled())
        warning = "Mapping off (" + m_mapping_hotkey + "): using built-in and all games keys";

    int bx, by, bw, bh;
    GetResetRect(&bx, &by, &bw, &bh);
    m_canvas.DrawText(k_margin, by - k_line_height - 4, warning.c_str(), k_color_warning, true);
}

void KeyMapper::DrawChips(int y)
{
    static const char* const k_head = "Host keys:";
    static const char* const k_tags[GT_KEY_MAP_LAYER_COUNT] = { " (built in)", " (all games)", " (game)", " (user)" };
    static const u8 k_fills[GT_KEY_MAP_LAYER_COUNT] = { k_color_key, k_color_global, k_color_game, k_color_remapped };
    int start = k_margin + OsdCanvas::GetTextWidth(k_head) + 8;
    int x = start;
    int row = 0;
    bool any = false;

    m_canvas.DrawText(k_margin, y, k_head, k_color_dim, true);

    for (int l = GT_KEY_MAP_BUILT_IN; (l < GT_KEY_MAP_LAYER_COUNT) && (row < 2); l++)
    {
        for (int usage = 0; (usage < GT_HOST_KEY_COUNT) && (row < 2); usage++)
        {
            GT_Key_Map_Layer layer = GT_KEY_MAP_BUILT_IN;
            const GT_Key_Map_Entry& entry = m_key_map->GetEntry((u16)usage, &layer);

            if (layer != l)
                continue;

            if (m_key_map->UsesLayout((u16)usage))
            {
                any = DrawLayoutChips((u16)usage, start, &x, &y, &row) || any;
                continue;
            }

            if (!drives(entry, m_selected))
                continue;

            any = true;
            AddChip(DescribeHostKey((u16)usage) + k_tags[l], k_fills[l], (u16)usage, false, start, &x, &y, &row);
        }
    }

    if (!any)
        m_canvas.DrawText(start, y, "none", k_color_dim, true);
}

bool KeyMapper::DrawLayoutChips(u16 usage, int start, int* x, int* y, int* row)
{
    int levels = m_key_map->HasHostLevels() ? GT_KEY_MAP_LEVELS : 2;
    int masks[2] = { 0, 0 };

    for (int level = 0; level < levels; level++)
    {
        GT_Key_Map_Char target;

        if (m_key_map->GetLayoutTarget(usage, level, &target))
            masks[0] |= (target.key == (u8)m_selected) ? (1 << level) : 0;
        else if ((level < 2) && drives(m_key_map->GetEntry(usage), m_selected))
            masks[1] |= 1 << level;
    }

    for (int i = 0; (i < 2) && (*row < 2); i++)
    {
        const char* tag = (i == 0) ? " (layout)" : " (built in)";
        int mask = masks[i];

        if ((mask & 3) == 3)
        {
            AddChip(DescribeHostKey(usage) + tag, k_color_key, usage, true, start, x, y, row);
            mask &= ~3;
        }

        for (int level = 0; (level < GT_KEY_MAP_LEVELS) && (*row < 2); level++)
        {
            if ((mask & (1 << level)) != 0)
                AddChip(level_prefix(level) + DescribeHostKey(usage) + tag, k_color_key, usage, true, start, x, y, row);
        }
    }

    return (masks[0] | masks[1]) != 0;
}

void KeyMapper::AddChip(const std::string& text, u8 fill, u16 usage, bool layout, int start, int* x, int* y, int* row)
{
    int width = OsdCanvas::GetTextWidth(text.c_str()) + 8 + k_chip_close_width;

    if ((*x + width > k_key_mapper_width - k_margin) && (*x > start))
    {
        if (++(*row) == 2)
            return;

        *x = start;
        *y += k_line_height;
    }

    int close_x = *x + width - k_chip_close_width;
    bool hover = IsPointerIn(close_x, *y - 1, k_chip_close_width, k_chip_height);

    m_canvas.FillRect(*x, *y - 1, width, k_chip_height, fill);
    m_canvas.FrameRect(*x, *y - 1, width, k_chip_height, k_color_border);
    m_canvas.DrawText(*x + 4, *y, text.c_str(), k_color_text, true);
    m_canvas.FrameRect(close_x, *y - 1, k_chip_close_width, k_chip_height, hover ? k_color_hover : k_color_border);
    m_canvas.DrawText(close_x + 4, *y, "x", hover ? k_color_hover : k_color_text, true);

    if (m_chip_count < KEY_MAPPER_CHIPS)
    {
        KeyMapper_Chip& chip = m_chips[m_chip_count++];
        chip.x = *x;
        chip.y = *y - 1;
        chip.width = width;
        chip.usage = usage;
        chip.layout = layout;
    }

    *x += width + 6;
}

void KeyMapper::DrawButtons()
{
    int x, y, width, height;
    GetResetRect(&x, &y, &width, &height);
    DrawButton(x, y, width, "Reset All");
    GetCloseRect(&x, &y, &width, &height);
    DrawButton(x, y, width, "Close");

    bool global = m_key_map->GetEditLayer() == GT_KEY_MAP_GLOBAL;
    GetLayerRect(0, &x, &y, &width, &height);
    DrawButton(x, y, width, "This Game", !global, HasGame());
    GetLayerRect(1, &x, &y, &width, &height);
    DrawButton(x, y, width, "All Games", global);
}

void KeyMapper::DrawButton(int x, int y, int width, const char* text, bool active, bool enabled)
{
    bool hover = enabled && IsPointerIn(x, y, width, k_button_height);
    m_canvas.FillRect(x, y, width, k_button_height, active ? k_color_selected : k_color_button);
    m_canvas.FrameRect(x, y, width, k_button_height, hover ? k_color_hover : k_color_border);
    m_canvas.DrawText(x + ((width - OsdCanvas::GetTextWidth(text)) / 2), y + 3, text,
        !enabled ? k_color_dim : active ? k_color_dark : k_color_text, true);
}

void KeyMapper::SetEditLayer(GT_Key_Map_Layer layer)
{
    if ((layer == GT_KEY_MAP_USER) && !HasGame())
        return;

    m_key_map->SetEditLayer(layer);
    m_message = layer == GT_KEY_MAP_GLOBAL ? "Changes now apply to all games" : "Changes now apply to this game";
}

bool KeyMapper::HasGame() const
{
    return !m_title.empty();
}

void KeyMapper::GetResetRect(int* x, int* y, int* width, int* height) const
{
    *x = k_margin;
    *y = k_key_mapper_height - k_button_height - 10;
    *width = k_button_width;
    *height = k_button_height;
}

void KeyMapper::GetLayerRect(int index, int* x, int* y, int* width, int* height) const
{
    *x = (k_key_mapper_width / 2) - k_button_width - 2 + (index * (k_button_width + 4));
    *y = k_key_mapper_height - k_button_height - 10;
    *width = k_button_width;
    *height = k_button_height;
}

void KeyMapper::GetCloseRect(int* x, int* y, int* width, int* height) const
{
    *x = k_key_mapper_width - k_margin - k_button_width;
    *y = k_key_mapper_height - k_button_height - 10;
    *width = k_button_width;
    *height = k_button_height;
}

bool KeyMapper::IsPointerIn(int x, int y, int width, int height) const
{
    return is_inside(m_pointer_x, m_pointer_y, x, y, width, height);
}

std::string KeyMapper::DescribeHostKey(u16 usage) const
{
    u32 c = m_key_map->GetHostChar(usage, 0);

    if ((c > 0x20) && (c < 0x7F))
    {
        char text[2] = { (char)(((c >= 'a') && (c <= 'z')) ? (c - 32) : c), 0 };
        return text;
    }

    const char* label = KeyMap::GetHostKeyLabel(usage);

    if (IsValidPointer(label))
        return label;

    char text[16];
    snprintf(text, sizeof(text), "Key %02Xh", usage);
    return text;
}

std::string KeyMapper::DescribeHostKeyAction(u16 usage) const
{
    int level = m_key_map->GetHostLevel();
    GT_Key_Map_Char target;

    if (m_key_map->GetActiveLayoutTarget(usage, level, &target))
        return level_prefix(level) + DescribeHostKey(usage) + " \x1A " + DescribeLayoutTarget(target, level);

    return DescribeHostKey(usage) + " \x1A " + DescribeTargets(m_key_map->GetActiveEntry(usage));
}

std::string KeyMapper::DescribeLayoutTarget(const GT_Key_Map_Char& target, int level) const
{
    const char* label = KeyMap::GetKeyLabel((GT_Keys)target.key);
    std::string text = IsValidPointer(label) ? label : "?";

    if (target.shift != 0)
        return "SHIFT + " + text;

    return ((level & 1) != 0) ? text + " (SHIFT off)" : text;
}

std::string KeyMapper::DescribeOwnHostKeys(GT_Keys key) const
{
    std::string text;

    for (int usage = 0; usage < GT_HOST_KEY_COUNT; usage++)
    {
        GT_Key_Map_Layer layer = GT_KEY_MAP_BUILT_IN;
        const GT_Key_Map_Entry& entry = m_key_map->GetEntry((u16)usage, &layer);

        if ((layer == GT_KEY_MAP_BUILT_IN) || !drives(m_key_map->GetLayerEntry(GT_KEY_MAP_BUILT_IN, (u16)usage), key))
            continue;

        if ((entry.count == 1) && (entry.keys[0] == (u8)key))
            continue;

        text += (text.empty() ? "Host " : ", ") + DescribeHostKey((u16)usage) + " \x1A " + DescribeTargets(entry);
    }

    return text;
}

std::string KeyMapper::DescribeTargets(const GT_Key_Map_Entry& entry) const
{
    if (entry.count == 0)
        return "nothing";

    std::string text;

    for (int i = 0; i < entry.count; i++)
    {
        const char* label = KeyMap::GetKeyLabel((GT_Keys)entry.keys[i]);
        text += std::string(i > 0 ? " + " : "") + (IsValidPointer(label) ? label : "?");
    }

    return text;
}
