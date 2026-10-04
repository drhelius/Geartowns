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

#include "i386.h"

#if !defined(GT_DISABLE_DISASSEMBLER)

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "i386_names.h"

static const char* k_i386_register8_names[8] =
{
    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"
};

static const char* k_i386_register16_names[8] =
{
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di"
};

static const char* k_i386_register32_names[8] =
{
    "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"
};

static const char* k_i386_segment_names[8] =
{
    "es", "cs", "ss", "ds", "fs", "gs", "?s", "?s"
};

static const char* k_i386_group1_names[8] =
{
    "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp"
};

static const char* k_i386_group2_names[8] =
{
    "rol", "ror", "rcl", "rcr", "shl", "shr", "sal", "sar"
};

static const char* k_i386_group3_names[8] =
{
    "test", "test", "not", "neg", "mul", "imul", "div", "idiv"
};

static const char* k_i386_group6_names[8] =
{
    "sldt", "str", "lldt", "ltr", "verr", "verw", NULL, NULL
};

struct I386_Disassembly_Context
{
    const I386_Decode_State* state;
    I386_Disassembler_Record* record;
    u16 cs;
    u32 cs_base;
    int mode;
    bool default32;
};

static void append_text(char* output, size_t output_size, size_t& length, const char* text)
{
    if (!IsValidPointer(text) || length >= output_size)
        return;

    size_t remaining = output_size - length;
    int written = snprintf(output + length, remaining, "%s", text);

    if (written < 0)
        return;

    if ((size_t)written >= remaining)
        length = output_size - 1;
    else
        length += (size_t)written;
}

static void append_format(char* output, size_t output_size, size_t& length, const char* format, ...)
{
    if (length >= output_size)
        return;

    size_t remaining = output_size - length;
    va_list arguments;

    va_start(arguments, format);
    int written = vsnprintf(output + length, remaining, format, arguments);
    va_end(arguments);

    if (written < 0)
        return;

    if ((size_t)written >= remaining)
        length = output_size - 1;
    else
        length += (size_t)written;
}

static const char* register_name(int width, int index)
{
    index &= 7;

    if (width == 8)
        return k_i386_register8_names[index];

    if (width == 16)
        return k_i386_register16_names[index];

    return k_i386_register32_names[index];
}

static void append_memory_term(char* expression, size_t expression_size, size_t& length, const char* term)
{
    if (length != 0)
        append_text(expression, expression_size, length, "+");

    append_text(expression, expression_size, length, term);
}

static void append_displacement(char* expression, size_t expression_size, size_t& length, s32 displacement, bool direct,
    int address_size)
{
    if (direct)
    {
        if (address_size == 2)
            append_format(expression, expression_size, length, "0x%04X", (u16)displacement);
        else
            append_format(expression, expression_size, length, "0x%08X", (u32)displacement);

        return;
    }

    if (displacement > 0)
        append_format(expression, expression_size, length, "+0x%X", (u32)displacement);
    else if (displacement < 0)
        append_format(expression, expression_size, length, "-0x%X", 0U - (u32)displacement);
}

static void format_memory(const I386_Decode_State& state, int width, bool pointer, char* output, size_t output_size)
{
    char expression[96] = { };
    size_t expression_length = 0;
    bool direct = false;

    if (state.address_size == 2)
    {
        static const char* k_terms[8] =
        {
            "bx+si", "bx+di", "bp+si", "bp+di",
            "si", "di", "bp", "bx"
        };

        direct = state.mod == 0 && state.rm == 6;

        if (!direct)
            append_text(expression, sizeof(expression), expression_length, k_terms[state.rm & 7]);
    }
    else if (state.has_sib)
    {
        bool has_base = !(state.mod == 0 && state.sib_base == 5);

        if (has_base)
        {
            append_memory_term(expression, sizeof(expression), expression_length, register_name(32, state.sib_base));

            // Match the execution decoder's 386 no-index scaling behavior
            if (state.sib_index == 4 && state.sib_scale != 0)
                append_format(expression, sizeof(expression), expression_length, "*%u", 1U << state.sib_scale);
        }

        if (state.sib_index != 4)
        {
            char index[24];

            if (state.sib_scale == 0)
                snprintf(index, sizeof(index), "%s", register_name(32, state.sib_index));
            else
                snprintf(index, sizeof(index), "%s*%u", register_name(32, state.sib_index), 1U << state.sib_scale);

            append_memory_term(expression, sizeof(expression), expression_length, index);
        }

        direct = !has_base && state.sib_index == 4;
    }
    else
    {
        direct = state.mod == 0 && state.rm == 5;

        if (!direct)
            append_memory_term(expression, sizeof(expression), expression_length, register_name(32, state.rm));
    }

    append_displacement(expression, sizeof(expression), expression_length, state.displacement, direct,
        state.address_size);

    size_t length = 0;

    if (pointer)
    {
        if (width == 8)
            append_text(output, output_size, length, "byte ptr ");
        else if (width == 16)
            append_text(output, output_size, length, "word ptr ");
        else if (width == 32)
            append_text(output, output_size, length, "dword ptr ");
        else if (width == 48)
            append_text(output, output_size, length, "fword ptr ");
        else if (width == 64)
            append_text(output, output_size, length, "qword ptr ");
        else if (width == 80)
            append_text(output, output_size, length, "tbyte ptr ");
    }

    if (state.segment_override < I386_SEGMENT_COUNT)
        append_format(output, output_size, length, "%s:", k_i386_segment_names[state.segment_override]);

    append_format(output, output_size, length, "[%s]", expression);
}

