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
#include <vector>
#include "../config.h"
#include "../emu.h"
#include "gui_debug_audio.h"
#include "gui_debug_cdrom.h"
#include "gui_debug_disassembler.h"
#include "gui_debug_framebuffers.h"
#include "gui_debug_floppy.h"
#include "gui_debug_i386.h"
#include "gui_debug_i386_tables.h"
#include "gui_debug_input.h"
#include "gui_debug_memory.h"
#include "gui_debug_profiler.h"
#include "gui_debug_rewind.h"
#include "gui_debug_system.h"
#include "gui_debug_trace.h"
#include "gui_debug_video.h"

static const char* GTDEBUG_MAGIC = "GTDEBUG1";
static const int GTDEBUG_MAGIC_SIZE = 8;

static std::string get_auto_debug_settings_path(void);
static bool read_settings_data(std::istream& stream, void* data, size_t size);
static bool read_settings_count(std::istream& stream, int& count, size_t record_size);

void gui_debug_init(void)
{
    gui_debug_audio_init();
    gui_debug_disassembler_init();
    gui_debug_memory_init();
}

void gui_debug_destroy(void)
{
    gui_debug_audio_destroy();
    gui_debug_disassembler_destroy();
    gui_debug_memory_destroy();
}

void gui_debug_reset(void)
{
    gui_debug_disassembler_reset();
    gui_debug_memory_reset();
    gui_debug_reset_breakpoints();
    gui_debug_reset_symbols();
    gui_debug_reset_disassembler_bookmarks();
}

void gui_debug_update(void)
{
    gui_debug_memory_update();
}

