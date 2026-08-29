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

#ifndef GUI_DEBUG_MEMEDITOR_H
#define GUI_DEBUG_MEMEDITOR_H

#include <iosfwd>
#include <vector>
#include "debug_memory.h"

class DebugMemoryProvider;

class MemEditor
{
public:
    struct Options
    {
        int bytes_per_row;
        bool uppercase_hex;
        bool gray_out_zeros;
        bool auto_refresh;
        int refresh_rate;
        int text_encoding;
        int preview_endian;
    };

public:
    MemEditor();
    ~MemEditor();
    void Init(DebugMemoryProvider* provider, int id);
    void Reset();
    void Update();
    void Draw();
    void Refresh(bool preserve_previous = true);
    void RequestRefresh();
    void JumpToAddress(u32 address, bool add_history = true);
    void SetSource(const GT_Debug_Memory_Address& source);
    const GT_Debug_Memory_Address& GetSource() const;
    u32 GetWindowBase() const;
    u32 GetWindowSize() const;
    void GetSelection(u32& start, u32& end) const;
    void SetSelection(u32 start, u32 end);
    void CopySelection(bool decimal = false);
    void PasteSelection();
    void FillSelection(u8 value);
    const char* GetTitle() const;
    bool IsAvailable() const;
    void SetAvailable(bool available);
    bool TakeBookmarkRequest(GT_Debug_Memory_Address& address, u32& end);
    bool TakeWatchRequest(GT_Debug_Memory_Address& address);
    bool TakeBreakpointRequest(GT_Debug_Memory_Address& address, u32& end);
    DebugMemoryProvider* GetProvider() const;
    Options GetOptions() const;
    void SetOptions(const Options& options);
    void SaveSettings(std::ostream& stream) const;
    bool LoadSettings(std::istream& stream);

private:
    void DrawToolbar();
    void DrawGrid();
    void DrawCell(u32 address, u32 offset, int column,
        int bytes_per_row, float cell_width);
    void DrawContextMenu(u32 address);
    void DrawOptionsPopup();
    void UpdateTitle();
    void SetWindowForAddress(u32 address);
    bool ParseAddressInput(GT_Debug_Memory_Address& address,
        char* reason, size_t reason_size) const;
    bool ReadSelection(std::vector<u8>& data,
        std::vector<GT_Debug_Memory_Status>& status) const;
    u32 SelectionStart() const;
    u32 SelectionEnd() const;
    u32 SelectionSize() const;
    bool AddressInWindow(u32 address) const;
    bool AddressInSource(u32 address) const;
    void PushHistory(u32 address);
    void HistoryBack();
    void HistoryForward();

private:
    static const u32 WINDOW_SIZE = 0x4000;
    static const int HISTORY_SIZE = 32;

    DebugMemoryProvider* m_provider;
    int m_id;
    char m_title[96];
    GT_Debug_Memory_Address m_source;
    u32 m_window_base;
    u32 m_selection_start;
    u32 m_selection_end;
    u32 m_editing_address;
    u32 m_history[HISTORY_SIZE];
    int m_history_count;
    int m_history_position;
    int m_update_counter;
    char m_address_input[96];
    char m_edit_buffer[3];
    Options m_options;
    GT_Debug_Memory_Block_Info m_block_info;
    std::vector<u8> m_data;
    std::vector<u8> m_previous;
    std::vector<GT_Debug_Memory_Status> m_status;
    bool m_available;
    bool m_has_snapshot;
    bool m_refresh_requested;
    bool m_edit_focus;
    bool m_drag_selecting;
    bool m_follow_expression;
    bool m_bookmark_request;
    bool m_watch_request;
    bool m_breakpoint_request;
    u32 m_request_end;
};

#endif /* GUI_DEBUG_MEMEDITOR_H */
