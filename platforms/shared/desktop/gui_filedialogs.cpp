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
#include "gui_notifications.h"
#include "debug/gui_debug.h"
#include "debug/gui_debug_disassembler.h"
#include "debug/gui_debug_trace_logger.h"
#include "debug/gui_debug_framebuffers.h"
#include "debug/gui_debug_memory.h"
#include "gui_menus.h"
#include "gui_floppy.h"
#include "gui_hard_disks.h"
#include "gui_memory_card.h"
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
    FileDialog_AttachHardDisk,
    FileDialog_CreateHardDisk,
    FileDialog_InsertMemoryCard,
    FileDialog_CreateMemoryCard,
    FileDialog_ChooseScreenshotPath,
    FileDialog_ChooseVideoRecordingPath,
    FileDialog_SaveScreenshot,
    FileDialog_SaveVGM,
    FileDialog_SaveVideo,
    FileDialog_LoadBIOS,
    FileDialog_LoadModelFirmware,
    FileDialog_SaveMemoryDump,
    FileDialog_LoadMemoryDump,
    FileDialog_SaveDebugSettings,
    FileDialog_LoadDebugSettings,
    FileDialog_SaveSprite,
    FileDialog_SaveAllSprites,
    FileDialog_LoadSymbols,
    FileDialog_SaveLog,
    FileDialog_ChooseTracePath,
    FileDialog_SaveDisassemblerFull,
    FileDialog_SaveDisassemblerVisible
};

static FileDialogID pending_dialog_id = FileDialog_None;
static std::string pending_dialog_path;
static bool dialog_active = false;
static int dialog_floppy_drive = 0;
static int dialog_hard_disk_id = 0;
static GT_Machine_Model dialog_firmware_model = GT_MACHINE_MODEL1_2;
static int dialog_sprite_index = 0;
static bool pending_refocus_window = false;
#if !defined(__APPLE__)
static bool was_exclusive_fullscreen = false;
#endif

static bool begin_dialog(void);
static std::string* get_dialog_folder(FileDialogID id);
static const char* get_dialog_location(FileDialogID id);
static void remember_dialog_location(FileDialogID id, const char* path);
static void show_open_file_dialog(FileDialogID id, const SDL_DialogFileFilter* filters, int filter_count);
static void show_save_file_dialog(FileDialogID id, const SDL_DialogFileFilter* filters, int filter_count);
static void show_open_folder_dialog(FileDialogID id);
static void SDLCALL file_dialog_callback(void* userdata, const char* const* filelist, int filter);
static void process_dialog_result(FileDialogID id, const char* path);

void gui_file_dialog_open_rom(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = {
        { "CD-ROM Images", "cue;chd;iso;zip;m3u" }
    };
    show_open_file_dialog(FileDialog_OpenMedia, filters, 1);
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
    show_save_file_dialog(FileDialog_SaveScreenshot, filters, 1);
}

void gui_file_dialog_save_sprite(int index)
{
    if (!begin_dialog())
        return;

    dialog_sprite_index = index;
    SDL_DialogFileFilter filters[] = { { "PNG Files", "png" } };
    show_save_file_dialog(FileDialog_SaveSprite, filters, 1);
}

void gui_file_dialog_save_log(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Log Files", "txt" } };
    show_save_file_dialog(FileDialog_SaveLog, filters, 1);
}

void gui_file_dialog_save_disassembler(bool full)
{
    if (!begin_dialog())
        return;

    FileDialogID id = full ? FileDialog_SaveDisassemblerFull : FileDialog_SaveDisassemblerVisible;
    SDL_DialogFileFilter filters[] = { { "Text Files", "txt" } };
    show_save_file_dialog(id, filters, 1);
}

void gui_file_dialog_choose_trace_path(void)
{
    if (!begin_dialog())
        return;

    const char* default_path = config_debug.trace_disk_path.empty() ? NULL : config_debug.trace_disk_path.c_str();
    SDL_ShowOpenFolderDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_ChooseTracePath, application_sdl_window,
        default_path, false);
}

void gui_file_dialog_load_symbols(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Symbol Files", "sym;txt;map" } };
    show_open_file_dialog(FileDialog_LoadSymbols, filters, 1);
}

void gui_file_dialog_save_all_sprites(void)
{
    if (!begin_dialog())
        return;

    show_open_folder_dialog(FileDialog_SaveAllSprites);
}

void gui_file_dialog_save_vgm(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "VGM Files", "vgm" } };
    show_save_file_dialog(FileDialog_SaveVGM, filters, 1);
}

