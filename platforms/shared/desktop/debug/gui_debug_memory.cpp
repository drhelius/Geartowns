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
#include <ctype.h>
#include <fstream>
#include <string>
#include <vector>
#include <SDL3/SDL.h>

#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../gui_colors.h"
#include "gui_debug_memeditor.h"
#include "gui_debug_memory_provider.h"
#include "../gui_filedialogs.h"
#include "../gui_notifications.h"
#include "../utils.h"
#include "imgui.h"
#include "imgui_internal.h"

static const int MEMORY_VIEW_COUNT = 8;
static const float MEMORY_SOURCES_WIDTH = 136.0f;
static const float MEMORY_INSPECTOR_WIDTH = 173.0f;
static const int MEMORY_SETTINGS_MAX_RECORDS = 0x10000;
static const u32 MEMORY_SEARCH_MAX_SIZE = 0x04000000;
static const u32 MEMORY_IMPORT_CHUNK_SIZE = 0x100000;
static const int MEMORY_SEARCH_MAX_VISIBLE_RESULTS = 10000;
static const int SEARCH_REFERENCE_PREVIOUS = 0;
static const int SEARCH_REFERENCE_INITIAL = 1;
static const int SEARCH_REFERENCE_VALUE = 2;
static const int SEARCH_REFERENCE_PREVIOUS_PLUS = 3;
static const int SEARCH_TYPE_HEX = 0;
static const int SEARCH_TYPE_SIGNED = 1;
static const int SEARCH_TYPE_UNSIGNED = 2;

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
    MemorySearch();
    void Reset();
    bool Start(DebugMemoryProvider& provider, const GT_Debug_Memory_Address& source, u32 start, u32 size, int width,
        int endian, bool signed_values, bool aligned, bool known, u64 known_value);
    bool FilterOperator(DebugMemoryProvider& provider, int comparison, int reference, u64 value, bool signed_values);
    void Undo();
    bool FindPattern(DebugMemoryProvider& provider, const GT_Debug_Memory_Address& source, u32 start, u32 size,
        const std::vector<u8>& pattern, const std::vector<u8>& mask);

    const GT_Debug_Memory_Address& GetSource() const;
    const std::vector<MemorySearchResult>& GetResults() const;
    u32 GetCandidateCount() const;
    int GetWidth() const;
    int GetEndian() const;
    u32 GetPatternSize() const;
    bool IsActive() const;
    bool CanUndo() const;

private:
    bool IsValueAvailable(const std::vector<GT_Debug_Memory_Status>& status, u32 offset) const;
    u64 ReadValue(const std::vector<u8>& data, u32 offset) const;
    u64 MaskValue(u64 value) const;
    s64 GetSignedValue(u64 value) const;
    bool CompareOperator(u64 current, u64 reference, int comparison, bool signed_values) const;
    bool IsCandidate(u32 offset) const;
    void SetCandidate(u32 offset, bool candidate);
    void BuildResults(const std::vector<u8>& current, const std::vector<u8>& previous);

private:
    GT_Debug_Memory_Address m_source;
    u32 m_start;
    u32 m_size;
    int m_width;
    int m_endian;
    bool m_signed;
    bool m_aligned;

    u32 m_candidate_count;
    u32 m_pattern_size;
    bool m_active;
    bool m_can_undo;

    std::vector<u8> m_initial_data;
    std::vector<u8> m_previous_data;
    std::vector<u8> m_candidate_bits;

    std::vector<u8> m_undo_data;
    std::vector<u8> m_undo_candidate_bits;

    std::vector<MemorySearchResult> m_results;
};

static const int MEMORY_GROUP_MEMORY = 0;
static const int MEMORY_GROUP_ROM = 1;
static const int MEMORY_GROUP_MEDIA = 2;
static const int MEMORY_GROUP_CPU_WINDOW = 3;

struct MemorySourceInfo
{
    int id;
    int group;
    bool hidden;
    const char* description;
};

static const MemorySourceInfo k_memory_sources[] =
{
    { GT_DEBUG_REGION_MAIN_RAM, MEMORY_GROUP_MEMORY, false, "Main memory with the program code, data and stack" },
    { GT_DEBUG_REGION_VRAM, MEMORY_GROUP_MEMORY, false,
        "Video RAM as stored, layer 0 at 00000 and layer 1 at 40000 in two-page modes" },
    { GT_DEBUG_REGION_SPRITE_RAM, MEMORY_GROUP_MEMORY, false, "Sprite attributes, patterns and color tables" },
    { GT_DEBUG_REGION_PCM_RAM, MEMORY_GROUP_MEMORY, false, "RF5C68 wave memory with all 16 banks of PCM samples" },
    { GT_DEBUG_REGION_CMOS, MEMORY_GROUP_MEMORY, false, "Battery backed CMOS RAM with the system settings and saved data" },
    { GT_DEBUG_REGION_SYSTEM_ROM, MEMORY_GROUP_ROM, false, "Boot code and BIOS (FMT_SYS.ROM)" },
    { GT_DEBUG_REGION_OS_ROM, MEMORY_GROUP_ROM, false, "Operating system ROM (FMT_DOS.ROM)" },
    { GT_DEBUG_REGION_DICTIONARY_ROM, MEMORY_GROUP_ROM, false, "Kana-kanji conversion dictionary (FMT_DIC.ROM)" },
    { GT_DEBUG_REGION_FONT_ROM, MEMORY_GROUP_ROM, false, "Kanji and ANK fonts (FMT_FNT.ROM)" },
    { GT_DEBUG_REGION_FONT20_ROM, MEMORY_GROUP_ROM, false, "20 dot kanji font (FMT_F20.ROM)" },
    { GT_DEBUG_REGION_VRAM_TWO_PAGE, MEMORY_GROUP_CPU_WINDOW, false,
        "VRAM as the CPU writes it in two-page modes, through the VRAM write mask (0458/045A)" },
    { GT_DEBUG_REGION_VRAM_SINGLE_PAGE, MEMORY_GROUP_CPU_WINDOW, false,
        "VRAM as one linear page, the layout of single-page modes" },
    { GT_DEBUG_REGION_PCM_WINDOW, MEMORY_GROUP_CPU_WINDOW, false,
        "The 4 KB bank of PCM wave memory selected on the RF5C68, where samples are uploaded" },
    { GT_DEBUG_REGION_SYSTEM_ROM_LOW_ALIAS, MEMORY_GROUP_CPU_WINDOW, false,
        "The last 32 KB of the System ROM below 1 MB, where the CPU boots, until port 0480 maps RAM" },
    { GT_DEBUG_REGION_DICTIONARY_ROM_LOW_WINDOW, MEMORY_GROUP_CPU_WINDOW, false,
        "The 32 KB Dictionary ROM bank selected with port 0484, below 1 MB" },
    { GT_DEBUG_REGION_CMOS_LOW_WINDOW, MEMORY_GROUP_CPU_WINDOW, false, "CMOS RAM below 1 MB, for real mode code" },
    { GT_DEBUG_REGION_FMR_PLANES, MEMORY_GROUP_CPU_WINDOW, false,
        "Layer 0 VRAM as FM-R bit planes, when port 0404 maps the FM-R devices" },
    { GT_DEBUG_REGION_FMR_TEXT, MEMORY_GROUP_CPU_WINDOW, false,
        "FM-R text RAM and ANK font, when port 0404 maps the FM-R devices" },
    { GT_DEBUG_REGION_FMR_REGISTERS, MEMORY_GROUP_CPU_WINDOW, false,
        "FM-R video registers as memory, when port 0404 maps the FM-R devices" },
    { GT_DEBUG_REGION_FMR_VIEW, MEMORY_GROUP_CPU_WINDOW, true, "The rest of the FM-R area, with nothing mapped" }
};

static DebugMemoryProvider memory_provider;
static MemEditor memory_editor[MEMORY_VIEW_COUNT];
static int current_editor = 0;
static int select_editor = -1;
static bool reset_layout[MEMORY_VIEW_COUNT] = { };

static std::vector<MemoryBookmark> memory_bookmarks;
static std::vector<MemoryWatch> memory_watches;

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
static int search_type = SEARCH_TYPE_UNSIGNED;
static int search_operator = 2;
static int search_reference = SEARCH_REFERENCE_PREVIOUS;
static int search_pattern_type = 0;
static char search_error[64] = {};

static bool search_aligned = true;
static bool search_range_valid = false;
static GT_Debug_Memory_Address search_range_source;
static int search_tab_request = -1;
static bool show_bookmark_popup = false;
static MemoryBookmark new_bookmark;

static u32 memory_media_crc = 0;
static bool memory_media_crc_valid = false;

static MemEditor* active_editor();
static int active_view_count();
static void new_editor_view();
static void draw_memory_menu();
static void draw_editor_tab(MemEditor& editor, int index);
static void draw_region_browser(MemEditor& editor);
static void draw_browser_title(const char* title);
static void draw_region_group(MemEditor& editor, const char* title, int group);
static void draw_region_tooltip(const GT_Debug_Memory_Region& region);
static bool draw_source_item(const char* name, bool selected);
static void draw_inspector(MemEditor& editor);
static bool begin_inspector_table(const char* id);
static void draw_inspector_label(const char* label);
static void draw_watches_window();
static void draw_watch_menu(MemoryWatch& watch, int index, int& remove);
static void draw_search_window();
static void draw_search_label(const char* label, bool spaced);
static void draw_search_bytes(const GT_Debug_Memory_Address& address, int count);
static void draw_search_result_menu(const GT_Debug_Memory_Address& address, int size);
static void draw_search_tooltip(const char* text);
static void set_search_error(const char* text);
static void set_default_search_range(MemEditor& editor);
static bool parse_search_value(const char* text, int type, u64& value);
static void format_search_value(u64 value, int width, int type, char* text, size_t size);
static float search_combo_width(const char* items);
static void draw_breakpoints_window();
static void add_selection_bookmark();
static void add_selection_watch();
static void add_selection_breakpoint();
static bool has_memory_breakpoints();
static void remove_memory_breakpoints();
static void goto_bookmark(MemEditor& editor, const MemoryBookmark& bookmark);
static void request_bookmark(const GT_Debug_Memory_Address& address, u32 end);
static void draw_bookmark_popup();
static void draw_row_tooltip(const char* text);
static void process_editor_requests(MemEditor& editor);
static void add_bookmark(const GT_Debug_Memory_Address& address, u32 end);
static void add_watch(const GT_Debug_Memory_Address& address);
static bool add_breakpoint(const GT_Debug_Memory_Address& address, u32 end,
    u8 type = I386_BREAKPOINT_READ | I386_BREAKPOINT_WRITE);
