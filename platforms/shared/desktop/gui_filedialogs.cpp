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
#include "debug/gui_debug.h"
#include "debug/gui_debug_disassembler.h"
#include "debug/gui_debug_trace.h"
#include "debug/gui_debug_framebuffers.h"
#include "debug/gui_debug_memory.h"
#include "gui_menus.h"
#include "gui_floppy.h"
#include "utils.h"

enum FileDialogID
{
    FileDialog_None = 0,
    FileDialog_OpenMedia,
    FileDialog_LoadState,
    FileDialog_SaveState,
    FileDialog_ChooseSavestatePath,
    FileDialog_ChooseSavefilesPath,
    FileDialog_OpenFloppy,
    FileDialog_SaveFloppy,
    FileDialog_NewFloppy,
    FileDialog_ChooseScreenshotPath,
    FileDialog_ChooseVideoRecordingPath,
    FileDialog_SaveScreenshot,
    FileDialog_SaveVideo,
    FileDialog_LoadBIOS,
    FileDialog_SaveMemoryDump,
    FileDialog_LoadMemoryDump,
    FileDialog_SaveDebugSettings,
    FileDialog_LoadDebugSettings,
    FileDialog_SaveSprite,
    FileDialog_SaveAllSprites,
    FileDialog_LoadSymbols,
    FileDialog_SaveTrace,
    FileDialog_ChooseTracePath,
    FileDialog_SaveDisassemblerFull,
    FileDialog_SaveDisassemblerVisible
};

static FileDialogID pending_dialog_id = FileDialog_None;
static std::string pending_dialog_path;
static bool dialog_active = false;
static int dialog_floppy_drive = 0;
static int dialog_sprite_index = 0;
static bool pending_refocus_window = false;
#if !defined(__APPLE__)
static bool was_exclusive_fullscreen = false;
#endif

static bool begin_dialog(void);
static void SDLCALL file_dialog_callback(void* userdata, const char* const* filelist, int filter);
static void process_dialog_result(FileDialogID id, const char* path);

void gui_file_dialog_open_rom(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = {
        { "CD-ROM Images", "cue;chd;iso;zip;m3u" }
    };
    const char* default_path = config_emulator.last_open_path.empty() ? NULL : config_emulator.last_open_path.c_str();
    SDL_ShowOpenFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_OpenMedia, application_sdl_window,
        filters, 1, default_path, false);
}

void gui_file_dialog_choose_screenshot_path(void)
{
    if (!begin_dialog())
        return;

    const char* default_path =
        config_emulator.screenshots_path.empty() ? NULL : config_emulator.screenshots_path.c_str();
    SDL_ShowOpenFolderDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_ChooseScreenshotPath,
        application_sdl_window, default_path, false);
}

void gui_file_dialog_choose_video_recording_path(void)
{
    if (!begin_dialog())
        return;

    const char* default_path =
        config_emulator.video_recordings_path.empty() ? NULL : config_emulator.video_recordings_path.c_str();
    SDL_ShowOpenFolderDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_ChooseVideoRecordingPath,
        application_sdl_window, default_path, false);
}

void gui_file_dialog_save_screenshot(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "PNG Files", "png" } };
    SDL_ShowSaveFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_SaveScreenshot, application_sdl_window,
        filters, 1, NULL);
}

void gui_file_dialog_save_sprite(int index)
{
    if (!begin_dialog())
        return;

    dialog_sprite_index = index;
    SDL_DialogFileFilter filters[] = { { "PNG Files", "png" } };
    SDL_ShowSaveFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_SaveSprite, application_sdl_window,
        filters, 1, NULL);
}

void gui_file_dialog_save_trace(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Text Files", "txt" } };
    SDL_ShowSaveFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_SaveTrace, application_sdl_window,
        filters, 1, NULL);
}

void gui_file_dialog_save_disassembler(bool full)
{
    if (!begin_dialog())
        return;

    FileDialogID id = full ? FileDialog_SaveDisassemblerFull : FileDialog_SaveDisassemblerVisible;
    SDL_DialogFileFilter filters[] = { { "Text Files", "txt" } };
    SDL_ShowSaveFileDialog(file_dialog_callback, (void*)(intptr_t)id, application_sdl_window, filters, 1, NULL);
}

void gui_file_dialog_choose_trace_path(void)
{
    if (!begin_dialog())
        return;

    const char* default_path = config_debug.trace_output_path.empty() ? NULL : config_debug.trace_output_path.c_str();
    SDL_ShowOpenFolderDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_ChooseTracePath, application_sdl_window,
        default_path, false);
}