void gui_debug_windows(void)
{
    gui_debug_update();
    gui_debug_trace_update();
    gui_debug_profiler_update();

    if (config_debug.debug)
    {
        if (config_debug.show_trace_logger)
            gui_debug_window_trace_logger();

        if (config_debug.show_profiler)
            gui_debug_window_profiler();

        if (config_debug.show_processor)
            gui_debug_window_i386();

        if (config_debug.show_processor_details)
            gui_debug_window_i386_details();

        if (config_debug.show_memory)
            gui_debug_window_memory();

        if (config_debug.show_disassembler)
            gui_debug_window_disassembler();

        if (config_debug.show_i386_descriptors)
            gui_debug_window_descriptor_tables();

        if (config_debug.show_i386_paging)
            gui_debug_window_paging();

        if (config_debug.show_call_stack)
            gui_debug_window_call_stack();

        if (config_debug.show_breakpoints)
            gui_debug_window_breakpoints();

        if (config_debug.show_symbols)
            gui_debug_window_symbols();

        if (config_debug.show_rewind)
            gui_debug_window_rewind();

        if (config_debug.show_pic)
            gui_debug_window_pic();

        if (config_debug.show_pit)
            gui_debug_window_pit();

        if (config_debug.show_dma)
            gui_debug_window_dma();

        if (config_debug.show_rtc)
            gui_debug_window_rtc();

        if (config_debug.show_system_control)
            gui_debug_window_system_control();

        if (config_debug.show_keyboard)
            gui_debug_window_keyboard();

        if (config_debug.show_crtc)
            gui_debug_window_crtc();

        if (config_debug.show_crtc_registers)
            gui_debug_window_crtc_registers();

        if (config_debug.show_video_output)
            gui_debug_window_video_output();

        if (config_debug.show_palettes)
            gui_debug_window_palettes();

        if (config_debug.show_framebuffers)
            gui_debug_window_framebuffers();

        if (config_debug.show_sprites)
            gui_debug_window_sprites();

        if (config_debug.show_ym3438)
            gui_debug_window_ym3438();

        if (config_debug.show_ym3438_registers)
            gui_debug_window_ym3438_registers();

        if (config_debug.show_rf5c68)
            gui_debug_window_rf5c68();

        if (config_debug.show_sound_control)
            gui_debug_window_sound_control();

        if (config_debug.show_cdrom)
            gui_debug_window_cdrom();

        if (config_debug.show_cdrom_toc)
            gui_debug_window_cdrom_toc();

        if (config_debug.show_cdrom_audio)
            gui_debug_window_cdrom_audio();

        if (config_debug.show_fdc)
            gui_debug_window_fdc();

        if (config_debug.show_floppy_drives)
            gui_debug_window_floppy_drives();

        if (config_debug.show_disk_viewer)
            gui_debug_window_disk_viewer();

        gui_debug_memory_auxiliary_windows();
    }
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

    std::vector<I386_Breakpoint>* breakpoints = emu_get_core()->GetI386()->GetBreakpoints();
    int breakpoint_count = (int)breakpoints->size();
    file.write((const char*)&breakpoint_count, sizeof(breakpoint_count));

    for (int i = 0; i < breakpoint_count; i++)
    {
        const I386_Breakpoint& breakpoint = (*breakpoints)[i];
        file.write((const char*)&breakpoint, sizeof(breakpoint));
    }

    std::vector<I386_Interrupt_Breakpoint>* interrupts = emu_get_core()->GetI386()->GetInterruptBreakpoints();
    int interrupt_count = (int)interrupts->size();
    file.write((const char*)&interrupt_count, sizeof(interrupt_count));

    for (int i = 0; i < interrupt_count; i++)
    {
        const I386_Interrupt_Breakpoint& breakpoint = (*interrupts)[i];
        file.write((const char*)&breakpoint, sizeof(breakpoint));
    }

    std::vector<DisassemblerBookmark>* bookmarks = gui_debug_get_disassembler_bookmarks();
    int bookmark_count = (int)bookmarks->size();
    file.write((const char*)&bookmark_count, sizeof(bookmark_count));

    for (int i = 0; i < bookmark_count; i++)
    {
        const DisassemblerBookmark& bookmark = (*bookmarks)[i];
        file.write((const char*)&bookmark, sizeof(bookmark));
    }

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
    int breakpoint_count = 0;
    int interrupt_count = 0;
    int bookmark_count = 0;
    std::vector<I386_Breakpoint> breakpoints;
    std::vector<I386_Interrupt_Breakpoint> interrupts;
    std::vector<DisassemblerBookmark> bookmarks;
    bool valid = read_settings_data(file, magic, sizeof(magic)) && memcmp(magic, GTDEBUG_MAGIC, sizeof(magic)) == 0 &&
        read_settings_count(file, breakpoint_count, sizeof(I386_Breakpoint));

    if (valid)
    {
        breakpoints.resize(breakpoint_count);

        for (int i = 0; valid && i < breakpoint_count; i++)
        {
            valid = read_settings_data(file, &breakpoints[i], sizeof(breakpoints[i])) && breakpoints[i].type != 0 &&
                (breakpoints[i].type & ~(I386_BREAKPOINT_EXECUTE | I386_BREAKPOINT_READ | I386_BREAKPOINT_WRITE)) == 0 &&
                breakpoints[i].space < I386_BREAKPOINT_SPACE_COUNT;
        }
    }

    valid = valid && read_settings_count(file, interrupt_count, sizeof(I386_Interrupt_Breakpoint));

    if (valid)
    {
        interrupts.resize(interrupt_count);

        for (int i = 0; valid && i < interrupt_count; i++)
        {
            valid = read_settings_data(file, &interrupts[i], sizeof(interrupts[i])) &&
                interrupts[i].source < I386_INTERRUPT_SOURCE_COUNT;
        }
    }

    valid = valid && read_settings_count(file, bookmark_count, sizeof(DisassemblerBookmark));

    if (valid)
    {
        bookmarks.resize(bookmark_count);

        for (int i = 0; valid && i < bookmark_count; i++)
        {
            valid = read_settings_data(file, &bookmarks[i], sizeof(bookmarks[i]));
            bookmarks[i].name[sizeof(bookmarks[i].name) - 1] = 0;
        }
    }

    if (!valid || !gui_debug_memory_load_settings(file))
    {
        Log("Invalid debug settings file: %s", file_path);
        file.close();
        return;
    }

    file.close();

    *emu_get_core()->GetI386()->GetBreakpoints() = breakpoints;
    *emu_get_core()->GetI386()->GetInterruptBreakpoints() = interrupts;
    *gui_debug_get_disassembler_bookmarks() = bookmarks;

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

static bool read_settings_data(std::istream& stream, void* data, size_t size)
{
    stream.read((char*)data, (std::streamsize)size);
    return !stream.fail() && stream.gcount() == (std::streamsize)size;
}

static bool read_settings_count(std::istream& stream, int& count, size_t record_size)
{
    if (!read_settings_data(stream, &count, sizeof(count)) || count < 0 || count > 0x10000)
        return false;

    std::streampos position = stream.tellg();
    stream.seekg(0, std::ios::end);
    std::streampos end = stream.tellg();
    stream.seekg(position);

    return !stream.fail() && end >= position && (u64)count <= (u64)(end - position) / record_size;
}

static std::string get_auto_debug_settings_path(void)
{
    GeartownsCore* core = emu_get_core();

    if (!IsValidPointer(core) || !IsValidPointer(core->GetMedia()) || !core->GetMedia()->IsReady())
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
