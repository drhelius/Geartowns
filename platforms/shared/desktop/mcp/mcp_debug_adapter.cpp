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
#include "../utils.h"

DebugAdapter::DebugAdapter(GeartownsCore* core)
{
    m_core = core;
    memset(m_buttons, 0, sizeof(m_buttons));
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
        {"debug", config_debug.debug},
        {"debug_idle", emu_is_debug_idle()},
        {"media_loading", emu_is_media_loading()},
        {"media_ready", m_core && m_core->GetMedia()->IsReady()},
        {"bios_ready", m_core && m_core->GetFirmware()->IsReady()},
        {"firmware_ready", m_core && m_core->GetFirmware()->IsReady()},
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

json DebugAdapter::ControllerButton(int player, const std::string& button,
    const std::string& action)
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
