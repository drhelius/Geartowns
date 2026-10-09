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

#define GUI_DEBUG_TRACE_LOGGER_IMPORT
#include "gui_debug_trace_logger.h"

#include <cstring>
#include <time.h>
#include "imgui.h"
#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../gui_colors.h"
#include "../gui_filedialogs.h"
#include "../gui_notifications.h"
#include "../utils.h"
#include "gui_debug_disassembler.h"
#include "trace_logger_formatter.h"

#define TRACE_DISK_BUFFER_SIZE (1024 * 1024)
#define TRACE_DISK_STAGING_CAPACITY 500000

static bool trace_logger_enabled = false;
static bool trace_logger_follow_latest = true;
static bool trace_logger_scroll_to_bottom = false;
static bool trace_logger_wait_for_scroll_away = false;
static bool trace_logger_choose_output_path = false;
static FILE* trace_logger_disk_file = NULL;
static char trace_logger_disk_path[4096] = {};
static char trace_logger_disk_directory[4096] = {};
static char trace_logger_disk_buffer[TRACE_DISK_BUFFER_SIZE];
static size_t trace_logger_disk_buffer_used = 0;
static u64 trace_logger_disk_entries = 0;
static u64 trace_logger_disk_flushed_total = 0;
static u64 trace_logger_disk_bytes = 0;
static u64 trace_logger_disk_previous_cycle = 0;
static bool trace_logger_disk_previous_cycle_valid = false;
static bool trace_logger_disk_limit_reached = false;
static bool trace_logger_disk_overflow = false;
static bool trace_logger_disk_error = false;
static Uint64 trace_logger_disk_last_flush = 0;

static const u32 k_trace_logger_capacities[] = { 100000, 500000, 1000000, 2000000, 5000000 };
static const char* const k_trace_logger_capacity_names[] = { "100K", "500K", "1M", "2M", "5M" };
static const char* const k_trace_logger_capacity_labels[] =
{
    "100K (14 MB)", "500K (69 MB)", "1M (137 MB)", "2M (275 MB)", "5M (687 MB)"
};
static const char* const k_trace_logger_disk_size_names[] =
{
    "10MB", "50MB", "100MB", "250MB", "500MB", "1GB", "unbounded"
};
static const u64 k_trace_logger_disk_sizes[] =
{
    10ULL * 1024ULL * 1024ULL,
    50ULL * 1024ULL * 1024ULL,
    100ULL * 1024ULL * 1024ULL,
    250ULL * 1024ULL * 1024ULL,
    500ULL * 1024ULL * 1024ULL,
    1024ULL * 1024ULL * 1024ULL,
    0
};

static void trace_logger_menu(void);
static void trace_logger_menu_filters(void);
static void trace_logger_sync_flags(void);
static void trace_logger_sync_vblank_watch(bool enabled);
static u32 trace_logger_get_config_flags(void);
static void trace_logger_set_config_flags(u32 flags);
static int* trace_logger_get_config_event_filter(GT_Trace_Type type);
static void trace_logger_menu_event_filter(const char* label, int* filter, u32 mask);
static bool trace_logger_apply_capacity(void);
static bool trace_logger_start_disk(void);
static bool trace_logger_start(u32 flags, bool update_config);
static bool trace_logger_stop(bool show_status);
static bool trace_logger_stop_disk(bool show_status, bool flush_entries);
static bool trace_logger_flush_disk_buffer(bool flush_file);
static bool trace_logger_flush_disk_entries(void);
static void format_entry_text(const GT_Trace_Entry& entry, bool cycles, bool previous_cycle_valid, u64 previous_cycle,
    char* buf, int buf_size);
static void render_cpu_entry_colored(const GT_Trace_Entry& entry, int prefix_length);
static void render_entry_colored(const GT_Trace_Entry& entry, u64 index, bool previous_cycle_valid, u64 previous_cycle);

void gui_debug_window_trace_logger(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(340, 168), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(544, 362), ImGuiCond_FirstUseEver);

    ImGui::Begin("Trace Logger", &config_debug.show_trace_logger, ImGuiWindowFlags_MenuBar);

    trace_logger_menu();

    TraceLogger* tl = emu_get_core()->GetTraceLogger();

    if (ImGui::Button(trace_logger_enabled ? "Stop" : "Start"))
    {
        if (trace_logger_enabled)
            gui_debug_trace_logger_stop();
        else
            trace_logger_start(trace_logger_get_config_flags(), false);
    }

    ImGui::SameLine();

    ImGui::BeginDisabled(trace_logger_enabled && config_debug.trace_output == gui_TraceOutput_Disk);

    if (ImGui::Button("Clear"))
        gui_debug_trace_logger_clear();

    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(trace_logger_enabled);
    ImGui::SetNextItemWidth(90.0f);
    int previous_output = config_debug.trace_output;

    if (ImGui::Combo("##trace_output", &config_debug.trace_output, "Memory\0Disk\0\0"))
    {
        if (!trace_logger_apply_capacity())
            config_debug.trace_output = previous_output;
    }

    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(trace_logger_enabled);
    ImGui::SetNextItemWidth(145.0f);

    if (config_debug.trace_output == gui_TraceOutput_Memory)
    {
        int previous_capacity = config_debug.trace_capacity;

        if (ImGui::Combo("##trace_capacity", &config_debug.trace_capacity, k_trace_logger_capacity_labels,
            IM_ARRAYSIZE(k_trace_logger_capacity_labels)) && !trace_logger_apply_capacity())
            config_debug.trace_capacity = previous_capacity;
    }
    else
        ImGui::Combo("##trace_disk_size", &config_debug.trace_disk_size,
            "10 MB\0" "50 MB\0" "100 MB\0" "250 MB\0" "500 MB\0" "1 GB\0" "Unbounded\0\0");

    ImGui::EndDisabled();

    if (config_debug.trace_output == gui_TraceOutput_Memory && ImGui::IsItemHovered())
    {
        double memory_mib = ((double)k_trace_logger_capacities[config_debug.trace_capacity] * sizeof(GT_Trace_Entry)) /
            (1024.0 * 1024.0);
        ImGui::SetTooltip("Preallocated memory: %.1f MiB (%u bytes per entry).", memory_mib,
            (u32)sizeof(GT_Trace_Entry));
    }

    if (config_debug.trace_output == gui_TraceOutput_Memory)
    {
        ImGui::SameLine();
        ImGui::Text("Entries: %u / %u", tl->GetCount(), tl->GetCapacity());
    }

    if (config_debug.trace_output == gui_TraceOutput_Disk && trace_logger_disk_path[0] != '\0')
    {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputText("##trace_disk_file", trace_logger_disk_path, sizeof(trace_logger_disk_path),
            ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);
    }

    if (trace_logger_enabled)
        trace_logger_sync_flags();

    u32 count = tl->GetCount();
    ImGui::PushFont(gui_default_font);
    float line_height = ImGui::GetTextLineHeightWithSpacing();
    float content_height = (float)count * line_height;
    ImGui::SetNextWindowContentSize(ImVec2(0.0f, content_height));

    if ((trace_logger_enabled && trace_logger_follow_latest) || trace_logger_scroll_to_bottom)
        ImGui::SetNextWindowScroll(ImVec2(-1.0f, content_height));

    if (ImGui::BeginChild("##logger", ImVec2(ImGui::GetContentRegionAvail().x, 0), true,
        ImGuiWindowFlags_HorizontalScrollbar))
    {
        float scroll_y = ImGui::GetScrollY();
        float scroll_max_y = ImGui::GetScrollMaxY();
        bool at_bottom = scroll_y >= scroll_max_y - 0.5f;
        bool user_scrolling = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
            (ImGui::GetIO().MouseWheel != 0.0f || ImGui::IsMouseDragging(ImGuiMouseButton_Left));

        if (trace_logger_enabled)
        {
            if (trace_logger_scroll_to_bottom)
            {
                trace_logger_follow_latest = true;
                trace_logger_wait_for_scroll_away = false;
            }
            else if (trace_logger_follow_latest && user_scrolling)
            {
                trace_logger_follow_latest = false;
                trace_logger_wait_for_scroll_away = true;
            }
            else if (!trace_logger_follow_latest)
            {
                if (trace_logger_wait_for_scroll_away)
                {
                    if (!at_bottom)
                        trace_logger_wait_for_scroll_away = false;
                }
                else if (at_bottom)
                    trace_logger_follow_latest = true;
            }
        }

        ImGuiListClipper clipper;
        clipper.Begin((int)count, line_height);

        while (clipper.Step())
        {
            for (int item = clipper.DisplayStart; item < clipper.DisplayEnd; item++)
            {
                const GT_Trace_Entry& entry = tl->GetEntry((u32)item);
                u64 entry_number = tl->GetSequence() - (u64)count + (u64)item;
                bool previous_cycle_valid = item > 0;
                u64 previous_cycle = previous_cycle_valid ? tl->GetEntry((u32)item - 1).cycle : 0;
                render_entry_colored(entry, entry_number, previous_cycle_valid, previous_cycle);
            }
        }

        trace_logger_scroll_to_bottom = false;
    }

    ImGui::EndChild();
    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();

    if (trace_logger_choose_output_path)
    {
        trace_logger_choose_output_path = false;
        gui_file_dialog_choose_trace_path();
    }
}

