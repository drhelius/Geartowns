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
#include <ctype.h>
#include <functional>
#include <math.h>
#include <iomanip>
#include <sstream>
#include <stdlib.h>
#include "mcp_debug_adapter.h"
#include "common/log.h"
#include "drive/floppy_disk.h"
#include "input/keyboard.h"
#include "system/msm58321.h"
#include "system/pic.h"
#include "system/pit.h"
#include "system/scheduler.h"
#include "system/system_control.h"
#include "system/upd71071.h"
#include "system/io.h"
#include "audio/audio.h"
#include "cdrom/cdrom.h"
#include "cdrom/cdrom_audio.h"
#include "cdrom/cdrom_media.h"
#include "drive/fdc.h"
#include "drive/floppy_image.h"
#include "drive/mb8877.h"
#include "video/sprite.h"
#include "video/video.h"
#include "../config.h"
#include "../emu.h"
#include "../events.h"
#include "../gui.h"
#include "../gui_actions.h"
#include "../rewind.h"
#include "../debug/gui_debug_cdrom.h"
#include "../debug/gui_debug_constants.h"
#include "../debug/gui_debug_floppy.h"
#include "../debug/gui_debug_disassembler.h"
#include "../debug/gui_debug_i386_tables.h"
#include "../debug/gui_debug_profiler.h"
#include "../debug/gui_debug_rewind.h"
#include "../debug/gui_debug_trace_logger.h"
#include "../debug/trace_logger_formatter.h"
#include "../emu_floppy.h"
#include "../utils.h"
#include "../video_recorder.h"

struct McpFlagName
{
    const char* name;
    u32 mask;
};

static const McpFlagName k_mcp_eflags[] =
{
    { "cf", I386_FLAG_CF }, { "pf", I386_FLAG_PF }, { "af", I386_FLAG_AF }, { "zf", I386_FLAG_ZF },
    { "sf", I386_FLAG_SF }, { "tf", I386_FLAG_TF }, { "if", I386_FLAG_IF }, { "df", I386_FLAG_DF },
    { "of", I386_FLAG_OF }, { "nt", I386_FLAG_NT }, { "rf", I386_FLAG_RF }, { "vm", I386_FLAG_VM }
};

static const McpFlagName k_mcp_cr0_flags[] =
{
    { "pe", 0x00000001 }, { "mp", 0x00000002 }, { "em", 0x00000004 }, { "ts", 0x00000008 }, { "et", 0x00000010 },
    { "pg", 0x80000000 }
};

static const char* k_mcp_segment_names[I386_SEGMENT_COUNT] = { "es", "cs", "ss", "ds", "fs", "gs" };
static const int k_mcp_segment_order[I386_SEGMENT_COUNT] =
{
    I386_SEGMENT_CS, I386_SEGMENT_DS, I386_SEGMENT_ES, I386_SEGMENT_SS, I386_SEGMENT_FS, I386_SEGMENT_GS
};

static const char* k_mcp_area_flags[] =
{
    "readable", "writable", "executable", "rom", "mapped", "mmio", "video", "audio", "overlay"
};

static const int k_mcp_max_read_size = 0x10000;
static const int k_mcp_max_disassembly_lines = 1000;

static bool parse_hex_text(const std::string& text, u32& value)
{
    std::string trimmed = text;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    return parse_hex_with_prefix(trimmed, &value);
}

