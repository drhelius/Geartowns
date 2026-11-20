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

#ifndef KEYBOARD_CHARS_H
#define KEYBOARD_CHARS_H

#include "../common/common.h"

static inline bool keyboard_char_key(u32 character, GT_Keys* key, bool* shift)
{
    static const char* k_rows[3] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
    static const GT_Keys k_row_start[3] = { GT_KEY_Q, GT_KEY_A, GT_KEY_Z };
    static const char* k_shifted_digits = "!\"#$%&'()";
    static const char k_symbols[] = "-^\\@[;:],./ ";
    static const GT_Keys k_symbol_keys[] =
    {
        GT_KEY_MINUS, GT_KEY_CARET, GT_KEY_YEN, GT_KEY_AT, GT_KEY_LEFT_BRACKET, GT_KEY_SEMICOLON, GT_KEY_COLON,
        GT_KEY_RIGHT_BRACKET, GT_KEY_COMMA, GT_KEY_PERIOD, GT_KEY_SLASH, GT_KEY_SPACE
    };
    static const char k_shifted_symbols[] = "=~|`{+*}<>?_";
    static const GT_Keys k_shifted_symbol_keys[] =
    {
        GT_KEY_MINUS, GT_KEY_CARET, GT_KEY_YEN, GT_KEY_AT, GT_KEY_LEFT_BRACKET, GT_KEY_SEMICOLON, GT_KEY_COLON,
        GT_KEY_RIGHT_BRACKET, GT_KEY_COMMA, GT_KEY_PERIOD, GT_KEY_SLASH, GT_KEY_UNDERSCORE
    };

    *key = GT_KEY_NONE;
    *shift = false;

    if (character == 0xA5)
    {
        *key = GT_KEY_YEN;
        return true;
    }

    if ((character == 0) || (character >= 0x80))
        return false;

    char c = (char)character;

    if (c >= '1' && c <= '9')
        *key = (GT_Keys)(GT_KEY_1 + (c - '1'));
    else if (c == '0')
        *key = GT_KEY_0;
    else if (c == '\n' || c == '\r')
        *key = GT_KEY_RETURN;
    else if (c == '\t' || c == '\b')
        *key = c == '\t' ? GT_KEY_TAB : GT_KEY_BACKSPACE;
    else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
    {
        char upper = (c >= 'a') ? (char)(c - 32) : c;

        for (int row = 0; row < 3; row++)
        {
            const char* found = strchr(k_rows[row], upper);

            if (IsValidPointer(found))
                *key = (GT_Keys)(k_row_start[row] + (found - k_rows[row]));
        }

        *shift = c < 'a';
    }
    else
    {
        const char* digit = strchr(k_shifted_digits, c);
        const char* symbol = strchr(k_symbols, c);
        const char* shifted = strchr(k_shifted_symbols, c);

        if (IsValidPointer(digit))
        {
            *key = (GT_Keys)(GT_KEY_1 + (digit - k_shifted_digits));
            *shift = true;
        }
        else if (IsValidPointer(symbol))
            *key = k_symbol_keys[symbol - k_symbols];
        else if (IsValidPointer(shifted))
        {
            *key = k_shifted_symbol_keys[shifted - k_shifted_symbols];
            *shift = true;
        }
    }

    return *key != GT_KEY_NONE;
}

#endif /* KEYBOARD_CHARS_H */