void gui_file_dialog_load_symbols(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Symbol Files", "sym;txt" } };
    SDL_ShowOpenFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_LoadSymbols, application_sdl_window,
        filters, 1, NULL, false);
}

void gui_file_dialog_save_all_sprites(void)
{
    if (!begin_dialog())
        return;

    SDL_ShowOpenFolderDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_SaveAllSprites, application_sdl_window,
        NULL, false);
}

void gui_file_dialog_save_video(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "AVI Files", "avi" } };
    SDL_ShowSaveFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_SaveVideo, application_sdl_window,
        filters, 1, NULL);
}

void gui_file_dialog_load_state(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = {
        { "Save State Files", "state;state1;state2;state3;state4;state5" }
    };
    const char* default_path = config_emulator.last_open_path.empty() ? NULL : config_emulator.last_open_path.c_str();
    SDL_ShowOpenFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_LoadState, application_sdl_window,
        filters, 1, default_path, false);
}

void gui_file_dialog_save_state(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Save State Files", "state" } };
    const char* default_path = config_emulator.last_open_path.empty() ? NULL : config_emulator.last_open_path.c_str();
    SDL_ShowSaveFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_SaveState, application_sdl_window,
        filters, 1, default_path);
}

void gui_file_dialog_choose_savestate_path(void)
{
    if (!begin_dialog())
        return;

    const char* default_path = config_emulator.savestates_path.empty() ? NULL : config_emulator.savestates_path.c_str();
    SDL_ShowOpenFolderDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_ChooseSavestatePath,
        application_sdl_window, default_path, false);
}

void gui_file_dialog_choose_savefiles_path(void)
{
    if (!begin_dialog())
        return;

    const char* default_path = config_emulator.savefiles_path.empty() ? NULL : config_emulator.savefiles_path.c_str();
    SDL_ShowOpenFolderDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_ChooseSavefilesPath,
        application_sdl_window, default_path, false);
}

void gui_file_dialog_open_floppy(int drive)
{
    if (!begin_dialog())
        return;

    dialog_floppy_drive = drive;
    SDL_DialogFileFilter filters[] = {
        { "Floppy Images", "d77;d88;hdm;xdf;img;bin;zip;m3u" }
    };
    const char* default_path = config_emulator.last_open_path.empty() ? NULL : config_emulator.last_open_path.c_str();
    SDL_ShowOpenFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_OpenFloppy, application_sdl_window,
        filters, 1, default_path, false);
}

void gui_file_dialog_save_floppy(int drive)
{
    if (!begin_dialog())
        return;

    dialog_floppy_drive = drive;
    SDL_DialogFileFilter filters[] = {
        { "D77 Images", "d77;d88" },
        { "Raw Images", "hdm;img;xdf" }
    };
    const char* default_path = config_emulator.last_open_path.empty() ? NULL : config_emulator.last_open_path.c_str();
    SDL_ShowSaveFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_SaveFloppy, application_sdl_window,
        filters, 2, default_path);
}

void gui_file_dialog_new_floppy(int drive)
{
    if (!begin_dialog())
        return;

    dialog_floppy_drive = drive;
    SDL_DialogFileFilter filters[] = {
        { "D77 Images", "d77" },
        { "Raw Images", "hdm;img;xdf" }
    };
    const char* default_path = config_emulator.last_open_path.empty() ? NULL : config_emulator.last_open_path.c_str();
    SDL_ShowSaveFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_NewFloppy, application_sdl_window,
        filters, 2, default_path);
}

void gui_file_dialog_load_bios(void)
{
    if (!begin_dialog())
        return;

    const char* default_path = config_emulator.bios_path.empty() ? NULL : config_emulator.bios_path.c_str();
    SDL_ShowOpenFolderDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_LoadBIOS, application_sdl_window,
        default_path, false);
}

void gui_file_dialog_save_memory_dump(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Binary Files", "bin" } };
    SDL_ShowSaveFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_SaveMemoryDump, application_sdl_window,
        filters, 1, NULL);
}

void gui_file_dialog_load_memory_dump(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Binary Files", "bin;rom;dat" } };
    SDL_ShowOpenFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_LoadMemoryDump, application_sdl_window,
        filters, 1, NULL, false);
}

void gui_file_dialog_save_debug_settings(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Geartowns Debug Settings", "gtdebug" } };
    SDL_ShowSaveFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_SaveDebugSettings, application_sdl_window,
        filters, 1, config_root_path);
}

