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

static void draw_flag(const char* name, bool on);
static void draw_bits4(u8 value);
static void draw_vector_tooltip(u8 vector);
static void draw_pic_column(const I8259::I8259_State* chip, bool master, int row);
static void goto_physical(u32 address);

void gui_debug_window_pic(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(440, 540), ImGuiCond_FirstUseEver);
    ImGui::Begin("Interrupts", &config_debug.show_pic);

    ImGui::PushFont(gui_default_font);

    PIC* pic = emu_get_core()->GetPIC();
    I8259::I8259_State* master = pic->GetMaster()->GetState();
    I8259::I8259_State* slave = pic->GetSlave()->GetState();

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
            ImGui::TextColored(orange, "%2d", irq);
            ImGui::TableNextColumn();
            ImGui::TextColored(brown, "%-13s", k_debug_irq_sources[irq]);
            ImGui::TableNextColumn();
            ImGui::TextColored((chip->input_levels & bit) ? green : gray, " %s ", (chip->input_levels & bit) ? "HI" : "LO");
            ImGui::TableNextColumn();
            ImGui::TextColored((chip->irr & bit) ? yellow : gray, " %d ", (chip->irr & bit) ? 1 : 0);
            ImGui::TableNextColumn();
            ImGui::TextColored((chip->isr & bit) ? green : gray, " %d  ", (chip->isr & bit) ? 1 : 0);
            ImGui::TableNextColumn();
            ImGui::TextColored((chip->imr & bit) ? red : gray, " %d  ", (chip->imr & bit) ? 1 : 0);
            ImGui::TableNextColumn();
            ImGui::TextColored(white, " $%02X ", vector);

            if (ImGui::IsItemHovered())
                draw_vector_tooltip(vector);
        }

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "CONTROLLERS"); ImGui::Separator();

    if (ImGui::BeginTable("##pics", 3, flags))
    {
        ImGui::TableSetupColumn(" ");
        ImGui::TableSetupColumn("MASTER");
        ImGui::TableSetupColumn("SLAVE");
        ImGui::TableHeadersRow();

        static const char* rows[] = { "ICW1", "ICW2", "ICW3", "ICW4", "INIT", "READ REG", "SPEC MASK", "POLL",
            "PRIORITY", "AUTO ROTATE", "INT OUTPUT" };

        for (int row = 0; row < (int)(sizeof(rows) / sizeof(rows[0])); row++)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(violet, "%-11s", rows[row]);
            ImGui::TableNextColumn();
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
    ImGui::SetNextWindowPos(ImVec2(90, 90), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Timers", &config_debug.show_pit);

    ImGui::PushFont(gui_default_font);

    GeartownsCore* core = emu_get_core();
    PIT* pit = core->GetPIT();
    PIT::PIT_State* state = pit->GetState();
    u64 clocks = core->GetScheduler()->GetClocks();
    u8 board = pit->Peek(0x0060, clocks);

    ImGui::TextColored(cyan, "BOARD (0060)"); ImGui::Separator();

    draw_flag("TM0 ENABLE", (state->timer_enable & 0x01) != 0); ImGui::SameLine();
    draw_flag(" TM1 ENABLE", (state->timer_enable & 0x02) != 0); ImGui::SameLine();
    draw_flag(" SOUND", state->sound || state->sound_memory);

    draw_flag("LATCH 0   ", (board & 0x01) != 0); ImGui::SameLine();
    draw_flag(" LATCH 1   ", (board & 0x02) != 0); ImGui::SameLine();
    ImGui::TextColored(violet, " IRQ0"); ImGui::SameLine();
    bool irq0 = (board & state->timer_enable & 0x03) != 0;
    ImGui::TextColored(irq0 ? yellow : gray, "%s", irq0 ? "HIGH" : "LOW ");

    ImGui::NewLine(); ImGui::TextColored(cyan, "COUNTERS"); ImGui::Separator();

    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_NoHostExtendX;

    if (ImGui::BeginTable("##counters", 11, flags))
    {
        static const char* headers[] = { "CH", "PORT", "USE", "CLOCK", "MODE", "ACCESS", "BCD", "RELOAD", "COUNT", "OUT",
            "PERIOD" };

        for (int i = 0; i < 11; i++)
            ImGui::TableSetupColumn(headers[i]);

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
            ImGui::TextColored(brown, "%-14s", k_debug_pit_uses[channel]);
            ImGui::TableNextColumn();
            ImGui::TextColored(orange, "%s", channel == k_pit_serial_channel ? "1.2288M" : "307.2K ");
            ImGui::TableNextColumn();

            if (programmed)
            {
                ImGui::TextColored(blue, "%-11s", k_pit_mode_short_names[counter.mode % 6]);

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", k_debug_pit_mode_names[counter.mode % 6]);
            }
            else
                ImGui::TextColored(gray, "--         ");

            ImGui::TableNextColumn();
            ImGui::TextColored(color, "%-7s", programmed ? k_debug_pit_access_names[counter.access & 3] : "--");
            ImGui::TableNextColumn();
            ImGui::TextColored(programmed && counter.bcd ? green : gray, "%s", counter.bcd ? "ON " : "OFF");
            ImGui::TableNextColumn();
            ImGui::TextColored(color, "$%04X", counter.reload);
            ImGui::TableNextColumn();

            if (programmed)
                ImGui::TextColored(white, "$%04X", chip->PeekCount(channel % 3, tick));
            else
                ImGui::TextColored(gray, "--   ");

            ImGui::TableNextColumn();
            bool out = chip->PeekOutput(channel % 3, tick);
            ImGui::TextColored(programmed && out ? green : gray, "%d", out ? 1 : 0);
            ImGui::TableNextColumn();

            u32 count = counter.bcd ? ((counter.reload >> 12) & 0x0F) * 1000 + ((counter.reload >> 8) & 0x0F) * 100 +
                ((counter.reload >> 4) & 0x0F) * 10 + (counter.reload & 0x0F) : counter.reload;

            if (count == 0)
                count = counter.bcd ? 10000 : 0x10000;

            if (!programmed || !counter.counting || counter.mode == 1 || counter.mode == 5)
                ImGui::TextColored(gray, "--         ");
            else if (counter.mode == 2 || counter.mode == 3)
                ImGui::TextColored(white, "%9.2f Hz", rate / count);
            else
                ImGui::TextColored(white, "%9.3f ms", (count * 1000.0) / rate);
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
    ImGui::SetNextWindowPos(ImVec2(120, 120), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 320), ImGuiCond_FirstUseEver);
    ImGui::Begin("DMA", &config_debug.show_dma);

    ImGui::PushFont(gui_default_font);

    UPD71071::UPD71071_State* state = emu_get_core()->GetDMA()->GetState();

    ImGui::TextColored(cyan, "CONTROLLER (00A0-00AF)"); ImGui::Separator();

    ImGui::TextColored(violet, "DEVICE CONTROL "); ImGui::SameLine();
    ImGui::TextColored(white, "$%04X ", state->device_control); ImGui::SameLine(0, 0);

    for (int i = 0; i < 10; i++)
    {
        bool set = (state->device_control & (1 << i)) != 0;
        ImGui::SameLine();
        ImGui::TextColored(set ? (i == 2 ? red : green) : gray, "%s", k_debug_dma_control_names[i]);
    }

    ImGui::TextColored(violet, "STATE          "); ImGui::SameLine();
    bool disabled = (state->device_control & k_upd71071_ddma) != 0;
    ImGui::TextColored(disabled ? red : green, "%s", disabled ? "DMA DISABLED" : "DMA ENABLED "); ImGui::SameLine();
    ImGui::TextColored(violet, "  BUS"); ImGui::SameLine();
    ImGui::TextColored(white, "%s", state->bus_16bit ? "16-BIT" : "8-BIT ");

    ImGui::TextColored(violet, "MASK           "); ImGui::SameLine();
    draw_bits4(state->mask); ImGui::SameLine();
    ImGui::TextColored(violet, "   SOFTWARE REQ"); ImGui::SameLine();
    draw_bits4(state->software_requests);

    ImGui::TextColored(violet, "REQUEST LEVELS "); ImGui::SameLine();
    draw_bits4(state->request_levels); ImGui::SameLine();
    ImGui::TextColored(violet, "   TERMINAL CNT"); ImGui::SameLine();
    draw_bits4(state->status_tc);

    ImGui::TextColored(violet, "SELECTED       "); ImGui::SameLine();
    ImGui::TextColored(orange, "CH%d", state->selected_channel & 3); ImGui::SameLine();
    ImGui::TextColored(blue, "%s", state->base_access ? "BASE   " : "CURRENT"); ImGui::SameLine();
    ImGui::TextColored(violet, " HIGH ADDRESS"); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", state->high_address);

    ImGui::NewLine(); ImGui::TextColored(cyan, "CHANNELS"); ImGui::Separator();

    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_NoHostExtendX;

    if (ImGui::BeginTable("##dma_channels", 8, flags))
    {
        static const char* headers[] = { "CH", "DEVICE", "MODE", "ADDRESS", "COUNT", "MASK", "REQ", "TC" };

        for (int i = 0; i < 8; i++)
            ImGui::TableSetupColumn(headers[i]);

        ImGui::TableHeadersRow();

        for (int channel = 0; channel < 4; channel++)
        {
            const UPD71071::UPD71071_Channel& item = state->channels[channel];
            u8 bit = (u8)(1 << channel);
            u8 mode = item.mode;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(orange, "%d", channel);
            ImGui::TableNextColumn();
            ImGui::TextColored(brown, "%-7s", k_debug_dma_devices[channel]);
            ImGui::TableNextColumn();
            ImGui::TextColored(white, "%-7s %s %-6s %s %s", k_debug_dma_direction_names[(mode >> 2) & 3],
                (mode & 0x01) ? "WORD" : "BYTE", k_debug_dma_service_names[(mode >> 6) & 3], (mode & 0x10) ? "AUTO" : "    ",
                (mode & 0x20) ? "DEC" : "INC");

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::TextColored(cyan, "MODE $%02X", mode);
                ImGui::Text("Direction: %s", k_debug_dma_direction_names[(mode >> 2) & 3]);
                ImGui::Text("Unit: %s", (mode & 0x01) ? "word" : "byte");
                ImGui::Text("Service: %s", k_debug_dma_service_names[(mode >> 6) & 3]);
                ImGui::Text("Auto-initialize: %s", (mode & 0x10) ? "on" : "off");
                ImGui::Text("Address: %s", (mode & 0x20) ? "decrement" : "increment");
                ImGui::EndTooltip();
            }

            ImGui::TableNextColumn();
            ImGui::TextColored(cyan, "$%08X", item.current_address);

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Base $%08X", item.base_address);

            if (ImGui::IsItemClicked())
                goto_physical(item.current_address);

            ImGui::TableNextColumn();
            ImGui::TextColored(white, "$%04X/$%04X", item.current_count, item.base_count);
            ImGui::TableNextColumn();
            ImGui::TextColored((state->mask & bit) ? red : gray, " %d  ", (state->mask & bit) ? 1 : 0);
            ImGui::TableNextColumn();
            bool request = ((state->request_levels | state->software_requests) & bit) != 0;
            ImGui::TextColored(request ? yellow : gray, " %d ", request ? 1 : 0);
            ImGui::TableNextColumn();
            ImGui::TextColored((state->status_tc & bit) ? green : gray, "%d", (state->status_tc & bit) ? 1 : 0);
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_rtc(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(150, 150), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260, 320), ImGuiCond_FirstUseEver);
    ImGui::Begin("RTC", &config_debug.show_rtc);

    ImGui::PushFont(gui_default_font);

    GeartownsCore* core = emu_get_core();
    MSM58321 rtc(*core->GetRTC());
    rtc.Synchronize(core->GetScheduler()->GetClocks());
    MSM58321::MSM58321_State* state = rtc.GetState();
    const u8* r = state->registers;
    bool hour24 = (r[5] & k_msm58321_24_hour) != 0;
    bool pm = (r[5] & k_msm58321_pm) != 0;

    ImGui::TextColored(cyan, "DATE AND TIME"); ImGui::Separator();

    ImGui::TextColored(violet, "DATE   "); ImGui::SameLine();
    ImGui::TextColored(white, "%d%d-%d%d-%d%d", r[12], r[11], r[10] & 0x01, r[9], r[8] & 0x03, r[7]); ImGui::SameLine();
    ImGui::TextColored(blue, "%s", k_debug_weekday_names[r[6] % 7]);

    ImGui::TextColored(violet, "TIME   "); ImGui::SameLine();
    ImGui::TextColored(white, "%d%d:%d%d:%d%d", r[5] & 0x03, r[4], r[3] & 0x07, r[2], r[1] & 0x07, r[0]); ImGui::SameLine();
    ImGui::TextColored(blue, "%s", hour24 ? "24H" : "12H"); ImGui::SameLine();
    ImGui::TextColored(!hour24 && pm ? green : gray, "PM");

    ImGui::TextColored(violet, "LEAP   "); ImGui::SameLine();
    ImGui::TextColored(white, "YEAR %% 4 = %d", (r[8] >> 2) & 0x03);

    ImGui::NewLine(); ImGui::TextColored(cyan, "INTERFACE (0070/0080)"); ImGui::Separator();

    ImGui::TextColored(violet, "ADDRESS"); ImGui::SameLine();
    ImGui::TextColored(white, "$%X", state->address & 0x0F); ImGui::SameLine();
    ImGui::TextColored(violet, " DATA"); ImGui::SameLine();
    ImGui::TextColored(white, "$%X", state->data & 0x0F);

    ImGui::TextColored(violet, "COMMAND"); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", state->command); ImGui::SameLine();
    ImGui::TextColored((state->command & 0x80) ? green : gray, "CS"); ImGui::SameLine();
    ImGui::TextColored((state->command & 0x04) ? green : gray, "READ"); ImGui::SameLine();
    ImGui::TextColored((state->command & 0x02) ? green : gray, "WRITE"); ImGui::SameLine();
    ImGui::TextColored((state->command & 0x01) ? green : gray, "ADDR");

    ImGui::NewLine(); ImGui::TextColored(cyan, "REGISTERS"); ImGui::Separator();

    for (int i = 0; i < 16; i++)
    {
        if ((i & 3) != 0)
            ImGui::SameLine();

        u8 value = i == k_msm58321_divider_reset ? 0 : r[i];

        ImGui::TextColored(cyan, "%X", i); ImGui::SameLine(0, 4);
        ImGui::TextColored(violet, "%-5s", k_debug_rtc_register_names[i]); ImGui::SameLine(0, 2);
        ImGui::TextColored(i >= k_msm58321_divider_reset ? gray : white, "%X", value & 0x0F);
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_system_control(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(180, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 460), ImGuiCond_FirstUseEver);
    ImGui::Begin("System Control", &config_debug.show_system_control);

    ImGui::PushFont(gui_default_font);

    GeartownsCore* core = emu_get_core();
    const GT_Machine_Config& machine = core->GetMachineConfig();
    SystemControl::SystemControl_State* control = core->GetSystemControl()->GetState();
    Memory::Memory_State* memory = core->GetMemory()->GetState();

    ImGui::TextColored(cyan, "MACHINE"); ImGui::Separator();

    ImGui::TextColored(violet, "MODEL      "); ImGui::SameLine();
    ImGui::TextColored(white, "%s", k_machine_profiles[machine.model].name);
    ImGui::TextColored(violet, "CPU        "); ImGui::SameLine();
    ImGui::TextColored(white, "%s", k_machine_cpus[machine.cpu].name); ImGui::SameLine();
    ImGui::TextColored(orange, "%.0f MHz", machine.cpu_clock_rate / 1000000.0);
    ImGui::TextColored(violet, "RAM        "); ImGui::SameLine();
    ImGui::TextColored(white, "%u KB", machine.ram_size / 1024);
    ImGui::TextColored(violet, "FLOPPIES   "); ImGui::SameLine();
    ImGui::TextColored(white, "%d", machine.floppy_drives);
    ImGui::TextColored(violet, "MACHINE ID "); ImGui::SameLine();
    ImGui::TextColored(white, "$0101"); ImGui::SameLine();
    ImGui::TextColored(gray, "(0030/0031)");

    ImGui::NewLine(); ImGui::TextColored(cyan, "RESET (0020/0022)"); ImGui::Separator();

    ImGui::TextColored(violet, "CAUSE      "); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", control->reset_cause); ImGui::SameLine();
    ImGui::TextColored((control->reset_cause & k_system_control_reset_soft) ? green : gray, "SOFT"); ImGui::SameLine();
    ImGui::TextColored((control->reset_cause & k_system_control_reset_shutdown) ? green : gray, "SHUTDOWN");
    ImGui::TextColored(violet, "PENDING    "); ImGui::SameLine();
    ImGui::TextColored(control->reset_pending ? yellow : gray, "%s", control->reset_pending ? "YES" : "NO ");
    ImGui::TextColored(violet, "POWER OFF  "); ImGui::SameLine();
    ImGui::TextColored(control->power_off ? red : gray, "%s", control->power_off ? "REQUESTED" : "NO       ");

    ImGui::NewLine(); ImGui::TextColored(cyan, "MEMORY MAP"); ImGui::Separator();

    ImGui::TextColored(violet, "LOW WINDOW "); ImGui::SameLine();
    ImGui::TextColored(blue, "%s", memory->main_memory ? "MAIN RAM    " : "FM-R DEVICES"); ImGui::SameLine();
    ImGui::TextColored(gray, "0404");
    ImGui::TextColored(violet, "F8000      "); ImGui::SameLine();
    ImGui::TextColored(blue, "%s", memory->boot_ram ? "RAM         " : "BOOT ROM    "); ImGui::SameLine();
    ImGui::TextColored(gray, "0480");
    ImGui::TextColored(violet, "DICTIONARY "); ImGui::SameLine();
    ImGui::TextColored(memory->dictionary ? green : gray, "%s", memory->dictionary ? "ON " : "OFF"); ImGui::SameLine();
    ImGui::TextColored(violet, " BANK"); ImGui::SameLine();
    ImGui::TextColored(orange, "%2d", memory->dictionary_bank & 0x0F); ImGui::SameLine();
    ImGui::TextColored(gray, "     0484");
    ImGui::TextColored(violet, "CMOS WP    "); ImGui::SameLine();
    ImGui::TextColored(control->write_protect ? yellow : gray, "%s", control->write_protect ? "ON " : "OFF"); ImGui::SameLine();
    ImGui::TextColored(gray, "         0020");
    ImGui::TextColored(violet, "UNDOC      "); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", control->port_05e0); ImGui::SameLine();
    ImGui::TextColored(gray, "         05E0");

    ImGui::NewLine(); ImGui::TextColored(cyan, "SERIAL ROM (0032)"); ImGui::Separator();

    ImGui::TextColored(violet, "CONTROL    "); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", control->serial_rom_control); ImGui::SameLine();
    ImGui::TextColored((control->serial_rom_control & 0x80) ? green : gray, "RESET"); ImGui::SameLine();
    ImGui::TextColored((control->serial_rom_control & 0x40) ? green : gray, "CLK"); ImGui::SameLine();
    ImGui::TextColored((control->serial_rom_control & 0x20) ? gray : green, "CS");
    ImGui::TextColored(violet, "BIT        "); ImGui::SameLine();
    ImGui::TextColored(white, "%3d", control->serial_rom_bit); ImGui::SameLine();
    ImGui::TextColored(violet, " DATA"); ImGui::SameLine();
    ImGui::TextColored(white, "%d", core->GetSystemControl()->Peek(0x0032) & 0x01);
    ImGui::TextColored(violet, "CONTENTS   "); ImGui::SameLine();
    ImGui::TextColored(white, "FUJITSU, MODEL 0101");

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

static void draw_flag(const char* name, bool on)
{
    ImGui::TextColored(violet, "%s", name); ImGui::SameLine();
    ImGui::TextColored(on ? green : gray, "%s", on ? "ON " : "OFF");
}

static void draw_bits4(u8 value)
{
    ImGui::TextColored(white, "$%X ", value & 0x0F); ImGui::SameLine(0, 0);
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
    else if (emu_get_core()->GetI386()->GetState()->execution_mode != I386_MODE_PROTECTED)
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
    switch (row)
    {
        case 0:
            ImGui::TextColored(white, "$%02X", chip->icw1); ImGui::SameLine();
            ImGui::TextColored(blue, "%s %s", (chip->icw1 & k_i8259_icw1_ltim) ? "LEVEL" : "EDGE ",
                (chip->icw1 & k_i8259_icw1_sngl) ? "SINGLE " : "CASCADE");
            break;
        case 1:
            ImGui::TextColored(white, "$%02X", chip->icw2); ImGui::SameLine();
            ImGui::TextColored(violet, "BASE"); ImGui::SameLine();
            ImGui::TextColored(white, "$%02X     ", chip->icw2 & 0xF8);
            break;
        case 2:
            ImGui::TextColored(white, "$%02X", chip->icw3); ImGui::SameLine();

            if (master)
                ImGui::TextColored(violet, "SLAVE MASK   ");
            else
                ImGui::TextColored(violet, "ID %d         ", chip->icw3 & 0x07);

            break;
        case 3:
            ImGui::TextColored(white, "$%02X", chip->icw4); ImGui::SameLine();
            ImGui::TextColored((chip->icw4 & k_i8259_icw4_upm) ? green : gray, "uPM"); ImGui::SameLine();
            ImGui::TextColored((chip->icw4 & k_i8259_icw4_aeoi) ? green : gray, "AEOI"); ImGui::SameLine();
            ImGui::TextColored((chip->icw4 & 0x08) ? green : gray, "BUF"); ImGui::SameLine();
            ImGui::TextColored((chip->icw4 & k_i8259_icw4_sfnm) ? green : gray, "SFNM");
            break;
        case 4:
            if (chip->init_step == I8259::I8259_INIT_READY)
                ImGui::TextColored(green, "READY        ");
            else
                ImGui::TextColored(yellow, "WAIT %-8s", k_debug_pic_init_names[chip->init_step & 3]);

            break;
        case 5:
            ImGui::TextColored(blue, "%s", chip->read_isr ? "ISR" : "IRR");
            break;
        case 6:
            ImGui::TextColored(chip->special_mask ? green : gray, "%s", chip->special_mask ? "ON " : "OFF");
            break;
        case 7:
            ImGui::TextColored(chip->poll_pending ? yellow : gray, "%s", chip->poll_pending ? "ARMED" : "OFF  ");
            break;
        case 8:
            ImGui::TextColored(white, "IR%d", (chip->lowest_priority + 1) & 7); ImGui::SameLine();
            ImGui::TextColored(gray, "FIRST");
            break;
        case 9:
            ImGui::TextColored(chip->rotate_on_aeoi ? green : gray, "%s", chip->rotate_on_aeoi ? "ON " : "OFF");
            break;
        default:
            ImGui::TextColored(chip->int_output ? yellow : gray, "%s", chip->int_output ? "HIGH" : "LOW ");
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
