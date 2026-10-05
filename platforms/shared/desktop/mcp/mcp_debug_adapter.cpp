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
#include <iomanip>
#include <sstream>
#include <stdlib.h>
#include "mcp_debug_adapter.h"
#include "common/log.h"
#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../utils.h"

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
    json result = {
        {"paused", emu_is_paused()},
        {"debug", config_debug.debug},
        {"debug_idle", emu_is_debug_idle()},
        {"media_loading", emu_is_media_loading()},
        {"media_ready", m_core && m_core->GetMedia()->IsReady()},
        {"bios_ready", m_core && m_core->GetFirmware()->IsReady()},
        {"firmware_ready", m_core && m_core->GetFirmware()->IsReady()},
        {"powered_on", m_core && m_core->IsPoweredOn()},
        {"frame", emu_frame_counter}
    };

    if (IsValidPointer(m_core) && IsValidPointer(m_core->GetI386()))
    {
        I386* cpu = m_core->GetI386();
        I386_Debug_State state;
        u32 breakpoint_address = 0;

        cpu->CopyDebugState(state);

        result["halted"] = cpu->Halted();
        result["shutdown"] = cpu->Shutdown();
        result["cs"] = Hex32(state.segment[I386_SEGMENT_CS].selector).substr(4);
        result["eip"] = Hex32(state.eip);
        result["linear_pc"] = Hex32(cpu->GetCurrentLinearPC());
        result["eflags"] = Hex32(state.eflags);
        result["call_stack_depth"] = cpu->GetDisassemblerCallStack().size();
        result["breakpoint_hit"] = cpu->GetBreakpointHitAddress(breakpoint_address);
        result["run_to_hit"] = cpu->RunToBreakpointHit();

        if (result["breakpoint_hit"].get<bool>())
            result["breakpoint_address"] = Hex32(breakpoint_address);

        result["registers"] = {
            {"eax", Hex32(state.eax)}, {"ecx", Hex32(state.ecx)},
            {"edx", Hex32(state.edx)}, {"ebx", Hex32(state.ebx)},
            {"esp", Hex32(state.esp)}, {"ebp", Hex32(state.ebp)},
            {"esi", Hex32(state.esi)}, {"edi", Hex32(state.edi)}
        };
    }

    return result;
}

json DebugAdapter::SetBreakpoint(u32 address)
{
    m_core->GetI386()->AddBreakpoint(address);
    return {{"success", true}, {"address", Hex32(address)}};
}

json DebugAdapter::SetBreakpointRange(u32 start_address, u32 end_address)
{
    m_core->GetI386()->AddBreakpoint(start_address, end_address);
    return {
        {"success", true},
        {"start_address", Hex32(start_address)},
        {"end_address", Hex32(end_address)}
    };
}

json DebugAdapter::RemoveBreakpoint(u32 address, u32 end_address)
{
    m_core->GetI386()->RemoveBreakpoint(address, end_address);
    return {{"success", true}, {"address", Hex32(address)}};
}

json DebugAdapter::ListBreakpoints()
{
    json breakpoints = json::array();
    std::vector<I386_Breakpoint>* items = m_core->GetI386()->GetBreakpoints();

    for (size_t i = 0; i < items->size(); i++)
    {
        const I386_Breakpoint& item = (*items)[i];
        json breakpoint = {
            {"enabled", item.enabled},
            {"address", Hex32(item.address1)},
            {"execute", true},
            {"range", item.range}
        };

        if (item.range)
            breakpoint["end_address"] = Hex32(item.address2);

        breakpoints.push_back(breakpoint);
    }

    return {{"breakpoints", breakpoints}, {"count", breakpoints.size()}};
}

json DebugAdapter::GetDisassembly(u32 start_address, u32 end_address, bool resolve_symbols, bool detailed)
{
    I386* cpu = m_core->GetI386();
    I386_Debug_State state;
    cpu->CopyDebugState(state);

    u32 cs_base = state.segment[I386_SEGMENT_CS].base;
    u32 cs_limit = state.segment[I386_SEGMENT_CS].limit;
    u32 address = start_address;
    json lines = json::array();

    while (address <= end_address)
    {
        u32 eip = address - cs_base;
        bool current_context = eip <= cs_limit;
        I386_Disassembler_Record* record =
            current_context ? cpu->Disassemble(eip) : cpu->GetDisassemblerRecord(address);

        if (!IsValidPointer(record) || record->name[0] == 0 || record->linear != address)
        {
            if (address == 0xFFFFFFFFU)
                break;

            address++;
            continue;
        }

        json line = {
            {"address", Hex32(record->linear)},
            {"logical", Hex32(record->eip)},
            {"segment", record->segment},
            {"instruction", record->name},
            {"size", record->size},
            {"historical", !current_context}
        };

        if (record->auto_symbol[0] != 0)
            line["symbol"] = record->auto_symbol;

        if (detailed)
        {
            line["bytes"] = record->bytes;
            line["jump"] = record->jump;
            line["subroutine"] = record->subroutine;
            line["returns"] = record->returns;

            if (record->jump && record->jump_target_known)
            {
                line["jump_address"] = Hex32(record->jump_linear);

                if (resolve_symbols)
                {
                    I386_Disassembler_Record* target = cpu->GetDisassemblerRecord(record->jump_linear);

                    if (IsValidPointer(target) && target->auto_symbol[0] != 0)
                        line["jump_symbol"] = target->auto_symbol;
                }
            }
        }

        lines.push_back(line);

        u32 next = address + (u32)record->size;

        if (next <= address)
            break;

        address = next;
    }

    return {
        {"start_address", Hex32(start_address)},
        {"end_address", Hex32(end_address)},
        {"lines", lines},
        {"count", lines.size()}
    };
}