void gui_file_dialog_load_debug_settings(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Geartowns Debug Settings", "gtdebug" } };
    SDL_ShowOpenFileDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_LoadDebugSettings, application_sdl_window,
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

static void SDLCALL file_dialog_callback(void* userdata, const char* const* filelist, int filter)
{
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
    else if (id == FileDialog_SaveVideo)
        append_extension_if_missing(pending_dialog_path, ".avi");
    else if (id == FileDialog_SaveMemoryDump)
        append_extension_if_missing(pending_dialog_path, ".bin");
    else if (id == FileDialog_SaveDebugSettings)
        append_extension_if_missing(pending_dialog_path, ".gtdebug");
    else if (id == FileDialog_SaveSprite)
        append_extension_if_missing(pending_dialog_path, ".png");
    else if (id == FileDialog_SaveTrace || id == FileDialog_SaveDisassemblerFull ||
        id == FileDialog_SaveDisassemblerVisible)
        append_extension_if_missing(pending_dialog_path, ".txt");
    else if (id == FileDialog_SaveFloppy || id == FileDialog_NewFloppy)
    {
        const char* path = pending_dialog_path.c_str();
        bool named = ends_with_no_case(path, ".d77") || ends_with_no_case(path, ".d88") ||
            ends_with_no_case(path, ".hdm") || ends_with_no_case(path, ".img") || ends_with_no_case(path, ".xdf");

        if (!named)
            pending_dialog_path += filter == 1 ? ".hdm" : ".d77";
    }
}

static void process_dialog_result(FileDialogID id, const char* path)
{
    switch (id)
    {
        case FileDialog_OpenMedia:
        {
            std::string full_path(path);
            size_t separator = full_path.find_last_of("/\\");
            config_emulator.last_open_path = separator == std::string::npos ? "" : full_path.substr(0, separator + 1);
            gui_load_rom(path);
            break;
        }

        case FileDialog_ChooseScreenshotPath:
            strncpy_fit(gui_screenshots_path, path, sizeof(gui_screenshots_path));
            config_emulator.screenshots_path = path;
            break;
        case FileDialog_ChooseVideoRecordingPath:
            strncpy_fit(gui_video_recordings_path, path, sizeof(gui_video_recordings_path));
            config_emulator.video_recordings_path = path;
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
        case FileDialog_ChooseSavefilesPath:
            strncpy_fit(gui_savefiles_path, path, sizeof(gui_savefiles_path));
            config_emulator.savefiles_path = path;
            break;
        case FileDialog_OpenFloppy:
            gui_floppy_dialog_insert(dialog_floppy_drive, path);
            break;
        case FileDialog_SaveFloppy:
            gui_floppy_dialog_save_as(dialog_floppy_drive, path);
            break;
        case FileDialog_NewFloppy:
            gui_floppy_dialog_new_blank(dialog_floppy_drive, path);
            break;
        case FileDialog_SaveScreenshot:
            gui_action_save_screenshot(path);
            break;
        case FileDialog_SaveVideo:
            gui_action_start_video_recording(path);
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
        case FileDialog_SaveSprite:
            if (gui_debug_save_sprite(path, dialog_sprite_index))
                gui_set_status_message("Sprite saved", 3000);
            else
                gui_set_error_message("Unable to save sprite");
            break;
        case FileDialog_SaveTrace:
            if (gui_debug_trace_save(path))
                gui_set_status_message("Trace saved", 3000);
            else
                gui_set_error_message("Unable to save the trace");
            break;
        case FileDialog_ChooseTracePath:
            config_debug.trace_output_path = path;
            break;
        case FileDialog_SaveDisassemblerFull:
        case FileDialog_SaveDisassemblerVisible:
            if (gui_debug_save_disassembler(path, id == FileDialog_SaveDisassemblerFull))
                gui_set_status_message("Disassembly saved", 3000);
            else
                gui_set_error_message("Unable to save the disassembly");
            break;
        case FileDialog_LoadSymbols:
        {
            int count = gui_debug_load_symbols(path);

            if (count >= 0)
            {
                char message[64];
                snprintf(message, sizeof(message), "%d symbols loaded", count);
                gui_set_status_message(message, 3000);
            }
            else
                gui_set_error_message("Unable to load symbols");

            break;
        }
        case FileDialog_SaveAllSprites:
            if (gui_debug_save_all_sprites(path))
                gui_set_status_message("Sprites saved", 3000);
            else
                gui_set_error_message("Unable to save sprites");
            break;
        default:
            break;
    }
}
