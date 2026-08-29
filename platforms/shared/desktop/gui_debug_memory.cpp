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

#include "gui_debug_memory.h"

#include <algorithm>
#include <climits>
#include <fstream>
#include <string>
#include <vector>
#include <SDL3/SDL.h>

#include "config.h"
#include "emu.h"
#include "gui.h"
#include "gui_debug_constants.h"
#include "gui_debug_memeditor.h"
#include "gui_debug_memory_provider.h"
#include "gui_filedialogs.h"
#include "imgui.h"

static const int MEMORY_VIEW_COUNT = 8;
static const int MEMORY_SETTINGS_MAX_RECORDS = 0x10000;
static const u32 MEMORY_SEARCH_MAX_SIZE = 0x04000000;
static const int MEMORY_SEARCH_MAX_VISIBLE_RESULTS = 10000;

struct MemoryBookmark
{
    GT_Debug_Memory_Address address;
    u32 end;
    char name[64];
};

struct MemoryWatch
{
    GT_Debug_Memory_Address address;
    char name[64];
    int size;
    int format;
    int endian;
    u64 value;
    u64 previous;
    u64 frozen_value;
    bool valid;
    bool freeze;
};

struct MemoryBreakpoint
{
    GT_Debug_Memory_Address address;
    u32 end;
    int pass_count;
    bool enabled;
    bool read;
    bool write;
    bool execute;
    bool cpu;
    bool dma;
    bool log_only;
    bool one_shot;
};

struct MemorySearchResult
{
    u32 address;
    u64 initial;
    u64 previous;
    u64 current;
};

struct MemoryViewSettings
{
    GT_Debug_Memory_Address source;
    MemEditor::Options options;
};

class MemorySearch
{
public:
    MemorySearch()
    {
        Reset();
    }

    void Reset()
    {
        memset(&m_source, 0, sizeof(m_source));
        m_start = 0;
        m_size = 0;
        m_width = 1;
        m_endian = 0;
        m_signed = false;
        m_aligned = true;
        m_candidate_count = 0;
        m_active = false;
        m_can_undo = false;
        m_initial.clear();
        m_previous.clear();
        m_candidates.clear();
        m_undo_previous.clear();
        m_undo_candidates.clear();
        m_results.clear();
    }

    bool Start(DebugMemoryProvider& provider, const GT_Debug_Memory_Address& source,
        u32 start, u32 size, int width, int endian, bool signed_values,
        bool aligned, bool known, u64 known_value)
    {
        Reset();
        if (size == 0 || size > MEMORY_SEARCH_MAX_SIZE ||
            (u64)start + size > (u64)provider.GetAddressLimit(source) + 1)
            return false;

        m_source = source;
        m_start = start;
        m_size = size;
        m_width = width;
        m_endian = endian;
        m_signed = signed_values;
        m_aligned = aligned;
        m_initial.resize(size);
        m_previous.resize(size);
        m_candidates.resize((size + 7) / 8, 0);
        std::vector<GT_Debug_Memory_Status> status(size);
        GT_Debug_Memory_Address address = source;
        address.address = start;
        provider.ReadBlock(address, &m_initial[0], &status[0], size, NULL);
        m_previous = m_initial;

        u32 step = aligned ? (u32)width : 1;
        for (u32 offset = 0; offset + width <= size; offset += step)
        {
            if (!ValueAvailable(status, offset))
                continue;
            u64 value = ReadValue(m_initial, offset);
            if (!known || value == MaskValue(known_value))
            {
                SetCandidate(offset, true);
                m_candidate_count++;
            }
        }
        m_active = true;
        BuildResults(m_initial, m_previous);
        return true;
    }

    bool Filter(DebugMemoryProvider& provider, int comparison, u64 value)
    {
        if (!m_active)
            return false;

        std::vector<u8> current(m_size);
        std::vector<GT_Debug_Memory_Status> status(m_size);
        GT_Debug_Memory_Address address = m_source;
        address.address = m_start;
        provider.ReadBlock(address, &current[0], &status[0], m_size, NULL);

        m_undo_previous = m_previous;
        m_undo_candidates = m_candidates;
        m_can_undo = true;
        m_candidate_count = 0;

        u32 step = m_aligned ? (u32)m_width : 1;
        for (u32 offset = 0; offset + m_width <= m_size; offset += step)
        {
            if (!IsCandidate(offset))
                continue;
            if (!ValueAvailable(status, offset))
            {
                SetCandidate(offset, false);
                continue;
            }

            u64 current_value = ReadValue(current, offset);
            u64 previous_value = ReadValue(m_previous, offset);
            u64 initial_value = ReadValue(m_initial, offset);
            bool keep = Compare(current_value, previous_value, initial_value,
                value, comparison);
            SetCandidate(offset, keep);
            if (keep)
                m_candidate_count++;
        }
        BuildResults(current, m_previous);
        m_previous.swap(current);
        return true;
    }

    void Undo()
    {
        if (!m_can_undo)
            return;
        m_previous.swap(m_undo_previous);
        m_candidates.swap(m_undo_candidates);
        m_candidate_count = 0;
        for (u32 offset = 0; offset < m_size; offset++)
        {
            if (IsCandidate(offset))
                m_candidate_count++;
        }
        m_can_undo = false;
        BuildResults(m_undo_previous, m_previous);
    }

    bool FindPattern(DebugMemoryProvider& provider,
        const GT_Debug_Memory_Address& source, u32 start, u32 size,
        const std::vector<u8>& pattern, const std::vector<u8>& mask)
    {
        Reset();
        if (pattern.empty() || pattern.size() != mask.size() || size == 0 ||
            size > MEMORY_SEARCH_MAX_SIZE || pattern.size() > size ||
            (u64)start + size > (u64)provider.GetAddressLimit(source) + 1)
            return false;

        m_source = source;
        m_start = start;
        m_size = size;
        m_width = 1;
        m_initial.resize(size);
        std::vector<GT_Debug_Memory_Status> status(size);
        GT_Debug_Memory_Address address = source;
        address.address = start;
        provider.ReadBlock(address, &m_initial[0], &status[0], size, NULL);

        for (u32 offset = 0; offset + pattern.size() <= size; offset++)
        {
            bool match = true;
            for (u32 i = 0; i < pattern.size(); i++)
            {
                if ((status[offset + i] != GT_DEBUG_MEMORY_VALID &&
                    status[offset + i] != GT_DEBUG_MEMORY_READ_ONLY) ||
                    (m_initial[offset + i] & mask[i]) != (pattern[i] & mask[i]))
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                MemorySearchResult result;
                result.address = start + offset;
                result.initial = 0;
                result.previous = 0;
                result.current = 0;
                m_candidate_count++;
                if ((int)m_results.size() < MEMORY_SEARCH_MAX_VISIBLE_RESULTS)
                    m_results.push_back(result);
            }
        }
        m_active = false;
        return true;
    }

    const GT_Debug_Memory_Address& GetSource() const
    {
        return m_source;
    }

    const std::vector<MemorySearchResult>& GetResults() const
    {
        return m_results;
    }

    u32 GetCandidateCount() const
    {
        return m_candidate_count;
    }

    bool IsActive() const
    {
        return m_active;
    }