static std::string to_lower(const std::string& text)
{
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

static std::string to_upper(const std::string& text)
{
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

static std::string trim_text(const char* text)
{
    std::string result = text;
    size_t end = result.find_last_not_of(' ');
    return end == std::string::npos ? std::string() : result.substr(0, end + 1);
}

static std::string hex_text(u32 value, int digits)
{
    char text[16];
    snprintf(text, sizeof(text), "%0*X", digits, value);
    return text;
}

static const char* execution_mode_name(I386_Execution_Mode mode)
{
    switch (mode)
    {
        case I386_MODE_PROTECTED: return "protected";
        case I386_MODE_VM86: return "vm86";
        default: return "real";
    }
}

static bool is_prefix_word(const std::string& word)
{
    return word == "LOCK" || word == "REP" || word == "REPE" || word == "REPNE" || word == "ADDR16" ||
        word == "ADDR32" || word == "ES" || word == "CS" || word == "SS" || word == "DS" || word == "FS" ||
        word == "GS";
}

static void split_instruction(const char* text, std::string& mnemonic, std::string& operands)
{
    std::istringstream stream(text);
    std::string word;
    mnemonic.clear();
    operands.clear();

    while (stream >> word)
    {
        if (!mnemonic.empty())
            mnemonic += " ";

        mnemonic += word;

        if (!is_prefix_word(word))
            break;
    }

    std::getline(stream, operands);
    operands.erase(0, operands.find_first_not_of(" "));
}

static int get_opcode_index(const I386_Disassembler_Record* record)
{
    int index = 0;

    while (index < record->size)
    {
        u8 byte = record->opcodes[index];

        if (byte != 0x26 && byte != 0x2E && byte != 0x36 && byte != 0x3E && byte != 0x64 && byte != 0x65 &&
            byte != 0x66 && byte != 0x67 && byte != 0xF0 && byte != 0xF2 && byte != 0xF3)
            break;

        index++;
    }

    return index;
}

static bool load_segment(I386_State& state, int index, u32 value, std::string& error)
{
    I386_Segment& segment = state.segments[index];

    if (value > 0xFFFF)
    {
        error = "Segment selectors are 16-bit";
        return false;
    }

    if (state.execution_mode != I386_MODE_PROTECTED)
    {
        segment.selector = (u16)value;
        segment.base = value << 4;
        segment.limit = 0xFFFF;
        return true;
    }

    if ((value & 0xFFFC) == 0)
    {
        if (index == I386_SEGMENT_CS || index == I386_SEGMENT_SS)
        {
            error = "CS and SS cannot hold a null selector";
            return false;
        }

        memset(&segment, 0, sizeof(segment));
        segment.selector = (u16)value;
        return true;
    }

    GuiDebugDescriptor descriptor;

    if (!gui_debug_i386_read_descriptor((u16)value, descriptor) || descriptor.system || !descriptor.present)
    {
        char text[80];
        snprintf(text, sizeof(text), "Selector %04X is not a present code or data descriptor", value);
        error = text;
        return false;
    }

    segment.selector = (u16)value;
    segment.base = descriptor.base;
    segment.limit = descriptor.limit;
    segment.attributes = gui_debug_i386_segment_attributes(descriptor);
    segment.dpl = descriptor.dpl;
    return true;
}

DebugAdapter::DebugAdapter(GeartownsCore* core)
{
    m_core = core;
    memset(m_buttons, 0, sizeof(m_buttons));
}

void DebugAdapter::Pause()
{
    config_debug.debug = true;
    emu_debug_break();
}

void DebugAdapter::Resume()
{
    config_debug.debug = true;
    emu_debug_continue();
}

void DebugAdapter::StepInto()
{
    config_debug.debug = true;
    emu_debug_step_into();
}

void DebugAdapter::StepOver()
{
    config_debug.debug = true;
    emu_debug_step_over();
}

void DebugAdapter::StepOut()
{
    config_debug.debug = true;
    emu_debug_step_out();
}

void DebugAdapter::StepFrame()
{
    config_debug.debug = true;
    emu_debug_step_frame();
}

void DebugAdapter::Reset()
{
    emu_reset();
    ClearControllerState();
}

json DebugAdapter::GetDebugStatus()
{
    json result;

    if (!m_core)
    {
        result["error"] = "Core not initialized";
        return result;
    }

    I386* cpu = m_core->GetI386();
    I386_State* state = cpu->GetState();
    const I386_Segment& cs = state->segments[I386_SEGMENT_CS];
    u32 breakpoint_address = 0;
    bool at_breakpoint = cpu->GetBreakpointHitAddress(breakpoint_address);
    bool code32 = (cs.attributes & I386_SEGMENT_DEFAULT_32) != 0;

    result["paused"] = emu_is_debug_idle() || emu_is_paused();
    result["at_breakpoint"] = at_breakpoint;
    result["pc"] = Hex32(cpu->GetCurrentLinearPC());
    result["logical_pc"] = Hex(cs.selector, 4) + ":" + Hex(state->eip, code32 ? 8 : 4);
    result["mode"] = execution_mode_name(state->execution_mode);
    result["halted"] = state->halted;

    if (at_breakpoint)
        result["breakpoint"] = GetBreakpointHit();
    else if (cpu->RunToBreakpointHit())
        result["breakpoint"] = {{"kind", "run_to"}, {"space", "linear"}, {"address", Hex32(cpu->GetCurrentLinearPC())}};

    result["media_loading"] = emu_is_media_loading();
    result["media_ready"] = m_core->GetMedia()->IsReady();
    result["powered_on"] = m_core->IsPoweredOn();
    result["frame"] = emu_frame_counter;
    return result;
}

json DebugAdapter::RunToAddress(const McpAddress& address)
{
    json result;

    if (emu_is_empty())
    {
        result["error"] = "Emulator is powered off";
        return result;
    }

    config_debug.debug = true;
    gui_debug_runto_address(address.linear);

    result["success"] = true;
    result["address"] = Hex32(address.linear);
    return result;
}

json DebugAdapter::SetFastForwardSpeed(int speed)
{
    json result;

    if (speed < 0 || speed > 4)
    {
        result["error"] = "Invalid speed (must be 0-4: 0=1.5x, 1=2x, 2=2.5x, 3=3x, 4=Unlimited)";
        Log("[MCP] SetFastForwardSpeed failed: Invalid speed %d", speed);
        return result;
    }

    config_emulator.ffwd_speed = speed;

    result["success"] = true;
    result["speed"] = speed;

    const char* speed_names[] = {"1.5x", "2x", "2.5x", "3x", "Unlimited"};
    result["speed_name"] = speed_names[speed];

    return result;
}

json DebugAdapter::ToggleFastForward(bool enabled)
{
    json result;

    config_emulator.ffwd = enabled;
    gui_action_ffwd();

    result["success"] = true;
    result["enabled"] = enabled;
    result["speed"] = config_emulator.ffwd_speed;

    return result;
}

bool DebugAdapter::ParseAddress(const std::string& text, McpAddress& address, std::string& error)
{
    memset(&address, 0, sizeof(address));
    address.segment_register = -1;

    std::string input = text;
    input.erase(0, input.find_first_not_of(" \t"));
    input.erase(input.find_last_not_of(" \t") + 1);
    size_t colon = input.find(':');

    if (colon == std::string::npos)
    {
        if (!parse_hex_text(input, address.linear))
        {
            error = "Invalid address '" + text + "'. Use a linear hex address, SR:offset or SSSS:offset";
            return false;
        }

        return true;
    }

    std::string segment = to_upper(input.substr(0, colon));
    u32 offset = 0;

    if (!parse_hex_text(input.substr(colon + 1), offset))
    {
        error = "Invalid offset in '" + text + "'";
        return false;
    }

    I386_State* state = m_core->GetI386()->GetState();
    address.logical = true;
    address.offset = offset;

    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
    {
        if (segment != to_upper(k_mcp_segment_names[i]))
            continue;

        address.segment_register = i;
        address.segment = state->segments[i].selector;
        address.linear = state->segments[i].base + offset;
        return true;
    }

    u32 selector = 0;

    if (!parse_hex_text(segment, selector) || selector > 0xFFFF)
    {
        error = "Invalid segment in '" + text + "'";
        return false;
    }

    u32 base = 0;
    u32 limit = 0;
    char reason[GT_DEBUG_MEMORY_REASON_SIZE];

    if (!gui_debug_i386_selector_base((u16)selector, base, limit, reason, sizeof(reason)))
    {
        error = reason;
        return false;
    }

    UNUSED(limit);
    address.segment = (u16)selector;
    address.linear = base + offset;
    return true;
}

static const char* const k_mcp_breakpoint_spaces[I386_BREAKPOINT_SPACE_COUNT] = { "linear", "physical", "io" };
static const char* const k_mcp_interrupt_sources[I386_INTERRUPT_SOURCE_COUNT] =
{
    "any", "exception", "hardware", "software"
};

static const char* breakpoint_type_name(u8 type)
{
    if (type == I386_BREAKPOINT_EXECUTE)
        return "execute";

    if (type == (I386_BREAKPOINT_READ | I386_BREAKPOINT_WRITE))
        return "access";

    return type == I386_BREAKPOINT_WRITE ? "write" : "read";
}

static std::string breakpoint_address_text(u32 address, u8 space)
{
    return space == I386_BREAKPOINT_IO ? hex_text(address, 4) : hex_text(address, 8);
}

json DebugAdapter::SetBreakpoint(u32 address, u32 end_address, bool range, u8 type, u8 space)
{
    if (!m_core->GetI386()->AddBreakpoint(address, range ? end_address : address, type, space))
        return {{"error", "Invalid breakpoint: execute breakpoints are linear only and I/O ports end at FFFF"}};

    json result = {
        {"success", true},
        {"type", breakpoint_type_name(type)},
        {"space", k_mcp_breakpoint_spaces[space]},
        {"address", breakpoint_address_text(address, space)}
    };

    if (range)
        result["end_address"] = breakpoint_address_text(end_address, space);

    return result;
}

json DebugAdapter::RemoveBreakpoint(u32 address, u32 end_address, bool range, u8 type, u8 space)
{
    bool removed = m_core->GetI386()->RemoveBreakpoint(address, range ? end_address : address, type, space);
    json result = {
        {"success", true},
        {"removed", removed},
        {"type", breakpoint_type_name(type)},
        {"space", k_mcp_breakpoint_spaces[space]},
        {"address", breakpoint_address_text(address, space)}
    };

    if (range)
        result["end_address"] = breakpoint_address_text(end_address, space);

    return result;
}

json DebugAdapter::ListBreakpoints()
{
    json breakpoints = json::array();
    std::vector<I386_Breakpoint>* items = m_core->GetI386()->GetBreakpoints();

    for (size_t i = 0; i < items->size(); i++)
    {
        const I386_Breakpoint& item = (*items)[i];
        u8 space = item.space % I386_BREAKPOINT_SPACE_COUNT;
        json breakpoint = {
            {"enabled", item.enabled},
            {"type", breakpoint_type_name(item.type)},
            {"space", k_mcp_breakpoint_spaces[space]},
            {"address", breakpoint_address_text(item.address1, space)},
            {"range", item.range}
        };

        if (item.range)
            breakpoint["end_address"] = breakpoint_address_text(item.address2, space);
        else if (space == I386_BREAKPOINT_IO && IsValidPointer(gui_debug_port_label((u16)item.address1)))
            breakpoint["port_name"] = gui_debug_port_label((u16)item.address1);
        else if (space == I386_BREAKPOINT_LINEAR && IsValidPointer(GetSymbolAt(item.address1)))
            breakpoint["symbol"] = GetSymbolAt(item.address1);

        breakpoints.push_back(breakpoint);
    }

    return {{"breakpoints", breakpoints}, {"count", breakpoints.size()}};
}

static int parse_interrupt_source(const std::string& source)
{
    for (int i = 0; i < I386_INTERRUPT_SOURCE_COUNT; i++)
    {
        if (source == k_mcp_interrupt_sources[i])
            return i;
    }

    return -1;
}

json DebugAdapter::SetInterruptBreakpoint(int vector, const std::string& source)
{
    int index = parse_interrupt_source(source);

    if (vector < 0 || vector > 0xFF || index < 0)
        return {{"error", "vector must be 0-255 and source any, exception, hardware or software"}};

    m_core->GetI386()->AddInterruptBreakpoint((u8)vector, (u8)index);
    char name[16];
    char description[64];
    gui_debug_i386_vector_name((u8)vector, name, sizeof(name), description, sizeof(description));

    return {{"success", true}, {"vector", hex_text(vector, 2)}, {"vector_name", name},
        {"vector_description", description}, {"source", source}};
}

json DebugAdapter::ClearInterruptBreakpoint(int vector, const std::string& source)
{
    int index = parse_interrupt_source(source);

    if (vector < 0 || vector > 0xFF || index < 0)
        return {{"error", "vector must be 0-255 and source any, exception, hardware or software"}};

    bool removed = m_core->GetI386()->RemoveInterruptBreakpoint((u8)vector, (u8)index);
    return {{"success", true}, {"removed", removed}, {"vector", hex_text(vector, 2)}, {"source", source}};
}

json DebugAdapter::ListInterruptBreakpoints()
{
    json list = json::array();
    std::vector<I386_Interrupt_Breakpoint>* items = m_core->GetI386()->GetInterruptBreakpoints();

    for (size_t i = 0; i < items->size(); i++)
    {
        const I386_Interrupt_Breakpoint& item = (*items)[i];
        char name[16];
        char description[64];
        gui_debug_i386_vector_name(item.vector, name, sizeof(name), description, sizeof(description));
        list.push_back({{"enabled", item.enabled}, {"vector", hex_text(item.vector, 2)}, {"vector_name", name},
            {"vector_description", description},
            {"source", k_mcp_interrupt_sources[item.source % I386_INTERRUPT_SOURCE_COUNT]}});
    }

    return {{"interrupt_breakpoints", list}, {"count", list.size()}};
}

json DebugAdapter::SetIRQBreakpoint(int irq)
{
    if (irq < 0 || irq > 15)
        return {{"error", "irq must be 0-15"}};

    m_core->GetI386()->SetIRQBreakpoint(irq, true);
    return {{"success", true}, {"irq", irq}, {"irq_name", k_debug_irq_sources[irq]}};
}

json DebugAdapter::ClearIRQBreakpoint(int irq)
{
    if (irq < 0 || irq > 15)
        return {{"error", "irq must be 0-15"}};

    bool removed = m_core->GetI386()->IsIRQBreakpoint(irq);
    m_core->GetI386()->SetIRQBreakpoint(irq, false);
    return {{"success", true}, {"removed", removed}, {"irq", irq}};
}

json DebugAdapter::ListIRQBreakpoints()
{
    json list = json::array();

    for (int i = 0; i < 16; i++)
    {
        if (m_core->GetI386()->IsIRQBreakpoint(i))
            list.push_back({{"irq", i}, {"irq_name", k_debug_irq_sources[i]},
                {"enabled", m_core->GetI386()->IsIRQBreakpointEnabled(i)}});
    }

    return {{"irq_breakpoints", list}, {"count", list.size()}};
}

json DebugAdapter::GetBreakpointHit()
{
    I386_Breakpoint_Hit hit;

    if (!m_core->GetI386()->GetBreakpointHit(hit))
        return json();

    if (hit.interrupt)
    {
        char name[16];
        char description[64];
        gui_debug_i386_vector_name(hit.vector, name, sizeof(name), description, sizeof(description));
        json result = {{"kind", "interrupt"}, {"vector", hex_text(hit.vector, 2)}, {"vector_name", name},
            {"vector_description", description},
            {"source", k_mcp_interrupt_sources[hit.source % I386_INTERRUPT_SOURCE_COUNT]}};

        if (hit.line < 16)
        {
            result["irq"] = hit.line;
            result["irq_name"] = k_debug_irq_sources[hit.line];
        }

        const char* function = hit.source == I386_INTERRUPT_SOFTWARE ?
            gui_debug_i386_interrupt_function(hit.vector, hit.ax) : NULL;

        if (IsValidPointer(function))
            result["function"] = function;

        return result;
    }

    u8 space = hit.space % I386_BREAKPOINT_SPACE_COUNT;
    json result = {
        {"kind", breakpoint_type_name(hit.type)},
        {"space", k_mcp_breakpoint_spaces[space]},
        {"address", breakpoint_address_text(hit.address, space)}
    };

    if (hit.type != I386_BREAKPOINT_EXECUTE)
        result["size"] = hit.size;

    if (space == I386_BREAKPOINT_IO && IsValidPointer(gui_debug_port_label((u16)hit.address)))
        result["port_name"] = gui_debug_port_label((u16)hit.address);

    return result;
}

json DebugAdapter::ListMemoryAreas()
{
    json areas = json::array();
    int count = gui_debug_memory_get_area_count();

    for (int i = 0; i < count; i++)
    {
        GuiDebugMemoryArea area;

        if (!gui_debug_memory_get_area(i, area) || area.size == 0)
            continue;

        json flags = json::array();

        for (int f = 0; f < (int)(sizeof(k_mcp_area_flags) / sizeof(k_mcp_area_flags[0])); f++)
        {
            if ((area.flags & (1U << f)) != 0)
                flags.push_back(k_mcp_area_flags[f]);
        }

        json item = {
            {"id", area.id},
            {"name", area.name},
            {"size", area.size},
            {"unit_size", 1},
            {"flags", flags}
        };

        if ((area.flags & GT_DEBUG_REGION_MAPPED) != 0)
            item["physical_base"] = Hex32(area.physical_base);

        areas.push_back(item);
    }

    return {{"areas", areas}, {"count", areas.size()}};
}

bool DebugAdapter::GetMemoryArea(int area, GuiDebugMemoryArea& info, std::string& error)
{
    if (!gui_debug_memory_get_area(area, info))
    {
        error = "Invalid memory area " + std::to_string(area) + ", use list_memory_areas";
        return false;
    }

    return true;
}

json DebugAdapter::ReadMemory(int area, const std::string& offset_text, size_t size)
{
    GuiDebugMemoryArea info;
    std::string error;
    u32 offset = 0;

    if (!GetMemoryArea(area, info, error) || !ParseAreaOffset(info, offset_text, offset, error))
        return {{"error", error}};

    if (size == 0 || size > (size_t)k_mcp_max_read_size)
        return {{"error", "size must be 1-65536 bytes"}};

    if ((u64)offset >= info.size)
        return {{"error", "offset is outside the memory area"}};

    if ((u64)offset + size > info.size)
        size = (size_t)(info.size - offset);

    std::vector<u8> data(size);
    std::vector<GT_Debug_Memory_Status> status(size);
    GT_Debug_Memory_Address address = info.source;
    address.address = offset;
    gui_debug_memory_read(address, &data[0], &status[0], (u32)size);

    std::ostringstream hex;
    int unavailable = 0;

    for (size_t i = 0; i < size; i++)
    {
        if (i > 0)
            hex << " ";

        if (status[i] == GT_DEBUG_MEMORY_VALID || status[i] == GT_DEBUG_MEMORY_READ_ONLY)
            hex << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)data[i];
        else
        {
            hex << "??";
            unavailable++;
        }
    }

    json result = {
        {"area", area},
        {"offset", FormatAreaOffset(info, offset)},
        {"size", size},
        {"data", hex.str()}
    };

    if (unavailable > 0)
    {
        result["unavailable_bytes"] = unavailable;
        result["note"] = "?? marks unmapped bytes or bytes that cannot be read without side effects";
    }

    return result;
}

json DebugAdapter::WriteMemory(int area, const std::string& offset_text, const std::vector<u8>& data)
{
    GuiDebugMemoryArea info;
    std::string error;
    u32 offset = 0;

    if (!GetMemoryArea(area, info, error) || !ParseAreaOffset(info, offset_text, offset, error))
        return {{"error", error}};

    if (info.source.space == GT_DEBUG_MEMORY_IO)
        return {{"error", "The I/O PORTS area is read-only, writes would have side effects"}};

    if (data.empty())
        return {{"error", "bytes is empty"}};

    if ((u64)offset + data.size() > info.size)
        return {{"error", "write goes past the end of the memory area"}};

    GT_Debug_Memory_Address address = info.source;
    address.address = offset;

    if (!gui_debug_memory_write(address, &data[0], (u32)data.size()))
        return {{"error", "Write rejected: the range is read-only, unmapped or unavailable"}};

    return {{"success", true}, {"area", area}, {"offset", FormatAreaOffset(info, offset)},
        {"bytes_written", data.size()}};
}

json DebugAdapter::TranslateAddress(const std::string& text)
{
    McpAddress address;
    std::string error;

    if (!ParseAddress(text, address, error))
        return {{"error", error}};

    I386* cpu = m_core->GetI386();
    GT_Debug_Memory_Translation translation;
    memset(&translation, 0, sizeof(translation));
    bool translated = cpu->DebugTranslateLinear(address.linear, translation);
    json result;

    if (address.logical)
    {
        result["logical"] = Hex(address.segment, 4) + ":" + Hex32(address.offset);

        if (address.segment_register >= 0)
            result["segment_register"] = to_upper(k_mcp_segment_names[address.segment_register]);
    }

    result["linear"] = Hex32(address.linear);
    result["paging"] = (cpu->GetState()->cr0 & 0x80000000U) != 0;

    if (result["paging"].get<bool>())
    {
        result["pde_index"] = address.linear >> 22;
        result["pte_index"] = (address.linear >> 12) & 0x3FF;

        if (translation.page_directory_entry != 0)
        {
            u32 value = 0;
            u8 bytes[4];
            GT_Debug_Memory_Status status[4];
            GT_Debug_Memory_Address physical = {};
            physical.space = GT_DEBUG_MEMORY_PHYSICAL;
            physical.segment_register = -1;
            physical.address = translation.page_directory_entry;
            gui_debug_memory_read(physical, bytes, status, 4);
            value = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | ((u32)bytes[3] << 24);
            result["pde_address"] = Hex32(translation.page_directory_entry);
            result["pde"] = Hex32(value);

            if (translation.page_table_entry != 0)
            {
                physical.address = translation.page_table_entry;
                gui_debug_memory_read(physical, bytes, status, 4);
                value = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | ((u32)bytes[3] << 24);
                result["pte_address"] = Hex32(translation.page_table_entry);
                result["pte"] = Hex32(value);
            }
        }

        if (translated)
        {
            u32 pte = translation.page_flags >> 12;
            u32 pde = translation.page_flags & 0xFFF;
            result["flags"] = {
                {"present", true},
                {"user", (pde & pte & 0x04) != 0},
                {"writable", (pde & pte & 0x02) != 0},
                {"accessed", (pte & 0x20) != 0},
                {"dirty", (pte & 0x40) != 0}
            };
        }
    }

    if (!translated)
    {
        result["success"] = false;
        result["reason"] = translation.reason;
        return result;
    }

    result["success"] = true;
    result["physical"] = Hex32(translation.physical);

    if (translation.bus_valid)
        result["bus"] = Hex32(translation.bus);

    if (translation.region_valid)
    {
        result["region"] = translation.region_name;
        result["region_offset"] = Hex32(translation.region_offset);
    }

    return result;
}

json DebugAdapter::SelectMemoryRange(int area, u32 start_address, u32 end_address)
{
    GuiDebugMemoryArea info;
    std::string error;

    if (!GetMemoryArea(area, info, error))
        return {{"error", error}};

    if (start_address > end_address)
        std::swap(start_address, end_address);

    if ((u64)end_address >= info.size)
        return {{"error", "Selection range outside memory area"}};

    if (!gui_debug_memory_select_range(info.source, start_address, end_address))
        return {{"error", "Unable to apply memory selection"}};

    return {
        {"success", true},
        {"area", area},
        {"start_address", FormatAreaOffset(info, start_address)},
        {"end_address", FormatAreaOffset(info, end_address)}
    };
}

json DebugAdapter::SetMemorySelectionValue(int area, u8 value)
{
    GuiDebugMemoryArea info;
    std::string error;

    if (!GetMemoryArea(area, info, error))
        return {{"error", error}};

    if (info.source.space == GT_DEBUG_MEMORY_IO)
        return {{"error", "The I/O PORTS area is read-only"}};

    int values_written = gui_debug_memory_set_selection_value(info.source, value);

    if (values_written <= 0)
        return {{"error", "No writable selection in this area, use select_memory_range first"}};

    return {{"success", true}, {"area", area}, {"value", Hex(value, 2)}, {"values_written", values_written}};
}

json DebugAdapter::GetMemorySelection(int area)
{
    GuiDebugMemoryArea info;
    std::string error;

    if (!GetMemoryArea(area, info, error))
        return {{"error", error}};

    u32 start = 0;
    u32 end = 0;
    json result = {{"area", area}};

    if (gui_debug_memory_get_selection(info.source, start, end))
    {
        result["selected"] = true;
        result["start"] = FormatAreaOffset(info, start);
        result["end"] = FormatAreaOffset(info, end);
        result["size"] = (u64)end - start + 1;
    }
    else
    {
        result["selected"] = false;
        result["size"] = 0;
    }

    return result;
}

json DebugAdapter::AddMemoryBookmark(int area, u32 address, const std::string& name)
{
    GuiDebugMemoryArea info;
    std::string error;

    if (!GetMemoryArea(area, info, error))
        return {{"error", error}};

    if ((u64)address >= info.size)
        return {{"error", "Bookmark address outside memory area"}};

    GT_Debug_Memory_Address bookmark = info.source;
    bookmark.address = address;
    gui_debug_memory_add_bookmark(bookmark, address, name.c_str());

    return {{"success", true}, {"area", area}, {"address", FormatAreaOffset(info, address)},
        {"name", name.empty() ? "auto-generated" : name}};
}

json DebugAdapter::RemoveMemoryBookmark(int area, u32 address)
{
    GuiDebugMemoryArea info;
    std::string error;

    if (!GetMemoryArea(area, info, error))
        return {{"error", error}};

    GT_Debug_Memory_Address bookmark = info.source;
    bookmark.address = address;
    bool removed = gui_debug_memory_remove_bookmark(bookmark);

    return {{"success", true}, {"removed", removed}, {"area", area}, {"address", FormatAreaOffset(info, address)}};
}

json DebugAdapter::ListMemoryBookmarks(int area)
{
    GuiDebugMemoryArea info;
    std::string error;

    if (!GetMemoryArea(area, info, error))
        return {{"error", error}};

    std::vector<GuiDebugMemoryBookmark> bookmarks;
    gui_debug_memory_get_bookmarks(info.source, bookmarks);
    json items = json::array();

    for (size_t i = 0; i < bookmarks.size(); i++)
    {
        items.push_back({
            {"address", FormatAreaOffset(info, bookmarks[i].address.address)},
            {"name", bookmarks[i].name}
        });
    }

    return {{"area", area}, {"bookmarks", items}, {"count", items.size()}};
}

json DebugAdapter::AddMemoryWatch(int area, u32 address, const std::string& notes, int size)
{
    GuiDebugMemoryArea info;
    std::string error;

    if (!GetMemoryArea(area, info, error))
        return {{"error", error}};

    if ((u64)address >= info.size)
        return {{"error", "Watch address outside memory area"}};

    GT_Debug_Memory_Address watch = info.source;
    watch.address = address;

    if (!gui_debug_memory_add_watch(watch, notes.c_str(), size / 8))
        return {{"error", "Unable to add memory watch, size must be 8, 16, 32 or 64"}};

    return {{"success", true}, {"area", area}, {"address", FormatAreaOffset(info, address)}, {"notes", notes},
        {"size", size}};
}

json DebugAdapter::RemoveMemoryWatch(int area, u32 address)
{
    GuiDebugMemoryArea info;
    std::string error;

    if (!GetMemoryArea(area, info, error))
        return {{"error", error}};

    GT_Debug_Memory_Address watch = info.source;
    watch.address = address;
    bool removed = gui_debug_memory_remove_watch(watch);

    return {{"success", true}, {"removed", removed}, {"area", area}, {"address", FormatAreaOffset(info, address)}};
}

json DebugAdapter::ListMemoryWatches(int area)
{
    GuiDebugMemoryArea info;
    std::string error;

    if (!GetMemoryArea(area, info, error))
        return {{"error", error}};

    std::vector<GuiDebugMemoryWatch> watches;
    gui_debug_memory_get_watches(info.source, watches);
    json items = json::array();

    for (size_t i = 0; i < watches.size(); i++)
    {
        json item = {
            {"address", FormatAreaOffset(info, watches[i].address.address)},
            {"notes", watches[i].name},
            {"size", watches[i].size * 8},
            {"frozen", watches[i].freeze}
        };

        if (watches[i].valid)
        {
            std::ostringstream value;
            value << std::hex << std::uppercase << std::setfill('0') << std::setw(watches[i].size * 2) <<
                (unsigned long long)watches[i].value;
            item["value"] = value.str();
        }
        else
            item["value"] = NULL;

        items.push_back(item);
    }

    return {{"area", area}, {"watches", items}, {"count", items.size()}};
}

json DebugAdapter::MemorySearchCapture(int area, const json& arguments)
{
    GuiDebugMemoryArea info;
    std::string error;
    u32 start = 0;
    u32 size = 0;

    if (!GetMemoryArea(area, info, error) || !GetAreaRange(info, arguments, start, size, error))
        return {{"error", error}};

    int width = arguments.value("width", 8);

    if (width != 8 && width != 16 && width != 32)
        return {{"error", "width must be 8, 16 or 32"}};

    if (!gui_debug_memory_search_capture(info.source, start, size, width / 8))
        return {{"error", "Unable to capture the memory range"}};

    return {{"success", true}, {"area", area}, {"start", FormatAreaOffset(info, start)}, {"size", size},
        {"width", width}};
}

json DebugAdapter::MemorySearch(int area, const std::string& op, const std::string& compare_type, u64 compare_value,
    const std::string& data_type)
{
    GuiDebugMemoryArea info;
    std::string error;

    if (!GetMemoryArea(area, info, error))
        return {{"error", error}};

    int op_index = 0;

    if (op == "<") op_index = 0;
    else if (op == ">") op_index = 1;
    else if (op == "==") op_index = 2;
    else if (op == "!=") op_index = 3;
    else if (op == "<=") op_index = 4;
    else if (op == ">=") op_index = 5;
    else
        return {{"error", "Invalid operator"}};

    bool previous = compare_type == "previous";

    if (compare_type == "address")
    {
        if (compare_value >= info.size)
            return {{"error", "Compare address outside memory area"}};

        u8 bytes[4] = { };
        GT_Debug_Memory_Status status[4];
        GT_Debug_Memory_Address address = info.source;
        address.address = (u32)compare_value;
        gui_debug_memory_read(address, bytes, status, 4);
        compare_value = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | ((u64)bytes[3] << 24);
    }
    else if (!previous && compare_type != "value")
        return {{"error", "Invalid compare_type"}};

    std::vector<GuiDebugMemorySearchResult> results;
    int count = gui_debug_memory_search(info.source, op_index, previous, compare_value, data_type == "signed",
        results);

    if (count < 0)
        return {{"error", "No search captured in this area, call memory_search_capture first"}};

    json items = json::array();
    int max_results = MIN((int)results.size(), 1000);

    for (int i = 0; i < max_results; i++)
    {
        json item = json::array();
        item.push_back(FormatAreaOffset(info, results[i].address));

        if (data_type == "hex")
        {
            item.push_back(Hex((u32)results[i].current, 2));
            item.push_back(Hex((u32)results[i].previous, 2));
        }
        else
        {
            item.push_back(results[i].current);
            item.push_back(results[i].previous);
        }

        items.push_back(item);
    }

    json result = {{"area", area}, {"count", count}, {"fields", json::array({"address", "value", "previous"})},
        {"results", items}};

    if (count > max_results)
        result["total_matches"] = count;

    return result;
}

json DebugAdapter::MemoryFind(int area, const std::string& value, bool text, bool case_sensitive,
    const json& arguments)
{
    GuiDebugMemoryArea info;
    std::string error;
    u32 start = 0;
    u32 size = 0;

    if (!GetMemoryArea(area, info, error) || !GetAreaRange(info, arguments, start, size, error))
        return {{"error", error}};

    if (value.empty())
        return {{"error", text ? "text is empty" : "hex_bytes is empty"}};

    std::vector<u8> pattern;

    if (text)
    {
        if (value.size() > 512)
            return {{"error", "text must not exceed 512 bytes"}};

        pattern.assign(value.begin(), value.end());
    }
    else
    {
        std::string compact;

        for (size_t i = 0; i < value.size(); i++)
        {
            if (!isspace((unsigned char)value[i]))
                compact += value[i];
        }

        if (compact.empty() || (compact.size() & 1) != 0)
            return {{"error", "hex_bytes must contain valid hex byte pairs"}};

        for (size_t i = 0; i < compact.size(); i += 2)
        {
            u8 byte = 0;

            if (!parse_hex_string(compact.c_str() + i, 2, &byte))
                return {{"error", "hex_bytes must contain valid hex byte pairs"}};

            pattern.push_back(byte);
        }

        case_sensitive = true;
    }

    std::vector<u32> addresses;
    int count = gui_debug_memory_find(info.source, start, size, pattern, case_sensitive, addresses, 100);

    if (count < 0)
        return {{"error", "Pattern does not fit in the searched range"}};

    json items = json::array();

    for (size_t i = 0; i < addresses.size(); i++)
        items.push_back(FormatAreaOffset(info, addresses[i]));

    json result = {{"area", area}, {"count", count}, {"addresses", items}};

    if (count > 100)
        result["total_matches"] = count;

    return result;
}

json DebugAdapter::GetI386Status()
{
    I386* cpu = m_core->GetI386();
    I386_State state;
    cpu->CopyState(state);

    const I386_Segment& cs = state.segments[I386_SEGMENT_CS];
    const I386_Segment& ss = state.segments[I386_SEGMENT_SS];
    bool code32 = (cs.attributes & I386_SEGMENT_DEFAULT_32) != 0;
    bool stack32 = (ss.attributes & I386_SEGMENT_DEFAULT_32) != 0;
    static const char* register_names[I386_REG_COUNT] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi" };
    json status;

    for (int i = 0; i < I386_REG_COUNT; i++)
        status[register_names[i]] = Hex32(state.registers[i].value);

    status["eip"] = Hex32(state.eip);
    status["eflags"] = Hex32(state.eflags);

    json flags;

    for (size_t i = 0; i < sizeof(k_mcp_eflags) / sizeof(k_mcp_eflags[0]); i++)
        flags[k_mcp_eflags[i].name] = (state.eflags & k_mcp_eflags[i].mask) != 0;

    status["flags"] = flags;
    status["cs_eip"] = Hex(cs.selector, 4) + ":" + Hex(state.eip, code32 ? 8 : 4);
    status["ss_esp"] = Hex(ss.selector, 4) + ":" + Hex(state.registers[I386_REG_ESP].value, stack32 ? 8 : 4);

    GT_Debug_Memory_Translation pc;
    memset(&pc, 0, sizeof(pc));
    cpu->DebugTranslateLogical(I386_SEGMENT_CS, state.eip, pc);
    status["linear_pc"] = pc.linear_valid ? json(Hex32(pc.linear)) : json(NULL);
    status["physical_pc"] = pc.physical_valid ? json(Hex32(pc.physical)) : json(NULL);

    status["cr0"] = Hex32(state.cr0);
    status["cr2"] = Hex32(state.cr2);
    status["cr3"] = Hex32(state.cr3);

    json cr0_flags;

    for (size_t i = 0; i < sizeof(k_mcp_cr0_flags) / sizeof(k_mcp_cr0_flags[0]); i++)
        cr0_flags[k_mcp_cr0_flags[i].name] = (state.cr0 & k_mcp_cr0_flags[i].mask) != 0;

    status["cr0_flags"] = cr0_flags;
    status["mode"] = execution_mode_name(state.execution_mode);
    status["cpl"] = state.current_privilege_level;
    status["iopl"] = (state.eflags & I386_FLAG_IOPL) >> 12;
    status["code_size"] = code32 ? 32 : 16;
    status["stack_size"] = stack32 ? 32 : 16;

    if (state.last_exception_vector == 0xFF)
        status["last_exception"] = NULL;
    else
    {
        char name[16];
        char description[64];
        u8 vector = state.last_exception_vector;
        gui_debug_i386_vector_name(vector, name, sizeof(name), description, sizeof(description));
        status["last_exception"] = {
            {"vector", Hex(vector, 2)},
            {"name", name},
            {"description", description}
        };
    }

    status["halted"] = state.halted;
    status["shutdown"] = state.shutdown;

    json segments = json::array();

    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
    {
        int index = k_mcp_segment_order[i];
        const I386_Segment& segment = state.segments[index];
        segments.push_back({
            {"name", to_upper(k_mcp_segment_names[index])},
            {"selector", Hex(segment.selector, 4)},
            {"base", Hex32(segment.base)},
            {"limit", Hex32(segment.limit)},
            {"access", Hex(segment.attributes, 4)},
            {"dpl", segment.dpl},
            {"present", (segment.attributes & I386_SEGMENT_PRESENT) != 0},
            {"size", (segment.attributes & I386_SEGMENT_DEFAULT_32) != 0 ? 32 : 16}
        });
    }

    status["segments"] = segments;
    status["gdtr"] = {{"base", Hex32(state.gdtr.base)}, {"limit", Hex(state.gdtr.limit, 4)}};
    status["idtr"] = {{"base", Hex32(state.idtr.base)}, {"limit", Hex(state.idtr.limit, 4)}};
    status["ldtr"] = {{"selector", Hex(state.ldtr.selector, 4)}, {"base", Hex32(state.ldtr.base)},
        {"limit", Hex32(state.ldtr.limit)}, {"access", Hex(state.ldtr.attributes, 4)}, {"dpl", state.ldtr.dpl}};
    status["tr"] = {{"selector", Hex(state.task_register.selector, 4)}, {"base", Hex32(state.task_register.base)},
        {"limit", Hex32(state.task_register.limit)}, {"access", Hex(state.task_register.attributes, 4)},
        {"dpl", state.task_register.dpl}};
    status["debug_registers"] = {
        {"dr0", Hex32(state.debug_registers[0])}, {"dr1", Hex32(state.debug_registers[1])},
        {"dr2", Hex32(state.debug_registers[2])}, {"dr3", Hex32(state.debug_registers[3])},
        {"dr6", Hex32(state.debug_registers[6])}, {"dr7", Hex32(state.debug_registers[7])}
    };
    status["test_registers"] = {{"tr6", Hex32(state.test_registers[0])}, {"tr7", Hex32(state.test_registers[1])}};

    return status;
}

json DebugAdapter::WriteI386Register(const std::string& name, u32 value)
{
    I386* cpu = m_core->GetI386();
    I386_State state;
    cpu->CopyState(state);
    std::string reg = to_upper(name);
    std::string error;
    bool pc_changed = false;
    int general = -1;
    int segment = -1;
    static const char* register_names[I386_REG_COUNT] = { "EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI" };

    for (int i = 0; i < I386_REG_COUNT; i++)
    {
        if (reg == register_names[i])
            general = i;
    }

    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
    {
        if (reg == to_upper(k_mcp_segment_names[i]))
            segment = i;
    }

    if (general >= 0)
        state.registers[general].value = value;
    else if (segment >= 0)
    {
        if (!load_segment(state, segment, value, error))
            return {{"error", error}};

        pc_changed = segment == I386_SEGMENT_CS;
    }
    else if (reg == "EIP")
    {
        state.eip = value;
        state.halted = false;
        pc_changed = true;
    }
    else if (reg == "EFLAGS")
    {
        state.eflags = (value & 0x00037FD5U) | I386_FLAG_FIXED;
        pc_changed = true;
    }
    else if (reg == "CR0")
    {
        state.cr0 = value;
        pc_changed = true;
    }
    else if (reg == "CR2")
        state.cr2 = value;
    else if (reg == "CR3")
    {
        state.cr3 = value;
        pc_changed = true;
    }
    else if (reg.size() == 3 && reg[0] == 'D' && reg[1] == 'R' && reg[2] >= '0' && reg[2] <= '7')
        state.debug_registers[reg[2] - '0'] = value;
    else
        return {{"error", "Invalid register name, use EAX-EDI, EIP, EFLAGS, CR0, CR2, CR3, DR0-DR7 or CS-GS"}};

    state.repeat.active = false;
    cpu->SetState(state);

    if (pc_changed)
        emu_debug_pc_changed = true;

    return {{"success", true}, {"register", reg}, {"value", Hex32(value)}};
}

json DebugAdapter::GetDisassembly(u32 start_address, u32 end_address, int count, int code_size, bool resolve_symbols,
    bool detailed)
{
    I386* cpu = m_core->GetI386();
    I386_State* state = cpu->GetState();
    I386_Segment code = state->segments[I386_SEGMENT_CS];
    bool inside_cs = start_address - code.base <= code.limit;

    if (!inside_cs)
    {
        code.selector = 0;
        code.base = 0;
        code.limit = 0xFFFFFFFF;
        code.attributes = (code.attributes & I386_SEGMENT_DEFAULT_32) | I386_SEGMENT_PRESENT | I386_SEGMENT_READABLE |
            I386_SEGMENT_EXECUTABLE;
    }

    if (code_size == 16)
        code.attributes &= ~I386_SEGMENT_DEFAULT_32;
    else if (code_size == 32)
        code.attributes |= I386_SEGMENT_DEFAULT_32;

    u32 address = start_address;
    json lines = json::array();

    while ((count > 0 ? (int)lines.size() < count : address <= end_address) &&
        (int)lines.size() < k_mcp_max_disassembly_lines)
    {
        u32 eip = address - code.base;
        I386_Disassembler_Record* record = eip <= code.limit ? cpu->Disassemble(code, eip) : NULL;

        if (!IsValidPointer(record) || record->name[0] == 0 || record->size <= 0)
        {
            lines.push_back({{"linear", Hex32(address)}, {"bytes", ""}, {"mnemonic", "??"}, {"operands", ""},
                {"length", 1}});

            if (address == 0xFFFFFFFFU)
                break;

            address++;
            continue;
        }

        std::string mnemonic;
        std::string operands;
        split_instruction(record->name, mnemonic, operands);

        json line = {
            {"linear", Hex32(record->linear)},
            {"bytes", record->bytes},
            {"mnemonic", mnemonic},
            {"operands", operands},
            {"length", record->size}
        };

        if (record->cs != 0 || inside_cs)
            line["logical"] = Hex(record->cs, 4) + ":" + Hex(record->eip, record->default32 ? 8 : 4);

        if (resolve_symbols && IsValidPointer(GetSymbolAt(record->linear)))
            line["symbol"] = GetSymbolAt(record->linear);

        if (detailed)
        {
            int index = get_opcode_index(record);
            u8 opcode = index < record->size ? record->opcodes[index] : 0;
            const char* flow = NULL;

            if (opcode == 0xCF)
                flow = "iret";
            else if (record->returns)
                flow = "return";
            else if (opcode == 0xCC || opcode == 0xCD || opcode == 0xCE || opcode == 0xF1)
                flow = "int";
            else if (record->subroutine)
                flow = "call";
            else if (record->jump)
                flow = record->unconditional ? "jump" : "conditional";

            if (IsValidPointer(flow))
                line["flow"] = flow;

            if (record->jump && record->jump_target_known)
            {
                json target = {{"linear", Hex32(record->jump_linear)}};

                if (IsValidPointer(GetSymbolAt(record->jump_linear)))
                    target["symbol"] = GetSymbolAt(record->jump_linear);

                line["target"] = target;
            }

            if (opcode >= 0xE4 && opcode <= 0xE7 && index + 1 < record->size)
            {
                u16 port = record->opcodes[index + 1];
                const char* label = gui_debug_port_label(port);
                line["port"] = Hex(port, 4);

                if (IsValidPointer(label))
                    line["port_name"] = label;
            }

            if ((opcode == 0xCD && index + 1 < record->size) || opcode == 0xCC || opcode == 0xCE || opcode == 0xF1)
            {
                u8 vector = opcode == 0xCD ? record->opcodes[index + 1] : opcode == 0xCC ? 3 : opcode == 0xCE ? 4 : 1;
                char name[16];
                char description[64];
                gui_debug_i386_vector_name(vector, name, sizeof(name), description, sizeof(description));
                line["vector"] = Hex(vector, 2);
                line["vector_name"] = name;
                line["vector_description"] = description;

                // AX only names the function for the instruction about to run
                if (record->linear == m_core->GetI386()->GetCurrentLinearPC())
                {
                    I386_Debug_State state;
                    m_core->GetI386()->CopyDebugState(state);
                    const char* function = gui_debug_i386_interrupt_function(vector, state.eax);

                    if (IsValidPointer(function))
                        line["function"] = function;
                }
            }
        }

        lines.push_back(line);

        u32 next = address + (u32)record->size;

        if (next <= address)
            break;

        address = next;
    }

    json result = {
        {"start_address", Hex32(start_address)},
        {"code_size", (code.attributes & I386_SEGMENT_DEFAULT_32) != 0 ? 32 : 16},
        {"lines", lines},
        {"count", lines.size()}
    };

    if (count <= 0)
        result["end_address"] = Hex32(end_address);

    if (!inside_cs)
        result["note"] = "The range is outside CS, decoded as flat code with the selected size";

    return result;
}

json DebugAdapter::ListCallStack()
{
    I386* cpu = m_core->GetI386();
    const std::vector<I386_CallStackEntry>& entries = cpu->GetDisassemblerCallStack();
    static const char* kind_names[] = { "call", "interrupt", "interrupt", "exception" };
    json stack = json::array();

    for (int i = (int)entries.size() - 1; i >= 0; i--)
    {
        const I386_CallStackEntry& entry = entries[i];
        json item = {
            {"kind", kind_names[entry.type & 3]},
            {"from", {{"logical", FormatLogical(entry.src_cs, entry.src, entry.src_linear)},
                {"linear", Hex32(entry.src_linear)}}},
            {"to", {{"logical", FormatLogical(entry.dest_cs, entry.dest, entry.dest_linear)},
                {"linear", Hex32(entry.dest_linear)}}},
            {"return", {{"logical", FormatLogical(entry.back_cs, entry.back, entry.back_linear)},
                {"linear", Hex32(entry.back_linear)}}}
        };

        if (entry.type != I386_CALL)
        {
            item["vector"] = Hex(entry.vector, 2);
            item["source"] = entry.type == I386_CALL_SOFTWARE_INTERRUPT ? "software" :
                entry.type == I386_CALL_HARDWARE_INTERRUPT ? "hardware" : "exception";

            char name[16];
            char description[64];
            gui_debug_i386_vector_name(entry.vector, name, sizeof(name), description, sizeof(description));
            item["vector_name"] = name;
            item["vector_description"] = description;
        }

        if (IsValidPointer(GetSymbolAt(entry.dest_linear)))
            item["symbol"] = GetSymbolAt(entry.dest_linear);

        stack.push_back(item);
    }

    return {{"stack", stack}, {"depth", stack.size()}};
}

json DebugAdapter::AddDisassemblerBookmark(u32 address, const std::string& name)
{
    gui_debug_add_disassembler_bookmark(address, name.c_str());
    return {{"success", true}, {"address", Hex32(address)}, {"name", gui_debug_get_disassembler_bookmarks()->back().name}};
}

json DebugAdapter::RemoveDisassemblerBookmark(u32 address)
{
    bool removed = gui_debug_remove_disassembler_bookmark(address);
    return {{"success", true}, {"removed", removed}, {"address", Hex32(address)}};
}

json DebugAdapter::ListDisassemblerBookmarks()
{
    std::vector<DisassemblerBookmark>* bookmarks = gui_debug_get_disassembler_bookmarks();
    json items = json::array();

    for (size_t i = 0; i < bookmarks->size(); i++)
        items.push_back({{"address", Hex32((*bookmarks)[i].address)}, {"name", (*bookmarks)[i].name}});

    return {{"bookmarks", items}, {"count", items.size()}};
}

json DebugAdapter::ListSymbols(const std::string& filter)
{
    json symbols = json::array();
    std::string needle = to_lower(filter);
    const std::map<u32, std::string>& users = gui_debug_get_user_symbols();
    std::map<u32, std::string>::const_iterator user;

    for (user = users.begin(); user != users.end(); user++)
    {
        if (!needle.empty() && to_lower(user->second).find(needle) == std::string::npos)
            continue;

        symbols.push_back({{"address", Hex32(user->first)}, {"name", user->second}, {"type", "user"}});
    }

    const std::map<u32, I386_Disassembler_Record>& records = m_core->GetI386()->GetDisassemblerRecords();
    std::map<u32, I386_Disassembler_Record>::const_iterator record;

    for (record = records.begin(); record != records.end(); record++)
    {
        if (record->second.auto_symbol[0] == 0)
            continue;

        if (!needle.empty() && to_lower(record->second.auto_symbol).find(needle) == std::string::npos)
            continue;

        symbols.push_back({
            {"address", Hex32(record->first)},
            {"name", record->second.auto_symbol},
            {"type", "automatic"}
        });
    }

    return {{"symbols", symbols}, {"count", symbols.size()}};
}

json DebugAdapter::AddSymbol(u32 address, const std::string& name)
{
    if (!gui_debug_add_user_symbol(address, name.c_str()))
        return {{"error", "Invalid symbol name: letters, digits, _ . @ ? $, not starting with a digit, up to 63 characters"}};

    return {{"success", true}, {"address", Hex32(address)}, {"name", name}};
}

json DebugAdapter::RemoveSymbol(u32 address)
{
    return {{"success", true}, {"removed", gui_debug_remove_user_symbol(address)}, {"address", Hex32(address)}};
}

json DebugAdapter::LoadSymbols(const std::string& file_path)
{
    int count = gui_debug_load_symbols(file_path.c_str());

    if (count < 0)
        return {{"error", "Unable to read " + file_path}};

    return {{"success", true}, {"file_path", file_path}, {"loaded", count}};
}

json DebugAdapter::LookupSymbolByName(const std::string& name)
{
    json matches = json::array();
    const std::map<u32, std::string>& users = gui_debug_get_user_symbols();
    std::map<u32, std::string>::const_iterator user;

    for (user = users.begin(); user != users.end(); user++)
    {
        if (name == user->second)
            matches.push_back({{"address", Hex32(user->first)}, {"name", user->second}, {"type", "user"}});
    }

    const std::map<u32, I386_Disassembler_Record>& records = m_core->GetI386()->GetDisassemblerRecords();
    std::map<u32, I386_Disassembler_Record>::const_iterator record;

    for (record = records.begin(); record != records.end(); record++)
    {
        if (name == record->second.auto_symbol)
        {
            matches.push_back({
                {"address", Hex32(record->first)},
                {"name", record->second.auto_symbol},
                {"type", "automatic"}
            });
        }
    }

    return {{"matches", matches}, {"count", matches.size()}};
}

json DebugAdapter::LookupSymbolAtAddress(u32 address)
{
    I386_Disassembler_Record* record = m_core->GetI386()->GetDisassemblerRecord(address);
    json result = {{"address", Hex32(address)}, {"found", false}};
    const char* user = gui_debug_get_user_symbol(address);

    if (IsValidPointer(user))
    {
        result["found"] = true;
        result["name"] = user;
        result["type"] = "user";
    }
    else if (IsValidPointer(record) && record->auto_symbol[0] != 0)
    {
        result["found"] = true;
        result["name"] = record->auto_symbol;
        result["type"] = "automatic";
    }

    return result;
}

json DebugAdapter::GetI386Descriptors(const std::string& table, int start, int count)
{
    GuiDebugDescriptorTable id = table == "gdt" ? GuiDebugDescriptorTable_GDT : table == "ldt" ? GuiDebugDescriptorTable_LDT :
        GuiDebugDescriptorTable_IDT;

    if (table != "gdt" && table != "ldt" && table != "idt")
        return {{"error", "table must be gdt, ldt or idt"}};

    I386_State* state = m_core->GetI386()->GetState();
    u32 total = gui_debug_i386_table_entry_count(id);
    bool ivt = id == GuiDebugDescriptorTable_IDT && state->execution_mode != I386_MODE_PROTECTED;
    count = CLAMP(count, 1, 256);
    json entries = json::array();

    for (u32 i = (u32)MAX(start, 0); i < total && (int)entries.size() < count; i++)
    {
        GuiDebugDescriptor descriptor;
        bool readable = gui_debug_i386_read_table_entry(id, i, descriptor);
        json entry = {{"index", i}};

        if (id != GuiDebugDescriptorTable_IDT)
            entry["selector"] = hex_text((i << 3) | (id == GuiDebugDescriptorTable_LDT ? 4 : 0), 4);
        else
        {
            char name[16];
            char description[64];
            gui_debug_i386_vector_name((u8)i, name, sizeof(name), description, sizeof(description));
            entry["vector_name"] = name;
            entry["vector_description"] = description;
        }

        if (!readable)
        {
            entry["available"] = false;
            entries.push_back(entry);
            continue;
        }

        if (ivt)
        {
            entry["target"] = hex_text(descriptor.gate_selector, 4) + ":" + hex_text(descriptor.gate_offset, 4);
            entry["linear"] = Hex32(descriptor.base);
            entries.push_back(entry);
            continue;
        }

        char type[16];
        char flags[16];
        gui_debug_i386_descriptor_type(descriptor, type, sizeof(type));
        gui_debug_i386_descriptor_flags(descriptor, flags, sizeof(flags));
        entry["raw"] = Hex32(descriptor.high) + Hex32(descriptor.low);
        entry["type"] = (descriptor.low | descriptor.high) == 0 ? "NULL" : type;
        entry["dpl"] = descriptor.dpl;
        entry["present"] = descriptor.present;
        entry["flags"] = flags;

        if (descriptor.gate)
        {
            u32 base = 0;
            u32 limit = 0;
            char reason[GT_DEBUG_MEMORY_REASON_SIZE];
            entry["target"] = hex_text(descriptor.gate_selector, 4) + ":" + Hex32(descriptor.gate_offset);

            if (descriptor.type != 0x05 &&
                gui_debug_i386_selector_base(descriptor.gate_selector, base, limit, reason, sizeof(reason)))
            {
                entry["target_linear"] = Hex32(base + descriptor.gate_offset);

                if (IsValidPointer(GetSymbolAt(base + descriptor.gate_offset)))
                    entry["symbol"] = GetSymbolAt(base + descriptor.gate_offset);
            }
        }
        else
        {
            entry["base"] = Hex32(descriptor.base);
            entry["limit"] = Hex32(descriptor.limit);
        }

        entries.push_back(entry);
    }

    u32 base = id == GuiDebugDescriptorTable_GDT ? state->gdtr.base : id == GuiDebugDescriptorTable_LDT ? state->ldtr.base :
        state->idtr.base;
    u32 limit = id == GuiDebugDescriptorTable_GDT ? state->gdtr.limit : id == GuiDebugDescriptorTable_LDT ?
        state->ldtr.limit : state->idtr.limit;

    return {
        {"table", table},
        {"format", ivt ? "real_mode_ivt" : "descriptors"},
        {"base", Hex32(base)},
        {"limit", Hex32(limit)},
        {"entry_count", total},
        {"entries", entries}
    };
}

json DebugAdapter::GetPageDirectory(int index)
{
    I386_State* state = m_core->GetI386()->GetState();

    if ((state->cr0 & 0x80000000U) == 0)
        return {{"paging", false}, {"cr3", Hex32(state->cr3)}, {"note", "Paging is off: linear addresses are physical"}};

    u32 directory = state->cr3 & 0xFFFFF000U;
    json entries = json::array();
    char flags[16];

    if (index < 0)
    {
        for (u32 i = 0; i < 1024; i++)
        {
            u32 entry = 0;

            if (!gui_debug_i386_read_page_entry(directory, i, entry) || (entry & 1) == 0)
                continue;

            gui_debug_i386_page_flags(entry, flags, sizeof(flags));
            entries.push_back({{"index", hex_text(i, 3)}, {"linear_start", Hex32(i << 22)}, {"pde", Hex32(entry)},
                {"table", Hex32(entry & 0xFFFFF000U)}, {"flags", flags}});
        }

        return {{"paging", true}, {"cr3", Hex32(state->cr3)}, {"cr2", Hex32(state->cr2)}, {"present_entries", entries}};
    }

    if (index > 1023)
        return {{"error", "index must be 0-1023 (3FF)"}};

    u32 directory_entry = 0;

    if (!gui_debug_i386_read_page_entry(directory, (u32)index, directory_entry) || (directory_entry & 1) == 0)
        return {{"error", "Page-directory entry " + hex_text(index, 3) + " is not present"}};

    for (u32 i = 0; i < 1024; i++)
    {
        u32 entry = 0;

        if (!gui_debug_i386_read_page_entry(directory_entry, i, entry) || (entry & 1) == 0)
            continue;

        gui_debug_i386_page_flags(entry, flags, sizeof(flags));
        entries.push_back({{"index", hex_text(i, 3)}, {"linear", Hex32(((u32)index << 22) | (i << 12))},
            {"physical", Hex32(entry & 0xFFFFF000U)}, {"flags", flags}});
    }

    gui_debug_i386_page_flags(directory_entry, flags, sizeof(flags));
    return {
        {"paging", true},
        {"index", hex_text(index, 3)},
        {"pde", Hex32(directory_entry)},
        {"table", Hex32(directory_entry & 0xFFFFF000U)},
        {"pde_flags", flags},
        {"present_pages", entries}
    };
}

json DebugAdapter::GetPICStatus()
{
    PIC* pic = m_core->GetPIC();
    I8259::I8259_State* chips[2] = { pic->GetMaster()->GetState(), pic->GetSlave()->GetState() };
    json irqs = json::array();

    for (int irq = 0; irq < 16; irq++)
    {
        const I8259::I8259_State* chip = chips[irq >> 3];
        u8 bit = (u8)(1 << (irq & 7));
        u8 vector = (u8)((chip->icw2 & 0xF8) | (irq & 7));

        irqs.push_back({
            {"irq", irq},
            {"source", k_debug_irq_sources[irq]},
            {"level", (chip->input_levels & bit) != 0},
            {"requested", (chip->irr & bit) != 0},
            {"in_service", (chip->isr & bit) != 0},
            {"masked", (chip->imr & bit) != 0},
            {"vector", Hex(vector, 2)}
        });
    }

    json controllers = json::array();

    for (int i = 0; i < 2; i++)
    {
        const I8259::I8259_State* chip = chips[i];
        json item = {
            {"name", i == 0 ? "master" : "slave"},
            {"icw1", Hex(chip->icw1, 2)},
            {"icw2", Hex(chip->icw2, 2)},
            {"icw3", Hex(chip->icw3, 2)},
            {"icw4", Hex(chip->icw4, 2)},
            {"trigger", (chip->icw1 & k_i8259_icw1_ltim) ? "level" : "edge"},
            {"cascade", (chip->icw1 & k_i8259_icw1_sngl) == 0},
            {"vector_base", Hex(chip->icw2 & 0xF8, 2)},
            {"upm_8086", (chip->icw4 & k_i8259_icw4_upm) != 0},
            {"auto_eoi", (chip->icw4 & k_i8259_icw4_aeoi) != 0},
            {"buffered", (chip->icw4 & 0x08) != 0},
            {"special_fully_nested", (chip->icw4 & k_i8259_icw4_sfnm) != 0},
            {"init", chip->init_step == I8259::I8259_INIT_READY ? "ready" :
                std::string("waiting ") + to_lower(k_debug_pic_init_names[chip->init_step & 3])},
            {"read_register", chip->read_isr ? "isr" : "irr"},
            {"special_mask", chip->special_mask},
            {"poll_armed", chip->poll_pending},
            {"highest_priority_line", (chip->lowest_priority + 1) & 7},
            {"rotate_on_aeoi", chip->rotate_on_aeoi},
            {"int_output", chip->int_output},
            {"irr", Hex(chip->irr, 2)},
            {"isr", Hex(chip->isr, 2)},
            {"imr", Hex(chip->imr, 2)}
        };

        if (i == 0)
            item["slave_lines"] = Hex(chip->icw3, 2);
        else
            item["slave_id"] = chip->icw3 & 0x07;

        controllers.push_back(item);
    }

    return {{"irqs", irqs}, {"master", controllers[0]}, {"slave", controllers[1]}};
}

json DebugAdapter::GetPITStatus()
{
    PIT* pit = m_core->GetPIT();
    PIT::PIT_State* state = pit->GetState();
    u64 clocks = m_core->GetScheduler()->GetClocks();
    u8 board = pit->Peek(0x0060, clocks);
    json counters = json::array();

    for (int channel = 0; channel < 6; channel++)
    {
        I8253* chip = pit->GetPIT(channel / 3);
        const I8253::I8253_Counter& counter = chip->GetState()->counters[channel % 3];
        u64 tick = pit->GetTick(channel, clocks);
        double rate = channel == k_pit_serial_channel ? 1228800.0 : 307200.0;
        json item = {
            {"channel", channel},
            {"port", Hex((channel < 3 ? 0x0040 : 0x0050) + (channel % 3) * 2, 4)},
            {"use", to_lower(k_debug_pit_uses[channel])},
            {"clock_hz", rate},
            {"programmed", counter.programmed}
        };

        if (counter.programmed)
        {
            u32 count = counter.bcd ? ((counter.reload >> 12) & 0x0F) * 1000 + ((counter.reload >> 8) & 0x0F) * 100 +
                ((counter.reload >> 4) & 0x0F) * 10 + (counter.reload & 0x0F) : counter.reload;

            if (count == 0)
                count = counter.bcd ? 10000 : 0x10000;

            item["mode"] = counter.mode;
            item["mode_name"] = to_lower(k_debug_pit_mode_names[counter.mode % 6]);
            item["access"] = to_lower(k_debug_pit_access_names[counter.access & 3]);
            item["bcd"] = counter.bcd;
            item["reload"] = Hex(counter.reload, 4);
            item["count"] = Hex(chip->PeekCount(channel % 3, tick), 4);
            item["out"] = chip->PeekOutput(channel % 3, tick);
            item["counting"] = counter.counting;

            if (counter.counting && (counter.mode == 2 || counter.mode == 3))
                item["frequency_hz"] = rate / count;
            else if (counter.counting && (counter.mode == 0 || counter.mode == 4))
                item["period_ms"] = (count * 1000.0) / rate;
        }

        counters.push_back(item);
    }

    return {
        {"board", {
            {"timer0_enable", (state->timer_enable & 0x01) != 0},
            {"timer1_enable", (state->timer_enable & 0x02) != 0},
            {"sound", state->sound},
            {"memory_buzzer", state->sound_memory},
            {"latch0", (board & 0x01) != 0},
            {"latch1", (board & 0x02) != 0},
            {"irq0", (board & state->timer_enable & 0x03) != 0},
            {"value", Hex(board, 2)}
        }},
        {"counters", counters}
    };
}

json DebugAdapter::GetDMAStatus()
{
    UPD71071::UPD71071_State* state = m_core->GetDMA()->GetState();
    json control = json::array();

    for (int i = 0; i < 10; i++)
    {
        if ((state->device_control & (1 << i)) != 0)
            control.push_back(k_debug_dma_control_names[i]);
    }

    json channels = json::array();

    for (int channel = 0; channel < 4; channel++)
    {
        const UPD71071::UPD71071_Channel& item = state->channels[channel];
        u8 bit = (u8)(1 << channel);
        u8 mode = item.mode;

        channels.push_back({
            {"channel", channel},
            {"device", k_debug_dma_devices[channel]},
            {"mode", Hex(mode, 2)},
            {"direction", k_debug_dma_direction_names[(mode >> 2) & 3]},
            {"unit", (mode & 0x01) ? "word" : "byte"},
            {"service", to_lower(k_debug_dma_service_names[(mode >> 6) & 3])},
            {"auto_initialize", (mode & 0x10) != 0},
            {"decrement", (mode & 0x20) != 0},
            {"current_address", Hex32(item.current_address)},
            {"base_address", Hex32(item.base_address)},
            {"current_count", Hex(item.current_count, 4)},
            {"base_count", Hex(item.base_count, 4)},
            {"masked", (state->mask & bit) != 0},
            {"request", ((state->request_levels | state->software_requests) & bit) != 0},
            {"terminal_count", (state->status_tc & bit) != 0}
        });
    }

    return {
        {"device_control", Hex(state->device_control, 4)},
        {"device_control_bits", control},
        {"dma_disabled", (state->device_control & k_upd71071_ddma) != 0},
        {"bus_16bit", state->bus_16bit},
        {"mask", Hex(state->mask & 0x0F, 1)},
        {"software_requests", Hex(state->software_requests & 0x0F, 1)},
        {"request_levels", Hex(state->request_levels & 0x0F, 1)},
        {"terminal_count", Hex(state->status_tc & 0x0F, 1)},
        {"selected_channel", state->selected_channel & 3},
        {"selected_register", state->base_access ? "base" : "current"},
        {"high_address", Hex(state->high_address, 2)},
        {"channels", channels}
    };
}

json DebugAdapter::GetRTCStatus()
{
    MSM58321 rtc(*m_core->GetRTC());
    rtc.Synchronize(m_core->GetScheduler()->GetClocks());
    MSM58321::MSM58321_State* state = rtc.GetState();
    const u8* r = state->registers;
    bool hour24 = (r[5] & k_msm58321_24_hour) != 0;
    char date[16];
    char time[16];
    json registers = json::array();

    snprintf(date, sizeof(date), "%d%d-%d%d-%d%d", r[12], r[11], r[10] & 0x01, r[9], r[8] & 0x03, r[7]);
    snprintf(time, sizeof(time), "%d%d:%d%d:%d%d", r[5] & 0x03, r[4], r[3] & 0x07, r[2], r[1] & 0x07, r[0]);

    for (int i = 0; i < 16; i++)
        registers.push_back({{"index", i}, {"name", k_debug_rtc_register_names[i]},
            {"value", Hex(i == k_msm58321_divider_reset ? 0 : r[i] & 0x0F, 1)}});

    return {
        {"date", date},
        {"time", time},
        {"weekday", k_debug_weekday_names[r[6] % 7]},
        {"hour_24", hour24},
        {"pm", !hour24 && (r[5] & k_msm58321_pm) != 0},
        {"leap_phase", (r[8] >> 2) & 0x03},
        {"note", "The MSM58321 keeps a two-digit year"},
        {"interface", {
            {"address", Hex(state->address & 0x0F, 1)},
            {"data", Hex(state->data & 0x0F, 1)},
            {"command", Hex(state->command, 2)},
            {"cs", (state->command & 0x80) != 0},
            {"read", (state->command & 0x04) != 0},
            {"write", (state->command & 0x02) != 0},
            {"address_write", (state->command & 0x01) != 0}
        }},
        {"registers", registers}
    };
}

json DebugAdapter::GetSystemStatus()
{
    const GT_Machine_Config& machine = m_core->GetMachineConfig();
    SystemControl::SystemControl_State* control = m_core->GetSystemControl()->GetState();
    Memory::Memory_State* memory = m_core->GetMemory()->GetState();

    return {
        {"machine", {
            {"model", k_machine_profiles[machine.model].name},
            {"cpu", k_machine_cpus[machine.cpu].name},
            {"cpu_clock_hz", machine.cpu_clock_rate},
            {"ram_kb", machine.ram_size / 1024},
            {"floppy_drives", machine.floppy_drives},
            {"machine_id", "0101"}
        }},
        {"reset", {
            {"cause", Hex(control->reset_cause, 2)},
            {"soft", (control->reset_cause & k_system_control_reset_soft) != 0},
            {"shutdown", (control->reset_cause & k_system_control_reset_shutdown) != 0},
            {"pending", control->reset_pending},
            {"power_off_requested", control->power_off}
        }},
        {"memory_map", {
            {"low_window", memory->main_memory ? "main_ram" : "fmr_devices"},
            {"boot_window", memory->boot_ram ? "ram" : "boot_rom"},
            {"dictionary_window", memory->dictionary},
            {"dictionary_bank", memory->dictionary_bank & 0x0F},
            {"cmos_write_protect", control->write_protect},
            {"port_05e0", Hex(control->port_05e0, 2)}
        }},
        {"serial_rom", {
            {"control", Hex(control->serial_rom_control, 2)},
            {"reset", (control->serial_rom_control & 0x80) != 0},
            {"clock", (control->serial_rom_control & 0x40) != 0},
            {"chip_select", (control->serial_rom_control & 0x20) == 0},
            {"bit", control->serial_rom_bit},
            {"data", m_core->GetSystemControl()->Peek(0x0032) & 0x01},
            {"contents", "FUJITSU, model 0101"}
        }}
    };
}

json DebugAdapter::GetKeyboardStatus()
{
    Keyboard* keyboard = m_core->GetKeyboard();
    Keyboard::Keyboard_State* state = keyboard->GetState();
    json queue = json::array();
    json pressed = json::array();

    for (int i = 0; i < state->fifo_count; i++)
    {
        u8 value = state->fifo[(state->fifo_read + i) & (KEYBOARD_FIFO_SIZE - 1)];
        json item = {{"value", Hex(value, 2)}};

        if ((value & 0x80) != 0)
            item["event"] = std::string((value & 0x10) ? "break" : "make") + ((value & 0x08) ? " ctrl" : "") +
                ((value & 0x04) ? " shift" : "");
        else if (IsValidPointer(gui_debug_key_name(value)))
            item["key"] = gui_debug_key_name(value);

        queue.push_back(item);
    }

    for (int key = GT_KEY_NONE + 1; key < GT_KEY_COUNT; key++)
    {
        if (state->keys[key] && IsValidPointer(gui_debug_key_name(key)))
            pressed.push_back({{"code", Hex(key, 2)}, {"key", gui_debug_key_name(key)}});
    }

    return {
        {"data", Hex(keyboard->Peek(0x0600), 2)},
        {"status", Hex(keyboard->Peek(0x0602), 2)},
        {"irq_enabled", state->irq_enabled},
        {"kbint", state->kbint},
        {"last_command", Hex(state->last_command, 2)},
        {"queue", queue},
        {"queue_size", KEYBOARD_FIFO_SIZE},
        {"pressed", pressed}
    };
}

static const char* const k_mcp_layer_formats[4] = { "off", "16_colors", "256_colors", "32k_colors" };
static const char* const k_mcp_framebuffers[Emu_Debug_Buffer_Count] =
{
    "layer0", "layer1", "sprite_display", "sprite_draw", "custom"
};

static json layer_json(Video* video, int layer)
{
    const u16* crtc = video->GetState()->crtc;
    Emu_Debug_Buffer_Info info;
    emu_debug_get_buffer_info(layer, NULL, info);
    u32 zoom = (u32)crtc[k_video_crtc_zoom] >> (layer * 8);
    u32 hds = crtc[k_video_crtc_hds0 + layer * 2];
    u32 hde = crtc[k_video_crtc_hde0 + layer * 2];
    u32 vds = crtc[k_video_crtc_vds0 + layer * 2];
    u32 vde = crtc[k_video_crtc_vde0 + layer * 2];
    json result = {
        {"layer", layer},
        {"active", layer == 0 || video->IsTwoPage()},
        {"format", k_mcp_layer_formats[info.format & 3]},
        {"h_window", {{"start", hds}, {"end", hde}, {"dots", hde > hds ? hde - hds : 0}}},
        {"v_window", {{"start", vds}, {"end", vde}, {"lines", vde > vds ? (vde - vds) / 2 : 0}}},
        {"vram_start", hex_text(info.page_base + info.start, 5)},
        {"fa", hex_text(crtc[k_video_crtc_fa0 + layer * 4], 4)},
        {"stride_bytes", info.stride},
        {"haj", crtc[k_video_crtc_haj0 + layer * 4]},
        {"field_offset", hex_text(crtc[k_video_crtc_fo0 + layer * 4], 4)},
        {"zoom_x", (zoom & 0x0F) + 1},
        {"zoom_y", ((zoom >> 4) & 0x0F) + 1}
    };

    if (info.window)
        result["visible_size"] = {{"width", info.window_width}, {"height", info.window_height}};
    else
        result["visible_size"] = json();

    return result;
}

json DebugAdapter::GetCRTCStatus()
{
    Video* video = m_core->GetVideo();
    Video::Video_State* state = video->GetState();
    const u16* crtc = state->crtc;
    double clock = k_debug_crtc_clocks[crtc[k_video_crtc_cr1] & 0x03] * 1000000.0;
    u32 line_clocks = (u32)crtc[k_video_crtc_hst] + 1;
    u32 half_lines = (u32)crtc[k_video_crtc_vst] + 1;
    u64 clocks = m_core->GetScheduler()->GetClocks();
    json raster;

    if (state->running)
    {
        u8 status = video->GetSyncStatus(clocks);
        raster = {
            {"line", video->GetBeamHalfLine(clocks) / 2},
            {"dot", video->GetBeamClock(clocks)},
            {"field", (status & 0x08) ? 1 : 0},
            {"h_state", (status & 0x02) ? "sync" : (status & 0x30) ? "display" : "blank"},
            {"v_state", (status & 0x04) ? "sync" : (status & 0xC0) ? "display" : "blank"}
        };
    }

    raster["running"] = state->running;
    raster["vsync_irq_pending"] = state->vsync_irq;

    return {
        {"display", {
            {"dot_clock_hz", (u32)clock},
            {"line_dots", line_clocks},
            {"line_rate_khz", state->running ? json(clock / line_clocks / 1000.0) : json(NULL)},
            {"frame_half_lines", half_lines},
            {"refresh_hz", state->running ? json(clock * 2.0 / ((double)half_lines * line_clocks)) : json(NULL)},
            {"interlaced", (half_lines & 1) != 0},
            {"hsw1", crtc[0x00]},
            {"hsw2", crtc[0x01]},
            {"vst1", crtc[k_video_crtc_vst1]},
            {"vst2", crtc[k_video_crtc_vst2]},
            {"eet", crtc[0x07]}
        }},
        {"raster", raster},
        {"layers", json::array({layer_json(video, 0), layer_json(video, 1)})}
    };
}

json DebugAdapter::GetCRTCRegisters()
{
    Video::Video_State* state = m_core->GetVideo()->GetState();
    json registers = json::array();

    for (int i = 0; i < 32; i++)
        registers.push_back({{"index", Hex(i, 2)}, {"name", k_debug_crtc_register_names[i]}, {"value", Hex(state->crtc[i], 4)}});

    return {{"index", Hex(state->crtc_index, 2)}, {"registers", registers}};
}

json DebugAdapter::WriteCRTCRegister(int reg, u16 value)
{
    if (reg < 0 || reg > 31)
        return {{"error", "Register must be 0-31"}};

    Video* video = m_core->GetVideo();
    u64 clocks = m_core->GetScheduler()->GetClocks();
    u8 saved = video->GetState()->crtc_index;

    video->Write(0x0440, (u8)reg, clocks);
    video->Write(0x0442, (u8)value, clocks);
    video->Write(0x0443, (u8)(value >> 8), clocks);
    video->Write(0x0440, saved, clocks);

    return {
        {"success", true},
        {"register", Hex(reg, 2)},
        {"name", k_debug_crtc_register_names[reg]},
        {"value", Hex(video->GetState()->crtc[reg], 4)}
    };
}

json DebugAdapter::GetVideoOutputStatus()
{
    Video* video = m_core->GetVideo();
    Video::Video_State* state = video->GetState();
    Sprite* sprite = video->GetSprite();
    Emu_Debug_Buffer_Info layer0;
    Emu_Debug_Buffer_Info layer1;
    emu_debug_get_buffer_info(Emu_Debug_Buffer_Layer0, NULL, layer0);
    emu_debug_get_buffer_info(Emu_Debug_Buffer_Layer1, NULL, layer1);
    bool two_page = video->IsTwoPage();
    int palette = (state->output[1] >> 4) & 0x03;
    json output_registers = json::array();
    json mask = json::array();

    for (int i = 0; i < 4; i++)
    {
        output_registers.push_back(Hex(state->output[i], 2));
        mask.push_back(Hex(state->mask[i], 2));
    }

    return {
        {"output_controller", {
            {"index", state->output_index & 3},
            {"registers", output_registers},
            {"mode", two_page ? "two_page" : "single_page"},
            {"layer0_format", k_mcp_layer_formats[layer0.format & 3]},
            {"layer1_format", k_mcp_layer_formats[layer1.format & 3]},
            {"front_layer", two_page ? state->output[1] & 0x01 : 0},
            {"palette_select", palette == 0 ? "layer0" : palette == 2 ? "layer1" : "256"}
        }},
        {"display", {
            {"fda0", Hex(state->display_enable, 2)},
            {"layer0", (state->display_enable & 0x0C) != 0},
            {"layer1", two_page && (state->display_enable & 0x03) != 0}
        }},
        {"status", {
            {"dpmd", state->digital_palette_modified},
            {"sprite_busy", sprite->IsBusy()},
            {"sprite_page", sprite->GetPage() ? 1 : 0}
        }},
        {"vram_write_mask", {{"index", state->mask_index & 1}, {"mask", mask}}},
        {"fmr", {
            {"low_window", m_core->GetMemory()->GetState()->main_memory ? "main_ram" : "fmr"},
            {"access_mask", Hex(state->fmr_mask, 2)},
            {"read_plane", (state->fmr_mask >> 6) & 3},
            {"write_planes", Hex(state->fmr_mask & 0x0F, 1)},
            {"display_planes", Hex(state->fmr_display_planes & 0x0F, 1)},
            {"display_page", state->fmr_display_page ? 1 : 0},
            {"access_page", state->fmr_page ? 1 : 0},
            {"ank", state->fmr_ank},
            {"kanji_jis", Hex(((u32)state->kanji_high << 8) | state->kanji_low, 4)},
            {"kanji_row", state->kanji_row},
            {"tvram_written", state->fmr_text_written}
        }}
    };
}

static json palette_entries(const u8 (*colors)[3], int count, bool nibbles)
{
    json entries = json::array();

    for (int i = 0; i < count; i++)
    {
        const u8* color = colors[i];
        u8 red = nibbles ? (u8)(color[1] | (color[1] >> 4)) : color[1];
        u8 green = nibbles ? (u8)(color[2] | (color[2] >> 4)) : color[2];
        u8 blue = nibbles ? (u8)(color[0] | (color[0] >> 4)) : color[0];
        char rgb[8];
        snprintf(rgb, sizeof(rgb), "#%02X%02X%02X", red, green, blue);

        if (nibbles)
            entries.push_back({{"index", i}, {"b", color[0] >> 4}, {"r", color[1] >> 4}, {"g", color[2] >> 4}, {"rgb", rgb}});
        else
            entries.push_back({{"index", i}, {"b", color[0]}, {"r", color[1]}, {"g", color[2]}, {"rgb", rgb}});
    }

    return entries;
}

json DebugAdapter::GetPalettes(const std::string& palette)
{
    Video* video = m_core->GetVideo();
    Video::Video_State* state = video->GetState();
    int selected = (state->output[1] >> 4) & 0x03;
    json result = {
        {"index", Hex(state->palette_index, 2)},
        {"selected", selected == 0 ? "layer0" : selected == 2 ? "layer1" : "256"},
        {"in_use", {
            {"layer0", video->GetLayerFormat(0) == Video::VIDEO_LAYER_4BPP},
            {"layer1", video->GetLayerFormat(1) == Video::VIDEO_LAYER_4BPP},
            {"256", video->GetLayerFormat(0) == Video::VIDEO_LAYER_8BPP}
        }}
    };
    bool all = palette.empty() || palette == "all";

    if (!all && palette != "layer0" && palette != "layer1" && palette != "256" && palette != "digital")
        return {{"error", "palette must be layer0, layer1, 256, digital or all"}};

    if (all || palette == "layer0")
        result["layer0"] = palette_entries(state->palette16[0], 16, true);

    if (all || palette == "layer1")
        result["layer1"] = palette_entries(state->palette16[1], 16, true);

    if (all || palette == "256")
        result["256"] = palette_entries(state->palette256, 256, false);

    if (all || palette == "digital")
    {
        json digital = json::array();

        for (int i = 0; i < 8; i++)
        {
            u8 value = state->digital_palette[i] & 0x0F;
            digital.push_back({{"port", Hex(0xFD98 + i, 4)}, {"value", Hex(value, 1)}, {"intensity", (value >> 3) & 1},
                {"g", (value >> 2) & 1}, {"r", (value >> 1) & 1}, {"b", value & 1}});
        }

        result["digital"] = digital;
        result["dpmd"] = state->digital_palette_modified;
    }

    return result;
}

json DebugAdapter::GetFrameBuffer(const std::string& buffer, const json& arguments)
{
    if (emu_is_empty())
        return {{"error", "Emulator is powered off"}};

    int index = -1;

    for (int i = 0; i < Emu_Debug_Buffer_Count; i++)
    {
        if (buffer == k_mcp_framebuffers[i])
            index = i;
    }

    if (index < 0)
        return {{"error", "buffer must be layer0, layer1, sprite_display, sprite_draw or custom"}};

    Emu_Debug_Buffer_Request request;
    request.offset = 0;
    request.format = 1;
    request.width = 512;
    request.height = 256;
    request.palette = 0;

    if (index == Emu_Debug_Buffer_Custom)
    {
        u32 offset = 0;
        std::string format = arguments.value("format", "16_colors");
        std::string palette = arguments.value("palette", "layer0");

        if (arguments.contains("offset") && (!arguments["offset"].is_string() ||
            !parse_hex_with_prefix(arguments["offset"].get<std::string>(), &offset)))
            return {{"error", "Invalid offset format"}};

        if (offset >= VIDEO_VRAM_SIZE)
            return {{"error", "offset must be below 80000"}};

        request.offset = offset;
        request.format = format == "16_colors" ? 1 : format == "256_colors" ? 2 : format == "32k_colors" ? 3 : 0;
        request.palette = palette == "layer0" ? 0 : palette == "layer1" ? 1 : palette == "256" ? 2 : -1;
        request.width = arguments.value("width", 512);
        request.height = arguments.value("height", 256);

        if (request.format == 0)
            return {{"error", "format must be 16_colors, 256_colors or 32k_colors"}};

        if (request.palette < 0)
            return {{"error", "palette must be layer0, layer1 or 256"}};

        if (request.width < 1 || request.width > EMU_DEBUG_FRAMEBUFFER_WIDTH || request.height < 1 ||
            request.height > EMU_DEBUG_FRAMEBUFFER_HEIGHT)
            return {{"error", "width must be 1-1024 and height 1-512"}};
    }

    Emu_Debug_Buffer_Info info;
    emu_debug_get_buffer_info(index, &request, info);

    if (info.width <= 0 || info.height <= 0)
        return {{"error", "The layer is off"}, {"format", k_mcp_layer_formats[info.format & 3]}};

    unsigned char* png = NULL;
    int size = emu_get_debug_buffer_png(index, &request, &png);

    if (size <= 0 || !png)
        return {{"error", "Failed to encode the buffer"}};

    json result;
    result["__mcp_image"] = true;
    result["data"] = base64_encode(png, size);
    result["mimeType"] = "image/png";
    free(png);
    return result;
}

static json sprite_json(const Emu_Debug_Sprite& sprite, bool detailed)
{
    json result = {
        {"index", sprite.index},
        {"x", sprite.screen_x},
        {"y", sprite.screen_y},
        {"pattern", sprite.pattern},
        {"colors", sprite.table ? "16_table" : "32k"},
        {"drawn", sprite.drawn},
        {"visible", sprite.visible}
    };

    if (sprite.table)
        result["color_table"] = sprite.color_table;

    json flags = json::array();

    if (sprite.offset) flags.push_back("offset");
    if (sprite.swap) flags.push_back("rotate");
    if (sprite.flip_x) flags.push_back("flip_x");
    if (sprite.flip_y) flags.push_back("flip_y");
    if (sprite.half_x) flags.push_back("half_x");
    if (sprite.half_y) flags.push_back("half_y");
    if (sprite.through) flags.push_back("through");
    if (sprite.hide) flags.push_back("hide");

    result["flags"] = flags;

    if (!detailed)
        return result;

    result["sprite_ram_address"] = hex_text(sprite.address, 5);
    result["raw"] = {{"x", sprite.x}, {"y", sprite.y}};
    result["attributes"] = hex_text(sprite.attributes, 4);
    result["color"] = hex_text(sprite.color, 4);
    result["pattern_address"] = hex_text(sprite.pattern_address, 5);

    if (sprite.table)
        result["color_table_address"] = hex_text(sprite.color_table_address, 5);

    return result;
}

json DebugAdapter::ListSprites(int start, int count, const std::string& filter)
{
    if (filter != "all" && filter != "drawn" && filter != "visible")
        return {{"error", "filter must be all, drawn or visible"}};

    if (start < 0 || start >= (int)k_sprite_entries)
        return {{"error", "start must be 0-1023"}};

    count = CLAMP(count, 1, (int)k_sprite_entries);

    Sprite* sprite = m_core->GetVideo()->GetSprite();
    Sprite::Sprite_State* state = sprite->GetState();
    int first = ((state->registers[k_sprite_control1] & 0x03) << 8) | state->registers[k_sprite_control0];
    int offset_x = ((state->registers[k_sprite_offset_x + 1] & 0x01) << 8) | state->registers[k_sprite_offset_x];
    int offset_y = ((state->registers[k_sprite_offset_y + 1] & 0x01) << 8) | state->registers[k_sprite_offset_y];
    json sprites = json::array();
    int next = -1;

    for (int i = start; i < (int)k_sprite_entries; i++)
    {
        Emu_Debug_Sprite item;
        emu_debug_get_sprite(i, item);

        if ((filter == "drawn" && !item.drawn) || (filter == "visible" && !item.visible))
            continue;

        if ((int)sprites.size() == count)
        {
            next = i;
            break;
        }

        sprites.push_back(sprite_json(item, false));
    }

    json result = {
        {"enabled", sprite->IsEnabled()},
        {"busy", sprite->IsBusy()},
        {"first_drawn", first},
        {"drawn_count", (int)k_sprite_entries - first},
        {"offset_x", offset_x},
        {"offset_y", offset_y},
        {"display_page", sprite->GetDisplayOffset() != 0 ? 1 : 0},
        {"draw_page", sprite->GetPage() ? 1 : 0},
        {"filter", filter},
        {"sprites", sprites}
    };

    if (next >= 0)
        result["next_start"] = next;

    return result;
}

json DebugAdapter::GetSprite(int index, const std::string& format)
{
    if (index < 0 || index >= (int)k_sprite_entries)
        return {{"error", "index must be 0-1023"}};

    if (format != "image" && format != "info")
        return {{"error", "format must be image or info"}};

    if (format == "info")
    {
        Emu_Debug_Sprite sprite;
        emu_debug_get_sprite(index, sprite);
        return sprite_json(sprite, true);
    }

    unsigned char* png = NULL;
    int size = emu_get_sprite_png(index, 8, &png);

    if (size <= 0 || !png)
        return {{"error", "Failed to encode the sprite"}};

    json result;
    result["__mcp_image"] = true;
    result["data"] = base64_encode(png, size);
    result["mimeType"] = "image/png";
    free(png);
    return result;
}

static const double k_mcp_ym3438_sample_rate = (double)GT_SOUND_CLOCK_RATE / k_ym3438_native_sample_cycles;
static const double k_mcp_rf5c68_sample_rate = (double)GT_SOUND_CLOCK_RATE / k_rf5c68_cycles_per_sample;

static json fm_frequency_json(u16 f_number, u8 block)
{
    double hz = (double)f_number * (double)(1 << block) * k_mcp_ym3438_sample_rate / (double)(1 << 21);
    json result = {{"f_number", hex_text(f_number, 3)}, {"block", block}, {"hz", hz}};

    if (f_number != 0 && hz >= 8.0)
    {
        int midi = (int)floor(69.0 + 12.0 * log2(hz / 440.0) + 0.5);
        result["note"] = std::string(k_debug_note_names[((midi % 12) + 12) % 12]) + std::to_string(midi / 12 - 1);
    }

    return result;
}

static json fm_channel_json(YM3438* ym3438, int channel)
{
    YM3438::YM3438_State* state = ym3438->GetState();
    YM3438::YM3438_Channel& ch = state->channels[channel];
    json operators = json::array();

    for (int i = 0; i < YM3438_OPERATOR_COUNT; i++)
    {
        YM3438::YM3438_Operator& op = ch.operators[i];
        u32 attenuation = MIN((u32)ym3438->GetEnvelopeOutput(op) + ((u32)op.total_level << 3), (u32)k_ym3438_envelope_max);

        operators.push_back({
            {"operator", i + 1},
            {"dt", op.detune},
            {"mul", op.multiple},
            {"tl", op.total_level},
            {"ks", op.key_scale},
            {"ar", op.attack_rate},
            {"d1r", op.decay_rate},
            {"d2r", op.sustain_rate},
            {"d1l", op.sustain_level},
            {"rr", op.release_rate},
            {"am", op.amplitude_modulation_enabled != 0},
            {"ssg_eg", hex_text(op.ssg_envelope, 1)},
            {"key_on", op.key_on != 0},
            {"envelope_state", to_lower(trim_text(k_debug_ym3438_envelope_names[op.state & 3]))},
            {"envelope", hex_text(op.envelope, 3)},
            {"level_percent", (int)((k_ym3438_envelope_max - attenuation) * 100 / k_ym3438_envelope_max)}
        });
    }

    json result = {
        {"channel", channel + 1},
        {"muted", ym3438->IsChannelMuted(channel)},
        {"frequency", fm_frequency_json(ch.f_number, ch.block)},
        {"algorithm", ch.algorithm},
        {"feedback", ch.feedback},
        {"pan_left", ch.pan_left != 0},
        {"pan_right", ch.pan_right != 0},
        {"ams", ch.amplitude_modulation},
        {"pms", ch.phase_modulation},
        {"output", ch.output},
        {"operators", operators}
    };

    if (channel == 2)
    {
        json special = json::array();

        for (int i = 0; i < 3; i++)
        {
            json frequency = fm_frequency_json(ch.special_f_number[i], ch.special_block[i]);
            frequency["operator"] = i + 1;
            special.push_back(frequency);
        }

        result["ch3_special_active"] = state->channel_3_mode != 0;
        result["ch3_special_frequencies"] = special;
    }

    if (channel == 5)
        result["dac_replaces_output"] = state->dac_enabled != 0;

    return result;
}

json DebugAdapter::GetYM3438Status(int channel)
{
    if (channel != 0 && (channel < 1 || channel > YM3438_CHANNEL_COUNT))
        return {{"error", "channel must be 1-6"}};

    Audio* audio = m_core->GetAudio();
    YM3438* ym3438 = audio->GetYM3438();
    YM3438::YM3438_State* state = ym3438->GetState();
    double lfo_rate = k_mcp_ym3438_sample_rate / (128.0 * (k_debug_ym3438_lfo_cycles[state->lfo_frequency & 7] + 1));

    json result = {
        {"fm_muted", audio->IsSourceMuted(Audio::AUDIO_SOURCE_FM)},
        {"native_sample_rate_hz", k_mcp_ym3438_sample_rate},
        {"lfo", {{"enabled", state->lfo_enabled != 0}, {"frequency", state->lfo_frequency}, {"rate_hz", lfo_rate}}},
        {"ch3_mode", to_lower(trim_text(k_debug_ym3438_ch3_mode_names[state->channel_3_mode & 3]))},
        {"dac", {{"enabled", state->dac_enabled != 0}, {"data", hex_text((u8)((state->dac_data / 2) + 128), 2)}}},
        {"timer_a", {
            {"value", state->timer_a_register},
            {"period_ms", (1024 - state->timer_a_register) * 1000.0 / k_mcp_ym3438_sample_rate},
            {"load", state->timer_a_load != 0},
            {"enable", state->timer_a_enable != 0},
            {"flag", state->timer_a_flag != 0}
        }},
        {"timer_b", {
            {"value", state->timer_b_register},
            {"period_ms", (256 - state->timer_b_register) * 16 * 1000.0 / k_mcp_ym3438_sample_rate},
            {"load", state->timer_b_load != 0},
            {"enable", state->timer_b_enable != 0},
            {"flag", state->timer_b_flag != 0}
        }},
        {"status", hex_text(state->status, 2)},
        {"busy", state->busy_cycles != 0}
    };

    json channels = json::array();

    for (int i = 0; i < YM3438_CHANNEL_COUNT; i++)
    {
        if (channel == 0 || channel == i + 1)
            channels.push_back(fm_channel_json(ym3438, i));
    }

    result["channels"] = channels;
    return result;
}

json DebugAdapter::GetYM3438Registers(int part)
{
    if (part < 0 || part > 1)
        return {{"error", "part must be 0 or 1"}};

    YM3438* ym3438 = m_core->GetAudio()->GetYM3438();
    u16 latch = ym3438->GetSelectedAddress();
    json registers = json::array();

    for (int address = 0x20; address < 0x100; address++)
    {
        char name[64];

        if (!gui_debug_ym3438_register_name(part, (u8)address, name, sizeof(name)))
            continue;

        registers.push_back({{"address", hex_text(address, 2)}, {"name", name},
            {"value", hex_text(ym3438->GetRegister((u16)((part << 8) | address)), 2)}});
    }

    return {
        {"part", part},
        {"address_latch", {{"part", (latch >> 8) & 1}, {"address", hex_text(latch & 0xFF, 2)}}},
        {"registers", registers}
    };
}

json DebugAdapter::GetRF5C68Status()
{
    Audio* audio = m_core->GetAudio();
    RF5C68* rf5c68 = audio->GetRF5C68();
    RF5C68::RF5C68_State* state = rf5c68->GetState();
    json channels = json::array();

    for (int i = 0; i < RF5C68_CHANNEL_COUNT; i++)
    {
        RF5C68::RF5C68_Channel& ch = state->channels[i];

        channels.push_back({
            {"channel", i + 1},
            {"enabled", ch.enabled != 0},
            {"sounding", state->enabled && ch.enabled},
            {"muted", rf5c68->IsChannelMuted(i)},
            {"envelope", hex_text(ch.envelope, 2)},
            {"pan_left", ch.pan & 0x0F},
            {"pan_right", ch.pan >> 4},
            {"step", hex_text(ch.step, 4)},
            {"playback_rate_hz", k_mcp_rf5c68_sample_rate * ch.step / 2048.0},
            {"loop_start", hex_text(ch.loop_start, 4)},
            {"start", hex_text((u32)ch.start << 8, 4)},
            {"address", hex_text(ch.address >> k_rf5c68_address_fraction_bits, 4)}
        });
    }

    return {
        {"pcm_muted", audio->IsSourceMuted(Audio::AUDIO_SOURCE_PCM)},
        {"sound_enabled", state->enabled},
        {"channel_bank", state->channel_bank + 1},
        {"wave_bank", state->wave_bank},
        {"wave_bank_range", hex_text((u32)state->wave_bank << 12, 4) + "-" + hex_text(((u32)state->wave_bank << 12) | 0x0FFF, 4)},
        {"irq_mask", hex_text(state->irq_mask, 2)},
        {"irq_flags", hex_text(state->irq_flags, 2)},
        {"output_sample_rate_hz", k_mcp_rf5c68_sample_rate},
        {"channels", channels}
    };
}

json DebugAdapter::GetSoundStatus()
{
    Audio* audio = m_core->GetAudio();
    Audio::Audio_State* state = audio->GetState();
    YM3438::YM3438_State* fm = audio->GetYM3438()->GetState();
    RF5C68::RF5C68_State* pcm = audio->GetRF5C68()->GetState();
    json volumes = json::array();

    for (int chip = 0; chip < AUDIO_VOLUME_CHIPS; chip++)
    {
        json channels = json::array();

        for (int channel = 0; channel < AUDIO_VOLUME_CHANNELS; channel++)
        {
            u8 control = state->volume_control[chip][channel];
            s32 gain = audio->GetVolumeGain(chip, channel);
            json entry = {
                {"channel", channel},
                {"data", hex_text(state->volume_data[chip][channel], 2)},
                {"enabled", (control & 0x04) != 0},
                {"fixed_0db", (control & 0x08) != 0},
                {"fixed_minus_32db", (control & 0x10) != 0}
            };

            if (gain > 0)
                entry["gain_db"] = 20.0 * log10((double)gain / 32768.0);
            else
                entry["gain_db"] = json();

            if (chip == 1 && channel < 2)
                entry["use"] = channel == 0 ? "cdda_left" : "cdda_right";

            channels.push_back(entry);
        }

        volumes.push_back({{"chip", chip + 1}, {"port", hex_text(0x04E0 + chip * 2, 4)},
            {"selected_channel", state->volume_channel[chip]}, {"channels", channels}});
    }

    return {
        {"electronic_volume", volumes},
        {"output_gates", {
            {"mute_control", hex_text(state->mute_control, 2)},
            {"fm_enabled", (state->mute_control & 0x02) != 0},
            {"pcm_enabled", (state->mute_control & 0x01) != 0},
            {"output_control", hex_text(state->output_control, 2)},
            {"output_enabled", (state->output_control & 0x40) != 0},
            {"level_leds", (state->output_control & 0x80) == 0}
        }},
        {"interrupts", {
            {"cause_fm", fm->timer_a_flag || fm->timer_b_flag},
            {"cause_pcm", pcm->irq_flags != 0},
            {"fm_timer_a_flag", fm->timer_a_flag != 0},
            {"fm_timer_b_flag", fm->timer_b_flag != 0},
            {"pcm_mask", hex_text(pcm->irq_mask, 2)},
            {"pcm_flags", hex_text(pcm->irq_flags, 2)}
        }},
        {"debugger_mutes", {
            {"fm", audio->IsSourceMuted(Audio::AUDIO_SOURCE_FM)},
            {"pcm", audio->IsSourceMuted(Audio::AUDIO_SOURCE_PCM)},
            {"cdda", audio->IsSourceMuted(Audio::AUDIO_SOURCE_CDDA)}
        }}
    };
}

json DebugAdapter::SetAudioMute(const std::string& source, int channel, bool mute)
{
    Audio* audio = m_core->GetAudio();

    if (source == "fm")
    {
        if (channel == 0)
            audio->SetSourceMute(Audio::AUDIO_SOURCE_FM, mute);
        else if (channel >= 1 && channel <= YM3438_CHANNEL_COUNT)
            audio->GetYM3438()->SetChannelMute(channel - 1, mute);
        else
            return {{"error", "FM channel must be 1-6"}};
    }
    else if (source == "pcm")
    {
        if (channel == 0)
            audio->SetSourceMute(Audio::AUDIO_SOURCE_PCM, mute);
        else if (channel >= 1 && channel <= RF5C68_CHANNEL_COUNT)
            audio->GetRF5C68()->SetChannelMute(channel - 1, mute);
        else
            return {{"error", "PCM channel must be 1-8"}};
    }
    else if (source == "cdda")
    {
        if (channel != 0)
            return {{"error", "CD-DA has no channels"}};

        audio->SetSourceMute(Audio::AUDIO_SOURCE_CDDA, mute);
    }
    else
        return {{"error", "source must be fm, pcm or cdda"}};

    json result = {{"success", true}, {"source", source}, {"muted", mute}};

    if (channel != 0)
        result["channel"] = channel;

    return result;
}

static std::string msf_text(u32 lba)
{
    GT_CdRomMSF msf;
    char text[16];
    LbaToMsf(lba, &msf);
    snprintf(text, sizeof(text), "%02u:%02u:%02u", msf.minutes, msf.seconds, msf.frames);
    return text;
}

static std::string hex_bytes(const u8* data, u32 size)
{
    std::string text;
    char byte[4];

    for (u32 i = 0; i < size; i++)
    {
        snprintf(byte, sizeof(byte), i == 0 ? "%02X" : " %02X", data[i]);
        text += byte;
    }

    return text;
}

json DebugAdapter::GetCDROMStatus()
{
    CdRom* cdrom = m_core->GetCDROM();
    CdRom::CdRom_State* state = cdrom->GetState();
    CdRomMedia* media = m_core->GetCDROMMedia();
    u8 master = cdrom->Peek(0x04C0);
    u8 command = state->command & k_cdrom_command_mask;
    bool reading = gui_debug_cdrom_reading();
    bool ready = media->IsReady();
    u32 head = gui_debug_cdrom_head();
    json params = json::array();
    json queue = json::array();
    static const char* transfer_names[4] = { "off", "ready", "dma", "cpu" };

    for (int i = 0; i < CDROM_PARAM_COUNT; i++)
        params.push_back(hex_text(state->active_params[i], 2));

    for (int i = 0; i < MIN((int)state->status_count, 4); i++)
        queue.push_back(hex_text(state->status[(state->status_head + i) % CDROM_STATUS_QUEUE_SIZE], 2));

    json drive = {{"state", to_lower(gui_debug_cdrom_drive_state())}};

    if (ready)
    {
        s32 track = media->FindTrackFromLBA(head, true);
        drive["head_lba"] = head;
        drive["head_msf"] = msf_text(head + 150);

        if (track >= 0)
            drive["head_track"] = track + 1;
    }

    if (reading)
    {
        drive["read_start_lba"] = state->read_lba;
        drive["read_end_lba"] = state->read_end_lba;
        drive["sectors_left"] = state->read_lba <= state->read_end_lba ? state->read_end_lba - state->read_lba + 1 : 0;
    }

    json media_json = {{"ready", ready}, {"disc_changed", state->disc_changed}};

    if (ready)
    {
        GT_CdRomMSF length = media->GetCdRomLength();
        char text[16];
        snprintf(text, sizeof(text), "%02u:%02u:%02u", length.minutes, length.seconds, length.frames);
        media_json["type"] = media->GetFileExtension();
        media_json["tracks"] = media->GetTrackCount();
        media_json["length"] = text;
        media_json["sectors"] = media->GetSectorCount();
    }

    return {
        {"ports", {
            {"master_status", hex_text(master, 2)},
            {"sirq", (master & 0x80) != 0},
            {"dei", (master & 0x40) != 0},
            {"cpu_transfer", (master & 0x20) != 0},
            {"dma_transfer", (master & 0x10) != 0},
            {"status_available", (master & 0x02) != 0},
            {"command_ready", (master & 0x01) != 0},
            {"command", hex_text(state->command, 2)},
            {"command_name", gui_debug_cdrom_command_name(command)},
            {"command_irq", (state->command & k_cdrom_flag_irq) != 0},
            {"command_status", (state->command & k_cdrom_flag_status) != 0},
            {"parameters", params},
            {"pending_parameters", state->param_count},
            {"status_queue_count", state->status_count},
            {"status_queue_next", queue},
            {"transfer", transfer_names[state->transfer & 3]}
        }},
        {"interrupts", {
            {"sirq_enabled", state->enable_sirq},
            {"sirq_pending", state->sirq},
            {"dei_enabled", state->enable_dei},
            {"dei_pending", state->dei},
            {"irq9", (state->sirq && state->sirq_irq && state->enable_sirq) || (state->dei && state->enable_dei)}
        }},
        {"drive", drive},
        {"media", media_json}
    };
}

json DebugAdapter::ListCDROMTracks()
{
    CdRomMedia* media = m_core->GetCDROMMedia();

    if (!media->IsReady())
        return {{"error", "No CD-ROM inserted"}};

    const std::vector<CdRomImage::Track>& tracks = media->GetTracks();
    s32 current = media->FindTrackFromLBA(gui_debug_cdrom_head(), true);
    json list = json::array();

    for (size_t i = 0; i < tracks.size(); i++)
    {
        const CdRomImage::Track& track = tracks[i];
        json entry = {
            {"track", (int)i + 1},
            {"type", track.type == GT_CDROM_AUDIO_TRACK ? "audio" : "data"},
            {"format", TrackTypeName(track.type)},
            {"start_msf", msf_text(track.start_lba + 150)},
            {"end_msf", msf_text(track.end_lba + 150)},
            {"length", msf_text(track.sector_count)},
            {"start_lba", track.start_lba},
            {"end_lba", track.end_lba},
            {"sectors", track.sector_count},
            {"current", (s32)i == current}
        };

        if (track.has_lead_in)
            entry["pregap_sectors"] = track.start_lba - track.lead_in_lba;

        list.push_back(entry);
    }

    GT_CdRomMSF length = media->GetCdRomLength();
    char text[16];
    snprintf(text, sizeof(text), "%02u:%02u:%02u", length.minutes, length.seconds, length.frames);

    return {
        {"file_name", media->GetFileName()},
        {"length", text},
        {"sectors", media->GetSectorCount()},
        {"note", "MSF values are absolute (LBA + 150), as the drive reports them"},
        {"tracks", list}
    };
}

json DebugAdapter::GetCDROMAudioStatus()
{
    Audio* audio = m_core->GetAudio();
    CdRomAudio::CdRomAudio_State* state = m_core->GetCDROMAudio()->GetState();
    CdRomMedia* media = m_core->GetCDROMMedia();
    static const char* states[3] = { "idle", "playing", "paused" };
    bool active = state->play_state != CdRomAudio::CDROM_AUDIO_IDLE;
    json result = {
        {"state", states[state->play_state % 3]},
        {"cdda_muted", audio->IsSourceMuted(Audio::AUDIO_SOURCE_CDDA)}
    };

    if (active)
    {
        s32 track = media->IsReady() ? media->FindTrackFromLBA(state->current_lba, true) : -1;
        result["end"] = state->repeat ? "repeat" : "stop";
        result["start_lba"] = state->start_lba;
        result["start_msf"] = msf_text(state->start_lba + 150);
        result["stop_lba"] = state->end_lba;
        result["stop_msf"] = msf_text(state->end_lba + 150);
        result["current_lba"] = state->current_lba;
        result["current_msf"] = msf_text(state->current_lba + 150);

        if (track >= 0)
        {
            u32 position = state->current_lba - MIN(state->current_lba, media->GetTracks()[track].start_lba);
            result["track"] = track + 1;
            result["track_position_lba"] = position;
            result["track_position"] = msf_text(position);
        }
    }

    json volume = json::object();

    for (int channel = 0; channel < 2; channel++)
    {
        s32 gain = audio->GetVolumeGain(1, channel);
        volume[channel == 0 ? "left_db" : "right_db"] = gain > 0 ? json(20.0 * log10((double)gain / 32768.0)) : json();
    }

    result["volume"] = volume;
    return result;
}

// A failed read on a physical drive would mark the disc as failed for the game too
json DebugAdapter::ReadCDROMSector(u32 lba, const std::string& mode)
{
    CdRomMedia* media = m_core->GetCDROMMedia();

    if (!media->IsReady())
        return {{"error", "No CD-ROM inserted"}};

    if (m_core->GetMedia()->IsPhysicalCdRom())
        return {{"error", "Not available for physical CD drives: a failed read would stop the drive for the game"}};

    if (lba >= media->GetSectorCount())
        return {{"error", "lba must be below " + std::to_string(media->GetSectorCount())}};

    if (mode != "user" && mode != "raw")
        return {{"error", "mode must be user or raw"}};

    s32 track = media->FindTrackFromLBA(lba);
    bool audio = media->IsAudioSector(lba);

    if (mode == "user" && audio)
        return {{"error", "Audio sectors have no user data; use mode raw"}};

    u8 buffer[2352];
    u32 head = media->GetCurrentSector();
    bool ok = mode == "raw" ? media->ReadRawSector2352(lba, buffer) : media->ReadSector(lba, buffer);
    media->SetCurrentSector(head);

    if (!ok)
        return {{"error", "Failed to read the sector"}};

    u32 size = mode == "raw" ? 2352 : 2048;
    json result = {
        {"lba", lba},
        {"msf", msf_text(lba + 150)},
        {"mode", mode},
        {"type", audio ? "audio" : "data"},
        {"size", size},
        {"data", hex_bytes(buffer, size)}
    };

    if (track >= 0)
        result["track"] = track + 1;

    return result;
}

json DebugAdapter::GetFDCStatus()
{
    FDC* fdc = m_core->GetFDC();
    FDC::FDC_State* state = fdc->GetState();
    MB8877::MB8877_State* mb8877 = fdc->GetMB8877()->GetState();
    u64 clocks = m_core->GetScheduler()->GetClocks();
    u8 status = fdc->Peek(0x0200, clocks);
    u8 drive_status = fdc->Peek(0x0208, clocks);
    char command[48];
    json bits = json::array();

    gui_debug_mb8877_command(mb8877->command, command, sizeof(command));

    for (int bit = 7; bit >= 0; bit--)
    {
        if ((status >> bit) & 1)
            bits.push_back(mb8877->type1 ? k_debug_mb8877_type1_status[bit] : k_debug_mb8877_type2_status[bit]);
    }

    int selected = fdc->GetSelectedDrive();
    json select = {
        {"value", hex_text(state->drive_select, 2)},
        {"rpm", fdc->GetRPM()}
    };

    if (selected >= 0)
        select["selected_drive"] = selected;
    else
        select["selected_drive"] = json();

    return {
        {"registers", {
            {"status", hex_text(status, 2)},
            {"status_type", mb8877->type1 ? "type_1" : "type_2_3"},
            {"status_bits", bits},
            {"command", hex_text(mb8877->command, 2)},
            {"command_decoded", command},
            {"track", hex_text(mb8877->track, 2)},
            {"sector", hex_text(mb8877->sector, 2)},
            {"data", hex_text(mb8877->data, 2)}
        }},
        {"signals", {
            {"busy", (status & 0x01) != 0},
            {"drq", mb8877->drq},
            {"intrq", mb8877->intrq},
            {"irq6", mb8877->intrq && (state->drive_control & k_fdc_irq_enable) != 0}
        }},
        {"drive_control", {
            {"value", hex_text(state->drive_control, 2)},
            {"irq_enable", (state->drive_control & k_fdc_irq_enable) != 0},
            {"density", (state->drive_control & k_fdc_double_density) ? "mfm" : "fm"},
            {"side", (state->drive_control & k_fdc_side) ? 1 : 0},
            {"motor", (state->drive_control & k_fdc_motor) != 0},
            {"slow_clock", (state->drive_control & k_fdc_slow_clock) != 0}
        }},
        {"drive_status", {{"value", hex_text(drive_status, 2)}, {"ready", (drive_status & 0x02) != 0}}},
        {"drive_select", select},
        {"drive_switch", hex_text(state->drive_switch, 2)}
    };
}

json DebugAdapter::ListFloppyDrives()
{
    FDC* fdc = m_core->GetFDC();
    int present = m_core->GetMachineConfig().floppy_drives;
    int selected = fdc->GetSelectedDrive();
    u64 clocks = m_core->GetScheduler()->GetClocks();
    json drives = json::array();

    for (int drive = 0; drive < FDC_DRIVES; drive++)
    {
        FloppyDisk* disk = fdc->GetDisk(drive);
        json entry = {{"drive", drive}, {"present", drive < present}};

        if (drive >= present)
        {
            drives.push_back(entry);
            continue;
        }

        entry["inserted"] = disk->IsInserted();
        entry["head_cylinder"] = fdc->GetState()->cylinders[drive];
        entry["selected"] = selected == drive;
        entry["motor"] = selected == drive && fdc->IsSpinning();
        entry["ready"] = selected == drive && fdc->IsReady(clocks);

        if (disk->IsInserted())
        {
            Emu_FloppyInfo info;
            int cylinders = 0;
            int heads = 0;
            int sectors = 0;
            int sector_size = 0;

            char name[18] = { };
            gui_debug_floppy_disk_name(disk, name, sizeof(name));
            entry["disk_name"] = name;

            if (emu_floppy_get_info(drive, &info))
                entry["image"] = info.path;

            entry["media"] = k_debug_floppy_media_names[disk->GetMedia() % 3];
            entry["rpm"] = disk->GetRPM();
            entry["write_protected"] = disk->IsWriteProtected();
            entry["modified"] = disk->IsDirty();

            if (gui_debug_floppy_geometry(disk, cylinders, heads, sectors, sector_size))
                entry["geometry"] = {{"cylinders", cylinders}, {"heads", heads}, {"sectors", sectors},
                    {"sector_size", sector_size}};
        }

        drives.push_back(entry);
    }

    return {{"drives", drives}};
}

static FloppyDisk* get_floppy_disk(GeartownsCore* core, int drive, std::string& error)
{
    if (drive < 0 || drive >= MIN(FDC_DRIVES, core->GetMachineConfig().floppy_drives))
    {
        error = "This machine has no drive " + std::to_string(drive);
        return NULL;
    }

    FloppyDisk* disk = core->GetFDC()->GetDisk(drive);

    if (!disk->IsInserted())
    {
        error = "No disk in drive " + std::to_string(drive);
        return NULL;
    }

    return disk;
}

json DebugAdapter::ListFloppySectors(int drive, int cylinder, int head)
{
    std::string error;
    FloppyDisk* disk = get_floppy_disk(m_core, drive, error);

    if (!IsValidPointer(disk))
        return {{"error", error}};

    if (cylinder < 0 || cylinder >= k_floppy_tracks / 2 || head < 0 || head > 1)
        return {{"error", "cylinder must be 0-81 and head 0-1"}};

    FloppyDisk_Sector sectors[k_floppy_max_sectors];
    int count = disk->GetSectors(cylinder * 2 + head, sectors);
    json list = json::array();

    for (int i = 0; i < count; i++)
    {
        const FloppyDisk_Sector& sector = sectors[i];
        list.push_back({
            {"index", i},
            {"c", hex_text(sector.id[0], 2)},
            {"h", hex_text(sector.id[1], 2)},
            {"r", hex_text(sector.id[2], 2)},
            {"n", hex_text(sector.id[3], 2)},
            {"size", sector.size},
            {"density", sector.fm ? "fm" : "mfm"},
            {"deleted", sector.deleted},
            {"status", to_lower(gui_debug_floppy_status_name(sector.status))},
            {"image_offset", hex_text(sector.header + k_floppy_sector_header_size, 6)}
        });
    }

    return {{"drive", drive}, {"cylinder", cylinder}, {"head", head}, {"formatted", count > 0}, {"sectors", list}};
}

json DebugAdapter::ReadFloppySector(int drive, int cylinder, int head, int sector)
{
    std::string error;
    FloppyDisk* disk = get_floppy_disk(m_core, drive, error);

    if (!IsValidPointer(disk))
        return {{"error", error}};

    if (cylinder < 0 || cylinder >= k_floppy_tracks / 2 || head < 0 || head > 1 || sector < 0 || sector > 255)
        return {{"error", "cylinder must be 0-81, head 0-1 and sector 0-255"}};

    FloppyDisk_Sector sectors[k_floppy_max_sectors];
    int count = disk->GetSectors(cylinder * 2 + head, sectors);

    for (int i = 0; i < count; i++)
    {
        const FloppyDisk_Sector& entry = sectors[i];

        if (entry.id[2] != sector)
            continue;

        u32 offset = entry.header + k_floppy_sector_header_size;
        u32 size = MIN((u32)entry.size, disk->GetImageSize() > offset ? disk->GetImageSize() - offset : 0);

        return {
            {"drive", drive},
            {"cylinder", cylinder},
            {"head", head},
            {"id", {{"c", hex_text(entry.id[0], 2)}, {"h", hex_text(entry.id[1], 2)}, {"r", hex_text(entry.id[2], 2)},
                {"n", hex_text(entry.id[3], 2)}}},
            {"density", entry.fm ? "fm" : "mfm"},
            {"deleted", entry.deleted},
            {"status", to_lower(gui_debug_floppy_status_name(entry.status))},
            {"image_offset", hex_text(offset, 6)},
            {"size", size},
            {"data", hex_bytes(disk->GetImage() + offset, size)}
        };
    }

    return {{"error", "Sector R=" + hex_text(sector, 2) + " not found on that track"}};
}

json DebugAdapter::InsertFloppy(int drive, const std::string& file_path, const json& write_protected)
{
    if (drive < 0 || drive >= MIN(FDC_DRIVES, m_core->GetMachineConfig().floppy_drives))
        return {{"error", "This machine has no drive " + std::to_string(drive)}};

    if (!emu_floppy_insert(drive, file_path.c_str(), false))
        return {{"error", "Failed to insert the floppy image"}};

    if (write_protected.is_boolean())
        emu_floppy_set_write_protected(drive, write_protected.get<bool>());

    return {{"success", true}, {"drive", drive}, {"file_path", file_path},
        {"write_protected", m_core->GetFDC()->GetDisk(drive)->IsWriteProtected()}};
}

json DebugAdapter::EjectFloppy(int drive)
{
    if (drive < 0 || drive >= MIN(FDC_DRIVES, m_core->GetMachineConfig().floppy_drives))
        return {{"error", "This machine has no drive " + std::to_string(drive)}};

    if (!m_core->GetFDC()->GetDisk(drive)->IsInserted())
        return {{"error", "No disk in drive " + std::to_string(drive)}};

    if (!emu_floppy_eject(drive, false))
        return {{"error", "Failed to keep the disk changes; the disk stays inserted"}};

    return {{"success", true}, {"drive", drive}};
}

json DebugAdapter::SwapFloppies()
{
    if (m_core->GetMachineConfig().floppy_drives < 2)
        return {{"error", "This machine has a single drive"}};

    emu_floppy_swap();
    return {{"success", true}};
}

json DebugAdapter::SetFloppyWriteProtect(int drive, bool write_protected)
{
    std::string error;
    FloppyDisk* disk = get_floppy_disk(m_core, drive, error);

    if (!IsValidPointer(disk))
        return {{"error", error}};

    if (!emu_floppy_set_write_protected(drive, write_protected))
        return {{"error", "This disk comes from a save state and stays write protected"}};

    return {{"success", true}, {"drive", drive}, {"write_protected", disk->IsWriteProtected()}};
}

json DebugAdapter::GetTraceLog(s64 start, int count)
{
    json result;
    TraceLogger* tl = m_core->GetTraceLogger();
    u32 retained = tl->GetCount();
    u64 total = tl->GetSequence();
    u64 oldest = total - retained;

    if (count < 1)
        count = 100;

    if (count > 1000)
        count = 1000;

    u64 actual_start;
    bool overrun = false;

    if (start < 0)
    {
        u64 tail = (u64)(-(start + 1)) + 1;
        actual_start = (total - oldest > tail) ? (total - tail) : oldest;
    }
    else
    {
        actual_start = (u64)start;

        if (actual_start < oldest)
        {
            actual_start = oldest;
            overrun = true;
        }
    }

    if (actual_start >= total)
    {
        result["total_entries"] = retained;
        result["total_logged"] = total;
        result["oldest_sequence"] = oldest;
        result["start"] = actual_start;
        result["next_sequence"] = actual_start;
        result["count"] = 0;
        result["overrun"] = overrun;
        result["lines"] = json::array();
        return result;
    }

    u32 actual_count = (u32)count;

    if ((u64)actual_count > total - actual_start)
        actual_count = (u32)(total - actual_start);

    u32 buffer_start = (u32)(actual_start - oldest);
    json lines = json::array();

    for (u32 i = 0; i < actual_count; i++)
    {
        const GT_Trace_Entry& entry = tl->GetEntry(buffer_start + i);
        char buf[GT_TRACE_FORMAT_BUFFER_SIZE];

        GT_Trace_Format_Options options = {};
        options.linear = true;
        options.registers = true;
        options.segments = true;
        options.flags = true;
        options.bytes = true;
        options.cycles = true;
        options.previous_cycle_valid = (buffer_start + i) > 0;
        options.previous_cycle = options.previous_cycle_valid ? tl->GetEntry(buffer_start + i - 1).cycle : 0;
        trace_logger_format_entry(entry, options, buf, sizeof(buf));
        lines.push_back(buf);
    }

    result["total_entries"] = retained;
    result["total_logged"] = total;
    result["oldest_sequence"] = oldest;
    result["start"] = actual_start;
    result["next_sequence"] = actual_start + actual_count;
    result["count"] = actual_count;
    result["overrun"] = overrun;
    result["lines"] = lines;
    return result;
}

json DebugAdapter::SetTraceLog(bool enabled, u32 flags, const std::string& output, const std::string& memory_size,
    const std::string& disk_size, const std::string& output_path, const u32* event_filters,
    const std::string& vblank_watch_address, const std::string& vblank_watch_operation)
{
    static const char* const k_vblank_watch_operations[] = { "read", "write", "read_write" };
    json result;
    TraceLogger* tl = m_core->GetTraceLogger();

    if (!enabled)
    {
        if (!gui_debug_trace_logger_stop())
        {
            result["error"] = "Unable to stop trace logger cleanly";
            return result;
        }

        tl->SetEnabledFlags(0);
        result["status"] = "stopped";
        result["total_entries"] = tl->GetCount();
        return result;
    }

    bool was_enabled = gui_debug_trace_logger_is_enabled();
    int output_value;

    if (output.empty())
        output_value = was_enabled ? config_debug.trace_output : gui_TraceOutput_Memory;
    else if (output == "memory")
        output_value = gui_TraceOutput_Memory;
    else if (output == "disk")
        output_value = gui_TraceOutput_Disk;
    else
    {
        result["error"] = "Invalid trace output";
        return result;
    }

    int memory_size_value = config_debug.trace_capacity;

    if (!memory_size.empty())
    {
        memory_size_value = gui_debug_trace_logger_memory_size_index(memory_size.c_str());

        if (memory_size_value < 0)
        {
            result["error"] = "Invalid trace memory size";
            return result;
        }
    }

    int disk_size_value = config_debug.trace_disk_size;

    if (!disk_size.empty())
    {
        disk_size_value = gui_debug_trace_logger_disk_size_index(disk_size.c_str());

        if (disk_size_value < 0)
        {
            result["error"] = "Invalid trace disk size";
            return result;
        }
    }

    int vblank_watch_address_value = config_debug.trace_vblank_watch_address;

    if (!vblank_watch_address.empty())
    {
        u32 address = 0;

        if (!parse_hex_with_prefix(vblank_watch_address, &address))
        {
            result["error"] = "Invalid vblank watch address";
            return result;
        }

        vblank_watch_address_value = (int)address;
    }

    int vblank_watch_operation_value = config_debug.trace_vblank_watch_operation;

    if (!vblank_watch_operation.empty())
    {
        vblank_watch_operation_value = -1;

        for (int i = 0; i < 3; i++)
        {
            if (vblank_watch_operation == k_vblank_watch_operations[i])
                vblank_watch_operation_value = i;
        }

        if (vblank_watch_operation_value < 0)
        {
            result["error"] = "Invalid vblank watch operation";
            return result;
        }
    }

    bool configuration_changed = output_value != config_debug.trace_output;

    if (output_value == gui_TraceOutput_Memory)
        configuration_changed = configuration_changed || memory_size_value != config_debug.trace_capacity;
    else
    {
        configuration_changed = configuration_changed || disk_size_value != config_debug.trace_disk_size;

        if (!output_path.empty())
            configuration_changed = configuration_changed ||
                config_debug.trace_disk_dir_option != Directory_Location_Custom ||
                output_path != config_debug.trace_disk_path;
    }

    if (was_enabled && configuration_changed && !gui_debug_trace_logger_stop())
    {
        result["error"] = "Unable to stop trace logger cleanly";
        return result;
    }

    if (!gui_debug_trace_logger_is_enabled() &&
        !gui_debug_trace_logger_configure(output_value, memory_size_value, disk_size_value, output_path.c_str()))
    {
        result["error"] = "Unable to configure trace logger";
        return result;
    }

    gui_debug_trace_logger_set_event_filters(event_filters);
    config_debug.trace_vblank_watch_address = vblank_watch_address_value;
    config_debug.trace_vblank_watch_operation = vblank_watch_operation_value;

    if (!gui_debug_trace_logger_start(flags))
    {
        result["error"] = "Unable to start trace logger";
        return result;
    }

    result["status"] = "started";
    result["output"] = config_debug.trace_output == gui_TraceOutput_Disk ? "disk" : "memory";
    result["memory_size"] = gui_debug_trace_logger_memory_size_name(config_debug.trace_capacity);
    result["disk_size"] = gui_debug_trace_logger_disk_size_name(config_debug.trace_disk_size);

    if (config_debug.trace_output == gui_TraceOutput_Disk)
        result["output_path"] = gui_debug_trace_logger_get_output_path();

    u32 enabled_flags = tl->GetEnabledFlags();
    json event_filter_list = json::array();

    for (int i = 0; i < k_mcp_trace_filter_count; i++)
    {
        const McpTraceFilter& filter = k_mcp_trace_filters[i];
        u32 events = tl->GetEventFilter(filter.type);

        if ((enabled_flags & (1U << filter.type)) != 0 && (events & filter.mask) == filter.mask)
            event_filter_list.push_back(filter.name);
    }

    result["filters"] = event_filter_list;

    if ((enabled_flags & TRACE_FLAG_VIDEO) != 0 &&
        (tl->GetEventFilter(TRACE_VIDEO) & TRACE_VIDEO_EVENT_MISSED_VBLANK) != 0)
    {
        char address[16];
        snprintf(address, sizeof(address), "%08X", (u32)config_debug.trace_vblank_watch_address);
        result["vblank_watch_address"] = address;
        result["vblank_watch_operation"] = k_vblank_watch_operations[config_debug.trace_vblank_watch_operation];
    }

    result["total_entries"] = tl->GetCount();
    return result;
}

struct ProfilerEntry
{
    u32 index;
    u64 key;
    std::string name;
    const char* symbol;
};

static const char* const k_profiler_type_names[] = { "root", "call", "interrupt" };

static bool profiler_entry_compare(const ProfilerEntry& a, const ProfilerEntry& b)
{
    if (a.key != b.key)
        return a.key > b.key;

    return a.index < b.index;
}

static double profiler_round(double value)
{
    return floor((value * 100.0) + 0.5) / 100.0;
}

json DebugAdapter::SetProfiler(const std::string& action)
{
    json result;

    Profiler* profiler = m_core->GetProfiler();

    if (!IsValidPointer(profiler))
    {
        result["error"] = "Profiler not available";
        return result;
    }

    if (action == "start")
    {
        if (!config_debug.debug)
        {
            config_debug.debug = true;
            emu_debug_continue();
        }

        gui_debug_profiler_show(true);
    }
    else if (action == "stop")
        gui_debug_profiler_show(false);
    else if (action == "reset")
    {
        profiler->Reset();
        gui_debug_profiler_reset();
    }
    else
    {
        result["error"] = "Invalid profiler action";
        return result;
    }

    result["success"] = true;
    result["action"] = action;
    result["window_open"] = config_debug.show_profiler;
    return result;
}

json DebugAdapter::GetProfilerData(const std::string& sort, int count, const std::string& filter)
{
    json result;

    Profiler* profiler = m_core->GetProfiler();

    if (!IsValidPointer(profiler) || !IsValidPointer(profiler->GetFunctions()))
    {
        result["error"] = "Profiler not available";
        return result;
    }

    if (count < 1)
        count = 50;

    if (count > 1000)
        count = 1000;

    profiler->Sync();

    const GT_Profiler_Function* functions = profiler->GetFunctions();
    u32 function_count = profiler->GetFunctionCount();
    u64 total = profiler->GetTotalCycles();
    u32 frames = profiler->GetFrames();

    std::string filter_upper = filter;
    std::transform(filter_upper.begin(), filter_upper.end(), filter_upper.begin(), ::toupper);

    std::vector<ProfilerEntry> entries;

    for (u32 i = 0; i < function_count; i++)
    {
        const GT_Profiler_Function& function = functions[i];
        bool pseudo = (function.type == PROFILER_FUNCTION_ROOT);

        ProfilerEntry entry;
        entry.index = i;
        entry.key = 0;
        entry.symbol = "none";

        if (function.type == PROFILER_FUNCTION_ROOT)
            entry.name = "[Root]";
        else
        {
            bool is_manual = false;
            const char* name = gui_debug_get_symbol_name(function.address, &is_manual);

            if (IsValidPointer(name))
            {
                entry.name = name;
                entry.symbol = is_manual ? "manual" : "auto";
            }
        }

        if (!filter_upper.empty())
        {
            std::string name_upper = entry.name;
            std::transform(name_upper.begin(), name_upper.end(), name_upper.begin(), ::toupper);

            char address[16];
            snprintf(address, sizeof(address), "%08X", function.address);

            bool name_match = (name_upper.find(filter_upper) != std::string::npos);
            bool address_match = !pseudo && (std::string(address).find(filter_upper) != std::string::npos);

            if (!name_match && !address_match)
                continue;
        }

        if (sort == "exclusive")
            entry.key = function.exclusive_cycles;
        else if (sort == "calls")
            entry.key = function.calls;
        else if (sort == "average")
            entry.key = (function.completed > 0) ? function.inclusive_cycles / function.completed : 0;
        else if (sort == "max")
            entry.key = (function.completed > 0) ? function.max_cycles : 0;
        else
            entry.key = function.inclusive_cycles;

        entries.push_back(entry);
    }

    std::sort(entries.begin(), entries.end(), profiler_entry_compare);

    json functions_array = json::array();

    for (size_t i = 0; (i < entries.size()) && (i < (size_t)count); i++)
    {
        const GT_Profiler_Function& function = functions[entries[i].index];
        bool root = (function.type == PROFILER_FUNCTION_ROOT);
        json item;
        char text[16];

        item["name"] = entries[i].name;
        item["type"] = k_profiler_type_names[function.type];

        if (!root)
        {
            item["symbol"] = entries[i].symbol;
            snprintf(text, sizeof(text), "%08X", function.address);
            item["address"] = text;
        }

        if (function.type == PROFILER_FUNCTION_INTERRUPT)
        {
            char name[16];
            char description[64];
            gui_debug_i386_vector_name(function.vector, name, sizeof(name), description, sizeof(description));
            snprintf(text, sizeof(text), "%02X", function.vector);
            item["vector"] = text;
            item["vector_name"] = name;
            item["vector_description"] = description;
        }

        if (!root)
        {
            item["calls"] = function.calls;
            item["calls_per_frame"] = (frames > 0) ? profiler_round((double)function.calls / (double)frames) : 0.0;
            item["inclusive_cycles"] = function.inclusive_cycles;
            item["inclusive_percent"] = (total > 0) ?
                profiler_round(((double)function.inclusive_cycles * 100.0) / (double)total) : 0.0;
        }

        item["exclusive_cycles"] = function.exclusive_cycles;
        item["exclusive_percent"] = (total > 0) ?
            profiler_round(((double)function.exclusive_cycles * 100.0) / (double)total) : 0.0;

        if (!root && (function.completed > 0))
        {
            item["average_cycles"] = function.inclusive_cycles / function.completed;
            item["min_cycles"] = function.min_cycles;
            item["max_cycles"] = function.max_cycles;
        }

        functions_array.push_back(item);
    }

    result["collecting"] = profiler->IsEnabled();
    result["window_open"] = config_debug.show_profiler;
    result["total_cycles"] = total;
    result["frames"] = frames;
    result["function_count"] = function_count - 1;
    result["sort"] = sort;
    result["count"] = functions_array.size();
    result["functions"] = functions_array;

    return result;
}

json DebugAdapter::GetScreenshot()
{
    json result;

    if (!m_core || emu_is_empty())
    {
        result["error"] = "Emulator is powered off";
        return result;
    }

    GT_Runtime_Info runtime;
    emu_get_runtime(runtime);

    unsigned char* png_buffer = NULL;
    int png_size = emu_get_screenshot_png(&png_buffer);

    if (png_size <= 0 || !png_buffer)
    {
        result["error"] = "Failed to capture screenshot";
        return result;
    }

    std::string base64_png = base64_encode(png_buffer, png_size);
    free(png_buffer);

    result["__mcp_image"] = true;
    result["data"] = base64_png;
    result["mimeType"] = "image/png";
    result["width"] = runtime.screen_width;
    result["height"] = runtime.screen_height;
    return result;
}

static const char* const k_video_recording_ratios[] = { "screen", "square", "4:3", "16:9" };
static const char* const k_video_recording_qualities[] = { "low", "medium", "high", "lossless" };
static const int k_video_recording_ratio_count =
    sizeof(k_video_recording_ratios) / sizeof(k_video_recording_ratios[0]);
static const int k_video_recording_quality_count =
    sizeof(k_video_recording_qualities) / sizeof(k_video_recording_qualities[0]);

static int find_video_recording_option(const std::string& value, const char* const* options, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (value == options[i])
            return i;
    }
    return -1;
}

