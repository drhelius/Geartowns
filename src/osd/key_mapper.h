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

#ifndef KEY_MAPPER_H
#define KEY_MAPPER_H

#include <string>
#include "../common/common.h"
#include "../input/key_map.h"
#include "osd_canvas.h"

#define KEY_MAPPER_CHIPS 16
#define KEY_MAPPER_BUTTON_LEFT 0x01
#define KEY_MAPPER_BUTTON_RIGHT 0x02

class KeyMapper
{
public:
    struct KeyMapper_Chip
    {
        int x;
        int y;
        int width;
        u16 usage;
        bool layout;
    };

public:
    KeyMapper();
    void Init(KeyMap* key_map);
    void SetTitle(const char* title);
    void Open();
    void Close();
    bool IsOpen() const;
    void SetHotkeyNames(const char* mapper, const char* mapping);
    void HostKey(u16 usage, bool pressed);
    void ReleaseHostKeys();
    void SetPointer(int x, int y);
    void MovePointer(int dx, int dy);
    void SetPointerButtons(u8 buttons);
    void Render(u8* frame_buffer);
    int GetWidth() const;
    int GetHeight() const;
    int GetDisplayWidth() const;
    int GetDisplayHeight() const;

private:
    void Click(int x, int y);
    void RightClick(int x, int y);
    void ClearKey(GT_Keys key);
    void BindHostKey(u16 usage);
    int FindLayoutKey(int x, int y) const;
    void GetKeyRect(int index, int* x, int* y, int* width, int* height) const;
    int GetKeyboardBottom() const;
    void UpdateKeyFlags();
    void DrawTitle();
    void DrawKeyboard();
    void DrawKeyLabel(int x, int y, int width, const char* label, u8 color, bool small);
    void DrawLegend();
    void DrawInfo();
    void DrawChips(int y);
    bool DrawLayoutChips(u16 usage, int start, int* x, int* y, int* row);
    void AddChip(const std::string& text, u8 fill, u16 usage, bool layout, int start, int* x, int* y, int* row);
    void DrawButtons();
    void DrawButton(int x, int y, int width, const char* text);
    void GetResetRect(int* x, int* y, int* width, int* height) const;
    void GetCloseRect(int* x, int* y, int* width, int* height) const;
    bool IsPointerIn(int x, int y, int width, int height) const;
    std::string DescribeHostKey(u16 usage) const;
    std::string DescribeHostKeyAction(u16 usage) const;
    std::string DescribeLayoutTarget(const GT_Key_Map_Char& target, int level) const;
    std::string DescribeOwnHostKeys(GT_Keys key) const;
    std::string DescribeTargets(const GT_Key_Map_Entry& entry) const;

private:
    KeyMap* m_key_map;
    OsdCanvas m_canvas;
    bool m_open;
    std::string m_title;
    std::string m_mapper_hotkey;
    std::string m_mapping_hotkey;
    int m_pointer_x;
    int m_pointer_y;
    u8 m_buttons;
    GT_Keys m_selected;
    bool m_held[GT_HOST_KEY_COUNT];
    int m_held_count;
    u16 m_tap_usage;
    bool m_tap_valid;
    u16 m_query_usage;
    bool m_reset_armed;
    std::string m_message;
    u8 m_key_flags[GT_KEY_COUNT];
    KeyMapper_Chip m_chips[KEY_MAPPER_CHIPS];
    int m_chip_count;
};

#endif /* KEY_MAPPER_H */
