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

#include <algorithm>
#include <istream>
#include <ostream>
#include <string>
#include <SDL3/SDL.h>

#include "gui_debug_memeditor.h"
#include "gui_debug_constants.h"
#include "gui_debug_memory_provider.h"
#include "i386.h"
#include "imgui.h"

class MemoryExpressionParser
{
public:
    MemoryExpressionParser(const char* expression, DebugMemoryProvider* provider)
    {
        m_expression = expression;
        m_provider = provider;
        m_valid = true;
    }

    bool Parse(u32& value)
    {
        u64 result = ParseAddSubtract();
        SkipSpaces();
        if (!m_valid || *m_expression != 0 || result > 0xFFFFFFFFULL)
            return false;
        value = (u32)result;
        return true;
    }

private:
    void SkipSpaces()
    {
        while (*m_expression == ' ' || *m_expression == '\t')
            m_expression++;
    }

    u64 ParseAddSubtract()
    {
        u64 value = ParseMultiplyDivide();
        for (;;)
        {
            SkipSpaces();
            char operation = *m_expression;
            if (operation != '+' && operation != '-')
                return value;
            m_expression++;
            u64 right = ParseMultiplyDivide();
            value = operation == '+' ? value + right : value - right;
        }
    }

    u64 ParseMultiplyDivide()
    {
        u64 value = ParsePrimary();
        for (;;)
        {
            SkipSpaces();
            char operation = *m_expression;
            if (operation != '*' && operation != '/')
                return value;
            m_expression++;
            u64 right = ParsePrimary();
            if (operation == '/' && right == 0)
            {
                m_valid = false;
                return 0;
            }
            value = operation == '*' ? value * right : value / right;
        }
    }

    u64 ParsePrimary()
    {
        SkipSpaces();
        if (*m_expression == '(')
        {
            m_expression++;
            u64 value = ParseAddSubtract();
            SkipSpaces();
            if (*m_expression != ')')
            {
                m_valid = false;
                return 0;
            }
            m_expression++;
            return value;
        }
        if (*m_expression == '-')
        {
            m_expression++;
            return (u32)(0 - (u32)ParsePrimary());
        }

        if (isalpha((unsigned char)*m_expression) || *m_expression == '_')
        {
            char name[32];
            int length = 0;
            while ((isalnum((unsigned char)*m_expression) ||
                *m_expression == '_') && length < (int)sizeof(name) - 1)
            {
                name[length++] = *m_expression++;
            }
            name[length] = 0;
            u32 value = 0;
            if (!IsValidPointer(m_provider) ||
                !m_provider->GetRegisterValue(name, value))
                m_valid = false;
            return value;
        }

        const char* start = m_expression;
        if (*m_expression == '$')
            m_expression++;
        else if (m_expression[0] == '0' &&
            (m_expression[1] == 'x' || m_expression[1] == 'X'))
            m_expression += 2;

        const char* digits = m_expression;
        while (is_hex_digit(*m_expression))
            m_expression++;
        if (digits == m_expression)
        {
            m_valid = false;
            return 0;
        }

        u32 value = 0;
        if (!parse_hex_string(digits, (size_t)(m_expression - digits), &value))
        {
            m_expression = start;
            m_valid = false;
        }
        return value;
    }

private:
    const char* m_expression;
    DebugMemoryProvider* m_provider;
    bool m_valid;
};

static bool equal_prefix(const std::string& left, const char* right)
{
    if (left.length() != strlen(right))
        return false;
    for (size_t i = 0; i < left.length(); i++)
    {
        if (toupper((unsigned char)left[i]) != toupper((unsigned char)right[i]))
            return false;
    }
    return true;
}

MemEditor::MemEditor()
{
    InitPointer(m_provider);
    m_id = 0;
    m_available = false;
    Reset();
}

MemEditor::~MemEditor()
{
}

void MemEditor::Init(DebugMemoryProvider* provider, int id)
{
    m_provider = provider;
    m_id = id;
    m_data.resize(WINDOW_SIZE);
    m_previous.resize(WINDOW_SIZE);
    m_status.resize(WINDOW_SIZE);
    m_available = true;
    Reset();
}

