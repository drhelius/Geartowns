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

#define GUI_FILEDIALOGS_IMPORT
#include "gui_filedialogs.h"

#include <SDL3/SDL.h>
#include <string>
#include "application.h"
#include "config.h"
#include "emu.h"
#include "gui.h"
#include "gui_actions.h"
#include "gui_debug.h"
#include "gui_debug_memory.h"
#include "gui_menus.h"
#include "utils.h"

enum FileDialogID
{
    FileDialog_None = 0,
    FileDialog_OpenMedia,
    FileDialog_LoadState,
    FileDialog_SaveState,
    FileDialog_ChooseSavestatePath,
    FileDialog_ChooseScreenshotPath,
    FileDialog_SaveScreenshot,
    FileDialog_LoadBIOS,
    FileDialog_SaveMemoryDump,
    FileDialog_LoadMemoryDump,
    FileDialog_SaveDebugSettings,
    FileDialog_LoadDebugSettings
};

static FileDialogID pending_dialog_id = FileDialog_None;
static std::string pending_dialog_path;
static bool dialog_active = false;
static bool pending_refocus_window = false;
#if !defined(__APPLE__)
static bool was_exclusive_fullscreen = false;
#endif

static bool begin_dialog(void);
static void SDLCALL file_dialog_callback(void* userdata, const char* const* filelist,
    int filter);
static void process_dialog_result(FileDialogID id, const char* path);

void gui_file_dialog_open_rom(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = {
        { "FM Towns Media", "d77;rdd;cue;chd;iso;bin;zip" }
    };
    const char* default_path = config_emulator.last_open_path.empty() ? NULL :
        config_emulator.last_open_path.c_str();
    SDL_ShowOpenFileDialog(file_dialog_callback,
        (void*)(intptr_t)FileDialog_OpenMedia, application_sdl_window, filters, 1,
        default_path, false);
}

void gui_file_dialog_choose_screenshot_path(void)
{
    if (!begin_dialog())
        return;

    const char* default_path = config_emulator.screenshots_path.empty() ? NULL :
        config_emulator.screenshots_path.c_str();
    SDL_ShowOpenFolderDialog(file_dialog_callback,
        (void*)(intptr_t)FileDialog_ChooseScreenshotPath, application_sdl_window,
        default_path, false);
}

void gui_file_dialog_save_screenshot(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "PNG Files", "png" } };
    SDL_ShowSaveFileDialog(file_dialog_callback,
        (void*)(intptr_t)FileDialog_SaveScreenshot, application_sdl_window, filters,
        1, NULL);
}

void gui_file_dialog_load_state(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = {
        { "Save State Files", "state;state1;state2;state3;state4;state5" }
    };
    const char* default_path = config_emulator.last_open_path.empty() ? NULL :
        config_emulator.last_open_path.c_str();
    SDL_ShowOpenFileDialog(file_dialog_callback,
        (void*)(intptr_t)FileDialog_LoadState, application_sdl_window, filters, 1,
        default_path, false);
}

void gui_file_dialog_save_state(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Save State Files", "state" } };
    const char* default_path = config_emulator.last_open_path.empty() ? NULL :
        config_emulator.last_open_path.c_str();
    SDL_ShowSaveFileDialog(file_dialog_callback,
        (void*)(intptr_t)FileDialog_SaveState, application_sdl_window, filters, 1,
        default_path);
}

void gui_file_dialog_choose_savestate_path(void)
{
    if (!begin_dialog())
        return;

    const char* default_path = config_emulator.savestates_path.empty() ? NULL :
        config_emulator.savestates_path.c_str();
    SDL_ShowOpenFolderDialog(file_dialog_callback,
        (void*)(intptr_t)FileDialog_ChooseSavestatePath, application_sdl_window,
        default_path, false);
}

void gui_file_dialog_load_bios(void)
{
    if (!begin_dialog())
        return;

    const char* default_path = config_emulator.bios_path.empty() ? NULL :
        config_emulator.bios_path.c_str();
    SDL_ShowOpenFolderDialog(file_dialog_callback,
        (void*)(intptr_t)FileDialog_LoadBIOS, application_sdl_window, default_path,
        false);
}

void gui_file_dialog_save_memory_dump(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Binary Files", "bin" } };
    SDL_ShowSaveFileDialog(file_dialog_callback,
        (void*)(intptr_t)FileDialog_SaveMemoryDump, application_sdl_window,
        filters, 1, NULL);
}