    bool CanUndo() const
    {
        return m_can_undo;
    }

private:
    bool ValueAvailable(const std::vector<GT_Debug_Memory_Status>& status,
        u32 offset) const
    {
        for (int i = 0; i < m_width; i++)
        {
            GT_Debug_Memory_Status byte_status = status[offset + i];
            if (byte_status != GT_DEBUG_MEMORY_VALID &&
                byte_status != GT_DEBUG_MEMORY_READ_ONLY)
                return false;
        }
        return true;
    }

    u64 ReadValue(const std::vector<u8>& data, u32 offset) const
    {
        u64 value = 0;
        for (int i = 0; i < m_width; i++)
        {
            int source = m_endian == 0 ? i : m_width - i - 1;
            value |= (u64)data[offset + source] << (i * 8);
        }
        return value;
    }

    u64 MaskValue(u64 value) const
    {
        if (m_width >= 8)
            return value;
        return value & ((1ULL << (m_width * 8)) - 1);
    }

    s64 SignedValue(u64 value) const
    {
        switch (m_width)
        {
            case 1: return (s8)value;
            case 2: return (s16)value;
            case 4: return (s32)value;
            default: return (s64)value;
        }
    }

    bool Compare(u64 current, u64 previous, u64 initial, u64 specific,
        int comparison) const
    {
        current = MaskValue(current);
        previous = MaskValue(previous);
        initial = MaskValue(initial);
        specific = MaskValue(specific);
        if (m_signed)
        {
            s64 current_signed = SignedValue(current);
            s64 previous_signed = SignedValue(previous);
            s64 initial_signed = SignedValue(initial);
            s64 specific_signed = SignedValue(specific);
            switch (comparison)
            {
                case 0: return current_signed == specific_signed;
                case 1: return current_signed == previous_signed;
                case 2: return current_signed != previous_signed;
                case 3: return current_signed > previous_signed;
                case 4: return current_signed < previous_signed;
                case 5:
                {
                    if (specific_signed > 0 &&
                        previous_signed > LLONG_MAX - specific_signed)
                        return false;
                    if (specific_signed < 0 &&
                        previous_signed < LLONG_MIN - specific_signed)
                        return false;
                    return current_signed == previous_signed + specific_signed;
                }
                case 6: return current_signed > specific_signed;
                case 7: return current_signed < specific_signed;
                case 8: return current_signed == initial_signed;
                case 9: return current_signed != initial_signed;
                case 10: return current_signed > initial_signed;
                case 11: return current_signed < initial_signed;
                default: return false;
            }
        }

        switch (comparison)
        {
            case 0: return current == specific;
            case 1: return current == previous;
            case 2: return current != previous;
            case 3: return current > previous;
            case 4: return current < previous;
            case 5: return MaskValue(current - previous) == specific;
            case 6: return current > specific;
            case 7: return current < specific;
            case 8: return current == initial;
            case 9: return current != initial;
            case 10: return current > initial;
            case 11: return current < initial;
            default: return false;
        }
    }

    bool IsCandidate(u32 offset) const
    {
        return offset < m_size &&
            (m_candidates[offset >> 3] & (1U << (offset & 7))) != 0;
    }

    void SetCandidate(u32 offset, bool candidate)
    {
        u8 mask = (u8)(1U << (offset & 7));
        if (candidate)
            m_candidates[offset >> 3] |= mask;
        else
            m_candidates[offset >> 3] &= (u8)~mask;
    }

    void BuildResults(const std::vector<u8>& current,
        const std::vector<u8>& previous)
    {
        m_results.clear();
        u32 step = m_aligned ? (u32)m_width : 1;
        for (u32 offset = 0; offset + m_width <= m_size; offset += step)
        {
            if (!IsCandidate(offset))
                continue;
            MemorySearchResult result;
            result.address = m_start + offset;
            result.initial = ReadValue(m_initial, offset);
            result.previous = ReadValue(previous, offset);
            result.current = ReadValue(current, offset);
            m_results.push_back(result);
            if ((int)m_results.size() >= MEMORY_SEARCH_MAX_VISIBLE_RESULTS)
                break;
        }
    }

private:
    GT_Debug_Memory_Address m_source;
    u32 m_start;
    u32 m_size;
    int m_width;
    int m_endian;
    bool m_signed;
    bool m_aligned;
    u32 m_candidate_count;
    bool m_active;
    bool m_can_undo;
    std::vector<u8> m_initial;
    std::vector<u8> m_previous;
    std::vector<u8> m_candidates;
    std::vector<u8> m_undo_previous;
    std::vector<u8> m_undo_candidates;
    std::vector<MemorySearchResult> m_results;
};

static DebugMemoryProvider memory_provider;
static MemEditor memory_editor[MEMORY_VIEW_COUNT];
static int current_editor = 0;
static std::vector<MemoryBookmark> memory_bookmarks;
static std::vector<MemoryWatch> memory_watches;
static std::vector<MemoryBreakpoint> memory_breakpoints;
static MemorySearch memory_search;
static bool show_watches = false;
static bool show_search = false;
static bool show_breakpoints = false;
static bool show_fill_popup = false;
static char fill_value[3] = {};
static char search_start[16] = {};
static char search_size[16] = {};
static char search_value[24] = {};
static char search_pattern[1024] = {};
static int search_width = 0;
static int search_endian = 0;
static int search_comparison = 0;
static int search_pattern_type = 0;
static bool search_signed = false;
static bool search_aligned = true;
static bool search_known = false;
static u32 memory_media_crc = 0;
static bool memory_media_crc_valid = false;

static MemEditor* active_editor();
static int active_view_count();
static void new_editor_view();
static void draw_memory_menu();
static void draw_editor_tab(MemEditor& editor, int index);
static void draw_region_browser(MemEditor& editor);
static void draw_inspector(MemEditor& editor);
static void draw_watches_window();
static void draw_search_window();
static void draw_breakpoints_window();
static void process_editor_requests(MemEditor& editor);
static void add_bookmark(const GT_Debug_Memory_Address& address, u32 end);
static void add_watch(const GT_Debug_Memory_Address& address);
static void add_breakpoint(const GT_Debug_Memory_Address& address, u32 end);
static bool read_value(const GT_Debug_Memory_Address& address, int size,
    int endian, u64& value, GT_Debug_Memory_Status& status);
static void format_address(const GT_Debug_Memory_Address& address,
    char* text, size_t text_size);
static void format_value(u64 value, int size, int format, char* text,
    size_t text_size);
static int watch_size_bytes(int size);
static void navigate_to(const GT_Debug_Memory_Address& address);
static bool parse_u32(const char* text, u32& value);
static bool parse_u64(const char* text, u64& value);
static bool parse_pattern(const char* text, int type,
    std::vector<u8>& pattern, std::vector<u8>& mask);
static bool read_data(std::istream& stream, void* data, size_t size);
static bool read_count(std::istream& stream, int& count, size_t record_size);

void gui_debug_memory_init(void)
{
    memory_provider.Init();
    for (int i = 0; i < MEMORY_VIEW_COUNT; i++)
    {
        memory_editor[i].Init(&memory_provider, i);
        memory_editor[i].SetAvailable(i == 0);
    }
    current_editor = 0;
    gui_debug_memory_reset();
}