void gui_file_dialog_save_video(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "AVI Files", "avi" } };
    show_save_file_dialog(FileDialog_SaveVideo, filters, 1);
}

void gui_file_dialog_load_state(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = {
        { "Save State Files", "state;state1;state2;state3;state4;state5" }
    };
    show_open_file_dialog(FileDialog_LoadState, filters, 1);
}

void gui_file_dialog_save_state(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Save State Files", "state" } };
    show_save_file_dialog(FileDialog_SaveState, filters, 1);
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
        { "Floppy Images", "d77;d88;hdm;xdf;img;mfm;bin;zip;m3u" }
    };
    show_open_file_dialog(FileDialog_OpenFloppy, filters, 1);
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
    show_save_file_dialog(FileDialog_SaveFloppy, filters, 2);
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
    show_save_file_dialog(FileDialog_NewFloppy, filters, 2);
}

void gui_file_dialog_attach_hard_disk(int id)
{
    if (!begin_dialog())
        return;

    dialog_hard_disk_id = id;
    SDL_DialogFileFilter filters[] = {
        { "Hard Disk Images", "hdd;hdm;chd;h0;h1;h2;h3;h4;h5;h6" },
        { "All Files", "*" }
    };
    show_open_file_dialog(FileDialog_AttachHardDisk, filters, 2);
}

void gui_file_dialog_create_hard_disk(int id)
{
    if (!begin_dialog())
        return;

    dialog_hard_disk_id = id;
    SDL_DialogFileFilter filters[] = {
        { "Hard Disk Images", "hdd;h0;h1;h2;h3;h4;h5;h6" }
    };
    show_save_file_dialog(FileDialog_CreateHardDisk, filters, 1);
}

void gui_file_dialog_insert_memory_card(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = {
        { "IC Memory Card Images", "icm" },
        { "All Files", "*" }
    };
    show_open_file_dialog(FileDialog_InsertMemoryCard, filters, 2);
}

void gui_file_dialog_create_memory_card(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = {
        { "IC Memory Card Images", "icm" }
    };
    show_save_file_dialog(FileDialog_CreateMemoryCard, filters, 1);
}

void gui_file_dialog_load_bios(void)
{
    if (!begin_dialog())
        return;

    const char* default_path = config_emulator.bios_path.empty() ? NULL : config_emulator.bios_path.c_str();
    SDL_ShowOpenFolderDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_LoadBIOS, application_sdl_window,
        default_path, false);
}

void gui_file_dialog_load_model_firmware(int model)
{
    if (!begin_dialog())
        return;

    const std::string& folder = config_machine.firmware_paths[model];
    const char* default_path = !folder.empty() ? folder.c_str() :
        config_emulator.bios_path.empty() ? NULL : config_emulator.bios_path.c_str();
    dialog_firmware_model = (GT_Machine_Model)model;
    SDL_ShowOpenFolderDialog(file_dialog_callback, (void*)(intptr_t)FileDialog_LoadModelFirmware,
        application_sdl_window, default_path, false);
}

void gui_file_dialog_save_memory_dump(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Binary Files", "bin" } };
    show_save_file_dialog(FileDialog_SaveMemoryDump, filters, 1);
}

void gui_file_dialog_load_memory_dump(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Binary Files", "bin;rom;dat" } };
    show_open_file_dialog(FileDialog_LoadMemoryDump, filters, 1);
}

void gui_file_dialog_save_debug_settings(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Geartowns Debug Settings", "gtdebug" } };
    show_save_file_dialog(FileDialog_SaveDebugSettings, filters, 1);
}

