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

#include "key_map.h"
#include "host_keys.h"
#include "keyboard_chars.h"
#include "keyboard_keys.h"
#include "../media/media_file.h"

static const GT_Key_Map_Entry k_key_map_empty = { false, 0, { 0, 0, 0, 0 } };

KeyMap::KeyMap()
{
    InitPointer(m_callback);
    InitPointer(m_context);
    memset(m_layers, 0, sizeof(m_layers));
    memset(m_host_targets, 0, sizeof(m_host_targets));
    memset(m_host_down, 0, sizeof(m_host_down));
    memset(m_host_deferred, 0, sizeof(m_host_deferred));
    memset(m_references, 0, sizeof(m_references));
    memset(m_host_chars, 0, sizeof(m_host_chars));
    memset(m_layout_targets, 0, sizeof(m_layout_targets));
    memset(m_level_host_keys, 0, sizeof(m_level_host_keys));
    m_host_levels = false;
    m_layout = GT_KEY_LAYOUT_CHARACTER;
    m_shift_owner = 0;
    m_shift_forced = false;
    m_enabled = true;
    m_blocked = false;
    m_changed = false;
    ResetBuiltIn();
}

void KeyMap::Init(GT_Key_Event_Fn callback, void* context)
{
    m_callback = callback;
    m_context = context;
}

void KeyMap::HostKeyPressed(u16 usage)
{
    if (!IsHostKey(usage) || m_host_down[usage])
        return;

    m_host_down[usage] = true;
    m_host_targets[usage] = k_key_map_empty;

    if (IsDeferredHostKey(usage))
    {
        m_host_deferred[usage] = true;
        return;
    }

    if (usage < k_host_key_first_modifier)
    {
        FlushDeferredHostKeys(usage);
        ClearShiftOverride();
    }

    PressHostTargets(usage);
}

void KeyMap::HostKeyReleased(u16 usage)
{
    if (!IsHostKey(usage) || !m_host_down[usage])
        return;

    if (m_host_deferred[usage])
    {
        m_host_deferred[usage] = false;
        PressHostTargets(usage);
    }

    m_host_down[usage] = false;
    GT_Key_Map_Entry& targets = m_host_targets[usage];

    for (int i = targets.count - 1; i >= 0; i--)
        ReleaseKey(targets.keys[i]);

    targets = k_key_map_empty;

    if (m_shift_owner == usage)
        ClearShiftOverride();
}

void KeyMap::IgnoreHostKey(u16 usage)
{
    if (!IsHostKey(usage) || m_host_down[usage])
        return;

    m_host_down[usage] = true;
    m_host_targets[usage] = k_key_map_empty;
}

void KeyMap::DiscardDeferredHostKeys()
{
    for (int i = k_host_key_first_modifier; i < GT_HOST_KEY_COUNT; i++)
        m_host_deferred[i] = false;
}

bool KeyMap::IsHostKeyDown(u16 usage) const
{
    return IsHostKey(usage) && m_host_down[usage];
}

void KeyMap::ReleaseAll()
{
    for (int i = GT_KEY_NONE + 1; i < GT_KEY_COUNT; i++)
    {
        bool down = IsKeyDown((u8)i);
        m_references[i] = 0;

        if (down && IsValidPointer(m_callback))
            m_callback(m_context, (GT_Keys)i, false);
    }

    m_shift_owner = 0;
    memset(m_host_targets, 0, sizeof(m_host_targets));
    memset(m_host_down, 0, sizeof(m_host_down));
    memset(m_host_deferred, 0, sizeof(m_host_deferred));
}

void KeyMap::Suspend()
{
    DiscardDeferredHostKeys();

    for (int usage = 0; usage < GT_HOST_KEY_COUNT; usage++)
    {
        GT_Key_Map_Entry& targets = m_host_targets[usage];

        for (int i = targets.count - 1; i >= 0; i--)
            ReleaseKey(targets.keys[i]);

        targets = k_key_map_empty;
    }

    ClearShiftOverride();
}

