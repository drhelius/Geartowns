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

struct McpTraceFilter
{
    const char* name;
    GT_Trace_Type type;
    u32 mask;
};

// Filter names for set_trace_log, cpu.instructions has no event mask
static const McpTraceFilter k_mcp_trace_filters[] =
{
    { "cpu.instructions", TRACE_CPU, 0 },
    { "cpu.irqs", TRACE_CPU_INTERRUPT, TRACE_CPU_INTERRUPT_EVENT_IRQS },
    { "cpu.exceptions", TRACE_CPU_INTERRUPT, TRACE_CPU_INTERRUPT_EVENT_EXCEPTIONS },
    { "cpu.software_ints", TRACE_CPU_INTERRUPT, TRACE_CPU_INTERRUPT_EVENT_SOFTWARE },
    { "io.reads", TRACE_IO, TRACE_IO_EVENT_READS },
    { "io.writes", TRACE_IO, TRACE_IO_EVENT_WRITES },
    { "pic.requests", TRACE_PIC, TRACE_PIC_EVENT_REQUESTS },
    { "pic.mask", TRACE_PIC, TRACE_PIC_EVENT_MASK },
    { "pic.commands", TRACE_PIC, TRACE_PIC_EVENT_COMMANDS },
    { "pic.init", TRACE_PIC, TRACE_PIC_EVENT_INIT },
    { "timer.timeouts", TRACE_TIMER, TRACE_TIMER_EVENT_TIMEOUTS },
    { "timer.counters", TRACE_TIMER, TRACE_TIMER_EVENT_COUNTERS },
    { "timer.interrupt_control", TRACE_TIMER, TRACE_TIMER_EVENT_INTERRUPT },
    { "dma.registers", TRACE_DMA, TRACE_DMA_EVENT_REGISTERS },
    { "dma.requests", TRACE_DMA, TRACE_DMA_EVENT_REQUESTS },
    { "dma.ends", TRACE_DMA, TRACE_DMA_EVENT_ENDS },
    { "video.crtc", TRACE_VIDEO, TRACE_VIDEO_EVENT_CRTC },
    { "video.output", TRACE_VIDEO, TRACE_VIDEO_EVENT_OUTPUT },
    { "video.palette", TRACE_VIDEO, TRACE_VIDEO_EVENT_PALETTE },
    { "video.vram_mask", TRACE_VIDEO, TRACE_VIDEO_EVENT_MASK },
    { "video.vsync", TRACE_VIDEO, TRACE_VIDEO_EVENT_VSYNC },
    { "video.fmr", TRACE_VIDEO, TRACE_VIDEO_EVENT_FMR },
    { "video.missed_vblank", TRACE_VIDEO, TRACE_VIDEO_EVENT_MISSED_VBLANK },
    { "sprite.registers", TRACE_SPRITE, TRACE_SPRITE_EVENT_REGISTERS },
    { "sprite.transfers", TRACE_SPRITE, TRACE_SPRITE_EVENT_TRANSFERS },
    { "sprite.busy", TRACE_SPRITE, TRACE_SPRITE_EVENT_BUSY },
    { "fm.key", TRACE_FM, TRACE_FM_EVENT_KEY },
    { "fm.frequency", TRACE_FM, TRACE_FM_EVENT_FREQUENCY },
    { "fm.operators", TRACE_FM, TRACE_FM_EVENT_OPERATORS },
    { "fm.channels", TRACE_FM, TRACE_FM_EVENT_CHANNELS },
    { "fm.global", TRACE_FM, TRACE_FM_EVENT_GLOBAL },
    { "fm.dac", TRACE_FM, TRACE_FM_EVENT_DAC },
    { "fm.timers", TRACE_FM, TRACE_FM_EVENT_TIMERS },
    { "fm.irqs", TRACE_FM, TRACE_FM_EVENT_IRQS },
    { "pcm.channels", TRACE_PCM, TRACE_PCM_EVENT_CHANNELS },
    { "pcm.key", TRACE_PCM, TRACE_PCM_EVENT_KEY },
    { "pcm.control", TRACE_PCM, TRACE_PCM_EVENT_CONTROL },
    { "pcm.irqs", TRACE_PCM, TRACE_PCM_EVENT_IRQS },
    { "mixer.volume", TRACE_MIXER, TRACE_MIXER_EVENT_VOLUME },
    { "mixer.mute", TRACE_MIXER, TRACE_MIXER_EVENT_MUTE },
    { "cdrom.commands", TRACE_CDROM, TRACE_CDROM_EVENT_COMMANDS },
    { "cdrom.status", TRACE_CDROM, TRACE_CDROM_EVENT_STATUS },
    { "cdrom.irqs", TRACE_CDROM, TRACE_CDROM_EVENT_IRQS },
    { "cdrom.control", TRACE_CDROM, TRACE_CDROM_EVENT_CONTROL },
    { "cdrom.data", TRACE_CDROM, TRACE_CDROM_EVENT_DATA },
    { "cdrom.cdda", TRACE_CDROM, TRACE_CDROM_EVENT_CDDA },
    { "fdc.commands", TRACE_FDC, TRACE_FDC_EVENT_COMMANDS },
    { "fdc.results", TRACE_FDC, TRACE_FDC_EVENT_RESULTS },
    { "fdc.drives", TRACE_FDC, TRACE_FDC_EVENT_DRIVES },
    { "keyboard.keys", TRACE_KEYBOARD, TRACE_KEYBOARD_EVENT_KEYS },
    { "keyboard.reads", TRACE_KEYBOARD, TRACE_KEYBOARD_EVENT_READS },
    { "keyboard.commands", TRACE_KEYBOARD, TRACE_KEYBOARD_EVENT_COMMANDS },
    { "input.reads", TRACE_INPUT, TRACE_INPUT_EVENT_READS },
    { "input.writes", TRACE_INPUT, TRACE_INPUT_EVENT_WRITES },
    { "input.changes", TRACE_INPUT, TRACE_INPUT_EVENT_CHANGES },
    { "system.reset", TRACE_SYSTEM, TRACE_SYSTEM_EVENT_RESET },
    { "system.memory_map", TRACE_SYSTEM, TRACE_SYSTEM_EVENT_MEMORY },
    { "system.rtc", TRACE_SYSTEM, TRACE_SYSTEM_EVENT_RTC }
};

static const int k_mcp_trace_filter_count = sizeof(k_mcp_trace_filters) / sizeof(k_mcp_trace_filters[0]);

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
    bool StepOut();
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
    json EnableBreakpoint(u32 address, u32 end_address, bool range, u8 type, u8 space, bool enabled);
    json EnableInterruptBreakpoint(int vector, const std::string& source, bool enabled);
    json EnableIRQBreakpoint(int irq, bool enabled);
    json SetBreakpointsActive(bool active);
    json SetInterruptBreakpoint(int vector, const std::string& source);
    json ClearInterruptBreakpoint(int vector, const std::string& source);
    json ListInterruptBreakpoints();
    json SetIRQBreakpoint(int irq);
    json ClearIRQBreakpoint(int irq);
    json ListIRQBreakpoints();
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
    json ListSymbols(const std::string& filter, int start, int count);
    json AddSymbol(u32 address, const std::string& name);
    json RemoveSymbol(u32 address);
    json LoadSymbols(const std::string& file_path);
    json LookupSymbolByName(const std::string& name, bool partial);
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
    json SetTraceLog(bool enabled, u32 flags, const std::string& output, const std::string& memory_size,
        const std::string& disk_size, const std::string& output_path, const u32* event_filters,
        const std::string& vblank_watch_address, const std::string& vblank_watch_operation);
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
