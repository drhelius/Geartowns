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

#ifndef MCP_DEBUG_ADAPTER_H
#define MCP_DEBUG_ADAPTER_H

#include <string>
#include <vector>
#include "json.hpp"
#include "geartowns.h"
#include "../debug/gui_debug_memory.h"

using json = nlohmann::json;

static const int k_mcp_mouse_motion_step = 4;

struct McpAddress
{
    u32 linear;
    bool logical;
    u16 segment;
    u32 offset;
    int segment_register;
};

class DebugAdapter
{
public:
    DebugAdapter(GeartownsCore* core);

    // Execution control
    void Pause();
    void Resume();
    void StepInto();
    void StepOver();
    void StepOut();
    void StepFrame();
    void Reset();
    json GetDebugStatus();
    json RunToAddress(const McpAddress& address);
    json SetFastForwardSpeed(int speed);
    json ToggleFastForward(bool enabled);

    // Addresses
    bool ParseAddress(const std::string& text, McpAddress& address, std::string& error);

    // Breakpoints
    json SetBreakpoint(u32 address, u32 end_address, bool range, u8 type, u8 space);
    json RemoveBreakpoint(u32 address, u32 end_address, bool range, u8 type, u8 space);
    json ListBreakpoints();
    json SetInterruptBreakpoint(int vector, const std::string& source);
    json ClearInterruptBreakpoint(int vector, const std::string& source);
    json ListInterruptBreakpoints();
    json GetBreakpointHit();

    // Memory areas
    json ListMemoryAreas();
    bool GetMemoryArea(int area, GuiDebugMemoryArea& info, std::string& error);
    json ReadMemory(int area, const std::string& offset, size_t size);
    json WriteMemory(int area, const std::string& offset, const std::vector<u8>& data);
    json TranslateAddress(const std::string& address);
    json SelectMemoryRange(int area, u32 start_address, u32 end_address);
    json SetMemorySelectionValue(int area, u8 value);
    json GetMemorySelection(int area);
    json AddMemoryBookmark(int area, u32 address, const std::string& name);
    json RemoveMemoryBookmark(int area, u32 address);
    json ListMemoryBookmarks(int area);
    json AddMemoryWatch(int area, u32 address, const std::string& notes, int size);
    json RemoveMemoryWatch(int area, u32 address);
    json ListMemoryWatches(int area);
    json MemorySearchCapture(int area, const json& arguments);
    json MemorySearch(int area, const std::string& op, const std::string& compare_type, u64 compare_value,
        const std::string& data_type);
    json MemoryFind(int area, const std::string& value, bool text, bool case_sensitive, const json& arguments);

    // CPU
    json GetI386Status();
    json GetI386Descriptors(const std::string& table, int start, int count);
    json GetPageDirectory(int index);
    json WriteI386Register(const std::string& name, u32 value);

    // Disassembly and symbols
    json GetDisassembly(u32 start_address, u32 end_address, int count, int code_size, bool resolve_symbols,
        bool detailed);
    json ListCallStack();
    json AddDisassemblerBookmark(u32 address, const std::string& name);
    json RemoveDisassemblerBookmark(u32 address);
    json ListDisassemblerBookmarks();
    json ListSymbols(const std::string& filter);
    json AddSymbol(u32 address, const std::string& name);
    json RemoveSymbol(u32 address);
    json LoadSymbols(const std::string& file_path);
    json LookupSymbolByName(const std::string& name);
    json LookupSymbolAtAddress(u32 address);

    // System hardware
    json GetPICStatus();
    json GetPITStatus();
    json GetDMAStatus();
    json GetRTCStatus();
    json GetSystemStatus();
    json GetKeyboardStatus();

    // Video hardware
    json GetCRTCStatus();
    json GetCRTCRegisters();
    json WriteCRTCRegister(int reg, u16 value);
    json GetVideoOutputStatus();
    json GetPalettes(const std::string& palette);
    json GetFrameBuffer(const std::string& buffer, const json& arguments);
    json ListSprites(int start, int count, const std::string& filter);
    json GetSprite(int index, const std::string& format);

    // Audio hardware
    json GetYM3438Status(int channel);
    json GetYM3438Registers(int part);
    json GetRF5C68Status();
    json GetSoundStatus();
    json SetAudioMute(const std::string& source, int channel, bool mute);

    // Storage
    json GetCDROMStatus();
    json ListCDROMTracks();
    json GetCDROMAudioStatus();
    json ReadCDROMSector(u32 lba, const std::string& mode);
    json GetFDCStatus();
    json ListFloppyDrives();
    json ListFloppySectors(int drive, int cylinder, int head);
    json ReadFloppySector(int drive, int cylinder, int head, int sector);
    json InsertFloppy(int drive, const std::string& file_path, const json& write_protected);
    json EjectFloppy(int drive);
    json SwapFloppies();
    json SetFloppyWriteProtect(int drive, bool write_protected);

    // Trace and profiler
    json GetTraceLog(s64 start, int count);
    json SetTraceLog(const json& arguments);
    json SetProfiler(const std::string& action);
    json GetProfilerData(const std::string& sort, int count, const std::string& filter);

    // Capture
    json GetScreenshot();
    json StartVideoRecording(const std::string& file_path, int scale, const std::string& aspect_ratio,
        const std::string& quality);
    json StopVideoRecording();

    // Media and state management
    json GetMediaInfo();
    json ListRecentMedia();
    json LoadBios(const std::string& directory_path);
    json StartLoadMedia(const std::string& file_path);
    bool IsMediaLoading() const;
    json FinishLoadMedia(const std::string& file_path);
    json ListSaveStateSlots();
    json SelectSaveStateSlot(int slot);
    json SaveState();
    json LoadState();
    json SaveStateFile(const std::string& file_path);
    json LoadStateFile(const std::string& file_path);

    // Input
    json ControllerButton(int player, const std::string& button, const std::string& action);
    json ControllerSetType(int player, const std::string& type);
    json ControllerGetType(int player);
    bool IsMouseController(int player) const;
    bool ApplyMouseMotion(int player, int delta_x, int delta_y);
    json KeyboardKey(const std::string& key, const std::string& action);
    bool GetKeyCode(const std::string& name, GT_Keys& key) const;
    bool GetTypedKey(char character, GT_Keys& key, bool& shift) const;
    json GetInputState();
    void ClearControllerState();

    // Rewind
    json GetRewindStatus();
    json RewindSeek(int snapshot);

private:
    u16 ButtonMask(const std::string& button) const;
    void ApplyControllerState(int player);
    std::string Hex(u32 value, int digits) const;
    std::string Hex32(u32 value) const;
    std::string FormatAreaOffset(const GuiDebugMemoryArea& area, u32 offset) const;
    bool ParseAreaOffset(const GuiDebugMemoryArea& area, const std::string& text, u32& offset, std::string& error);
    bool GetAreaRange(const GuiDebugMemoryArea& area, const json& arguments, u32& start, u32& size,
        std::string& error);
    const char* GetSymbolAt(u32 linear);
    std::string FormatLogical(u16 selector, u32 offset, u32 linear);

private:
    GeartownsCore* m_core;
    u16 m_buttons[GT_MAX_GAMEPADS];
};

#endif /* MCP_DEBUG_ADAPTER_H */