void KeyMap::Resync()
{
    for (int i = GT_KEY_NONE + 1; i < GT_KEY_COUNT; i++)
    {
        if ((m_references[i] == 0) && !IsModifier((u8)i) && IsValidPointer(m_callback))
            m_callback(m_context, (GT_Keys)i, false);
    }

    for (int i = 0; i < k_modifier_key_count; i++)
        SyncKey((u8)k_modifier_keys[i]);

    for (int i = GT_KEY_NONE + 1; i < GT_KEY_COUNT; i++)
    {
        if ((m_references[i] > 0) && !IsModifier((u8)i) && IsValidPointer(m_callback))
            m_callback(m_context, (GT_Keys)i, true);
    }
}

void KeyMap::SetBlocked(bool blocked)
{
    if (blocked && !m_blocked)
        Suspend();

    m_blocked = blocked;
}

bool KeyMap::IsBlocked() const
{
    return m_blocked;
}

void KeyMap::SetEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return;

    Suspend();
    m_enabled = enabled;
}

bool KeyMap::IsEnabled() const
{
    return m_enabled;
}

void KeyMap::SetLayout(GT_Key_Layout layout)
{
    if (layout == m_layout)
        return;

    Suspend();
    m_layout = layout;
}

GT_Key_Layout KeyMap::GetLayout() const
{
    return m_layout;
}

void KeyMap::SetHostChars(const u32* chars)
{
    u32 host_chars[GT_HOST_KEY_COUNT][GT_KEY_MAP_LEVELS];
    memset(host_chars, 0, sizeof(host_chars));

    for (int usage = 0; IsValidPointer(chars) && (usage < GT_HOST_KEY_COUNT); usage++)
    {
        if (!IsLayoutHostKey((u16)usage))
            continue;

        const u32* levels = chars + (usage * GT_KEY_MAP_LEVELS);

        for (int level = 0; level < GT_KEY_MAP_LEVELS; level++)
        {
            u32 c = levels[level];
            bool ascii = (c > 0x20) && (c < 0x7F);

            if ((level < 2) || (ascii && (c != levels[0]) && (c != levels[1])))
                host_chars[usage][level] = c;
        }
    }

    if (memcmp(host_chars, m_host_chars, sizeof(host_chars)) == 0)
        return;

    Suspend();
    memcpy(m_host_chars, host_chars, sizeof(host_chars));
    BuildLayoutTargets();
}

u32 KeyMap::GetHostChar(u16 usage, int level) const
{
    if (!IsLayoutHostKey(usage) || (level < 0) || (level >= GT_KEY_MAP_LEVELS))
        return 0;

    return m_host_chars[usage][level];
}

void KeyMap::SetLevelHostKeys(const u16* usages, int count)
{
    bool keys[GT_HOST_KEY_COUNT];
    memset(keys, 0, sizeof(keys));

    for (int i = 0; IsValidPointer(usages) && (i < count); i++)
    {
        if ((usages[i] >= k_host_key_first_modifier) && IsHostKey(usages[i]))
            keys[usages[i]] = true;
    }

    if (memcmp(keys, m_level_host_keys, sizeof(keys)) == 0)
        return;

    Suspend();
    memcpy(m_level_host_keys, keys, sizeof(keys));
    BuildLayoutTargets();
}

bool KeyMap::HasHostLevels() const
{
    return m_host_levels && (m_layout == GT_KEY_LAYOUT_CHARACTER);
}

int KeyMap::GetHostLevel() const
{
    int level = (m_host_down[k_host_key_left_shift] || m_host_down[k_host_key_right_shift]) ? 1 : 0;

    for (int i = k_host_key_first_modifier; HasHostLevels() && (i < GT_HOST_KEY_COUNT); i++)
    {
        if (m_level_host_keys[i] && m_host_down[i])
            return level + 2;
    }

    return level;
}

bool KeyMap::UsesLayout(u16 usage) const
{
    if ((m_layout != GT_KEY_LAYOUT_CHARACTER) || !IsLayoutHostKey(usage))
        return false;

    return !m_layers[GT_KEY_MAP_GAME][usage].set && !m_layers[GT_KEY_MAP_USER][usage].set;
}

