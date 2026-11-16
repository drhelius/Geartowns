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

#if defined(_MSC_VER)
#include <intrin.h>
#endif

static const u32 k_x87_fast_inexact = 0x0020;
static const u32 k_x87_fast_round_up = 0x0200;
static const u64 k_x87_fast_top = 0x8000000000000000ULL;
static const int k_x87_fast_relation_less = -1;
static const int k_x87_fast_relation_equal = 0;
static const int k_x87_fast_relation_greater = 1;

static inline u64 FastMultiply(u64 a, u64 b, u64& low)
{
#if defined(__SIZEOF_INT128__)
    unsigned __int128 product = (unsigned __int128)a * b;
    low = (u64)product;
    return (u64)(product >> 64);
#elif defined(_MSC_VER) && defined(_M_X64)
    return _umul128(a, b, &low);
#elif defined(_MSC_VER) && defined(_M_ARM64)
    low = a * b;
    return __umulh(a, b);
#else
    u64 a0 = (u32)a;
    u64 a1 = a >> 32;
    u64 b0 = (u32)b;
    u64 b1 = b >> 32;
    u64 p00 = a0 * b0;
    u64 p01 = a0 * b1;
    u64 p10 = a1 * b0;
    u64 p11 = a1 * b1;
    u64 middle = (p00 >> 32) + (u32)p01 + (u32)p10;
    low = (middle << 32) | (u32)p00;
    return p11 + (p01 >> 32) + (p10 >> 32) + (middle >> 32);
#endif
}

static inline int FastLeadingZeros(u64 value)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_clzll(value);
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    unsigned long index;
    _BitScanReverse64(&index, value);
    return 63 - (int)index;
#else
    int count = 0;

    while ((value & k_x87_fast_top) == 0)
    {
        value <<= 1;
        count++;
    }

    return count;
#endif
}

static inline bool FastPlain(const X87::X87_Register& value)
{
    u16 exp = value.sign_exponent & 0x7FFF;

    if (exp == 0)
        return value.significand == 0;

    return exp != 0x7FFF && (value.significand & k_x87_fast_top) != 0;
}

static inline void FastPack(X87::X87_Register& value, bool sign, s32 exp, u64 sig)
{
    value.significand = sig;
    value.sign_exponent = (u16)((sign ? 0x8000 : 0) | exp);
}

static inline int FastPrecision(u16 control)
{
    int precision = (control >> 8) & 3;
    return (precision == 3 || precision == 1) ? 80 : precision == 2 ? 64 : 32;
}

static bool FastRound(bool sign, s32 exp, u64 sig, u64 extra, int precision, int mode, X87::X87_Register& result, u32& flags)
{
    if ((u32)(exp - 1) >= 0x7FFD)
        return false;

    bool near_even = mode == 0;
    bool away = mode == (sign ? 1 : 2);

    if (precision != 80)
    {
        u64 mask = precision == 64 ? 0x7FFULL : 0xFFFFFFFFFFULL;
        u64 increment = precision == 64 ? 0x400ULL : 0x8000000000ULL;

        sig |= extra != 0 ? 1 : 0;

        if (!near_even)
            increment = away ? mask : 0;

        u64 bits = sig & mask;
        u64 exact = sig;
        sig += increment;

        if (sig < increment)
        {
            exp++;
            sig = k_x87_fast_top;
            exact >>= 1;
        }

        increment = mask + 1;

        if (near_even && (bits << 1) == increment)
            mask |= increment;

        sig &= ~mask;

        if (bits != 0)
        {
            flags |= k_x87_fast_inexact;

            if (sig > exact)
                flags |= k_x87_fast_round_up;
        }
    }
    else
    {
        bool increment = near_even ? extra >= k_x87_fast_top : (away && extra != 0);

        if (extra != 0)
            flags |= k_x87_fast_inexact;

        if (increment)
        {
            u64 exact = sig;
            sig++;

            if (sig == 0)
            {
                exp++;
                sig = k_x87_fast_top;
                exact >>= 1;
            }
            else if (near_even && (extra & ~k_x87_fast_top) == 0)
                sig &= ~1ULL;

            if (sig > exact)
                flags |= k_x87_fast_round_up;
        }
    }

    FastPack(result, sign, exp, sig);
    return true;
}

