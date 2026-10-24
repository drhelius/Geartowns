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

#define GUI_DEBUG_X87_IMPORT
#include "gui_debug_x87.h"

#include <math.h>
#include <stdio.h>
#include "imgui.h"
#include "../gui_colors.h"
#include "../gui.h"
#include "../config.h"
#include "../emu.h"

struct X87BitDefinition
{
    const char* name;
    u8 bit;
};

static const X87BitDefinition k_x87_masks[] =
{
    { "PM", 5 }, { "UM", 4 }, { "OM", 3 }, { "ZM", 2 }, { "DM", 1 }, { "IM", 0 }
};

static const X87BitDefinition k_x87_conditions[] =
{
    { "B", 15 }, { "C3", 14 }, { "C2", 10 }, { "C1", 9 }, { "C0", 8 }, { "ES", 7 }, { "SF", 6 }
};

static const X87BitDefinition k_x87_exceptions[] =
{
    { "PE", 5 }, { "UE", 4 }, { "OE", 3 }, { "ZE", 2 }, { "DE", 1 }, { "IE", 0 }
};

static const char* const k_x87_tag_names[4] = { "VALID", "ZERO", "SPECIAL", "EMPTY" };
static const char* const k_x87_precision_names[4] = { "24-BIT", "RESERVED", "53-BIT", "64-BIT" };
static const char* const k_x87_rounding_names[4] = { "NEAREST", "DOWN", "UP", "CHOP" };

static void draw_bits(const X87BitDefinition* bits, int count, u16 value, bool fitted);
static void draw_label(const char* label);
static void draw_register(const char* label, u16 value, bool fitted);

