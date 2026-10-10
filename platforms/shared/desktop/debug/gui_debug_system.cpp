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

#define GUI_DEBUG_SYSTEM_IMPORT
#include "gui_debug_system.h"

#include "imgui.h"
#include "geartowns.h"
#include "system/io.h"
#include "system/msm58321.h"
#include "system/pic.h"
#include "system/pit.h"
#include "system/scheduler.h"
#include "system/system_control.h"
#include "system/upd71071.h"
#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../gui_colors.h"
#include "../utils.h"
#include "gui_debug_constants.h"
#include "gui_debug_i386_tables.h"
#include "gui_debug_memory.h"

static const char* const k_pit_mode_short_names[6] =
{
    "0 INT ON TC", "1 ONE-SHOT", "2 RATE GEN", "3 SQUARE", "4 SW STROBE", "5 HW STROBE"
};

static void setup_fixed_columns(const int* widths, int count);
static void draw_grid_label(const char* label);
static void draw_flag(const char* label, bool on);
static void draw_bits4(u8 value);
static void draw_vector_tooltip(u8 vector);
static void draw_pic_column(const I8259::I8259_State* chip, bool master, int row);
static void goto_physical(u32 address);

void gui_debug_window_pic(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(212, 244), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(410, 597), ImGuiCond_FirstUseEver);
    ImGui::Begin("Interrupts", &config_debug.show_pic);

    ImGui::PushFont(gui_default_font);

    PIC* pic = emu_get_core()->GetPIC();
    I8259::I8259_State* master = pic->GetMaster()->GetState();
    I8259::I8259_State* slave = pic->GetSlave()->GetState();
    float character = ImGui::CalcTextSize("0").x;

    ImGui::TextColored(cyan, "IRQ LINES"); ImGui::Separator();

    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_NoHostExtendX;

    if (ImGui::BeginTable("##irqs", 7, flags))
    {
        ImGui::TableSetupColumn("IRQ");
        ImGui::TableSetupColumn("SOURCE");
        ImGui::TableSetupColumn("LEVEL");
        ImGui::TableSetupColumn("REQ");
        ImGui::TableSetupColumn("SERV");
        ImGui::TableSetupColumn("MASK");
        ImGui::TableSetupColumn("VECTOR");
        ImGui::TableHeadersRow();

        for (int irq = 0; irq < 16; irq++)
        {
            const I8259::I8259_State* chip = irq < 8 ? master : slave;
            u8 bit = (u8)(1 << (irq & 7));
            u8 vector = (u8)((chip->icw2 & 0xF8) | (irq & 7));

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(orange, "%d", irq);
            ImGui::TableNextColumn();
            ImGui::TextColored(brown, "%s", k_debug_irq_sources[irq]);
            ImGui::TableNextColumn();
            ImGui::TextColored((chip->input_levels & bit) ? green : gray, "%s", (chip->input_levels & bit) ? "HI" : "LO");
            ImGui::TableNextColumn();
            ImGui::TextColored((chip->irr & bit) ? yellow : gray, "%d", (chip->irr & bit) ? 1 : 0);
            ImGui::TableNextColumn();
            ImGui::TextColored((chip->isr & bit) ? green : gray, "%d", (chip->isr & bit) ? 1 : 0);
            ImGui::TableNextColumn();
            ImGui::TextColored((chip->imr & bit) ? red : gray, "%d", (chip->imr & bit) ? 1 : 0);
            ImGui::TableNextColumn();
            ImGui::TextColored(white, "$%02X", vector);

            if (ImGui::IsItemHovered())
                draw_vector_tooltip(vector);
        }

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "CONTROLLERS"); ImGui::Separator();

    if (ImGui::BeginTable("##pics", 3, flags))
    {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, character * 11);
        ImGui::TableSetupColumn("MASTER", ImGuiTableColumnFlags_WidthFixed, character * 21);
        ImGui::TableSetupColumn("SLAVE", ImGuiTableColumnFlags_WidthFixed, character * 21);
        ImGui::TableHeadersRow();

        static const char* rows[] = { "ICW1", "ICW2", "ICW3", "ICW4", "INIT", "READ REG", "SPEC MASK", "POLL",
            "PRIORITY", "AUTO ROTATE", "INT OUTPUT" };

        for (int row = 0; row < (int)(sizeof(rows) / sizeof(rows[0])); row++)
        {
            ImGui::TableNextRow();
            draw_grid_label(rows[row]);
            draw_pic_column(master, true, row);
            ImGui::TableNextColumn();
            draw_pic_column(slave, false, row);
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_pit(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(253, 111), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(616, 261), ImGuiCond_FirstUseEver);
    ImGui::Begin("Timers", &config_debug.show_pit);

    ImGui::PushFont(gui_default_font);

    GeartownsCore* core = emu_get_core();
    PIT* pit = core->GetPIT();
    PIT::PIT_State* state = pit->GetState();
    u64 clocks = core->GetScheduler()->GetClocks();
    u8 board = pit->Peek(0x0060, clocks);
    float character = ImGui::CalcTextSize("0").x;

    ImGui::TextColored(cyan, "BOARD (0060)"); ImGui::Separator();

    if (ImGui::BeginTable("##pit_board", 6, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX))
    {
        static const int widths[6] = { 10, 4, 10, 4, 5, 4 };
        bool irq0 = (board & state->timer_enable & 0x03) != 0;

        setup_fixed_columns(widths, 6);

        ImGui::TableNextRow();
        draw_flag("TM0 ENABLE", (state->timer_enable & 0x01) != 0);
        draw_flag("TM1 ENABLE", (state->timer_enable & 0x02) != 0);
        draw_flag("SOUND", state->sound || state->sound_memory);

        ImGui::TableNextRow();
        draw_flag("LATCH 0", (board & 0x01) != 0);
        draw_flag("LATCH 1", (board & 0x02) != 0);
        draw_grid_label("IRQ0");
        ImGui::TextColored(irq0 ? yellow : gray, "%s", irq0 ? "HIGH" : "LOW");

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "COUNTERS"); ImGui::Separator();

    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_NoHostExtendX;

    if (ImGui::BeginTable("##counters", 11, flags))
    {
        static const char* headers[11] = { "CH", "PORT", "USE", "CLOCK", "MODE", "ACCESS", "BCD", "RELOAD", "COUNT", "OUT",
            "PERIOD" };
        static const int widths[11] = { 2, 4, 14, 7, 11, 7, 3, 6, 5, 3, 10 };

        for (int i = 0; i < 11; i++)
            ImGui::TableSetupColumn(headers[i], ImGuiTableColumnFlags_WidthFixed, character * widths[i]);

        ImGui::TableHeadersRow();

        for (int channel = 0; channel < 6; channel++)
        {
            I8253* chip = pit->GetPIT(channel / 3);
            const I8253::I8253_Counter& counter = chip->GetState()->counters[channel % 3];
            u64 tick = pit->GetTick(channel, clocks);
            double rate = channel == k_pit_serial_channel ? 1228800.0 : 307200.0;
            bool programmed = counter.programmed;
            ImVec4 color = programmed ? white : gray;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(orange, "%d", channel);
            ImGui::TableNextColumn();
            ImGui::TextColored(cyan, "%04X", (channel < 3 ? 0x0040 : 0x0050) + (channel % 3) * 2);
            ImGui::TableNextColumn();
            ImGui::TextColored(brown, "%s", k_debug_pit_uses[channel]);
            ImGui::TableNextColumn();
            ImGui::TextColored(orange, "%s", channel == k_pit_serial_channel ? "1.2288M" : "307.2K");
            ImGui::TableNextColumn();

            if (programmed)
            {
                ImGui::TextColored(blue, "%s", k_pit_mode_short_names[counter.mode % 6]);

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", k_debug_pit_mode_names[counter.mode % 6]);
            }
            else
                ImGui::TextColored(gray, "--");

            ImGui::TableNextColumn();
            ImGui::TextColored(color, "%s", programmed ? k_debug_pit_access_names[counter.access & 3] : "--");
            ImGui::TableNextColumn();
            ImGui::TextColored(programmed && counter.bcd ? green : gray, "%s", counter.bcd ? "ON" : "OFF");
            ImGui::TableNextColumn();
            ImGui::TextColored(color, "$%04X", counter.reload);
            ImGui::TableNextColumn();

            if (programmed)
                ImGui::TextColored(white, "$%04X", chip->PeekCount(channel % 3, tick));
            else
                ImGui::TextColored(gray, "--");

            ImGui::TableNextColumn();
            bool out = chip->PeekOutput(channel % 3, tick);
            ImGui::TextColored(programmed && out ? green : gray, "%d", out ? 1 : 0);
            ImGui::TableNextColumn();

            u32 count = counter.bcd ? ((counter.reload >> 12) & 0x0F) * 1000 + ((counter.reload >> 8) & 0x0F) * 100 +
                ((counter.reload >> 4) & 0x0F) * 10 + (counter.reload & 0x0F) : counter.reload;

            if (count == 0)
                count = counter.bcd ? 10000 : 0x10000;

            double frequency = rate / count;

            if (!programmed || !counter.counting || counter.mode == 1 || counter.mode == 5)
                ImGui::TextColored(gray, "--");
            else if ((counter.mode == 2 || counter.mode == 3) && frequency >= 1000.0)
                ImGui::TextColored(white, "%.2f kHz", frequency / 1000.0);
            else if (counter.mode == 2 || counter.mode == 3)
                ImGui::TextColored(white, "%.2f Hz", frequency);
            else
                ImGui::TextColored(white, "%.3f ms", (count * 1000.0) / rate);
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_dma(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(294, 178), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(548, 301), ImGuiCond_FirstUseEver);
    ImGui::Begin("DMA", &config_debug.show_dma);

    ImGui::PushFont(gui_default_font);

    UPD71071::UPD71071_State* state = emu_get_core()->GetDMA()->GetState();
    ImGuiTableFlags grid = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    float character = ImGui::CalcTextSize("0").x;
    float space = ImGui::CalcTextSize(" ").x;

    ImGui::TextColored(cyan, "CONTROLLER (00A0-00AF)"); ImGui::Separator();

    if (ImGui::BeginTable("##dma_device_control", 2, grid))
    {
        static const int widths[2] = { 14, 42 };
        setup_fixed_columns(widths, 2);

        ImGui::TableNextRow();
        draw_grid_label("DEVICE CONTROL");
        ImGui::TextColored(white, "$%04X", state->device_control);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();

        for (int i = 0; i < 10; i++)
        {
            bool set = (state->device_control & (1 << i)) != 0;

            if (i > 0)
                ImGui::SameLine(0.0f, space);

            ImGui::TextColored(set ? (i == 2 ? red : green) : gray, "%s", k_debug_dma_control_names[i]);
        }

        ImGui::EndTable();
    }

    if (ImGui::BeginTable("##dma_controller", 4, grid))
    {
        static const int widths[4] = { 14, 14, 12, 12 };
        bool disabled = (state->device_control & k_upd71071_ddma) != 0;

        setup_fixed_columns(widths, 4);

        ImGui::TableNextRow();
        draw_grid_label("STATE");
        ImGui::TextColored(disabled ? red : green, "%s", disabled ? "DMA DISABLED" : "DMA ENABLED");
        draw_grid_label("BUS");
        ImGui::TextColored(white, "%s", state->bus_16bit ? "16-BIT" : "8-BIT");

        ImGui::TableNextRow();
        draw_grid_label("MASK");
        draw_bits4(state->mask);
        draw_grid_label("SOFTWARE REQ");
        draw_bits4(state->software_requests);

        ImGui::TableNextRow();
        draw_grid_label("REQUEST LEVELS");
        draw_bits4(state->request_levels);
        draw_grid_label("TERMINAL CNT");
        draw_bits4(state->status_tc);

        ImGui::TableNextRow();
        draw_grid_label("SELECTED");
        ImGui::TextColored(orange, "CH%d", state->selected_channel & 3); ImGui::SameLine(0.0f, space);
        ImGui::TextColored(blue, "%s", state->base_access ? "BASE" : "CURRENT");
        draw_grid_label("HIGH ADDRESS");
        ImGui::TextColored(white, "$%02X", state->high_address);

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "CHANNELS"); ImGui::Separator();

    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_NoHostExtendX;

    if (ImGui::BeginTable("##dma_channels", 8, flags))
    {
        static const char* headers[8] = { "CH", "DEVICE", "MODE", "ADDRESS", "COUNT", "MASK", "REQ", "TC" };
        static const int widths[8] = { 2, 7, 28, 9, 11, 4, 3, 2 };

        for (int i = 0; i < 8; i++)
            ImGui::TableSetupColumn(headers[i], ImGuiTableColumnFlags_WidthFixed, character * widths[i]);

        ImGui::TableHeadersRow();

        for (int channel = 0; channel < 4; channel++)
        {
            const UPD71071::UPD71071_Channel& item = state->channels[channel];
            u8 bit = (u8)(1 << channel);
            u8 mode = item.mode;
            bool request = ((state->request_levels | state->software_requests) & bit) != 0;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(orange, "%d", channel);
            ImGui::TableNextColumn();
            ImGui::TextColored(brown, "%s", k_debug_dma_devices[channel]);
            ImGui::TableNextColumn();
            ImGui::TextColored(white, "%-7s %s %-6s %-4s %s", k_debug_dma_direction_names[(mode >> 2) & 3],
                (mode & 0x01) ? "WORD" : "BYTE", k_debug_dma_service_names[(mode >> 6) & 3], (mode & 0x10) ? "AUTO" : "",
                (mode & 0x20) ? "DEC" : "INC");

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::TextColored(cyan, "MODE $%02X", mode);
                ImGui::TextColored(violet, "DIRECTION"); ImGui::SameLine();
                ImGui::TextColored(white, "%s", k_debug_dma_direction_names[(mode >> 2) & 3]);
                ImGui::TextColored(violet, "UNIT     "); ImGui::SameLine();
                ImGui::TextColored(white, "%s", (mode & 0x01) ? "WORD" : "BYTE");
                ImGui::TextColored(violet, "SERVICE  "); ImGui::SameLine();
                ImGui::TextColored(white, "%s", k_debug_dma_service_names[(mode >> 6) & 3]);
                ImGui::TextColored(violet, "AUTO INIT"); ImGui::SameLine();
                ImGui::TextColored((mode & 0x10) ? green : gray, "%s", (mode & 0x10) ? "ON" : "OFF");
                ImGui::TextColored(violet, "ADDRESS  "); ImGui::SameLine();
                ImGui::TextColored(white, "%s", (mode & 0x20) ? "DECREMENT" : "INCREMENT");
                ImGui::EndTooltip();
            }

            ImGui::TableNextColumn();
            ImGui::TextColored(cyan, "$%08X", item.current_address);

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("BASE $%08X", item.base_address);

            if (ImGui::IsItemClicked())
                goto_physical(item.current_address);

            ImGui::TableNextColumn();
            ImGui::TextColored(white, "$%04X/$%04X", item.current_count, item.base_count);
            ImGui::TableNextColumn();
            ImGui::TextColored((state->mask & bit) ? red : gray, "%d", (state->mask & bit) ? 1 : 0);
            ImGui::TableNextColumn();
            ImGui::TextColored(request ? yellow : gray, "%d", request ? 1 : 0);
            ImGui::TableNextColumn();
            ImGui::TextColored((state->status_tc & bit) ? green : gray, "%d", (state->status_tc & bit) ? 1 : 0);
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

u16 gui_debug_system_machine_id(void)
{
    GeartownsCore* core = emu_get_core();
    u64 clocks = core->GetScheduler()->GetClocks();
    u8 low = 0;
    u8 high = 0;

    core->GetIO()->Peek(0x0030, clocks, low);
    core->GetIO()->Peek(0x0031, clocks, high);
    return (u16)((high << 8) | low);
}

void gui_debug_system_serial_rom_text(char* text, size_t text_size)
{
    const u8* rom = emu_get_core()->GetSystemControl()->GetSerialRom();
    char maker[8];

    for (int i = 0; i < 7; i++)
    {
        char value = (char)(((rom[i] & 0x0F) << 4) | (rom[i + 1] >> 4));
        maker[i] = value >= 32 && value < 127 ? value : '.';
    }

    maker[7] = 0;
    snprintf(text, text_size, "%s, MODEL %02X%02X", maker, rom[23], rom[24]);
}

void gui_debug_window_rtc(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(135, 245), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(277, 317), ImGuiCond_FirstUseEver);
    ImGui::Begin("RTC", &config_debug.show_rtc);

    ImGui::PushFont(gui_default_font);

    GeartownsCore* core = emu_get_core();
    MSM58321 rtc(*core->GetRTC());
    rtc.Synchronize(core->GetScheduler()->GetClocks());
    MSM58321::MSM58321_State* state = rtc.GetState();
    const u8* r = state->registers;
    bool hour24 = (r[5] & k_msm58321_24_hour) != 0;
    bool pm = (r[5] & k_msm58321_pm) != 0;
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    static const int widths[2] = { 7, 22 };
    float space = ImGui::CalcTextSize(" ").x;

    ImGui::TextColored(cyan, "DATE AND TIME"); ImGui::Separator();

    if (ImGui::BeginTable("##rtc_date", 2, flags))
    {
        setup_fixed_columns(widths, 2);

        ImGui::TableNextRow();
        draw_grid_label("DATE");
        ImGui::TextColored(white, "%d%d-%d%d-%d%d", r[12], r[11], r[10] & 0x01, r[9], r[8] & 0x03, r[7]);

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("YY-MM-DD, the MSM58321 keeps a two-digit year");

        ImGui::SameLine(0.0f, space);
        ImGui::TextColored(blue, "%s", k_debug_weekday_names[r[6] % 7]);

        ImGui::TableNextRow();
        draw_grid_label("TIME");
        ImGui::TextColored(white, "%d%d:%d%d:%d%d", r[5] & 0x03, r[4], r[3] & 0x07, r[2], r[1] & 0x07, r[0]);
        ImGui::SameLine(0.0f, space);
        ImGui::TextColored(blue, "%s", hour24 ? "24H" : "12H"); ImGui::SameLine(0.0f, space);
        ImGui::TextColored(!hour24 && pm ? green : gray, "PM");

        ImGui::TableNextRow();
        draw_grid_label("LEAP");
        ImGui::TextColored(white, "YEAR %% 4 = %d", (r[8] >> 2) & 0x03);

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "INTERFACE (0070/0080)"); ImGui::Separator();

    if (ImGui::BeginTable("##rtc_interface", 2, flags))
    {
        setup_fixed_columns(widths, 2);

        ImGui::TableNextRow();
        draw_grid_label("ADDRESS");
        ImGui::TextColored(white, "$%X", state->address & 0x0F);

        ImGui::TableNextRow();
        draw_grid_label("DATA");
        ImGui::TextColored(white, "$%X", state->data & 0x0F);

        ImGui::TableNextRow();
        draw_grid_label("COMMAND");
        ImGui::TextColored(white, "$%02X", state->command); ImGui::SameLine(0.0f, space);
        ImGui::TextColored((state->command & 0x80) ? green : gray, "CS"); ImGui::SameLine(0.0f, space);
        ImGui::TextColored((state->command & 0x04) ? green : gray, "READ"); ImGui::SameLine(0.0f, space);
        ImGui::TextColored((state->command & 0x02) ? green : gray, "WRITE"); ImGui::SameLine(0.0f, space);
        ImGui::TextColored((state->command & 0x01) ? green : gray, "ADDR");

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "REGISTERS"); ImGui::Separator();

    if (ImGui::BeginTable("##rtc_registers", 4, flags))
    {
        static const int register_widths[4] = { 9, 9, 9, 9 };
        setup_fixed_columns(register_widths, 4);

        for (int i = 0; i < 16; i++)
        {
            u8 value = i == k_msm58321_divider_reset ? 0 : r[i];

            if ((i & 3) == 0)
                ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextColored(cyan, "%X", i); ImGui::SameLine(0.0f, space);
            ImGui::TextColored(violet, "%-5s", k_debug_rtc_register_names[i]); ImGui::SameLine(0.0f, space);
            ImGui::TextColored(i >= k_msm58321_divider_reset ? gray : white, "%X", value & 0x0F);
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_system_control(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(176, 112), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(236, 480), ImGuiCond_FirstUseEver);
    ImGui::Begin("System Control", &config_debug.show_system_control);

    ImGui::PushFont(gui_default_font);

    GeartownsCore* core = emu_get_core();
    const GT_Machine_Config& machine = core->GetMachineConfig();
    SystemControl::SystemControl_State* control = core->GetSystemControl()->GetState();
    Memory::Memory_State* memory = core->GetMemory()->GetState();
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    static const int widths[2] = { 10, 22 };
    float space = ImGui::CalcTextSize(" ").x;

    ImGui::TextColored(cyan, "MACHINE"); ImGui::Separator();

    if (ImGui::BeginTable("##system_machine", 2, flags))
    {
        setup_fixed_columns(widths, 2);

        ImGui::TableNextRow();
        draw_grid_label("MODEL");
        ImGui::TextColored(white, "%s", k_machine_profiles[machine.model].name);

        ImGui::TableNextRow();
        draw_grid_label("CPU");
        ImGui::TextColored(white, "%s", k_machine_cpus[machine.cpu].name); ImGui::SameLine(0.0f, space);
        ImGui::TextColored(orange, "%.0f MHz", machine.cpu_clock_rate / 1000000.0);

        ImGui::TableNextRow();
        draw_grid_label("RAM");
        ImGui::TextColored(white, "%u KB", machine.ram_size / 1024);

        ImGui::TableNextRow();
        draw_grid_label("FLOPPIES");
        ImGui::TextColored(white, "%d", machine.floppy_drives);

        ImGui::TableNextRow();
        draw_grid_label("MACHINE ID");
        ImGui::TextColored(white, "$%04X", gui_debug_system_machine_id()); ImGui::SameLine(0.0f, space);
        ImGui::TextColored(gray, "(0030/0031)");

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "RESET (0020/0022)"); ImGui::Separator();

    if (ImGui::BeginTable("##system_reset", 2, flags))
    {
        setup_fixed_columns(widths, 2);

        ImGui::TableNextRow();
        draw_grid_label("CAUSE");
        ImGui::TextColored(white, "$%02X", control->reset_cause); ImGui::SameLine(0.0f, space);
        ImGui::TextColored((control->reset_cause & k_system_control_reset_soft) ? green : gray, "SOFT");
        ImGui::SameLine(0.0f, space);
        ImGui::TextColored((control->reset_cause & k_system_control_reset_shutdown) ? green : gray, "SHUTDOWN");

        ImGui::TableNextRow();
        draw_grid_label("PENDING");
        ImGui::TextColored(control->reset_pending ? yellow : gray, "%s", control->reset_pending ? "YES" : "NO");

        ImGui::TableNextRow();
        draw_grid_label("POWER OFF");
        ImGui::TextColored(control->power_off ? red : gray, "%s", control->power_off ? "REQUESTED" : "NO");

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "MEMORY MAP"); ImGui::Separator();

    if (ImGui::BeginTable("##system_memory", 3, flags))
    {
        static const int memory_widths[3] = { 10, 12, 6 };
        setup_fixed_columns(memory_widths, 3);

        ImGui::TableNextRow();
        draw_grid_label("LOW WINDOW");
        ImGui::TextColored(blue, "%s", memory->main_memory ? "MAIN RAM" : "FM-R DEVICES");
        ImGui::TableNextColumn();
        ImGui::TextColored(gray, "(0404)");

        ImGui::TableNextRow();
        draw_grid_label("F8000");
        ImGui::TextColored(blue, "%s", memory->boot_ram ? "RAM" : "BOOT ROM");
        ImGui::TableNextColumn();
        ImGui::TextColored(gray, "(0480)");

        ImGui::TableNextRow();
        draw_grid_label("DICTIONARY");
        ImGui::TextColored(memory->dictionary ? green : gray, "%s", memory->dictionary ? "ON" : "OFF");
        ImGui::SameLine(0.0f, space);
        ImGui::TextColored(violet, "BANK"); ImGui::SameLine(0.0f, space);
        ImGui::TextColored(orange, "%d", memory->dictionary_bank & 0x0F);
        ImGui::TableNextColumn();
        ImGui::TextColored(gray, "(0484)");

        ImGui::TableNextRow();
        draw_grid_label("CMOS WP");
        ImGui::TextColored(control->write_protect ? yellow : gray, "%s", control->write_protect ? "ON" : "OFF");
        ImGui::TableNextColumn();
        ImGui::TextColored(gray, "(0020)");

        ImGui::TableNextRow();
        draw_grid_label("UNDOC");
        ImGui::TextColored(white, "$%02X", control->port_05e0);
        ImGui::TableNextColumn();
        ImGui::TextColored(gray, "(05E0)");

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "SERIAL ROM (0032)"); ImGui::Separator();

    if (ImGui::BeginTable("##system_serial_rom", 2, flags))
    {
        setup_fixed_columns(widths, 2);

        ImGui::TableNextRow();
        draw_grid_label("CONTROL");
        ImGui::TextColored(white, "$%02X", control->serial_rom_control); ImGui::SameLine(0.0f, space);
        ImGui::TextColored((control->serial_rom_control & 0x80) ? green : gray, "RESET"); ImGui::SameLine(0.0f, space);
        ImGui::TextColored((control->serial_rom_control & 0x40) ? green : gray, "CLK"); ImGui::SameLine(0.0f, space);
        ImGui::TextColored((control->serial_rom_control & 0x20) ? gray : green, "CS");

        ImGui::TableNextRow();
        draw_grid_label("BIT");
        ImGui::TextColored(white, "%d", control->serial_rom_bit);

        ImGui::TableNextRow();
        draw_grid_label("DATA");
        ImGui::TextColored(white, "%d", core->GetSystemControl()->Peek(0x0032) & 0x01);

        char contents[32];
        gui_debug_system_serial_rom_text(contents, sizeof(contents));

        ImGui::TableNextRow();
        draw_grid_label("CONTENTS");
        ImGui::TextColored(white, "%s", contents);

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

static void setup_fixed_columns(const int* widths, int count)
{
    float character = ImGui::CalcTextSize("0").x;

    for (int i = 0; i < count; i++)
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * widths[i]);
}

static void draw_grid_label(const char* label)
{
    ImGui::TableNextColumn();
    ImGui::TextColored(violet, "%s", label);
    ImGui::TableNextColumn();
}

static void draw_flag(const char* label, bool on)
{
    draw_grid_label(label);
    ImGui::TextColored(on ? green : gray, "%s", on ? "ON" : "OFF");
}

static void draw_bits4(u8 value)
{
    ImGui::TextColored(white, "$%X", value & 0x0F); ImGui::SameLine(0.0f, ImGui::CalcTextSize(" ").x);
    ImGui::TextColored(gray, "(%d%d%d%d)", (value >> 3) & 1, (value >> 2) & 1, (value >> 1) & 1, value & 1);
}

static void draw_vector_tooltip(u8 vector)
{
    GuiDebugDescriptor gate;
    char name[16];
    char description[64];
    gui_debug_i386_vector_name(vector, name, sizeof(name), description, sizeof(description));

    ImGui::BeginTooltip();
    ImGui::TextColored(cyan, "VECTOR $%02X", vector);
    ImGui::Text("%s", description);

    if (!gui_debug_i386_read_table_entry(GuiDebugDescriptorTable_IDT, vector, gate))
        ImGui::TextColored(gray, "Gate unavailable");
    else if (gui_debug_i386_idt_is_ivt())
        ImGui::Text("IVT %04X:%04X (linear $%08X)", gate.gate_selector, gate.gate_offset, gate.base);
    else
    {
        char type[32];
        gui_debug_i386_descriptor_type(gate, type, sizeof(type));
        ImGui::Text("%s %04X:%08X%s", type, gate.gate_selector, gate.gate_offset, gate.present ? "" : " NOT PRESENT");
    }

    ImGui::EndTooltip();
}

static void draw_pic_column(const I8259::I8259_State* chip, bool master, int row)
{
    float space = ImGui::CalcTextSize(" ").x;

    switch (row)
    {
        case 0:
            ImGui::TextColored(white, "$%02X", chip->icw1); ImGui::SameLine(0.0f, space);
            ImGui::TextColored(blue, "%s %s", (chip->icw1 & k_i8259_icw1_ltim) ? "LEVEL" : "EDGE",
                (chip->icw1 & k_i8259_icw1_sngl) ? "SINGLE" : "CASCADE");
            break;
        case 1:
            ImGui::TextColored(white, "$%02X", chip->icw2); ImGui::SameLine(0.0f, space);
            ImGui::TextColored(violet, "BASE"); ImGui::SameLine(0.0f, space);
            ImGui::TextColored(white, "$%02X", chip->icw2 & 0xF8);
            break;
        case 2:
            ImGui::TextColored(white, "$%02X", chip->icw3); ImGui::SameLine(0.0f, space);

            if (master)
                ImGui::TextColored(violet, "SLAVE MASK");
            else
            {
                ImGui::TextColored(violet, "ID"); ImGui::SameLine(0.0f, space);
                ImGui::TextColored(white, "%d", chip->icw3 & 0x07);
            }

            break;
        case 3:
            ImGui::TextColored(white, "$%02X", chip->icw4); ImGui::SameLine(0.0f, space);
            ImGui::TextColored((chip->icw4 & k_i8259_icw4_upm) ? green : gray, "uPM"); ImGui::SameLine(0.0f, space);
            ImGui::TextColored((chip->icw4 & k_i8259_icw4_aeoi) ? green : gray, "AEOI"); ImGui::SameLine(0.0f, space);
            ImGui::TextColored((chip->icw4 & 0x08) ? green : gray, "BUF"); ImGui::SameLine(0.0f, space);
            ImGui::TextColored((chip->icw4 & k_i8259_icw4_sfnm) ? green : gray, "SFNM");
            break;
        case 4:
            if (chip->init_step == I8259::I8259_INIT_READY)
                ImGui::TextColored(green, "READY");
            else
                ImGui::TextColored(yellow, "WAIT %s", k_debug_pic_init_names[chip->init_step & 3]);

            break;
        case 5:
            ImGui::TextColored(blue, "%s", chip->read_isr ? "ISR" : "IRR");
            break;
        case 6:
            ImGui::TextColored(chip->special_mask ? green : gray, "%s", chip->special_mask ? "ON" : "OFF");
            break;
        case 7:
            ImGui::TextColored(chip->poll_pending ? yellow : gray, "%s", chip->poll_pending ? "ARMED" : "OFF");
            break;
        case 8:
            ImGui::TextColored(white, "IR%d", (chip->lowest_priority + 1) & 7); ImGui::SameLine(0.0f, space);
            ImGui::TextColored(gray, "FIRST");
            break;
        case 9:
            ImGui::TextColored(chip->rotate_on_aeoi ? green : gray, "%s", chip->rotate_on_aeoi ? "ON" : "OFF");
            break;
        default:
            ImGui::TextColored(chip->int_output ? yellow : gray, "%s", chip->int_output ? "HIGH" : "LOW");
            break;
    }
}

static void goto_physical(u32 address)
{
    GT_Debug_Memory_Address target = { };
    target.space = GT_DEBUG_MEMORY_PHYSICAL;
    target.address = address;
    target.segment_register = -1;
    target.region = -1;
    gui_debug_memory_goto(target);
}