static void format_rm(const I386_Decode_State& state, int width, bool pointer, char* output, size_t output_size)
{
    if (!state.memory_operand)
    {
        snprintf(output, output_size, "%s", register_name(width, state.rm));
        return;
    }

    format_memory(state, width, pointer, output, output_size);
}

static u32 relative_target(const I386_Decode_State& state, bool short_offset)
{
    s32 displacement;

    if (short_offset)
        displacement = (s8)(u8)state.immediate;
    else if (state.operand_size == 2)
        displacement = (s16)(u16)state.immediate;
    else
        displacement = (s32)state.immediate;

    u32 target = state.next_eip + displacement;

    if (state.operand_size == 2)
        target = (u16)target;

    return target;
}

static void set_relative_target(I386_Disassembly_Context& context, bool short_offset, char* output, size_t output_size)
{
    u32 target = relative_target(*context.state, short_offset);

    context.record->jump = true;
    context.record->jump_target_known = true;
    context.record->jump_cs = context.cs;
    context.record->jump_eip = target;
    context.record->jump_linear = context.cs_base + target;

    snprintf(output, output_size, "0x%08X", target);
}

static void format_moffs(const I386_Decode_State& state, int width, char* output, size_t output_size)
{
    size_t length = 0;

    if (width == 8)
        append_text(output, output_size, length, "byte ptr ");
    else if (width == 16)
        append_text(output, output_size, length, "word ptr ");
    else
        append_text(output, output_size, length, "dword ptr ");

    if (state.segment_override < I386_SEGMENT_COUNT)
        append_format(output, output_size, length, "%s:", k_i386_segment_names[state.segment_override]);

    if (state.address_size == 2)
        append_format(output, output_size, length, "[0x%04X]", (u16)state.immediate);
    else
        append_format(output, output_size, length, "[0x%08X]", state.immediate);
}

static const char* extended_format(u8 opcode)
{
    size_t count = sizeof(k_i386_extended_opcode_names) / sizeof(k_i386_extended_opcode_names[0]);

    for (size_t i = 0; i < count; i++)
    {
        if (k_i386_extended_opcode_names[i].opcode == opcode)
            return k_i386_extended_opcode_names[i].name;
    }

    return "@invalid";
}

static void select_x87_format(const I386_Decode_State& state, char* selected, size_t selected_size)
{
    static const char* k_memory_formats[8][8] =
    {
        { "fadd {M32}", "fmul {M32}", "fcom {M32}", "fcomp {M32}", "fsub {M32}", "fsubr {M32}", "fdiv {M32}",
            "fdivr {M32}" },
        { "fld {M32}", NULL, "fst {M32}", "fstp {M32}", "fldenv {M}", "fldcw {M16}", "fnstenv {M}", "fnstcw {M16}" },
        { "fiadd {M32}", "fimul {M32}", "ficom {M32}", "ficomp {M32}", "fisub {M32}", "fisubr {M32}", "fidiv {M32}",
            "fidivr {M32}" },
        { "fild {M32}", NULL, "fist {M32}", "fistp {M32}", NULL, "fld {M80}", NULL, "fstp {M80}" },
        { "fadd {M64}", "fmul {M64}", "fcom {M64}", "fcomp {M64}", "fsub {M64}", "fsubr {M64}", "fdiv {M64}",
            "fdivr {M64}" },
        { "fld {M64}", NULL, "fst {M64}", "fstp {M64}", "frstor {M}", NULL, "fnsave {M}", "fnstsw {M16}" },
        { "fiadd {M16}", "fimul {M16}", "ficom {M16}", "ficomp {M16}", "fisub {M16}", "fisubr {M16}", "fidiv {M16}",
            "fidivr {M16}" },
        { "fild {M16}", NULL, "fist {M16}", "fistp {M16}", "fbld {M80}", "fild {M64}", "fbstp {M80}", "fistp {M64}" }
    };

    if (state.memory_operand)
    {
        const char* format = k_memory_formats[state.opcode - 0xD8][state.reg];
        snprintf(selected, selected_size, "%s", IsValidPointer(format) ? format : "@invalid");
        return;
    }

    u8 index = state.rm;

    if (state.opcode == 0xD8)
    {
        static const char* k_names[8] = { "fadd", "fmul", "fcom", "fcomp", "fsub", "fsubr", "fdiv", "fdivr" };

        if (state.reg == 2 || state.reg == 3)
            snprintf(selected, selected_size, "%s st(%u)", k_names[state.reg], index);
        else
            snprintf(selected, selected_size, "%s st,st(%u)", k_names[state.reg], index);

        return;
    }

    if (state.opcode == 0xD9)
    {
        static const char* k_d9_e0_names[8] = { "fchs", "fabs", NULL, NULL, "ftst", "fxam", NULL, NULL };
        static const char* k_d9_e8_names[8] = { "fld1", "fldl2t", "fldl2e", "fldpi", "fldlg2", "fldln2", "fldz", NULL };
        static const char* k_d9_f0_names[16] =
        {
            "f2xm1", "fyl2x", "fptan", "fpatan", "fxtract", "fprem1", "fdecstp", "fincstp",
            "fprem", "fyl2xp1", "fsqrt", "fsincos", "frndint", "fscale", "fsin", "fcos"
        };

        if (state.reg == 0)
            snprintf(selected, selected_size, "fld st(%u)", index);
        else if (state.reg == 1)
            snprintf(selected, selected_size, "fxch st(%u)", index);
        else if (state.modrm == 0xD0)
            snprintf(selected, selected_size, "fnop");
        else if (state.modrm >= 0xE0 && state.modrm <= 0xE7 && IsValidPointer(k_d9_e0_names[index]))
            snprintf(selected, selected_size, "%s", k_d9_e0_names[index]);
        else if (state.modrm >= 0xE8 && state.modrm <= 0xEF && IsValidPointer(k_d9_e8_names[index]))
            snprintf(selected, selected_size, "%s", k_d9_e8_names[index]);
        else if (state.modrm >= 0xF0)
            snprintf(selected, selected_size, "%s", k_d9_f0_names[state.modrm - 0xF0]);
        else
            snprintf(selected, selected_size, "@invalid");

        return;
    }

    if (state.opcode == 0xDA && state.modrm == 0xE9)
    {
        snprintf(selected, selected_size, "fucompp");
        return;
    }

    if (state.opcode == 0xDB && state.modrm >= 0xE0 && state.modrm <= 0xE4)
    {
        static const char* k_names[5] = { "fneni", "fndisi", "fnclex", "fninit", "fnsetpm" };

        snprintf(selected, selected_size, "%s", k_names[state.modrm - 0xE0]);
        return;
    }

    if (state.opcode == 0xDC)
    {
        static const char* k_names[8] = { "fadd", "fmul", "fcom", "fcomp", "fsubr", "fsub", "fdivr", "fdiv" };

        snprintf(selected, selected_size, "%s st(%u),st", k_names[state.reg], index);
        return;
    }

    if (state.opcode == 0xDD)
    {
        static const char* k_names[8] = { "ffree", NULL, "fst", "fstp", "fucom", "fucomp", NULL, NULL };

        if (IsValidPointer(k_names[state.reg]))
            snprintf(selected, selected_size, "%s st(%u)", k_names[state.reg], index);
        else
            snprintf(selected, selected_size, "@invalid");

        return;
    }

    if (state.opcode == 0xDE)
    {
        static const char* k_names[8] = { "faddp", "fmulp", NULL, NULL, "fsubrp", "fsubp", "fdivrp", "fdivp" };

        if (state.modrm == 0xD9)
            snprintf(selected, selected_size, "fcompp");
        else if (IsValidPointer(k_names[state.reg]))
            snprintf(selected, selected_size, "%s st(%u),st", k_names[state.reg], index);
        else
            snprintf(selected, selected_size, "@invalid");

        return;
    }

    if (state.opcode == 0xDF && state.modrm == 0xE0)
    {
        snprintf(selected, selected_size, "fnstsw ax");
        return;
    }

    snprintf(selected, selected_size, "@invalid");
}

