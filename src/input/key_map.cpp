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
#include "keyboard_keys.h"

static const GT_Key_Map_Entry k_key_map_empty = { false, 0, { 0, 0, 0, 0 } };

static std::string key_map_trim(const std::string& text)
{
    size_t start = text.find_first_not_of(" \t\r\n");

    if (start == std::string::npos)
        return "";

    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

static std::string key_map_lower(const std::string& text)
{
    std::string result(text);

    for (size_t i = 0; i < result.size(); i++)
        result[i] = ((result[i] >= 'A') && (result[i] <= 'Z')) ? (char)(result[i] + 32) : result[i];

    return result;
}

KeyMap::KeyMap()
{
    InitPointer(m_callback);
    InitPointer(m_context);
    memset(m_layers, 0, sizeof(m_layers));
    memset(m_host_targets, 0, sizeof(m_host_targets));
    memset(m_host_down, 0, sizeof(m_host_down));
    memset(m_host_deferred, 0, sizeof(m_host_deferred));
    memset(m_references, 0, sizeof(m_references));
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
        FlushDeferredHostKeys();

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
    m_host_deferred[k_host_key_left_gui] = false;
    m_host_deferred[k_host_key_right_gui] = false;
}

bool KeyMap::IsHostKeyDown(u16 usage) const
{
    return IsHostKey(usage) && m_host_down[usage];
}

void KeyMap::ReleaseAll()
{
    for (int i = GT_KEY_NONE + 1; i < GT_KEY_COUNT; i++)
    {
        if (m_references[i] == 0)
            continue;

        m_references[i] = 0;

        if (IsValidPointer(m_callback))
            m_callback(m_context, (GT_Keys)i, false);
    }

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
        std::string line = key_map_trim(data.substr(start, end - start));
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
                *error = "Line " + std::to_string(number) + ": " + message;

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

    const GT_Key_Map_Entry& current = GetEntry(usage);
    GT_Key_Map_Entry entry = k_key_map_empty;
    entry.set = true;

    for (int i = 0; i < current.count; i++)
    {
        if (current.keys[i] != (u8)key)
            entry.keys[entry.count++] = current.keys[i];
    }

    if (entry.count == current.count)
        return false;

    SetUserEntry(usage, entry);
    return true;
}

bool KeyMap::RestoreBuiltIn(u16 usage)
{
    if (!IsValidPointer(GetHostKeyName(usage)))
        return false;

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

bool KeyMap::IsHostKey(u16 usage)
{
    return usage < GT_HOST_KEY_COUNT;
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
    std::string name = key_map_lower(key_map_trim(host));
    int found = FindHostKey(name.c_str());

    if (found < 0)
    {
        *error = "Unknown host key " + name;
        return false;
    }

    *usage = (u16)found;
    *entry = k_key_map_empty;
    entry->set = true;
    std::string targets = key_map_lower(key_map_trim(value));

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
        std::string target = key_map_trim(targets.substr(start, plus - start));
        start = plus + 1;
        int key = FindKey(target.c_str());

        if (key < 0)
        {
            *error = target.empty() ? "Empty key in " + name : "Unknown key " + target;
            return false;
        }

        if (entry->count == GT_KEY_MAP_TARGETS)
        {
            *error = "More than " + std::to_string(GT_KEY_MAP_TARGETS) + " keys for " + name;
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
    if (SameTargets(entry, GetBaseEntry(GT_KEY_MAP_USER, usage)))
        m_layers[GT_KEY_MAP_USER][usage] = k_key_map_empty;
    else
        m_layers[GT_KEY_MAP_USER][usage] = entry;

    m_changed = true;
}

void KeyMap::PressHostTargets(u16 usage)
{
    if (m_blocked)
        return;

    const GT_Key_Map_Entry& entry = GetActiveEntry(usage);
    m_host_targets[usage] = entry;

    for (int i = 0; i < entry.count; i++)
        PressKey(entry.keys[i]);
}

void KeyMap::FlushDeferredHostKeys()
{
    if (m_host_deferred[k_host_key_left_gui])
    {
        m_host_deferred[k_host_key_left_gui] = false;
        PressHostTargets(k_host_key_left_gui);
    }

    if (m_host_deferred[k_host_key_right_gui])
    {
        m_host_deferred[k_host_key_right_gui] = false;
        PressHostTargets(k_host_key_right_gui);
    }
}

bool KeyMap::IsDeferredHostKey(u16 usage)
{
    return (usage == k_host_key_left_gui) || (usage == k_host_key_right_gui);
}

void KeyMap::PressKey(u8 key)
{
    if ((m_references[key]++ == 0) && IsValidPointer(m_callback))
        m_callback(m_context, (GT_Keys)key, true);
}

void KeyMap::ReleaseKey(u8 key)
{
    if ((m_references[key] > 0) && (--m_references[key] == 0) && IsValidPointer(m_callback))
        m_callback(m_context, (GT_Keys)key, false);
}

void KeyMap::SyncKey(u8 key)
{
    if (IsValidPointer(m_callback))
        m_callback(m_context, (GT_Keys)key, m_references[key] > 0);
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