void gui_debug_memory_destroy(void)
{
    memory_bookmarks.clear();
    memory_watches.clear();
    memory_breakpoints.clear();
    memory_search.Reset();
}

void gui_debug_memory_reset(void)
{
    GeartownsCore* core = emu_get_core();
    bool media_ready = IsValidPointer(core) && IsValidPointer(core->GetMedia()) &&
        core->GetMedia()->IsReady();
    u32 media_crc = media_ready ? core->GetMedia()->GetCRC() : 0;
    if (!memory_media_crc_valid || media_crc != memory_media_crc)
    {
        memory_bookmarks.clear();
        memory_watches.clear();
        memory_breakpoints.clear();
        memory_media_crc = media_crc;
        memory_media_crc_valid = true;
    }

    memory_provider.Reset();
    for (int i = 0; i < MEMORY_VIEW_COUNT; i++)
    {
        if (memory_editor[i].IsAvailable())
            memory_editor[i].RequestRefresh();
    }
    memory_search.Reset();

    int region_count = memory_provider.GetRegionCount();
    if (region_count > 0)
    {
        GT_Debug_Memory_Region region;
        if (memory_provider.GetRegion(0, region))
        {
            GT_Debug_Memory_Address source = {};
            source.space = GT_DEBUG_MEMORY_REGION;
            source.region = region.id;
            source.segment_register = -1;
            memory_editor[0].SetSource(source);
        }
    }
}

void gui_debug_memory_update(void)
{
    memory_provider.Update();
    if (memory_provider.ConsumeChanged())
    {
        for (int i = 0; i < MEMORY_VIEW_COUNT; i++)
        {
            if (memory_editor[i].IsAvailable())
                memory_editor[i].RequestRefresh();
        }
    }

    for (size_t i = 0; config_debug.debug && i < memory_watches.size(); i++)
    {
        MemoryWatch& watch = memory_watches[i];
        watch.previous = watch.value;
        GT_Debug_Memory_Status status;
        watch.valid = read_value(watch.address, watch_size_bytes(watch.size),
            watch.endian, watch.value, status);
        if (watch.freeze && watch.valid && watch.value != watch.frozen_value)
        {
            int bytes = watch_size_bytes(watch.size);
            u8 data[8];
            for (int b = 0; b < bytes; b++)
            {
                int destination = watch.endian == 0 ? b : bytes - b - 1;
                data[destination] = (u8)(watch.frozen_value >> (b * 8));
            }
            memory_provider.QueueWrite(watch.address, data, bytes);
        }
    }

    for (int i = 0; i < MEMORY_VIEW_COUNT; i++)
    {
        if (memory_editor[i].IsAvailable())
            memory_editor[i].Update();
    }
}