bool KeyMap::GetLayoutTarget(u16 usage, int level, GT_Key_Map_Char* target) const
{
    if ((m_layout != GT_KEY_LAYOUT_CHARACTER) || !IsLayoutHostKey(usage) || (level < 0) || (level >= GT_KEY_MAP_LEVELS))
        return false;

    if ((level >= 2) && !m_host_levels)
        return false;

    *target = m_layout_targets[usage][level];
    return target->key != GT_KEY_NONE;
}

bool KeyMap::ResolveLayoutTarget(u16 usage, int level, GT_Key_Map_Char* target) const
{
    return UsesLayout(usage) && FindLayoutTarget(usage, level, target);
}

void KeyMap::GetReachable(bool* reached) const
{
    memset(reached, 0, sizeof(bool) * GT_KEY_COUNT);

    for (int usage = 0; usage < GT_HOST_KEY_COUNT; usage++)
    {
        if (!IsValidPointer(GetHostKeyName((u16)usage)))
            continue;

        const GT_Key_Map_Entry& entry = GetEntry((u16)usage);
        bool layout = UsesLayout((u16)usage);
        bool built_in = !layout;

        for (int level = 0; layout && (level < GT_KEY_MAP_LEVELS); level++)
        {
            GT_Key_Map_Char target;

            if (GetLayoutTarget((u16)usage, level, &target))
            {
                reached[target.key] = true;
                reached[GT_KEY_SHIFT] = reached[GT_KEY_SHIFT] || (target.shift != 0);
            }
            else if (level < 2)
                built_in = true;
        }

        for (int i = 0; built_in && (i < entry.count); i++)
            reached[entry.keys[i]] = true;
    }
}

void KeyMap::ResetBuiltIn()
{
    memset(m_layers[GT_KEY_MAP_BUILT_IN], 0, sizeof(m_layers[GT_KEY_MAP_BUILT_IN]));

    for (int i = 0; i < k_default_key_count; i++)
    {
        GT_Key_Map_Entry& entry = m_layers[GT_KEY_MAP_BUILT_IN][k_default_keys[i].usage];
        entry.set = true;
        entry.count = 1;
        entry.keys[0] = (u8)k_default_keys[i].key;
    }
}

bool KeyMap::SetLayer(GT_Key_Map_Layer layer, const char* text, std::string* error)
{
    GT_Key_Map_Entry entries[GT_HOST_KEY_COUNT];
    memset(entries, 0, sizeof(entries));
    std::string data = IsValidPointer(text) ? text : "";
    size_t start = 0;
    int number = 0;

    while (start < data.size())
    {
        size_t end = data.find('\n', start);
        end = end == std::string::npos ? data.size() : end;
        std::string line = trim_chars(data.substr(start, end - start));
        start = end + 1;
        number++;

        if (line.empty() || (line[0] == ';') || (line[0] == '#') || (line[0] == '['))
            continue;

        size_t equals = line.find('=');
        std::string message;
        u16 usage = 0;
        GT_Key_Map_Entry entry;

        if (equals == std::string::npos)
            message = "Expected host_key = keys";
        else if (ParseEntry(line.substr(0, equals), line.substr(equals + 1), &usage, &entry, &message))
        {
            if (entries[usage].set)
                message = std::string(GetHostKeyName(usage)) + " is set twice";
            else
                entries[usage] = entry;
        }

        if (!message.empty())
        {
            if (IsValidPointer(error))
                *error = "Line " + int_to_string(number) + ": " + message;

            return false;
        }
    }

    if (layer == GT_KEY_MAP_BUILT_IN)
    {
        ResetBuiltIn();

        for (int i = 0; i < GT_HOST_KEY_COUNT; i++)
        {
            if (entries[i].set)
                m_layers[layer][i] = entries[i];
        }
    }
    else
        memcpy(m_layers[layer], entries, sizeof(entries));

    return true;
}

std::string KeyMap::GetLayer(GT_Key_Map_Layer layer) const
{
    return FormatLayers(layer, layer);
}

std::string KeyMap::GetMerged() const
{
    return FormatLayers(GT_KEY_MAP_GAME, GT_KEY_MAP_USER);
}

