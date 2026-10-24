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

#include "x87.h"
#include "../common/state_serializer.h"
#include "bochs_fpu.h"

static const u16 k_x87_status_ie = 0x0001;
static const u16 k_x87_status_de = 0x0002;
static const u16 k_x87_status_ze = 0x0004;
static const u16 k_x87_status_oe = 0x0008;
static const u16 k_x87_status_ue = 0x0010;
static const u16 k_x87_status_pe = 0x0020;
static const u16 k_x87_status_sf = 0x0040;
static const u16 k_x87_status_es = 0x0080;
static const u16 k_x87_status_c0 = 0x0100;
static const u16 k_x87_status_c1 = 0x0200;
static const u16 k_x87_status_c2 = 0x0400;
static const u16 k_x87_status_c3 = 0x4000;
static const u16 k_x87_status_busy = 0x8000;
static const u16 k_x87_status_unordered = k_x87_status_c0 | k_x87_status_c2 | k_x87_status_c3;
static const u16 k_x87_exceptions = 0x003F;
static const u32 k_x87_stack_overflow = k_x87_status_ie | k_x87_status_sf | k_x87_status_c1;
static const u32 k_x87_stack_underflow = k_x87_status_ie | k_x87_status_sf;
static const u16 k_x87_control_reserved = 0xE0C0;
static const u16 k_x87_control_fixed = 0x0040;
static const u16 k_x87_control_im = 0x0001;

// Midpoints of the Intel387 DX datasheet ranges (240448-005), with its d and R notes applied
static const u16 k_x87_memory_clocks[8][8] =
{
    { 20, 25, 19, 19, 20, 21, 81, 81 },
    { 13, 12, 34, 34, 71, 19, 103, 15 },
    { 45, 57, 43, 43, 45, 46, 107, 108 },
    { 34, 12, 66, 66, 12, 27, 12, 49 },
    { 24, 38, 20, 20, 24, 25, 86, 86 },
    { 19, 12, 38, 38, 308, 12, 375, 15 },
    { 51, 60, 50, 50, 51, 52, 114, 138 },
    { 47, 12, 67, 67, 71, 40, 151, 71 }
};

static const u16 k_x87_register_clocks[8][8] =
{
    { 19, 33, 17, 17, 22, 22, 78, 78 },
    { 9, 13, 12, 9, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 22, 33, 17, 17, 19, 19, 81, 81 },
    { 18, 13, 9, 9, 17, 17, 12, 12 },
    { 22, 33, 17, 12, 19, 19, 81, 81 },
    { 18, 13, 9, 9, 12, 12, 12, 12 }
};

static const u16 k_x87_d9_clocks[32] =
{
    20, 17, 12, 12, 21, 30, 12, 12, 18, 31, 31, 31, 30, 32, 13, 12,
    288, 267, 296, 335, 52, 124, 22, 21, 98, 328, 104, 400, 51, 63, 400, 401
};

static const X87::X87_Register k_x87_default_nan = { 0xC000000000000000ULL, 0xFFFF };
static const X87::X87_Register k_x87_zero = { 0x0000000000000000ULL, 0x0000 };
static const X87::X87_Register k_x87_one = { 0x8000000000000000ULL, 0x3FFF };
static const X87::X87_Register k_x87_log2_10 = { 0xD49A784BCD1B8AFEULL, 0x4000 };
static const X87::X87_Register k_x87_log2_e = { 0xB8AA3B295C17F0BCULL, 0x3FFF };
static const X87::X87_Register k_x87_pi = { 0xC90FDAA22168C235ULL, 0x4000 };
static const X87::X87_Register k_x87_log10_2 = { 0x9A209A84FBCFF799ULL, 0x3FFD };
static const X87::X87_Register k_x87_ln_2 = { 0xB17217F7D1CF79ACULL, 0x3FFE };

static floatx80 ToFloat(const X87::X87_Register& value)
{
    floatx80 result;
    result.signif = value.significand;
    result.signExp = value.sign_exponent;
    return result;
}

static X87::X87_Register FromFloat(const floatx80& value)
{
    X87::X87_Register result;
    result.significand = value.signif;
    result.sign_exponent = value.signExp;
    return result;
}

static u16 LoadWord(const u8* data)
{
    return (u16)(data[0] | (data[1] << 8));
}

static u32 LoadDword(const u8* data)
{
    return (u32)data[0] | ((u32)data[1] << 8) | ((u32)data[2] << 16) | ((u32)data[3] << 24);
}

static u64 LoadQword(const u8* data)
{
    return (u64)LoadDword(data) | ((u64)LoadDword(data + 4) << 32);
}

static void StoreWord(u8* data, u16 value)
{
    data[0] = (u8)value;
    data[1] = (u8)(value >> 8);
}

static void StoreDword(u8* data, u32 value)
{
    StoreWord(data, (u16)value);
    StoreWord(data + 2, (u16)(value >> 16));
}

static void StoreQword(u8* data, u64 value)
{
    StoreDword(data, (u32)value);
    StoreDword(data + 4, (u32)(value >> 32));
}

static softfloat_status_t MakeStatus(u16 control, bool extended)
{
    softfloat_status_t status;
    int precision = (control >> 8) & 3;

    // Precision control 01 is reserved and rounds as 80 bits
    status.extF80_roundingPrecision = (extended || precision == 3 || precision == 1) ? 80 : precision == 2 ? 64 : 32;
    status.softfloat_exceptionFlags = 0;
    status.softfloat_roundingMode = (u8)((control >> 10) & 3);
    status.softfloat_flush_underflow_to_zero = false;
    status.softfloat_suppressException = 0;
    status.softfloat_exceptionMasks = (u8)(control & k_x87_exceptions);
    status.softfloat_denormals_are_zeros = false;
    return status;
}

static floatx80 Operate(int operation, floatx80 a, floatx80 b, softfloat_status_t& status)
{
    switch (operation)
    {
        case 0:
            return extF80_add(a, b, &status);
        case 1:
            return extF80_mul(a, b, &status);
        case 4:
            return extF80_sub(a, b, &status);
        case 5:
            return extF80_sub(b, a, &status);
        case 6:
            return extF80_div(a, b, &status);
        default:
            return extF80_div(b, a, &status);
    }
}