static inline void FastShiftJam(u64 sig, s32 dist, u64& value, u64& extra)
{
    if (dist < 64)
    {
        value = sig >> dist;
        extra = sig << (-dist & 63);
    }
    else
    {
        value = 0;
        extra = dist == 64 ? sig : (sig != 0 ? 1 : 0);
    }
}

static bool FastAddMagnitudes(const X87::X87_Register& a, const X87::X87_Register& b, bool sign, int precision,
    int mode, X87::X87_Register& result, u32& flags)
{
    s32 exp_a = a.sign_exponent & 0x7FFF;
    s32 exp_b = b.sign_exponent & 0x7FFF;
    u64 sig_a = a.significand;
    u64 sig_b = b.significand;

    if (exp_a == 0 && exp_b == 0)
    {
        FastPack(result, sign, 0, 0);
        return true;
    }

    if (exp_a == 0)
        return FastRound(sign, exp_b, sig_b, 0, precision, mode, result, flags);

    if (exp_b == 0)
        return FastRound(sign, exp_a, sig_a, 0, precision, mode, result, flags);

    s32 dist = exp_a - exp_b;
    s32 exp = exp_a;
    u64 extra = 0;
    u64 sig;

    if (dist == 0)
        sig = sig_a + sig_b;
    else
    {
        if (dist < 0)
        {
            exp = exp_b;
            FastShiftJam(sig_a, -dist, sig_a, extra);
        }
        else
            FastShiftJam(sig_b, dist, sig_b, extra);

        sig = sig_a + sig_b;

        if ((sig & k_x87_fast_top) != 0)
            return FastRound(sign, exp, sig, extra, precision, mode, result, flags);
    }

    extra = (sig << 63) | (extra != 0 ? 1 : 0);
    sig = (sig >> 1) | k_x87_fast_top;
    return FastRound(sign, exp + 1, sig, extra, precision, mode, result, flags);
}

static inline void FastShiftJam128(u64 sig, s32 dist, u64& high, u64& low)
{
    if (dist < 64)
    {
        high = sig >> dist;
        low = sig << (-dist & 63);
    }
    else
    {
        high = 0;
        low = dist < 127 ? (sig >> (dist & 63)) | ((sig & ((1ULL << (dist & 63)) - 1)) != 0 ? 1 : 0) :
            (sig != 0 ? 1 : 0);
    }
}

static bool FastSubtractMagnitudes(const X87::X87_Register& a, const X87::X87_Register& b, bool sign, int precision,
    int mode, X87::X87_Register& result, u32& flags)
{
    s32 exp_a = a.sign_exponent & 0x7FFF;
    s32 exp_b = b.sign_exponent & 0x7FFF;
    u64 sig_a = a.significand;
    u64 sig_b = b.significand;

    if (exp_a == 0 && exp_b == 0)
    {
        FastPack(result, mode == 1, 0, 0);
        return true;
    }

    if (exp_a == 0)
        return FastRound(!sign, exp_b, sig_b, 0, precision, mode, result, flags);

    if (exp_b == 0)
        return FastRound(sign, exp_a, sig_a, 0, precision, mode, result, flags);

    s32 dist = exp_a - exp_b;
    s32 exp = exp_a;
    u64 extra = 0;
    u64 high;
    u64 low;

    if (dist == 0 && sig_a == sig_b)
    {
        FastPack(result, mode == 1, 0, 0);
        return true;
    }

    if (dist < 0 || (dist == 0 && sig_a < sig_b))
    {
        if (dist < 0)
        {
            FastShiftJam128(sig_a, -dist, sig_a, extra);
            exp = exp_b;
        }

        sign = !sign;
        low = 0 - extra;
        high = sig_b - sig_a - (extra != 0 ? 1 : 0);
    }
    else
    {
        if (dist > 0)
            FastShiftJam128(sig_b, dist, sig_b, extra);

        low = 0 - extra;
        high = sig_a - sig_b - (extra != 0 ? 1 : 0);
    }

    if (high == 0)
    {
        exp -= 64;
        high = low;
        low = 0;
    }

    int shift = FastLeadingZeros(high);
    exp -= shift;

    if (shift != 0)
    {
        high = (high << shift) | (low >> (64 - shift));
        low <<= shift;
    }

    return FastRound(sign, exp, high, low, precision, mode, result, flags);
}