void gui_debug_window_x87(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(80, 160), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(418, 557), ImGuiCond_FirstUseEver);
    ImGui::Begin("Intel 80387", &config_debug.show_x87);

    ImGui::PushFont(gui_default_font);

    X87* fpu = emu_get_core()->GetX87();
    X87::X87_State* state = fpu->GetState();
    bool fitted = emu_get_core()->GetMachineConfig().fpu;
    u16 tags = fpu->GetTagWord();
    int top = (state->status >> 11) & 7;
    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoPadOuterX |
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    float character = ImGui::CalcTextSize("0").x;

    ImGui::TextColored(cyan, "COPROCESSOR"); ImGui::Separator();

    if (ImGui::BeginTable("##x87_fitted", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX))
    {
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 10);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 40);

        draw_label("FITTED");
        ImGui::TextColored(fitted ? green : gray, "%s", fitted ? "80387" : "NONE");

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "STACK"); ImGui::Separator();

    if (ImGui::BeginTable("##x87_stack", 5, flags))
    {
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 4);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 3);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 8);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 21);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 24);

        for (int i = 0; i < X87_REGISTER_COUNT; i++)
        {
            int physical = (top + i) & 7;
            int tag = (tags >> (physical * 2)) & 3;
            const X87::X87_Register& value = state->registers[physical];
            char text[64];

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(violet, "ST%d", i);
            ImGui::TableNextColumn();

            if (!fitted)
            {
                for (int column = 0; column < 4; column++)
                {
                    if (column > 0)
                        ImGui::TableNextColumn();

                    ImGui::TextColored(gray, "--");
                }

                continue;
            }

            ImGui::TextColored(gray, "R%d", physical);
            ImGui::TableNextColumn();
            ImGui::TextColored(tag == 3 ? gray : tag == 2 ? orange : green, "%s", k_x87_tag_names[tag]);
            ImGui::TableNextColumn();
            ImGui::TextColored(tag == 3 ? gray : white, "%04X %016llX", value.sign_exponent,
                (unsigned long long)value.significand);
            ImGui::TableNextColumn();

            if (tag == 3)
                ImGui::TextColored(gray, "--");
            else
            {
                gui_debug_x87_format_value(value.sign_exponent, value.significand, text, sizeof(text));
                ImGui::TextColored(cyan, "%s", text);
            }
        }

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "CONTROL"); ImGui::Separator();

    if (ImGui::BeginTable("##x87_control", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX))
    {
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 10);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 40);

        draw_register("CW", state->control, fitted);
        ImGui::SameLine();
        draw_bits(k_x87_masks, 6, state->control, fitted);

        draw_label("PRECISION");
        ImGui::TextColored(fitted ? blue : gray, "%s", fitted ? gui_debug_x87_precision_name(state->control) : "--");

        draw_label("ROUNDING");
        ImGui::TextColored(fitted ? blue : gray, "%s", fitted ? gui_debug_x87_rounding_name(state->control) : "--");

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "STATUS"); ImGui::Separator();

    if (ImGui::BeginTable("##x87_status", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX))
    {
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 10);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 40);

        draw_register("SW", state->status, fitted);
        ImGui::SameLine();
        ImGui::TextColored(violet, "TOP");
        ImGui::SameLine();

        if (fitted)
            ImGui::TextColored(white, "%d", top);
        else
            ImGui::TextColored(gray, "--");

        draw_label("CONDITION");
        draw_bits(k_x87_conditions, 7, state->status, fitted);

        draw_label("EXCEPTION");
        draw_bits(k_x87_exceptions, 6, state->status, fitted);

        draw_register("TW", tags, fitted);

        ImGui::EndTable();
    }

    ImGui::NewLine(); ImGui::TextColored(cyan, "LAST INSTRUCTION"); ImGui::Separator();

    if (ImGui::BeginTable("##x87_pointers", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX))
    {
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 10);
        ImGui::TableSetupColumn(NULL, ImGuiTableColumnFlags_WidthFixed, character * 40);

        draw_label("ADDRESS");

        if (fitted)
            ImGui::TextColored(white, "$%04X:$%08X", state->instruction_selector, state->instruction_offset);
        else
            ImGui::TextColored(gray, "--");

        draw_label("OPCODE");

        if (fitted)
            ImGui::TextColored(white, "%02X %02X", 0xD8 | ((state->opcode >> 8) & 7), state->opcode & 0xFF);
        else
            ImGui::TextColored(gray, "--");

        draw_label("OPERAND");

        if (fitted)
            ImGui::TextColored(white, "$%04X:$%08X", state->operand_selector, state->operand_offset);
        else
            ImGui::TextColored(gray, "--");

        ImGui::EndTable();
    }

    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_x87_format_value(u16 sign_exponent, u64 significand, char* text, size_t text_size)
{
    int exponent = sign_exponent & 0x7FFF;
    const char* sign = (sign_exponent & 0x8000) != 0 ? "-" : "";
    bool integer = (significand & 0x8000000000000000ULL) != 0;

    if (exponent == 0x7FFF)
    {
        if (!integer)
            snprintf(text, text_size, "UNSUPPORTED");
        else if ((significand << 1) == 0)
            snprintf(text, text_size, "%sINF", sign[0] != 0 ? "-" : "+");
        else
            snprintf(text, text_size, "%s", (significand & 0x4000000000000000ULL) != 0 ? "QNAN" : "SNAN");

        return;
    }

    if ((exponent != 0) && !integer)
    {
        snprintf(text, text_size, "UNSUPPORTED");
        return;
    }

    if (significand == 0)
    {
        snprintf(text, text_size, "%s0", sign);
        return;
    }

    int power = (exponent == 0 ? 1 : exponent) - 16383 - 63;

    // A double holds the value directly within its range, beyond it the digits come from the logarithm
    if ((power > -1000) && (power < 950))
    {
        snprintf(text, text_size, "%s%.17g", sign, ldexp((double)significand, power));
        return;
    }

    double digits = log10((double)significand) + power * log10(2.0);
    double decimal = floor(digits);
    snprintf(text, text_size, "%s%.15fe%+d", sign, pow(10.0, digits - decimal), (int)decimal);
}

const char* gui_debug_x87_tag_name(int tag)
{
    return k_x87_tag_names[tag & 3];
}

const char* gui_debug_x87_precision_name(u16 control)
{
    return k_x87_precision_names[(control >> 8) & 3];
}

const char* gui_debug_x87_rounding_name(u16 control)
{
    return k_x87_rounding_names[(control >> 10) & 3];
}

static void draw_bits(const X87BitDefinition* bits, int count, u16 value, bool fitted)
{
    for (int i = 0; i < count; i++)
    {
        if (i > 0)
            ImGui::SameLine();

        bool set = fitted && ((value & (1U << bits[i].bit)) != 0);
        ImGui::TextColored(set ? orange : gray, "%s", bits[i].name);
    }
}

static void draw_label(const char* label)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(violet, "%s", label);
    ImGui::TableNextColumn();
}

static void draw_register(const char* label, u16 value, bool fitted)
{
    draw_label(label);

    if (fitted)
        ImGui::TextColored(white, "$%04X", value);
    else
        ImGui::TextColored(gray, "--");
}