static floatx80 LoadRealOperand(u32 size, const u8* operand, softfloat_status_t& status)
{
    if (size == 4)
        return f32_to_extF80(LoadDword(operand), &status);

    return f64_to_extF80(LoadQword(operand), &status);
}

// The x87 rules for a NaN against a single or double operand, applied before the operand is converted
static bool HandleNaN(floatx80 a, u32 size, const u8* operand, floatx80& result, softfloat_status_t& status)
{
    if (extF80_isUnsupported(a))
    {
        softfloat_raiseFlags(&status, softfloat_flag_invalid);
        result = floatx80_default_nan;
        return true;
    }

    bool a_nan = extF80_isNaN(a);
    bool a_signaling = extF80_isSignalingNaN(a);
    bool b_nan = size == 4 ? f32_isNaN(LoadDword(operand)) : f64_isNaN(LoadQword(operand));
    bool b_signaling = size == 4 ? f32_isSignalingNaN(LoadDword(operand)) : f64_isSignalingNaN(LoadQword(operand));

    if (!a_nan && !b_nan)
        return false;

    if (a_signaling || b_signaling)
        softfloat_raiseFlags(&status, softfloat_flag_invalid);

    a = softfloat_propagateNaNExtF80UI(a.signExp, a.signif, 0, 0, &status);
    result = a;

    if (!b_nan)
        return true;

    floatx80 b = LoadRealOperand(size, operand, status);

    if (!a_nan || (a_signaling && !b_signaling))
        result = b;
    else if (a_signaling == b_signaling)
    {
        if (a.signif != b.signif)
            result = a.signif < b.signif ? b : a;
        else
            result = a.signExp < b.signExp ? a : b;
    }

    return true;
}

static u16 CompareCodes(int relation)
{
    switch (relation)
    {
        case softfloat_relation_less:
            return k_x87_status_c0;
        case softfloat_relation_equal:
            return k_x87_status_c3;
        case softfloat_relation_greater:
            return 0;
        default:
            return k_x87_status_unordered;
    }
}

static int TagOf(const X87::X87_Register& value)
{
    u16 exponent = value.sign_exponent & 0x7FFF;

    if (exponent == 0)
        return value.significand == 0 ? 1 : 2;

    if (exponent == 0x7FFF || (value.significand & 0x8000000000000000ULL) == 0)
        return 2;

    return 0;
}

X87::X87()
{
    m_flavor = X87_FLAVOR_80387;
    Reset();
}

X87::~X87()
{
}

void X87::Init(X87_Flavor flavor)
{
    m_flavor = flavor;
    Reset();
}

void X87::Reset()
{
    memset(&m_state, 0, sizeof(m_state));
    Initialize();

    // The 387 asserts ERROR# after RESET, which is how the 386 detects it and sets CR0 ET
    if (m_flavor == X87_FLAVOR_80387)
    {
        m_state.control &= ~k_x87_control_im;
        m_state.status |= k_x87_status_busy | k_x87_status_es | k_x87_status_ie;
    }
}

u16 X87::GetTagWord() const
{
    u16 tag = 0;

    for (int i = 0; i < X87_REGISTER_COUNT; i++)
    {
        int value = ((m_state.tag >> (i * 2)) & 3) == 3 ? 3 : TagOf(m_state.registers[i]);
        tag |= (u16)(value << (i * 2));
    }

    return tag;
}

bool X87::IsWaiting(u8 opcode, u8 modrm)
{
    u8 operation = (modrm >> 3) & 7;

    if ((modrm >> 6) != 3)
        return !((opcode == 0xD9 || opcode == 0xDD) && operation >= 6);

    return !((opcode == 0xDB && modrm >= 0xE0 && modrm <= 0xE4) || (opcode == 0xDF && modrm == 0xE0));
}

u32 X87::GetMemoryOperandSize(u8 opcode, u8 modrm, bool operand32, bool& write)
{
    u8 operation = (modrm >> 3) & 7;
    write = false;

    switch (opcode)
    {
        case 0xD8:
        case 0xDA:
            return 4;
        case 0xDC:
            return 8;
        case 0xDE:
            return 2;
        case 0xD9:
            write = operation == 2 || operation == 3 || operation >= 6;

            if (operation == 0 || operation == 2 || operation == 3)
                return 4;

            if (operation == 4 || operation == 6)
                return operand32 ? 28 : 14;

            return operation == 1 ? 0 : 2;
        case 0xDB:
            write = operation == 2 || operation == 3 || operation == 7;

            if (operation == 0 || operation == 2 || operation == 3)
                return 4;

            return (operation == 5 || operation == 7) ? 10 : 0;
        case 0xDD:
            write = operation == 2 || operation == 3 || operation >= 6;

            if (operation == 0 || operation == 2 || operation == 3)
                return 8;

            if (operation == 4 || operation == 6)
                return operand32 ? 108 : 94;

            return operation == 7 ? 2 : 0;
        default:
            write = operation == 2 || operation == 3 || operation >= 6;

            if (operation == 0 || operation == 2 || operation == 3)
                return 2;

            if (operation == 4 || operation == 6)
                return 10;

            return operation == 1 ? 0 : 8;
    }
}

bool X87::Execute(const X87_Operation& operation, u8* operand)
{
    u8 reg = (operation.modrm >> 3) & 7;
    bool control = operation.memory ? (operation.opcode == 0xD9 && reg >= 4) ||
        (operation.opcode == 0xDD && (reg == 4 || reg >= 6)) : (operation.opcode == 0xDB &&
        operation.modrm >= 0xE0 && operation.modrm <= 0xE4) || (operation.opcode == 0xDF && operation.modrm == 0xE0);

    if (!control)
        UpdatePointers(operation);

    if (operation.memory)
        return ExecuteMemory(operation, operand);

    ExecuteRegister(operation);
    return false;
}

