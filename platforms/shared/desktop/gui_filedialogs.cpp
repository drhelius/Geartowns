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
#include <mutex>
#include <string>
#include "gui.h"
#include "gui_actions.h"
#include "gui_menus.h"
#include "application.h"
#include "config.h"
#include "utils.h"

enum FileDialogID
{
    FileDialog_None = 0,
    FileDialog_OpenROM,
    FileDialog_ChooseScreenshotPath,
    FileDialog_LoadBios,
    FileDialog_SaveScreenshot
};

static FileDialogID pending_dialog_id = FileDialog_None;
static std::string pending_dialog_path;
static bool dialog_active = false;
static bool pending_refocus_window = false;
#if !defined(__APPLE__)
static bool was_exclusive_fullscreen = false;
#endif
static std::mutex dialog_mutex;
static const SDL_DialogFileFilter media_filters[] = {{"FM Towns Media", "d77;rdd;cue;chd;iso;bin;zip"}};
static const SDL_DialogFileFilter screenshot_filters[] = {{"PNG Files", "png"}};

static void SDLCALL file_dialog_callback(void* userdata, const char* const* filelist, int filter);
static void process_dialog_result(FileDialogID id, const char* path);

static bool begin_dialog(void)
{
    std::lock_guard<std::mutex> lock(dialog_mutex);
    if (dialog_active)
        return false;

    dialog_active = true;
    gui_dialog_in_use = true;

#if !defined(__APPLE__)
    if (config_emulator.fullscreen && config_emulator.fullscreen_mode == 1)
    {
        was_exclusive_fullscreen = true;
        application_trigger_fullscreen(false);
    }
#endif

    return true;
}

void gui_file_dialog_open_rom(void)
{
    if (!begin_dialog())
        return;

    const char* default_path = config_emulator.last_open_path.empty() ? NULL : config_emulator.last_open_path.c_str();
    SDL_ShowOpenFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_OpenROM, application_sdl_window, media_filters, 1, default_path, false);
}

void gui_file_dialog_choose_screenshot_path(void)
{
    if (!begin_dialog())
        return;

    const char* default_path = config_emulator.screenshots_path.empty() ? NULL : config_emulator.screenshots_path.c_str();
    SDL_ShowOpenFolderDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_ChooseScreenshotPath, application_sdl_window, default_path, false);
}

void gui_file_dialog_load_bios(void)
{
    if (!begin_dialog())
        return;

    const char* default_path = config_emulator.bios_path.empty() ? NULL : config_emulator.bios_path.c_str();
    SDL_ShowOpenFolderDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_LoadBios, application_sdl_window, default_path, false);
}

void gui_file_dialog_save_screenshot(void)
{
    if (!begin_dialog())
        return;

    SDL_ShowSaveFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_SaveScreenshot, application_sdl_window, screenshot_filters, 1, NULL);
}

void gui_file_dialog_process_results(void)
{
    FileDialogID id = FileDialog_None;
    std::string path;
    bool refocus_window = false;
#if !defined(__APPLE__)
    bool restore_exclusive_fullscreen = false;
#endif

    {
        std::lock_guard<std::mutex> lock(dialog_mutex);

        if (pending_refocus_window && !dialog_active)
        {
            pending_refocus_window = false;
            refocus_window = true;
        }

#if !defined(__APPLE__)
        if (was_exclusive_fullscreen && !dialog_active)
        {
            was_exclusive_fullscreen = false;
            restore_exclusive_fullscreen = true;
        }
#endif

        if (pending_dialog_id != FileDialog_None)
        {
            id = pending_dialog_id;
            path = pending_dialog_path;
            pending_dialog_id = FileDialog_None;
            pending_dialog_path.clear();
        }
    }

#if !defined(__APPLE__)
    if (restore_exclusive_fullscreen)
        application_trigger_fullscreen(true);
#endif

    if (refocus_window)
    {
        gui_dialog_in_use = false;
        application_refocus_window();
    }

    if (id != FileDialog_None)
        process_dialog_result(id, path.c_str());
}

bool gui_file_dialog_is_active(void)
{
    std::lock_guard<std::mutex> lock(dialog_mutex);
    return dialog_active;
}

static void SDLCALL file_dialog_callback(void* userdata, const char* const* filelist, int filter)
{
    UNUSED(filter);
    std::lock_guard<std::mutex> lock(dialog_mutex);

    dialog_active = false;
    pending_refocus_window = true;

    FileDialogID id = (FileDialogID)(intptr_t)userdata;

    if (!filelist || !filelist[0] || !filelist[0][0])
        return;

    pending_dialog_id = id;
    pending_dialog_path = filelist[0];
}

static void process_dialog_result(FileDialogID id, const char* path)
{
    if (!path || !path[0])
        return;

    switch (id)
    {
        case FileDialog_OpenROM:
        {
            std::string str_path = path;
            std::string::size_type pos = str_path.find_last_of("\\/");
            config_emulator.last_open_path.assign(str_path.substr(0, pos + 1));
            gui_load_rom(path);
            break;
        }
        case FileDialog_ChooseScreenshotPath:
        {
            strncpy_fit(gui_screenshots_path, path, sizeof(gui_screenshots_path));
            config_emulator.screenshots_path.assign(path);
            break;
        }
        case FileDialog_LoadBios:
        {
            gui_load_bios(path);
            break;
        }
        case FileDialog_SaveScreenshot:
        {
            gui_action_save_screenshot(path);
            break;
        }
        default:
            break;
    }
}
