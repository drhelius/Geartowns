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

#define GUI_ACTIONS_IMPORT
#include "gui_actions.h"

#include <string>
#include <time.h>
#include "application.h"
#include "config.h"
#include "display.h"
#include "emu.h"
#include "ogl_renderer.h"
#include "events.h"
#include "gui.h"
#include "gui_notifications.h"
#include "debug/gui_debug.h"
#include "debug/gui_debug_trace_logger.h"
#include "rewind.h"
#include "utils.h"
#include "video_recorder.h"

static std::string get_auto_file_path(int dir_option, const std::string& custom_path, const char* extension);

void gui_action_load_defaults(void)
{
    if (gui_is_rom_loading() || emu_is_media_loading())
        return;

    if (!gui_debug_trace_logger_stop())
        return;

    emu_stop_video_recording();

    GeartownsCore* core = emu_get_core();
    gui_debug_auto_save_settings();
    core->EjectMedia();
    application_update_title_with_rom(NULL);
    core->UnloadBios();

    config_load_defaults();
    gui_apply_settings();

    emu_resume();
    emu_reset();

    gui_debug_reset();
    gui_debug_trace_logger_init();
    update_savestates_data();
    events_sync_input();
    ogl_renderer_unload_shader_preset();
    application_apply_settings();

    config_write();
    gui_notify(gui_NotificationSuccess, ICON_MD_SETTINGS_BACKUP_RESTORE, "Default settings restored");
}

void gui_action_power_on(void)
{
    if (!emu_is_empty() || gui_is_rom_loading())
        return;

    emu_resume();

    if (!emu_power_on())
    {
        gui_notify(gui_NotificationError, NULL, "Unable to power on", "Firmware is not loaded");
        return;
    }

    gui_notify(gui_NotificationInfo, ICON_MD_POWER_SETTINGS_NEW, "Powered on", NULL, "power");

    if (config_emulator.start_paused)
        emu_pause();
}

void gui_action_power_off(void)
{
    if (emu_is_empty())
        return;

    emu_resume();
    emu_power_off();
    gui_notify(gui_NotificationInfo, ICON_MD_POWER_SETTINGS_NEW, "Powered off", NULL, "power");
}

void gui_action_reset(void)
{
    if (emu_is_empty())
        return;

    gui_notify(gui_NotificationInfo, ICON_MD_REFRESH, "Reset");
    gui_debug_trace_logger_clear();
    emu_resume();
    emu_reset();

    if (config_emulator.start_paused)
        emu_pause();
}

void gui_action_reload_rom(void)
{
    if (!config_debug.debug || emu_is_empty())
        return;

    gui_reload_cdrom();
}

void gui_action_eject_media(void)
{
    Media* media = emu_get_core()->GetMedia();

    if (!media->IsReady())
        return;

    bool ejected = emu_eject_media();

    if (!media->IsReady())
        application_update_title_with_rom(NULL);

    if (ejected)
        gui_notify(gui_NotificationInfo, ICON_MD_EJECT, "CD-ROM ejected");
    else
        gui_notify(gui_NotificationError, NULL, "Unable to eject CD-ROM");
}

void gui_action_pause(void)
{
    if (emu_is_empty())
        return;

    if (emu_is_paused())
    {
        emu_resume();
        gui_notify(gui_NotificationInfo, ICON_MD_PLAY_ARROW, "Resumed", NULL, "pause", 1500);
    }
    else
    {
        emu_pause();
        gui_notify(gui_NotificationInfo, ICON_MD_PAUSE, "Paused", NULL, "pause", 1500);
    }
}

void gui_action_ffwd(void)
{
    config_audio.sync = !config_emulator.ffwd;

    if (config_emulator.ffwd)
    {
        display_disable_vsync();
        gui_notify(gui_NotificationInfo, ICON_MD_FAST_FORWARD, "Fast forward on", NULL, "ffwd", 1500);
    }
    else
    {
        display_use_vsync_if_enabled();
        emu_audio_reset();
        gui_notify(gui_NotificationInfo, ICON_MD_FAST_FORWARD, "Fast forward off", NULL, "ffwd", 1500);
    }
}

void gui_action_rewind_pressed(void)
{
    if (emu_is_empty() || !config_rewind.enabled)
        return;

    if (rewind_get_snapshot_count() < 1 || rewind_is_active())
        return;

    emu_reset_rewind_timing();
    rewind_set_active(true);
    display_use_vsync_if_enabled();
    gui_notify(gui_NotificationInfo, ICON_MD_FAST_REWIND, "Rewinding", NULL, "rewind", 500);
}

void gui_action_rewind_released(void)
{
    if (!rewind_is_active())
        return;

    rewind_set_active(false);
    events_sync_input();
    emu_reset_rewind_timing();

    if (config_emulator.ffwd)
        display_disable_vsync();
    else
        display_use_vsync_if_enabled();

    emu_audio_reset();
}

