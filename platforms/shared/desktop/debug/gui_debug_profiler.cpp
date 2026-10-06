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

#define GUI_DEBUG_PROFILER_IMPORT
#include "gui_debug_profiler.h"

#include <algorithm>
#include <string.h>
#include <vector>
#include "imgui.h"
#include "geartowns.h"
#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../gui_colors.h"
#include "gui_debug_disassembler.h"
#include "gui_debug_i386_tables.h"

enum ProfilerColumn
{
    ProfilerColumn_Function = 0,
    ProfilerColumn_Calls,
    ProfilerColumn_CallsPerFrame,
    ProfilerColumn_Inclusive,
    ProfilerColumn_Exclusive,
    ProfilerColumn_Average,
    ProfilerColumn_Max
};

static std::vector<u32> sorted;
static int sort_column = ProfilerColumn_Inclusive;
static bool sort_ascending = false;

static void function_name(const GT_Profiler_Function& function, char* text, size_t size);
static bool compare_functions(u32 a, u32 b);
static u64 average_cycles(const GT_Profiler_Function& function);

void gui_debug_window_profiler(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(130, 90), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 460), ImGuiCond_FirstUseEver);
    ImGui::Begin("Profiler", &config_debug.show_profiler);

    Profiler* profiler = emu_get_core()->GetProfiler();
    bool running = profiler->IsRunning();

    if (ImGui::Button(running ? "Stop" : "Start", ImVec2(60, 0)))
    {
        if (running)
            profiler->Stop();
        else
            profiler->Start();
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset", ImVec2(60, 0)))
        profiler->Reset();

    static char filter[64] = "";
    ImGui::SameLine();
    ImGui::PushItemWidth(140);
    ImGui::InputTextWithHint("##profiler_filter", "Filter...", filter, IM_ARRAYSIZE(filter));
    ImGui::PopItemWidth();

    ImGui::PushFont(gui_default_font);

    u64 total = profiler->GetTotalCycles();
    u32 frames = profiler->GetFrames();
    ImGui::SameLine();
    ImGui::TextColored(violet, " CYCLES"); ImGui::SameLine();
    ImGui::TextColored(white, "%llu", (unsigned long long)total); ImGui::SameLine();
    ImGui::TextColored(violet, " FRAMES"); ImGui::SameLine();
    ImGui::TextColored(white, "%u", frames);
    ImGui::Separator();

    const GT_Profiler_Function* functions = profiler->GetFunctions();
    u32 count = IsValidPointer(functions) ? profiler->GetFunctionCount() : 0;
    ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_Sortable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("##profiler", 7, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("FUNCTION", ImGuiTableColumnFlags_WidthStretch, 0.0f, ProfilerColumn_Function);
        ImGui::TableSetupColumn("CALLS", 0, 0.0f, ProfilerColumn_Calls);
        ImGui::TableSetupColumn("/FRAME", 0, 0.0f, ProfilerColumn_CallsPerFrame);
        ImGui::TableSetupColumn("INCLUSIVE", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending,
            0.0f, ProfilerColumn_Inclusive);
        ImGui::TableSetupColumn("EXCLUSIVE", ImGuiTableColumnFlags_PreferSortDescending, 0.0f, ProfilerColumn_Exclusive);
        ImGui::TableSetupColumn("AVG", ImGuiTableColumnFlags_PreferSortDescending, 0.0f, ProfilerColumn_Average);
        ImGui::TableSetupColumn("MAX", ImGuiTableColumnFlags_PreferSortDescending, 0.0f, ProfilerColumn_Max);
        ImGui::TableHeadersRow();

        ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();

        if (IsValidPointer(specs) && specs->SpecsCount > 0)
        {
            sort_column = (int)specs->Specs[0].ColumnUserID;
            sort_ascending = specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
        }

        sorted.clear();

        for (u32 i = 0; i < count; i++)
        {
            char name[64];
            function_name(functions[i], name, sizeof(name));

            if (filter[0] != 0 && strstr(name, filter) == NULL)
                continue;

            sorted.push_back(i);
        }

        std::sort(sorted.begin(), sorted.end(), compare_functions);

        ImGuiListClipper clipper;
        clipper.Begin((int)sorted.size());

        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                const GT_Profiler_Function& function = functions[sorted[row]];
                char name[64];
                function_name(function, name, sizeof(name));
                double inclusive = total > 0 ? 100.0 * (double)function.inclusive_cycles / (double)total : 0.0;
                double exclusive = total > 0 ? 100.0 * (double)function.exclusive_cycles / (double)total : 0.0;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(row);

                if (ImGui::Selectable("##function", false, ImGuiSelectableFlags_SpanAllColumns) &&
                    function.type != PROFILER_FUNCTION_ROOT)
                    gui_debug_goto_address(function.address);

                ImGui::SameLine(0, 0);
                ImGui::TextColored(function.type == PROFILER_FUNCTION_ROOT ? gray :
                    function.type == PROFILER_FUNCTION_INTERRUPT ? yellow : green, "%s", name);

                if (ImGui::IsItemHovered() && function.type != PROFILER_FUNCTION_ROOT)
                    ImGui::SetTooltip("%08X\nMIN %u cycles", function.address, function.completed > 0 ? function.min_cycles : 0);

                ImGui::PopID();
                ImGui::TableNextColumn();
                ImGui::TextColored(white, "%u", function.calls);
                ImGui::TableNextColumn();
                ImGui::TextColored(white, "%.1f", frames > 0 ? (double)function.calls / frames : 0.0);
                ImGui::TableNextColumn();
                ImGui::TextColored(white, "%llu", (unsigned long long)function.inclusive_cycles); ImGui::SameLine();
                ImGui::TextColored(gray, "%5.1f%%", inclusive);
                ImGui::TableNextColumn();
                ImGui::TextColored(white, "%llu", (unsigned long long)function.exclusive_cycles); ImGui::SameLine();
                ImGui::TextColored(gray, "%5.1f%%", exclusive);
                ImGui::TableNextColumn();
                ImGui::TextColored(white, "%llu", (unsigned long long)average_cycles(function));
                ImGui::TableNextColumn();
                ImGui::TextColored(white, "%u", function.max_cycles);
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_profiler_update(void)
{
    if (!IsValidPointer(emu_get_core()))
        return;

    Profiler* profiler = emu_get_core()->GetProfiler();

    if (profiler->IsRunning() && (!config_debug.show_profiler || !config_debug.debug))
        profiler->Stop();
}

static void function_name(const GT_Profiler_Function& function, char* text, size_t size)
{
    if (function.type == PROFILER_FUNCTION_ROOT)
    {
        snprintf(text, size, "(outside calls)");
        return;
    }

    if (function.type == PROFILER_FUNCTION_INTERRUPT)
    {
        char name[16];
        char description[64];
        gui_debug_i386_vector_name(function.vector, name, sizeof(name), description, sizeof(description));
        snprintf(text, size, "INT %02X %s", function.vector, description);
        return;
    }

    const char* symbol = gui_debug_get_symbol(function.address);

    if (IsValidPointer(symbol))
        snprintf(text, size, "%s", symbol);
    else
        snprintf(text, size, "%08X", function.address);
}

static u64 average_cycles(const GT_Profiler_Function& function)
{
    return function.completed > 0 ? function.inclusive_cycles / function.completed : 0;
}

static bool compare_functions(u32 a, u32 b)
{
    const GT_Profiler_Function* functions = emu_get_core()->GetProfiler()->GetFunctions();
    const GT_Profiler_Function& fa = functions[a];
    const GT_Profiler_Function& fb = functions[b];
    u64 va = 0;
    u64 vb = 0;

    switch (sort_column)
    {
        case ProfilerColumn_Function:
            va = fa.address;
            vb = fb.address;
            break;
        case ProfilerColumn_Calls:
        case ProfilerColumn_CallsPerFrame:
            va = fa.calls;
            vb = fb.calls;
            break;
        case ProfilerColumn_Exclusive:
            va = fa.exclusive_cycles;
            vb = fb.exclusive_cycles;
            break;
        case ProfilerColumn_Average:
            va = average_cycles(fa);
            vb = average_cycles(fb);
            break;
        case ProfilerColumn_Max:
            va = fa.max_cycles;
            vb = fb.max_cycles;
            break;
        default:
            va = fa.inclusive_cycles;
            vb = fb.inclusive_cycles;
            break;
    }

    if (va == vb)
        return a < b;

    return sort_ascending ? va < vb : va > vb;
}
