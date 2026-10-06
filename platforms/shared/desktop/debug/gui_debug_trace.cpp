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

#define GUI_DEBUG_TRACE_IMPORT
#include "gui_debug_trace.h"

#include <stdio.h>
#include <string>
#include <time.h>
#include "imgui.h"
#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../gui_colors.h"
#include "../gui_filedialogs.h"
#include "../utils.h"
#include "gui_debug_constants.h"
#include "gui_debug_i386_tables.h"

static const u32 k_trace_capacities[4] = { 100000, 500000, 1000000, 2000000 };
static const u64 k_trace_disk_sizes[4] = { 10ULL << 20, 100ULL << 20, 1ULL << 30, 0 };
static const char* const k_trace_type_names[TRACE_TYPE_COUNT] = { "CPU", "INT", "I/O", "DMA", "CD", "FDC", "VID" };
static const char* const k_trace_filter_names[TRACE_TYPE_COUNT] =
{
    "Instructions", "Interrupts", "I/O Ports", "DMA", "CD-ROM", "FDC", "VSYNC"
};

static FILE* disk_file = NULL;
static std::string disk_path;
static u64 disk_bytes = 0;
static u64 disk_sequence = 0;
static bool disk_full = false;
static u64 lost_entries = 0;
static bool follow_latest = true;
static u32 last_count = 0;

static void trace_menu(void);
static void drain_disk(void);
static void write_entry(FILE* file, const GT_Trace_Entry& entry, u64& bytes);
static ImVec4 type_color(int type);
static const char* mode_name(u8 mode);