json DebugAdapter::StartVideoRecording(const std::string& file_path, int scale, const std::string& aspect_ratio,
    const std::string& quality)
{
    json result;

    if (!m_core || emu_is_empty())
    {
        result["error"] = "Emulator is powered off";
        Log("[MCP] StartVideoRecording failed: Emulator is powered off");
        return result;
    }

    if (emu_is_video_recording())
    {
        result["error"] = "Video recording is already active";
        return result;
    }

    int ratio = aspect_ratio.empty() ? config_video.recording_ratio :
        find_video_recording_option(aspect_ratio, k_video_recording_ratios, k_video_recording_ratio_count);
    int quality_index = quality.empty() ? config_video.recording_quality :
        find_video_recording_option(quality, k_video_recording_qualities, k_video_recording_quality_count);

    if ((scale != 0) && ((scale < 1) || (scale > 20)))
    {
        result["error"] = "Invalid scale";
        return result;
    }

    if ((ratio < 0) || (quality_index < 0))
    {
        result["error"] = "Invalid aspect ratio or quality";
        return result;
    }

    if (scale != 0)
        config_video.recording_scale = scale;
    config_video.recording_ratio = ratio;
    config_video.recording_quality = quality_index;

    if (!gui_action_start_video_recording(file_path.empty() ? NULL : file_path.c_str()))
    {
        result["error"] = "Failed to start video recording";
        Log("[MCP] StartVideoRecording failed: %s", file_path.c_str());
        return result;
    }

    result["success"] = true;
    result["file_path"] = video_recorder_get_file_path();
    result["scale"] = config_video.recording_scale;
    result["aspect_ratio"] = k_video_recording_ratios[config_video.recording_ratio];
    result["quality"] = k_video_recording_qualities[config_video.recording_quality];

    return result;
}