void KeyMap::ClearLayer(GT_Key_Map_Layer layer)
{
    if (layer == GT_KEY_MAP_BUILT_IN)
        ResetBuiltIn();
    else
        memset(m_layers[layer], 0, sizeof(m_layers[layer]));
}

bool KeyMap::IsLayerEmpty(GT_Key_Map_Layer layer) const
{
    for (int i = 0; i < GT_HOST_KEY_COUNT; i++)
    {
        if (m_layers[layer][i].set)
            return false;
    }

    return true;
}

void KeyMap::SetEntry(GT_Key_Map_Layer layer, u16 usage, const GT_Key_Map_Entry& entry)
{
    if (IsHostKey(usage))
        m_layers[layer][usage] = entry;
}

const GT_Key_Map_Entry& KeyMap::GetLayerEntry(GT_Key_Map_Layer layer, u16 usage) const
{
    return IsHostKey(usage) ? m_layers[layer][usage] : k_key_map_empty;
}

const GT_Key_Map_Entry& KeyMap::GetEntry(u16 usage, GT_Key_Map_Layer* layer) const
{
    if (!IsHostKey(usage))
    {
        if (IsValidPointer(layer))
            *layer = GT_KEY_MAP_BUILT_IN;

        return k_key_map_empty;
    }

    for (int l = GT_KEY_MAP_USER; l > GT_KEY_MAP_BUILT_IN; l--)
    {
        if (m_layers[l][usage].set)
        {
            if (IsValidPointer(layer))
                *layer = (GT_Key_Map_Layer)l;

            return m_layers[l][usage];
        }
    }

    if (IsValidPointer(layer))
        *layer = GT_KEY_MAP_BUILT_IN;

    return m_layers[GT_KEY_MAP_BUILT_IN][usage];
}

bool KeyMap::Bind(u16 usage, const GT_Keys* keys, int count)
{
    if (!IsValidPointer(GetHostKeyName(usage)) || (count < 0) || (count > GT_KEY_MAP_TARGETS))
        return false;

    GT_Key_Map_Entry entry = k_key_map_empty;
    entry.set = true;

    for (int i = 0; i < count; i++)
    {
        if (!IsValidPointer(GetKeyName(keys[i])))
            return false;

        entry.keys[entry.count++] = (u8)keys[i];
    }

    SetUserEntry(usage, entry);
    return true;
}

bool KeyMap::Unbind(u16 usage, GT_Keys key)
{
    if (!IsValidPointer(GetHostKeyName(usage)))
        return false;

    GT_Key_Map_Layer layer = GT_KEY_MAP_BUILT_IN;
    const GT_Key_Map_Entry& current = GetEntry(usage, &layer);
    GT_Key_Map_Entry entry = k_key_map_empty;
    entry.set = true;

    for (int i = 0; i < current.count; i++)
    {
        if (current.keys[i] != (u8)key)
            entry.keys[entry.count++] = current.keys[i];
    }

    if (entry.count == current.count)
        return false;

    if ((entry.count == 0) && (layer == GT_KEY_MAP_USER))
    {
        m_layers[GT_KEY_MAP_USER][usage] = k_key_map_empty;
        m_changed = true;
        return true;
    }

    SetUserEntry(usage, entry);
    return true;
}

bool KeyMap::RestoreBuiltIn(u16 usage)
{
    if (!IsValidPointer(GetHostKeyName(usage)))
        return false;

    if (KeepsUserEntry(usage))
    {
        m_layers[GT_KEY_MAP_USER][usage] = k_key_map_empty;
        m_changed = true;
        return true;
    }

    GT_Key_Map_Entry entry = m_layers[GT_KEY_MAP_BUILT_IN][usage];
    entry.set = true;
    SetUserEntry(usage, entry);
    return true;
}

bool KeyMap::IsChanged() const
{
    return m_changed;
}

void KeyMap::SetChanged(bool changed)
{
    m_changed = changed;
}