u32 X87::GetClocks(u8 opcode, u8 modrm, const u8* operand) const
{
    int row = opcode & 7;
    int reg = (modrm >> 3) & 7;

    if ((modrm >> 6) != 3)
    {
        u32 clocks = k_x87_memory_clocks[row][reg];

        // Loading a single or double zero takes 5 more
        if (reg == 0 && opcode == 0xD9 && (LoadDword(operand) & 0x7FFFFFFF) == 0)
            clocks += 5;
        else if (reg == 0 && opcode == 0xDD && (LoadQword(operand) & 0x7FFFFFFFFFFFFFFFULL) == 0)
            clocks += 5;

        return clocks;
    }

    if (opcode == 0xD9 && modrm >= 0xE0)
        return k_x87_d9_clocks[modrm - 0xE0];

    if ((opcode == 0xDA && modrm == 0xE9) || (opcode == 0xDE && modrm == 0xD9))
        return 17;

    if (opcode == 0xDB && (modrm == 0xE2 || modrm == 0xE3))
        return modrm == 0xE2 ? 11 : 33;

    if (opcode == 0xDF && modrm == 0xE0)
        return 13;

    return k_x87_register_clocks[row][reg];
}

void X87::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void X87::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void X87::Serialize(StateSerializer& serializer)
{
    for (int i = 0; i < X87_REGISTER_COUNT; i++)
    {
        G_SERIALIZE(serializer, m_state.registers[i].significand);
        G_SERIALIZE(serializer, m_state.registers[i].sign_exponent);
    }

    G_SERIALIZE(serializer, m_state.control);
    G_SERIALIZE(serializer, m_state.status);
    G_SERIALIZE(serializer, m_state.tag);
    G_SERIALIZE(serializer, m_state.opcode);
    G_SERIALIZE(serializer, m_state.instruction_offset);
    G_SERIALIZE(serializer, m_state.instruction_selector);
    G_SERIALIZE(serializer, m_state.operand_offset);
    G_SERIALIZE(serializer, m_state.operand_selector);
}

void X87::SanitizeState()
{
    m_state.control = (u16)((m_state.control & ~k_x87_control_reserved) | k_x87_control_fixed);
    m_state.opcode &= 0x07FF;
}

void X87::Initialize()
{
    m_state.control = 0x037F;
    m_state.status = 0;
    m_state.tag = 0xFFFF;

    // Only the 486 clears the pointers, the 387 keeps them
    if (m_flavor == X87_FLAVOR_80486)
    {
        m_state.opcode = 0;
        m_state.instruction_offset = 0;
        m_state.instruction_selector = 0;
        m_state.operand_offset = 0;
        m_state.operand_selector = 0;
    }
}

void X87::UpdatePointers(const X87_Operation& operation)
{
    m_state.opcode = (u16)(((operation.opcode & 7) << 8) | operation.modrm);
    m_state.instruction_selector = operation.instruction_selector;
    m_state.instruction_offset = operation.instruction_offset;

    if (operation.memory)
    {
        m_state.operand_selector = operation.operand_selector;
        m_state.operand_offset = operation.operand_offset;
    }
}

void X87::ExecuteRegister(const X87_Operation& operation)
{
    u8 modrm = operation.modrm;
    int reg = (modrm >> 3) & 7;
    int rm = modrm & 7;

    switch (operation.opcode)
    {
        case 0xD8:
            if (reg == 2 || reg == 3)
                Compare(rm, reg - 2, false);
            else
                Arithmetic(reg, 0, rm, false);
            break;
        case 0xD9:
            ExecuteD9(modrm);
            break;
        case 0xDA:
            if (modrm == 0xE9)
                Compare(1, 2, true);
            break;
        case 0xDB:
            if (modrm == 0xE2)
                m_state.status &= ~(k_x87_status_busy | k_x87_status_es | k_x87_status_sf | k_x87_exceptions);
            else if (modrm == 0xE3)
                Initialize();
            break;
        case 0xDC:
            if (reg == 2 || reg == 3)
                Compare(rm, reg - 2, false);
            else
                Arithmetic(reg >= 4 ? reg ^ 1 : reg, rm, 0, false);
            break;
        case 0xDD:
            if (reg == 0)
                Free(rm, false);
            else if (reg == 1)
                Exchange(rm);
            else if (reg == 2 || reg == 3)
                StoreRegister(rm, reg == 3, false);
            else if (reg == 4 || reg == 5)
                Compare(rm, reg - 4, true);
            break;
        case 0xDE:
            if (reg == 2)
                Compare(rm, 1, false);
            else if (reg == 3)
            {
                if (rm == 1)
                    Compare(1, 2, false);
            }
            else
                Arithmetic(reg >= 4 ? reg ^ 1 : reg, rm, 0, true);
            break;
        default:
            if (reg == 0)
                Free(rm, true);
            else if (reg == 1)
                Exchange(rm);
            else if (reg == 2 || reg == 3)
                StoreRegister(rm, true, true);
            break;
    }
}

void X87::ExecuteD9(u8 modrm)
{
    int reg = (modrm >> 3) & 7;
    int rm = modrm & 7;
    bool chop_or_down = (m_state.control & 0x0400) != 0;

    switch (reg)
    {
        case 0:
            LoadRegister(rm);
            return;
        case 1:
            Exchange(rm);
            return;
        case 3:
            StoreRegister(rm, true, true);
            return;
        default:
            break;
    }

    switch (modrm)
    {
        case 0xE0:
            ChangeSign(false);
            break;
        case 0xE1:
            ChangeSign(true);
            break;
        case 0xE4:
            Test();
            break;
        case 0xE5:
            Examine();
            break;
        case 0xE8:
            LoadConstant(k_x87_one, 0);
            break;
        case 0xE9:
            LoadConstant(k_x87_log2_10, (m_state.control & 0x0C00) == 0x0800 ? 1 : 0);
            break;
        case 0xEA:
            LoadConstant(k_x87_log2_e, chop_or_down ? -1 : 0);
            break;
        case 0xEB:
            LoadConstant(k_x87_pi, chop_or_down ? -1 : 0);
            break;
        case 0xEC:
            LoadConstant(k_x87_log10_2, chop_or_down ? -1 : 0);
            break;
        case 0xED:
            LoadConstant(k_x87_ln_2, chop_or_down ? -1 : 0);
            break;
        case 0xEE:
            LoadConstant(k_x87_zero, 0);
            break;
        case 0xF0:
        case 0xFA:
        case 0xFC:
        case 0xFE:
        case 0xFF:
            Unary(modrm);
            break;
        case 0xF1:
        case 0xF3:
        case 0xF9:
        case 0xFD:
            Binary(modrm);
            break;
        case 0xF2:
        case 0xF4:
        case 0xFB:
            PushPair(modrm);
            break;
        case 0xF5:
        case 0xF8:
            Remainder(modrm == 0xF5);
            break;
        case 0xF6:
            ClearC1();
            SetTop(GetTop() - 1);
            break;
        case 0xF7:
            ClearC1();
            SetTop(GetTop() + 1);
            break;
        default:
            break;
    }
}