static const char* select_group_format(const I386_Decode_State& state, const char* format, char* selected,
    size_t selected_size)
{
    if (strcmp(format, "@x87") == 0)
        select_x87_format(state, selected, selected_size);
    else if (strcmp(format, "@g1b") == 0)
        snprintf(selected, selected_size, "%s {Eb},{Ib}", k_i386_group1_names[state.reg]);
    else if (strcmp(format, "@g1v") == 0)
        snprintf(selected, selected_size, "%s {Ev},{Iv}", k_i386_group1_names[state.reg]);
    else if (strcmp(format, "@g1s") == 0)
        snprintf(selected, selected_size, "%s {Ev},{Is}", k_i386_group1_names[state.reg]);
    else if (strcmp(format, "@g2bi") == 0)
        snprintf(selected, selected_size, "%s {Eb},{Ib}", k_i386_group2_names[state.reg]);
    else if (strcmp(format, "@g2vi") == 0)
        snprintf(selected, selected_size, "%s {Ev},{Ib}", k_i386_group2_names[state.reg]);
    else if (strcmp(format, "@g2b1") == 0)
        snprintf(selected, selected_size, "%s {Eb},1", k_i386_group2_names[state.reg]);
    else if (strcmp(format, "@g2v1") == 0)
        snprintf(selected, selected_size, "%s {Ev},1", k_i386_group2_names[state.reg]);
    else if (strcmp(format, "@g2bc") == 0)
        snprintf(selected, selected_size, "%s {Eb},cl", k_i386_group2_names[state.reg]);
    else if (strcmp(format, "@g2vc") == 0)
        snprintf(selected, selected_size, "%s {Ev},cl", k_i386_group2_names[state.reg]);
    else if (strcmp(format, "@g3b") == 0)
        snprintf(selected, selected_size, state.reg <= 1 ? "%s {Eb},{Ib}" : "%s {Eb}", k_i386_group3_names[state.reg]);
    else if (strcmp(format, "@g3v") == 0)
        snprintf(selected, selected_size, state.reg <= 1 ? "%s {Ev},{Iv}" : "%s {Ev}", k_i386_group3_names[state.reg]);
    else if (strcmp(format, "@g4") == 0)
    {
        if (state.reg <= 1)
            snprintf(selected, selected_size, "%s {Eb}", state.reg == 0 ? "inc" : "dec");
        else
            snprintf(selected, selected_size, "@invalid");
    }
    else if (strcmp(format, "@g5") == 0)
    {
        static const char* k_formats[8] =
        {
            "inc {Ev}", "dec {Ev}", "call {Ev}", "call {FAR}{Mp}", "jmp {Ev}", "jmp {FAR}{Mp}", "push {Ev}", "@invalid"
        };

        snprintf(selected, selected_size, "%s", k_formats[state.reg]);
    }
    else if (strcmp(format, "@g6") == 0)
    {
        const char* name = k_i386_group6_names[state.reg];

        if (IsValidPointer(name))
            snprintf(selected, selected_size, "%s {Ew}", name);
        else
            snprintf(selected, selected_size, "@invalid");
    }
    else if (strcmp(format, "@g7") == 0)
    {
        static const char* k_formats[8] =
        {
            "sgdt {Ms}", "sidt {Ms}", "lgdt {Ms}", "lidt {Ms}", "smsw {EwRv}", "@invalid", "lmsw {Ew}", "@invalid"
        };

        snprintf(selected, selected_size, "%s", k_formats[state.reg]);
    }
    else if (strcmp(format, "@g8") == 0)
    {
        static const char* k_names[8] = { NULL, NULL, NULL, NULL, "bt", "bts", "btr", "btc" };

        if (IsValidPointer(k_names[state.reg]))
            snprintf(selected, selected_size, "%s {Ev},{Ib}", k_names[state.reg]);
        else
            snprintf(selected, selected_size, "@invalid");
    }
    else
        return format;

    return selected;
}