bool KeyMap::LoadFiles(const char* user_path, const char* game_text, std::string* error)
{
    std::string game_error;
    std::string text;

    SetEnabled(true);
    ClearLayer(GT_KEY_MAP_GAME);
    ClearLayer(GT_KEY_MAP_USER);
    SetChanged(false);

    if (!SetLayer(GT_KEY_MAP_GAME, game_text, &game_error))
        ClearLayer(GT_KEY_MAP_GAME);

    if (!IsValidPointer(user_path) || (user_path[0] == 0) ||
        !MediaFile::ReadFile(user_path, k_media_file_no_limit, &text, true))
        return true;

    if (SetLayer(GT_KEY_MAP_USER, text.c_str(), error))
        return true;

    ClearLayer(GT_KEY_MAP_USER);
    return false;
}

bool KeyMap::SaveUserFile(const char* path)
{
    if (!IsChanged() || !IsValidPointer(path) || (path[0] == 0))
        return true;

    SetChanged(false);

    if (IsLayerEmpty(GT_KEY_MAP_USER))
    {
        MediaFile::RemoveFile(path);
        return true;
    }

    std::string text = "[keys]\n" + GetLayer(GT_KEY_MAP_USER);
    return MediaFile::WriteFileAtomic(path, text.data(), text.size());
}

bool KeyMap::IsHostKey(u16 usage)
{
    return usage < GT_HOST_KEY_COUNT;
}

bool KeyMap::IsLayoutHostKey(u16 usage)
{
    if ((usage >= 0x04) && (usage <= 0x27))
        return true;

    return ((usage >= 0x2D) && (usage <= 0x34)) || ((usage >= 0x36) && (usage <= 0x38)) || (usage == 0x64);
}

int KeyMap::FindHostKey(const char* name)
{
    for (int i = 0; IsValidPointer(name) && (i < k_host_key_count); i++)
    {
        if (strcmp(name, k_host_keys[i].name) == 0)
            return k_host_keys[i].usage;
    }

    return -1;
}

const char* KeyMap::GetHostKeyName(u16 usage)
{
    for (int i = 0; i < k_host_key_count; i++)
    {
        if (k_host_keys[i].usage == usage)
            return k_host_keys[i].name;
    }

    return NULL;
}

const char* KeyMap::GetHostKeyLabel(u16 usage)
{
    for (int i = 0; i < k_host_key_count; i++)
    {
        if (k_host_keys[i].usage == usage)
            return k_host_keys[i].label;
    }

    return NULL;
}

int KeyMap::FindKey(const char* name)
{
    for (int i = 0; IsValidPointer(name) && (i < k_key_info_count); i++)
    {
        if (strcmp(name, k_key_infos[i].name) == 0)
            return k_key_infos[i].key;
    }

    return -1;
}

const char* KeyMap::GetKeyName(GT_Keys key)
{
    for (int i = 0; i < k_key_info_count; i++)
    {
        if (k_key_infos[i].key == key)
            return k_key_infos[i].name;
    }

    return NULL;
}

const char* KeyMap::GetKeyLabel(GT_Keys key)
{
    for (int i = 0; i < k_key_info_count; i++)
    {
        if (k_key_infos[i].key == key)
            return k_key_infos[i].label;
    }

    return NULL;
}

bool KeyMap::ParseEntry(const std::string& host, const std::string& value, u16* usage, GT_Key_Map_Entry* entry,
    std::string* error)
{
    std::string name = to_lower_ascii(trim_chars(host));
    int found = FindHostKey(name.c_str());

    if (found < 0)
    {
        *error = "Unknown host key " + name;
        return false;
    }

    *usage = (u16)found;
    *entry = k_key_map_empty;
    entry->set = true;
    std::string targets = to_lower_ascii(trim_chars(value));

    if (targets.empty())
    {
        *error = "No keys for " + name;
        return false;
    }

    if (targets == "none")
        return true;

    size_t start = 0;

    while (start <= targets.size())
    {
        size_t plus = targets.find('+', start);
        plus = plus == std::string::npos ? targets.size() : plus;
        std::string target = trim_chars(targets.substr(start, plus - start));
        start = plus + 1;
        int key = FindKey(target.c_str());

        if (key < 0)
        {
            *error = target.empty() ? "Empty key in " + name : "Unknown key " + target;
            return false;
        }

        if (entry->count == GT_KEY_MAP_TARGETS)
        {
            *error = "More than " + int_to_string(GT_KEY_MAP_TARGETS) + " keys for " + name;
            return false;
        }

        for (int i = 0; i < entry->count; i++)
        {
            if (entry->keys[i] == (u8)key)
            {
                *error = target + " is twice in " + name;
                return false;
            }
        }

        entry->keys[entry->count++] = (u8)key;
    }

    return true;
}