bool X87::ExecuteMemory(const X87_Operation& operation, u8* operand)
{
    int reg = (operation.modrm >> 3) & 7;

    switch (operation.opcode)
    {
        case 0xD8:
        case 0xDC:
            if (reg == 2 || reg == 3)
                CompareReal(operation.opcode == 0xD8 ? 4 : 8, operand, reg == 3);
            else
                ArithmeticReal(reg, operation.opcode == 0xD8 ? 4 : 8, operand);
            return false;
        case 0xDA:
        case 0xDE:
        {
            s32 value = operation.opcode == 0xDA ? (s32)LoadDword(operand) : (s16)LoadWord(operand);

            if (reg == 2 || reg == 3)
                CompareInteger(value, reg == 3);
            else
                ArithmeticInteger(reg, value);
            return false;
        }
        case 0xD9:
            switch (reg)
            {
                case 0:
                    LoadReal(4, operand);
                    return false;
                case 2:
                case 3:
                    return StoreReal(4, operand, reg == 3);
                case 4:
                    LoadEnvironment(operand, operation.protected_mode, operation.operand32);
                    return false;
                case 5:
                    m_state.control = (u16)((LoadWord(operand) & ~k_x87_control_reserved) | k_x87_control_fixed);
                    UpdateErrorSummary();
                    return false;
                case 6:
                    StoreEnvironment(operand, operation.protected_mode, operation.operand32);
                    m_state.control |= k_x87_exceptions;
                    m_state.status &= ~(k_x87_status_busy | k_x87_status_es);
                    return true;
                case 7:
                    StoreWord(operand, m_state.control);
                    return true;
                default:
                    return false;
            }
        case 0xDB:
            switch (reg)
            {
                case 0:
                    LoadInteger((s32)LoadDword(operand));
                    return false;
                case 2:
                case 3:
                    return StoreInteger(4, operand, reg == 3);
                case 5:
                    LoadExtended(operand);
                    return false;
                case 7:
                    return StoreExtended(operand);
                default:
                    return false;
            }
        case 0xDD:
            switch (reg)
            {
                case 0:
                    LoadReal(8, operand);
                    return false;
                case 2:
                case 3:
                    return StoreReal(8, operand, reg == 3);
                case 4:
                    Restore(operand, operation.protected_mode, operation.operand32);
                    return false;
                case 6:
                    Save(operand, operation.protected_mode, operation.operand32);
                    return true;
                case 7:
                    StoreWord(operand, m_state.status);
                    return true;
                default:
                    return false;
            }
        default:
            switch (reg)
            {
                case 0:
                    LoadInteger((s16)LoadWord(operand));
                    return false;
                case 2:
                case 3:
                    return StoreInteger(2, operand, reg == 3);
                case 4:
                    LoadBCD(operand);
                    return false;
                case 5:
                    LoadInteger((s64)LoadQword(operand));
                    return false;
                case 6:
                    return StoreBCD(operand);
                case 7:
                    return StoreInteger(8, operand, true);
                default:
                    return false;
            }
    }
}

u32 X87::RaiseExceptions(u32 exceptions, bool store)
{
    exceptions &= 0x027F;
    u16 previous = m_state.status;
    u32 unmasked = exceptions & ~m_state.control & k_x87_exceptions;

    if ((exceptions & (k_x87_status_ie | k_x87_status_ze)) != 0)
        unmasked &= k_x87_status_ie | k_x87_status_ze;

    if (unmasked != 0)
        m_state.status |= k_x87_status_busy | k_x87_status_es;

    if ((exceptions & k_x87_status_ie) != 0)
    {
        m_state.status |= (u16)exceptions;

        // C1 tells a stack overflow from an underflow
        if ((exceptions & k_x87_status_sf) != 0 && (exceptions & k_x87_status_c1) == 0)
            m_state.status &= ~k_x87_status_c1;

        return unmasked;
    }

    if ((exceptions & k_x87_status_ze) != 0)
    {
        m_state.status |= k_x87_status_ze;
        return unmasked;
    }

    if ((exceptions & k_x87_status_de) != 0)
    {
        m_state.status |= k_x87_status_de;

        if ((unmasked & k_x87_status_de) != 0)
            return unmasked & k_x87_status_de;
    }

    m_state.status |= (u16)exceptions;

    // C1 tells a result rounded up from one rounded down
    if ((exceptions & k_x87_status_pe) != 0 && (exceptions & k_x87_status_c1) == 0)
        m_state.status &= ~k_x87_status_c1;

    unmasked &= ~k_x87_status_pe;

    // An unmasked overflow or underflow still writes the rescaled result to a register, never to memory
    if ((unmasked & (k_x87_status_ue | k_x87_status_oe)) != 0)
    {
        if (!store)
            unmasked &= ~(k_x87_status_ue | k_x87_status_oe);
        else
        {
            m_state.status &= ~k_x87_status_c1;

            if ((previous & k_x87_status_pe) == 0)
                m_state.status &= ~k_x87_status_pe;
        }
    }

    return unmasked;
}

void X87::StackOverflow()
{
    if ((m_state.control & k_x87_control_im) != 0)
    {
        Push();
        Write(k_x87_default_nan, 0);
    }

    RaiseExceptions(k_x87_stack_overflow);
}