void gui_file_dialog_load_memory_dump(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Binary Files", "bin;rom;dat" } };
    SDL_ShowOpenFileDialog(file_dialog_callback,
        (void*)(intptr_t)FileDialog_LoadMemoryDump, application_sdl_window,
        filters, 1, NULL, false);
}

void gui_file_dialog_save_debug_settings(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Geartowns Debug Settings", "gtdebug" } };
    SDL_ShowSaveFileDialog(file_dialog_callback,
        (void*)(intptr_t)FileDialog_SaveDebugSettings, application_sdl_window,
        filters, 1, config_root_path);
}

void gui_file_dialog_load_debug_settings(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Geartowns Debug Settings", "gtdebug" } };
    SDL_ShowOpenFileDialog(file_dialog_callback,
        (void*)(intptr_t)FileDialog_LoadDebugSettings, application_sdl_window,
        filters, 1, config_root_path, false);
}

void gui_file_dialog_process_results(void)
{
    bool refocus_window = pending_refocus_window && !dialog_active;

#if !defined(__APPLE__)
    if (was_exclusive_fullscreen && !dialog_active)
    {
        was_exclusive_fullscreen = false;
        application_trigger_fullscreen(true);
    }
#endif

    if (pending_dialog_id != FileDialog_None)
    {
        FileDialogID id = pending_dialog_id;
        std::string path = pending_dialog_path;
        pending_dialog_id = FileDialog_None;
        pending_dialog_path.clear();
        process_dialog_result(id, path.c_str());
    }

    if (refocus_window)
    {
        pending_refocus_window = false;
        application_refocus_window();
    }
}

bool gui_file_dialog_is_active(void)
{
    return dialog_active;
}

static bool begin_dialog(void)
{
    if (dialog_active)
        return false;

    dialog_active = true;

#if !defined(__APPLE__)
    if (config_emulator.fullscreen && config_emulator.fullscreen_mode == 1)
    {
        was_exclusive_fullscreen = true;
        application_trigger_fullscreen(false);
    }
#endif

    return true;
}

static void SDLCALL file_dialog_callback(void* userdata, const char* const* filelist,
    int filter)
{
    UNUSED(filter);
    dialog_active = false;
    pending_refocus_window = true;

    if (!filelist || !filelist[0])
        return;

    FileDialogID id = (FileDialogID)(intptr_t)userdata;
    pending_dialog_id = id;
    pending_dialog_path = filelist[0];

    if (id == FileDialog_SaveState)
        append_extension_if_missing(pending_dialog_path, ".state");
    else if (id == FileDialog_SaveScreenshot)
        append_extension_if_missing(pending_dialog_path, ".png");
    else if (id == FileDialog_SaveMemoryDump)
        append_extension_if_missing(pending_dialog_path, ".bin");
    else if (id == FileDialog_SaveDebugSettings)
        append_extension_if_missing(pending_dialog_path, ".gtdebug");
}

static void process_dialog_result(FileDialogID id, const char* path)
{
    switch (id)
    {
        case FileDialog_OpenMedia:
        {
            std::string full_path(path);
            size_t separator = full_path.find_last_of("/\\");
            config_emulator.last_open_path = separator == std::string::npos ? "" :
                full_path.substr(0, separator + 1);
            gui_load_rom(path);
            break;
        }
        case FileDialog_ChooseScreenshotPath:
            strncpy_fit(gui_screenshots_path, path, sizeof(gui_screenshots_path));
            config_emulator.screenshots_path = path;
            break;
        case FileDialog_LoadState:
        {
            std::string message("Loading state from ");
            message += path;
            gui_set_status_message(message.c_str(), 3000);
            emu_load_state_file(path);
            break;
        }
        case FileDialog_SaveState:
        {
            std::string message("Saving state to ");
            message += path;
            gui_set_status_message(message.c_str(), 3000);
            emu_save_state_file(path);
            break;
        }
        case FileDialog_ChooseSavestatePath:
            strncpy_fit(gui_savestates_path, path, sizeof(gui_savestates_path));
            config_emulator.savestates_path = path;
            update_savestates_data();
            break;
        case FileDialog_SaveScreenshot:
            gui_action_save_screenshot(path);
            break;
        case FileDialog_LoadBIOS:
            gui_load_bios(path);
            break;
        case FileDialog_SaveMemoryDump:
            gui_debug_memory_save_dump(path);
            break;
        case FileDialog_LoadMemoryDump:
            gui_debug_memory_load_dump(path);
            break;
        case FileDialog_SaveDebugSettings:
            gui_debug_save_settings(path);
            break;
        case FileDialog_LoadDebugSettings:
            gui_debug_load_settings(path);
            break;
        default:
            break;
    }
}