static void expand_token(const char* token, I386_Disassembly_Context& context, char* output, size_t output_size)
{
    const I386_Decode_State& state = *context.state;
    int operand_width = state.operand_size * 8;

    if (strcmp(token, "Eb") == 0)
        format_rm(state, 8, true, output, output_size);
    else if (strcmp(token, "Ew") == 0)
        format_rm(state, 16, true, output, output_size);
    else if (strcmp(token, "EwRv") == 0)
        format_rm(state, state.memory_operand ? 16 : operand_width, true, output, output_size);
    else if (strcmp(token, "Ev") == 0)
        format_rm(state, operand_width, true, output, output_size);
    else if (strcmp(token, "E?") == 0)
        format_rm(state, operand_width, false, output, output_size);
    else if (strcmp(token, "M") == 0)
        format_memory(state, 0, false, output, output_size);
    else if (strcmp(token, "M16") == 0)
        format_memory(state, 16, true, output, output_size);
    else if (strcmp(token, "M32") == 0)
        format_memory(state, 32, true, output, output_size);
    else if (strcmp(token, "M64") == 0)
        format_memory(state, 64, true, output, output_size);
    else if (strcmp(token, "M80") == 0)
        format_memory(state, 80, true, output, output_size);
    else if (strcmp(token, "Ma") == 0)
        format_memory(state, operand_width * 2, true, output, output_size);
    else if (strcmp(token, "Mp") == 0)
        format_memory(state, operand_width + 16, true, output, output_size);
    else if (strcmp(token, "Ms") == 0)
        format_memory(state, 48, true, output, output_size);
    else if (strcmp(token, "Gb") == 0)
        snprintf(output, output_size, "%s", register_name(8, state.reg));
    else if (strcmp(token, "Gw") == 0)
        snprintf(output, output_size, "%s", register_name(16, state.reg));
    else if (strcmp(token, "Gv") == 0)
        snprintf(output, output_size, "%s", register_name(operand_width, state.reg));
    else if (strcmp(token, "Rd") == 0)
        snprintf(output, output_size, "%s", register_name(32, state.rm));
    else if (strcmp(token, "Sw") == 0)
        snprintf(output, output_size, "%s", k_i386_segment_names[state.reg]);
    else if (strcmp(token, "Cd") == 0)
        snprintf(output, output_size, "cr%u", state.reg);
    else if (strcmp(token, "Dd") == 0)
        snprintf(output, output_size, "dr%u", state.reg);
    else if (strcmp(token, "Td") == 0)
        snprintf(output, output_size, "tr%u", state.reg);
    else if (strcmp(token, "FAR") == 0)
        snprintf(output, output_size, "far ");
    else if (strcmp(token, "r8") == 0)
        snprintf(output, output_size, "%s", register_name(8, state.opcode));
    else if (strcmp(token, "rV") == 0)
        snprintf(output, output_size, "%s", register_name(operand_width, state.opcode));
    else if (strcmp(token, "eAX") == 0)
        snprintf(output, output_size, "%s", operand_width == 16 ? "ax" : "eax");
    else if (strcmp(token, "Ib") == 0)
        snprintf(output, output_size, "0x%02X", (u8)state.immediate);
    else if (strcmp(token, "Ib2") == 0)
        snprintf(output, output_size, "0x%02X", (u8)state.immediate2);
    else if (strcmp(token, "Iw") == 0)
        snprintf(output, output_size, "0x%04X", (u16)state.immediate);
    else if (strcmp(token, "Iv") == 0)
    {
        if (operand_width == 16)
            snprintf(output, output_size, "0x%04X", (u16)state.immediate);
        else
            snprintf(output, output_size, "0x%08X", state.immediate);
    }
    else if (strcmp(token, "Is") == 0)
        snprintf(output, output_size, "%d", (s8)(u8)state.immediate);
    else if (strcmp(token, "Jb") == 0)
        set_relative_target(context, true, output, output_size);
    else if (strcmp(token, "Jv") == 0)
        set_relative_target(context, false, output, output_size);
    else if (strcmp(token, "Ap") == 0)
    {
        context.record->jump = true;
        context.record->jump_far = true;
        context.record->jump_cs = (u16)state.immediate2;
        context.record->jump_eip = operand_width == 16 ? (u16)state.immediate : state.immediate;

        if (context.mode != I386_MODE_PROTECTED)
        {
            context.record->jump_target_known = true;
            context.record->jump_linear = ((u32)context.record->jump_cs << 4) + context.record->jump_eip;
        }

        snprintf(output, output_size, "0x%04X:0x%0*X", context.record->jump_cs, operand_width == 16 ? 4 : 8,
            context.record->jump_eip);
    }
    else if (strcmp(token, "Ob") == 0)
        format_moffs(state, 8, output, output_size);
    else if (strcmp(token, "Ov") == 0)
        format_moffs(state, operand_width, output, output_size);
    else if (strcmp(token, "PUSHA") == 0)
        snprintf(output, output_size, "%s", operand_width == 16 ? "pusha" : "pushad");
    else if (strcmp(token, "POPA") == 0)
        snprintf(output, output_size, "%s", operand_width == 16 ? "popa" : "popad");
    else if (strcmp(token, "CBW") == 0)
        snprintf(output, output_size, "%s", operand_width == 16 ? "cbw" : "cwde");
    else if (strcmp(token, "CWD") == 0)
        snprintf(output, output_size, "%s", operand_width == 16 ? "cwd" : "cdq");
    else if (strcmp(token, "PUSHF") == 0)
        snprintf(output, output_size, "%s", operand_width == 16 ? "pushf" : "pushfd");
    else if (strcmp(token, "POPF") == 0)
        snprintf(output, output_size, "%s", operand_width == 16 ? "popf" : "popfd");
    else if (strcmp(token, "IRET") == 0)
        snprintf(output, output_size, "%s", operand_width == 16 ? "iret" : "iretd");
    else if (strcmp(token, "INSv") == 0)
        snprintf(output, output_size, "%s", operand_width == 16 ? "insw" : "insd");
    else if (strcmp(token, "OUTSv") == 0)
        snprintf(output, output_size, "%s", operand_width == 16 ? "outsw" : "outsd");
    else if (strcmp(token, "MOVSv") == 0)
        snprintf(output, output_size, "%s", operand_width == 16 ? "movsw" : "movsd");
    else if (strcmp(token, "CMPSv") == 0)
        snprintf(output, output_size, "%s", operand_width == 16 ? "cmpsw" : "cmpsd");
    else if (strcmp(token, "STOSv") == 0)
        snprintf(output, output_size, "%s", operand_width == 16 ? "stosw" : "stosd");
    else if (strcmp(token, "LODSv") == 0)
        snprintf(output, output_size, "%s", operand_width == 16 ? "lodsw" : "lodsd");
    else if (strcmp(token, "SCASv") == 0)
        snprintf(output, output_size, "%s", operand_width == 16 ? "scasw" : "scasd");
    else if (strcmp(token, "JCXZ") == 0)
        snprintf(output, output_size, "%s", state.address_size == 2 ? "jcxz" : "jecxz");
    else
        snprintf(output, output_size, "{%s}", token);
}