json DebugAdapter::StopVideoRecording()
{
    json result;

    if (!emu_is_video_recording())
    {
        result["error"] = "Video recording is not active";
        return result;
    }

    std::string file_path = video_recorder_get_file_path();
    u32 frames = video_recorder_get_frame_count();

    gui_action_stop_video_recording();

    result["success"] = true;
    result["file_path"] = file_path;
    result["frames"] = frames;

    return result;
}

json DebugAdapter::GetMediaInfo()
{
    Media* media = m_core->GetMedia();
    Firmware* firmware = m_core->GetFirmware();
    json result = {
        {"emulator", GT_TITLE},
        {"emulator_version", GT_VERSION},
        {"ready", media->IsReady()},
        {"bios_ready", firmware->IsReady()},
        {"firmware_ready", firmware->IsReady()},
        {"firmware_directory", firmware->GetDirectory()},
        {"file_path", media->GetFilePath()},
        {"file_name", media->GetFileName()},
        {"file_directory", media->GetFileDirectory()},
        {"file_extension", media->GetFileExtension()},
        {"size", media->GetSize()},
        {"crc", media->GetCRC()}
    };

    result["firmware"] = json::array();

    for (int i = 0; i < GT_FIRMWARE_COUNT; i++)
    {
        GT_Firmware_Type type = (GT_Firmware_Type)i;
        const GT_Firmware_Info& info = firmware->GetInfo(type);
        result["firmware"].push_back({
            {"type", Firmware::GetComponentName(type)},
            {"file_name", Firmware::GetFileName(type)},
            {"path", info.path},
            {"size", info.size},
            {"crc", info.crc},
            {"required", Firmware::IsRequired(type)},
            {"loaded", info.loaded},
            {"recognized", info.recognized},
            {"synthetic", info.synthetic},
            {"database_name", info.database_name}
        });
    }

    static const char* media_names[] = { "2D", "2DD", "2HD" };
    json drives = json::array();

    for (int i = 0; i < config_floppy_drives; i++)
    {
        Emu_FloppyInfo info;
        json drive = {{"drive", i}, {"present", i < m_core->GetMachineConfig().floppy_drives}};

        if (emu_floppy_get_info(i, &info) && info.inserted)
        {
            FloppyDisk* disk = m_core->GetFloppy(i);
            drive["inserted"] = true;
            drive["file_path"] = info.path;
            drive["disk_name"] = IsValidPointer(emu_floppy_get_disk_name(i, info.disk_index)) ?
                emu_floppy_get_disk_name(i, info.disk_index) : "";
            drive["disk_index"] = info.disk_index;
            drive["disk_count"] = info.disk_count;
            drive["media"] = IsValidPointer(disk) ? media_names[disk->GetMedia() % 3] : "";
            drive["write_protected"] = info.write_protected;
            drive["modified"] = info.dirty;
        }
        else
            drive["inserted"] = false;

        drives.push_back(drive);
    }

    result["floppy_drives"] = drives;
    return result;
}