void X87::StackUnderflow(int index, bool pop)
{
    if ((m_state.control & k_x87_control_im) != 0)
    {
        Write(k_x87_default_nan, index);

        if (pop)
            Pop();
    }

    RaiseExceptions(k_x87_stack_underflow);
}

void X87::UpdateErrorSummary()
{
    if ((m_state.status & ~m_state.control & k_x87_exceptions) != 0)
        m_state.status |= k_x87_status_busy | k_x87_status_es;
    else
        m_state.status &= ~(k_x87_status_busy | k_x87_status_es);
}

void X87::Arithmetic(int operation, int destination, int source, bool pop)
{
    ClearC1();

    if (IsEmpty(0) || IsEmpty(destination == 0 ? source : destination))
    {
        StackUnderflow(destination, pop);
        return;
    }

    softfloat_status_t status = MakeStatus(m_state.control, false);
    floatx80 result = Operate(operation, ToFloat(Read(destination)), ToFloat(Read(source)), status);

    if (RaiseExceptions(status.softfloat_exceptionFlags) != 0)
        return;

    Write(FromFloat(result), destination);

    if (pop)
        Pop();
}

void X87::ArithmeticReal(int operation, u32 size, const u8* operand)
{
    ClearC1();

    if (IsEmpty(0))
    {
        StackUnderflow(0, false);
        return;
    }

    softfloat_status_t status = MakeStatus(m_state.control, false);
    floatx80 a = ToFloat(Read(0));
    floatx80 result;

    if (!HandleNaN(a, size, operand, result, status))
        result = Operate(operation, a, LoadRealOperand(size, operand, status), status);

    if (RaiseExceptions(status.softfloat_exceptionFlags) == 0)
        Write(FromFloat(result), 0);
}

void X87::ArithmeticInteger(int operation, s32 value)
{
    ClearC1();

    if (IsEmpty(0))
    {
        StackUnderflow(0, false);
        return;
    }

    softfloat_status_t status = MakeStatus(m_state.control, false);
    floatx80 result = Operate(operation, ToFloat(Read(0)), i32_to_extF80(value), status);

    if (RaiseExceptions(status.softfloat_exceptionFlags) == 0)
        Write(FromFloat(result), 0);
}

void X87::Compare(int source, int pops, bool quiet)
{
    ClearC1();

    if (IsEmpty(0) || IsEmpty(source))
    {
        RaiseExceptions(k_x87_stack_underflow);
        SetConditionCodes(k_x87_status_unordered);

        for (int i = 0; ((m_state.control & k_x87_control_im) != 0) && (i < pops); i++)
            Pop();

        return;
    }

    softfloat_status_t status = MakeStatus(m_state.control, false);
    int relation = extF80_compare(ToFloat(Read(0)), ToFloat(Read(source)), quiet ? 1 : 0, &status);
    SetConditionCodes(CompareCodes(relation));

    if (RaiseExceptions(status.softfloat_exceptionFlags) != 0)
        return;

    for (int i = 0; i < pops; i++)
        Pop();
}

void X87::CompareReal(u32 size, const u8* operand, bool pop)
{
    ClearC1();

    if (IsEmpty(0))
    {
        RaiseExceptions(k_x87_stack_underflow);
        SetConditionCodes(k_x87_status_unordered);

        if (pop && ((m_state.control & k_x87_control_im) != 0))
            Pop();

        return;
    }

    softfloat_status_t status = MakeStatus(m_state.control, false);
    floatx80 a = ToFloat(Read(0));
    bool b_nan = size == 4 ? f32_isNaN(LoadDword(operand)) : f64_isNaN(LoadQword(operand));
    int relation = softfloat_relation_unordered;

    if (extF80_isNaN(a) || extF80_isUnsupported(a) || b_nan)
        softfloat_raiseFlags(&status, softfloat_flag_invalid);
    else
        relation = extF80_compare(a, LoadRealOperand(size, operand, status), &status);

    SetConditionCodes(CompareCodes(relation));

    if (RaiseExceptions(status.softfloat_exceptionFlags) == 0 && pop)
        Pop();
}

void X87::CompareInteger(s32 value, bool pop)
{
    ClearC1();

    if (IsEmpty(0))
    {
        RaiseExceptions(k_x87_stack_underflow);
        SetConditionCodes(k_x87_status_unordered);

        if (pop && ((m_state.control & k_x87_control_im) != 0))
            Pop();

        return;
    }

    softfloat_status_t status = MakeStatus(m_state.control, false);
    int relation = extF80_compare(ToFloat(Read(0)), i32_to_extF80(value), &status);
    SetConditionCodes(CompareCodes(relation));

    if (RaiseExceptions(status.softfloat_exceptionFlags) == 0 && pop)
        Pop();
}

void X87::Test()
{
    ClearC1();

    if (IsEmpty(0))
    {
        RaiseExceptions(k_x87_stack_underflow);
        SetConditionCodes(k_x87_status_unordered);
        return;
    }

    softfloat_status_t status = MakeStatus(m_state.control, false);
    int relation = extF80_compare(ToFloat(Read(0)), ToFloat(k_x87_zero), &status);
    SetConditionCodes(CompareCodes(relation));
    RaiseExceptions(status.softfloat_exceptionFlags);
}

void X87::Examine()
{
    const X87_Register& value = Read(0);
    u16 codes = (value.sign_exponent & 0x8000) != 0 ? k_x87_status_c1 : 0;

    if (IsEmpty(0))
        codes |= k_x87_status_c3 | k_x87_status_c0;
    else
    {
        floatx80 number = ToFloat(value);

        switch (extF80_class(number))
        {
            case softfloat_zero:
                codes |= k_x87_status_c3;
                break;
            case softfloat_SNaN:
            case softfloat_QNaN:
                if (!extF80_isUnsupported(number))
                    codes |= k_x87_status_c0;
                break;
            case softfloat_negative_inf:
            case softfloat_positive_inf:
                codes |= k_x87_status_c2 | k_x87_status_c0;
                break;
            case softfloat_denormal:
                codes |= k_x87_status_c3 | k_x87_status_c2;
                break;
            default:
                codes |= k_x87_status_c2;
                break;
        }
    }

    SetConditionCodes(codes);
}