void MemEditor::Reset()
{
    memset(&m_source, 0, sizeof(m_source));
    m_source.space = GT_DEBUG_MEMORY_PHYSICAL;
    m_source.segment_register = I386_SEGMENT_CS;
    m_window_base = 0;
    m_selection_start = 0;
    m_selection_end = 0;
    m_editing_address = 0xFFFFFFFF;
    memset(m_history, 0, sizeof(m_history));
    m_history_count = 0;
    m_history_position = -1;
    m_update_counter = 0;
    snprintf(m_address_input, sizeof(m_address_input), "00000000");
    m_edit_buffer[0] = 0;
    memset(&m_options, 0, sizeof(m_options));
    m_options.bytes_per_row = 16;
    m_options.uppercase_hex = true;
    m_options.gray_out_zeros = true;
    m_options.auto_refresh = true;
    m_options.refresh_rate = 15;
    m_options.text_encoding = 0;
    m_options.preview_endian = 0;
    memset(&m_block_info, 0, sizeof(m_block_info));
    m_has_snapshot = false;
    m_refresh_requested = true;
    m_edit_focus = false;
    m_drag_selecting = false;
    m_follow_expression = false;
    m_bookmark_request = false;
    m_watch_request = false;
    m_breakpoint_request = false;
    m_request_end = 0;
    UpdateTitle();
}

void MemEditor::Update()
{
    if (!m_available || !IsValidPointer(m_provider))
        return;

    m_update_counter++;
    int refresh_rate = CLAMP(m_options.refresh_rate, 1, 120);
    if (m_follow_expression && (m_update_counter % refresh_rate) == 0)
    {
        GT_Debug_Memory_Address address;
        char reason[GT_DEBUG_MEMORY_REASON_SIZE];
        if (ParseAddressInput(address, reason, sizeof(reason)) &&
            address.address != m_selection_start)
        {
            SetSource(address);
            JumpToAddress(address.address, false);
        }
    }
    if (m_options.auto_refresh && (m_update_counter % refresh_rate) == 0)
        m_refresh_requested = true;
    if (m_refresh_requested)
        Refresh();
}

void MemEditor::Draw()
{
    if (!m_available)
        return;
    DrawToolbar();
    DrawGrid();
    DrawOptionsPopup();
}

void MemEditor::Refresh(bool preserve_previous)
{
    if (!IsValidPointer(m_provider) || m_data.size() != WINDOW_SIZE)
        return;

    if (preserve_previous && m_has_snapshot)
        m_previous = m_data;
    else
        memset(&m_previous[0], 0, m_previous.size());

    GT_Debug_Memory_Address address = m_source;
    address.address = m_window_base;
    m_provider->ReadBlock(address, &m_data[0], &m_status[0], WINDOW_SIZE,
        &m_block_info);
    m_has_snapshot = true;
    m_refresh_requested = false;
}

void MemEditor::RequestRefresh()
{
    m_refresh_requested = true;
}

void MemEditor::JumpToAddress(u32 address, bool add_history)
{
    if (!AddressInSource(address))
        return;
    if (add_history)
        PushHistory(address);
    SetWindowForAddress(address);
    m_selection_start = address;
    m_selection_end = address;
    m_editing_address = 0xFFFFFFFF;
    snprintf(m_address_input, sizeof(m_address_input), "%08X", address);
    m_refresh_requested = true;
}

void MemEditor::SetSource(const GT_Debug_Memory_Address& source)
{
    m_source = source;
    if (!AddressInSource(m_source.address))
        m_source.address = 0;
    m_window_base = 0;
    m_selection_start = m_source.address;
    m_selection_end = m_source.address;
    m_has_snapshot = false;
    m_history_count = 0;
    m_history_position = -1;
    snprintf(m_address_input, sizeof(m_address_input), "%08X", m_source.address);
    UpdateTitle();
    SetWindowForAddress(m_source.address);
    m_refresh_requested = true;
}

const GT_Debug_Memory_Address& MemEditor::GetSource() const
{
    return m_source;
}

u32 MemEditor::GetWindowBase() const
{
    return m_window_base;
}

u32 MemEditor::GetWindowSize() const
{
    return WINDOW_SIZE;
}

void MemEditor::GetSelection(u32& start, u32& end) const
{
    start = SelectionStart();
    end = SelectionEnd();
}

void MemEditor::SetSelection(u32 start, u32 end)
{
    if (!AddressInSource(start) || !AddressInSource(end))
        return;
    m_selection_start = start;
    m_selection_end = end;
    SetWindowForAddress(start);
    m_refresh_requested = true;
}

void MemEditor::CopySelection(bool decimal)
{
    std::vector<u8> data;
    std::vector<GT_Debug_Memory_Status> status;
    if (!ReadSelection(data, status))
        return;

    std::string text;
    char value[16];
    for (size_t i = 0; i < data.size(); i++)
    {
        if (i > 0)
            text += " ";
        if (status[i] != GT_DEBUG_MEMORY_VALID &&
            status[i] != GT_DEBUG_MEMORY_READ_ONLY)
            text += "??";
        else
        {
            snprintf(value, sizeof(value), decimal ? "%u" :
                (m_options.uppercase_hex ? "%02X" : "%02x"), data[i]);
            text += value;
        }
    }
    SDL_SetClipboardText(text.c_str());
}