json DebugAdapter::ListRecentMedia()
{
    json result;
    json recent_media = json::array();

    for (int index = 0; index < config_max_recent_roms; index++)
    {
        const std::string& path = config_emulator.recent_roms[index];

        if (path.empty())
            continue;

        json entry;
        entry["index"] = index;
        entry["file_path"] = path;
        entry["file_name"] = get_filename(path.c_str());
        recent_media.push_back(entry);
    }

    json recent_floppies = json::array();

    for (int index = 0; index < config_max_recent_floppies; index++)
    {
        const std::string& path = config_emulator.recent_floppies[index];

        if (path.empty())
            continue;

        recent_floppies.push_back({
            {"index", index},
            {"file_path", path},
            {"file_name", get_filename(path.c_str())}
        });
    }

    result["count"] = recent_media.size();
    result["recent_media"] = recent_media;
    result["recent_floppies"] = recent_floppies;

    return result;
}

json DebugAdapter::LoadBios(const std::string& directory_path)
{
    if (directory_path.empty())
        return {{"error", "Firmware directory path is required"}};

    if (!emu_load_bios(directory_path.c_str()))
        return {{"error", "Failed to load FM Towns firmware directory"}};

    if (emu_is_empty())
        emu_power_on();

    config_debug.debug = true;
    emu_debug_break();

    return {
        {"success", true},
        {"directory_path", directory_path},
        {"firmware_ready", m_core->GetFirmware()->IsReady()}
    };
}