static void mark_control_flow(const I386_Decode_State& state, I386_Disassembler_Record& record)
{
    u8 opcode = state.two_byte ? state.opcode2 : state.opcode;

    if (state.two_byte)
        return;

    if (opcode == 0x9A || opcode == 0xE8 || (opcode == 0xFF && (state.reg == 2 || state.reg == 3)))
        record.subroutine = true;

    if (opcode == 0xC2 || opcode == 0xC3 || opcode == 0xCA || opcode == 0xCB || opcode == 0xCF)
    {
        record.returns = true;
        record.unconditional = true;
    }

    if (opcode == 0xE9 || opcode == 0xEA || opcode == 0xEB || (opcode == 0xFF && (state.reg == 4 || state.reg == 5)))
    {
        record.jump = true;
        record.unconditional = true;
    }

    if (opcode == 0xFF && (state.reg == 2 || state.reg == 3))
        record.jump = true;
}

// Encoding rules only: protected-mode instructions keep their mnemonic in other modes, because look-ahead
// disassembly cannot know the mode the code will run in
static bool valid_encoding(const I386_Decode_State& state)
{
    if (state.invalid_lock)
        return false;

    if (state.two_byte)
    {
        switch (state.opcode2)
        {
            case 0x01:
                return state.reg > 3 || state.memory_operand;
            case 0x20:
            case 0x22:
                return !state.memory_operand && (state.reg == 0 || state.reg == 2 || state.reg == 3);
            case 0x21:
            case 0x23:
                return !state.memory_operand && state.reg != 4 && state.reg != 5;
            case 0x24:
            case 0x26:
                return !state.memory_operand && state.reg >= 6;
            case 0xB2:
            case 0xB4:
            case 0xB5:
                return state.memory_operand;
        }
    }
    else
    {
        switch (state.opcode)
        {
            case 0x62:
            case 0x8D:
            case 0xC4:
            case 0xC5:
                return state.memory_operand;
            case 0x8C:
                return state.reg < I386_SEGMENT_COUNT;
            case 0x8E:
                return state.reg < I386_SEGMENT_COUNT && state.reg != I386_SEGMENT_CS;
            case 0x8F:
            case 0xC6:
            case 0xC7:
                return state.reg == 0;
            case 0xFF:
                return (state.reg != 3 && state.reg != 5) || state.memory_operand;
        }
    }

    return true;
}