void MemEditor::PasteSelection()
{
    char* clipboard = SDL_GetClipboardText();
    if (!IsValidPointer(clipboard))
        return;

    std::vector<u8> data;
    std::string compact;
    bool hexadecimal = true;
    for (const char* p = clipboard; *p != 0; p++)
    {
        if (is_hex_digit(*p))
            compact += *p;
        else if (!isspace((unsigned char)*p) && *p != ',' && *p != '-')
            hexadecimal = false;
    }

    if (hexadecimal && !compact.empty() && (compact.length() & 1) == 0)
    {
        for (size_t i = 0; i < compact.length(); i += 2)
        {
            u8 value = 0;
            if (!parse_hex_string(compact.c_str() + i, 2, &value))
            {
                data.clear();
                break;
            }
            data.push_back(value);
        }
    }
    else
    {
        size_t length = strlen(clipboard);
        data.assign((u8*)clipboard, (u8*)clipboard + length);
    }
    SDL_free(clipboard);

    u32 selection_size = SelectionSize();
    if (data.empty() || selection_size == 0)
        return;
    if (data.size() > selection_size)
        data.resize(selection_size);

    GT_Debug_Memory_Address address = m_source;
    address.address = SelectionStart();
    if (m_provider->QueueWrite(address, &data[0], (u32)data.size()))
        m_refresh_requested = true;
}

void MemEditor::FillSelection(u8 value)
{
    u32 size = SelectionSize();
    if (size == 0 || size > 0x100000)
        return;
    std::vector<u8> data(size, value);
    GT_Debug_Memory_Address address = m_source;
    address.address = SelectionStart();
    if (m_provider->QueueWrite(address, &data[0], size))
        m_refresh_requested = true;
}

const char* MemEditor::GetTitle() const
{
    return m_title;
}

bool MemEditor::IsAvailable() const
{
    return m_available;
}

void MemEditor::SetAvailable(bool available)
{
    m_available = available;
}

bool MemEditor::TakeBookmarkRequest(GT_Debug_Memory_Address& address, u32& end)
{
    if (!m_bookmark_request)
        return false;
    m_bookmark_request = false;
    address = m_source;
    address.address = SelectionStart();
    end = m_request_end;
    return true;
}

bool MemEditor::TakeWatchRequest(GT_Debug_Memory_Address& address)
{
    if (!m_watch_request)
        return false;
    m_watch_request = false;
    address = m_source;
    address.address = SelectionStart();
    return true;
}

bool MemEditor::TakeBreakpointRequest(GT_Debug_Memory_Address& address, u32& end)
{
    if (!m_breakpoint_request)
        return false;
    m_breakpoint_request = false;
    address = m_source;
    address.address = SelectionStart();
    end = m_request_end;
    return true;
}

DebugMemoryProvider* MemEditor::GetProvider() const
{
    return m_provider;
}

MemEditor::Options MemEditor::GetOptions() const
{
    return m_options;
}

void MemEditor::SetOptions(const Options& options)
{
    m_options = options;
    m_options.bytes_per_row = CLAMP(m_options.bytes_per_row, 8, 32);
    m_options.refresh_rate = CLAMP(m_options.refresh_rate, 1, 120);
    m_options.text_encoding = CLAMP(m_options.text_encoding, 0, 1);
    m_options.preview_endian = CLAMP(m_options.preview_endian, 0, 1);
}

void MemEditor::SaveSettings(std::ostream& stream) const
{
    stream.write((const char*)&m_source.space, sizeof(m_source.space));
    stream.write((const char*)&m_source.address, sizeof(m_source.address));
    stream.write((const char*)&m_source.segment, sizeof(m_source.segment));
    stream.write((const char*)&m_source.segment_register,
        sizeof(m_source.segment_register));
    stream.write((const char*)&m_source.region, sizeof(m_source.region));
    stream.write((const char*)&m_options, sizeof(m_options));
}

bool MemEditor::LoadSettings(std::istream& stream)
{
    GT_Debug_Memory_Address source;
    Options options;
    stream.read((char*)&source.space, sizeof(source.space));
    stream.read((char*)&source.address, sizeof(source.address));
    stream.read((char*)&source.segment, sizeof(source.segment));
    stream.read((char*)&source.segment_register, sizeof(source.segment_register));
    stream.read((char*)&source.region, sizeof(source.region));
    stream.read((char*)&options, sizeof(options));
    if (stream.fail() || source.space < 0 ||
        source.space >= GT_DEBUG_MEMORY_SPACE_COUNT)
        return false;
    SetOptions(options);
    SetSource(source);
    return true;
}

