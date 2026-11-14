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

#ifndef KEY_MAP_H
#define KEY_MAP_H

#include <string>
#include "../common/common.h"

#define GT_HOST_KEY_COUNT 0xE8
#define GT_KEY_MAP_TARGETS 4

typedef void (*GT_Key_Event_Fn)(void* context, GT_Keys key, bool pressed);

enum GT_Key_Map_Layer
{
    GT_KEY_MAP_BUILT_IN = 0,
    GT_KEY_MAP_GAME,
    GT_KEY_MAP_USER,
    GT_KEY_MAP_LAYER_COUNT
};

struct GT_Key_Map_Entry
{
    bool set;
    u8 count;
    u8 keys[GT_KEY_MAP_TARGETS];
};

class KeyMap
{
public:
    KeyMap();
    void Init(GT_Key_Event_Fn callback, void* context);
    void HostKeyPressed(u16 usage);
    void HostKeyReleased(u16 usage);
    void IgnoreHostKey(u16 usage);
    bool IsHostKeyDown(u16 usage) const;
    void ReleaseAll();
    void Suspend();
    void Resync();
    void SetBlocked(bool blocked);
    bool IsBlocked() const;
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    void ResetBuiltIn();
    bool SetLayer(GT_Key_Map_Layer layer, const char* text, std::string* error);
    std::string GetLayer(GT_Key_Map_Layer layer) const;
    std::string GetMerged() const;
    void ClearLayer(GT_Key_Map_Layer layer);
    bool IsLayerEmpty(GT_Key_Map_Layer layer) const;
    void SetEntry(GT_Key_Map_Layer layer, u16 usage, const GT_Key_Map_Entry& entry);
    const GT_Key_Map_Entry& GetLayerEntry(GT_Key_Map_Layer layer, u16 usage) const;
    const GT_Key_Map_Entry& GetEntry(u16 usage, GT_Key_Map_Layer* layer = NULL) const;
    bool Bind(u16 usage, const GT_Keys* keys, int count);
    bool Unbind(u16 usage, GT_Keys key);
    bool RestoreBuiltIn(u16 usage);
    bool IsChanged() const;
    void SetChanged(bool changed);

    static bool IsHostKey(u16 usage);
    static int FindHostKey(const char* name);
    static const char* GetHostKeyName(u16 usage);
    static const char* GetHostKeyLabel(u16 usage);
    static int FindKey(const char* name);
    static const char* GetKeyName(GT_Keys key);
    static const char* GetKeyLabel(GT_Keys key);
    static bool ParseEntry(const std::string& host, const std::string& value, u16* usage, GT_Key_Map_Entry* entry,
        std::string* error);
    static std::string FormatTargets(const GT_Key_Map_Entry& entry);

private:
    const GT_Key_Map_Entry& GetActiveEntry(u16 usage) const;
    const GT_Key_Map_Entry& GetBaseEntry(GT_Key_Map_Layer layer, u16 usage) const;
    void SetUserEntry(u16 usage, const GT_Key_Map_Entry& entry);
    void PressKey(u8 key);
    void ReleaseKey(u8 key);
    void SyncKey(u8 key);
    static bool IsModifier(u8 key);
    static bool SameTargets(const GT_Key_Map_Entry& a, const GT_Key_Map_Entry& b);
    std::string FormatLayers(int first, int last) const;

private:
    GT_Key_Map_Entry m_layers[GT_KEY_MAP_LAYER_COUNT][GT_HOST_KEY_COUNT];
    GT_Key_Map_Entry m_host_targets[GT_HOST_KEY_COUNT];
    bool m_host_down[GT_HOST_KEY_COUNT];
    int m_references[GT_KEY_COUNT];
    bool m_enabled;
    bool m_blocked;
    bool m_changed;
    GT_Key_Event_Fn m_callback;
    void* m_context;
};

#endif /* KEY_MAP_H */