void gui_debug_window_trace_logger(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(100, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("Trace Logger", &config_debug.show_trace_logger, ImGuiWindowFlags_MenuBar);

    trace_menu();

    TraceLogger* logger = emu_get_core()->GetTraceLogger();
    bool running = gui_debug_trace_is_running();

    if (ImGui::Button(running ? "Stop" : "Start", ImVec2(60, 0)))
    {
        if (running)
            gui_debug_trace_stop();
        else
            gui_debug_trace_start();
    }

    ImGui::SameLine();

    if (ImGui::Button("Clear", ImVec2(60, 0)))
        logger->Clear();

    ImGui::SameLine();
    ImGui::Checkbox("Follow", &follow_latest);
    ImGui::SameLine();
    ImGui::PushFont(gui_default_font);

    if (config_debug.trace_output == 1 && (running || IsValidPointer(disk_file) || disk_bytes > 0))
        ImGui::TextColored(disk_full ? yellow : gray, "DISK %.1f MB%s", disk_bytes / 1048576.0, disk_full ? " LIMIT" : "");
    else
        ImGui::TextColored(gray, "%u / %u", logger->GetCount(), logger->GetCapacity());

    if (lost_entries > 0)
    {
        ImGui::SameLine();
        ImGui::TextColored(red, " %llu LOST", (unsigned long long)lost_entries);
    }

    ImGui::PopFont();
    ImGui::Separator();

    ImGui::BeginChild("##trace_lines", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushFont(gui_default_font);

    u32 count = config_debug.trace_output == 0 ? logger->GetCount() : 0;

    if (config_debug.trace_output == 1)
        ImGui::TextColored(gray, "Tracing to %s", disk_path.empty() ? "disk" : disk_path.c_str());

    ImGuiListClipper clipper;
    clipper.Begin((int)count);

    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            const GT_Trace_Entry& entry = logger->GetEntry((u32)i);
            char text[GUI_DEBUG_TRACE_TEXT_SIZE];
            gui_debug_trace_format(entry, text, sizeof(text), config_debug.trace_registers, false);

            if (config_debug.trace_cycles)
            {
                ImGui::TextColored(gray, "%12llu", (unsigned long long)entry.cycle);
                ImGui::SameLine();
            }

            ImGui::TextColored(type_color(entry.type), "%-3s", k_trace_type_names[entry.type % TRACE_TYPE_COUNT]);
            ImGui::SameLine();
            ImGui::TextUnformatted(text);
        }
    }

    if (follow_latest && count != last_count)
        ImGui::SetScrollHereY(1.0f);

    last_count = count;

    ImGui::PopFont();
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_trace_update(void)
{
    if (!IsValidPointer(emu_get_core()))
        return;

    if (gui_debug_trace_is_running() && (!config_debug.show_trace_logger || !config_debug.debug))
        gui_debug_trace_stop();

    if (IsValidPointer(disk_file))
        drain_disk();
}

bool gui_debug_trace_start(void)
{
    TraceLogger* logger = emu_get_core()->GetTraceLogger();
    u32 capacity = k_trace_capacities[CLAMP(config_debug.trace_capacity, 0, 3)];

    if (logger->GetCapacity() != capacity && !logger->SetCapacity(capacity))
    {
        gui_set_error_message("Not enough memory for the trace buffer");
        return false;
    }

    lost_entries = 0;
    disk_full = false;

    if (config_debug.trace_output == 1)
    {
        char name[64];
        time_t now = time(NULL);
        strftime(name, sizeof(name), "geartowns_trace_%Y%m%d_%H%M%S.txt", localtime(&now));
        disk_path = config_debug.trace_output_path.empty() ? std::string(config_root_path) : config_debug.trace_output_path;
        append_path_component(disk_path, name);
        disk_file = fopen_utf8(disk_path.c_str(), "w");

        if (!IsValidPointer(disk_file))
        {
            gui_set_error_message("Unable to create the trace file");
            return false;
        }

        disk_bytes = 0;
        disk_sequence = logger->GetSequence();
    }

    config_debug.show_trace_logger = true;
    logger->Start((u32)config_debug.trace_flags);
    return true;
}

void gui_debug_trace_stop(void)
{
    emu_get_core()->GetTraceLogger()->Stop();

    if (IsValidPointer(disk_file))
    {
        drain_disk();
        fclose(disk_file);
        InitPointer(disk_file);
    }
}

bool gui_debug_trace_is_running(void)
{
    return emu_get_core()->GetTraceLogger()->IsRunning();
}

const char* gui_debug_trace_get_disk_path(void)
{
    return disk_path.c_str();
}

u64 gui_debug_trace_get_disk_bytes(void)
{
    return disk_bytes;
}

const char* gui_debug_trace_type_name(int type)
{
    return k_trace_type_names[type % TRACE_TYPE_COUNT];
}

bool gui_debug_trace_save(const char* file_path)
{
    FILE* file = fopen_utf8(file_path, "w");

    if (!IsValidPointer(file))
        return false;

    TraceLogger* logger = emu_get_core()->GetTraceLogger();
    u64 bytes = 0;

    for (u32 i = 0; i < logger->GetCount(); i++)
        write_entry(file, logger->GetEntry(i), bytes);

    fclose(file);
    return true;
}

void gui_debug_trace_format(const GT_Trace_Entry& entry, char* text, size_t size, bool registers, bool cycles)
{
    char prefix[24] = "";

    if (cycles)
        snprintf(prefix, sizeof(prefix), "%12llu %-3s ", (unsigned long long)entry.cycle,
            k_trace_type_names[entry.type % TRACE_TYPE_COUNT]);

    switch (entry.type)
    {
        case TRACE_CPU:
        {
            char bytes[32] = "";
            char address[24];

            for (int i = 0; i < MIN((int)entry.cpu.size, 7); i++)
                snprintf(bytes + i * 3, sizeof(bytes) - i * 3, "%02X ", entry.cpu.bytes[i]);

            if (entry.cpu.size > 7)
                bytes[20] = '+';

            if (entry.cpu.mode == I386_MODE_PROTECTED)
                snprintf(address, sizeof(address), "%04X:%08X", entry.cpu.cs, entry.cpu.eip);
            else
                snprintf(address, sizeof(address), "%04X:%04X    ", entry.cpu.cs, entry.cpu.eip & 0xFFFF);

            int length = snprintf(text, size, "%s%s %08X %s %-21s %s", prefix, address, entry.cpu.linear,
                mode_name(entry.cpu.mode), bytes, entry.cpu.name[0] != 0 ? entry.cpu.name : "??");

            if (registers && length > 0 && (size_t)length < size)
            {
                const u32* r = entry.cpu.registers;
                snprintf(text + length, size - length,
                    "  EAX=%08X EBX=%08X ECX=%08X EDX=%08X ESI=%08X EDI=%08X EBP=%08X ESP=%08X EFL=%08X",
                    r[I386_REG_EAX], r[I386_REG_EBX], r[I386_REG_ECX], r[I386_REG_EDX], r[I386_REG_ESI],
                    r[I386_REG_EDI], r[I386_REG_EBP], r[I386_REG_ESP], entry.cpu.eflags);
            }

            break;
        }
        case TRACE_INTERRUPT:
        {
            char name[16];
            char description[64];
            gui_debug_i386_vector_name(entry.interrupt.vector, name, sizeof(name), description, sizeof(description));

            if (entry.event == TRACE_INTERRUPT_REQUEST)
                snprintf(text, size, "%sIRQ%d %s requested, vector %02X", prefix, entry.interrupt.line,
                    k_debug_irq_sources[entry.interrupt.line & 0x0F], entry.interrupt.vector);
            else
            {
                static const char* k_sources[4] = { "", "exception", "hardware", "software" };
                char error[24] = "";

                if (entry.interrupt.has_error_code)
                    snprintf(error, sizeof(error), " error %04X", entry.interrupt.error_code);

                snprintf(text, size, "%sINT %02X %s (%s)%s  %08X -> %08X", prefix, entry.interrupt.vector, description,
                    k_sources[entry.interrupt.source & 3], error, entry.interrupt.from, entry.interrupt.to);
            }

            break;
        }
        case TRACE_IO:
        {
            const char* label = gui_debug_port_label(entry.io.port);
            int digits = entry.io.size * 2;

            snprintf(text, size, "%s%-3s %04X %-16s %s %0*X  at %08X", prefix, entry.io.write ? "OUT" : "IN",
                entry.io.port, IsValidPointer(label) ? label : "", entry.io.write ? "<-" : "->", digits,
                entry.io.value & (digits == 8 ? 0xFFFFFFFFU : (1U << (digits * 4)) - 1), entry.io.pc);
            break;
        }
        case TRACE_DMA:
            if (entry.event == TRACE_DMA_REQUEST)
                snprintf(text, size, "%sDMA%d %s request  address %08X count %04X mode %02X", prefix, entry.dma.channel,
                    k_debug_dma_devices[entry.dma.channel & 3], entry.dma.address, entry.dma.count, entry.dma.mode);
            else
                snprintf(text, size, "%sDMA%d %s end%s  address %08X count %04X", prefix, entry.dma.channel,
                    k_debug_dma_devices[entry.dma.channel & 3], entry.dma.terminal ? " TC" : "", entry.dma.address,
                    entry.dma.count);

            break;
        case TRACE_CDROM:
        {
            const u8* b = entry.cdrom.bytes;

            if (entry.event == TRACE_CDROM_COMMAND)
                snprintf(text, size, "%sCD-ROM command %02X %s%s%s  %02X %02X %02X %02X %02X %02X %02X %02X", prefix,
                    entry.cdrom.command, gui_debug_cdrom_command_name(entry.cdrom.command & k_cdrom_command_mask),
                    (entry.cdrom.command & k_cdrom_flag_irq) ? " +IRQ" : "", (entry.cdrom.command & k_cdrom_flag_status) ?
                    " +STATUS" : "", b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
            else
                snprintf(text, size, "%sCD-ROM status %02X %02X %02X %02X", prefix, b[0], b[1], b[2], b[3]);

            break;
        }
        case TRACE_FDC:
        {
            char command[48];
            gui_debug_mb8877_command(entry.fdc.command, command, sizeof(command));

            if (entry.event == TRACE_FDC_COMMAND)
                snprintf(text, size, "%sFDC command %02X %s  drive %d track %02X sector %02X data %02X", prefix,
                    entry.fdc.command, command, entry.fdc.drive, entry.fdc.track, entry.fdc.sector, entry.fdc.data);
            else
                snprintf(text, size, "%sFDC end %s  status %02X track %02X sector %02X", prefix, command,
                    entry.fdc.status, entry.fdc.track, entry.fdc.sector);

            break;
        }
        default:
            snprintf(text, size, "%sVSYNC frame %u", prefix, entry.video.frame);
            break;
    }
}

static void trace_menu(void)
{
    if (!ImGui::BeginMenuBar())
        return;

    bool running = gui_debug_trace_is_running();

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Save Trace As...", NULL, false, config_debug.trace_output == 0))
            gui_file_dialog_save_trace();

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Events"))
    {
        for (int i = 0; i < TRACE_TYPE_COUNT; i++)
        {
            bool enabled = (config_debug.trace_flags & (1 << i)) != 0;

            if (ImGui::MenuItem(k_trace_filter_names[i], NULL, &enabled, !running))
                config_debug.trace_flags = enabled ? (config_debug.trace_flags | (1 << i)) : (config_debug.trace_flags & ~(1 << i));
        }

        if (running)
            ImGui::TextDisabled("Stop tracing to change the events");

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("Registers", NULL, &config_debug.trace_registers);
        ImGui::MenuItem("Cycles", NULL, &config_debug.trace_cycles);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Output"))
    {
        if (ImGui::MenuItem("Memory", NULL, config_debug.trace_output == 0, !running))
            config_debug.trace_output = 0;

        if (ImGui::MenuItem("Disk", NULL, config_debug.trace_output == 1, !running))
            config_debug.trace_output = 1;

        ImGui::Separator();

        if (ImGui::BeginMenu("Memory Size", !running))
        {
            static const char* k_names[4] = { "100K Entries", "500K Entries", "1M Entries", "2M Entries" };

            for (int i = 0; i < 4; i++)
            {
                if (ImGui::MenuItem(k_names[i], NULL, config_debug.trace_capacity == i))
                    config_debug.trace_capacity = i;
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Disk Size", !running))
        {
            static const char* k_names[4] = { "10 MB", "100 MB", "1 GB", "Unbounded" };

            for (int i = 0; i < 4; i++)
            {
                if (ImGui::MenuItem(k_names[i], NULL, config_debug.trace_disk_size == i))
                    config_debug.trace_disk_size = i;
            }

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Choose Disk Folder...", NULL, false, !running))
            gui_file_dialog_choose_trace_path();

        ImGui::TextDisabled("%s", config_debug.trace_output_path.empty() ? config_root_path :
            config_debug.trace_output_path.c_str());
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

static void drain_disk(void)
{
    TraceLogger* logger = emu_get_core()->GetTraceLogger();
    u64 sequence = logger->GetSequence();
    u64 pending = sequence - disk_sequence;
    u64 limit = k_trace_disk_sizes[CLAMP(config_debug.trace_disk_size, 0, 3)];

    if (pending == 0 || disk_full)
    {
        disk_sequence = sequence;
        return;
    }

    if (pending > logger->GetCount())
    {
        lost_entries += pending - logger->GetCount();
        pending = logger->GetCount();
    }

    u32 first = logger->GetCount() - (u32)pending;

    for (u32 i = 0; i < (u32)pending; i++)
    {
        if (limit != 0 && disk_bytes >= limit)
        {
            disk_full = true;
            break;
        }

        write_entry(disk_file, logger->GetEntry(first + i), disk_bytes);
    }

    disk_sequence = sequence;
    fflush(disk_file);
}

static void write_entry(FILE* file, const GT_Trace_Entry& entry, u64& bytes)
{
    char text[GUI_DEBUG_TRACE_TEXT_SIZE];
    gui_debug_trace_format(entry, text, sizeof(text), config_debug.trace_registers, true);
    int length = fprintf(file, "%s\n", text);

    if (length > 0)
        bytes += (u64)length;
}

static ImVec4 type_color(int type)
{
    switch (type)
    {
        case TRACE_CPU: return cyan;
        case TRACE_INTERRUPT: return yellow;
        case TRACE_IO: return orange;
        case TRACE_DMA: return green;
        case TRACE_CDROM: return violet;
        case TRACE_FDC: return blue;
        default: return magenta;
    }
}

static const char* mode_name(u8 mode)
{
    return mode == I386_MODE_PROTECTED ? "P" : mode == I386_MODE_VM86 ? "V" : "R";
}