std::string KeyMap::FormatTargets(const GT_Key_Map_Entry& entry)
{
    if (entry.count == 0)
        return "none";

    std::string out;

    for (int i = 0; i < entry.count; i++)
    {
        const char* name = GetKeyName((GT_Keys)entry.keys[i]);
        out += std::string(i > 0 ? " + " : "") + (IsValidPointer(name) ? name : "none");
    }

    return out;
}

const GT_Key_Map_Entry& KeyMap::GetActiveEntry(u16 usage) const
{
    return m_enabled ? GetEntry(usage) : m_layers[GT_KEY_MAP_BUILT_IN][usage];
}

const GT_Key_Map_Entry& KeyMap::GetBaseEntry(GT_Key_Map_Layer layer, u16 usage) const
{
    for (int l = (int)layer - 1; l > GT_KEY_MAP_BUILT_IN; l--)
    {
        if (m_layers[l][usage].set)
            return m_layers[l][usage];
    }

    return m_layers[GT_KEY_MAP_BUILT_IN][usage];
}

void KeyMap::SetUserEntry(u16 usage, const GT_Key_Map_Entry& entry)
{
    if (!KeepsUserEntry(usage) && SameTargets(entry, GetBaseEntry(GT_KEY_MAP_USER, usage)))
        m_layers[GT_KEY_MAP_USER][usage] = k_key_map_empty;
    else
        m_layers[GT_KEY_MAP_USER][usage] = entry;

    m_changed = true;
}

bool KeyMap::KeepsUserEntry(u16 usage) const
{
    return (m_layout == GT_KEY_LAYOUT_CHARACTER) && IsLayoutHostKey(usage);
}

bool KeyMap::GetActiveLayoutTarget(u16 usage, int level, GT_Key_Map_Char* target) const
{
    if (m_enabled && !UsesLayout(usage))
        return false;

    return FindLayoutTarget(usage, level, target);
}

bool KeyMap::FindLayoutTarget(u16 usage, int level, GT_Key_Map_Char* target) const
{
    return GetLayoutTarget(usage, level, target) || ((level >= 2) && GetLayoutTarget(usage, level & 1, target));
}

void KeyMap::BuildLayoutTargets()
{
    memset(m_layout_targets, 0, sizeof(m_layout_targets));
    bool levels = false;

    for (int usage = 0; usage < GT_HOST_KEY_COUNT; usage++)
    {
        for (int level = 0; level < GT_KEY_MAP_LEVELS; level++)
        {
            GT_Keys key = GT_KEY_NONE;
            bool shift = false;
            u32 c = m_host_chars[usage][level];

            if ((c < 0x20) || !keyboard_char_key(c, &key, &shift))
                continue;

            m_layout_targets[usage][level].key = (u8)key;
            m_layout_targets[usage][level].shift = shift ? 1 : 0;
            levels = levels || (level >= 2);
        }
    }

    bool keys = false;

    for (int i = k_host_key_first_modifier; i < GT_HOST_KEY_COUNT; i++)
        keys = keys || m_level_host_keys[i];

    m_host_levels = levels && keys;
}