void MemEditor::DrawToolbar()
{
    ImGui::SetNextItemWidth(105.0f);
    if (ImGui::BeginCombo("##memory_source", m_title))
    {
        for (int i = 0; i < GT_DEBUG_MEMORY_SPACE_COUNT; i++)
        {
            if (i == GT_DEBUG_MEMORY_REGION)
                continue;
            GT_Debug_Memory_Space space = (GT_Debug_Memory_Space)i;
            bool selected = m_source.space == space;
            if (ImGui::Selectable(DebugMemoryProvider::GetSpaceName(space), selected))
            {
                GT_Debug_Memory_Address source = m_source;
                source.space = space;
                source.address = 0;
                source.region = 0;
                if (space == GT_DEBUG_MEMORY_LOGICAL)
                    source.segment_register = I386_SEGMENT_CS;
                SetSource(source);
            }
        }

        int region_count = m_provider->GetRegionCount();
        if (region_count > 0)
            ImGui::Separator();
        for (int i = 0; i < region_count; i++)
        {
            GT_Debug_Memory_Region region;
            if (!m_provider->GetRegion(i, region))
                continue;
            bool selected = m_source.space == GT_DEBUG_MEMORY_REGION &&
                m_source.region == region.id;
            if (ImGui::Selectable(region.name, selected))
            {
                GT_Debug_Memory_Address source = m_source;
                source.space = GT_DEBUG_MEMORY_REGION;
                source.address = 0;
                source.region = region.id;
                SetSource(source);
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::ArrowButton("##memory_back", ImGuiDir_Left))
        HistoryBack();
    ImGui::SameLine();
    if (ImGui::ArrowButton("##memory_forward", ImGuiDir_Right))
        HistoryForward();
    ImGui::SameLine();

    ImGui::SetNextItemWidth(135.0f);
    bool go = ImGui::InputText("##memory_address", m_address_input,
        sizeof(m_address_input), ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_AutoSelectAll);
    ImGui::SameLine();
    go = ImGui::Button("GoTo") || go;
    if (go)
    {
        GT_Debug_Memory_Address address;
        char reason[GT_DEBUG_MEMORY_REASON_SIZE];
        if (ParseAddressInput(address, reason, sizeof(reason)))
        {
            SetSource(address);
            JumpToAddress(address.address);
        }
    }

    ImGui::Checkbox("Follow", &m_follow_expression);
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        Refresh();
    ImGui::SameLine();
    if (ImGui::Button("Options"))
        ImGui::OpenPopup("memory_options");

    ImGui::TextColored(cyan, "VIEW:");
    ImGui::SameLine();
    ImGui::Text("%08X-%08X", m_window_base,
        m_window_base + WINDOW_SIZE - 1);
    ImGui::SameLine();
    ImGui::TextColored(cyan, "SELECTION:");
    ImGui::SameLine();
    ImGui::Text(SelectionStart() == SelectionEnd() ? "%08X" : "%08X-%08X",
        SelectionStart(), SelectionEnd());
}

void MemEditor::DrawGrid()
{
    int bytes_per_row = CLAMP(m_options.bytes_per_row, 8, 32);
    int row_count = (int)((WINDOW_SIZE + bytes_per_row - 1) / bytes_per_row);
    ImVec2 character_size = ImGui::CalcTextSize("0");
    float address_width = ImGui::CalcTextSize("FFFFFFFF").x + 12.0f;
    float cell_width = character_size.x * 2.0f + 6.0f;
    const char* text_header = m_options.text_encoding == 0 ? "ASCII" : "SHIFT-JIS";
    float text_width = MAX(character_size.x * bytes_per_row,
        ImGui::CalcTextSize(text_header).x) + 4.0f;
    ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_NoKeepColumnsVisible;
    float inner_width = address_width + bytes_per_row * cell_width +
        text_width + 8.0f;

    if (!ImGui::BeginChild("##memory_grid_container", ImVec2(0.0f, -1.0f),
        ImGuiChildFlags_None, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav))
    {
        ImGui::EndChild();
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(2.0f, 0.0f));
    if (!ImGui::BeginTable("##memory_grid", bytes_per_row + 2, flags,
        ImVec2(0, -1), inner_width))
    {
        ImGui::PopStyleVar();
        ImGui::EndChild();
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("ADDR##memory_address_column",
        ImGuiTableColumnFlags_WidthFixed, address_width);
    for (int i = 0; i < bytes_per_row; i++)
    {
        char column[48];
        snprintf(column, sizeof(column), "%02X##memory_column_%02X", i, i);
        ImGui::TableSetupColumn(column, ImGuiTableColumnFlags_WidthFixed,
            cell_width);
    }
    char text_column[48];
    snprintf(text_column, sizeof(text_column), "%s##memory_text_column",
        text_header);
    ImGui::TableSetupColumn(text_column, ImGuiTableColumnFlags_WidthFixed,
        text_width);
    ImGui::PushStyleColor(ImGuiCol_Text, yellow);
    ImGui::TableHeadersRow();
    ImGui::PopStyleColor();

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        m_drag_selecting = false;

    ImGuiListClipper clipper;
    clipper.Begin(row_count);
    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
        {
            u32 row_offset = (u32)row * bytes_per_row;
            u32 row_address = m_window_base + row_offset;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(cyan, "%08X", row_address);

            char text[65];
            int text_length = 0;
            for (int column = 0; column < bytes_per_row; column++)
            {
                ImGui::TableNextColumn();
                u32 offset = row_offset + column;
                if (offset >= WINDOW_SIZE)
                {
                    ImGui::TextUnformatted("");
                    text[text_length++] = '.';
                    continue;
                }
                u32 address = row_address + column;
                DrawCell(address, offset, column, bytes_per_row, cell_width);

                if (offset < m_data.size() &&
                    (m_status[offset] == GT_DEBUG_MEMORY_VALID ||
                    m_status[offset] == GT_DEBUG_MEMORY_READ_ONLY))
                {
                    u8 value = m_data[offset];
                    text[text_length++] = value >= 32 && value < 127 ?
                        (char)value : '.';
                }
                else
                    text[text_length++] = '.';
            }
            text[text_length] = 0;

            ImGui::TableNextColumn();
            ImVec2 text_position = ImGui::GetCursorScreenPos();
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            for (int column = 0; column < bytes_per_row; column++)
            {
                u32 offset = row_offset + column;
                u32 address = row_address + column;
                if (offset >= WINDOW_SIZE || address < SelectionStart() ||
                    address > SelectionEnd())
                    continue;
                ImVec2 minimum = text_position +
                    ImVec2(character_size.x * column, 0.0f);
                ImVec2 maximum = minimum +
                    ImVec2(character_size.x, character_size.y);
                draw_list->AddRectFilled(minimum, maximum,
                    ImGui::GetColorU32(dark_cyan));
            }
            if (m_options.text_encoding == 0)
                ImGui::TextColored(magenta, "%s", text);
            else
            {
                char raw[65];
                int raw_length = MIN(bytes_per_row, 64);
                for (int i = 0; i < raw_length; i++)
                {
                    u32 offset = row_offset + i;
                    raw[i] = offset < m_data.size() ? (char)m_data[offset] : 0;
                }
                raw[raw_length] = 0;
                char* converted = SDL_iconv_string("UTF-8", "SHIFT-JIS", raw,
                    (size_t)raw_length + 1);
                ImGui::TextColored(magenta, "%s",
                    IsValidPointer(converted) ? converted : text);
                SDL_free(converted);
            }
        }
    }

    if (m_drag_selecting && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        float line_height = ImGui::GetTextLineHeightWithSpacing();
        float mouse_y = ImGui::GetMousePos().y;
        float window_top = ImGui::GetWindowPos().y + line_height * 2.0f;
        float window_bottom = ImGui::GetWindowPos().y +
            ImGui::GetWindowHeight() - line_height;
        if (mouse_y < window_top)
            ImGui::SetScrollY(MAX(0.0f, ImGui::GetScrollY() - line_height));
        else if (mouse_y > window_bottom)
            ImGui::SetScrollY(ImGui::GetScrollY() + line_height);
    }
    ImGui::EndTable();
    ImGui::PopStyleVar();
    ImGui::EndChild();
}

void MemEditor::DrawCell(u32 address, u32 offset, int column,
    int bytes_per_row, float cell_width)
{
    if (offset >= m_data.size())
        return;

    GT_Debug_Memory_Status status = m_status[offset];
    bool changed = m_has_snapshot && m_data[offset] != m_previous[offset] &&
        (status == GT_DEBUG_MEMORY_VALID || status == GT_DEBUG_MEMORY_READ_ONLY);
    ImVec4 color = white;
    const char* display = "??";
    char value[4];

    if (status == GT_DEBUG_MEMORY_VALID || status == GT_DEBUG_MEMORY_READ_ONLY)
    {
        snprintf(value, sizeof(value), m_options.uppercase_hex ? "%02X" : "%02x",
            m_data[offset]);
        display = value;
        if (changed)
            color = orange;
        else if (m_options.gray_out_zeros && m_data[offset] == 0)
            color = mid_gray;
        else if (status == GT_DEBUG_MEMORY_READ_ONLY)
            color = gray;
    }
    else if (status == GT_DEBUG_MEMORY_UNMAPPED)
    {
        display = "--";
        color = mid_gray;
    }
    else
        color = red;

    ImVec2 cell_minimum = ImGui::GetCursorScreenPos() -
        ImVec2(ImGui::GetStyle().CellPadding.x, 0.0f);
    ImVec2 cell_maximum = cell_minimum +
        ImVec2(cell_width, ImGui::GetTextLineHeight());
    ImGuiHoveredFlags hover_flags = ImGuiHoveredFlags_ChildWindows |
        ImGuiHoveredFlags_AllowWhenBlockedByActiveItem;
    bool cell_hovered = ImGui::IsWindowHovered(hover_flags) &&
        ImGui::IsMouseHoveringRect(cell_minimum, cell_maximum, false);
    if (cell_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (!ImGui::GetIO().KeyShift)
            m_selection_start = address;
        m_selection_end = address;
        m_drag_selecting = true;
        if (m_editing_address != address)
            m_editing_address = 0xFFFFFFFF;
    }
    else if (cell_hovered && m_drag_selecting &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        m_selection_end = address;
    }

    bool selected = address >= SelectionStart() && address <= SelectionEnd();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (selected)
    {
        draw_list->AddRectFilled(cell_minimum, cell_maximum,
            ImGui::GetColorU32(dark_cyan));
    }

    ImGui::PushID("memory_cell");
    ImGui::PushID((int)offset);
    bool item_hovered = false;
    if (m_editing_address == address)
    {
        ImGui::SetNextItemWidth(cell_width - ImGui::GetStyle().CellPadding.x * 2.0f);
        if (m_edit_focus)
        {
            ImGui::SetKeyboardFocusHere();
            m_edit_focus = false;
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        bool commit = ImGui::InputText("##edit", m_edit_buffer,
            sizeof(m_edit_buffer), ImGuiInputTextFlags_CharsHexadecimal |
            ImGuiInputTextFlags_CharsUppercase |
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_AutoSelectAll);
        item_hovered = ImGui::IsItemHovered();
        ImGui::PopStyleVar();
        if (commit)
        {
            u8 data = 0;
            if (parse_hex_string(m_edit_buffer, strlen(m_edit_buffer), &data))
            {
                GT_Debug_Memory_Address write_address = m_source;
                write_address.address = address;
                m_provider->QueueWrite(write_address, &data, 1);
                if (address != 0xFFFFFFFF && AddressInSource(address + 1))
                {
                    m_editing_address = address + 1;
                    m_selection_start = m_selection_end = address + 1;
                    snprintf(m_edit_buffer, sizeof(m_edit_buffer), "%02X",
                        offset + 1 < m_data.size() ? m_data[offset + 1] : 0);
                    m_edit_focus = true;
                }
                else
                    m_editing_address = 0xFFFFFFFF;
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            m_editing_address = 0xFFFFFFFF;
    }
    else
    {
        ImVec2 text_position = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(cell_minimum);
        ImGui::InvisibleButton("##value",
            ImVec2(cell_width, ImGui::GetTextLineHeight()),
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        item_hovered = ImGui::IsItemHovered();
        draw_list->AddText(text_position, ImGui::GetColorU32(color), display);
        if (item_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
            status == GT_DEBUG_MEMORY_VALID)
        {
            m_editing_address = address;
            snprintf(m_edit_buffer, sizeof(m_edit_buffer), "%02X", m_data[offset]);
            m_edit_focus = true;
        }
        DrawContextMenu(address);
    }

    if (selected)
    {
        ImU32 frame_color = ImGui::GetColorU32(cyan);
        if (column == 0 || address == SelectionStart())
        {
            draw_list->AddLine(cell_minimum, ImVec2(cell_minimum.x,
                cell_maximum.y), frame_color);
        }
        if (column == bytes_per_row - 1 || address == SelectionEnd())
        {
            draw_list->AddLine(ImVec2(cell_maximum.x, cell_minimum.y),
                cell_maximum, frame_color);
        }
        if ((u64)address < (u64)SelectionStart() + bytes_per_row)
            draw_list->AddLine(cell_minimum,
                ImVec2(cell_maximum.x, cell_minimum.y), frame_color);
        if ((u64)address + bytes_per_row > SelectionEnd())
        {
            draw_list->AddLine(ImVec2(cell_minimum.x, cell_maximum.y),
                cell_maximum, frame_color);
        }
    }

    if (item_hovered)
    {
        ImGui::BeginTooltip();
        ImGui::TextColored(cyan, "%08X", address);
        ImGui::Text("%s", DebugMemoryProvider::GetStatusName(status));
        if (changed)
            ImGui::TextColored(orange, "Changed since previous refresh");
        ImGui::EndTooltip();
    }
    ImGui::PopID();
    ImGui::PopID();
}

void MemEditor::DrawContextMenu(u32 address)
{
    if (!ImGui::BeginPopupContextItem())
        return;

    if (address < SelectionStart() || address > SelectionEnd())
        m_selection_start = m_selection_end = address;

    if (ImGui::MenuItem("Copy", "Ctrl+C"))
        CopySelection();
    if (ImGui::MenuItem("Copy As Decimal"))
        CopySelection(true);
    if (ImGui::MenuItem("Paste", "Ctrl+V"))
        PasteSelection();
    ImGui::Separator();
    if (ImGui::MenuItem("Add Bookmark"))
    {
        m_bookmark_request = true;
        m_request_end = SelectionEnd();
    }
    if (ImGui::MenuItem("Add Watch"))
        m_watch_request = true;
    if (ImGui::MenuItem("Add Breakpoint"))
    {
        m_breakpoint_request = true;
        m_request_end = SelectionEnd();
    }
    ImGui::EndPopup();
}

void MemEditor::DrawOptionsPopup()
{
    if (!ImGui::BeginPopup("memory_options"))
        return;

    ImGui::Text("Columns:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderInt("##memory_columns", &m_options.bytes_per_row, 8, 32);
    ImGui::Checkbox("Uppercase hex", &m_options.uppercase_hex);
    ImGui::Checkbox("Gray out zeros", &m_options.gray_out_zeros);
    ImGui::Checkbox("Auto refresh", &m_options.auto_refresh);
    ImGui::Text("Refresh every:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::SliderInt("##memory_refresh_rate", &m_options.refresh_rate, 1, 120);
    ImGui::SameLine();
    ImGui::Text("updates");
    ImGui::Text("Text:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::Combo("##memory_encoding", &m_options.text_encoding,
        "ASCII\0Shift-JIS\0\0");
    ImGui::Text("Preview:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::Combo("##memory_endian", &m_options.preview_endian,
        "Little Endian\0Big Endian\0\0");
    ImGui::EndPopup();
}

void MemEditor::UpdateTitle()
{
    if (m_source.space == GT_DEBUG_MEMORY_REGION && IsValidPointer(m_provider))
    {
        GT_Debug_Memory_Region region;
        if (m_provider->GetRegionById(m_source.region, region))
        {
            snprintf(m_title, sizeof(m_title), "%s", region.name);
            return;
        }
    }
    if (m_source.space == GT_DEBUG_MEMORY_LOGICAL && m_source.segment_register >= 0)
    {
        static const char* segments[] = { "ES", "CS", "SS", "DS", "FS", "GS" };
        const char* segment = m_source.segment_register < I386_SEGMENT_COUNT ?
            segments[m_source.segment_register] : "?";
        snprintf(m_title, sizeof(m_title), "Logical %s", segment);
    }
    else
    {
        snprintf(m_title, sizeof(m_title), "%s",
            DebugMemoryProvider::GetSpaceName(m_source.space));
    }
}

void MemEditor::SetWindowForAddress(u32 address)
{
    u32 limit = m_provider->GetAddressLimit(m_source);
    u32 base = address & ~0xFFFU;
    if ((u64)base + WINDOW_SIZE - 1 > limit)
    {
        if ((u64)limit + 1 <= WINDOW_SIZE)
            base = 0;
        else
            base = (limit - WINDOW_SIZE + 1) & ~0xFFFU;
    }
    if (base != m_window_base)
    {
        m_window_base = base;
        m_has_snapshot = false;
    }
}

bool MemEditor::ParseAddressInput(GT_Debug_Memory_Address& address,
    char* reason, size_t reason_size) const
{
    address = m_source;
    std::string input(m_address_input);
    size_t colon = input.find(':');
    std::string expression = input;
    if (colon != std::string::npos)
    {
        std::string prefix = input.substr(0, colon);
        expression = input.substr(colon + 1);
        if (equal_prefix(prefix, "CS"))
        {
            address.space = GT_DEBUG_MEMORY_LOGICAL;
            address.segment_register = I386_SEGMENT_CS;
        }
        else if (equal_prefix(prefix, "SS"))
        {
            address.space = GT_DEBUG_MEMORY_LOGICAL;
            address.segment_register = I386_SEGMENT_SS;
        }
        else if (equal_prefix(prefix, "DS"))
        {
            address.space = GT_DEBUG_MEMORY_LOGICAL;
            address.segment_register = I386_SEGMENT_DS;
        }
        else if (equal_prefix(prefix, "ES"))
        {
            address.space = GT_DEBUG_MEMORY_LOGICAL;
            address.segment_register = I386_SEGMENT_ES;
        }
        else if (equal_prefix(prefix, "FS"))
        {
            address.space = GT_DEBUG_MEMORY_LOGICAL;
            address.segment_register = I386_SEGMENT_FS;
        }
        else if (equal_prefix(prefix, "GS"))
        {
            address.space = GT_DEBUG_MEMORY_LOGICAL;
            address.segment_register = I386_SEGMENT_GS;
        }
        else if (equal_prefix(prefix, "LINE") || equal_prefix(prefix, "L"))
            address.space = GT_DEBUG_MEMORY_LINEAR;
        else if (equal_prefix(prefix, "PHYS") || equal_prefix(prefix, "P"))
            address.space = GT_DEBUG_MEMORY_PHYSICAL;
        else if (equal_prefix(prefix, "BUS") || equal_prefix(prefix, "B"))
            address.space = GT_DEBUG_MEMORY_BUS;
        else if (equal_prefix(prefix, "IO"))
            address.space = GT_DEBUG_MEMORY_IO;
        else
        {
            u16 segment = 0;
            if (!parse_hex_with_prefix(prefix, &segment))
            {
                strncpy_fit(reason, "Unknown address-space or segment prefix",
                    reason_size);
                return false;
            }
            address.space = GT_DEBUG_MEMORY_LOGICAL;
            address.segment = segment;
            address.segment_register = -1;
        }
    }

    MemoryExpressionParser parser(expression.c_str(), m_provider);
    if (!parser.Parse(address.address))
    {
        strncpy_fit(reason, "Invalid expression or unavailable CPU register",
            reason_size);
        return false;
    }
    if (address.address > m_provider->GetAddressLimit(address))
    {
        strncpy_fit(reason, "Address is outside the selected source", reason_size);
        return false;
    }
    reason[0] = 0;
    return true;
}

bool MemEditor::ReadSelection(std::vector<u8>& data,
    std::vector<GT_Debug_Memory_Status>& status) const
{
    u32 size = SelectionSize();
    if (size == 0 || size > 0x100000 || !IsValidPointer(m_provider))
        return false;
    data.resize(size);
    status.resize(size);
    GT_Debug_Memory_Address address = m_source;
    address.address = SelectionStart();
    m_provider->ReadBlock(address, &data[0], &status[0], size, NULL);
    return true;
}

u32 MemEditor::SelectionStart() const
{
    return MIN(m_selection_start, m_selection_end);
}

u32 MemEditor::SelectionEnd() const
{
    return MAX(m_selection_start, m_selection_end);
}

u32 MemEditor::SelectionSize() const
{
    return SelectionEnd() - SelectionStart() + 1;
}

bool MemEditor::AddressInWindow(u32 address) const
{
    return address >= m_window_base &&
        (u64)address < (u64)m_window_base + WINDOW_SIZE;
}

bool MemEditor::AddressInSource(u32 address) const
{
    return IsValidPointer(m_provider) &&
        address <= m_provider->GetAddressLimit(m_source);
}

void MemEditor::PushHistory(u32 address)
{
    if (m_history_position >= 0 &&
        m_history[m_history_position] == address)
        return;
    if (m_history_position + 1 < m_history_count)
        m_history_count = m_history_position + 1;
    if (m_history_count >= HISTORY_SIZE)
    {
        memmove(m_history, m_history + 1,
            sizeof(m_history[0]) * (HISTORY_SIZE - 1));
        m_history_count--;
    }
    m_history[m_history_count++] = address;
    m_history_position = m_history_count - 1;
}

void MemEditor::HistoryBack()
{
    if (m_history_position > 0)
    {
        m_history_position--;
        JumpToAddress(m_history[m_history_position], false);
    }
}

void MemEditor::HistoryForward()
{
    if (m_history_position + 1 < m_history_count)
    {
        m_history_position++;
        JumpToAddress(m_history[m_history_position], false);
    }
}