void gui_action_save_screenshot(const char* path)
{
    if (!IsValidPointer(emu_frame_buffer))
        return;

    std::string file_path;

    if (IsValidPointer(path) && path[0] != '\0')
    {
        file_path = path;

        if (file_path.find_last_of('.') == std::string::npos)
            file_path += ".png";
    }
    else
        file_path = get_auto_file_path(config_emulator.screenshots_dir_option, config_emulator.screenshots_path, ".png");

    if (emu_save_screenshot(file_path.c_str()))
        gui_notify(gui_NotificationSuccess, ICON_MD_PHOTO_CAMERA, "Screenshot saved", file_path.c_str());
    else
        gui_notify(gui_NotificationError, NULL, "Unable to save screenshot", file_path.c_str());
}

bool gui_action_start_video_recording(const char* path)
{
    if (emu_is_empty())
        return false;

    std::string file_path;

    if (IsValidPointer(path) && path[0] != '\0')
        file_path = path;
    else
        file_path = get_auto_file_path(config_emulator.video_recordings_dir_option, config_emulator.video_recordings_path,
            ".avi");

    if (!emu_start_video_recording(file_path.c_str()))
    {
        gui_notify(gui_NotificationError, NULL, "Unable to start video recording", file_path.c_str());
        return false;
    }

    gui_notify(gui_NotificationInfo, ICON_MD_FIBER_MANUAL_RECORD, "Recording video", file_path.c_str(), "video");
    return true;
}

void gui_action_stop_video_recording(void)
{
    if (!emu_is_video_recording())
        return;

    std::string file_path = video_recorder_get_file_path();
    emu_stop_video_recording();
    gui_notify(gui_NotificationSuccess, ICON_MD_VIDEOCAM, "Video saved", file_path.c_str(), "video");
}

void gui_action_toggle_video_recording(void)
{
    if (emu_is_video_recording())
        gui_action_stop_video_recording();
    else
        gui_action_start_video_recording(NULL);
}

void gui_action_save_state(const char* path)
{
    if (emu_is_empty())
        return;

    if (IsValidPointer(path) && path[0] != '\0')
    {
        if (emu_save_state_file(path))
            gui_notify(gui_NotificationSuccess, ICON_MD_SAVE, "State saved", path);
        else
            gui_notify(gui_NotificationError, NULL, "Unable to save state", path);

        return;
    }

    int slot = config_emulator.save_slot + 1;
    char message[64];

    if (emu_save_state_slot(slot))
    {
        snprintf(message, sizeof(message), "State saved to slot %d", slot);
        gui_notify(gui_NotificationSuccess, ICON_MD_SAVE, message);
    }
    else
    {
        snprintf(message, sizeof(message), "Unable to save state to slot %d", slot);
        gui_notify(gui_NotificationError, NULL, message);
    }
}

void gui_action_load_state(const char* path)
{
    if (emu_is_empty())
        return;

    if (IsValidPointer(path) && path[0] != '\0')
    {
        if (emu_load_state_file(path))
            gui_notify(gui_NotificationSuccess, ICON_MD_RESTORE, "State loaded", path);
        else
            gui_notify(gui_NotificationError, NULL, "Unable to load state", path);

        return;
    }

    int slot = config_emulator.save_slot + 1;
    char message[64];

    if (emu_load_state_slot(slot))
    {
        snprintf(message, sizeof(message), "State loaded from slot %d", slot);
        gui_notify(gui_NotificationSuccess, ICON_MD_RESTORE, message);
    }
    else
    {
        snprintf(message, sizeof(message), "Unable to load state from slot %d", slot);
        gui_notify(gui_NotificationError, NULL, message);
    }
}

static std::string get_auto_file_path(int dir_option, const std::string& custom_path, const char* extension)
{
    time_t now = time(NULL);
    struct tm time_info;
    char date_time[32] = { };

    if (get_local_time(now, &time_info))
        strftime(date_time, sizeof(date_time), "%Y-%m-%d %H%M%S", &time_info);

    const char* base_path = config_root_path;
    const char* media_name = "Geartowns";

    if (!emu_is_empty())
    {
        media_name = emu_get_core()->GetMedia()->GetFileName();

        if (dir_option == Directory_Location_ROM &&
            emu_get_core()->GetMedia()->GetFileDirectory()[0] != '\0')
        {
            base_path = emu_get_core()->GetMedia()->GetFileDirectory();
        }
        else if (dir_option == Directory_Location_Custom)
        {
            base_path = custom_path.c_str();
        }
    }

    std::string file_path = base_path;
    append_path_component(file_path, media_name);
    file_path += " - ";
    file_path += date_time;
    file_path += extension;
    return file_path;
}