json DebugAdapter::StartLoadMedia(const std::string& file_path)
{
    if (file_path.empty())
        return {{"error", "File path is required"}};

    if (!gui_load_rom(file_path.c_str()))
        return {{"error", "Another media load is already in progress"}};

    return {{"file_path", file_path}};
}

bool DebugAdapter::IsMediaLoading() const
{
    return gui_is_rom_loading() && emu_is_media_loading();
}

json DebugAdapter::FinishLoadMedia(const std::string& file_path)
{
    if (gui_is_rom_loading() && !gui_finish_loading_rom())
        return {{"error", "Failed to load media file"}};

    if (!m_core->GetMedia()->IsReady())
        return {{"error", "Failed to load media file"}};

    return {{"success", true}, {"file_path", file_path}};
}

json DebugAdapter::ListSaveStateSlots()
{
    json result;
    json slots = json::array();
    json empty_slots = json::array();

    update_savestates_data();

    for (int i = 0; i < 5; i++)
    {
        json slot;
        slot["slot"] = i + 1;

        if (emu_savestates[i].rom_name[0] != 0)
        {
            slot["rom_name"] = emu_savestates[i].rom_name;
            slot["timestamp"] = emu_savestates[i].timestamp;
            slot["version"] = emu_savestates[i].version;
            slot["valid"] = emu_savestates[i].version == GT_SAVESTATE_VERSION;
            slot["has_screenshot"] = IsValidPointer(emu_savestates_screenshots[i].data);

            if (emu_savestates[i].emu_build[0] != 0)
                slot["emu_build"] = emu_savestates[i].emu_build;

            slots.push_back(slot);
        }
        else
        {
            empty_slots.push_back(i + 1);
        }
    }

    result["current_slot"] = config_emulator.save_slot + 1;
    result["empty_slots"] = empty_slots;
    result["slots"] = slots;

    return result;
}

