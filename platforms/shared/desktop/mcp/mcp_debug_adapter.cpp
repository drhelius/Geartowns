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
#include <stdlib.h>
#include "mcp_debug_adapter.h"
#include "log.h"
#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../gui_actions.h"
#include "../utils.h"

DebugAdapter::DebugAdapter(GeartownsCore* core)
{
    m_core = core;
    for (int i = 0; i < GT_MAX_GAMEPADS; i++)
        m_buttons[i] = 0;
}

void DebugAdapter::Pause()
{
    emu_pause();
}

void DebugAdapter::Resume()
{
    emu_resume();
}

void DebugAdapter::Reset()
{
    emu_reset();
    ClearControllerState();
}

json DebugAdapter::GetDebugStatus()
{
    return {
        {"paused", emu_is_paused()},
        {"media_loading", emu_is_media_loading()},
        {"media_ready", m_core && m_core->GetMedia()->IsReady()},
        {"frame", emu_frame_counter}
    };
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
    return {
        {"emulator", GT_TITLE},
        {"emulator_version", GT_VERSION},
        {"ready", media->IsReady()},
        {"bios_ready", media->IsBiosReady()},
        {"file_path", media->GetFilePath()},
        {"file_name", media->GetFileName()},
        {"file_directory", media->GetFileDirectory()},
        {"file_extension", media->GetFileExtension()}
    };
}

json DebugAdapter::ListRecentMedia()
{
    json entries = json::array();

    for (int i = 0; i < config_max_recent_roms; i++)
    {
        if (!config_emulator.recent_roms[i].empty())
        {
            const std::string& path = config_emulator.recent_roms[i];
            entries.push_back({{"index", i}, {"file_path", path}, {"file_name", get_filename(path.c_str())}});
        }
    }

    return {{"count", entries.size()}, {"recent_media", entries}};
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

json DebugAdapter::LoadBios(const std::string& file_path)
{
    if (file_path.empty())
        return {{"error", "File path is required"}};

    if (!emu_load_bios(file_path.c_str()))
        return {{"error", "Failed to load BIOS"}};

    return {{"success", true}, {"file_path", file_path}};
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
    json result = {{"success", true}, {"player", player}, {"button", button}, {"action", action}};
    if (delayed_release)
        result["__delayed_release"] = true;
    return result;
}

json DebugAdapter::GetInputState()
{
    static const char* names[] = {"up", "down", "left", "right", "start", "run", "A", "B", "C", "X", "Y", "Z"};
    static const u16 masks[] = {
        GT_GAMEPAD_UP, GT_GAMEPAD_DOWN, GT_GAMEPAD_LEFT, GT_GAMEPAD_RIGHT,
        GT_GAMEPAD_START, GT_GAMEPAD_RUN, GT_GAMEPAD_A, GT_GAMEPAD_B,
        GT_GAMEPAD_C, GT_GAMEPAD_X, GT_GAMEPAD_Y, GT_GAMEPAD_Z
    };

    json players = json::array();
    for (int player = 0; player < GT_MAX_GAMEPADS; player++)
    {
        json pressed = json::array();
        for (size_t i = 0; i < sizeof(masks) / sizeof(masks[0]); i++)
        {
            u16 effective_buttons = m_core->GetInput()->GetGamePadState(player).buttons | m_buttons[player];
            if (effective_buttons & masks[i])
                pressed.push_back(names[i]);
        }
        players.push_back({{"player", player + 1}, {"pressed", pressed}});
    }

    return {{"players", players}};
}

json DebugAdapter::ControllerSetType(int player, const std::string& type)
{
    if (player < 1 || player > GT_MAX_GAMEPADS)
        return {{"error", "Invalid player number"}};

    GT_Controller_Type controller_type = GT_CONTROLLER_NONE;
    if (type == "original")
        controller_type = GT_CONTROLLER_ORIGINAL_GAMEPAD;
    else if (type == "6_button")
        controller_type = GT_CONTROLLER_6_BUTTON_GAMEPAD;
    else if (type != "none")
        return {{"error", "Invalid controller type"}};

    emu_set_pad_type(player - 1, controller_type);
    config_input.controller_type[player - 1] = controller_type;
    return {{"success", true}, {"player", player}, {"type", type}};
}

json DebugAdapter::ControllerGetType(int player)
{
    if (player < 1 || player > GT_MAX_GAMEPADS)
        return {{"error", "Invalid player number"}};

    GT_Controller_Type type = emu_get_pad_type(player - 1);
    return {{"success", true}, {"player", player}, {"type", ControllerTypeName(type)}};
}

const char* DebugAdapter::ControllerTypeName(GT_Controller_Type type) const
{
    if (type == GT_CONTROLLER_ORIGINAL_GAMEPAD)
        return "original";
    if (type == GT_CONTROLLER_6_BUTTON_GAMEPAD)
        return "6_button";
    return "none";
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