void X87::LoadRegister(int source)
{
    ClearC1();

    if (!IsEmpty(-1))
    {
        StackOverflow();
        return;
    }

    X87_Register value = k_x87_default_nan;

    if (IsEmpty(source))
    {
        RaiseExceptions(k_x87_stack_underflow);

        if ((m_state.control & k_x87_control_im) == 0)
            return;
    }
    else
        value = Read(source);

    Push();
    Write(value, 0);
}

void X87::LoadReal(u32 size, const u8* operand)
{
    ClearC1();

    if (!IsEmpty(-1))
    {
        StackOverflow();
        return;
    }

    softfloat_status_t status = MakeStatus(m_state.control, false);
    floatx80 result = LoadRealOperand(size, operand, status);

    if ((RaiseExceptions(status.softfloat_exceptionFlags) & k_x87_status_ie) != 0)
        return;

    Push();
    Write(FromFloat(result), 0);
}

void X87::LoadExtended(const u8* operand)
{
    ClearC1();

    if (!IsEmpty(-1))
    {
        StackOverflow();
        return;
    }

    X87_Register value;
    value.significand = LoadQword(operand);
    value.sign_exponent = LoadWord(operand + 8);
    Push();
    Write(value, 0);
}

void X87::LoadInteger(s64 value)
{
    ClearC1();

    if (!IsEmpty(-1))
    {
        StackOverflow();
        return;
    }

    Push();
    Write(FromFloat(i64_to_extF80(value)), 0);
}

void X87::LoadBCD(const u8* operand)
{
    ClearC1();

    if (!IsEmpty(-1))
    {
        StackOverflow();
        return;
    }

    u64 digits = LoadQword(operand);
    u16 high = LoadWord(operand + 8);
    s64 scale = 1;
    s64 value = 0;

    for (int i = 0; i < 16; i++)
    {
        value += (s64)(digits & 0x0F) * scale;
        digits >>= 4;
        scale *= 10;
    }

    value += (s64)(high & 0x0F) * scale;
    value += (s64)((high >> 4) & 0x0F) * scale * 10;

    X87_Register result = FromFloat(i64_to_extF80(value));

    if ((high & 0x8000) != 0)
        result.sign_exponent ^= 0x8000;

    Push();
    Write(result, 0);
}

void X87::LoadConstant(const X87_Register& value, int adjust)
{
    ClearC1();

    if (!IsEmpty(-1))
    {
        StackOverflow();
        return;
    }

    X87_Register result = value;
    result.significand += (s64)adjust;
    Push();
    Write(result, 0);
}

void X87::StoreRegister(int destination, bool pop, bool alias)
{
    ClearC1();

    if (IsEmpty(0))
    {
        // The undocumented FSTP aliases pop an empty register without a stack fault
        if (alias)
            Pop();
        else
            StackUnderflow(destination, pop);

        return;
    }

    Write(Read(0), destination);

    if (pop)
        Pop();
}

bool X87::StoreReal(u32 size, u8* operand, bool pop)
{
    ClearC1();

    u64 value = size == 4 ? 0xFFC00000ULL : 0xFFF8000000000000ULL;

    if (IsEmpty(0))
    {
        RaiseExceptions(k_x87_stack_underflow);

        if ((m_state.control & k_x87_control_im) == 0)
            return false;
    }
    else
    {
        softfloat_status_t status = MakeStatus(m_state.control, false);
        floatx80 source = ToFloat(Read(0));
        value = size == 4 ? (u64)extF80_to_f32(source, &status) : (u64)extF80_to_f64(source, &status);

        if (RaiseExceptions(status.softfloat_exceptionFlags, true) != 0)
            return false;
    }

    if (size == 4)
        StoreDword(operand, (u32)value);
    else
        StoreQword(operand, value);

    if (pop)
        Pop();

    return true;
}

bool X87::StoreExtended(u8* operand)
{
    ClearC1();

    X87_Register value = k_x87_default_nan;

    if (IsEmpty(0))
    {
        RaiseExceptions(k_x87_stack_underflow);

        if ((m_state.control & k_x87_control_im) == 0)
            return false;
    }
    else
        value = Read(0);

    StoreQword(operand, value.significand);
    StoreWord(operand + 8, value.sign_exponent);
    Pop();
    return true;
}

bool X87::StoreInteger(u32 size, u8* operand, bool pop)
{
    ClearC1();

    s64 value = size == 2 ? (s64)int16_indefinite : size == 4 ? (s64)int32_indefinite : int64_indefinite;

    if (IsEmpty(0))
    {
        RaiseExceptions(k_x87_stack_underflow);

        if ((m_state.control & k_x87_control_im) == 0)
            return false;
    }
    else
    {
        softfloat_status_t status = MakeStatus(m_state.control, false);
        floatx80 source = ToFloat(Read(0));

        if (size == 2)
            value = extF80_to_i16(source, &status);
        else if (size == 4)
            value = extF80_to_i32(source, &status);
        else
            value = extF80_to_i64(source, &status);

        if (RaiseExceptions(status.softfloat_exceptionFlags, true) != 0)
            return false;
    }

    if (size == 2)
        StoreWord(operand, (u16)value);
    else if (size == 4)
        StoreDword(operand, (u32)value);
    else
        StoreQword(operand, (u64)value);

    if (pop)
        Pop();

    return true;
}

bool X87::StoreBCD(u8* operand)
{
    ClearC1();

    u16 high = 0xFFFF;
    u64 low = 0xC000000000000000ULL;

    if (IsEmpty(0))
    {
        RaiseExceptions(k_x87_stack_underflow);

        if ((m_state.control & k_x87_control_im) == 0)
            return false;
    }
    else
    {
        softfloat_status_t status = MakeStatus(m_state.control, false);
        floatx80 source = ToFloat(Read(0));
        u64 value = (u64)extF80_to_i64(source, &status);
        bool negative = extF80_sign(source);

        if (negative)
            value = 0 - value;

        // Only 18 digits fit, anything larger stores the indefinite
        if ((s64)value > 999999999999999999LL)
            softfloat_setFlags(&status, softfloat_flag_invalid);

        if ((status.softfloat_exceptionFlags & softfloat_flag_invalid) == 0)
        {
            high = negative ? 0x8000 : 0;
            low = 0;

            for (int i = 0; i < 16; i++)
            {
                low |= (value % 10) << (4 * i);
                value /= 10;
            }

            high |= (u16)(value % 10);
            value /= 10;
            high |= (u16)((value % 10) << 4);
        }

        if (RaiseExceptions(status.softfloat_exceptionFlags, true) != 0)
            return false;
    }

    StoreQword(operand, low);
    StoreWord(operand + 8, high);
    Pop();
    return true;
}