void KeyMap::PressHostTargets(u16 usage)
{
    if (m_blocked)
        return;

    GT_Key_Map_Char target;

    if (GetActiveLayoutTarget(usage, GetHostLevel(), &target))
    {
        bool shift = target.shift != 0;

        if (IsKeyDown(GT_KEY_SHIFT) != shift)
            SetShiftOverride(usage, shift);

        GT_Key_Map_Entry& targets = m_host_targets[usage];
        targets = k_key_map_empty;
        targets.set = true;
        targets.count = 1;
        targets.keys[0] = target.key;
        PressKey(target.key);
        return;
    }

    const GT_Key_Map_Entry& entry = GetActiveEntry(usage);
    m_host_targets[usage] = entry;

    for (int i = 0; i < entry.count; i++)
        PressKey(entry.keys[i]);
}

void KeyMap::FlushDeferredHostKeys(u16 usage)
{
    GT_Key_Map_Char target;
    int level = GetHostLevel();
    bool typed = (level >= 2) && (!m_enabled || UsesLayout(usage)) && GetLayoutTarget(usage, level, &target);

    for (int i = k_host_key_first_modifier; i < GT_HOST_KEY_COUNT; i++)
    {
        if (!m_host_deferred[i])
            continue;

        m_host_deferred[i] = false;

        if (!typed || !IsLevelHostKey((u16)i))
            PressHostTargets((u16)i);
    }
}

bool KeyMap::IsDeferredHostKey(u16 usage) const
{
    return (usage == k_host_key_left_gui) || (usage == k_host_key_right_gui) || IsLevelHostKey(usage);
}

bool KeyMap::IsLevelHostKey(u16 usage) const
{
    return HasHostLevels() && IsHostKey(usage) && m_level_host_keys[usage];
}

bool KeyMap::IsKeyDown(u8 key) const
{
    if ((key == GT_KEY_SHIFT) && (m_shift_owner != 0))
        return m_shift_forced;

    return m_references[key] > 0;
}

void KeyMap::SetShiftOverride(u16 usage, bool shift)
{
    bool down = IsKeyDown(GT_KEY_SHIFT);
    m_shift_owner = usage;
    m_shift_forced = shift;

    if ((down != shift) && IsValidPointer(m_callback))
        m_callback(m_context, GT_KEY_SHIFT, shift);
}

void KeyMap::ClearShiftOverride()
{
    if (m_shift_owner == 0)
        return;

    bool down = IsKeyDown(GT_KEY_SHIFT);
    m_shift_owner = 0;

    if ((down != IsKeyDown(GT_KEY_SHIFT)) && IsValidPointer(m_callback))
        m_callback(m_context, GT_KEY_SHIFT, !down);
}

void KeyMap::PressKey(u8 key)
{
    bool down = IsKeyDown(key);
    m_references[key]++;

    if (!down && IsKeyDown(key) && IsValidPointer(m_callback))
        m_callback(m_context, (GT_Keys)key, true);
}

void KeyMap::ReleaseKey(u8 key)
{
    if (m_references[key] == 0)
        return;

    bool down = IsKeyDown(key);
    m_references[key]--;

    if (down && !IsKeyDown(key) && IsValidPointer(m_callback))
        m_callback(m_context, (GT_Keys)key, false);
}

void KeyMap::SyncKey(u8 key)
{
    if (IsValidPointer(m_callback))
        m_callback(m_context, (GT_Keys)key, IsKeyDown(key));
}

bool KeyMap::IsModifier(u8 key)
{
    for (int i = 0; i < k_modifier_key_count; i++)
    {
        if ((u8)k_modifier_keys[i] == key)
            return true;
    }

    return false;
}

bool KeyMap::SameTargets(const GT_Key_Map_Entry& a, const GT_Key_Map_Entry& b)
{
    if (a.count != b.count)
        return false;

    for (int i = 0; i < a.count; i++)
    {
        if (a.keys[i] != b.keys[i])
            return false;
    }

    return true;
}

std::string KeyMap::FormatLayers(int first, int last) const
{
    std::string out;

    for (int usage = 0; usage < GT_HOST_KEY_COUNT; usage++)
    {
        const char* name = GetHostKeyName((u16)usage);

        for (int l = last; IsValidPointer(name) && (l >= first); l--)
        {
            if (!m_layers[l][usage].set)
                continue;

            out += std::string(name) + " = " + FormatTargets(m_layers[l][usage]) + "\n";
            break;
        }
    }

    return out;
}
