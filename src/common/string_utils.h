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
#include <stdarg.h>
#include <cctype>
#include <string>
#include <vector>
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

inline bool ends_with_no_case(const std::string& text, const char* suffix)
{
    return ends_with_no_case(text.c_str(), suffix);
}

inline char to_upper_ascii(char c)
{
    return ((c >= 'a') && (c <= 'z')) ? (char)(c - 32) : c;
}

inline char to_lower_ascii(char c)
{
    return ((c >= 'A') && (c <= 'Z')) ? (char)(c + 32) : c;
}

inline std::string to_upper_ascii(const std::string& text)
{
    std::string result(text);

    for (size_t i = 0; i < result.size(); i++)
        result[i] = to_upper_ascii(result[i]);

    return result;
}

inline std::string to_lower_ascii(const std::string& text)
{
    std::string result(text);

    for (size_t i = 0; i < result.size(); i++)
        result[i] = to_lower_ascii(result[i]);

    return result;
}

inline bool strings_equal_ignore_case(const char* left, const char* right)
{
    if (!left || !right)
        return false;

    while ((*left != 0) && (*right != 0))
    {
        if (to_upper_ascii(*left) != to_upper_ascii(*right))
            return false;

        left++;
        right++;
    }

    return (*left == 0) && (*right == 0);
}

inline bool starts_with_no_case(const std::string& text, const std::string& prefix)
{
    if (text.size() < prefix.size())
        return false;

    for (size_t i = 0; i < prefix.size(); i++)
    {
        if (to_upper_ascii(text[i]) != to_upper_ascii(prefix[i]))
            return false;
    }

    return true;
}

inline bool contains_no_case(const char* text, const char* filter)
{
    if (filter[0] == 0)
        return true;

    for (const char* start = text; *start != 0; start++)
    {
        const char* t = start;
        const char* f = filter;

        while ((*t != 0) && (*f != 0) && (to_upper_ascii(*t) == to_upper_ascii(*f)))
        {
            t++;
            f++;
        }

        if (*f == 0)
            return true;
    }

    return false;
}

inline int find_no_case(const std::vector<std::string>& list, const std::string& name)
{
    for (size_t i = 0; i < list.size(); i++)
    {
        if ((list[i].size() == name.size()) && strings_equal_ignore_case(list[i].c_str(), name.c_str()))
            return (int)i;
    }

    return -1;
}

inline std::string trim_chars(const std::string& text, const char* set = " \t\r\n")
{
    size_t start = text.find_first_not_of(set);

    if (start == std::string::npos)
        return "";

    size_t end = text.find_last_not_of(set);
    return text.substr(start, end - start + 1);
}

inline void split_string(const std::string& text, char separator, std::vector<std::string>* parts, bool keep_empty = true)
{
    size_t start = 0;
    parts->clear();

    while (start < text.size())
    {
        size_t end = text.find(separator, start);
        end = end == std::string::npos ? text.size() : end;

        if (keep_empty || (end > start))
            parts->push_back(text.substr(start, end - start));

        start = end + 1;
    }
}

inline void hex_encode(const u8* data, size_t size, std::string* out, bool upper = true, char separator = 0)
{
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    for (size_t i = 0; i < size; i++)
    {
        if ((separator != 0) && (i > 0))
            *out += separator;

        *out += digits[data[i] >> 4];
        *out += digits[data[i] & 0x0F];
    }
}

inline std::string hex_string(const u8* data, size_t size, bool upper = true, char separator = 0)
{
    std::string out;
    out.reserve(size * (separator != 0 ? 3 : 2));
    hex_encode(data, size, &out, upper, separator);
    return out;
}

inline size_t hex_encode(const u8* data, size_t size, char* buffer, size_t buffer_size, bool upper = true,
    char separator = 0)
{
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t length = 0;

    if (buffer_size == 0)
        return 0;

    for (size_t i = 0; i < size; i++)
    {
        size_t needed = ((separator != 0) && (i > 0)) ? 3 : 2;

        if (length + needed >= buffer_size)
            break;

        if (needed == 3)
            buffer[length++] = separator;

        buffer[length++] = digits[data[i] >> 4];
        buffer[length++] = digits[data[i] & 0x0F];
    }

    buffer[length] = 0;
    return length;
}

inline bool parse_hex_bytes(const char* text, size_t length, std::vector<u8>* out, const char* skip = NULL)
{
    int high = -1;
    out->clear();

    for (size_t i = 0; i < length; i++)
    {
        if (IsValidPointer(skip) && (text[i] != 0) && (strchr(skip, text[i]) != NULL))
            continue;

        if (!is_hex_digit(text[i]))
            return false;

        if (high < 0)
            high = as_hex(text[i]);
        else
        {
            out->push_back((u8)((high << 4) | as_hex(text[i])));
            high = -1;
        }
    }

    return high < 0;
}

inline bool parse_hex_bytes(const std::string& text, std::vector<u8>* out, const char* skip = NULL)
{
    return parse_hex_bytes(text.c_str(), text.size(), out, skip);
}

inline bool parse_hex_bytes(const char* text, size_t length, std::string* out, const char* skip = NULL)
{
    std::vector<u8> bytes;

    if (!parse_hex_bytes(text, length, &bytes, skip))
    {
        out->clear();
        return false;
    }

    out->assign(bytes.begin(), bytes.end());
    return true;
}

inline char printable_char(u8 c, char fallback = '.')
{
    return ((c >= 0x20) && (c <= 0x7E)) ? (char)c : fallback;
}

inline std::string vformat_string(const char* format, va_list args)
{
    char buffer[256];
    va_list copy;
    va_copy(copy, args);
    int length = vsnprintf(buffer, sizeof(buffer), format, copy);
    va_end(copy);

    if (length < 0)
        return "";

    if ((size_t)length < sizeof(buffer))
        return std::string(buffer, (size_t)length);

    std::vector<char> text((size_t)length + 1);
    va_copy(copy, args);
    vsnprintf(&text[0], text.size(), format, copy);
    va_end(copy);
    return std::string(&text[0], (size_t)length);
}

inline std::string format_string(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    std::string text = vformat_string(format, args);
    va_end(args);
    return text;
}

#endif /* STRING_UTILS_H */