void X87::Exchange(int source)
{
    ClearC1();

    int tag0 = GetTag(0);
    int tag = GetTag(source);
    X87_Register st0 = Read(0);
    X87_Register sti = Read(source);

    if (tag0 == 3 || tag == 3)
    {
        RaiseExceptions(k_x87_stack_underflow);

        if ((m_state.control & k_x87_control_im) == 0)
            return;

        if (tag0 == 3)
            st0 = k_x87_default_nan;

        if (tag == 3)
            sti = k_x87_default_nan;
    }

    Write(st0, source);
    Write(sti, 0);
}

void X87::Free(int index, bool pop)
{
    ClearC1();
    SetTag(index, 3);

    if (pop)
        Pop();
}

void X87::ChangeSign(bool absolute)
{
    if (IsEmpty(0))
    {
        StackUnderflow(0, false);
        return;
    }

    ClearC1();

    X87_Register value = Read(0);

    if (absolute)
        value.sign_exponent &= 0x7FFF;
    else
        value.sign_exponent ^= 0x8000;

    Write(value, 0);
}

void X87::Unary(u8 modrm)
{
    ClearC1();

    if (modrm == 0xFE || modrm == 0xFF)
        m_state.status &= ~k_x87_status_c2;

    if (IsEmpty(0))
    {
        StackUnderflow(0, false);
        return;
    }

    softfloat_status_t status = MakeStatus(m_state.control, modrm != 0xFA && modrm != 0xFC);
    floatx80 value = ToFloat(Read(0));

    switch (modrm)
    {
        case 0xF0:
            value = f2xm1(value, status);
            break;
        case 0xFA:
            value = extF80_sqrt(value, &status);
            break;
        case 0xFC:
            value = extF80_roundToInt(value, &status);
            break;
        default:
            // Arguments beyond 2^63 are left alone with C2 set
            if ((modrm == 0xFE ? fsin(value, status) : fcos(value, status)) == -1)
            {
                m_state.status |= k_x87_status_c2;
                return;
            }
            break;
    }

    if (RaiseExceptions(status.softfloat_exceptionFlags) == 0)
        Write(FromFloat(value), 0);
}

void X87::Binary(u8 modrm)
{
    ClearC1();

    bool scale = modrm == 0xFD;

    if (IsEmpty(0) || IsEmpty(1))
    {
        if (scale)
            StackUnderflow(0, false);
        else
            StackUnderflow(1, true);

        return;
    }

    softfloat_status_t status = MakeStatus(m_state.control, !scale);
    floatx80 a = ToFloat(Read(0));
    floatx80 b = ToFloat(Read(1));
    floatx80 result;

    switch (modrm)
    {
        case 0xF1:
            result = fyl2x(a, b, status);
            break;
        case 0xF3:
            result = fpatan(a, b, status);
            break;
        case 0xF9:
            result = fyl2xp1(a, b, status);
            break;
        default:
            result = extF80_scale(a, b, &status);
            break;
    }

    if (RaiseExceptions(status.softfloat_exceptionFlags) != 0)
        return;

    if (!scale)
        Pop();

    Write(FromFloat(result), 0);
}

void X87::PushPair(u8 modrm)
{
    ClearC1();

    if (modrm != 0xF4)
        m_state.status &= ~k_x87_status_c2;

    if (IsEmpty(0) || !IsEmpty(-1))
    {
        RaiseExceptions(IsEmpty(0) ? k_x87_stack_underflow : k_x87_stack_overflow);

        if ((m_state.control & k_x87_control_im) != 0)
        {
            Write(k_x87_default_nan, 0);
            Push();
            Write(k_x87_default_nan, 0);
        }

        return;
    }

    softfloat_status_t status = MakeStatus(m_state.control, modrm != 0xF4);
    floatx80 first = ToFloat(Read(0));
    floatx80 second = first;

    if (modrm == 0xF4)
        first = extF80_extract(&second, &status);
    else if (modrm == 0xF2)
    {
        if (fptan(first, status) == -1)
        {
            m_state.status |= k_x87_status_c2;
            return;
        }

        second = extF80_isNaN(first) ? first : ToFloat(k_x87_one);
    }
    else if (fsincos(first, &first, &second, status) == -1)
    {
        m_state.status |= k_x87_status_c2;
        return;
    }

    if (RaiseExceptions(status.softfloat_exceptionFlags) != 0)
        return;

    Write(FromFloat(first), 0);
    Push();
    Write(FromFloat(second), 0);
}

void X87::Remainder(bool ieee)
{
    ClearC1();
    m_state.status &= ~k_x87_status_c2;

    if (IsEmpty(0) || IsEmpty(1))
    {
        StackUnderflow(0, false);
        return;
    }

    softfloat_status_t status = MakeStatus(m_state.control, false);
    floatx80 result;
    Bit64u quotient = 0;
    int partial = ieee ? floatx80_ieee754_remainder(ToFloat(Read(0)), ToFloat(Read(1)), result, quotient, &status) :
        floatx80_remainder(ToFloat(Read(0)), ToFloat(Read(1)), result, quotient, &status);

    if (RaiseExceptions(status.softfloat_exceptionFlags) != 0)
        return;

    // A complete reduction reports the low three quotient bits, a partial one sets C2
    if (partial > 0)
        SetConditionCodes(k_x87_status_c2);
    else if (partial == 0)
        SetConditionCodes(((quotient & 1) != 0 ? k_x87_status_c1 : 0) | ((quotient & 2) != 0 ? k_x87_status_c3 : 0) |
            ((quotient & 4) != 0 ? k_x87_status_c0 : 0));

    Write(FromFloat(result), 0);
}