bool X87::FastOperate(int operation, const X87_Register& a, const X87_Register& b, X87_Register& result, u32& flags) const
{
    if (operation >= 6 || !FastPlain(a) || !FastPlain(b))
        return false;

    int precision = FastPrecision(m_state.control);
    int mode = (m_state.control >> 10) & 3;
    flags = 0;

    if (operation == 1)
    {
        bool sign = ((a.sign_exponent ^ b.sign_exponent) & 0x8000) != 0;
        s32 exp_a = a.sign_exponent & 0x7FFF;
        s32 exp_b = b.sign_exponent & 0x7FFF;

        if (exp_a == 0 || exp_b == 0)
        {
            FastPack(result, sign, 0, 0);
            return true;
        }

        u64 low;
        u64 high = FastMultiply(a.significand, b.significand, low);
        s32 exp = exp_a + exp_b - 0x3FFE;

        if ((high & k_x87_fast_top) == 0)
        {
            exp--;
            high = (high << 1) | (low >> 63);
            low <<= 1;
        }

        return FastRound(sign, exp, high, low, precision, mode, result, flags);
    }

    const X87_Register& x = operation == 5 ? b : a;
    const X87_Register& y = operation == 5 ? a : b;
    bool sign_x = (x.sign_exponent & 0x8000) != 0;
    bool sign_y = (y.sign_exponent & 0x8000) != 0;

    if ((operation == 0) == (sign_x == sign_y))
        return FastAddMagnitudes(x, y, sign_x, precision, mode, result, flags);

    return FastSubtractMagnitudes(x, y, sign_x, precision, mode, result, flags);
}

bool X87::FastCompare(const X87_Register& a, const X87_Register& b, int& relation)
{
    if (!FastPlain(a) || !FastPlain(b))
        return false;

    bool sign_a = (a.sign_exponent & 0x8000) != 0;
    bool sign_b = (b.sign_exponent & 0x8000) != 0;
    u16 exp_a = a.sign_exponent & 0x7FFF;
    u16 exp_b = b.sign_exponent & 0x7FFF;

    if (exp_a == 0 && exp_b == 0)
        relation = k_x87_fast_relation_equal;
    else if (exp_a == 0)
        relation = sign_b ? k_x87_fast_relation_greater : k_x87_fast_relation_less;
    else if (exp_b == 0)
        relation = sign_a ? k_x87_fast_relation_less : k_x87_fast_relation_greater;
    else if (sign_a != sign_b)
        relation = sign_a ? k_x87_fast_relation_less : k_x87_fast_relation_greater;
    else if (exp_a == exp_b && a.significand == b.significand)
        relation = k_x87_fast_relation_equal;
    else
    {
        bool smaller = exp_a != exp_b ? exp_a < exp_b : a.significand < b.significand;
        relation = smaller != sign_a ? k_x87_fast_relation_less : k_x87_fast_relation_greater;
    }

    return true;
}

bool X87::FastLoadReal(u32 size, const u8* operand, X87_Register& value)
{
    if (size == 4)
    {
        u32 bits = (u32)operand[0] | ((u32)operand[1] << 8) | ((u32)operand[2] << 16) | ((u32)operand[3] << 24);
        u32 exp = (bits >> 23) & 0xFF;
        bool sign = (bits >> 31) != 0;

        if (exp == 0xFF || (exp == 0 && (bits & 0x7FFFFF) != 0))
            return false;

        if (exp == 0)
            FastPack(value, sign, 0, 0);
        else
            FastPack(value, sign, (s32)exp + 0x3F80, (u64)((bits & 0x7FFFFF) | 0x800000) << 40);

        return true;
    }

    u64 bits = 0;

    for (int i = 7; i >= 0; i--)
        bits = (bits << 8) | operand[i];

    u32 exp = (u32)(bits >> 52) & 0x7FF;
    bool sign = (bits >> 63) != 0;

    if (exp == 0x7FF || (exp == 0 && (bits & 0xFFFFFFFFFFFFFULL) != 0))
        return false;

    if (exp == 0)
        FastPack(value, sign, 0, 0);
    else
        FastPack(value, sign, (s32)exp + 0x3C00, ((bits & 0xFFFFFFFFFFFFFULL) | 0x10000000000000ULL) << 11);

    return true;
}

