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

#define GUI_DEBUG_IMPORT
#include "gui_debug.h"

#include <fstream>
#include <string>
#include "config.h"
#include "emu.h"
#include "gui_debug_memory.h"

static const char* GTDEBUG_MAGIC = "GTDEBUG1";
static const int GTDEBUG_MAGIC_SIZE = 8;

static std::string get_auto_debug_settings_path(void);

void gui_debug_init(void)
{
    gui_debug_memory_init();
}

void gui_debug_destroy(void)
{
    gui_debug_memory_destroy();
}

void gui_debug_reset(void)
{
    gui_debug_memory_reset();
    gui_debug_reset_symbols();
}

void gui_debug_update(void)
{
    gui_debug_memory_update();
}

void gui_debug_windows(void)
{
    gui_debug_update();

    if (config_debug.debug)
    {
        if (config_debug.show_memory)
            gui_debug_window_memory();
        gui_debug_memory_auxiliary_windows();
    }
}

void gui_debug_reset_symbols(void)
{
}

bool gui_debug_load_symbols_file(const char* file_path)
{
    UNUSED(file_path);
    return false;
}

void gui_debug_save_settings(const char* file_path)
{
    if (!IsValidPointer(file_path))
        return;

    std::ofstream file;
    open_ofstream_utf8(file, file_path, std::ios::binary);
    if (!file.is_open())
        return;

    file.write(GTDEBUG_MAGIC, GTDEBUG_MAGIC_SIZE);
    gui_debug_memory_save_settings(file);
    file.close();
}

void gui_debug_load_settings(const char* file_path)
{
    if (!IsValidPointer(file_path))
        return;

    std::ifstream file;
    open_ifstream_utf8(file, file_path, std::ios::binary);
    if (!file.is_open())
        return;

    char magic[GTDEBUG_MAGIC_SIZE];
    file.read(magic, sizeof(magic));
    if (file.fail() || memcmp(magic, GTDEBUG_MAGIC, sizeof(magic)) != 0 ||
        !gui_debug_memory_load_settings(file))
    {
        Log("Invalid debug settings file: %s", file_path);
        file.close();
        return;
    }
    file.close();
    Log("Debug settings loaded from: %s", file_path);
}

void gui_debug_auto_save_settings(void)
{
    if (!config_debug.auto_debug_settings)
        return;
    std::string path = get_auto_debug_settings_path();
    if (!path.empty())
        gui_debug_save_settings(path.c_str());
}

void gui_debug_auto_load_settings(void)
{
    if (!config_debug.auto_debug_settings)
        return;
    std::string path = get_auto_debug_settings_path();
    if (path.empty())
        return;

    std::ifstream file;
    open_ifstream_utf8(file, path.c_str(), std::ios::binary);
    if (!file.is_open())
        return;
    file.close();
    gui_debug_load_settings(path.c_str());
}

static std::string get_auto_debug_settings_path(void)
{
    GeartownsCore* core = emu_get_core();
    if (!IsValidPointer(core) || !IsValidPointer(core->GetMedia()) ||
        !core->GetMedia()->IsReady())
        return "";

    std::string filename = core->GetMedia()->GetFileName();
    std::string::size_type dot = filename.find_last_of('.');
    if (dot != std::string::npos)
        filename.resize(dot);
    filename += ".gtdebug";

    std::string path = config_root_path;
    append_path_component(path, filename.c_str());
    return path;
}