u32 X87::StoreEnvironment(u8* buffer, bool protected_mode, bool operand32)
{
    u16 tag = GetTagWord();
    u32 instruction = m_state.instruction_offset;
    u32 operand = m_state.operand_offset;

    if (!protected_mode)
    {
        instruction += (u32)m_state.instruction_selector << 4;
        operand += (u32)m_state.operand_selector << 4;
    }

    if (operand32)
    {
        StoreDword(buffer + 0x00, 0xFFFF0000U | m_state.control);
        StoreDword(buffer + 0x04, 0xFFFF0000U | m_state.status);
        StoreDword(buffer + 0x08, 0xFFFF0000U | tag);

        if (protected_mode)
        {
            StoreDword(buffer + 0x0C, instruction);
            StoreDword(buffer + 0x10, m_state.instruction_selector | ((u32)m_state.opcode << 16));
            StoreDword(buffer + 0x14, operand);
            StoreDword(buffer + 0x18, 0xFFFF0000U | m_state.operand_selector);
        }
        else
        {
            StoreDword(buffer + 0x0C, 0xFFFF0000U | (instruction & 0xFFFF));
            StoreDword(buffer + 0x10, ((instruction & 0xFFFF0000U) >> 4) | m_state.opcode);
            StoreDword(buffer + 0x14, 0xFFFF0000U | (operand & 0xFFFF));
            StoreDword(buffer + 0x18, (operand & 0xFFFF0000U) >> 4);
        }

        return 28;
    }

    StoreWord(buffer + 0x00, m_state.control);
    StoreWord(buffer + 0x02, m_state.status);
    StoreWord(buffer + 0x04, tag);

    if (protected_mode)
    {
        StoreWord(buffer + 0x06, (u16)instruction);
        StoreWord(buffer + 0x08, m_state.instruction_selector);
        StoreWord(buffer + 0x0A, (u16)operand);
        StoreWord(buffer + 0x0C, m_state.operand_selector);
    }
    else
    {
        StoreWord(buffer + 0x06, (u16)instruction);
        StoreWord(buffer + 0x08, (u16)(((instruction & 0xF0000) >> 4) | m_state.opcode));
        StoreWord(buffer + 0x0A, (u16)operand);
        StoreWord(buffer + 0x0C, (u16)((operand & 0xF0000) >> 4));
    }

    return 14;
}

u32 X87::LoadEnvironment(const u8* buffer, bool protected_mode, bool operand32)
{
    u16 tag = 0;

    if (operand32)
    {
        m_state.control = LoadWord(buffer + 0x00);
        m_state.status = LoadWord(buffer + 0x04);
        tag = LoadWord(buffer + 0x08);

        if (protected_mode)
        {
            u32 selector = LoadDword(buffer + 0x10);
            m_state.instruction_offset = LoadDword(buffer + 0x0C);
            m_state.instruction_selector = (u16)selector;
            m_state.opcode = (u16)((selector >> 16) & 0x07FF);
            m_state.operand_offset = LoadDword(buffer + 0x14);
            m_state.operand_selector = LoadWord(buffer + 0x18);
        }
        else
        {
            u32 opcode = LoadDword(buffer + 0x10);
            m_state.instruction_offset = ((opcode & 0x0FFFF000U) << 4) | LoadWord(buffer + 0x0C);
            m_state.instruction_selector = 0;
            m_state.opcode = (u16)(opcode & 0x07FF);
            m_state.operand_offset = ((LoadDword(buffer + 0x18) & 0x0FFFF000U) << 4) | LoadWord(buffer + 0x14);
            m_state.operand_selector = 0;
        }
    }
    else
    {
        m_state.control = LoadWord(buffer + 0x00);
        m_state.status = LoadWord(buffer + 0x02);
        tag = LoadWord(buffer + 0x04);

        if (protected_mode)
        {
            m_state.instruction_offset = LoadWord(buffer + 0x06);
            m_state.instruction_selector = LoadWord(buffer + 0x08);
            m_state.opcode = 0;
            m_state.operand_offset = LoadWord(buffer + 0x0A);
            m_state.operand_selector = LoadWord(buffer + 0x0C);
        }
        else
        {
            u16 opcode = LoadWord(buffer + 0x08);
            m_state.instruction_offset = ((u32)(opcode & 0xF000) << 4) | LoadWord(buffer + 0x06);
            m_state.instruction_selector = 0;
            m_state.opcode = opcode & 0x07FF;
            m_state.operand_offset = ((u32)(LoadWord(buffer + 0x0C) & 0xF000) << 4) | LoadWord(buffer + 0x0A);
            m_state.operand_selector = 0;
        }
    }

    // Only empty and in use are kept, the class of each register comes from its contents
    m_state.tag = 0;

    for (int i = 0; i < X87_REGISTER_COUNT; i++)
    {
        if (((tag >> (i * 2)) & 3) == 3)
            m_state.tag |= (u16)(3 << (i * 2));
    }

    m_state.control = (u16)((m_state.control & ~k_x87_control_reserved) | k_x87_control_fixed);
    UpdateErrorSummary();
    return operand32 ? 28 : 14;
}

void X87::Save(u8* buffer, bool protected_mode, bool operand32)
{
    u32 offset = StoreEnvironment(buffer, protected_mode, operand32);

    for (int i = 0; i < X87_REGISTER_COUNT; i++)
    {
        const X87_Register& value = Read(i);
        StoreQword(buffer + offset + i * 10, value.significand);
        StoreWord(buffer + offset + i * 10 + 8, value.sign_exponent);
    }

    Initialize();
}

void X87::Restore(const u8* buffer, bool protected_mode, bool operand32)
{
    u32 offset = LoadEnvironment(buffer, protected_mode, operand32);

    for (int i = 0; i < X87_REGISTER_COUNT; i++)
    {
        X87_Register& value = m_state.registers[(GetTop() + i) & 7];
        value.significand = LoadQword(buffer + offset + i * 10);
        value.sign_exponent = LoadWord(buffer + offset + i * 10 + 8);
    }
}