static bool read_value(const GT_Debug_Memory_Address& address, int size, int endian, u64& value,
    GT_Debug_Memory_Status& status);
static void format_address(const GT_Debug_Memory_Address& address, char* text, size_t text_size);
static void format_value(u64 value, int size, int format, char* text, size_t text_size);
static int watch_size_bytes(int size);
static void navigate_to(const GT_Debug_Memory_Address& address);
static bool parse_u32(const char* text, u32& value);
static bool parse_u64(const char* text, u64& value);
static bool parse_pattern(const char* text, int type, std::vector<u8>& pattern, std::vector<u8>& mask);
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
    memory_search.Reset();
}

void gui_debug_memory_reset(void)
{
    GeartownsCore* core = emu_get_core();
    bool media_ready = IsValidPointer(core) && IsValidPointer(core->GetMedia()) && core->GetMedia()->IsReady();
    u32 media_crc = media_ready ? core->GetMedia()->GetCRC() : 0;

    if (!memory_media_crc_valid || media_crc != memory_media_crc)
    {
        memory_bookmarks.clear();
        memory_watches.clear();
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

void gui_debug_memory_goto(const GT_Debug_Memory_Address& address)
{
    config_debug.show_memory = true;
    navigate_to(address);
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
        watch.valid = read_value(watch.address, watch_size_bytes(watch.size), watch.endian, watch.value, status);

        if (watch.freeze && watch.valid && watch.value != watch.frozen_value)
        {
            int bytes = watch_size_bytes(watch.size);
            u8 data[8];

            for (int b = 0; b < bytes; b++)
            {
                int destination = watch.endian == 0 ? b : bytes - b - 1;
                data[destination] = (u8)(watch.frozen_value >> (b * 8));
            }

            memory_provider.QueueWrite(watch.address, data, bytes, false);
        }
    }

    if (!config_debug.debug || !config_debug.show_memory)
        return;

    for (int i = 0; i < MEMORY_VIEW_COUNT; i++)
    {
        if (memory_editor[i].IsAvailable())
            memory_editor[i].Update();
    }
}

void gui_debug_window_memory(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(228, 236), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(882, 635), ImGuiCond_FirstUseEver);
    ImGui::Begin("Memory Workspace", &config_debug.show_memory, ImGuiWindowFlags_MenuBar);

    draw_memory_menu();

    if (ImGui::BeginTabBar("##memory_views", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs))
    {
        for (int i = 0; i < MEMORY_VIEW_COUNT; i++)
        {
            if (memory_editor[i].IsAvailable())
                draw_editor_tab(memory_editor[i], i);
        }

        select_editor = -1;

        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
            new_editor_view();

        ImGui::EndTabBar();
    }

    if (show_fill_popup)
        ImGui::OpenPopup("Fill Selection");

    if (ImGui::BeginPopupModal("Fill Selection", &show_fill_popup, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Hex value:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(40.0f);
        bool fill = ImGui::InputText("##fill_value", fill_value, sizeof(fill_value),
            ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase |
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        ImGui::SameLine();
        fill = ImGui::Button("Fill") || fill;

        if (fill)
        {
            u8 value = 0;

            if (parse_hex_string(fill_value, strlen(fill_value), &value) && IsValidPointer(active_editor()))
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

    draw_bookmark_popup();

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

bool gui_debug_memory_save_dump(const char* file_path)
{
    MemEditor* editor = active_editor();

    if (!IsValidPointer(editor) || !IsValidPointer(file_path))
        return false;

    u32 start = 0;
    u32 end = 0;
    editor->GetSelection(start, end);
    u64 size64 = (u64)end - start + 1;

    if (size64 == 0 || size64 > MEMORY_SEARCH_MAX_SIZE)
        return false;

    u32 size = (u32)size64;

    std::vector<u8> data(size);
    std::vector<GT_Debug_Memory_Status> status(size);
    GT_Debug_Memory_Address address = editor->GetSource();
    address.address = start;
    memory_provider.ReadBlock(address, &data[0], &status[0], size, NULL);

    for (u32 i = 0; i < size; i++)
    {
        if (status[i] != GT_DEBUG_MEMORY_VALID && status[i] != GT_DEBUG_MEMORY_READ_ONLY)
            return false;
    }

    FILE* file = fopen_utf8(file_path, "wb");

    if (!IsValidPointer(file))
        return false;

    bool written = fwrite(&data[0], 1, size, file) == size;
    fclose(file);
    return written;
}

bool gui_debug_memory_load_dump(const char* file_path)
{
    MemEditor* editor = active_editor();

    if (!IsValidPointer(editor) || !IsValidPointer(file_path))
        return false;

    std::ifstream file;
    open_ifstream_utf8(file, file_path, std::ios::binary);

    if (!file.is_open())
        return false;

    file.seekg(0, std::ios::end);
    std::streampos position = file.tellg();
    std::streamoff file_size = position;

    if (position == std::streampos(-1) || file_size <= 0 || (u64)file_size > MEMORY_SEARCH_MAX_SIZE)
    {
        file.close();
        return false;
    }

    u32 size = (u32)file_size;
    std::vector<u8> data(size);
    file.seekg(0, std::ios::beg);
    file.read((char*)&data[0], size);
    bool valid = !file.fail();
    file.close();

    if (!valid)
        return false;

    u32 start = 0;
    u32 end = 0;
    editor->GetSelection(start, end);
    GT_Debug_Memory_Address address = editor->GetSource();

    if ((u64)start + size - 1 > memory_provider.GetAddressLimit(address))
        return false;

    for (u32 offset = 0; offset < size; offset += MEMORY_IMPORT_CHUNK_SIZE)
    {
        address.address = start + offset;

        if (!memory_provider.QueueWrite(address, &data[offset], MIN(size - offset, MEMORY_IMPORT_CHUNK_SIZE)))
            return false;
    }

    return true;
}

static MemEditor* active_editor()
{
    if (current_editor < 0 || current_editor >= MEMORY_VIEW_COUNT || !memory_editor[current_editor].IsAvailable())
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
                memory_editor[i].JumpToAddress(current->GetSource().address, false);
            }

            current_editor = i;
            select_editor = i;
            reset_layout[i] = true;
            return;
        }
    }
}

static void draw_memory_menu()
{
    MemEditor* editor = active_editor();

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

    if (ImGui::BeginMenu("Bookmarks"))
    {
        if (ImGui::MenuItem("Add Bookmark...", NULL, false, IsValidPointer(editor)))
            add_selection_bookmark();

        if (ImGui::MenuItem("Clear All", NULL, false, !memory_bookmarks.empty()))
            memory_bookmarks.clear();

        if (!memory_bookmarks.empty())
            ImGui::Separator();

        int remove = -1;

        for (size_t i = 0; i < memory_bookmarks.size(); i++)
        {
            MemoryBookmark& bookmark = memory_bookmarks[i];
            char address[128];
            format_address(bookmark.address, address, sizeof(address));
            ImGui::PushID((int)i);

            if (ImGui::MenuItem(bookmark.name, address) && IsValidPointer(editor))
                goto_bookmark(*editor, bookmark);

            draw_row_tooltip("Right click to rename or remove");

            if (ImGui::BeginPopupContextItem("##bookmark_menu"))
            {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Name");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(200.0f);
                ImGui::InputText("##bookmark_name", bookmark.name, sizeof(bookmark.name));

                if (ImGui::MenuItem("Remove Bookmark"))
                    remove = (int)i;

                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        if (remove >= 0)
            memory_bookmarks.erase(memory_bookmarks.begin() + remove);

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Watches"))
    {
        if (ImGui::MenuItem("Open Watches"))
            show_watches = true;

        if (ImGui::MenuItem("Add Watch", NULL, false, IsValidPointer(editor)))
            add_selection_watch();

        if (ImGui::MenuItem("Clear All", NULL, false, !memory_watches.empty()))
            memory_watches.clear();

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Breakpoints"))
    {
        if (ImGui::MenuItem("Open Breakpoints"))
            show_breakpoints = true;

        if (ImGui::MenuItem("Add Breakpoint", NULL, false, IsValidPointer(editor)))
            add_selection_breakpoint();

        if (ImGui::MenuItem("Clear All", NULL, false, has_memory_breakpoints()))
            remove_memory_breakpoints();

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Search"))
    {
        if (ImGui::MenuItem("Search Values..."))
        {
            show_search = true;
            search_tab_request = 0;
        }

        if (ImGui::MenuItem("Find Bytes or Text..."))
        {
            show_search = true;
            search_tab_request = 1;
        }

        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

static void draw_editor_tab(MemEditor& editor, int index)
{
    bool open = true;
    bool* open_pointer = active_view_count() > 1 ? &open : NULL;
    ImGuiTabItemFlags flags = select_editor == index ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    char tab_label[128];
    snprintf(tab_label, sizeof(tab_label), "%s###memory_tab_%d", editor.GetTitle(), index);

    if (ImGui::BeginTabItem(tab_label, open_pointer, flags))
    {
        current_editor = index;
        ImGui::PushID(index);

        if (ImGui::BeginTable("##memory_layout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Sources", ImGuiTableColumnFlags_WidthFixed, MEMORY_SOURCES_WIDTH);
            ImGui::TableSetupColumn("Memory", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, MEMORY_INSPECTOR_WIDTH);

            if (reset_layout[index])
            {
                ImGui::TableSetColumnWidth(0, MEMORY_SOURCES_WIDTH);
                ImGui::TableSetColumnWidth(2, MEMORY_INSPECTOR_WIDTH);
                reset_layout[index] = false;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            draw_region_browser(editor);
            ImGui::TableNextColumn();
            editor.Draw();
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
    static const GT_Debug_Memory_Space k_spaces[4] =
    {
        GT_DEBUG_MEMORY_LOGICAL, GT_DEBUG_MEMORY_LINEAR, GT_DEBUG_MEMORY_PHYSICAL, GT_DEBUG_MEMORY_IO
    };
    static const char* k_space_descriptions[4] =
    {
        "Segment:offset addresses as instructions use them, through CS by default",
        "Addresses after segmentation and before paging",
        "Addresses after paging, as the CPU sees them with the current memory map",
        "The 64 KB I/O port space, read without side effects"
    };

    ImGui::BeginChild("##memory_sources", ImVec2(0, -1));
    ImGui::PushFont(gui_default_font);
    draw_browser_title("ADDRESS SPACES");

    for (int i = 0; i < 4; i++)
    {
        GT_Debug_Memory_Space space = k_spaces[i];
        bool selected = editor.GetSource().space == space;

        if (draw_source_item(DebugMemoryProvider::GetSpaceName(space), selected))
        {
            GT_Debug_Memory_Address source = {};
            source.space = space;
            source.segment_register = space == GT_DEBUG_MEMORY_LOGICAL ? I386_SEGMENT_CS : -1;
            editor.SetFollow(false);
            editor.SetSource(source);
        }

        draw_row_tooltip(k_space_descriptions[i]);
    }

    draw_region_group(editor, "MEMORY", MEMORY_GROUP_MEMORY);
    draw_region_group(editor, "ROMS", MEMORY_GROUP_ROM);
    draw_region_group(editor, "MEDIA", MEMORY_GROUP_MEDIA);
    draw_region_group(editor, "CPU WINDOWS", MEMORY_GROUP_CPU_WINDOW);

    ImGui::PopFont();
    ImGui::EndChild();
}

static void draw_region_group(MemEditor& editor, const char* title, int group)
{
    bool first = true;
    int region_count = memory_provider.GetRegionCount();

    for (int pass = 0; pass < 2; pass++)
    {
        int count = pass == 0 ? (int)(sizeof(k_memory_sources) / sizeof(k_memory_sources[0])) : region_count;

        for (int i = 0; i < count; i++)
        {
            GT_Debug_Memory_Region region;

            if (pass == 0)
            {
                if (k_memory_sources[i].group != group || k_memory_sources[i].hidden ||
                    !memory_provider.GetRegionById(k_memory_sources[i].id, region))
                    continue;
            }
            else if (!memory_provider.GetRegion(i, region) || region.id < GT_DEBUG_REGION_MEDIA_IMAGE ||
                group != MEMORY_GROUP_MEDIA)
                continue;

            if (first)
            {
                ImGui::NewLine();
                draw_browser_title(title);
                first = false;
            }

            bool selected = editor.GetSource().space == GT_DEBUG_MEMORY_REGION && editor.GetSource().region == region.id;

            if (draw_source_item(region.name, selected))
            {
                GT_Debug_Memory_Address source = {};
                source.space = GT_DEBUG_MEMORY_REGION;
                source.segment_register = -1;
                source.region = region.id;
                editor.SetFollow(false);
                editor.SetSource(source);
            }

            draw_region_tooltip(region);
        }
    }
}

static void draw_region_tooltip(const GT_Debug_Memory_Region& region)
{
    if (!ImGui::IsItemHovered() || ImGui::IsPopupOpen((const char*)NULL, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        return;

    const char* description = gui_debug_memory_region_description(region.id);

    ImGui::BeginTooltip();
    ImGui::TextColored(cyan, "%s", region.name);

    if (IsValidPointer(description))
    {
        ImGui::PushFont(gui_roboto_font);
        ImGui::TextUnformatted(description);
        ImGui::PopFont();
    }

    if ((region.flags & (GT_DEBUG_REGION_MAPPED | GT_DEBUG_REGION_OVERLAY)) != 0)
    {
        ImGui::TextColored(violet, "CPU   "); ImGui::SameLine();
        ImGui::TextColored(white, "%08X-%08X", region.physical_base, region.physical_base + region.size - 1);

        if ((region.flags & GT_DEBUG_REGION_OVERLAY) != 0)
        {
            bool mapped = (region.flags & GT_DEBUG_REGION_MAPPED) != 0;
            ImGui::SameLine();
            ImGui::TextColored(mapped ? green : gray, "%s", mapped ? "MAPPED" : "NOT MAPPED");
        }
    }

    ImGui::TextColored(violet, "SIZE  "); ImGui::SameLine();

    if (region.size >= 1024 && (region.size % 1024) == 0)
        ImGui::TextColored(white, "%u KB", region.size / 1024);
    else
        ImGui::TextColored(white, "%u BYTES", region.size);

    ImGui::TextColored(violet, "ACCESS"); ImGui::SameLine();
    ImGui::TextColored(white, "%s", (region.flags & GT_DEBUG_REGION_WRITABLE) ? "READ/WRITE" : "READ-ONLY");
    ImGui::EndTooltip();
}

static void draw_browser_title(const char* title)
{
    ImGui::TextColored(cyan, "%s", title);
    ImGui::Separator();
}

static bool draw_source_item(const char* name, bool selected)
{
    if (selected)
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);

    bool clicked = ImGui::Selectable(name, selected);

    if (selected)
        ImGui::PopStyleColor();

    return clicked;
}

static void draw_inspector(MemEditor& editor)
{
    ImGui::BeginChild("##memory_inspector", ImVec2(0, -1));
    ImGui::PushFont(gui_default_font);

    u32 start = 0;
    u32 end = 0;
    editor.GetSelection(start, end);
    GT_Debug_Memory_Address address = editor.GetSource();
    address.address = start;
    MemEditor::Options options = editor.GetOptions();
    GT_Debug_Memory_Status status;
    u64 value = 0;
    ImGui::TextColored(cyan, "DATA INSPECTOR"); ImGui::Separator();

    if (begin_inspector_table("##inspector_data"))
    {
        static const char* labels[3] = { "8-BIT", "16-BIT", "32-BIT" };

        for (int i = 0; i < 3; i++)
        {
            int size = 1 << i;
            bool valid = read_value(address, size, options.preview_endian, value, status);
            s64 signed_value = size == 1 ? (s64)(s8)value : size == 2 ? (s64)(s16)value : (s64)(s32)value;

            ImGui::TableNextRow();
            draw_inspector_label(labels[i]);

            if (valid)
                ImGui::TextColored(white, "$%0*llX", size * 2, (unsigned long long)value);
            else
                ImGui::TextColored(gray, "--");

            ImGui::TableNextRow();
            draw_inspector_label("");

            if (valid)
                ImGui::TextColored(white, "%llu", (unsigned long long)value);
            else
                ImGui::TextColored(gray, "--");

            ImGui::TableNextRow();
            draw_inspector_label("");

            if (valid)
                ImGui::TextColored(white, "%lld", (long long)signed_value);
            else
                ImGui::TextColored(gray, "--");

            if (size == 4)
            {
                u32 raw = (u32)value;
                float floating;
                memcpy(&floating, &raw, sizeof(floating));

                ImGui::TableNextRow();
                draw_inspector_label("FLOAT");

                if (valid)
                    ImGui::TextColored(white, "%.9g", floating);
                else
                    ImGui::TextColored(gray, "--");
            }
        }

        ImGui::TableNextRow();
        draw_inspector_label("BITS");

        if (read_value(address, 1, 0, value, status))
            ImGui::TextColored(white, BYTE_TO_BINARY_PATTERN_SPACED, BYTE_TO_BINARY((u8)value));
        else
            ImGui::TextColored(gray, "--");

        const int text_size = 8;
        u8 text_data[text_size + 1];
        GT_Debug_Memory_Status text_status[text_size];
        memory_provider.ReadBlock(address, text_data, text_status, text_size, NULL);
        int readable = 0;

        while (readable < text_size && (text_status[readable] == GT_DEBUG_MEMORY_VALID ||
            text_status[readable] == GT_DEBUG_MEMORY_READ_ONLY))
            readable++;

        char ascii[text_size + 1];

        for (int i = 0; i < readable; i++)
            ascii[i] = text_data[i] >= 32 && text_data[i] < 127 ? (char)text_data[i] : '.';

        ascii[readable] = 0;

        ImGui::TableNextRow();
        draw_inspector_label("ASCII");

        if (readable > 0)
            ImGui::TextColored(white, "%s", ascii);
        else
            ImGui::TextColored(gray, "--");

        int text_length = 0;

        while (text_length < readable && text_data[text_length] != 0)
            text_length++;

        text_data[text_length] = 0;
        char* shift_jis = text_length > 0 ?
            SDL_iconv_string("UTF-8", "SHIFT-JIS", (const char*)text_data, (size_t)text_length + 1) : NULL;

        ImGui::TableNextRow();
        draw_inspector_label("SJIS");

        if (IsValidPointer(shift_jis))
        {
            ImGui::TextColored(white, "%s", shift_jis);
            SDL_free(shift_jis);
        }
        else
            ImGui::TextColored(gray, "--");

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "TRANSLATION"); ImGui::Separator();

    GT_Debug_Memory_Translation translation;
    bool translated = memory_provider.Translate(address, translation);
    bool paged = translation.page_directory_entry != 0 || translation.page_table_entry != 0;

    if (begin_inspector_table("##inspector_translation"))
    {
        ImGui::TableNextRow();
        draw_inspector_label("LOGICAL");

        if (translation.logical_valid)
            ImGui::TextColored(white, "%04X:%08X", translation.segment, translation.offset);
        else
            ImGui::TextColored(gray, "--");

        ImGui::TableNextRow();
        draw_inspector_label("SEG BASE");

        if (translation.logical_valid)
            ImGui::TextColored(white, "%08X", translation.segment_base);
        else
            ImGui::TextColored(gray, "--");

        ImGui::TableNextRow();
        draw_inspector_label("SEG LIMIT");

        if (translation.logical_valid)
            ImGui::TextColored(white, "%08X", translation.segment_limit);
        else
            ImGui::TextColored(gray, "--");

        ImGui::TableNextRow();
        draw_inspector_label("LINEAR");

        if (translation.linear_valid)
            ImGui::TextColored(white, "%08X", translation.linear);
        else
            ImGui::TextColored(gray, "--");

        ImGui::TableNextRow();
        draw_inspector_label("PDE/PTE");

        if (paged)
            ImGui::TextColored(white, "%08X %08X", translation.page_directory_entry, translation.page_table_entry);
        else
            ImGui::TextColored(gray, "--");

        ImGui::TableNextRow();
        draw_inspector_label("PAGE FLAGS");

        if (paged)
            ImGui::TextColored(white, "%08X", translation.page_flags);
        else
            ImGui::TextColored(gray, "--");

        ImGui::TableNextRow();
        draw_inspector_label("PHYSICAL");

        if (translation.physical_valid)
            ImGui::TextColored(white, "%08X", translation.physical);
        else
            ImGui::TextColored(gray, "--");

        ImGui::TableNextRow();
        draw_inspector_label("BUS");

        if (translation.bus_valid)
            ImGui::TextColored(white, "%08X", translation.bus);
        else
            ImGui::TextColored(gray, "--");

        ImGui::EndTable();
    }

    if (!translated && translation.reason[0] != 0 && address.space != GT_DEBUG_MEMORY_IO)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)red);
        ImGui::TextWrapped("%s", translation.reason);
        ImGui::PopStyleColor();
    }

    ImGui::PopFont();
    ImGui::EndChild();
}

static bool begin_inspector_table(const char* id)
{
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingFixedFit))
        return false;

    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("0").x * 10);
    ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthStretch);
    return true;
}

static void draw_inspector_label(const char* label)
{
    ImGui::TableNextColumn();
    ImGui::TextColored(violet, "%s", label);
    ImGui::TableNextColumn();
}

static void process_editor_requests(MemEditor& editor)
{
    GT_Debug_Memory_Address address;
    u32 end = 0;

    if (editor.TakeBookmarkRequest(address, end))
        request_bookmark(address, end);

    if (editor.TakeWatchRequest(address))
        add_watch(address);

    u8 type = 0;

    if (editor.TakeBreakpointRequest(address, end, type) && !add_breakpoint(address, end, type))
        gui_notify(gui_NotificationWarning, NULL, "This memory has no linear, physical or I/O address");
}

static void add_bookmark(const GT_Debug_Memory_Address& address, u32 end)
{
    MemoryBookmark bookmark;
    memset(&bookmark, 0, sizeof(bookmark));
    bookmark.address = address;
    bookmark.end = end;
    snprintf(bookmark.name, sizeof(bookmark.name), "Bookmark_%08X", address.address);
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

static bool add_breakpoint(const GT_Debug_Memory_Address& address, u32 end, u8 type)
{
    I386* cpu = emu_get_core()->GetI386();
    u32 size = end >= address.address ? end - address.address : 0;
    bool added = false;

    if (address.space == GT_DEBUG_MEMORY_IO)
        added = cpu->AddBreakpoint(address.address, end, type, I386_BREAKPOINT_IO);
    else
    {
        GT_Debug_Memory_Translation translation;

        if (!memory_provider.Translate(address, translation))
            return false;

        if ((address.space == GT_DEBUG_MEMORY_LINEAR || address.space == GT_DEBUG_MEMORY_LOGICAL) && translation.linear_valid)
            added = cpu->AddBreakpoint(translation.linear, translation.linear + size, type, I386_BREAKPOINT_LINEAR);
        else if (translation.physical_valid)
            added = cpu->AddBreakpoint(translation.physical, translation.physical + size, type, I386_BREAKPOINT_PHYSICAL);
    }

    if (added)
        show_breakpoints = true;

    return added;
}

static void draw_watches_window()
{
    static const char* k_sizes[4] = { "8 BIT", "16 BIT", "32 BIT", "64 BIT" };
    static const char* k_formats[5] = { "HEX", "UNSIGNED", "SIGNED", "BINARY", "ASCII" };

    ImGui::SetNextWindowPos(ImVec2(269, 103), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(650, 360), ImGuiCond_FirstUseEver);
    ImGui::Begin("Memory Watches", &show_watches);

    ImGui::BeginDisabled(!IsValidPointer(active_editor()));

    if (ImGui::Button("Add Selection"))
        add_selection_watch();

    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(memory_watches.empty());

    if (ImGui::Button("Remove All"))
        memory_watches.clear();

    ImGui::EndDisabled();

    ImGui::PushFont(gui_default_font);

    if (ImGui::BeginTable("##memory_watches", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("ADDRESS");
        ImGui::TableSetupColumn("SIZE");
        ImGui::TableSetupColumn("FORMAT");
        ImGui::TableSetupColumn("ENDIAN");
        ImGui::TableSetupColumn("VALUE");
        ImGui::TableSetupColumn("FREEZE");
        ImGui::TableSetupColumn("NAME", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int remove = -1;

        for (size_t i = 0; i < memory_watches.size(); i++)
        {
            MemoryWatch& watch = memory_watches[i];
            int size = CLAMP(watch.size, 0, 3);
            char address[128];
            format_address(watch.address, address, sizeof(address));

            ImGui::PushID((int)i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)cyan);

            if (ImGui::Selectable(address, false, ImGuiSelectableFlags_SpanAllColumns))
                navigate_to(watch.address);

            ImGui::PopStyleColor();
            draw_row_tooltip("Click to go to the address, right click to change the watch");
            draw_watch_menu(watch, (int)i, remove);

            ImGui::TableNextColumn();
            ImGui::TextColored(yellow, "%s", k_sizes[size]);
            ImGui::TableNextColumn();
            ImGui::TextColored(white, "%s", k_formats[CLAMP(watch.format, 0, 4)]);
            ImGui::TableNextColumn();
            ImGui::TextColored(size > 0 ? white : gray, "%s", watch.endian == 0 ? "LE" : "BE");
            ImGui::TableNextColumn();

            if (watch.valid)
            {
                char value[96];
                format_value(watch.value, watch_size_bytes(watch.size), watch.format, value, sizeof(value));
                ImGui::TextColored(watch.value != watch.previous ? orange : white, "%s", value);
            }
            else
                ImGui::TextColored(gray, "--");

            ImGui::TableNextColumn();
            ImGui::TextColored(watch.freeze ? yellow : gray, "%s", watch.freeze ? "ON" : "OFF");
            ImGui::TableNextColumn();
            ImGui::TextColored(violet, "%s", watch.name);
            ImGui::PopID();
        }

        if (remove >= 0)
            memory_watches.erase(memory_watches.begin() + remove);

        ImGui::EndTable();
    }

    ImGui::PopFont();
    ImGui::End();
}

static void draw_watch_menu(MemoryWatch& watch, int index, int& remove)
{
    static const char* k_sizes[4] = { "8 bit", "16 bit", "32 bit", "64 bit" };
    static const char* k_formats[5] = { "Hex", "Unsigned", "Signed", "Binary", "ASCII" };

    if (!ImGui::BeginPopupContextItem())
        return;

    ImGui::PushFont(gui_roboto_font);

    if (ImGui::BeginMenu("Size"))
    {
        for (int i = 0; i < 4; i++)
        {
            if (ImGui::MenuItem(k_sizes[i], NULL, watch.size == i))
                watch.size = i;
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Format"))
    {
        for (int i = 0; i < 5; i++)
        {
            if (ImGui::MenuItem(k_formats[i], NULL, watch.format == i))
                watch.format = i;
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Endian"))
    {
        if (ImGui::MenuItem("Little Endian", NULL, watch.endian == 0))
            watch.endian = 0;

        if (ImGui::MenuItem("Big Endian", NULL, watch.endian == 1))
            watch.endian = 1;

        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Freeze", NULL, watch.freeze))
    {
        watch.freeze = !watch.freeze;
        watch.frozen_value = watch.value;
    }

    ImGui::Separator();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Name");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputText("##watch_name", watch.name, sizeof(watch.name));
    ImGui::Separator();

    if (ImGui::MenuItem("Remove Watch"))
        remove = index;

    ImGui::PopFont();
    ImGui::EndPopup();
}

static void draw_search_window()
{
    static const char k_widths[] = "8 bit\0" "16 bit\0" "32 bit\0" "64 bit\0";
    static const char k_endians[] = "Little\0Big\0";
    static const char k_types[] = "Hex\0Signed\0Unsigned\0";
    static const char k_operators[] = "<\0>\0=\0!=\0<=\0>=\0";
    static const char k_references[] = "previous\0initial\0number\0previous +\0";
    static const char k_pattern_types[] = "Hex + wildcards\0ASCII text\0Shift-JIS text\0";

    ImGui::SetNextWindowPos(ImVec2(110, 170), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(620, 520), ImGuiCond_FirstUseEver);
    ImGui::Begin("Memory Search", &show_search);
    MemEditor* editor = active_editor();

    if (!IsValidPointer(editor))
    {
        ImGui::TextDisabled("No memory view is active.");
        ImGui::End();
        return;
    }

    if (!search_range_valid || !gui_debug_memory_same_source(editor->GetSource(), search_range_source))
    {
        set_default_search_range(*editor);
        search_error[0] = 0;
    }

    bool pattern_results = memory_search.GetPatternSize() > 0;
    bool has_results = memory_search.IsActive() || pattern_results;
    GT_Debug_Memory_Address source = has_results ? memory_search.GetSource() : editor->GetSource();
    source.address = 0;
    char source_name[128];
    format_address(source, source_name, sizeof(source_name));

    ImGui::PushFont(gui_default_font);
    ImGui::TextColored(violet, "SOURCE"); ImGui::SameLine();
    ImGui::TextColored(white, "%s", source_name);
    ImGui::PopFont();

    float hex_input = ImGui::CalcTextSize("00000000").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

    if (ImGui::BeginTable("##search_range", 4, flags))
    {
        ImGui::TableNextRow();
        draw_search_label("Start", false);
        ImGui::SetNextItemWidth(hex_input);
        ImGui::InputText("##search_start", search_start, sizeof(search_start),
            ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
        draw_search_label("Size", true);
        ImGui::SetNextItemWidth(hex_input);
        ImGui::InputText("##search_size", search_size, sizeof(search_size),
            ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
        ImGui::EndTable();
    }

    if (ImGui::BeginTabBar("##search_tabs"))
    {
        if (ImGui::BeginTabItem("Values", NULL, search_tab_request == 0 ? ImGuiTabItemFlags_SetSelected : 0))
        {
            if (ImGui::BeginTable("##search_format", 6, flags))
            {
                ImGui::TableNextRow();
                draw_search_label("Width", false);
                ImGui::SetNextItemWidth(search_combo_width(k_widths));
                ImGui::Combo("##search_width", &search_width, k_widths);
                draw_search_label("Endian", true);
                ImGui::SetNextItemWidth(search_combo_width(k_endians));
                ImGui::Combo("##search_endian", &search_endian, k_endians);
                draw_search_label("Type", true);
                ImGui::SetNextItemWidth(search_combo_width(k_types));
                ImGui::Combo("##search_type", &search_type, k_types);
                draw_search_tooltip("How values are typed and shown");
                ImGui::EndTable();
            }

            ImGui::Checkbox("Aligned", &search_aligned);
            draw_search_tooltip("Only addresses that are a multiple of the width");
            ImGui::SameLine(0.0f, 16.0f);

            if (ImGui::Button("New Search"))
            {
                u32 start = 0;
                u32 size = 0;
                int widths[] = { 1, 2, 4, 8 };

                if (!parse_u32(search_start, start) || !parse_u32(search_size, size))
                    set_search_error("Invalid range");
                else if (!memory_search.Start(memory_provider, editor->GetSource(), start, size, widths[search_width],
                    search_endian, search_type == SEARCH_TYPE_SIGNED, search_aligned, false, 0))
                    set_search_error("The range does not fit the source");
                else
                    search_error[0] = 0;
            }

            draw_search_tooltip("Snapshots the range, every value becomes a candidate");

            bool needs_value = search_reference >= SEARCH_REFERENCE_VALUE;

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Value is");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(search_combo_width(k_operators));
            ImGui::Combo("##search_operator", &search_operator, k_operators);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(search_combo_width(k_references));
            ImGui::Combo("##search_reference", &search_reference, k_references);
            draw_search_tooltip("previous: at the last Apply\ninitial: at New Search");
            ImGui::SameLine();
            ImGui::BeginDisabled(!needs_value);
            ImGui::SetNextItemWidth(ImGui::CalcTextSize("-0000000000").x + ImGui::GetStyle().FramePadding.x * 2.0f);
            ImGui::InputText("##search_value", search_value, sizeof(search_value), ImGuiInputTextFlags_AutoSelectAll);
            draw_search_tooltip(search_type == SEARCH_TYPE_HEX ? "Hex value, negative to go down" :
                "Decimal, or hex with $\nNegative to go down");
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!memory_search.IsActive());

            if (ImGui::Button("Apply"))
            {
                u64 value = 0;

                if (needs_value && !parse_search_value(search_value, search_type, value))
                    set_search_error("Invalid value");
                else
                {
                    memory_search.FilterOperator(memory_provider, search_operator, search_reference, value,
                        search_type == SEARCH_TYPE_SIGNED);
                    search_error[0] = 0;
                }
            }

            draw_search_tooltip("Keeps the candidates that pass the rule");
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!memory_search.CanUndo());

            if (ImGui::Button("Undo"))
                memory_search.Undo();

            draw_search_tooltip("Undoes the last Apply");
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Bytes or Text", NULL, search_tab_request == 1 ? ImGuiTabItemFlags_SetSelected : 0))
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Type");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(search_combo_width(k_pattern_types));
            ImGui::Combo("##search_pattern_type", &search_pattern_type, k_pattern_types);

            if (search_pattern_type == 0)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("Example: 48 8B ?? ?? A? FF");
            }

            ImGui::PushFont(gui_default_font);
            ImGui::InputTextMultiline("##search_pattern", search_pattern, sizeof(search_pattern), ImVec2(-1, 70.0f));
            ImGui::PopFont();

            if (ImGui::Button("Find All"))
            {
                u32 start = 0;
                u32 size = 0;
                std::vector<u8> pattern;
                std::vector<u8> mask;

                if (!parse_u32(search_start, start) || !parse_u32(search_size, size))
                    set_search_error("Invalid range");
                else if (!parse_pattern(search_pattern, search_pattern_type, pattern, mask))
                    set_search_error("Invalid pattern");
                else if (!memory_search.FindPattern(memory_provider, editor->GetSource(), start, size, pattern, mask))
                    set_search_error("The range does not fit the source");
                else
                    search_error[0] = 0;
            }

            ImGui::EndTabItem();
        }

        search_tab_request = -1;
        ImGui::EndTabBar();
    }

    if (search_error[0] != 0)
        ImGui::TextColored(red, "%s", search_error);

    ImGui::Separator();
    ImGui::PushFont(gui_default_font);

    u32 count = memory_search.GetCandidateCount();
    ImGui::TextColored(violet, "%s", pattern_results ? "MATCHES" : "CANDIDATES"); ImGui::SameLine();
    ImGui::TextColored(white, "%u", count);

    if (count > MEMORY_SEARCH_MAX_VISIBLE_RESULTS)
    {
        ImGui::SameLine();
        ImGui::TextColored(gray, "(FIRST %d SHOWN)", MEMORY_SEARCH_MAX_VISIBLE_RESULTS);
    }

    if (ImGui::BeginTable("##search_results", pattern_results ? 2 : 4, ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
        ImVec2(0, -1)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("ADDRESS");

        if (pattern_results)
            ImGui::TableSetupColumn("BYTES", ImGuiTableColumnFlags_WidthStretch);
        else
        {
            ImGui::TableSetupColumn("VALUE");
            ImGui::TableSetupColumn("PREVIOUS");
            ImGui::TableSetupColumn("INITIAL", ImGuiTableColumnFlags_WidthStretch);
        }

        ImGui::TableHeadersRow();
        const std::vector<MemorySearchResult>& results = memory_search.GetResults();
        int width = memory_search.GetWidth();
        int bytes = pattern_results ? (int)MIN(memory_search.GetPatternSize(), 16U) : width;
        ImGuiListClipper clipper;
        clipper.Begin((int)results.size());

        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                const MemorySearchResult& result = results[row];
                GT_Debug_Memory_Address target = memory_search.GetSource();
                target.address = result.address;
                char address[16];
                snprintf(address, sizeof(address), "%08X", result.address);

                ImGui::PushID(row);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)cyan);

                if (ImGui::Selectable(address, false, ImGuiSelectableFlags_SpanAllColumns))
                    navigate_to(target);

                ImGui::PopStyleColor();
                draw_row_tooltip("Click to go there, right click for more");
                draw_search_result_menu(target, bytes);
                ImGui::TableNextColumn();

                if (pattern_results)
                    draw_search_bytes(target, bytes);
                else
                {
                    char text[32];
                    u64 value = 0;
                    GT_Debug_Memory_Status status;

                    if (read_value(target, width, memory_search.GetEndian(), value, status))
                    {
                        format_search_value(value, width, search_type, text, sizeof(text));
                        ImGui::TextColored(white, "%s", text);
                    }
                    else
                        ImGui::TextColored(gray, "--");

                    ImGui::TableNextColumn();
                    format_search_value(result.previous, width, search_type, text, sizeof(text));
                    ImGui::TextColored(orange, "%s", text);
                    ImGui::TableNextColumn();
                    format_search_value(result.initial, width, search_type, text, sizeof(text));
                    ImGui::TextColored(gray, "%s", text);
                }

                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();
    ImGui::End();
}

static void draw_search_bytes(const GT_Debug_Memory_Address& address, int count)
{
    u8 data[16];
    GT_Debug_Memory_Status status[16];
    memory_provider.ReadBlock(address, data, status, (u32)count, NULL);

    for (int i = 0; i < count; i++)
    {
        bool readable = status[i] == GT_DEBUG_MEMORY_VALID || status[i] == GT_DEBUG_MEMORY_READ_ONLY;

        if (i > 0)
            ImGui::SameLine(0.0f, ImGui::CalcTextSize(" ").x);

        if (readable)
            ImGui::TextColored(white, "%02X", data[i]);
        else
            ImGui::TextColored(gray, "--");
    }
}

static void draw_search_result_menu(const GT_Debug_Memory_Address& address, int size)
{
    if (!ImGui::BeginPopupContextItem("##search_result_menu"))
        return;

    u32 end = address.address + (u32)size - 1;
    ImGui::PushFont(gui_roboto_font);

    if (ImGui::MenuItem("Add Watch"))
    {
        add_watch(address);
        memory_watches.back().size = size >= 8 ? 3 : size >= 4 ? 2 : size >= 2 ? 1 : 0;
        memory_watches.back().endian = memory_search.GetEndian();
    }

    if (ImGui::MenuItem("Add Bookmark..."))
        request_bookmark(address, end);

    if (ImGui::MenuItem("Add Breakpoint") && !add_breakpoint(address, end))
        gui_notify(gui_NotificationWarning, NULL, "This memory has no linear, physical or I/O address");

    ImGui::PopFont();
    ImGui::EndPopup();
}

static void draw_search_tooltip(const char* text)
{
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        return;

    ImGui::SetTooltip("%s", text);
}

static void set_search_error(const char* text)
{
    strncpy_fit(search_error, text, sizeof(search_error));
}

static void set_default_search_range(MemEditor& editor)
{
    u32 start = editor.GetBufferBase();
    u32 size = editor.GetBufferSize();

    if (editor.GetSource().space == GT_DEBUG_MEMORY_REGION)
    {
        GT_Debug_Memory_Region region;

        if (memory_provider.GetRegionById(editor.GetSource().region, region) && region.size <= MEMORY_SEARCH_MAX_SIZE)
        {
            start = 0;
            size = region.size;
        }
    }

    snprintf(search_start, sizeof(search_start), "%08X", start);
    snprintf(search_size, sizeof(search_size), "%X", size);
    search_range_source = editor.GetSource();
    search_range_valid = true;
}

static bool parse_search_value(const char* text, int type, u64& value)
{
    bool negative = text[0] == '-';
    const char* digits = negative ? text + 1 : text;
    bool hex = type == SEARCH_TYPE_HEX || digits[0] == '$' || (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X'));

    if (digits[0] == 0)
        return false;

    if (hex)
    {
        if (!parse_u64(digits, value))
            return false;
    }
    else
    {
        char* end = NULL;
        unsigned long long parsed = strtoull(digits, &end, 10);

        if (end == digits || *end != 0)
            return false;

        value = (u64)parsed;
    }

    if (negative)
        value = (u64)0 - value;

    return true;
}

static void format_search_value(u64 value, int width, int type, char* text, size_t size)
{
    u64 mask = width >= 8 ? ~0ULL : ((1ULL << (width * 8)) - 1);
    value &= mask;

    if (type == SEARCH_TYPE_HEX)
        snprintf(text, size, "%0*llX", width * 2, (unsigned long long)value);
    else if (type == SEARCH_TYPE_SIGNED)
    {
        s64 signed_value = width == 1 ? (s64)(s8)value : width == 2 ? (s64)(s16)value : width == 4 ? (s64)(s32)value :
            (s64)value;
        snprintf(text, size, "%lld", (long long)signed_value);
    }
    else
        snprintf(text, size, "%llu", (unsigned long long)value);
}

static void draw_search_label(const char* label, bool spaced)
{
    ImGui::TableNextColumn();

    if (spaced)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
}

static float search_combo_width(const char* items)
{
    float width = 0.0f;

    for (const char* item = items; *item; item += strlen(item) + 1)
        width = MAX(width, ImGui::CalcTextSize(item).x);

    return width + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight();
}

static void draw_breakpoints_window()
{
    static const char* k_spaces[I386_BREAKPOINT_SPACE_COUNT] = { "LINEAR", "PHYSICAL", "I/O" };

    ImGui::SetNextWindowPos(ImVec2(151, 237), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(720, 360), ImGuiCond_FirstUseEver);
    ImGui::Begin("Memory Breakpoints", &show_breakpoints);

    I386* cpu = emu_get_core()->GetI386();
    std::vector<I386_Breakpoint>* breakpoints = cpu->GetBreakpoints();

    ImGui::BeginDisabled(!IsValidPointer(active_editor()));

    if (ImGui::Button("Add Selection"))
        add_selection_breakpoint();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Adds a read and write breakpoint on the selection\nRead and write breakpoints stop after the access");

    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(!has_memory_breakpoints());

    if (ImGui::Button("Remove All"))
        remove_memory_breakpoints();

    ImGui::EndDisabled();


    ImGui::PushFont(gui_default_font);

    if (ImGui::BeginTable("##memory_breakpoints", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("ON");
        ImGui::TableSetupColumn("SPACE");
        ImGui::TableSetupColumn("RANGE");
        ImGui::TableSetupColumn("R");
        ImGui::TableSetupColumn("W", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int remove = -1;

        for (size_t i = 0; i < breakpoints->size(); i++)
        {
            I386_Breakpoint& breakpoint = (*breakpoints)[i];

            if (breakpoint.type == I386_BREAKPOINT_EXECUTE)
                continue;

            int digits = breakpoint.space == I386_BREAKPOINT_IO ? 4 : 8;
            bool io = breakpoint.space == I386_BREAKPOINT_IO;
            bool read = (breakpoint.type & I386_BREAKPOINT_READ) != 0;
            bool write = (breakpoint.type & I386_BREAKPOINT_WRITE) != 0;
            char range[32];

            if (breakpoint.range)
                snprintf(range, sizeof(range), "%0*X-%0*X", digits, breakpoint.address1, digits, breakpoint.address2);
            else
                snprintf(range, sizeof(range), "%0*X", digits, breakpoint.address1);

            ImGui::PushID((int)i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(breakpoint.enabled ? green : gray, "%s", breakpoint.enabled ? "ON" : "OFF");

            if (ImGui::IsItemClicked())
                breakpoint.enabled = !breakpoint.enabled;

            draw_row_tooltip("Click to toggle");

            ImGui::TableNextColumn();
            ImGui::TextColored(violet, "%s", k_spaces[breakpoint.space % I386_BREAKPOINT_SPACE_COUNT]);
            ImGui::TableNextColumn();
            ImGui::TextColored(io ? white : cyan, "%s", range);

            if (!io && ImGui::IsItemClicked())
            {
                GT_Debug_Memory_Address target = { };
                target.space = breakpoint.space == I386_BREAKPOINT_LINEAR ? GT_DEBUG_MEMORY_LINEAR : GT_DEBUG_MEMORY_PHYSICAL;
                target.address = breakpoint.address1;
                target.segment_register = -1;
                navigate_to(target);
            }

            draw_row_tooltip(io ? "Right click to remove" : "Click to go to the address, right click to remove");

            if (ImGui::BeginPopupContextItem("##breakpoint_menu"))
            {
                ImGui::PushFont(gui_roboto_font);

                if (ImGui::MenuItem("Remove Breakpoint"))
                    remove = (int)i;

                ImGui::PopFont();
                ImGui::EndPopup();
            }

            ImGui::TableNextColumn();
            ImGui::TextColored(read ? green : gray, "%s", read ? "R" : "-");

            if (ImGui::IsItemClicked() && write)
                breakpoint.type ^= I386_BREAKPOINT_READ;

            draw_row_tooltip("Click to toggle read");
            ImGui::TableNextColumn();
            ImGui::TextColored(write ? green : gray, "%s", write ? "W" : "-");

            if (ImGui::IsItemClicked() && read)
                breakpoint.type ^= I386_BREAKPOINT_WRITE;

            draw_row_tooltip("Click to toggle write");
            ImGui::PopID();
        }

        if (remove >= 0)
        {
            I386_Breakpoint breakpoint = (*breakpoints)[remove];
            cpu->RemoveBreakpoint(breakpoint.address1, breakpoint.address2, breakpoint.type, breakpoint.space);
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();
    ImGui::End();
}

static void add_selection_bookmark()
{
    MemEditor* editor = active_editor();

    if (!IsValidPointer(editor))
        return;

    u32 start = 0;
    u32 end = 0;
    editor->GetSelection(start, end);
    GT_Debug_Memory_Address address = editor->GetSource();
    address.address = start;
    request_bookmark(address, end);
}

static void add_selection_watch()
{
    MemEditor* editor = active_editor();

    if (!IsValidPointer(editor))
        return;

    u32 start = 0;
    u32 end = 0;
    editor->GetSelection(start, end);
    GT_Debug_Memory_Address address = editor->GetSource();
    address.address = start;
    add_watch(address);
}

static void add_selection_breakpoint()
{
    MemEditor* editor = active_editor();

    if (!IsValidPointer(editor))
        return;

    u32 start = 0;
    u32 end = 0;
    editor->GetSelection(start, end);
    GT_Debug_Memory_Address address = editor->GetSource();
    address.address = start;

    if (!add_breakpoint(address, end))
        gui_notify(gui_NotificationWarning, NULL, "This memory has no linear, physical or I/O address");
}

static bool has_memory_breakpoints()
{
    std::vector<I386_Breakpoint>* breakpoints = emu_get_core()->GetI386()->GetBreakpoints();

    for (size_t i = 0; i < breakpoints->size(); i++)
    {
        if ((*breakpoints)[i].type != I386_BREAKPOINT_EXECUTE)
            return true;
    }

    return false;
}

static void remove_memory_breakpoints()
{
    I386* cpu = emu_get_core()->GetI386();
    std::vector<I386_Breakpoint> breakpoints = *cpu->GetBreakpoints();

    for (size_t i = 0; i < breakpoints.size(); i++)
    {
        if (breakpoints[i].type != I386_BREAKPOINT_EXECUTE)
            cpu->RemoveBreakpoint(breakpoints[i].address1, breakpoints[i].address2, breakpoints[i].type,
                breakpoints[i].space);
    }
}

static void request_bookmark(const GT_Debug_Memory_Address& address, u32 end)
{
    memset(&new_bookmark, 0, sizeof(new_bookmark));
    new_bookmark.address = address;
    new_bookmark.end = end;
    snprintf(new_bookmark.name, sizeof(new_bookmark.name), "Bookmark_%08X", address.address);
    show_bookmark_popup = true;
}

static void draw_bookmark_popup()
{
    if (show_bookmark_popup)
    {
        ImGui::OpenPopup("Add Bookmark");
        show_bookmark_popup = false;
    }

    if (!ImGui::BeginPopupModal("Add Bookmark", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    char address[128];
    format_address(new_bookmark.address, address, sizeof(address));

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Name");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);

    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();

    bool add = ImGui::InputText("##new_bookmark_name", new_bookmark.name, sizeof(new_bookmark.name),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

    ImGui::PushFont(gui_default_font);
    ImGui::TextColored(violet, "ADDRESS"); ImGui::SameLine();
    ImGui::TextColored(cyan, "%s", address);
    ImGui::PopFont();
    ImGui::Separator();

    add = ImGui::Button("OK", ImVec2(90, 0)) || add;

    if (add)
    {
        memory_bookmarks.push_back(new_bookmark);
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(90, 0)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

static void draw_row_tooltip(const char* text)
{
    if (!ImGui::IsItemHovered() || ImGui::IsPopupOpen((const char*)NULL, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        return;

    ImGui::PushFont(gui_roboto_font);
    ImGui::SetTooltip("%s", text);
    ImGui::PopFont();
}

static void goto_bookmark(MemEditor& editor, const MemoryBookmark& bookmark)
{
    editor.SetFollow(false);
    editor.SetSource(bookmark.address);
    editor.JumpToAddress(bookmark.address.address);
    editor.SetSelection(bookmark.address.address, bookmark.end);
}

static bool read_value(const GT_Debug_Memory_Address& address, int size, int endian, u64& value, GT_Debug_Memory_Status& status)
{
    u8 data[8];
    GT_Debug_Memory_Status statuses[8];
    memory_provider.ReadBlock(address, data, statuses, size, NULL);
    value = 0;
    status = GT_DEBUG_MEMORY_VALID;

    for (int i = 0; i < size; i++)
    {
        if (statuses[i] != GT_DEBUG_MEMORY_VALID && statuses[i] != GT_DEBUG_MEMORY_READ_ONLY)
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

static void format_address(const GT_Debug_Memory_Address& address, char* text, size_t text_size)
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

    snprintf(text, text_size, "%s:%08X", DebugMemoryProvider::GetSpaceName(address.space), address.address);
}

static void format_value(u64 value, int size, int format, char* text, size_t text_size)
{
    if (format == 0)
        snprintf(text, text_size, "%0*llX", size * 2, (unsigned long long)value);
    else if (format == 1)
        snprintf(text, text_size, "%llu", (unsigned long long)value);
    else if (format == 2)
    {
        s64 signed_value = size == 1 ? (s8)value : size == 2 ? (s16)value : size == 4 ? (s32)value : (s64)value;
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

    editor->SetFollow(false);
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

    if (length >= 2 && input[0] == '0' && (input[1] == 'x' || input[1] == 'X'))
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

static bool parse_pattern(const char* text, int type, std::vector<u8>& pattern, std::vector<u8>& mask)
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
        char* converted = SDL_iconv_string("SHIFT-JIS", "UTF-8", text, strlen(text) + 1);

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

int gui_debug_memory_get_area_count(void)
{
    return GUI_DEBUG_MEMORY_AREA_REGIONS + memory_provider.GetRegionCount();
}

const char* gui_debug_memory_region_group(int region_id)
{
    static const char* k_groups[4] = { "memory", "rom", "media", "cpu_window" };

    if (region_id >= GT_DEBUG_REGION_MEDIA_IMAGE)
        return k_groups[MEMORY_GROUP_MEDIA];

    for (size_t i = 0; i < sizeof(k_memory_sources) / sizeof(k_memory_sources[0]); i++)
    {
        if (k_memory_sources[i].id == region_id)
            return k_groups[k_memory_sources[i].group];
    }

    return NULL;
}

const char* gui_debug_memory_region_description(int region_id)
{
    if (region_id == GT_DEBUG_REGION_MEDIA_IMAGE)
        return "The loaded media image, as stored in the file";

    if (region_id >= GT_DEBUG_REGION_FLOPPY_IMAGE)
        return "The floppy disk image in the drive, in D77 layout";

    for (size_t i = 0; i < sizeof(k_memory_sources) / sizeof(k_memory_sources[0]); i++)
    {
        if (k_memory_sources[i].id == region_id)
            return k_memory_sources[i].description;
    }

    return NULL;
}

bool gui_debug_memory_get_area_at(int index, GuiDebugMemoryArea& area)
{
    if (index < GUI_DEBUG_MEMORY_AREA_REGIONS)
        return gui_debug_memory_get_area(index, area);

    GT_Debug_Memory_Region region;

    if (!memory_provider.GetRegion(index - GUI_DEBUG_MEMORY_AREA_REGIONS, region))
        return false;

    return gui_debug_memory_get_area(GUI_DEBUG_MEMORY_AREA_REGIONS + region.id - 1, area);
}

bool gui_debug_memory_get_area(int id, GuiDebugMemoryArea& area)
{
    memset(&area, 0, sizeof(area));
    area.id = id;
    area.source.segment_register = -1;
    area.source.region = -1;

    switch (id)
    {
        case GUI_DEBUG_MEMORY_AREA_LINEAR:
            area.source.space = GT_DEBUG_MEMORY_LINEAR;
            strncpy_fit(area.name, "LINEAR", sizeof(area.name));
            area.size = 0x100000000ULL;
            area.flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_WRITABLE | GT_DEBUG_REGION_EXECUTABLE;
            return true;
        case GUI_DEBUG_MEMORY_AREA_PHYSICAL:
            area.source.space = GT_DEBUG_MEMORY_PHYSICAL;
            strncpy_fit(area.name, "PHYSICAL", sizeof(area.name));
            area.size = 0x100000000ULL;
            area.flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_WRITABLE | GT_DEBUG_REGION_EXECUTABLE;
            return true;
        case GUI_DEBUG_MEMORY_AREA_IO:
            area.source.space = GT_DEBUG_MEMORY_IO;
            strncpy_fit(area.name, "I/O PORTS", sizeof(area.name));
            area.size = 0x10000;
            area.flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_MMIO;
            return true;
        default:
            break;
    }

    GT_Debug_Memory_Region region;

    if (id < GUI_DEBUG_MEMORY_AREA_REGIONS ||
        !memory_provider.GetRegionById(id - GUI_DEBUG_MEMORY_AREA_REGIONS + 1, region))
        return false;

    area.source.space = GT_DEBUG_MEMORY_REGION;
    area.source.region = region.id;
    strncpy_fit(area.name, region.name, sizeof(area.name));
    area.size = region.size;
    area.flags = region.flags;
    area.physical_base = region.physical_base;
    return true;
}

bool gui_debug_memory_same_source(const GT_Debug_Memory_Address& a, const GT_Debug_Memory_Address& b)
{
    if (a.space != b.space)
        return false;

    if (a.space == GT_DEBUG_MEMORY_REGION)
        return a.region == b.region;

    if (a.space == GT_DEBUG_MEMORY_LOGICAL)
        return a.segment_register == b.segment_register && (a.segment_register >= 0 || a.segment == b.segment);

    return true;
}

void gui_debug_memory_read(const GT_Debug_Memory_Address& address, u8* data, GT_Debug_Memory_Status* status, u32 size)
{
    memory_provider.ReadBlock(address, data, status, size, NULL);
}

bool gui_debug_memory_write(const GT_Debug_Memory_Address& address, const u8* data, u32 size)
{
    if (address.space == GT_DEBUG_MEMORY_IO)
        return false;

    return memory_provider.WriteNow(address, data, size);
}

bool gui_debug_memory_translate(const GT_Debug_Memory_Address& address, GT_Debug_Memory_Translation& translation)
{
    return memory_provider.Translate(address, translation);
}

bool gui_debug_memory_select_range(const GT_Debug_Memory_Address& source, u32 start, u32 end)
{
    MemEditor* editor = active_editor();

    if (!IsValidPointer(editor) || start > end)
        return false;

    config_debug.show_memory = true;
    editor->SetSource(source);
    editor->JumpToAddress(start);
    editor->SetSelection(start, end);
    return true;
}

bool gui_debug_memory_get_selection(const GT_Debug_Memory_Address& source, u32& start, u32& end)
{
    MemEditor* editor = active_editor();

    if (!IsValidPointer(editor) || !gui_debug_memory_same_source(editor->GetSource(), source))
        return false;

    editor->GetSelection(start, end);
    return true;
}

int gui_debug_memory_set_selection_value(const GT_Debug_Memory_Address& source, u8 value)
{
    u32 start = 0;
    u32 end = 0;

    if (!gui_debug_memory_get_selection(source, start, end))
        return 0;

    u64 size = (u64)end - start + 1;

    if (size > MEMORY_SEARCH_MAX_SIZE)
        return 0;

    std::vector<u8> data((size_t)size, value);
    GT_Debug_Memory_Address address = source;
    address.address = start;

    if (!gui_debug_memory_write(address, &data[0], (u32)size))
        return 0;

    return (int)size;
}

void gui_debug_memory_add_bookmark(const GT_Debug_Memory_Address& address, u32 end, const char* name)
{
    add_bookmark(address, end);

    if (IsValidPointer(name) && name[0] != 0)
        strncpy_fit(memory_bookmarks.back().name, name, sizeof(memory_bookmarks.back().name));
}

bool gui_debug_memory_remove_bookmark(const GT_Debug_Memory_Address& address)
{
    for (size_t i = 0; i < memory_bookmarks.size(); i++)
    {
        const MemoryBookmark& bookmark = memory_bookmarks[i];

        if (gui_debug_memory_same_source(bookmark.address, address) && bookmark.address.address == address.address)
        {
            memory_bookmarks.erase(memory_bookmarks.begin() + i);
            return true;
        }
    }

    return false;
}

void gui_debug_memory_get_bookmarks(const GT_Debug_Memory_Address& source, std::vector<GuiDebugMemoryBookmark>& bookmarks)
{
    bookmarks.clear();

    for (size_t i = 0; i < memory_bookmarks.size(); i++)
    {
        const MemoryBookmark& item = memory_bookmarks[i];

        if (!gui_debug_memory_same_source(item.address, source))
            continue;

        GuiDebugMemoryBookmark bookmark;
        bookmark.address = item.address;
        bookmark.end = item.end;
        strncpy_fit(bookmark.name, item.name, sizeof(bookmark.name));
        bookmarks.push_back(bookmark);
    }
}

bool gui_debug_memory_add_watch(const GT_Debug_Memory_Address& address, const char* name, int size)
{
    int size_index = size == 2 ? 1 : size == 4 ? 2 : size == 8 ? 3 : size == 1 ? 0 : -1;

    if (size_index < 0)
        return false;

    add_watch(address);
    MemoryWatch& watch = memory_watches.back();
    watch.size = size_index;
    GT_Debug_Memory_Status status;
    watch.valid = read_value(watch.address, size, watch.endian, watch.value, status);
    watch.previous = watch.value;
    watch.frozen_value = watch.value;

    if (IsValidPointer(name) && name[0] != 0)
        strncpy_fit(watch.name, name, sizeof(watch.name));

    return true;
}

bool gui_debug_memory_remove_watch(const GT_Debug_Memory_Address& address)
{
    for (size_t i = 0; i < memory_watches.size(); i++)
    {
        const MemoryWatch& watch = memory_watches[i];

        if (gui_debug_memory_same_source(watch.address, address) && watch.address.address == address.address)
        {
            memory_watches.erase(memory_watches.begin() + i);
            return true;
        }
    }

    return false;
}

void gui_debug_memory_get_watches(const GT_Debug_Memory_Address& source, std::vector<GuiDebugMemoryWatch>& watches)
{
    watches.clear();

    for (size_t i = 0; i < memory_watches.size(); i++)
    {
        const MemoryWatch& item = memory_watches[i];

        if (!gui_debug_memory_same_source(item.address, source))
            continue;

        GuiDebugMemoryWatch watch;
        watch.address = item.address;
        strncpy_fit(watch.name, item.name, sizeof(watch.name));
        watch.size = watch_size_bytes(item.size);
        GT_Debug_Memory_Status status;
        watch.valid = read_value(item.address, watch.size, item.endian, watch.value, status);
        watch.freeze = item.freeze;
        watches.push_back(watch);
    }
}

bool gui_debug_memory_search_capture(const GT_Debug_Memory_Address& source, u32 start, u32 size, int width)
{
    return memory_search.Start(memory_provider, source, start, size, width, 0, false, true, false, 0);
}

int gui_debug_memory_search(const GT_Debug_Memory_Address& source, int comparison, bool previous, u64 value,
    bool signed_values, std::vector<GuiDebugMemorySearchResult>& results)
{
    results.clear();

    if (!memory_search.IsActive() || !gui_debug_memory_same_source(memory_search.GetSource(), source))
        return -1;

    if (!memory_search.FilterOperator(memory_provider, comparison, previous ? SEARCH_REFERENCE_PREVIOUS :
        SEARCH_REFERENCE_VALUE, value, signed_values))
        return -1;

    const std::vector<MemorySearchResult>& items = memory_search.GetResults();

    for (size_t i = 0; i < items.size(); i++)
    {
        GuiDebugMemorySearchResult result;
        result.address = items[i].address;
        result.current = items[i].current;
        result.previous = items[i].previous;
        results.push_back(result);
    }

    return (int)memory_search.GetCandidateCount();
}

int gui_debug_memory_find(const GT_Debug_Memory_Address& source, u32 start, u32 size, const std::vector<u8>& pattern,
    bool case_sensitive, std::vector<u32>& addresses, int max)
{
    addresses.clear();

    if (pattern.empty() || size == 0 || size > MEMORY_SEARCH_MAX_SIZE || pattern.size() > size)
        return -1;

    std::vector<u8> data(size);
    std::vector<GT_Debug_Memory_Status> status(size);
    GT_Debug_Memory_Address address = source;
    address.address = start;
    memory_provider.ReadBlock(address, &data[0], &status[0], size, NULL);
    int count = 0;

    for (u32 offset = 0; offset + pattern.size() <= size; offset++)
    {
        bool match = true;

        for (u32 i = 0; i < pattern.size(); i++)
        {
            GT_Debug_Memory_Status byte_status = status[offset + i];
            u8 value = data[offset + i];
            u8 expected = pattern[i];

            if (!case_sensitive)
            {
                value = (u8)tolower(value);
                expected = (u8)tolower(expected);
            }

            if ((byte_status != GT_DEBUG_MEMORY_VALID && byte_status != GT_DEBUG_MEMORY_READ_ONLY) || value != expected)
            {
                match = false;
                break;
            }
        }

        if (!match)
            continue;

        if (count < max)
            addresses.push_back(start + offset);

        count++;
    }

    return count;
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
            u32 start = 0;
            u32 end = 0;
            memory_editor[i].GetSelection(start, end);
            settings.source = memory_editor[i].GetSource();
            settings.source.address = start;
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
}

bool gui_debug_memory_load_settings(std::istream& stream)
{
    int view_count = 0;

    if (!read_count(stream, view_count, sizeof(MemoryViewSettings)) || view_count > MEMORY_VIEW_COUNT)
        return false;

    MemoryViewSettings view_settings[MEMORY_VIEW_COUNT];
    memset(view_settings, 0, sizeof(view_settings));

    for (int i = 0; i < view_count; i++)
    {
        if (!read_data(stream, &view_settings[i], sizeof(view_settings[i])) ||
            view_settings[i].source.space < 0 || view_settings[i].source.space >= GT_DEBUG_MEMORY_SPACE_COUNT)
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

        if (bookmarks[i].address.space < 0 || bookmarks[i].address.space >= GT_DEBUG_MEMORY_SPACE_COUNT)
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

        watches[i].freeze = false;

        if (watches[i].address.space < 0 || watches[i].address.space >= GT_DEBUG_MEMORY_SPACE_COUNT ||
            watches[i].size < 0 || watches[i].size > 3 || watches[i].format < 0 || watches[i].format > 4 ||
            watches[i].endian < 0 || watches[i].endian > 1)
            return false;
    }

    for (int i = 0; i < MEMORY_VIEW_COUNT; i++)
        memory_editor[i].SetAvailable(false);

    for (int i = 0; i < view_count; i++)
    {
        memory_editor[i].SetAvailable(true);
        memory_editor[i].SetOptions(view_settings[i].options);
        memory_editor[i].SetSource(view_settings[i].source);
        memory_editor[i].JumpToAddress(view_settings[i].source.address, false);
    }

    if (view_count == 0)
    {
        memory_editor[0].SetAvailable(true);
        memory_editor[0].Reset();
    }

    current_editor = 0;
    select_editor = 0;
    memory_bookmarks.swap(bookmarks);
    memory_watches.swap(watches);
    return true;
}

static bool read_data(std::istream& stream, void* data, size_t size)
{
    stream.read((char*)data, (std::streamsize)size);
    return !stream.fail() && stream.gcount() == (std::streamsize)size;
}

static bool read_count(std::istream& stream, int& count, size_t record_size)
{
    if (!read_data(stream, &count, sizeof(count)) || count < 0 || count > MEMORY_SETTINGS_MAX_RECORDS ||
        record_size == 0)
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

MemorySearch::MemorySearch()
{
    Reset();
}

void MemorySearch::Reset()
{
    memset(&m_source, 0, sizeof(m_source));

    m_start = 0;
    m_size = 0;
    m_width = 1;
    m_endian = 0;
    m_signed = false;
    m_aligned = true;
    m_candidate_count = 0;
    m_pattern_size = 0;
    m_active = false;
    m_can_undo = false;

    m_initial_data.clear();
    m_previous_data.clear();
    m_candidate_bits.clear();
    m_undo_data.clear();
    m_undo_candidate_bits.clear();
    m_results.clear();
}

bool MemorySearch::Start(DebugMemoryProvider& provider, const GT_Debug_Memory_Address& source, u32 start, u32 size,
    int width, int endian, bool signed_values, bool aligned, bool known, u64 known_value)
{
    Reset();

    if (size == 0 || size > MEMORY_SEARCH_MAX_SIZE || (u64)start + size > (u64)provider.GetAddressLimit(source) + 1)
        return false;

    m_source = source;
    m_start = start;
    m_size = size;
    m_width = width;
    m_endian = endian;
    m_signed = signed_values;
    m_aligned = aligned;

    m_initial_data.resize(size);
    m_previous_data.resize(size);
    m_candidate_bits.resize((size + 7) / 8, 0);

    std::vector<GT_Debug_Memory_Status> status(size);
    GT_Debug_Memory_Address address = source;
    address.address = start;
    provider.ReadBlock(address, &m_initial_data[0], &status[0], size, NULL);
    m_previous_data = m_initial_data;

    u32 step = aligned ? (u32)width : 1;

    for (u32 offset = 0; offset + width <= size; offset += step)
    {
        if (!IsValueAvailable(status, offset))
            continue;

        u64 value = ReadValue(m_initial_data, offset);

        if (!known || value == MaskValue(known_value))
        {
            SetCandidate(offset, true);
            m_candidate_count++;
        }
    }

    m_active = true;
    BuildResults(m_initial_data, m_previous_data);
    return true;
}

bool MemorySearch::FilterOperator(DebugMemoryProvider& provider, int comparison, int reference, u64 value,
    bool signed_values)
{
    if (!m_active)
        return false;

    std::vector<u8> current(m_size);
    std::vector<GT_Debug_Memory_Status> status(m_size);
    GT_Debug_Memory_Address address = m_source;
    address.address = m_start;
    provider.ReadBlock(address, &current[0], &status[0], m_size, NULL);

    m_undo_data = m_previous_data;
    m_undo_candidate_bits = m_candidate_bits;
    m_can_undo = true;
    m_candidate_count = 0;

    u32 step = m_aligned ? (u32)m_width : 1;

    for (u32 offset = 0; offset + m_width <= m_size; offset += step)
    {
        if (!IsCandidate(offset))
            continue;

        if (!IsValueAvailable(status, offset))
        {
            SetCandidate(offset, false);
            continue;
        }

        u64 previous = ReadValue(m_previous_data, offset);
        u64 target = value;

        if (reference == SEARCH_REFERENCE_PREVIOUS)
            target = previous;
        else if (reference == SEARCH_REFERENCE_INITIAL)
            target = ReadValue(m_initial_data, offset);
        else if (reference == SEARCH_REFERENCE_PREVIOUS_PLUS)
            target = previous + value;

        bool keep = CompareOperator(ReadValue(current, offset), target, comparison, signed_values);
        SetCandidate(offset, keep);

        if (keep)
            m_candidate_count++;
    }

    BuildResults(current, m_previous_data);
    m_previous_data.swap(current);
    return true;
}

void MemorySearch::Undo()
{
    if (!m_can_undo)
        return;

    m_previous_data.swap(m_undo_data);
    m_candidate_bits.swap(m_undo_candidate_bits);
    m_candidate_count = 0;

    for (u32 offset = 0; offset < m_size; offset++)
    {
        if (IsCandidate(offset))
            m_candidate_count++;
    }

    m_can_undo = false;
    BuildResults(m_undo_data, m_previous_data);
}

bool MemorySearch::FindPattern(DebugMemoryProvider& provider, const GT_Debug_Memory_Address& source, u32 start,
    u32 size, const std::vector<u8>& pattern, const std::vector<u8>& mask)
{
    Reset();

    if (pattern.empty() || pattern.size() != mask.size() || size == 0 || size > MEMORY_SEARCH_MAX_SIZE ||
        pattern.size() > size || (u64)start + size > (u64)provider.GetAddressLimit(source) + 1)
        return false;

    m_source = source;
    m_start = start;
    m_size = size;
    m_width = 1;
    m_pattern_size = (u32)pattern.size();

    m_initial_data.resize(size);

    std::vector<GT_Debug_Memory_Status> status(size);
    GT_Debug_Memory_Address address = source;
    address.address = start;
    provider.ReadBlock(address, &m_initial_data[0], &status[0], size, NULL);

    for (u32 offset = 0; offset + pattern.size() <= size; offset++)
    {
        bool match = true;

        for (u32 i = 0; i < pattern.size(); i++)
        {
            if ((status[offset + i] != GT_DEBUG_MEMORY_VALID && status[offset + i] != GT_DEBUG_MEMORY_READ_ONLY) ||
                (m_initial_data[offset + i] & mask[i]) != (pattern[i] & mask[i]))
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

const GT_Debug_Memory_Address& MemorySearch::GetSource() const
{
    return m_source;
}

const std::vector<MemorySearchResult>& MemorySearch::GetResults() const
{
    return m_results;
}

u32 MemorySearch::GetCandidateCount() const
{
    return m_candidate_count;
}

int MemorySearch::GetWidth() const
{
    return m_width;
}

int MemorySearch::GetEndian() const
{
    return m_endian;
}

u32 MemorySearch::GetPatternSize() const
{
    return m_pattern_size;
}

bool MemorySearch::IsActive() const
{
    return m_active;
}

bool MemorySearch::CanUndo() const
{
    return m_can_undo;
}

bool MemorySearch::IsValueAvailable(const std::vector<GT_Debug_Memory_Status>& status, u32 offset) const
{
    for (int i = 0; i < m_width; i++)
    {
        GT_Debug_Memory_Status byte_status = status[offset + i];

        if (byte_status != GT_DEBUG_MEMORY_VALID && byte_status != GT_DEBUG_MEMORY_READ_ONLY)
            return false;
    }

    return true;
}

u64 MemorySearch::ReadValue(const std::vector<u8>& data, u32 offset) const
{
    u64 value = 0;

    for (int i = 0; i < m_width; i++)
    {
        int source = m_endian == 0 ? i : m_width - i - 1;
        value |= (u64)data[offset + source] << (i * 8);
    }

    return value;
}

u64 MemorySearch::MaskValue(u64 value) const
{
    if (m_width >= 8)
        return value;

    return value & ((1ULL << (m_width * 8)) - 1);
}

s64 MemorySearch::GetSignedValue(u64 value) const
{
    switch (m_width)
    {
        case 1: return (s8)value;
        case 2: return (s16)value;
        case 4: return (s32)value;
        default: return (s64)value;
    }
}

bool MemorySearch::CompareOperator(u64 current, u64 reference, int comparison, bool signed_values) const
{
    current = MaskValue(current);
    reference = MaskValue(reference);

    if (signed_values)
    {
        s64 left = GetSignedValue(current);
        s64 right = GetSignedValue(reference);

        switch (comparison)
        {
            case 0: return left < right;
            case 1: return left > right;
            case 2: return left == right;
            case 3: return left != right;
            case 4: return left <= right;
            case 5: return left >= right;
            default: return false;
        }
    }

    switch (comparison)
    {
        case 0: return current < reference;
        case 1: return current > reference;
        case 2: return current == reference;
        case 3: return current != reference;
        case 4: return current <= reference;
        case 5: return current >= reference;
        default: return false;
    }
}

bool MemorySearch::IsCandidate(u32 offset) const
{
    return offset < m_size && (m_candidate_bits[offset >> 3] & (1U << (offset & 7))) != 0;
}

void MemorySearch::SetCandidate(u32 offset, bool candidate)
{
    u8 mask = (u8)(1U << (offset & 7));

    if (candidate)
        m_candidate_bits[offset >> 3] |= mask;
    else
        m_candidate_bits[offset >> 3] &= (u8)~mask;
}

void MemorySearch::BuildResults(const std::vector<u8>& current, const std::vector<u8>& previous)
{
    m_results.clear();
    u32 step = m_aligned ? (u32)m_width : 1;

    for (u32 offset = 0; offset + m_width <= m_size; offset += step)
    {
        if (!IsCandidate(offset))
            continue;

        MemorySearchResult result;
        result.address = m_start + offset;
        result.initial = ReadValue(m_initial_data, offset);
        result.previous = ReadValue(previous, offset);
        result.current = ReadValue(current, offset);
        m_results.push_back(result);

        if ((int)m_results.size() >= MEMORY_SEARCH_MAX_VISIBLE_RESULTS)
            break;
    }
}
