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
#include "gui.h"
#include "config.h"
#include "emu.h"
#include "geartowns.h"
#include "application.h"
#include "display.h"
#include "utils.h"

void gui_action_reset(void)
{
    gui_set_status_message("Resetting...", 3000);

    emu_resume();
    emu_reset();

    if (config_emulator.start_paused)
        emu_pause();
}

void gui_action_reload_rom(void)
{
    if (!emu_is_empty())
    {
        char rom_path[4096];
        strncpy_fit(rom_path, emu_get_core()->GetMedia()->GetFilePath(), sizeof(rom_path));
        gui_load_rom(rom_path);
    }
}

void gui_action_pause(void)
{
    if (emu_is_paused())
    {
        gui_set_status_message("Resumed", 3000);
        emu_resume();
    }
    else
    {
        gui_set_status_message("Paused", 3000);
        emu_pause();
    }
}

void gui_action_ffwd(void)
{
    config_audio.sync = !config_emulator.ffwd;

    if (config_emulator.ffwd)
    {
        gui_set_status_message("Fast Forward ON", 3000);
        display_disable_vsync();
    }
    else
    {
        gui_set_status_message("Fast Forward OFF", 3000);
        display_use_vsync_if_enabled();
        emu_audio_reset();
    }
}

void gui_action_save_screenshot(const char* path)
{
    using namespace std;

    if (emu_is_empty() || !emu_get_core()->GetMedia()->IsReady())
        return;

    time_t now = time(0);
    tm ltm;

    char date_time_buffer[32] = {};
    if (get_local_time(now, &ltm))
        strftime(date_time_buffer, sizeof(date_time_buffer), "%Y-%m-%d %H%M%S", &ltm);
    else
        snprintf(date_time_buffer, sizeof(date_time_buffer), "screenshot");
    string date_time = date_time_buffer;

    string file_path;

    if (path != NULL && path[0])
    {
        file_path = path;
        if (file_path.find_last_of(".") == string::npos)
            file_path += ".png";
    }
    else
    {
        const char* directory = config_emulator.screenshots_path.empty() ? config_root_path : config_emulator.screenshots_path.c_str();
        file_path = directory;
        string file_name = emu_get_core()->GetMedia()->GetFileName();
        file_name += " - ";
        file_name += date_time;
        file_name += ".png";
        append_path_component(file_path, file_name.c_str());
    }

    emu_save_screenshot(file_path.c_str());

    string message = "Screenshot saved to " + file_path;
    gui_set_status_message(message.c_str(), 3000);
}