void gui_debug_trace_logger_init(void)
{
    strncpy_fit(trace_logger_disk_directory, config_debug.trace_disk_path.c_str(), sizeof(trace_logger_disk_directory));

    if (!trace_logger_apply_capacity())
    {
        config_debug.trace_capacity = 0;
        trace_logger_apply_capacity();
    }
}

void gui_debug_trace_logger_update(void)
{
    if (!trace_logger_enabled || config_debug.trace_output != gui_TraceOutput_Disk)
        return;

    if (!trace_logger_flush_disk_entries())
        trace_logger_stop_disk(false, false);
    else if (trace_logger_disk_limit_reached)
    {
        trace_logger_stop_disk(false, false);
        gui_notify(gui_NotificationWarning, NULL, "Trace recording stopped", "Maximum file size reached", "trace");
    }
    else
    {
        Uint64 now = SDL_GetTicks();

        if ((now - trace_logger_disk_last_flush) >= 1000)
        {
            if (!trace_logger_flush_disk_buffer(true))
                trace_logger_stop_disk(false, false);
            else
                trace_logger_disk_last_flush = now;
        }
    }
}

void gui_debug_trace_logger_shutdown(void)
{
    if (IsValidPointer(trace_logger_disk_file))
        trace_logger_stop_disk(false, true);
}

void gui_debug_trace_logger_clear(void)
{
    TraceLogger* tl = emu_get_core()->GetTraceLogger();

    if (trace_logger_enabled && config_debug.trace_output == gui_TraceOutput_Disk)
    {
        if (!trace_logger_flush_disk_entries())
        {
            trace_logger_stop_disk(false, false);
            return;
        }

        tl->Reset();
        trace_logger_disk_flushed_total = 0;
        trace_logger_disk_previous_cycle = 0;
        trace_logger_disk_previous_cycle_valid = false;
    }
    else
        tl->Reset();
}

void gui_debug_trace_logger_reset(void)
{
    trace_logger_stop(false);

    emu_get_core()->GetTraceLogger()->Reset();
    trace_logger_disk_flushed_total = 0;
    trace_logger_disk_previous_cycle = 0;
    trace_logger_disk_previous_cycle_valid = false;
}

void gui_debug_trace_logger_set_output_directory(const char* path)
{
    strncpy_fit(trace_logger_disk_directory, path, sizeof(trace_logger_disk_directory));
    config_debug.trace_disk_path.assign(path);
}

int gui_debug_trace_logger_memory_size_index(const char* size)
{
    if (!IsValidPointer(size))
        return -1;

    for (int i = 0; i < IM_ARRAYSIZE(k_trace_logger_capacity_names); i++)
    {
        if (strcmp(size, k_trace_logger_capacity_names[i]) == 0)
            return i;
    }

    return -1;
}

int gui_debug_trace_logger_disk_size_index(const char* size)
{
    if (!IsValidPointer(size))
        return -1;

    for (int i = 0; i < IM_ARRAYSIZE(k_trace_logger_disk_size_names); i++)
    {
        if (strcmp(size, k_trace_logger_disk_size_names[i]) == 0)
            return i;
    }

    return -1;
}

const char* gui_debug_trace_logger_memory_size_name(int index)
{
    if (index < 0 || index >= IM_ARRAYSIZE(k_trace_logger_capacity_names))
        return k_trace_logger_capacity_names[0];

    return k_trace_logger_capacity_names[index];
}

const char* gui_debug_trace_logger_disk_size_name(int index)
{
    if (index < 0 || index >= IM_ARRAYSIZE(k_trace_logger_disk_size_names))
        return k_trace_logger_disk_size_names[2];

    return k_trace_logger_disk_size_names[index];
}