void gui_debug_window_memory(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(740, 520), ImGuiCond_FirstUseEver);
    ImGui::Begin("Memory Workspace", &config_debug.show_memory,
        ImGuiWindowFlags_MenuBar);

    draw_memory_menu();

    if (ImGui::BeginTabBar("##memory_views",
        ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs))
    {
        for (int i = 0; i < MEMORY_VIEW_COUNT; i++)
        {
            if (memory_editor[i].IsAvailable())
                draw_editor_tab(memory_editor[i], i);
        }
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
            new_editor_view();
        ImGui::EndTabBar();
    }

    if (show_fill_popup)
        ImGui::OpenPopup("Fill Selection");
    if (ImGui::BeginPopupModal("Fill Selection", &show_fill_popup,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Hex value:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(40.0f);
        bool fill = ImGui::InputText("##fill_value", fill_value,
            sizeof(fill_value), ImGuiInputTextFlags_CharsHexadecimal |
            ImGuiInputTextFlags_CharsUppercase |
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_AutoSelectAll);
        ImGui::SameLine();
        fill = ImGui::Button("Fill") || fill;
        if (fill)
        {
            u8 value = 0;
            if (parse_hex_string(fill_value, strlen(fill_value), &value) &&
                IsValidPointer(active_editor()))
                active_editor()->FillSelection(value);
            show_fill_popup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            show_fill_popup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    const char* provider_message = memory_provider.GetLastMessage();
    if (provider_message[0] != 0)
        ImGui::TextColored(violet, "%s", provider_message);

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_memory_auxiliary_windows(void)
{
    if (show_watches)
        draw_watches_window();
    if (show_search)
        draw_search_window();
    if (show_breakpoints)
        draw_breakpoints_window();
}

void gui_debug_memory_copy(void)
{
    if (IsValidPointer(active_editor()))
        active_editor()->CopySelection();
}

void gui_debug_memory_paste(void)
{
    if (IsValidPointer(active_editor()))
        active_editor()->PasteSelection();
}

void gui_debug_memory_save_dump(const char* file_path)
{
    MemEditor* editor = active_editor();
    if (!IsValidPointer(editor) || !IsValidPointer(file_path))
        return;

    u32 start = 0;
    u32 end = 0;
    editor->GetSelection(start, end);
    u64 size64 = (u64)end - start + 1;
    if (size64 == 0 || size64 > MEMORY_SEARCH_MAX_SIZE)
        return;
    u32 size = (u32)size64;

    std::vector<u8> data(size);
    std::vector<GT_Debug_Memory_Status> status(size);
    GT_Debug_Memory_Address address = editor->GetSource();
    address.address = start;
    memory_provider.ReadBlock(address, &data[0], &status[0], size, NULL);
    for (u32 i = 0; i < size; i++)
    {
        if (status[i] != GT_DEBUG_MEMORY_VALID &&
            status[i] != GT_DEBUG_MEMORY_READ_ONLY)
            return;
    }

    FILE* file = fopen_utf8(file_path, "wb");
    if (IsValidPointer(file))
    {
        fwrite(&data[0], 1, size, file);
        fclose(file);
    }
}

void gui_debug_memory_load_dump(const char* file_path)
{
    MemEditor* editor = active_editor();
    if (!IsValidPointer(editor) || !IsValidPointer(file_path))
        return;

    std::ifstream file;
    open_ifstream_utf8(file, file_path, std::ios::binary);
    if (!file.is_open())
        return;
    file.seekg(0, std::ios::end);
    std::streampos position = file.tellg();
    std::streamoff file_size = position;
    if (position == std::streampos(-1) || file_size <= 0 ||
        (u64)file_size > MEMORY_SEARCH_MAX_SIZE)
    {
        file.close();
        return;
    }
    u32 size = (u32)file_size;
    std::vector<u8> data(size);
    file.seekg(0, std::ios::beg);
    file.read((char*)&data[0], size);
    bool valid = !file.fail();
    file.close();
    if (!valid)
        return;

    u32 start = 0;
    u32 end = 0;
    editor->GetSelection(start, end);
    GT_Debug_Memory_Address address = editor->GetSource();
    address.address = start;
    memory_provider.QueueWrite(address, &data[0], size);
}

static MemEditor* active_editor()
{
    if (current_editor < 0 || current_editor >= MEMORY_VIEW_COUNT ||
        !memory_editor[current_editor].IsAvailable())
        return NULL;
    return &memory_editor[current_editor];
}

static int active_view_count()
{
    int count = 0;
    for (int i = 0; i < MEMORY_VIEW_COUNT; i++)
    {
        if (memory_editor[i].IsAvailable())
            count++;
    }
    return count;
}

static void new_editor_view()
{
    for (int i = 0; i < MEMORY_VIEW_COUNT; i++)
    {
        if (!memory_editor[i].IsAvailable())
        {
            MemEditor* current = active_editor();
            memory_editor[i].SetAvailable(true);
            memory_editor[i].Reset();
            if (IsValidPointer(current))
            {
                memory_editor[i].SetOptions(current->GetOptions());
                memory_editor[i].SetSource(current->GetSource());
            }
            current_editor = i;
            return;
        }
    }
}

static void draw_memory_menu()
{
    ImGui::BeginMenuBar();
    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Export Selection..."))
            gui_file_dialog_save_memory_dump();
        if (ImGui::MenuItem("Import Binary at Selection..."))
            gui_file_dialog_load_memory_dump();
        ImGui::Separator();
        if (ImGui::MenuItem("Save Debug Settings..."))
            gui_file_dialog_save_debug_settings();
        if (ImGui::MenuItem("Load Debug Settings..."))
            gui_file_dialog_load_debug_settings();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, memory_provider.CanUndo()))
            memory_provider.RequestUndo();
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, memory_provider.CanRedo()))
            memory_provider.RequestRedo();
        ImGui::Separator();
        if (ImGui::MenuItem("Copy", "Ctrl+C"))
            gui_debug_memory_copy();
        if (ImGui::MenuItem("Paste", "Ctrl+V"))
            gui_debug_memory_paste();
        if (ImGui::MenuItem("Fill Selection..."))
        {
            fill_value[0] = 0;
            show_fill_popup = true;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View"))
    {
        if (ImGui::MenuItem("New Memory View"))
            new_editor_view();
        if (ImGui::MenuItem("Refresh"))
        {
            if (IsValidPointer(active_editor()))
                active_editor()->Refresh();
        }
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Search"))
        show_search = true;
    if (ImGui::MenuItem("Watches"))
        show_watches = true;
    if (ImGui::MenuItem("Breakpoints"))
        show_breakpoints = true;
    ImGui::EndMenuBar();
}

static void draw_editor_tab(MemEditor& editor, int index)
{
    bool open = true;
    bool* open_pointer = active_view_count() > 1 ? &open : NULL;
    ImGuiTabItemFlags flags = current_editor == index ?
        ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    char tab_label[128];
    snprintf(tab_label, sizeof(tab_label), "%s###memory_tab_%d",
        editor.GetTitle(), index);
    if (ImGui::BeginTabItem(tab_label, open_pointer, flags))
    {
        current_editor = index;
        ImGui::PushID(index);
        if (ImGui::BeginTable("##memory_layout", 3,
            ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Sources", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Memory", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 250.0f);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            draw_region_browser(editor);
            ImGui::TableNextColumn();
            ImGui::PushFont(gui_default_font);
            editor.Draw();
            ImGui::PopFont();
            process_editor_requests(editor);
            ImGui::TableNextColumn();
            draw_inspector(editor);
            ImGui::EndTable();
        }
        ImGui::PopID();
        ImGui::EndTabItem();
    }
    if (!open)
    {
        editor.SetAvailable(false);
        for (int i = 0; i < MEMORY_VIEW_COUNT; i++)
        {
            if (memory_editor[i].IsAvailable())
            {
                current_editor = i;
                break;
            }
        }
    }
}

static void draw_region_browser(MemEditor& editor)
{
    ImGui::BeginChild("##memory_sources", ImVec2(0, -1));
    ImGui::TextColored(yellow, "ADDRESS SPACES");
    for (int i = 0; i < GT_DEBUG_MEMORY_SPACE_COUNT; i++)
    {
        if (i == GT_DEBUG_MEMORY_REGION)
            continue;
        GT_Debug_Memory_Space space = (GT_Debug_Memory_Space)i;
        bool selected = editor.GetSource().space == space;
        if (ImGui::Selectable(DebugMemoryProvider::GetSpaceName(space), selected))
        {
            GT_Debug_Memory_Address source = {};
            source.space = space;
            source.segment_register = space == GT_DEBUG_MEMORY_LOGICAL ?
                I386_SEGMENT_CS : -1;
            editor.SetSource(source);
        }
    }

    ImGui::Separator();
    ImGui::TextColored(yellow, "REGIONS");
    int region_count = memory_provider.GetRegionCount();
    for (int i = 0; i < region_count; i++)
    {
        GT_Debug_Memory_Region region;
        if (!memory_provider.GetRegion(i, region))
            continue;
        bool selected = editor.GetSource().space == GT_DEBUG_MEMORY_REGION &&
            editor.GetSource().region == region.id;
        if (ImGui::Selectable(region.name, selected))
        {
            GT_Debug_Memory_Address source = {};
            source.space = GT_DEBUG_MEMORY_REGION;
            source.segment_register = -1;
            source.region = region.id;
            editor.SetSource(source);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Size: %u bytes", region.size);
            if ((region.flags & GT_DEBUG_REGION_MAPPED) != 0)
                ImGui::Text("Bus: %08X-%08X", region.physical_base,
                    region.physical_base + region.size - 1);
            ImGui::Text("%s%s%s", (region.flags & GT_DEBUG_REGION_READABLE) ? "R" : "-",
                (region.flags & GT_DEBUG_REGION_WRITABLE) ? "W" : "-",
                (region.flags & GT_DEBUG_REGION_EXECUTABLE) ? "X" : "-");
            ImGui::EndTooltip();
        }
    }

    if (!memory_bookmarks.empty())
    {
        ImGui::Separator();
        ImGui::TextColored(yellow, "BOOKMARKS");
        int remove = -1;
        for (size_t i = 0; i < memory_bookmarks.size(); i++)
        {
            MemoryBookmark& bookmark = memory_bookmarks[i];
            ImGui::PushID((int)i);
            if (ImGui::Selectable(bookmark.name))
            {
                editor.SetSource(bookmark.address);
                editor.JumpToAddress(bookmark.address.address);
                editor.SetSelection(bookmark.address.address, bookmark.end);
            }
            if (ImGui::BeginPopupContextItem())
            {
                ImGui::Text("Name:");
                ImGui::SetNextItemWidth(180.0f);
                ImGui::InputText("##bookmark_name", bookmark.name,
                    sizeof(bookmark.name));
                if (ImGui::MenuItem("Remove Bookmark"))
                    remove = (int)i;
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        if (remove >= 0)
            memory_bookmarks.erase(memory_bookmarks.begin() + remove);
    }
    ImGui::EndChild();
}

static void draw_inspector(MemEditor& editor)
{
    ImGui::BeginChild("##memory_inspector", ImVec2(0, -1));
    u32 start = 0;
    u32 end = 0;
    editor.GetSelection(start, end);
    GT_Debug_Memory_Address address = editor.GetSource();
    address.address = start;

    char address_text[128];
    format_address(address, address_text, sizeof(address_text));
    ImGui::TextColored(cyan, "%s", address_text);
    ImGui::Text("Length: %llu", (unsigned long long)((u64)end - start + 1));

    MemEditor::Options options = editor.GetOptions();
    GT_Debug_Memory_Status status;
    u64 value = 0;
    ImGui::Separator();
    ImGui::TextColored(yellow, "DATA INSPECTOR");
    const int sizes[] = { 1, 2, 4, 8 };
    for (int i = 0; i < 4; i++)
    {
        int size = sizes[i];
        if (read_value(address, size, options.preview_endian, value, status))
        {
            int digits = size * 2;
            ImGui::TextColored(yellow, "%2d-bit", size * 8);
            ImGui::SameLine(58.0f);
            ImGui::Text("0x%0*llX  %llu  %lld", digits,
                (unsigned long long)value, (unsigned long long)value,
                (long long)(size == 1 ? (s64)(s8)value :
                size == 2 ? (s64)(s16)value :
                size == 4 ? (s64)(s32)value : (s64)value));
            if (size == 4)
            {
                u32 raw = (u32)value;
                float floating;
                memcpy(&floating, &raw, sizeof(floating));
                ImGui::Text("        float: %.9g", floating);
            }
            else if (size == 8)
            {
                double floating;
                memcpy(&floating, &value, sizeof(floating));
                ImGui::Text("        double: %.17g", floating);
            }
        }
        else
        {
            ImGui::TextColored(red, "%2d-bit  unavailable", size * 8);
        }
    }

    if (read_value(address, 1, 0, value, status))
    {
        char bits[12];
        for (int i = 0; i < 8; i++)
            bits[i + (i >= 4 ? 1 : 0)] = (value & (1U << (7 - i))) ? '1' : '0';
        bits[4] = ' ';
        bits[9] = 0;
        ImGui::Text("Bits: %s", bits);
    }

    u8 string_data[65];
    GT_Debug_Memory_Status string_status[64];
    memory_provider.ReadBlock(address, string_data, string_status, 64, NULL);
    int string_length = 0;
    while (string_length < 64 && string_data[string_length] != 0 &&
        (string_status[string_length] == GT_DEBUG_MEMORY_VALID ||
        string_status[string_length] == GT_DEBUG_MEMORY_READ_ONLY))
        string_length++;
    string_data[string_length] = 0;
    char ascii[65];
    for (int i = 0; i < string_length; i++)
        ascii[i] = string_data[i] >= 32 && string_data[i] < 127 ?
            (char)string_data[i] : '.';
    ascii[string_length] = 0;
    ImGui::TextColored(violet, "ASCII: %s", ascii);
    char* shift_jis = SDL_iconv_string("UTF-8", "SHIFT-JIS",
        (const char*)string_data, (size_t)string_length + 1);
    if (IsValidPointer(shift_jis))
    {
        ImGui::TextColored(violet, "SJIS:  %s", shift_jis);
        SDL_free(shift_jis);
    }

    ImGui::Separator();
    ImGui::TextColored(yellow, "TRANSLATION");
    GT_Debug_Memory_Translation translation;
    bool translated = memory_provider.Translate(address, translation);
    if (translation.logical_valid)
    {
        ImGui::Text("Logical:  %04X:%08X", translation.segment,
            translation.offset);
        ImGui::Text("Segment:  base %08X limit %08X",
            translation.segment_base, translation.segment_limit);
    }
    if (translation.linear_valid)
        ImGui::Text("Linear:   %08X", translation.linear);
    if (translation.page_directory_entry != 0 ||
        translation.page_table_entry != 0)
    {
        ImGui::Text("PDE/PTE:  %08X / %08X",
            translation.page_directory_entry, translation.page_table_entry);
        ImGui::Text("Page flags: %08X", translation.page_flags);
    }
    if (translation.physical_valid)
        ImGui::Text("Physical: %08X", translation.physical);
    if (translation.bus_valid)
        ImGui::Text("Bus:      %08X", translation.bus);
    if (translation.region_valid)
        ImGui::Text("Region:   %s + %08X", translation.region_name,
            translation.region_offset);
    if (!translated && translation.reason[0] != 0)
        ImGui::TextColored(red, "%s", translation.reason);

    ImGui::Separator();
    ImGui::TextDisabled("All displayed reads are passive.");
    ImGui::TextDisabled("Edits are queued to a safe point.");
    ImGui::EndChild();
}

static void process_editor_requests(MemEditor& editor)
{
    GT_Debug_Memory_Address address;
    u32 end = 0;
    if (editor.TakeBookmarkRequest(address, end))
        add_bookmark(address, end);
    if (editor.TakeWatchRequest(address))
        add_watch(address);
    if (editor.TakeBreakpointRequest(address, end))
        add_breakpoint(address, end);
}

static void add_bookmark(const GT_Debug_Memory_Address& address, u32 end)
{
    MemoryBookmark bookmark;
    memset(&bookmark, 0, sizeof(bookmark));
    bookmark.address = address;
    bookmark.end = end;
    snprintf(bookmark.name, sizeof(bookmark.name), "Bookmark_%08X",
        address.address);
    memory_bookmarks.push_back(bookmark);
}

static void add_watch(const GT_Debug_Memory_Address& address)
{
    MemoryWatch watch;
    memset(&watch, 0, sizeof(watch));
    watch.address = address;
    snprintf(watch.name, sizeof(watch.name), "Watch_%08X", address.address);
    watch.size = 0;
    watch.format = 0;
    watch.endian = 0;
    GT_Debug_Memory_Status status;
    watch.valid = read_value(watch.address, 1, 0, watch.value, status);
    watch.previous = watch.value;
    watch.frozen_value = watch.value;
    memory_watches.push_back(watch);
    show_watches = true;
}

static void add_breakpoint(const GT_Debug_Memory_Address& address, u32 end)
{
    MemoryBreakpoint breakpoint;
    memset(&breakpoint, 0, sizeof(breakpoint));
    breakpoint.address = address;
    breakpoint.end = end;
    breakpoint.enabled = true;
    breakpoint.read = true;
    breakpoint.write = true;
    breakpoint.cpu = true;
    breakpoint.dma = true;
    memory_breakpoints.push_back(breakpoint);
    show_breakpoints = true;
}

static void draw_watches_window()
{
    ImGui::SetNextWindowSize(ImVec2(650, 360), ImGuiCond_FirstUseEver);
    ImGui::Begin("Memory Watches", &show_watches);
    if (ImGui::Button("Add Current Selection") && IsValidPointer(active_editor()))
    {
        u32 start = 0;
        u32 end = 0;
        active_editor()->GetSelection(start, end);
        GT_Debug_Memory_Address address = active_editor()->GetSource();
        address.address = start;
        add_watch(address);
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove All"))
        memory_watches.clear();

    if (ImGui::BeginTable("##memory_watches", 7,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupColumn("Format", ImGuiTableColumnFlags_WidthFixed, 85.0f);
        ImGui::TableSetupColumn("Endian", ImGuiTableColumnFlags_WidthFixed, 65.0f);
        ImGui::TableSetupColumn("Freeze", ImGuiTableColumnFlags_WidthFixed, 48.0f);
        ImGui::TableSetupColumn("Name");
        ImGui::TableHeadersRow();

        int remove = -1;
        for (size_t i = 0; i < memory_watches.size(); i++)
        {
            MemoryWatch& watch = memory_watches[i];
            ImGui::PushID((int)i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            char address[128];
            format_address(watch.address, address, sizeof(address));
            if (ImGui::Selectable(address, false))
                navigate_to(watch.address);
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Remove Watch"))
                    remove = (int)i;
                ImGui::EndPopup();
            }

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##size", &watch.size, "8 bit\0" "16 bit\0" "32 bit\0" "64 bit\0\0");
            ImGui::TableNextColumn();
            if (watch.valid)
            {
                char value[96];
                format_value(watch.value, watch_size_bytes(watch.size), watch.format,
                    value, sizeof(value));
                ImGui::TextColored(watch.value != watch.previous ? orange :
                    white, "%s", value);
            }
            else
                ImGui::TextColored(red, "unavailable");

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##format", &watch.format,
                "Hex\0Unsigned\0Signed\0Binary\0ASCII\0\0");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##endian", &watch.endian, "LE\0BE\0\0");
            ImGui::TableNextColumn();
            bool freeze = watch.freeze;
            if (ImGui::Checkbox("##freeze", &freeze))
            {
                watch.freeze = freeze;
                watch.frozen_value = watch.value;
            }
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##name", watch.name, sizeof(watch.name));
            ImGui::PopID();
        }
        if (remove >= 0)
            memory_watches.erase(memory_watches.begin() + remove);
        ImGui::EndTable();
    }
    ImGui::TextDisabled("Freeze currently reapplies at debugger safe points;");
    ImGui::TextDisabled("bus-level write filtering will activate when Memory hooks exist.");
    ImGui::End();
}

static void draw_search_window()
{
    ImGui::SetNextWindowSize(ImVec2(620, 520), ImGuiCond_FirstUseEver);
    ImGui::Begin("Memory Search", &show_search);
    MemEditor* editor = active_editor();
    if (!IsValidPointer(editor))
    {
        ImGui::TextDisabled("No memory view is active.");
        ImGui::End();
        return;
    }

    if (search_start[0] == 0 || search_size[0] == 0)
    {
        u32 start = editor->GetWindowBase();
        u32 size = editor->GetWindowSize();
        if (editor->GetSource().space == GT_DEBUG_MEMORY_REGION)
        {
            GT_Debug_Memory_Region region;
            if (memory_provider.GetRegionById(editor->GetSource().region, region) &&
                region.size <= MEMORY_SEARCH_MAX_SIZE)
            {
                start = 0;
                size = region.size;
            }
        }
        snprintf(search_start, sizeof(search_start), "%08X", start);
        snprintf(search_size, sizeof(search_size), "%X", size);
    }

    char source_name[128];
    GT_Debug_Memory_Address source = editor->GetSource();
    source.address = 0;
    format_address(source, source_name, sizeof(source_name));
    ImGui::TextColored(cyan, "Source: %s", source_name);
    ImGui::Text("Start:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::InputText("##search_start", search_start, sizeof(search_start),
        ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
    ImGui::SameLine();
    ImGui::Text("Size:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::InputText("##search_size", search_size, sizeof(search_size),
        ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);

    if (ImGui::BeginTabBar("##search_tabs"))
    {
        if (ImGui::BeginTabItem("Numeric"))
        {
            ImGui::SetNextItemWidth(100.0f);
            ImGui::Combo("Width", &search_width, "8 bit\0" "16 bit\0" "32 bit\0" "64 bit\0\0");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            ImGui::Combo("Endian", &search_endian, "Little\0Big\0\0");
            ImGui::Checkbox("Signed", &search_signed);
            ImGui::SameLine();
            ImGui::Checkbox("Aligned", &search_aligned);
            ImGui::SameLine();
            ImGui::Checkbox("Known initial value", &search_known);
            ImGui::SetNextItemWidth(130.0f);
            ImGui::InputText("Value", search_value, sizeof(search_value),
                ImGuiInputTextFlags_AutoSelectAll);

            if (ImGui::Button("New Search"))
            {
                u32 start = 0;
                u32 size = 0;
                u64 value = 0;
                if (parse_u32(search_start, start) && parse_u32(search_size, size) &&
                    (!search_known || parse_u64(search_value, value)))
                {
                    int widths[] = { 1, 2, 4, 8 };
                    memory_search.Start(memory_provider, editor->GetSource(), start,
                        size, widths[search_width], search_endian, search_signed,
                        search_aligned, search_known, value);
                }
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(155.0f);
            ImGui::Combo("##comparison", &search_comparison,
                "Equal value\0Unchanged\0Changed\0Increased\0Decreased\0"
                "Changed by\0Greater than\0Less than\0Equal initial\0"
                "Changed from initial\0Increased from initial\0"
                "Decreased from initial\0\0");
            ImGui::SameLine();
            if (ImGui::Button("Filter") && memory_search.IsActive())
            {
                u64 value = 0;
                if ((search_comparison != 0 && search_comparison != 5 &&
                    search_comparison != 6 && search_comparison != 7) ||
                    parse_u64(search_value, value))
                {
                    memory_search.Filter(memory_provider, search_comparison, value);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Undo Filter") && memory_search.CanUndo())
                memory_search.Undo();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Pattern / Text"))
        {
            ImGui::SetNextItemWidth(125.0f);
            ImGui::Combo("Type", &search_pattern_type,
                "Hex + wildcards\0ASCII text\0Shift-JIS text\0\0");
            ImGui::InputTextMultiline("##search_pattern", search_pattern,
                sizeof(search_pattern), ImVec2(-1, 70.0f));
            if (search_pattern_type == 0)
                ImGui::TextDisabled("Example: 48 8B ?? ?? A? FF");
            if (ImGui::Button("Find All"))
            {
                u32 start = 0;
                u32 size = 0;
                std::vector<u8> pattern;
                std::vector<u8> mask;
                if (parse_u32(search_start, start) && parse_u32(search_size, size) &&
                    parse_pattern(search_pattern, search_pattern_type, pattern, mask))
                {
                    memory_search.FindPattern(memory_provider, editor->GetSource(),
                        start, size, pattern, mask);
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::TextColored(yellow, "%u candidates%s",
        memory_search.GetCandidateCount(),
        memory_search.GetCandidateCount() > MEMORY_SEARCH_MAX_VISIBLE_RESULTS ?
        " (first 10000 shown)" : "");

    if (ImGui::BeginTable("##search_results", 4,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
        ImVec2(0, -1)))
    {
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Current");
        ImGui::TableSetupColumn("Previous");
        ImGui::TableSetupColumn("Initial");
        ImGui::TableHeadersRow();
        const std::vector<MemorySearchResult>& results = memory_search.GetResults();
        ImGuiListClipper clipper;
        clipper.Begin((int)results.size());
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                const MemorySearchResult& result = results[row];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                char address[16];
                snprintf(address, sizeof(address), "%08X", result.address);
                if (ImGui::Selectable(address, false,
                    ImGuiSelectableFlags_SpanAllColumns))
                {
                    GT_Debug_Memory_Address target = memory_search.GetSource();
                    target.address = result.address;
                    navigate_to(target);
                }
                ImGui::TableNextColumn();
                ImGui::Text("%llX", (unsigned long long)result.current);
                ImGui::TableNextColumn();
                ImGui::Text("%llX", (unsigned long long)result.previous);
                ImGui::TableNextColumn();
                ImGui::Text("%llX", (unsigned long long)result.initial);
            }
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

static void draw_breakpoints_window()
{
    ImGui::SetNextWindowSize(ImVec2(720, 360), ImGuiCond_FirstUseEver);
    ImGui::Begin("Memory Breakpoints", &show_breakpoints);
    ImGui::TextColored(violet,
        "Breakpoint definitions are ready; Memory/I386 execution hooks are not implemented yet.");
    if (ImGui::Button("Add Current Selection") && IsValidPointer(active_editor()))
    {
        u32 start = 0;
        u32 end = 0;
        active_editor()->GetSelection(start, end);
        GT_Debug_Memory_Address address = active_editor()->GetSource();
        address.address = start;
        add_breakpoint(address, end);
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove All"))
        memory_breakpoints.clear();

    if (ImGui::BeginTable("##memory_breakpoints", 10,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY))
    {
        const char* headings[] = {
            "On", "Range", "R", "W", "X", "CPU", "DMA", "Log", "Once", "Pass"
        };
        for (int i = 0; i < 10; i++)
            ImGui::TableSetupColumn(headings[i]);
        ImGui::TableHeadersRow();
        int remove = -1;
        for (size_t i = 0; i < memory_breakpoints.size(); i++)
        {
            MemoryBreakpoint& breakpoint = memory_breakpoints[i];
            ImGui::PushID((int)i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Checkbox("##enabled", &breakpoint.enabled);
            ImGui::TableNextColumn();
            char start[128];
            format_address(breakpoint.address, start, sizeof(start));
            char range[180];
            snprintf(range, sizeof(range), "%s-%08X", start, breakpoint.end);
            if (ImGui::Selectable(range, false))
                navigate_to(breakpoint.address);
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Remove Breakpoint"))
                    remove = (int)i;
                ImGui::EndPopup();
            }
            ImGui::TableNextColumn(); ImGui::Checkbox("##read", &breakpoint.read);
            ImGui::TableNextColumn(); ImGui::Checkbox("##write", &breakpoint.write);
            ImGui::TableNextColumn(); ImGui::Checkbox("##execute", &breakpoint.execute);
            ImGui::TableNextColumn(); ImGui::Checkbox("##cpu", &breakpoint.cpu);
            ImGui::TableNextColumn(); ImGui::Checkbox("##dma", &breakpoint.dma);
            ImGui::TableNextColumn(); ImGui::Checkbox("##log", &breakpoint.log_only);
            ImGui::TableNextColumn(); ImGui::Checkbox("##once", &breakpoint.one_shot);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputInt("##pass", &breakpoint.pass_count, 0, 0);
            if (breakpoint.pass_count < 0)
                breakpoint.pass_count = 0;
            ImGui::PopID();
        }
        if (remove >= 0)
            memory_breakpoints.erase(memory_breakpoints.begin() + remove);
        ImGui::EndTable();
    }
    ImGui::End();
}

static bool read_value(const GT_Debug_Memory_Address& address, int size,
    int endian, u64& value, GT_Debug_Memory_Status& status)
{
    u8 data[8];
    GT_Debug_Memory_Status statuses[8];
    memory_provider.ReadBlock(address, data, statuses, size, NULL);
    value = 0;
    status = GT_DEBUG_MEMORY_VALID;
    for (int i = 0; i < size; i++)
    {
        if (statuses[i] != GT_DEBUG_MEMORY_VALID &&
            statuses[i] != GT_DEBUG_MEMORY_READ_ONLY)
        {
            status = statuses[i];
            return false;
        }
        if (statuses[i] == GT_DEBUG_MEMORY_READ_ONLY)
            status = GT_DEBUG_MEMORY_READ_ONLY;
        int source = endian == 0 ? i : size - i - 1;
        value |= (u64)data[source] << (i * 8);
    }
    return true;
}

static void format_address(const GT_Debug_Memory_Address& address,
    char* text, size_t text_size)
{
    if (address.space == GT_DEBUG_MEMORY_REGION)
    {
        GT_Debug_Memory_Region region;
        if (memory_provider.GetRegionById(address.region, region))
        {
            snprintf(text, text_size, "%s+%08X", region.name, address.address);
            return;
        }
    }
    if (address.space == GT_DEBUG_MEMORY_LOGICAL)
    {
        static const char* segments[] = { "ES", "CS", "SS", "DS", "FS", "GS" };
        if (address.segment_register >= 0 && address.segment_register < I386_SEGMENT_COUNT)
            snprintf(text, text_size, "%s:%08X", segments[address.segment_register], address.address);
        else
            snprintf(text, text_size, "%04X:%08X", address.segment, address.address);
        return;
    }
    snprintf(text, text_size, "%s:%08X",
        DebugMemoryProvider::GetSpaceName(address.space), address.address);
}

static void format_value(u64 value, int size, int format, char* text,
    size_t text_size)
{
    if (format == 0)
        snprintf(text, text_size, "%0*llX", size * 2, (unsigned long long)value);
    else if (format == 1)
        snprintf(text, text_size, "%llu", (unsigned long long)value);
    else if (format == 2)
    {
        s64 signed_value = size == 1 ? (s8)value : size == 2 ? (s16)value :
            size == 4 ? (s32)value : (s64)value;
        snprintf(text, text_size, "%lld", (long long)signed_value);
    }
    else if (format == 3)
    {
        int position = 0;
        for (int bit = size * 8 - 1; bit >= 0 && position < (int)text_size - 2; bit--)
        {
            text[position++] = (value & (1ULL << bit)) ? '1' : '0';
            if (bit > 0 && (bit & 3) == 0)
                text[position++] = ' ';
        }
        text[position] = 0;
    }
    else
    {
        int length = MIN(size, (int)text_size - 1);
        for (int i = 0; i < length; i++)
        {
            u8 character = (u8)(value >> (i * 8));
            text[i] = character >= 32 && character < 127 ? character : '.';
        }
        text[length] = 0;
    }
}

static int watch_size_bytes(int size)
{
    const int sizes[] = { 1, 2, 4, 8 };
    return sizes[CLAMP(size, 0, 3)];
}

static void navigate_to(const GT_Debug_Memory_Address& address)
{
    MemEditor* editor = active_editor();
    if (!IsValidPointer(editor))
        return;
    editor->SetSource(address);
    editor->JumpToAddress(address.address);
}

static bool parse_u32(const char* text, u32& value)
{
    if (!IsValidPointer(text))
        return false;
    std::string input(text);
    return parse_hex_with_prefix(input, &value);
}

static bool parse_u64(const char* text, u64& value)
{
    if (!IsValidPointer(text) || text[0] == 0)
        return false;
    const char* input = text;
    size_t length = strlen(input);
    if (length >= 2 && input[0] == '0' &&
        (input[1] == 'x' || input[1] == 'X'))
    {
        input += 2;
        length -= 2;
    }
    else if (length > 0 && input[0] == '$')
    {
        input++;
        length--;
    }
    return parse_hex_string<u64>(input, length, &value, 16);
}

static bool parse_pattern(const char* text, int type,
    std::vector<u8>& pattern, std::vector<u8>& mask)
{
    pattern.clear();
    mask.clear();
    if (!IsValidPointer(text) || text[0] == 0)
        return false;

    if (type == 1)
    {
        size_t length = strlen(text);
        pattern.assign((const u8*)text, (const u8*)text + length);
        mask.assign(length, 0xFF);
        return !pattern.empty();
    }
    if (type == 2)
    {
        char* converted = SDL_iconv_string("SHIFT-JIS", "UTF-8", text,
            strlen(text) + 1);
        if (!IsValidPointer(converted))
            return false;
        size_t length = strlen(converted);
        pattern.assign((u8*)converted, (u8*)converted + length);
        mask.assign(length, 0xFF);
        SDL_free(converted);
        return !pattern.empty();
    }

    std::string compact;
    for (const char* p = text; *p != 0; p++)
    {
        if (!isspace((unsigned char)*p) && *p != ',' && *p != '-')
            compact += *p;
    }
    if (compact.empty() || (compact.length() & 1) != 0)
        return false;

    for (size_t i = 0; i < compact.length(); i += 2)
    {
        u8 byte = 0;
        u8 byte_mask = 0;
        for (int nibble = 0; nibble < 2; nibble++)
        {
            char character = compact[i + nibble];
            byte <<= 4;
            byte_mask <<= 4;
            if (character != '?')
            {
                if (!is_hex_digit(character))
                    return false;
                byte |= (u8)as_hex(character);
                byte_mask |= 0x0F;
            }
        }
        pattern.push_back(byte);
        mask.push_back(byte_mask);
    }
    return true;
}

void gui_debug_memory_save_settings(std::ostream& stream)
{
    int view_count = active_view_count();
    stream.write((const char*)&view_count, sizeof(view_count));
    for (int i = 0; i < MEMORY_VIEW_COUNT; i++)
    {
        if (memory_editor[i].IsAvailable())
        {
            MemoryViewSettings settings;
            memset(&settings, 0, sizeof(settings));
            settings.source = memory_editor[i].GetSource();
            settings.options = memory_editor[i].GetOptions();
            stream.write((const char*)&settings, sizeof(settings));
        }
    }

    int bookmark_count = (int)memory_bookmarks.size();
    stream.write((const char*)&bookmark_count, sizeof(bookmark_count));
    for (int i = 0; i < bookmark_count; i++)
    {
        const MemoryBookmark& item = memory_bookmarks[i];
        stream.write((const char*)&item, sizeof(item));
    }

    int watch_count = (int)memory_watches.size();
    stream.write((const char*)&watch_count, sizeof(watch_count));
    for (int i = 0; i < watch_count; i++)
    {
        const MemoryWatch& item = memory_watches[i];
        stream.write((const char*)&item, sizeof(item));
    }

    int breakpoint_count = (int)memory_breakpoints.size();
    stream.write((const char*)&breakpoint_count, sizeof(breakpoint_count));
    for (int i = 0; i < breakpoint_count; i++)
    {
        const MemoryBreakpoint& item = memory_breakpoints[i];
        stream.write((const char*)&item, sizeof(item));
    }
}

bool gui_debug_memory_load_settings(std::istream& stream)
{
    int view_count = 0;
    if (!read_count(stream, view_count, sizeof(MemoryViewSettings)) ||
        view_count > MEMORY_VIEW_COUNT)
        return false;

    MemoryViewSettings view_settings[MEMORY_VIEW_COUNT];
    memset(view_settings, 0, sizeof(view_settings));
    for (int i = 0; i < view_count; i++)
    {
        if (!read_data(stream, &view_settings[i], sizeof(view_settings[i])) ||
            view_settings[i].source.space < 0 ||
            view_settings[i].source.space >= GT_DEBUG_MEMORY_SPACE_COUNT)
            return false;
    }

    int bookmark_count = 0;
    if (!read_count(stream, bookmark_count, sizeof(MemoryBookmark)))
        return false;
    std::vector<MemoryBookmark> bookmarks(bookmark_count);
    for (int i = 0; i < bookmark_count; i++)
    {
        if (!read_data(stream, &bookmarks[i], sizeof(bookmarks[i])))
            return false;
        bookmarks[i].name[sizeof(bookmarks[i].name) - 1] = 0;
        if (bookmarks[i].address.space < 0 ||
            bookmarks[i].address.space >= GT_DEBUG_MEMORY_SPACE_COUNT)
            return false;
    }

    int watch_count = 0;
    if (!read_count(stream, watch_count, sizeof(MemoryWatch)))
        return false;
    std::vector<MemoryWatch> watches(watch_count);
    for (int i = 0; i < watch_count; i++)
    {
        if (!read_data(stream, &watches[i], sizeof(watches[i])))
            return false;
        watches[i].name[sizeof(watches[i].name) - 1] = 0;
        if (watches[i].address.space < 0 ||
            watches[i].address.space >= GT_DEBUG_MEMORY_SPACE_COUNT ||
            watches[i].size < 0 || watches[i].size > 3 ||
            watches[i].format < 0 || watches[i].format > 4 ||
            watches[i].endian < 0 || watches[i].endian > 1)
            return false;
    }

    int breakpoint_count = 0;
    if (!read_count(stream, breakpoint_count, sizeof(MemoryBreakpoint)))
        return false;
    std::vector<MemoryBreakpoint> breakpoints(breakpoint_count);
    for (int i = 0; i < breakpoint_count; i++)
    {
        if (!read_data(stream, &breakpoints[i], sizeof(breakpoints[i])))
            return false;
        if (breakpoints[i].address.space < 0 ||
            breakpoints[i].address.space >= GT_DEBUG_MEMORY_SPACE_COUNT)
            return false;
    }

    for (int i = 0; i < MEMORY_VIEW_COUNT; i++)
        memory_editor[i].SetAvailable(false);
    for (int i = 0; i < view_count; i++)
    {
        memory_editor[i].SetAvailable(true);
        memory_editor[i].SetOptions(view_settings[i].options);
        memory_editor[i].SetSource(view_settings[i].source);
    }
    if (view_count == 0)
    {
        memory_editor[0].SetAvailable(true);
        memory_editor[0].Reset();
    }
    current_editor = 0;
    memory_bookmarks.swap(bookmarks);
    memory_watches.swap(watches);
    memory_breakpoints.swap(breakpoints);
    return true;
}

static bool read_data(std::istream& stream, void* data, size_t size)
{
    stream.read((char*)data, (std::streamsize)size);
    return !stream.fail() && stream.gcount() == (std::streamsize)size;
}

static bool read_count(std::istream& stream, int& count, size_t record_size)
{
    if (!read_data(stream, &count, sizeof(count)) || count < 0 ||
        count > MEMORY_SETTINGS_MAX_RECORDS || record_size == 0)
        return false;

    std::streampos position = stream.tellg();
    if (position == std::streampos(-1))
        return false;
    stream.seekg(0, std::ios::end);
    std::streampos end = stream.tellg();
    stream.seekg(position);
    if (stream.fail() || end < position)
        return false;
    return (u64)count <= (u64)(end - position) / record_size;
}