json DebugAdapter::SelectSaveStateSlot(int slot)
{
    json result;

    if (slot < 1 || slot > 5)
    {
        result["error"] = "Invalid slot number (must be 1-5)";
        Log("[MCP] SelectSaveStateSlot failed: Invalid slot %d", slot);
        return result;
    }

    config_emulator.save_slot = slot - 1;

    result["success"] = true;
    result["slot"] = slot;

    return result;
}

json DebugAdapter::SaveState()
{
    json result;

    if (!m_core || emu_is_empty())
    {
        result["error"] = "Emulator is powered off";
        Log("[MCP] SaveState failed: Emulator is powered off");
        return result;
    }

    int slot = config_emulator.save_slot + 1;
    emu_save_state_slot(slot);

    result["success"] = true;
    result["slot"] = slot;
    result["rom_name"] = m_core->GetMedia()->GetFileName();

    return result;
}

json DebugAdapter::LoadState()
{
    json result;

    if (!m_core || emu_is_empty())
    {
        result["error"] = "Emulator is powered off";
        Log("[MCP] LoadState failed: Emulator is powered off");
        return result;
    }

    int slot = config_emulator.save_slot + 1;

    update_savestates_data();

    if (emu_savestates[config_emulator.save_slot].rom_name[0] == 0)
    {
        result["error"] = "Save state slot is empty";
        Log("[MCP] LoadState failed: Slot %d is empty", slot);
        return result;
    }

    emu_load_state_slot(slot);

    result["success"] = true;
    result["slot"] = slot;

    return result;
}