bool gui_debug_trace_logger_configure(int output, int memory_size, int disk_size, const char* output_path)
{
    if (trace_logger_enabled)
        return false;

    if (output < gui_TraceOutput_Memory || output > gui_TraceOutput_Disk)
        return false;

    if (memory_size < 0 || memory_size >= IM_ARRAYSIZE(k_trace_logger_capacities))
        return false;

    if (disk_size < 0 || disk_size >= IM_ARRAYSIZE(k_trace_logger_disk_sizes))
        return false;

    int previous_output = config_debug.trace_output;
    int previous_memory_size = config_debug.trace_capacity;
    int previous_disk_size = config_debug.trace_disk_size;
    int previous_dir_option = config_debug.trace_disk_dir_option;
    std::string previous_path = config_debug.trace_disk_path;

    config_debug.trace_output = output;
    config_debug.trace_capacity = memory_size;
    config_debug.trace_disk_size = disk_size;

    if (output == gui_TraceOutput_Disk && IsValidPointer(output_path) && output_path[0] != '\0')
    {
        config_debug.trace_disk_dir_option = Directory_Location_Custom;
        gui_debug_trace_logger_set_output_directory(output_path);
    }

    if (!trace_logger_apply_capacity())
    {
        config_debug.trace_output = previous_output;
        config_debug.trace_capacity = previous_memory_size;
        config_debug.trace_disk_size = previous_disk_size;
        config_debug.trace_disk_dir_option = previous_dir_option;
        config_debug.trace_disk_path = previous_path;
        strncpy_fit(trace_logger_disk_directory, previous_path.c_str(), sizeof(trace_logger_disk_directory));
        return false;
    }

    return true;
}

void gui_debug_trace_logger_set_event_filters(const u32* filters)
{
    if (!IsValidPointer(filters))
        return;

    for (int i = 0; i < TRACE_TYPE_COUNT; i++)
    {
        int* filter = trace_logger_get_config_event_filter((GT_Trace_Type)i);

        if (IsValidPointer(filter))
            *filter = (int)filters[i];
    }
}

bool gui_debug_trace_logger_start(u32 flags)
{
    config_debug.show_trace_logger = true;
    return trace_logger_start(flags, true);
}

bool gui_debug_trace_logger_stop(void)
{
    return trace_logger_stop(true);
}

bool gui_debug_trace_logger_is_enabled(void)
{
    return trace_logger_enabled;
}

const char* gui_debug_trace_logger_get_output_path(void)
{
    return trace_logger_disk_path;
}

bool gui_debug_save_log(const char* file_path)
{
    FILE* file = fopen_utf8(file_path, "w");

    if (!IsValidPointer(file))
        return false;

    TraceLogger* tl = emu_get_core()->GetTraceLogger();
    u32 count = tl->GetCount();
    u64 oldest = tl->GetSequence() - (u64)count;
    char buf[GT_TRACE_FORMAT_BUFFER_SIZE];

    for (u32 i = 0; i < count; i++)
    {
        const GT_Trace_Entry& entry = tl->GetEntry(i);
        bool previous_cycle_valid = i > 0;
        u64 previous_cycle = previous_cycle_valid ? tl->GetEntry(i - 1).cycle : 0;
        format_entry_text(entry, config_debug.trace_cycles, previous_cycle_valid, previous_cycle, buf, sizeof(buf));

        if (config_debug.trace_counter)
            fprintf(file, "%06llu %s\n", (unsigned long long)(oldest + i), buf);
        else
            fprintf(file, "%s\n", buf);
    }

    fclose(file);
    return true;
}