void X87::FastLoadInteger(s32 value, X87_Register& result)
{
    if (value == 0)
    {
        FastPack(result, false, 0, 0);
        return;
    }

    u64 magnitude = value < 0 ? (u64)(-(s64)value) : (u64)value;
    int shift = FastLeadingZeros(magnitude);
    FastPack(result, value < 0, 0x403E - shift, magnitude << shift);
}

bool X87::FastStoreInteger(u32 size, s64& value, u32& flags) const
{
    const X87_Register& source = Read(0);

    if (size == 8 || !FastPlain(source))
        return false;

    bool sign = (source.sign_exponent & 0x8000) != 0;
    s32 exp = source.sign_exponent & 0x7FFF;
    s32 dist = 0x4032 - exp;
    u64 sig = source.significand;
    int mode = (m_state.control >> 10) & 3;

    if (dist <= 0)
        dist = 1;

    sig = dist < 63 ? (sig >> dist) | ((sig << (-dist & 63)) != 0 ? 1 : 0) : (sig != 0 ? 1 : 0);

    u32 increment = mode == 0 ? 0x800 : mode == (sign ? 1 : 2) ? 0xFFF : 0;
    u32 bits = (u32)sig & 0xFFF;
    u32 exact = (u32)(sig >> 12);
    sig += increment;

    if ((sig & 0xFFFFF00000000000ULL) != 0)
        return false;

    u32 sig32 = (u32)(sig >> 12);

    if (bits == 0x800 && mode == 0)
        sig32 &= ~1U;

    s32 result = (s32)(sign ? 0U - sig32 : sig32);

    if (result != 0 && ((result < 0) != sign))
        return false;

    if (size == 2 && (s32)(s16)result != result)
        return false;

    flags = 0;

    if (bits != 0)
    {
        flags |= k_x87_fast_inexact;

        if (sig32 > exact)
            flags |= k_x87_fast_round_up;
    }

    value = result;
    return true;
}

bool X87::FastStoreReal(u32 size, u64& value, u32& flags) const
{
    const X87_Register& source = Read(0);

    if (!FastPlain(source))
        return false;

    bool sign = (source.sign_exponent & 0x8000) != 0;
    s32 exp = source.sign_exponent & 0x7FFF;
    u64 sig = source.significand;
    int mode = (m_state.control >> 10) & 3;
    bool near_even = mode == 0;
    bool away = mode == (sign ? 1 : 2);
    flags = 0;

    if (size == 4)
    {
        u32 sig32 = (u32)(sig >> 33) | ((sig & 0x1FFFFFFFFULL) != 0 ? 1 : 0);

        if (exp == 0 && sig32 == 0)
        {
            value = sign ? 0x80000000U : 0;
            return true;
        }

        exp -= 0x3F81;

        if (exp < 0 || exp >= 0xFD)
            return false;

        u32 increment = near_even ? 0x40 : away ? 0x7F : 0;
        u32 bits = sig32 & 0x7F;
        u32 exact = sig32;
        sig32 = (sig32 + increment) >> 7;

        if (near_even && bits == 0x40)
            sig32 &= ~1U;

        if (bits != 0)
        {
            flags |= k_x87_fast_inexact;

            if ((sig32 << 7) > exact)
                flags |= k_x87_fast_round_up;
        }

        value = (sign ? 0x80000000U : 0) + ((u32)exp << 23) + sig32;
        return true;
    }

    if (exp == 0 && sig == 0)
    {
        value = sign ? k_x87_fast_top : 0;
        return true;
    }

    sig = (sig >> 1) | (sig & 1);
    exp -= 0x3C01;

    if (exp < 0 || exp >= 0x7FD)
        return false;

    u64 increment = near_even ? 0x200 : away ? 0x3FF : 0;
    u64 bits = sig & 0x3FF;
    u64 exact = sig;
    sig = (sig + increment) >> 10;

    if (near_even && bits == 0x200)
        sig &= ~1ULL;

    if (bits != 0)
    {
        flags |= k_x87_fast_inexact;

        if ((sig << 10) > exact)
            flags |= k_x87_fast_round_up;
    }

    value = (sign ? k_x87_fast_top : 0) + ((u64)exp << 52) + sig;
    return true;
}