static void format_instruction(I386_Disassembly_Context& context, char* output, size_t output_size)
{
    const I386_Decode_State& state = *context.state;
    const char* format = state.two_byte ? extended_format(state.opcode2) : k_i386_opcode_names[state.opcode];
    char selected[96];

    format = select_group_format(state, format, selected, sizeof(selected));

    if (!valid_encoding(state))
        format = "@invalid";

    size_t length = 0;

    if (state.lock)
        append_text(output, output_size, length, "lock ");

    if (state.repeat == 2)
        append_text(output, output_size, length, "repne ");
    else if (state.repeat == 3)
        append_text(output, output_size, length, "rep ");

    if (format[0] == '@')
    {
        if (state.two_byte)
            append_format(output, output_size, length, "db 0x0F,0x%02X", state.opcode2);
        else
            append_format(output, output_size, length, "db 0x%02X", state.opcode);

        return;
    }

    mark_control_flow(state, *context.record);

    if (!state.two_byte)
    {
        u8 opcode = state.opcode;
        bool string_source = opcode == 0x6E || opcode == 0x6F || (opcode >= 0xA4 && opcode <= 0xA7) ||
            opcode == 0xAC || opcode == 0xAD;
        bool string_destination = opcode == 0x6C || opcode == 0x6D || opcode == 0xAA || opcode == 0xAB ||
            opcode == 0xAE || opcode == 0xAF;

        if ((string_source || string_destination) && (state.address_size == 4) != context.default32)
            append_format(output, output_size, length, "addr%d ", state.address_size * 8);

        if (string_source && state.segment_override < I386_SEGMENT_COUNT)
            append_format(output, output_size, length, "%s ", k_i386_segment_names[state.segment_override]);
    }

    const char* cursor = format;

    while (*cursor != 0)
    {
        if (*cursor != '{')
        {
            char value[2] = { *cursor, 0 };

            append_text(output, output_size, length, value);
            cursor++;
            continue;
        }

        const char* end = strchr(cursor + 1, '}');

        if (!IsValidPointer(end))
        {
            append_text(output, output_size, length, cursor);
            break;
        }

        char token[16];
        size_t token_length = (size_t)(end - cursor - 1);

        if (token_length >= sizeof(token))
            token_length = sizeof(token) - 1;

        memcpy(token, cursor + 1, token_length);
        token[token_length] = 0;

        char expanded[128] = { };

        expand_token(token, context, expanded, sizeof(expanded));
        append_text(output, output_size, length, expanded);
        cursor = end + 1;
    }
}

static void uppercase_intel_identifiers(char* text)
{
    for (size_t i = 0; text[i] != 0; i++)
    {
        if (text[i] < 'a' || text[i] > 'z')
            continue;

        if (text[i] == 'x' && i > 0 && text[i - 1] == '0')
            continue;

        text[i] = (char)(text[i] - 'a' + 'A');
    }
}

static void set_auto_symbol(I386_Disassembler_Record& record, bool subroutine)
{
    // A location first reached by a jump becomes a subroutine once something calls it
    bool upgrade = subroutine && strncmp(record.auto_symbol, "LOC_", 4) == 0;

    if (record.auto_symbol[0] != 0 && !upgrade)
        return;

    snprintf(record.auto_symbol, sizeof(record.auto_symbol), "%s_%08X", subroutine ? "SUB" : "LOC", record.linear);
}

I386_Disassembler_Record* I386::Disassemble(u32 eip)
{
    return Disassemble(m_state.segments[I386_SEGMENT_CS], eip);
}

I386_Disassembler_Record* I386::Disassemble(const I386_Segment& code_segment, u32 eip)
{
    u32 linear = code_segment.base + eip;
    I386_Decode_State state;

    if (!DecodeInstructionForDebugger(code_segment, eip, state))
    {
        I386_Disassembler_Record* record = GetDisassemblerRecord(linear);

        if (IsValidPointer(record))
        {
            // Preserve symbols, but stop exposing the old instruction as valid
            record->name[0] = 0;
            record->size = 0;
        }

        return NULL;
    }

    u64 new_end = (u64)linear + state.length;
    u32 max_previous_length = GT_I386_MAX_INSTRUCTION_LENGTH - 1;
    u32 first_overlap = linear >= max_previous_length ? linear - max_previous_length : 0;
    std::map<u32, I386_Disassembler_Record>::iterator existing = m_disassembler_records.lower_bound(first_overlap);

    // Drop the records of other instructions that overlap the new one
    while (existing != m_disassembler_records.end() && (u64)existing->first < new_end)
    {
        u64 existing_end = (u64)existing->first + existing->second.size;
        bool overlaps = existing->first != linear && existing->second.size > 0 && existing_end > linear;

        if (overlaps)
            existing = m_disassembler_records.erase(existing);
        else
            existing++;
    }

    I386_Disassembler_Record& record = m_disassembler_records[linear];
    char previous_symbol[sizeof(record.auto_symbol)];

    snprintf(previous_symbol, sizeof(previous_symbol), "%s", record.auto_symbol);
    memset(&record, 0, sizeof(record));
    snprintf(record.auto_symbol, sizeof(record.auto_symbol), "%s", previous_symbol);

    record.cs = code_segment.selector;
    record.eip = eip;
    record.linear = linear;
    record.size = state.length;

    memcpy(record.opcodes, state.bytes, state.length);
    snprintf(record.segment, sizeof(record.segment), "%04X", record.cs);

    size_t byte_length = 0;

    for (int i = 0; i < state.length; i++)
        append_format(record.bytes, sizeof(record.bytes), byte_length, i == 0 ? "%02X" : " %02X", state.bytes[i]);

    I386_Disassembly_Context context;
    context.state = &state;
    context.record = &record;
    context.cs = record.cs;
    context.cs_base = code_segment.base;
    context.mode = m_state.execution_mode;
    context.default32 = (code_segment.attributes & I386_SEGMENT_DEFAULT_32) != 0;

    format_instruction(context, record.name, sizeof(record.name));
    uppercase_intel_identifiers(record.name);

    if (record.linear == 0xFFFFFFF0U && record.auto_symbol[0] == 0)
        snprintf(record.auto_symbol, sizeof(record.auto_symbol), "RESET");

    if (record.jump && record.jump_target_known)
    {
        I386_Disassembler_Record& target = m_disassembler_records[record.jump_linear];

        if (target.linear == 0)
        {
            target.cs = record.jump_cs;
            target.eip = record.jump_eip;
            target.linear = record.jump_linear;
        }

        set_auto_symbol(target, record.subroutine);
    }

    return &record;
}

