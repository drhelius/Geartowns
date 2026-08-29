/*
 * Geartowns - FM Towns Emulator
 * Copyright (C) 2026  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#include "config_macros.h"
#include "shader_preset.h"

static inline void process(config_Operation operation)
{
    //**************************************
    // Debug
    //**************************************

    // Debugger
    CONFIG_BOOL("Debug", "Debug", config_debug.debug, false);
    CONFIG_BOOL("Debug", "Memory", config_debug.show_memory, false);
    CONFIG_BOOL("Debug", "AutoDebugSettings", config_debug.auto_debug_settings, false);

    // Interface
    CONFIG_INT_RANGE("Debug", "FontSize", config_debug.font_size, 0, 0, 3);
    CONFIG_BOOL("Debug", "MultiViewport", config_debug.multi_viewport, false);
    CONFIG_BOOL("Debug", "SingleInstance", config_debug.single_instance, false);

    //**************************************
    // Emulator
    //**************************************

    // Window and interface
    CONFIG_BOOL("Emulator", "Maximized", config_emulator.maximized, false);
    CONFIG_BOOL("Emulator", "FullScreen", config_emulator.fullscreen, false);
    CONFIG_INT("Emulator", "FullScreenMode", config_emulator.fullscreen_mode, 0);
    CONFIG_BOOL("Emulator", "AlwaysShowMenu", config_emulator.always_show_menu, false);
    CONFIG_INT_RANGE("Emulator", "Theme", config_emulator.theme, config_Theme_Dark, config_Theme_Light, config_Theme_Dark);
    CONFIG_INT("Emulator", "WindowWidth", config_emulator.window_width,
               operation == config_Operation_Defaults ? 800 : 770);
    CONFIG_INT("Emulator", "WindowHeight", config_emulator.window_height,
               operation == config_Operation_Defaults ? 640 : 600);
    CONFIG_BOOL("Emulator", "StatusMessages", config_emulator.status_messages, false);
    CONFIG_BOOL("Emulator", "AllowScreenSaver", config_emulator.allow_screensaver, false);

    // Emulation
    CONFIG_INT("Emulator", "FFWD", config_emulator.ffwd_speed, 1);
    CONFIG_INT_RANGE("Emulator", "RunAhead", config_emulator.runahead, 0, 0, 3);
    CONFIG_INT_RANGE("Emulator", "SaveSlot", config_emulator.save_slot, 0, 0, 4);
    CONFIG_BOOL("Emulator", "StartPaused", config_emulator.start_paused, false);
    CONFIG_BOOL("Emulator", "PauseWhenInactive", config_emulator.pause_when_inactive, true);

    // Files and paths
    CONFIG_INT("Emulator", "SaveStatesDirOption", config_emulator.savestates_dir_option, 0);
    CONFIG_STRING_NOT_EMPTY("Emulator", "SaveStatesPath", config_emulator.savestates_path, config_root_path);
    CONFIG_INT("Emulator", "ScreenshotDirOption", config_emulator.screenshots_dir_option, 0);
    CONFIG_STRING_NOT_EMPTY("Emulator", "ScreenshotPath", config_emulator.screenshots_path, config_root_path);
    CONFIG_STRING("Emulator", "LastOpenPath", config_emulator.last_open_path, "");
    CONFIG_STRING("Emulator", "BiosPath", config_emulator.bios_path, "");
    CONFIG_STRING_ARRAY("Emulator", "RecentROM%d", config_emulator.recent_roms, config_max_recent_roms, "");

    // Services
    CONFIG_INT("Emulator", "MCPTCPPort", config_emulator.mcp_tcp_port, 7777);
    CONFIG_STRING_NOT_EMPTY("Emulator", "MCPHTTPAddress", config_emulator.mcp_http_address, "127.0.0.1");

    //**************************************
    // Video
    //**************************************

    // Display
    CONFIG_INT("Video", "Scale", config_video.scale, 0);
    CONFIG_INT_RANGE("Video", "ScaleManual", config_video.scale_manual, 1, 1, 20);
    CONFIG_INT_RANGE("Video", "AspectRatio", config_video.ratio, 1, 0, 2);
    CONFIG_BOOL("Video", "FPS", config_video.fps, false);
    CONFIG_INT_RANGE("Video", "ShaderMode", config_video.shader_mode, config_ShaderMode_PixelPerfect, config_ShaderMode_PixelPerfect, config_ShaderMode_External);

    if (operation == config_Operation_Write)
    {
        std::string preset_file = get_filename(config_video.shader_preset_path.c_str());
        CONFIG_STRING("Video", "ShaderPresetFile", preset_file, "");
    }
    else
    {
        CONFIG_STRING("Video", "ShaderPresetFile", config_video.shader_preset_path, "");
    }

    CONFIG_INT_RANGE("Video", "SyncMode", config_video.sync_mode, config_VideoSync_Fixed, config_VideoSync_Disabled, config_VideoSync_VRR);

    // Background colors
    CONFIG_FLOAT("Video", "BackgroundColorR", config_video.background_color[config_Theme_Dark][0], 0.1f);
    CONFIG_FLOAT("Video", "BackgroundColorG", config_video.background_color[config_Theme_Dark][1], 0.1f);
    CONFIG_FLOAT("Video", "BackgroundColorB", config_video.background_color[config_Theme_Dark][2], 0.1f);
    CONFIG_FLOAT("Video", "BackgroundColorDebuggerR", config_video.background_color_debugger[config_Theme_Dark][0], 0.2f);
    CONFIG_FLOAT("Video", "BackgroundColorDebuggerG", config_video.background_color_debugger[config_Theme_Dark][1], 0.2f);
    CONFIG_FLOAT("Video", "BackgroundColorDebuggerB", config_video.background_color_debugger[config_Theme_Dark][2], 0.2f);
    CONFIG_FLOAT("Video", "BackgroundColorLightR", config_video.background_color[config_Theme_Light][0], 128.0f / 255.0f);
    CONFIG_FLOAT("Video", "BackgroundColorLightG", config_video.background_color[config_Theme_Light][1], 128.0f / 255.0f);
    CONFIG_FLOAT("Video", "BackgroundColorLightB", config_video.background_color[config_Theme_Light][2], 128.0f / 255.0f);
    CONFIG_FLOAT("Video", "BackgroundColorDebuggerLightR",
                 config_video.background_color_debugger[config_Theme_Light][0],
                 233.0f / 255.0f);
    CONFIG_FLOAT("Video", "BackgroundColorDebuggerLightG",
                 config_video.background_color_debugger[config_Theme_Light][1],
                 232.0f / 255.0f);
    CONFIG_FLOAT("Video", "BackgroundColorDebuggerLightB",
                 config_video.background_color_debugger[config_Theme_Light][2],
                 230.0f / 255.0f);

    //**************************************
    // Audio
    //**************************************

    CONFIG_BOOL("Audio", "Enable", config_audio.enable, true);
    CONFIG_BOOL("Audio", "Sync", config_audio.sync, true);
    CONFIG_FLOAT_RANGE("Audio", "MasterVolume", config_audio.master_volume, 1.0f, 0.0f, 2.0f);
    CONFIG_INT("Audio", "BufferCount", config_audio.buffer_count, 3);

    //**************************************
    // Rewind
    //**************************************

    CONFIG_BOOL("Rewind", "Enabled", config_rewind.enabled, true);
    CONFIG_INT_RANGE("Rewind", "BufferSeconds", config_rewind.buffer_seconds, 10, 1, 10);
    CONFIG_INT_MIN("Rewind", "FramesPerSnapshot", config_rewind.frames_per_snapshot, 1, 1);
    CONFIG_FLOAT_RANGE("Rewind", "Speed", config_rewind.speed, 2.0f, 1.0f, 8.0f);

    //**************************************
    // Input
    //**************************************

    CONFIG_BOOL("Input", "AllowUpDown", config_input.allow_up_down, false);
    CONFIG_BOOL("Input", "CaptureMouse", config_emulator.capture_mouse, false);
    CONFIG_INT_RANGE("Input", "MouseSensitivity", config_emulator.mouse_sensitivity, 5, 1, 15);

    // Players
    for (int i = 0; i < GT_MAX_GAMEPADS; i++)
    {
        char section[32];
        snprintf(section, sizeof(section), "Input%d", i + 1);
        CONFIG_INT_RANGE(section, "ControllerType", config_input.controller_type[i], GT_CONTROLLER_ORIGINAL_GAMEPAD, GT_CONTROLLER_NONE, GT_CONTROLLER_6_BUTTON_GAMEPAD);
    }

    // Keyboard
    const SDL_Scancode keyboard_defaults[GT_MAX_GAMEPADS][12] = {
        { SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
          SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_X, SDL_SCANCODE_Z,
          SDL_SCANCODE_C, SDL_SCANCODE_V, SDL_SCANCODE_B, SDL_SCANCODE_N },
        { SDL_SCANCODE_J, SDL_SCANCODE_L, SDL_SCANCODE_I, SDL_SCANCODE_K,
          SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_Y, SDL_SCANCODE_T,
          SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8 }
    };

    for (int i = 0; i < GT_MAX_GAMEPADS; i++)
    {
        char section[32];
        snprintf(section, sizeof(section), "InputKeyboard%d", i + 1);
        CONFIG_SCANCODE(section, "KeyLeft", config_input_keyboard[i].key_left, keyboard_defaults[i][0]);
        CONFIG_SCANCODE(section, "KeyRight", config_input_keyboard[i].key_right, keyboard_defaults[i][1]);
        CONFIG_SCANCODE(section, "KeyUp", config_input_keyboard[i].key_up, keyboard_defaults[i][2]);
        CONFIG_SCANCODE(section, "KeyDown", config_input_keyboard[i].key_down, keyboard_defaults[i][3]);
        CONFIG_SCANCODE(section, "KeyStart", config_input_keyboard[i].key_start, keyboard_defaults[i][4]);
        CONFIG_SCANCODE(section, "KeyRun", config_input_keyboard[i].key_run, keyboard_defaults[i][5]);
        CONFIG_SCANCODE(section, "KeyA", config_input_keyboard[i].key_A, keyboard_defaults[i][6]);
        CONFIG_SCANCODE(section, "KeyB", config_input_keyboard[i].key_B, keyboard_defaults[i][7]);
        CONFIG_SCANCODE(section, "KeyC", config_input_keyboard[i].key_C, keyboard_defaults[i][8]);
        CONFIG_SCANCODE(section, "KeyX", config_input_keyboard[i].key_X, keyboard_defaults[i][9]);
        CONFIG_SCANCODE(section, "KeyY", config_input_keyboard[i].key_Y, keyboard_defaults[i][10]);
        CONFIG_SCANCODE(section, "KeyZ", config_input_keyboard[i].key_Z, keyboard_defaults[i][11]);
    }

    // Gamepads
    for (int i = 0; i < GT_MAX_GAMEPADS; i++)
    {
        char section[32];
        snprintf(section, sizeof(section), "InputGamepad%d", i + 1);
        CONFIG_INT(section, "GamepadDirectional", config_input_gamepad[i].gamepad_directional, 0);
        CONFIG_BOOL(section, "GamepadInvertX", config_input_gamepad[i].gamepad_invert_x_axis, false);
        CONFIG_BOOL(section, "GamepadInvertY", config_input_gamepad[i].gamepad_invert_y_axis, false);
        CONFIG_INT(section, "GamepadStart", config_input_gamepad[i].gamepad_start, SDL_GAMEPAD_BUTTON_START);
        CONFIG_INT(section, "GamepadRun", config_input_gamepad[i].gamepad_run, SDL_GAMEPAD_BUTTON_BACK);
        CONFIG_INT(section, "GamepadXAxis", config_input_gamepad[i].gamepad_x_axis, SDL_GAMEPAD_AXIS_LEFTX);
        CONFIG_INT(section, "GamepadYAxis", config_input_gamepad[i].gamepad_y_axis, SDL_GAMEPAD_AXIS_LEFTY);
        CONFIG_INT(section, "GamepadA", config_input_gamepad[i].gamepad_A, SDL_GAMEPAD_BUTTON_SOUTH);
        CONFIG_INT(section, "GamepadB", config_input_gamepad[i].gamepad_B, SDL_GAMEPAD_BUTTON_EAST);
        CONFIG_INT(section, "GamepadC", config_input_gamepad[i].gamepad_C, SDL_GAMEPAD_BUTTON_WEST);
        CONFIG_INT(section, "GamepadX", config_input_gamepad[i].gamepad_X, SDL_GAMEPAD_BUTTON_NORTH);
        CONFIG_INT(section, "GamepadY", config_input_gamepad[i].gamepad_Y, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        CONFIG_INT(section, "GamepadZ", config_input_gamepad[i].gamepad_Z, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    }

    // Gamepad shortcuts
    for (int i = 0; i < GT_MAX_GAMEPADS; i++)
    {
        char section[32];
        snprintf(section, sizeof(section), "InputGamepadShortcuts%d", i + 1);
        CONFIG_INT_ARRAY(section, "Shortcut%d", config_input_gamepad_shortcuts[i].gamepad_shortcuts, config_HotkeyIndex_COUNT, SDL_GAMEPAD_BUTTON_INVALID);
    }

    // Hotkeys
    CONFIG_HOTKEY("OpenROM", config_hotkeys[config_HotkeyIndex_OpenROM], SDL_SCANCODE_O, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("ReloadROM", config_hotkeys[config_HotkeyIndex_ReloadROM], SDL_SCANCODE_D, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("Quit", config_hotkeys[config_HotkeyIndex_Quit], SDL_SCANCODE_Q, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("Reset", config_hotkeys[config_HotkeyIndex_Reset], SDL_SCANCODE_R, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("Pause", config_hotkeys[config_HotkeyIndex_Pause], SDL_SCANCODE_P, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("FFWD", config_hotkeys[config_HotkeyIndex_FFWD], SDL_SCANCODE_F, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("Rewind", config_hotkeys[config_HotkeyIndex_Rewind], SDL_SCANCODE_BACKSPACE, SDL_KMOD_NONE);
    CONFIG_HOTKEY("SaveState", config_hotkeys[config_HotkeyIndex_SaveState], SDL_SCANCODE_S, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("LoadState", config_hotkeys[config_HotkeyIndex_LoadState], SDL_SCANCODE_L, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("Screenshot", config_hotkeys[config_HotkeyIndex_Screenshot], SDL_SCANCODE_X, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("Fullscreen", config_hotkeys[config_HotkeyIndex_Fullscreen], SDL_SCANCODE_F12, SDL_KMOD_NONE);
    CONFIG_HOTKEY("ShowMainMenu", config_hotkeys[config_HotkeyIndex_ShowMainMenu], SDL_SCANCODE_M, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("DebugStepInto", config_hotkeys[config_HotkeyIndex_DebugStepInto], SDL_SCANCODE_F11, SDL_KMOD_NONE);
    CONFIG_HOTKEY("DebugStepOver", config_hotkeys[config_HotkeyIndex_DebugStepOver], SDL_SCANCODE_F10, SDL_KMOD_NONE);
    CONFIG_HOTKEY("DebugStepOut", config_hotkeys[config_HotkeyIndex_DebugStepOut], SDL_SCANCODE_F11, SDL_KMOD_SHIFT);
    CONFIG_HOTKEY("DebugStepFrame", config_hotkeys[config_HotkeyIndex_DebugStepFrame], SDL_SCANCODE_F6, SDL_KMOD_NONE);
    CONFIG_HOTKEY("DebugContinue", config_hotkeys[config_HotkeyIndex_DebugContinue], SDL_SCANCODE_F5, SDL_KMOD_NONE);
    CONFIG_HOTKEY("DebugBreak", config_hotkeys[config_HotkeyIndex_DebugBreak], SDL_SCANCODE_F7, SDL_KMOD_NONE);
    CONFIG_HOTKEY("DebugRunToCursor", config_hotkeys[config_HotkeyIndex_DebugRunToCursor], SDL_SCANCODE_F8, SDL_KMOD_NONE);
    CONFIG_HOTKEY("DebugBreakpoint", config_hotkeys[config_HotkeyIndex_DebugBreakpoint], SDL_SCANCODE_F9, SDL_KMOD_NONE);
    CONFIG_HOTKEY("DebugGoBack", config_hotkeys[config_HotkeyIndex_DebugGoBack], SDL_SCANCODE_BACKSPACE, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("SelectSlot1", config_hotkeys[config_HotkeyIndex_SelectSlot1], SDL_SCANCODE_1, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("SelectSlot2", config_hotkeys[config_HotkeyIndex_SelectSlot2], SDL_SCANCODE_2, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("SelectSlot3", config_hotkeys[config_HotkeyIndex_SelectSlot3], SDL_SCANCODE_3, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("SelectSlot4", config_hotkeys[config_HotkeyIndex_SelectSlot4], SDL_SCANCODE_4, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("SelectSlot5", config_hotkeys[config_HotkeyIndex_SelectSlot5], SDL_SCANCODE_5, SDL_KMOD_CTRL);
    CONFIG_HOTKEY("Mute", config_hotkeys[config_HotkeyIndex_Mute], SDL_SCANCODE_U, SDL_KMOD_CTRL);
}

//**************************************
// Emulator-specific behavior
//**************************************

static void before_read(int file_version);
static void after_read(int file_version);
static void before_write(void);
static void after_write(void);
static void before_defaults(void);
static void after_defaults(void);
static void normalize(void);
static void migrate(int file_version);
static void sync_shader_preset_parameter_defaults(void);

static void before_read(int file_version)
{
    migrate(file_version);
}

static void after_read(int file_version)
{
    UNUSED(file_version);
    sync_shader_preset_parameter_defaults();
}

static void before_write(void)
{
    if (config_emulator.ffwd)
        config_audio.sync = true;
}

static void after_write(void)
{
    sync_shader_preset_parameter_defaults();
}

static void before_defaults(void)
{
}

static void after_defaults(void)
{
    config_emulator.paused = false;
    config_emulator.ffwd = false;
    config_emulator.show_info = false;
    config_hotkeys[config_HotkeyIndex_CaptureMouse].key = SDL_SCANCODE_F1;
    config_hotkeys[config_HotkeyIndex_CaptureMouse].mod = SDL_KMOD_NONE;
    config_update_hotkey_string(&config_hotkeys[config_HotkeyIndex_CaptureMouse]);
}

static void normalize(void)
{
#if !defined(_WIN32)
    if (config_video.sync_mode == config_VideoSync_VRR)
        config_video.sync_mode = config_VideoSync_Fixed;
#endif
}

static void migrate(int file_version)
{
    std::string stored;

    if (file_version < 2)
    {
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        bool old_default = get_setting("Video", "BackgroundColorDebuggerLightR", &stored) &&
                           parse_float_string(stored, &red) &&
                           get_setting("Video", "BackgroundColorDebuggerLightG", &stored) &&
                           parse_float_string(stored, &green) &&
                           get_setting("Video", "BackgroundColorDebuggerLightB", &stored) &&
                           parse_float_string(stored, &blue) &&
                           std::fabs(red - (160.0f / 255.0f)) < 0.005f &&
                           std::fabs(green - (160.0f / 255.0f)) < 0.005f &&
                           std::fabs(blue - (160.0f / 255.0f)) < 0.005f;

        if (old_default)
        {
            write_float("Video", "BackgroundColorDebuggerLightR", 233.0f / 255.0f);
            write_float("Video", "BackgroundColorDebuggerLightG", 232.0f / 255.0f);
            write_float("Video", "BackgroundColorDebuggerLightB", 230.0f / 255.0f);
        }
    }
}

static void sync_shader_preset_parameter_defaults(void)
{
    ShaderPresetInfo presets[SHADER_PRESET_MAX_DISCOVERED];
    int preset_count = shader_preset_scan_bundled(presets, SHADER_PRESET_MAX_DISCOVERED);

    for (int i = 0; i < preset_count; i++)
    {
        ShaderPreset preset;
        char error[512];
        if (!shader_preset_load(presets[i].path, &preset, error, sizeof(error)))
            continue;

        char preset_file[SHADER_PRESET_MAX_PATH];
        if (!shader_preset_get_config_path(preset.preset_path, preset_file, sizeof(preset_file)))
            continue;

        std::string section = shader_preset_section_name(preset_file);
        for (int j = 0; j < preset.parameter_count; j++)
        {
            ShaderPresetParameter* parameter = &preset.parameters[j];
            if (config_ini_data[section].has(parameter->name))
                continue;

            write_float(section.c_str(), parameter->name, parameter->default_value);
        }
    }
}