json DebugAdapter::ListSymbols()
{
    json symbols = json::array();
    const std::map<u32, I386_Disassembler_Record>& records = m_core->GetI386()->GetDisassemblerRecords();
    std::map<u32, I386_Disassembler_Record>::const_iterator record;

    for (record = records.begin(); record != records.end(); record++)
    {
        if (record->second.auto_symbol[0] == 0)
            continue;

        symbols.push_back({
            {"address", Hex32(record->first)},
            {"name", record->second.auto_symbol},
            {"type", "automatic"}
        });
    }

    return {{"symbols", symbols}, {"count", symbols.size()}};
}

json DebugAdapter::LookupSymbolByName(const std::string& name)
{
    json matches = json::array();
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

    if (IsValidPointer(record) && record->auto_symbol[0] != 0)
    {
        result["found"] = true;
        result["name"] = record->auto_symbol;
        result["type"] = "automatic";
    }

    return result;
}

json DebugAdapter::ListCallStack()
{
    I386* cpu = m_core->GetI386();
    const std::vector<I386_CallStackEntry>& entries = cpu->GetDisassemblerCallStack();
    json stack = json::array();

    for (int i = (int)entries.size() - 1; i >= 0; i--)
    {
        const I386_CallStackEntry* entry = &entries[i];
        I386_Disassembler_Record* record = cpu->GetDisassemblerRecord(entry->dest_linear);
        json item = {
            {"function", Hex32(entry->dest_linear)},
            {"source", Hex32(entry->src_linear)},
            {"return", Hex32(entry->back_linear)},
            {"function_logical", Hex32(entry->dest)},
            {"source_logical", Hex32(entry->src)},
            {"return_logical", Hex32(entry->back)},
            {"interrupt", entry->interrupt}
        };

        if (IsValidPointer(record) && record->auto_symbol[0] != 0)
            item["symbol"] = record->auto_symbol;

        stack.push_back(item);
    }

    return {{"stack", stack}, {"depth", stack.size()}};
}

std::string DebugAdapter::Hex32(u32 value) const
{
    std::ostringstream stream;

    stream << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << value;
    return stream.str();
}

json DebugAdapter::GetScreenshot()
{
    json result;

    if (!m_core || !m_core->GetMedia()->IsReady())
    {
        result["error"] = "No media loaded";
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

u16 DebugAdapter::ButtonMask(const std::string& button) const
{
    std::string name = button;
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);

    if (name == "up") return GT_GAMEPAD_UP;
    if (name == "down") return GT_GAMEPAD_DOWN;
    if (name == "left") return GT_GAMEPAD_LEFT;
    if (name == "right") return GT_GAMEPAD_RIGHT;
    if (name == "start") return GT_GAMEPAD_START;
    if (name == "run") return GT_GAMEPAD_RUN;
    if (name == "a") return GT_GAMEPAD_A;
    if (name == "b") return GT_GAMEPAD_B;
    if (name == "c") return GT_GAMEPAD_C;
    if (name == "x") return GT_GAMEPAD_X;
    if (name == "y") return GT_GAMEPAD_Y;
    if (name == "z") return GT_GAMEPAD_Z;

    return 0;
}

json DebugAdapter::ControllerButton(int player, const std::string& button, const std::string& action)
{
    if (player < 1 || player > GT_MAX_GAMEPADS)
        return {{"error", "Invalid player number"}};

    u16 mask = ButtonMask(button);

    if (mask == 0)
        return {{"error", "Invalid button name"}};

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

json DebugAdapter::GetInputState()
{
    static const char* names[] = {
        "up", "down", "left", "right", "start", "run",
        "A", "B", "C", "X", "Y", "Z"
    };
    static const u16 masks[] = {
        GT_GAMEPAD_UP, GT_GAMEPAD_DOWN, GT_GAMEPAD_LEFT, GT_GAMEPAD_RIGHT,
        GT_GAMEPAD_START, GT_GAMEPAD_RUN, GT_GAMEPAD_A, GT_GAMEPAD_B,
        GT_GAMEPAD_C, GT_GAMEPAD_X, GT_GAMEPAD_Y, GT_GAMEPAD_Z
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

        players.push_back({{"player", player + 1}, {"pressed", pressed}});
    }

    return {{"players", players}};
}

void DebugAdapter::ApplyControllerState(int player)
{
    GT_GamePad_State state = {m_buttons[player - 1], 0, 0};
    m_core->GetInput()->SetInjectedGamePadState(player - 1, state);
}

void DebugAdapter::ClearControllerState()
{
    for (int player = 0; player < GT_MAX_GAMEPADS; player++)
    {
        m_buttons[player] = 0;
        GT_GamePad_State state = {0, 0, 0};
        m_core->GetInput()->SetInjectedGamePadState(player, state);
    }
}