void I386::DisassembleAhead(int count)
{
    DisassembleAhead(m_state.eip, count);
}

void I386::DisassembleAhead(u32 start_eip, int count, int depth)
{
    // Retain the main span and share one extra span across all branch paths
    int branch_budget = count;

    DisassembleAhead(m_state.segments[I386_SEGMENT_CS], start_eip, count, depth, branch_budget);
}

void I386::DisassembleAhead(const I386_Segment& code_segment, u32 start_eip, int count, int depth, int& branch_budget)
{
    if (count <= 0 || depth > 3)
        return;

    u32 eip = start_eip;

    for (int i = 0; i < count; i++)
    {
        if (depth > 0)
        {
            if (branch_budget <= 0)
                break;

            branch_budget--;
        }

        I386_Disassembler_Record* record = Disassemble(code_segment, eip);

        if (!IsValidPointer(record) || record->size <= 0)
            break;

        bool follow_jump = record->jump && record->jump_target_known;
        bool jump_far = record->jump_far;
        bool unconditional = record->unconditional;
        u16 jump_cs = record->jump_cs;
        u32 jump_eip = record->jump_eip;
        u32 instruction_size = (u32)record->size;
        const I386_Segment& current_code = m_state.segments[I386_SEGMENT_CS];
        bool current_instruction = eip == m_state.eip && code_segment.selector == current_code.selector &&
            code_segment.base == current_code.base && code_segment.limit == current_code.limit &&
            code_segment.attributes == current_code.attributes;

        if (follow_jump && (jump_far || jump_eip != eip))
        {
            int branch_count = count / 2;

            if (jump_far && current_instruction && m_state.execution_mode != I386_MODE_PROTECTED)
            {
                I386_Segment target_segment = {};

                target_segment.selector = jump_cs;
                target_segment.base = (u32)jump_cs << 4;
                target_segment.limit = 0xFFFF;
                target_segment.attributes = I386_SEGMENT_PRESENT | I386_SEGMENT_READABLE | I386_SEGMENT_EXECUTABLE;

                if (m_state.execution_mode == I386_MODE_VM86)
                {
                    target_segment.attributes |= I386_SEGMENT_SYSTEM | (2U << I386_SEGMENT_TYPE_SHIFT);
                    target_segment.dpl = 3;
                }

                DisassembleAhead(target_segment, jump_eip, branch_count, depth + 1, branch_budget);
            }
            else if (!jump_far)
                DisassembleAhead(code_segment, jump_eip, branch_count, depth + 1, branch_budget);
        }

        if (unconditional)
            break;

        eip += instruction_size;
    }
}

I386_Disassembler_Record* I386::GetDisassemblerRecord(u32 linear)
{
    std::map<u32, I386_Disassembler_Record>::iterator record = m_disassembler_records.find(linear);

    if (record == m_disassembler_records.end())
        return NULL;

    return &record->second;
}

const std::map<u32, I386_Disassembler_Record>& I386::GetDisassemblerRecords() const
{
    return m_disassembler_records;
}

void I386::ResetDisassembler()
{
    m_disassembler_records.clear();
    ResetDebuggerExecutionState();
}

void I386::ResetDebuggerExecutionState()
{
    m_disassembler_call_stack.clear();
    m_run_to_breakpoint = 0;
    m_run_to_breakpoint_enabled = false;
    m_breakpoint_hit = false;
    m_run_to_hit = false;
    m_breakpoint_hit_address = 0;
}

void I386::ResetBreakpoints()
{
    m_breakpoints.clear();
    m_run_to_breakpoint_enabled = false;
    m_breakpoint_hit = false;
    m_run_to_hit = false;
}

void I386::AddBreakpoint(u32 address)
{
    if (IsBreakpoint(address))
        return;

    I386_Breakpoint breakpoint;

    breakpoint.enabled = true;
    breakpoint.address1 = address;
    breakpoint.address2 = address;
    breakpoint.range = false;
    m_breakpoints.push_back(breakpoint);
}

void I386::AddBreakpoint(u32 start_address, u32 end_address)
{
    if (end_address < start_address)
    {
        u32 temporary = start_address;
        start_address = end_address;
        end_address = temporary;
    }

    if (start_address == end_address)
    {
        AddBreakpoint(start_address);
        return;
    }

    for (size_t i = 0; i < m_breakpoints.size(); i++)
    {
        const I386_Breakpoint& existing = m_breakpoints[i];

        if (existing.range && existing.address1 == start_address && existing.address2 == end_address)
            return;
    }

    I386_Breakpoint breakpoint;

    breakpoint.enabled = true;
    breakpoint.address1 = start_address;
    breakpoint.address2 = end_address;
    breakpoint.range = true;
    m_breakpoints.push_back(breakpoint);
}

void I386::AddRunToBreakpoint(u32 address)
{
    m_run_to_breakpoint = address;
    m_run_to_breakpoint_enabled = true;
}

void I386::RemoveBreakpoint(u32 address, u32 end_address)
{
    for (size_t i = 0; i < m_breakpoints.size(); i++)
    {
        const I386_Breakpoint& breakpoint = m_breakpoints[i];

        if (breakpoint.address1 == address && (!breakpoint.range || breakpoint.address2 == end_address))
        {
            m_breakpoints.erase(m_breakpoints.begin() + i);
            return;
        }
    }
}

