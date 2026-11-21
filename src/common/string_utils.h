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

#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stdio.h>
#include <string.h>
#include <cctype>
#include <string>
#include "defines.h"
#include "types.h"

inline u32 utf8_decode_next(const char* text, size_t length, size_t& index)
{
    const u8* data = (const u8*)text;
    u32 codepoint = data[index++];

    if (codepoint < 0x80)
        return codepoint;

    if (((codepoint & 0xE0) == 0xC0) && (index < length) && ((data[index] & 0xC0) == 0x80))
    {
        codepoint = ((codepoint & 0x1F) << 6) | (data[index++] & 0x3F);
        return codepoint >= 0x80 ? codepoint : 0xFFFD;
    }

    if (((codepoint & 0xF0) == 0xE0) && ((index + 1) < length) && ((data[index] & 0xC0) == 0x80) &&
        ((data[index + 1] & 0xC0) == 0x80))
    {
        codepoint = ((codepoint & 0x0F) << 12) | ((data[index] & 0x3F) << 6) | (data[index + 1] & 0x3F);
        index += 2;

        if ((codepoint >= 0x800) && ((codepoint < 0xD800) || (codepoint > 0xDFFF)))
            return codepoint;

        return 0xFFFD;
    }

    if (((codepoint & 0xF8) == 0xF0) && ((index + 2) < length) && ((data[index] & 0xC0) == 0x80) &&
        ((data[index + 1] & 0xC0) == 0x80) && ((data[index + 2] & 0xC0) == 0x80))
    {
        codepoint = ((codepoint & 0x07) << 18) | ((data[index] & 0x3F) << 12) | ((data[index + 1] & 0x3F) << 6) |
            (data[index + 2] & 0x3F);
        index += 3;
        return ((codepoint >= 0x10000) && (codepoint <= 0x10FFFF)) ? codepoint : 0xFFFD;
    }

    return 0xFFFD;
}

inline int as_hex(const char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 0xA;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 0xA;
    return 0;
}

inline bool is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

template<typename T>
inline bool parse_hex_string(const char* str, size_t len, T* result, size_t max_digits = sizeof(T) * 2)
{
    if (len == 0)
        return false;

    while (len > 1 && str[0] == '0')
    {
        str++;
        len--;
    }

    if (len > max_digits)
        return false;

    *result = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (!is_hex_digit(str[i]))
            return false;

        *result = (*result << 4);

        if (str[i] >= '0' && str[i] <= '9')
            *result |= (str[i] - '0');
        else if (str[i] >= 'a' && str[i] <= 'f')
            *result |= (str[i] - 'a' + 10);
        else // (str[i] >= 'A' && str[i] <= 'F')
            *result |= (str[i] - 'A' + 10);
    }
    return true;
}

inline bool parse_hex_string(const char* str, size_t len, u8* result)
{
    return parse_hex_string<u8>(str, len, result, 2);
}

inline bool parse_hex_string(const char* str, size_t len, u16* result)
{
    return parse_hex_string<u16>(str, len, result, 4);
}

inline bool parse_hex_string(const char* str, size_t len, u32* result)
{
    return parse_hex_string<u32>(str, len, result, 8);
}

template<typename T>
inline bool parse_hex_with_prefix(const std::string& hex_str, T* result)
{
    const char* str = hex_str.c_str();
    size_t len = hex_str.length();

    if (len >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        str += 2;
        len -= 2;
    }
    else if (len >= 1 && str[0] == '$')
    {
        str += 1;
        len -= 1;
    }

    return parse_hex_string(str, len, result);
}

inline std::string int_to_string(int value)
{
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return buffer;
}

inline char* strncpy_fit(char* dest, const char* src, size_t dest_size)
{
    if (dest_size == 0)
        return dest;

    size_t copy_size = strlen(src);
    if (copy_size >= dest_size)
        copy_size = dest_size - 1;

    memcpy(dest, src, copy_size);
    dest[copy_size] = '\0';

    return dest;
}

inline char* strncat_fit(char* dest, const char* src, size_t dest_size)
{
    if (dest_size == 0)
        return dest;

    size_t len = strlen(dest);
    if (len + 1 >= dest_size)
        return dest;

    return strncat(dest, src, dest_size - len - 1);
}

inline bool strings_equal_ignore_case(const std::string& left, const std::string& right)
{
    if (left.size() != right.size())
        return false;

    for (size_t i = 0; i < left.size(); i++)
    {
        unsigned char left_char = (unsigned char)left[i];
        unsigned char right_char = (unsigned char)right[i];
        if (std::tolower(left_char) != std::tolower(right_char))
            return false;
    }

    return true;
}

inline int ends_with(const char* s, const char* suffix)
{
    size_t sl = strlen(s);
    size_t su = strlen(suffix);

    if (sl < su)
    {
        return 0;
    }

    return (memcmp(s + (sl - su), suffix, su) == 0);
}

inline bool ends_with_no_case(const char* text, const char* suffix)
{
    if (!text || !suffix)
        return false;

    size_t text_length = strlen(text);
    size_t suffix_length = strlen(suffix);
    if (text_length < suffix_length)
        return false;

    const char* start = text + text_length - suffix_length;
    for (size_t i = 0; i < suffix_length; i++)
    {
        char a = start[i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z')
            a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z')
            b = (char)(b - 'A' + 'a');
        if (a != b)
            return false;
    }

    return true;
}

#endif /* STRING_UTILS_H */
