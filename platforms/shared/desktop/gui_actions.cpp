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
#include "events.h"
#include "gui.h"
#include "rewind.h"
#include "utils.h"

void gui_action_reset(void)
{
    if (emu_is_empty())
        return;

    gui_set_status_message("Resetting...", 3000);
    emu_resume();
    emu_reset();
    if (config_emulator.start_paused)
        emu_pause();
}

void gui_action_reload_rom(void)
{
    if (emu_is_empty())
        return;

    char media_path[GT_MAX_PATH];
    strncpy_fit(media_path, emu_get_core()->GetMedia()->GetFilePath(),
        sizeof(media_path));
    gui_load_rom(media_path);
}

void gui_action_pause(void)
{
    if (emu_is_empty())
        return;

    if (emu_is_paused())
    {
        emu_resume();
        gui_set_status_message("Resumed", 1500);
    }
    else
    {
        emu_pause();
        gui_set_status_message("Paused", 1500);
    }
}

void gui_action_ffwd(void)
{
    config_audio.sync = !config_emulator.ffwd;

    if (config_emulator.ffwd)
    {
        display_disable_vsync();
        gui_set_status_message("Fast Forward ON", 1500);
    }
    else
    {
        display_use_vsync_if_enabled();
        emu_audio_reset();
        gui_set_status_message("Fast Forward OFF", 1500);
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
    gui_set_status_message("Rewinding...", 500);
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

    time_t now = time(NULL);
    struct tm time_info;
    char date_time[32] = { };
    if (get_local_time(now, &time_info))
        strftime(date_time, sizeof(date_time), "%Y-%m-%d %H%M%S", &time_info);

    std::string file_path;
    if (IsValidPointer(path) && path[0] != '\0')
    {
        file_path = path;
        if (file_path.find_last_of('.') == std::string::npos)
            file_path += ".png";
    }
    else
    {
        const char* base_path = config_root_path;
        const char* media_name = "Geartowns";

        if (!emu_is_empty())
        {
            media_name = emu_get_core()->GetMedia()->GetFileName();
            if (config_emulator.screenshots_dir_option == Directory_Location_ROM &&
                emu_get_core()->GetMedia()->GetFileDirectory()[0] != '\0')
            {
                base_path = emu_get_core()->GetMedia()->GetFileDirectory();
            }
            else if (config_emulator.screenshots_dir_option ==
                Directory_Location_Custom)
            {
                base_path = config_emulator.screenshots_path.c_str();
            }
        }

        file_path = base_path;
        append_path_component(file_path, media_name);
        file_path += " - ";
        file_path += date_time;
        file_path += ".png";
    }

    if (emu_save_screenshot(file_path.c_str()))
    {
        std::string message = "Screenshot saved to ";
        message += file_path;
        gui_set_status_message(message.c_str(), 3000);
    }
    else
    {
        std::string message = "Unable to save screenshot to ";
        message += file_path;
        gui_set_error_message(message.c_str());
    }
}