bool I386::IsBreakpoint(u32 address) const
{
    for (size_t i = 0; i < m_breakpoints.size(); i++)
    {
        if (!m_breakpoints[i].range && m_breakpoints[i].address1 == address)
            return true;
    }

    return false;
}

std::vector<I386_Breakpoint>* I386::GetBreakpoints()
{
    return &m_breakpoints;
}

bool I386::CheckDebuggerBreakpoints(bool regular, bool run_to)
{
    u32 address = GetCurrentLinearPC();

    // Resuming from the stop address executes that instruction once without stopping again
    if (m_breakpoint_hit && m_breakpoint_hit_address == address)
    {
        m_breakpoint_hit = false;
        m_run_to_hit = false;
        return false;
    }

    if (run_to && m_run_to_breakpoint_enabled && address == m_run_to_breakpoint)
    {
        m_run_to_breakpoint_enabled = false;
        m_breakpoint_hit = true;
        m_run_to_hit = true;
        m_breakpoint_hit_address = address;
        return true;
    }

    if (!regular)
        return false;

    for (size_t i = 0; i < m_breakpoints.size(); i++)
    {
        const I386_Breakpoint& breakpoint = m_breakpoints[i];

        if (!breakpoint.enabled)
            continue;

        bool hit = breakpoint.range ? address >= breakpoint.address1 && address <= breakpoint.address2 :
            address == breakpoint.address1;

        if (hit)
        {
            m_breakpoint_hit = true;
            m_run_to_hit = false;
            m_breakpoint_hit_address = address;
            return true;
        }
    }

    return false;
}

bool I386::GetBreakpointHitAddress(u32& address) const
{
    if (!m_breakpoint_hit || m_run_to_hit)
        return false;

    address = m_breakpoint_hit_address;
    return true;
}

bool I386::RunToBreakpointHit() const
{
    return m_breakpoint_hit && m_run_to_hit;
}

const std::vector<I386_CallStackEntry>& I386::GetDisassemblerCallStack() const
{
    return m_disassembler_call_stack;
}

bool I386::DebugInstructionCompleted(const I386_State& before, const I386_Run_Result& result, u32* call_return_linear)
{
    if (result.steps == 0)
        return false;

    I386_Decode_State instruction;
    CopyDecodeState(instruction);

    u8 opcode = instruction.opcode;
    bool exception_entry = result.exception;
    bool completed = !exception_entry || result.exception_after_instruction;
    bool call = completed && !instruction.two_byte &&
        (opcode == 0x9A || opcode == 0xE8 || (opcode == 0xFF && (instruction.reg == 2 || instruction.reg == 3)));
    bool returns = completed && !instruction.two_byte &&
        (opcode == 0xC2 || opcode == 0xC3 || opcode == 0xCA || opcode == 0xCB || opcode == 0xCF);

    if (returns && !m_disassembler_call_stack.empty())
        m_disassembler_call_stack.pop_back();

    if (!call && !exception_entry)
        return false;

    I386_State after;
    CopyState(after);

    int entries = (call ? 1 : 0) + (exception_entry ? 1 : 0);
    int return_size = m_instruction.call_return_size != 0 ? m_instruction.call_return_size : instruction.operand_size;

    for (int i = 0; i < entries; i++)
    {
        I386_CallStackEntry entry;

        entry.interrupt = exception_entry && (!call || i != 0);
        entry.src_cs = before.segments[I386_SEGMENT_CS].selector;
        entry.src = instruction.start_eip;
        entry.src_linear = before.segments[I386_SEGMENT_CS].base + entry.src;
        entry.dest_cs = after.segments[I386_SEGMENT_CS].selector;
        entry.dest = after.eip;
        entry.dest_linear = after.segments[I386_SEGMENT_CS].base + entry.dest;
        entry.back_cs = entry.src_cs;
        entry.back = Truncate(instruction.next_eip, return_size * 8);
        entry.back_linear = before.segments[I386_SEGMENT_CS].base + entry.back;

        if (entry.interrupt)
        {
            entry.src_cs = result.exception_return_cs;
            entry.src = result.exception_source_eip;
            entry.src_linear = result.exception_return_base + entry.src;
            entry.back_cs = result.exception_return_cs;
            entry.back = result.exception_return_eip;
            entry.back_linear = result.exception_return_base + entry.back;
        }
        else if (exception_entry)
        {
            // Record the completed CALL before its following debug trap
            entry.dest_cs = result.exception_return_cs;
            entry.dest = result.exception_source_eip;
            entry.dest_linear = result.exception_return_base + entry.dest;
        }

        if (!entry.interrupt && IsValidPointer(call_return_linear))
            *call_return_linear = entry.back_linear;

        if (m_disassembler_call_stack.size() == 256)
            m_disassembler_call_stack.erase(m_disassembler_call_stack.begin());

        m_disassembler_call_stack.push_back(entry);

        I386_Disassembler_Record& target = m_disassembler_records[entry.dest_linear];

        if (target.linear == 0)
        {
            target.cs = entry.dest_cs;
            target.eip = entry.dest;
            target.linear = entry.dest_linear;
        }

        if (entry.interrupt)
        {
            if (target.auto_symbol[0] == 0)
                snprintf(target.auto_symbol, sizeof(target.auto_symbol), "INT_%02X", result.exception_vector);
        }
        else
            set_auto_symbol(target, true);
    }

    return call;
}

u32 I386::GetCurrentLinearPC() const
{
    return m_state.segments[I386_SEGMENT_CS].base + m_state.eip;
}

#endif