json DebugAdapter::SaveStateFile(const std::string& file_path)
{
    json result;

    if (file_path.empty())
    {
        result["error"] = "File path is required";
        Log("[MCP] SaveStateFile failed: File path is required");
        return result;
    }

    if (!m_core || emu_is_empty())
    {
        result["error"] = "Emulator is powered off";
        Log("[MCP] SaveStateFile failed: Emulator is powered off");
        return result;
    }

    if (!m_core->SaveState(file_path.c_str(), -1, false))
    {
        result["error"] = "Failed to save state file";
        Log("[MCP] SaveStateFile failed: %s", file_path.c_str());
        return result;
    }

    result["success"] = true;
    result["file_path"] = file_path;

    return result;
}

json DebugAdapter::LoadStateFile(const std::string& file_path)
{
    json result;

    if (file_path.empty())
    {
        result["error"] = "File path is required";
        Log("[MCP] LoadStateFile failed: File path is required");
        return result;
    }

    if (!m_core || emu_is_empty())
    {
        result["error"] = "Emulator is powered off";
        Log("[MCP] LoadStateFile failed: Emulator is powered off");
        return result;
    }

    if (!m_core->LoadState(file_path.c_str()))
    {
        result["error"] = "Failed to load state file";
        Log("[MCP] LoadStateFile failed: %s", file_path.c_str());
        return result;
    }

    emu_floppy_reconcile();
    emu_debug_state_restored();
    events_sync_input();
    rewind_reset();

    result["success"] = true;
    result["file_path"] = file_path;

    return result;
}

u16 DebugAdapter::ButtonMask(const std::string& button) const
{
    std::string name = to_lower(button);

    if (name == "up") return GT_GAMEPAD_UP;
    if (name == "down") return GT_GAMEPAD_DOWN;
    if (name == "left") return GT_GAMEPAD_LEFT;
    if (name == "right") return GT_GAMEPAD_RIGHT;
    if (name == "select") return GT_GAMEPAD_SELECT;
    if (name == "run") return GT_GAMEPAD_RUN;
    if (name == "a") return GT_GAMEPAD_A;
    if (name == "b") return GT_GAMEPAD_B;
    if (name == "c") return GT_GAMEPAD_C;
    if (name == "x") return GT_GAMEPAD_X;
    if (name == "y") return GT_GAMEPAD_Y;
    if (name == "z") return GT_GAMEPAD_Z;
    if (name == "zoom") return GT_GAMEPAD_ZOOM;

    return 0;
}

json DebugAdapter::ControllerButton(int player, const std::string& button, const std::string& action)
{
    if (player < 1 || player > GT_MAX_GAMEPADS)
        return {{"error", "Invalid player number (must be 1-2)"}};

    u16 mask = ButtonMask(button);

    if (mask == 0)
        return {{"error", "Invalid button name"}};

    // A mouse moves with the directions, held or one step per tap, and clicks with A (left) and B (right)
    if (IsMouseController(player))
    {
        const u16 directions = GT_GAMEPAD_UP | GT_GAMEPAD_DOWN | GT_GAMEPAD_LEFT | GT_GAMEPAD_RIGHT;

        if ((mask & (directions | GT_GAMEPAD_A | GT_GAMEPAD_B)) == 0)
            return {{"error", "A mouse only supports up, down, left, right, A (left button) and B (right button)"}};

        if (mask & directions)
        {
            if (action != "press" && action != "release" && action != "press_and_release")
                return {{"error", "Invalid action"}};

            json result = {{"success", true}, {"player", player}, {"button", button}, {"action", action}};

            if (action == "press_and_release")
            {
                int delta_x = mask == GT_GAMEPAD_LEFT ? -k_mcp_mouse_motion_step :
                    (mask == GT_GAMEPAD_RIGHT ? k_mcp_mouse_motion_step : 0);
                int delta_y = mask == GT_GAMEPAD_UP ? -k_mcp_mouse_motion_step :
                    (mask == GT_GAMEPAD_DOWN ? k_mcp_mouse_motion_step : 0);
                ApplyMouseMotion(player, delta_x, delta_y);
                result["mouse_delta_x"] = delta_x;
                result["mouse_delta_y"] = delta_y;
            }
            else
                result["__mouse_motion"] = true;

            return result;
        }
    }

    bool delayed_release = false;

    if (action == "press")
        m_buttons[player - 1] |= mask;
    else if (action == "release")
        m_buttons[player - 1] &= (u16)~mask;
    else if (action == "press_and_release")
    {
        m_buttons[player - 1] |= mask;
        delayed_release = true;
    }
    else
        return {{"error", "Invalid action"}};

    ApplyControllerState(player);
    json result = {
        {"success", true},
        {"player", player},
        {"button", button},
        {"action", action}
    };

    if (delayed_release)
        result["__delayed_release"] = true;

    return result;
}

static const char* const k_controller_type_names[] =
{
    "none", "original_gamepad", "marty_gamepad", "six_button_gamepad", "mouse"
};

static const int k_controller_type_count = (int)(sizeof(k_controller_type_names) / sizeof(k_controller_type_names[0]));

// The configuration follows so the desktop routes its host mouse and bindings the same way
json DebugAdapter::ControllerSetType(int player, const std::string& type)
{
    if (player < 1 || player > GT_MAX_GAMEPADS)
        return {{"error", "Invalid player number (must be 1-2)"}};

    std::string name = to_lower(type);

    for (int i = 0; i < k_controller_type_count; i++)
    {
        if (name != k_controller_type_names[i])
            continue;

        config_input.controller_type[player - 1] = i;
        emu_set_pad_type((GT_Controllers)(player - 1), (GT_Controller_Type)i);
        return {{"success", true}, {"player", player}, {"type", name}};
    }

    return {{"error", "Invalid controller type (must be: none, original_gamepad, marty_gamepad, six_button_gamepad, mouse)"}};
}

json DebugAdapter::ControllerGetType(int player)
{
    if (player < 1 || player > GT_MAX_GAMEPADS)
        return {{"error", "Invalid player number (must be 1-2)"}};

    int type = (int)emu_get_pad_type((GT_Controllers)(player - 1));
    const char* name = type >= 0 && type < k_controller_type_count ? k_controller_type_names[type] : "unknown";
    return {{"success", true}, {"player", player}, {"type", name}};
}

bool DebugAdapter::IsMouseController(int player) const
{
    if (player < 1 || player > GT_MAX_GAMEPADS)
        return false;

    return emu_get_pad_type((GT_Controllers)(player - 1)) == GT_CONTROLLER_MOUSE;
}

bool DebugAdapter::ApplyMouseMotion(int player, int delta_x, int delta_y)
{
    if (!IsMouseController(player))
        return false;

    if (delta_x != 0 || delta_y != 0)
        emu_set_mouse_delta((GT_Controllers)(player - 1), delta_x, delta_y);

    return true;
}

bool DebugAdapter::GetKeyCode(const std::string& name, GT_Keys& key) const
{
    std::string upper = to_upper(name);

    if (upper == "ENTER")
        upper = "RETURN";
    else if (upper == "ESC")
        upper = "ESCAPE";

    for (int i = 0; i < k_debug_key_name_count; i++)
    {
        if (upper == k_debug_key_names[i].name)
        {
            key = k_debug_key_names[i].key;
            return true;
        }
    }

    return false;
}

bool DebugAdapter::GetTypedKey(char character, GT_Keys& key, bool& shift) const
{
    static const char* k_rows[3] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
    static const GT_Keys k_row_start[3] = { GT_KEY_Q, GT_KEY_A, GT_KEY_Z };
    static const char* k_shifted_digits = "!\"#$%&'()";
    static const char k_symbols[] = "-^\\@[;:],./ ";
    static const GT_Keys k_symbol_keys[] =
    {
        GT_KEY_MINUS, GT_KEY_CARET, GT_KEY_YEN, GT_KEY_AT, GT_KEY_LEFT_BRACKET, GT_KEY_SEMICOLON, GT_KEY_COLON,
        GT_KEY_RIGHT_BRACKET, GT_KEY_COMMA, GT_KEY_PERIOD, GT_KEY_SLASH, GT_KEY_SPACE
    };
    static const char k_shifted_symbols[] = "=~|`{+*}<>?_";
    static const GT_Keys k_shifted_symbol_keys[] =
    {
        GT_KEY_MINUS, GT_KEY_CARET, GT_KEY_YEN, GT_KEY_AT, GT_KEY_LEFT_BRACKET, GT_KEY_SEMICOLON, GT_KEY_COLON,
        GT_KEY_RIGHT_BRACKET, GT_KEY_COMMA, GT_KEY_PERIOD, GT_KEY_SLASH, GT_KEY_UNDERSCORE
    };

    shift = false;

    if (character >= '1' && character <= '9')
    {
        key = (GT_Keys)(GT_KEY_1 + (character - '1'));
        return true;
    }

    if (character == '0')
    {
        key = GT_KEY_0;
        return true;
    }

    if (character == '\n' || character == '\r')
    {
        key = GT_KEY_RETURN;
        return true;
    }

    if (character == '\t' || character == '\b')
    {
        key = character == '\t' ? GT_KEY_TAB : GT_KEY_BACKSPACE;
        return true;
    }

    for (int row = 0; row < 3; row++)
    {
        const char* found = strchr(k_rows[row], toupper((unsigned char)character));

        if (isalpha((unsigned char)character) && IsValidPointer(found))
        {
            key = (GT_Keys)(k_row_start[row] + (found - k_rows[row]));
            shift = isupper((unsigned char)character) != 0;
            return true;
        }
    }

    const char* digit = strchr(k_shifted_digits, character);

    if (character != 0 && IsValidPointer(digit))
    {
        key = (GT_Keys)(GT_KEY_1 + (digit - k_shifted_digits));
        shift = true;
        return true;
    }

    const char* symbol = strchr(k_symbols, character);

    if (character != 0 && IsValidPointer(symbol))
    {
        key = k_symbol_keys[symbol - k_symbols];
        return true;
    }

    const char* shifted = strchr(k_shifted_symbols, character);

    if (character != 0 && IsValidPointer(shifted))
    {
        key = k_shifted_symbol_keys[shifted - k_shifted_symbols];
        shift = true;
        return true;
    }

    return false;
}

json DebugAdapter::KeyboardKey(const std::string& key, const std::string& action)
{
    GT_Keys code = GT_KEY_NONE;

    if (!GetKeyCode(key, code))
        return {{"error", "Invalid key name '" + key + "'"}};

    bool delayed_release = false;

    if (action == "press")
        emu_key_pressed(code);
    else if (action == "release")
        emu_key_released(code);
    else if (action == "press_and_release")
    {
        emu_key_pressed(code);
        delayed_release = true;
    }
    else
        return {{"error", "Invalid action"}};

    json result = {{"success", true}, {"key", gui_debug_key_name(code)}, {"action", action}};

    if (delayed_release)
        result["__delayed_key_release"] = true;

    return result;
}

json DebugAdapter::GetInputState()
{
    static const char* names[] = {
        "up", "down", "left", "right", "select", "run",
        "A", "B", "C", "X", "Y", "Z", "zoom"
    };
    static const u16 masks[] = {
        GT_GAMEPAD_UP, GT_GAMEPAD_DOWN, GT_GAMEPAD_LEFT, GT_GAMEPAD_RIGHT,
        GT_GAMEPAD_SELECT, GT_GAMEPAD_RUN, GT_GAMEPAD_A, GT_GAMEPAD_B,
        GT_GAMEPAD_C, GT_GAMEPAD_X, GT_GAMEPAD_Y, GT_GAMEPAD_Z, GT_GAMEPAD_ZOOM
    };

    json players = json::array();

    for (int player = 0; player < GT_MAX_GAMEPADS; player++)
    {
        json pressed = json::array();
        u16 buttons = m_core->GetInput()->GetGamePadState(player).buttons;

        for (size_t i = 0; i < sizeof(masks) / sizeof(masks[0]); i++)
        {
            if (buttons & masks[i])
                pressed.push_back(names[i]);
        }

        int type = (int)emu_get_pad_type((GT_Controllers)player);
        const char* name = type >= 0 && type < k_controller_type_count ? k_controller_type_names[type] : "unknown";
        players.push_back({{"player", player + 1}, {"type", name}, {"pressed", pressed}});
    }

    json keys = json::array();
    Keyboard::Keyboard_State* keyboard = m_core->GetKeyboard()->GetState();

    for (int i = 0; i < GT_KEY_COUNT; i++)
    {
        if (keyboard->keys[i] && IsValidPointer(gui_debug_key_name(i)))
            keys.push_back(gui_debug_key_name(i));
    }

    return {{"players", players}, {"keys_held", keys}};
}

json DebugAdapter::GetRewindStatus()
{
    json result;
    result["enabled"] = config_rewind.enabled;
    result["snapshot_count"] = rewind_get_snapshot_count();
    result["capacity"] = rewind_get_capacity();
    result["frames_per_snapshot"] = rewind_get_frames_per_snapshot();
    result["buffer_seconds"] = config_rewind.buffer_seconds;
    result["memory_usage"] = rewind_get_memory_usage();

    int fps = rewind_get_frames_per_snapshot();
    if (fps < 1) fps = 1;
    result["buffered_seconds"] = (double)(rewind_get_snapshot_count() * fps) / 60.0;

    return result;
}

json DebugAdapter::RewindSeek(int snapshot)
{
    bool paused = emu_is_paused() || emu_is_debug_idle();

    if (!paused)
        return {{"error", "Pause the emulator before seeking the rewind buffer"}};

    int count = rewind_get_snapshot_count();

    if (count == 0)
        return {{"error", "No rewind snapshots available"}};

    if (snapshot < 1 || snapshot > count)
        return {{"error", "Snapshot out of range (1-" + std::to_string(count) + ")"}};

    int age = count - snapshot;

    if (!gui_debug_rewind_seek(age))
        return {{"error", "Failed to load snapshot"}};

    I386* cpu = m_core->GetI386();
    I386_Debug_State state;
    cpu->CopyDebugState(state);

    int fps = rewind_get_frames_per_snapshot();
    if (fps < 1) fps = 1;

    json result;
    result["success"] = true;
    result["snapshot"] = snapshot;
    result["total"] = count;
    result["age_seconds"] = (double)(age * fps) / 60.0;
    result["cs"] = Hex(state.segment[I386_SEGMENT_CS].selector, 4);
    result["eip"] = Hex32(state.eip);
    result["linear_pc"] = Hex32(cpu->GetCurrentLinearPC());
    return result;
}

void DebugAdapter::ApplyControllerState(int player)
{
    GT_GamePad_State state = {m_buttons[player - 1]};
    m_core->GetInput()->SetInjectedGamePadState(player - 1, state);
}

void DebugAdapter::ClearControllerState()
{
    for (int player = 0; player < GT_MAX_GAMEPADS; player++)
    {
        m_buttons[player] = 0;
        GT_GamePad_State state = {0};
        m_core->GetInput()->SetInjectedGamePadState(player, state);
    }
}

std::string DebugAdapter::Hex(u32 value, int digits) const
{
    std::ostringstream stream;

    stream << std::hex << std::uppercase << std::setfill('0') << std::setw(digits) << value;
    return stream.str();
}

std::string DebugAdapter::Hex32(u32 value) const
{
    return Hex(value, 8);
}

std::string DebugAdapter::FormatAreaOffset(const GuiDebugMemoryArea& area, u32 offset) const
{
    int digits = 1;
    u64 last = area.size > 0 ? area.size - 1 : 0;

    while (last >>= 4)
        digits++;

    return Hex(offset, MAX(digits, 4));
}

bool DebugAdapter::ParseAreaOffset(const GuiDebugMemoryArea& area, const std::string& text, u32& offset,
    std::string& error)
{
    if (area.source.space == GT_DEBUG_MEMORY_LINEAR)
    {
        McpAddress address;

        if (!ParseAddress(text, address, error))
            return false;

        offset = address.linear;
        return true;
    }

    if (!parse_hex_text(text, offset))
    {
        error = "Invalid offset format '" + text + "'";
        return false;
    }

    return true;
}

bool DebugAdapter::GetAreaRange(const GuiDebugMemoryArea& area, const json& arguments, u32& start, u32& size,
    std::string& error)
{
    const u64 max_size = 0x04000000;
    start = 0;

    if (arguments.contains("start"))
    {
        if (!arguments["start"].is_string() || !ParseAreaOffset(area, arguments["start"].get<std::string>(), start, error))
        {
            if (error.empty())
                error = "start must be a hex string";

            return false;
        }
    }

    u64 range = area.size - MIN((u64)start, area.size);

    if (arguments.contains("size"))
    {
        u32 requested = 0;

        if (!arguments["size"].is_string() || !parse_hex_text(arguments["size"].get<std::string>(), requested))
        {
            error = "size must be a hex string";
            return false;
        }

        range = MIN((u64)requested, range);
    }

    if (range == 0 || range > max_size)
    {
        error = "Give start and size (hex, at most 4000000) for areas larger than 64 MB";
        return false;
    }

    size = (u32)range;
    return true;
}

std::string DebugAdapter::FormatLogical(u16 selector, u32 offset, u32 linear)
{
    I386_Disassembler_Record* record = m_core->GetI386()->GetDisassemblerRecord(linear);
    const I386_Segment& cs = m_core->GetI386()->GetState()->segments[I386_SEGMENT_CS];
    bool code32 = true;

    if (IsValidPointer(record) && record->size > 0)
        code32 = record->default32;
    else if (selector == cs.selector)
        code32 = (cs.attributes & I386_SEGMENT_DEFAULT_32) != 0;

    return Hex(selector, 4) + ":" + Hex(offset, code32 ? 8 : 4);
}

const char* DebugAdapter::GetSymbolAt(u32 linear)
{
    return gui_debug_get_symbol(linear);
}