static void trace_logger_menu(void)
{
    ImGui::BeginMenuBar();

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Save Log As...", NULL, false, config_debug.trace_output == gui_TraceOutput_Memory))
        {
            gui_file_dialog_save_log();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Settings"))
    {
        ImGui::MenuItem("Event Counter", "", &config_debug.trace_counter);
        ImGui::MenuItem("Clock Cycles", "", &config_debug.trace_cycles);

        if (ImGui::BeginMenu("CPU"))
        {
            ImGui::MenuItem("Linear Address", "", &config_debug.trace_linear);
            ImGui::MenuItem("Registers", "", &config_debug.trace_registers);
            ImGui::MenuItem("Segments", "", &config_debug.trace_segments);
            ImGui::MenuItem("Flags", "", &config_debug.trace_flags);
            ImGui::MenuItem("Bytes", "", &config_debug.trace_bytes);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Disk Output"))
        {
            ImGui::BeginDisabled(trace_logger_enabled);
            ImGui::SetNextItemWidth(180.0f);
            ImGui::Combo("##trace_disk_dir", &config_debug.trace_disk_dir_option,
                "Default Location\0Same as ROM\0Custom Location\0\0");

            switch ((Directory_Location)config_debug.trace_disk_dir_option)
            {
                default:
                case Directory_Location_Default:
                    ImGui::Text("%s", config_root_path);
                    break;
                case Directory_Location_ROM:
                    if (!emu_is_empty())
                        ImGui::Text("%s", emu_get_core()->GetMedia()->GetFileDirectory());
                    break;
                case Directory_Location_Custom:
                    if (ImGui::MenuItem("Choose..."))
                    {
                        trace_logger_choose_output_path = true;
                    }

                    ImGui::PushItemWidth(450.0f);

                    if (ImGui::InputText("##trace_disk_path", trace_logger_disk_directory,
                        sizeof(trace_logger_disk_directory), ImGuiInputTextFlags_AutoSelectAll))
                        config_debug.trace_disk_path.assign(trace_logger_disk_directory);

                    ImGui::PopItemWidth();
                    break;
            }

            ImGui::EndDisabled();
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Filters"))
    {
        trace_logger_menu_filters();
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

static void trace_logger_menu_filters(void)
{
    if (ImGui::BeginMenu("CPU"))
    {
        ImGui::MenuItem("Enabled", "", &config_debug.trace_cpu_enabled);
        ImGui::Separator();
        ImGui::BeginDisabled(!config_debug.trace_cpu_enabled);
        ImGui::MenuItem("Instructions", "", &config_debug.trace_cpu);
        trace_logger_menu_event_filter("IRQs", &config_debug.trace_cpu_interrupt_events, TRACE_CPU_INTERRUPT_EVENT_IRQS);
        trace_logger_menu_event_filter("Exceptions", &config_debug.trace_cpu_interrupt_events,
            TRACE_CPU_INTERRUPT_EVENT_EXCEPTIONS);
        trace_logger_menu_event_filter("Software INTs", &config_debug.trace_cpu_interrupt_events,
            TRACE_CPU_INTERRUPT_EVENT_SOFTWARE);
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("I/O Ports"))
    {
        ImGui::MenuItem("Enabled", "", &config_debug.trace_io);
        ImGui::Separator();
        ImGui::BeginDisabled(!config_debug.trace_io);
        trace_logger_menu_event_filter("Reads", &config_debug.trace_io_events, TRACE_IO_EVENT_READS);
        trace_logger_menu_event_filter("Writes", &config_debug.trace_io_events, TRACE_IO_EVENT_WRITES);
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Interrupt Controller"))
    {
        ImGui::MenuItem("Enabled", "", &config_debug.trace_pic);
        ImGui::Separator();
        ImGui::BeginDisabled(!config_debug.trace_pic);
        trace_logger_menu_event_filter("IRQ Requests", &config_debug.trace_pic_events, TRACE_PIC_EVENT_REQUESTS);
        trace_logger_menu_event_filter("Mask Writes", &config_debug.trace_pic_events, TRACE_PIC_EVENT_MASK);
        trace_logger_menu_event_filter("EOI / Commands", &config_debug.trace_pic_events, TRACE_PIC_EVENT_COMMANDS);
        trace_logger_menu_event_filter("Initialization", &config_debug.trace_pic_events, TRACE_PIC_EVENT_INIT);
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Timers"))
    {
        ImGui::MenuItem("Enabled", "", &config_debug.trace_timer);
        ImGui::Separator();
        ImGui::BeginDisabled(!config_debug.trace_timer);
        trace_logger_menu_event_filter("Timeouts", &config_debug.trace_timer_events, TRACE_TIMER_EVENT_TIMEOUTS);
        trace_logger_menu_event_filter("Counter Writes", &config_debug.trace_timer_events, TRACE_TIMER_EVENT_COUNTERS);
        trace_logger_menu_event_filter("Interrupt Control", &config_debug.trace_timer_events,
            TRACE_TIMER_EVENT_INTERRUPT);
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("DMA"))
    {
        ImGui::MenuItem("Enabled", "", &config_debug.trace_dma);
        ImGui::Separator();
        ImGui::BeginDisabled(!config_debug.trace_dma);
        trace_logger_menu_event_filter("Register Writes", &config_debug.trace_dma_events, TRACE_DMA_EVENT_REGISTERS);
        trace_logger_menu_event_filter("Requests", &config_debug.trace_dma_events, TRACE_DMA_EVENT_REQUESTS);
        trace_logger_menu_event_filter("Transfer Ends", &config_debug.trace_dma_events, TRACE_DMA_EVENT_ENDS);
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Video"))
    {
        ImGui::MenuItem("Enabled", "", &config_debug.trace_video);
        ImGui::Separator();
        ImGui::BeginDisabled(!config_debug.trace_video);
        trace_logger_menu_event_filter("CRTC Registers", &config_debug.trace_video_events, TRACE_VIDEO_EVENT_CRTC);
        trace_logger_menu_event_filter("Video Output", &config_debug.trace_video_events, TRACE_VIDEO_EVENT_OUTPUT);
        trace_logger_menu_event_filter("Palette", &config_debug.trace_video_events, TRACE_VIDEO_EVENT_PALETTE);
        trace_logger_menu_event_filter("VRAM Mask", &config_debug.trace_video_events, TRACE_VIDEO_EVENT_MASK);
        trace_logger_menu_event_filter("VSYNC IRQ", &config_debug.trace_video_events, TRACE_VIDEO_EVENT_VSYNC);
        trace_logger_menu_event_filter("FM-R Registers", &config_debug.trace_video_events, TRACE_VIDEO_EVENT_FMR);

        if (ImGui::BeginMenu("Missed VBlank"))
        {
            trace_logger_menu_event_filter("Enabled", &config_debug.trace_video_events, TRACE_VIDEO_EVENT_MISSED_VBLANK);

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Logs a missed VBlank if the watched access did not happen during the frame ending at VSYNC");

            float input_x = ImGui::GetCursorPosX() + ImGui::CalcTextSize("Operation").x + ImGui::GetStyle().ItemSpacing.x;

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Address");
            ImGui::SameLine(input_x);
            u32 address = (u32)config_debug.trace_vblank_watch_address;
            ImGui::PushItemWidth(75.0f);

            if (ImGui::InputScalar("##vblank_watch_address", ImGuiDataType_U32, &address, NULL, NULL, "%08X",
                ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase))
                config_debug.trace_vblank_watch_address = (int)address;

            ImGui::PopItemWidth();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Operation");
            ImGui::SameLine(input_x);
            ImGui::PushItemWidth(60.0f);
            ImGui::Combo("##vblank_watch_operation", &config_debug.trace_vblank_watch_operation, "R\0W\0R/W\0\0");
            ImGui::PopItemWidth();

            ImGui::EndMenu();
        }

        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Sprites"))
    {
        ImGui::MenuItem("Enabled", "", &config_debug.trace_sprite);
        ImGui::Separator();
        ImGui::BeginDisabled(!config_debug.trace_sprite);
        trace_logger_menu_event_filter("Registers", &config_debug.trace_sprite_events, TRACE_SPRITE_EVENT_REGISTERS);
        trace_logger_menu_event_filter("Transfers", &config_debug.trace_sprite_events, TRACE_SPRITE_EVENT_TRANSFERS);
        trace_logger_menu_event_filter("Busy at VSYNC", &config_debug.trace_sprite_events, TRACE_SPRITE_EVENT_BUSY);
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("YM3438 (FM)"))
    {
        ImGui::MenuItem("Enabled", "", &config_debug.trace_fm);
        ImGui::Separator();
        ImGui::BeginDisabled(!config_debug.trace_fm);
        trace_logger_menu_event_filter("Key On / Off", &config_debug.trace_fm_events, TRACE_FM_EVENT_KEY);
        trace_logger_menu_event_filter("Frequency", &config_debug.trace_fm_events, TRACE_FM_EVENT_FREQUENCY);
        trace_logger_menu_event_filter("Operators", &config_debug.trace_fm_events, TRACE_FM_EVENT_OPERATORS);
        trace_logger_menu_event_filter("Channel Control", &config_debug.trace_fm_events, TRACE_FM_EVENT_CHANNELS);
        trace_logger_menu_event_filter("Global / LFO", &config_debug.trace_fm_events, TRACE_FM_EVENT_GLOBAL);
        trace_logger_menu_event_filter("DAC Data", &config_debug.trace_fm_events, TRACE_FM_EVENT_DAC);
        trace_logger_menu_event_filter("Timers", &config_debug.trace_fm_events, TRACE_FM_EVENT_TIMERS);
        trace_logger_menu_event_filter("IRQs", &config_debug.trace_fm_events, TRACE_FM_EVENT_IRQS);
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("RF5C68 (PCM)"))
    {
        ImGui::MenuItem("Enabled", "", &config_debug.trace_pcm);
        ImGui::Separator();
        ImGui::BeginDisabled(!config_debug.trace_pcm);
        trace_logger_menu_event_filter("Channel Registers", &config_debug.trace_pcm_events, TRACE_PCM_EVENT_CHANNELS);
        trace_logger_menu_event_filter("Key On / Off", &config_debug.trace_pcm_events, TRACE_PCM_EVENT_KEY);
        trace_logger_menu_event_filter("Control", &config_debug.trace_pcm_events, TRACE_PCM_EVENT_CONTROL);
        trace_logger_menu_event_filter("IRQs", &config_debug.trace_pcm_events, TRACE_PCM_EVENT_IRQS);
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Sound Mixer"))
    {
        ImGui::MenuItem("Enabled", "", &config_debug.trace_mixer);
        ImGui::Separator();
        ImGui::BeginDisabled(!config_debug.trace_mixer);
        trace_logger_menu_event_filter("Electronic Volume", &config_debug.trace_mixer_events, TRACE_MIXER_EVENT_VOLUME);
        trace_logger_menu_event_filter("Mute", &config_debug.trace_mixer_events, TRACE_MIXER_EVENT_MUTE);
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("CD-ROM"))
    {
        ImGui::MenuItem("Enabled", "", &config_debug.trace_cdrom);
        ImGui::Separator();
        ImGui::BeginDisabled(!config_debug.trace_cdrom);
        trace_logger_menu_event_filter("Commands", &config_debug.trace_cdrom_events, TRACE_CDROM_EVENT_COMMANDS);
        trace_logger_menu_event_filter("Status", &config_debug.trace_cdrom_events, TRACE_CDROM_EVENT_STATUS);
        trace_logger_menu_event_filter("IRQs", &config_debug.trace_cdrom_events, TRACE_CDROM_EVENT_IRQS);
        trace_logger_menu_event_filter("Control", &config_debug.trace_cdrom_events, TRACE_CDROM_EVENT_CONTROL);
        trace_logger_menu_event_filter("Data Transfers", &config_debug.trace_cdrom_events, TRACE_CDROM_EVENT_DATA);
        trace_logger_menu_event_filter("CD-DA", &config_debug.trace_cdrom_events, TRACE_CDROM_EVENT_CDDA);
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Floppy Controller"))
    {
        ImGui::MenuItem("Enabled", "", &config_debug.trace_fdc);
        ImGui::Separator();
        ImGui::BeginDisabled(!config_debug.trace_fdc);
        trace_logger_menu_event_filter("Commands", &config_debug.trace_fdc_events, TRACE_FDC_EVENT_COMMANDS);
        trace_logger_menu_event_filter("Results", &config_debug.trace_fdc_events, TRACE_FDC_EVENT_RESULTS);
        trace_logger_menu_event_filter("Drive Control", &config_debug.trace_fdc_events, TRACE_FDC_EVENT_DRIVES);
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Keyboard"))
    {
        ImGui::MenuItem("Enabled", "", &config_debug.trace_keyboard);
        ImGui::Separator();
        ImGui::BeginDisabled(!config_debug.trace_keyboard);
        trace_logger_menu_event_filter("Key Events", &config_debug.trace_keyboard_events, TRACE_KEYBOARD_EVENT_KEYS);
        trace_logger_menu_event_filter("Data Reads", &config_debug.trace_keyboard_events, TRACE_KEYBOARD_EVENT_READS);
        trace_logger_menu_event_filter("Commands", &config_debug.trace_keyboard_events, TRACE_KEYBOARD_EVENT_COMMANDS);
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Game Ports"))
    {
        ImGui::MenuItem("Enabled", "", &config_debug.trace_input);
        ImGui::Separator();
        ImGui::BeginDisabled(!config_debug.trace_input);
        trace_logger_menu_event_filter("Reads", &config_debug.trace_input_events, TRACE_INPUT_EVENT_READS);
        trace_logger_menu_event_filter("Writes", &config_debug.trace_input_events, TRACE_INPUT_EVENT_WRITES);
        trace_logger_menu_event_filter("Changes", &config_debug.trace_input_events, TRACE_INPUT_EVENT_CHANGES);
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("System"))
    {
        ImGui::MenuItem("Enabled", "", &config_debug.trace_system);
        ImGui::Separator();
        ImGui::BeginDisabled(!config_debug.trace_system);
        trace_logger_menu_event_filter("Reset / Power", &config_debug.trace_system_events, TRACE_SYSTEM_EVENT_RESET);
        trace_logger_menu_event_filter("Memory Mapping", &config_debug.trace_system_events, TRACE_SYSTEM_EVENT_MEMORY);
        trace_logger_menu_event_filter("RTC", &config_debug.trace_system_events, TRACE_SYSTEM_EVENT_RTC);
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }
}

static bool trace_logger_apply_capacity(void)
{
    TraceLogger* tl = emu_get_core()->GetTraceLogger();
    u32 capacity = TRACE_DISK_STAGING_CAPACITY;

    if (config_debug.trace_output == gui_TraceOutput_Memory)
        capacity = k_trace_logger_capacities[config_debug.trace_capacity];

    if (!tl->SetCapacity(capacity))
    {
        gui_notify(gui_NotificationError, NULL, "Unable to allocate the selected trace logger capacity");
        return false;
    }

    return true;
}

static bool trace_logger_start_disk(void)
{
    if (!trace_logger_apply_capacity())
        return false;

    const char* directory = config_root_path;

    switch ((Directory_Location)config_debug.trace_disk_dir_option)
    {
        case Directory_Location_ROM:
        {
            const char* media_directory = emu_is_empty() ? NULL : emu_get_core()->GetMedia()->GetFileDirectory();

            if (IsValidPointer(media_directory) && media_directory[0] != '\0')
                directory = media_directory;
            break;
        }
        case Directory_Location_Custom:
            directory = config_debug.trace_disk_path.c_str();
            break;
        default:
            break;
    }

    time_t now = time(0);
    tm local_time;
    char date_time[32] = {};

    if (get_local_time(now, &local_time))
        strftime(date_time, sizeof(date_time), "%Y-%m-%d %H%M%S", &local_time);

    const char* rom_name = "Geartowns";

    if (!emu_is_empty() && emu_get_core()->GetMedia()->GetFileName()[0] != '\0')
        rom_name = emu_get_core()->GetMedia()->GetFileName();

    bool path_available = false;

    for (int index = 0; index < 1000; index++)
    {
        char filename[1024];

        if (index == 0)
            snprintf(filename, sizeof(filename), "%s - Trace - %s.txt", rom_name, date_time);
        else
            snprintf(filename, sizeof(filename), "%s - Trace - %s (%d).txt", rom_name, date_time, index + 1);

        if (!join_path(directory, filename, trace_logger_disk_path, sizeof(trace_logger_disk_path)))
        {
            gui_notify(gui_NotificationError, NULL, "Trace log path is too long", directory);
            return false;
        }

        if (!path_exists(trace_logger_disk_path))
        {
            path_available = true;
            break;
        }
    }

    if (!path_available)
    {
        gui_notify(gui_NotificationError, NULL, "Unable to create a unique trace log filename", directory);
        return false;
    }

    trace_logger_disk_file = fopen_utf8(trace_logger_disk_path, "wb");

    if (!IsValidPointer(trace_logger_disk_file))
    {
        gui_notify(gui_NotificationError, NULL, "Unable to create the trace log file", trace_logger_disk_path);
        trace_logger_disk_path[0] = '\0';
        return false;
    }

    trace_logger_disk_buffer_used = 0;
    trace_logger_disk_entries = 0;
    trace_logger_disk_flushed_total = 0;
    trace_logger_disk_bytes = 0;
    trace_logger_disk_previous_cycle = 0;
    trace_logger_disk_previous_cycle_valid = false;
    trace_logger_disk_limit_reached = false;
    trace_logger_disk_overflow = false;
    trace_logger_disk_error = false;
    trace_logger_disk_last_flush = SDL_GetTicks();
    emu_get_core()->GetTraceLogger()->Reset();
    gui_notify(gui_NotificationInfo, ICON_MD_FIBER_MANUAL_RECORD, "Trace recording started", trace_logger_disk_path,
        "trace");
    return true;
}

static bool trace_logger_start(u32 flags, bool update_config)
{
    if (flags == 0)
    {
        flags = TRACE_FLAG_CPU | TRACE_FLAG_CPU_INTERRUPT;
        update_config = true;
    }

    if (update_config)
        trace_logger_set_config_flags(flags);

    if (trace_logger_enabled)
    {
        trace_logger_sync_flags();
        return true;
    }

    if (config_debug.trace_output == gui_TraceOutput_Disk && !trace_logger_start_disk())
        return false;

    trace_logger_enabled = true;
    trace_logger_follow_latest = true;
    trace_logger_scroll_to_bottom = true;
    trace_logger_wait_for_scroll_away = false;
    trace_logger_sync_flags();
    return true;
}

static bool trace_logger_stop(bool show_status)
{
    if (!trace_logger_enabled)
        return true;

    trace_logger_scroll_to_bottom = trace_logger_follow_latest;

    if (config_debug.trace_output == gui_TraceOutput_Disk)
        return trace_logger_stop_disk(show_status, true);

    trace_logger_enabled = false;
    emu_get_core()->GetTraceLogger()->SetEnabledFlags(0);
    trace_logger_sync_vblank_watch(false);
    return true;
}

static bool trace_logger_stop_disk(bool show_status, bool flush_entries)
{
    bool success = IsValidPointer(trace_logger_disk_file) && !trace_logger_disk_overflow && !trace_logger_disk_error;

    if (IsValidPointer(trace_logger_disk_file))
    {
        if (flush_entries && !trace_logger_flush_disk_entries())
            success = false;

        if (!trace_logger_flush_disk_buffer(true))
            success = false;

        if (fclose(trace_logger_disk_file) != 0)
            success = false;

        InitPointer(trace_logger_disk_file);
    }

    trace_logger_enabled = false;
    emu_get_core()->GetTraceLogger()->SetEnabledFlags(0);
    trace_logger_sync_vblank_watch(false);

    if (!success)
    {
        const char* reason = trace_logger_disk_overflow ? "Staging buffer overflow" : "Disk write error";
        gui_notify(gui_NotificationError, NULL, "Trace recording stopped", reason, "trace");
        Error("Trace recording stopped: %s. File: %s", reason, trace_logger_disk_path);
    }
    else if (show_status)
        gui_notify(gui_NotificationInfo, ICON_MD_STOP, "Trace recording stopped", trace_logger_disk_path, "trace");

    return success;
}

static bool trace_logger_flush_disk_buffer(bool flush_file)
{
    if (!IsValidPointer(trace_logger_disk_file))
    {
        trace_logger_disk_error = true;
        return false;
    }

    if (trace_logger_disk_buffer_used > 0)
    {
        size_t written = fwrite(trace_logger_disk_buffer, 1, trace_logger_disk_buffer_used, trace_logger_disk_file);

        if (written > 0)
        {
            trace_logger_disk_buffer_used -= written;

            if (trace_logger_disk_buffer_used > 0)
                memmove(trace_logger_disk_buffer, trace_logger_disk_buffer + written, trace_logger_disk_buffer_used);
        }

        if (trace_logger_disk_buffer_used > 0)
        {
            trace_logger_disk_error = true;
            return false;
        }
    }

    if (flush_file && fflush(trace_logger_disk_file) != 0)
    {
        trace_logger_disk_error = true;
        return false;
    }

    return true;
}

static bool trace_logger_flush_disk_entries(void)
{
    if (!IsValidPointer(trace_logger_disk_file))
    {
        trace_logger_disk_error = true;
        return false;
    }

    if (trace_logger_disk_limit_reached)
        return true;

    TraceLogger* tl = emu_get_core()->GetTraceLogger();
    u32 count = tl->GetCount();
    u64 total = tl->GetTotalLogged();
    u64 oldest = total - (u64)count;

    if (trace_logger_disk_flushed_total < oldest)
    {
        trace_logger_disk_overflow = true;
        return false;
    }

    u32 first = (u32)(trace_logger_disk_flushed_total - oldest);
    char entry_text[GT_TRACE_FORMAT_BUFFER_SIZE];
    char line[GT_TRACE_FORMAT_BUFFER_SIZE + 64];

    for (u32 i = first; i < count; i++)
    {
        const GT_Trace_Entry& entry = tl->GetEntry(i);
        format_entry_text(entry, config_debug.trace_cycles, trace_logger_disk_previous_cycle_valid,
            trace_logger_disk_previous_cycle, entry_text, sizeof(entry_text));
        int length;

        if (config_debug.trace_counter)
            length = snprintf(line, sizeof(line), "%06llu %s\n", (unsigned long long)trace_logger_disk_entries,
                entry_text);
        else
            length = snprintf(line, sizeof(line), "%s\n", entry_text);

        if (length < 0)
        {
            trace_logger_disk_error = true;
            return false;
        }

        size_t line_size = MIN((size_t)length, sizeof(line) - 1);
        u64 max_size = k_trace_logger_disk_sizes[config_debug.trace_disk_size];

        if (max_size > 0 && trace_logger_disk_bytes + (u64)line_size > max_size)
        {
            trace_logger_disk_limit_reached = true;
            break;
        }

        if (trace_logger_disk_buffer_used + line_size > sizeof(trace_logger_disk_buffer) &&
            !trace_logger_flush_disk_buffer(false))
            return false;

        memcpy(trace_logger_disk_buffer + trace_logger_disk_buffer_used, line, line_size);
        trace_logger_disk_buffer_used += line_size;
        trace_logger_disk_entries++;
        trace_logger_disk_bytes += (u64)line_size;
        trace_logger_disk_previous_cycle = entry.cycle;
        trace_logger_disk_previous_cycle_valid = true;
    }

    trace_logger_disk_flushed_total = total;
    return true;
}

static void trace_logger_sync_flags(void)
{
    TraceLogger* tl = emu_get_core()->GetTraceLogger();
    tl->SetEnabledFlags(trace_logger_get_config_flags());

    for (int i = 0; i < TRACE_TYPE_COUNT; i++)
    {
        int* filter = trace_logger_get_config_event_filter((GT_Trace_Type)i);
        tl->SetEventFilter((GT_Trace_Type)i, IsValidPointer(filter) ? (u32)*filter : 0xFFFFFFFFU);
    }

    trace_logger_sync_vblank_watch(true);
}

// The watch is only armed while the missed VBlank filter is on, the debugger checks memory accesses for it
static void trace_logger_sync_vblank_watch(bool enabled)
{
    bool active = enabled && config_debug.trace_video &&
        (((u32)config_debug.trace_video_events & TRACE_VIDEO_EVENT_MISSED_VBLANK) != 0);
    int operation = config_debug.trace_vblank_watch_operation;
    bool read = active && (operation == 0 || operation == 2);
    bool write = active && (operation == 1 || operation == 2);
    emu_get_core()->GetI386()->SetVBlankWatch(read, write, (u32)config_debug.trace_vblank_watch_address);
}

static u32 trace_logger_get_config_flags(void)
{
    u32 flags = 0;

    if (config_debug.trace_cpu_enabled)
    {
        if (config_debug.trace_cpu) flags |= TRACE_FLAG_CPU;
        if (config_debug.trace_cpu_interrupt_events != 0) flags |= TRACE_FLAG_CPU_INTERRUPT;
    }

    if (config_debug.trace_io) flags |= TRACE_FLAG_IO;
    if (config_debug.trace_pic) flags |= TRACE_FLAG_PIC;
    if (config_debug.trace_timer) flags |= TRACE_FLAG_TIMER;
    if (config_debug.trace_dma) flags |= TRACE_FLAG_DMA;
    if (config_debug.trace_video) flags |= TRACE_FLAG_VIDEO;
    if (config_debug.trace_sprite) flags |= TRACE_FLAG_SPRITE;
    if (config_debug.trace_fm) flags |= TRACE_FLAG_FM;
    if (config_debug.trace_pcm) flags |= TRACE_FLAG_PCM;
    if (config_debug.trace_mixer) flags |= TRACE_FLAG_MIXER;
    if (config_debug.trace_cdrom) flags |= TRACE_FLAG_CDROM;
    if (config_debug.trace_fdc) flags |= TRACE_FLAG_FDC;
    if (config_debug.trace_keyboard) flags |= TRACE_FLAG_KEYBOARD;
    if (config_debug.trace_input) flags |= TRACE_FLAG_INPUT;
    if (config_debug.trace_system) flags |= TRACE_FLAG_SYSTEM;

    return flags;
}

static void trace_logger_set_config_flags(u32 flags)
{
    config_debug.trace_cpu_enabled = (flags & (TRACE_FLAG_CPU | TRACE_FLAG_CPU_INTERRUPT)) != 0;
    config_debug.trace_cpu = (flags & TRACE_FLAG_CPU) != 0;

    if ((flags & TRACE_FLAG_CPU_INTERRUPT) == 0)
        config_debug.trace_cpu_interrupt_events = 0;
    else if (config_debug.trace_cpu_interrupt_events == 0)
        config_debug.trace_cpu_interrupt_events = TRACE_CPU_INTERRUPT_EVENT_DEFAULT;

    config_debug.trace_io = (flags & TRACE_FLAG_IO) != 0;
    config_debug.trace_pic = (flags & TRACE_FLAG_PIC) != 0;
    config_debug.trace_timer = (flags & TRACE_FLAG_TIMER) != 0;
    config_debug.trace_dma = (flags & TRACE_FLAG_DMA) != 0;
    config_debug.trace_video = (flags & TRACE_FLAG_VIDEO) != 0;
    config_debug.trace_sprite = (flags & TRACE_FLAG_SPRITE) != 0;
    config_debug.trace_fm = (flags & TRACE_FLAG_FM) != 0;
    config_debug.trace_pcm = (flags & TRACE_FLAG_PCM) != 0;
    config_debug.trace_mixer = (flags & TRACE_FLAG_MIXER) != 0;
    config_debug.trace_cdrom = (flags & TRACE_FLAG_CDROM) != 0;
    config_debug.trace_fdc = (flags & TRACE_FLAG_FDC) != 0;
    config_debug.trace_keyboard = (flags & TRACE_FLAG_KEYBOARD) != 0;
    config_debug.trace_input = (flags & TRACE_FLAG_INPUT) != 0;
    config_debug.trace_system = (flags & TRACE_FLAG_SYSTEM) != 0;
}

static int* trace_logger_get_config_event_filter(GT_Trace_Type type)
{
    switch (type)
    {
        case TRACE_CPU_INTERRUPT: return &config_debug.trace_cpu_interrupt_events;
        case TRACE_IO: return &config_debug.trace_io_events;
        case TRACE_PIC: return &config_debug.trace_pic_events;
        case TRACE_TIMER: return &config_debug.trace_timer_events;
        case TRACE_DMA: return &config_debug.trace_dma_events;
        case TRACE_VIDEO: return &config_debug.trace_video_events;
        case TRACE_SPRITE: return &config_debug.trace_sprite_events;
        case TRACE_FM: return &config_debug.trace_fm_events;
        case TRACE_PCM: return &config_debug.trace_pcm_events;
        case TRACE_MIXER: return &config_debug.trace_mixer_events;
        case TRACE_CDROM: return &config_debug.trace_cdrom_events;
        case TRACE_FDC: return &config_debug.trace_fdc_events;
        case TRACE_KEYBOARD: return &config_debug.trace_keyboard_events;
        case TRACE_INPUT: return &config_debug.trace_input_events;
        case TRACE_SYSTEM: return &config_debug.trace_system_events;
        default: return NULL;
    }
}

static void trace_logger_menu_event_filter(const char* label, int* filter, u32 mask)
{
    bool enabled = ((u32)*filter & mask) != 0;

    if (ImGui::MenuItem(label, "", &enabled))
    {
        if (enabled)
            *filter |= (int)mask;
        else
            *filter &= ~(int)mask;
    }
}

static void format_entry_text(const GT_Trace_Entry& entry, bool cycles, bool previous_cycle_valid, u64 previous_cycle,
    char* buf, int buf_size)
{
    GT_Trace_Format_Options options;
    options.linear = config_debug.trace_linear;
    options.registers = config_debug.trace_registers;
    options.segments = config_debug.trace_segments;
    options.flags = config_debug.trace_flags;
    options.bytes = config_debug.trace_bytes;
    options.cycles = cycles;
    options.previous_cycle_valid = previous_cycle_valid;
    options.previous_cycle = previous_cycle;
    trace_logger_format_entry(entry, options, buf, buf_size);
}

static void render_cpu_entry_colored(const GT_Trace_Entry& entry, int prefix_length)
{
    char address[24];
    trace_log_format_cpu_address(entry, address, sizeof(address));

    if (config_debug.trace_linear)
    {
        ImGui::TextColored(violet, "%08X ", entry.cpu.linear);
        ImGui::SameLine(0, 0);
    }

    ImGui::TextColored(cyan, "%s", address);

    if (config_debug.trace_registers)
    {
        static const char* const k_names[8] = { "EAX", "EBX", "ECX", "EDX", "ESI", "EDI", "EBP", "ESP" };
        static const int k_registers[8] =
        {
            I386_REG_EAX, I386_REG_EBX, I386_REG_ECX, I386_REG_EDX, I386_REG_ESI, I386_REG_EDI, I386_REG_EBP,
            I386_REG_ESP
        };

        for (int i = 0; i < 8; i++)
        {
            ImGui::SameLine(0, 0);
            ImGui::TextColored(magenta, i == 0 ? "  %s:" : " %s:", k_names[i]);
            ImGui::SameLine(0, 0);
            ImGui::TextColored(white, "%08X", entry.cpu.registers[k_registers[i]]);
        }
    }

    if (config_debug.trace_segments)
    {
        static const char* const k_names[5] = { "DS", "ES", "SS", "FS", "GS" };
        const u16 segments[5] = { entry.cpu.ds, entry.cpu.es, entry.cpu.ss, entry.cpu.fs, entry.cpu.gs };

        for (int i = 0; i < 5; i++)
        {
            ImGui::SameLine(0, 0);
            ImGui::TextColored(magenta, i == 0 ? "  %s:" : " %s:", k_names[i]);
            ImGui::SameLine(0, 0);
            ImGui::TextColored(white, "%04X", segments[i]);
        }
    }

    if (config_debug.trace_flags)
    {
        char flags[24];
        trace_log_format_cpu_flags(entry, flags, sizeof(flags));
        ImGui::SameLine(0, 0);
        ImGui::TextColored(yellow, "  %s", flags);
    }

    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted("  ");
    ImGui::SameLine(0, 0);

    if (entry.cpu.name[0] != 0)
        gui_debug_disassembler_draw_text(entry.cpu.name);
    else
        ImGui::TextColored(gray, "???");

    if (config_debug.trace_bytes)
    {
        char bytes[48];
        trace_log_format_cpu_bytes(entry, bytes, sizeof(bytes));
        float char_width = ImGui::CalcTextSize("A").x;
        float bytes_column = char_width * (13 + 2 + GT_TRACE_INSTRUCTION_WIDTH + 1);
        if (config_debug.trace_linear)       bytes_column += char_width * 9;
        if (config_debug.trace_registers)    bytes_column += char_width * 105;
        if (config_debug.trace_segments)     bytes_column += char_width * 41;
        if (config_debug.trace_flags)        bytes_column += char_width * 15;
        bytes_column += char_width * prefix_length;
        ImGui::SameLine(bytes_column);
        ImGui::TextColored(gray, "%s", bytes);
    }
}

static void render_entry_colored(const GT_Trace_Entry& entry, u64 index, bool previous_cycle_valid, u64 previous_cycle)
{
    char buf[GT_TRACE_FORMAT_BUFFER_SIZE];
    int prefix_length = 0;

    if (config_debug.trace_counter)
    {
        char counter[32];
        snprintf(counter, sizeof(counter), "%06llu ", (unsigned long long)index);
        prefix_length += (int)strlen(counter);
        ImGui::TextColored(gray, "%s", counter);
        ImGui::SameLine(0, 0);
    }

    if (config_debug.trace_cycles)
    {
        char cycles[64];
        trace_log_format_cycle_prefix(entry, previous_cycle_valid, previous_cycle, cycles, sizeof(cycles));
        prefix_length += (int)strlen(cycles);
        ImGui::TextColored(gray, "%s", cycles);
        ImGui::SameLine(0, 0);
    }

    if (entry.type == TRACE_CPU)
    {
        render_cpu_entry_colored(entry, prefix_length);
        return;
    }

    format_entry_text(entry, false, false, 0, buf, sizeof(buf));

    switch (entry.type)
    {
        case TRACE_CPU_INTERRUPT: ImGui::TextColored(red, "%s", buf); break;
        case TRACE_IO: ImGui::TextColored(brown, "%s", buf); break;
        case TRACE_PIC: ImGui::TextColored(amber, "%s", buf); break;
        case TRACE_TIMER: ImGui::TextColored(orange, "%s", buf); break;
        case TRACE_DMA: ImGui::TextColored(cornflower, "%s", buf); break;
        case TRACE_VIDEO: ImGui::TextColored(green, "%s", buf); break;
        case TRACE_SPRITE: ImGui::TextColored(green, "%s", buf); break;
        case TRACE_FM: ImGui::TextColored(blue, "%s", buf); break;
        case TRACE_PCM: ImGui::TextColored(magenta, "%s", buf); break;
        case TRACE_MIXER: ImGui::TextColored(violet, "%s", buf); break;
        case TRACE_CDROM: ImGui::TextColored(cyan, "%s", buf); break;
        case TRACE_FDC: ImGui::TextColored(cornflower, "%s", buf); break;
        case TRACE_KEYBOARD: ImGui::TextColored(yellow, "%s", buf); break;
        case TRACE_INPUT: ImGui::TextColored(yellow, "%s", buf); break;
        default: ImGui::TextColored(white, "%s", buf); break;
    }
}