void gui_file_dialog_load_debug_settings(void)
{
    if (!begin_dialog())
        return;

    SDL_DialogFileFilter filters[] = { { "Geartowns Debug Settings", "gtdebug" } };
    show_open_file_dialog(FileDialog_LoadDebugSettings, filters, 1);
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
        remember_dialog_location(id, path.c_str());
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

static std::string* get_dialog_folder(FileDialogID id)
{
    switch (id)
    {
        case FileDialog_OpenMedia:
            return &config_file_dialogs.rom;
        case FileDialog_LoadState:
        case FileDialog_SaveState:
            return &config_file_dialogs.states;
        case FileDialog_OpenFloppy:
        case FileDialog_SaveFloppy:
        case FileDialog_NewFloppy:
            return &config_file_dialogs.floppies;
        case FileDialog_AttachHardDisk:
        case FileDialog_CreateHardDisk:
            return &config_file_dialogs.hard_disks;
        case FileDialog_InsertMemoryCard:
        case FileDialog_CreateMemoryCard:
            return &config_file_dialogs.memory_cards;
        case FileDialog_SaveScreenshot:
            return &config_file_dialogs.screenshots;
        case FileDialog_SaveVGM:
        case FileDialog_SaveVideo:
            return &config_file_dialogs.recordings;
        case FileDialog_LoadSymbols:
        case FileDialog_SaveMemoryDump:
        case FileDialog_LoadMemoryDump:
        case FileDialog_SaveDebugSettings:
        case FileDialog_LoadDebugSettings:
        case FileDialog_SaveSprite:
        case FileDialog_SaveAllSprites:
        case FileDialog_SaveLog:
        case FileDialog_SaveDisassemblerFull:
        case FileDialog_SaveDisassemblerVisible:
            return &config_file_dialogs.debug;
        default:
            return NULL;
    }
}

static const char* get_dialog_location(FileDialogID id)
{
    std::string* folder = get_dialog_folder(id);

    if (IsValidPointer(folder) && path_exists(folder->c_str()))
        return folder->c_str();

    return path_exists(config_file_dialogs.rom.c_str()) ? config_file_dialogs.rom.c_str() : NULL;
}

static void remember_dialog_location(FileDialogID id, const char* path)
{
    std::string* folder = get_dialog_folder(id);

    if (!IsValidPointer(folder))
        return;

    std::string location(path);
    SDL_PathInfo info;

    if (!SDL_GetPathInfo(path, &info) || (info.type != SDL_PATHTYPE_DIRECTORY))
        location.erase(location.find_last_of("/\\") + 1);
    else if (location.find_last_of("/\\") != location.size() - 1)
    {
        // Windows file dialogs take a location without a trailing separator as a file name
#if defined(_WIN32)
        location += '\\';
#else
        location += '/';
#endif
    }

    *folder = location;
}

static void show_open_file_dialog(FileDialogID id, const SDL_DialogFileFilter* filters, int filter_count)
{
    SDL_ShowOpenFileDialog(file_dialog_callback, (void*)(intptr_t)id, application_sdl_window, filters, filter_count,
        get_dialog_location(id), false);
}

static void show_save_file_dialog(FileDialogID id, const SDL_DialogFileFilter* filters, int filter_count)
{
    SDL_ShowSaveFileDialog(file_dialog_callback, (void*)(intptr_t)id, application_sdl_window, filters, filter_count,
        get_dialog_location(id));
}

static void show_open_folder_dialog(FileDialogID id)
{
    SDL_ShowOpenFolderDialog(file_dialog_callback, (void*)(intptr_t)id, application_sdl_window,
        get_dialog_location(id), false);
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
    else if (id == FileDialog_SaveVGM)
        append_extension_if_missing(pending_dialog_path, ".vgm");
    else if (id == FileDialog_SaveVideo)
        append_extension_if_missing(pending_dialog_path, ".avi");
    else if (id == FileDialog_SaveMemoryDump)
        append_extension_if_missing(pending_dialog_path, ".bin");
    else if (id == FileDialog_SaveDebugSettings)
        append_extension_if_missing(pending_dialog_path, ".gtdebug");
    else if (id == FileDialog_SaveSprite)
        append_extension_if_missing(pending_dialog_path, ".png");
    else if (id == FileDialog_SaveLog || id == FileDialog_SaveDisassemblerFull ||
        id == FileDialog_SaveDisassemblerVisible)
        append_extension_if_missing(pending_dialog_path, ".txt");
    else if (id == FileDialog_CreateHardDisk)
    {
        size_t dot = pending_dialog_path.find_last_of('.');
        size_t separator = pending_dialog_path.find_last_of("/\\");

        if ((dot == std::string::npos) || ((separator != std::string::npos) && (dot < separator)))
            pending_dialog_path += ".hdd";
    }
    else if (id == FileDialog_CreateMemoryCard)
        append_extension_if_missing(pending_dialog_path, ".icm");
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
            gui_load_rom(path);
            break;
        case FileDialog_ChooseScreenshotPath:
            strncpy_fit(gui_screenshots_path, path, sizeof(gui_screenshots_path));
            config_emulator.screenshots_path = path;
            break;
        case FileDialog_ChooseVideoRecordingPath:
            strncpy_fit(gui_video_recordings_path, path, sizeof(gui_video_recordings_path));
            config_emulator.video_recordings_path = path;
            break;
        case FileDialog_LoadState:
            gui_action_load_state(path);
            break;
        case FileDialog_SaveState:
            gui_action_save_state(path);
            break;

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
        case FileDialog_AttachHardDisk:
            gui_hard_disks_dialog_attach(dialog_hard_disk_id, path);
            break;
        case FileDialog_CreateHardDisk:
            gui_hard_disks_dialog_create(dialog_hard_disk_id, path);
            break;
        case FileDialog_InsertMemoryCard:
            gui_memory_card_dialog_insert(path);
            break;
        case FileDialog_CreateMemoryCard:
            gui_memory_card_dialog_create(path);
            break;
        case FileDialog_SaveScreenshot:
            gui_action_save_screenshot(path);
            break;
        case FileDialog_SaveVGM:
            if (emu_start_vgm_recording(path))
                gui_notify(gui_NotificationInfo, ICON_MD_FIBER_MANUAL_RECORD, "VGM recording started", path, "vgm");
            else
                gui_notify(gui_NotificationError, NULL, "Unable to start VGM recording", path, "vgm");
            break;
        case FileDialog_SaveVideo:
            gui_action_start_video_recording(path);
            break;
        case FileDialog_LoadBIOS:
            gui_load_bios(path);
            break;
        case FileDialog_LoadModelFirmware:
            gui_load_model_firmware(dialog_firmware_model, path);
            break;
        case FileDialog_SaveMemoryDump:
            if (gui_debug_memory_save_dump(path))
                gui_notify(gui_NotificationSuccess, ICON_MD_SAVE, "Memory dump saved", path);
            else
                gui_notify(gui_NotificationError, NULL, "Unable to save memory dump", path);
            break;
        case FileDialog_LoadMemoryDump:
            if (gui_debug_memory_load_dump(path))
                gui_notify(gui_NotificationSuccess, ICON_MD_FILE_UPLOAD, "Memory dump loaded", path);
            else
                gui_notify(gui_NotificationError, NULL, "Unable to load memory dump", path);
            break;
        case FileDialog_SaveDebugSettings:
            if (gui_debug_save_settings(path))
                gui_notify(gui_NotificationSuccess, ICON_MD_BUG_REPORT, "Debug settings saved", path);
            else
                gui_notify(gui_NotificationError, NULL, "Unable to save debug settings", path);
            break;
        case FileDialog_LoadDebugSettings:
            if (gui_debug_load_settings(path))
                gui_notify(gui_NotificationSuccess, ICON_MD_BUG_REPORT, "Debug settings loaded", path);
            else
                gui_notify(gui_NotificationError, NULL, "Unable to load debug settings", path);
            break;
        case FileDialog_SaveSprite:
            if (gui_debug_save_sprite(path, dialog_sprite_index))
                gui_notify(gui_NotificationSuccess, ICON_MD_IMAGE, "Sprite saved", path);
            else
                gui_notify(gui_NotificationError, NULL, "Unable to save sprite", path);
            break;
        case FileDialog_SaveLog:
            if (gui_debug_save_log(path))
                gui_notify(gui_NotificationSuccess, ICON_MD_DESCRIPTION, "Trace log saved", path);
            else
                gui_notify(gui_NotificationError, NULL, "Unable to save the trace log", path);
            break;
        case FileDialog_ChooseTracePath:
            gui_debug_trace_logger_set_output_directory(path);
            break;
        case FileDialog_SaveDisassemblerFull:
        case FileDialog_SaveDisassemblerVisible:
            if (gui_debug_save_disassembler(path, id == FileDialog_SaveDisassemblerFull))
                gui_notify(gui_NotificationSuccess, ICON_MD_CODE, "Disassembly saved", path);
            else
                gui_notify(gui_NotificationError, NULL, "Unable to save the disassembly", path);
            break;
        case FileDialog_LoadSymbols:
        {
            int skipped = 0;
            int count = gui_debug_load_symbols(path, &skipped);

            if (count >= 0)
            {
                char message[64];

                if (skipped > 0)
                    snprintf(message, sizeof(message), "%d symbols loaded, %d lines skipped", count, skipped);
                else
                    snprintf(message, sizeof(message), "%d symbols loaded", count);

                gui_notify(skipped > 0 ? gui_NotificationWarning : gui_NotificationSuccess, ICON_MD_LABEL, message, path);
            }
            else
                gui_notify(gui_NotificationError, NULL, "Unable to load symbols", path);

            break;
        }
        case FileDialog_SaveAllSprites:
            if (gui_debug_save_all_sprites(path))
                gui_notify(gui_NotificationSuccess, ICON_MD_PHOTO_LIBRARY, "Sprites saved", path);
            else
                gui_notify(gui_NotificationError, NULL, "Unable to save sprites", path);
            break;
        default:
            break;
    }
}
