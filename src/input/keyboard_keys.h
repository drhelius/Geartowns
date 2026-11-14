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

#ifndef KEYBOARD_KEYS_H
#define KEYBOARD_KEYS_H

#include "../common/common.h"

struct GT_Key_Info
{
    GT_Keys key;
    const char* name;
    const char* label;
};

struct GT_Default_Key
{
    u16 usage;
    GT_Keys key;
};

static const GT_Key_Info k_key_infos[] =
{
    { GT_KEY_ESCAPE, "escape", "ESC" },
    { GT_KEY_1, "1", "1" }, { GT_KEY_2, "2", "2" }, { GT_KEY_3, "3", "3" }, { GT_KEY_4, "4", "4" },
    { GT_KEY_5, "5", "5" }, { GT_KEY_6, "6", "6" }, { GT_KEY_7, "7", "7" }, { GT_KEY_8, "8", "8" },
    { GT_KEY_9, "9", "9" }, { GT_KEY_0, "0", "0" },
    { GT_KEY_MINUS, "minus", "-" },
    { GT_KEY_CARET, "caret", "^" },
    { GT_KEY_YEN, "yen", "\x9D" },
    { GT_KEY_BACKSPACE, "backspace", "Backspace" },
    { GT_KEY_TAB, "tab", "TAB" },
    { GT_KEY_Q, "q", "Q" }, { GT_KEY_W, "w", "W" }, { GT_KEY_E, "e", "E" }, { GT_KEY_R, "r", "R" },
    { GT_KEY_T, "t", "T" }, { GT_KEY_Y, "y", "Y" }, { GT_KEY_U, "u", "U" }, { GT_KEY_I, "i", "I" },
    { GT_KEY_O, "o", "O" }, { GT_KEY_P, "p", "P" },
    { GT_KEY_AT, "at", "@" },
    { GT_KEY_LEFT_BRACKET, "left_bracket", "[" },
    { GT_KEY_RETURN, "return", "Return" },
    { GT_KEY_A, "a", "A" }, { GT_KEY_S, "s", "S" }, { GT_KEY_D, "d", "D" }, { GT_KEY_F, "f", "F" },
    { GT_KEY_G, "g", "G" }, { GT_KEY_H, "h", "H" }, { GT_KEY_J, "j", "J" }, { GT_KEY_K, "k", "K" },
    { GT_KEY_L, "l", "L" },
    { GT_KEY_SEMICOLON, "semicolon", ";" },
    { GT_KEY_COLON, "colon", ":" },
    { GT_KEY_RIGHT_BRACKET, "right_bracket", "]" },
    { GT_KEY_Z, "z", "Z" }, { GT_KEY_X, "x", "X" }, { GT_KEY_C, "c", "C" }, { GT_KEY_V, "v", "V" },
    { GT_KEY_B, "b", "B" }, { GT_KEY_N, "n", "N" }, { GT_KEY_M, "m", "M" },
    { GT_KEY_COMMA, "comma", "," },
    { GT_KEY_PERIOD, "period", "." },
    { GT_KEY_SLASH, "slash", "/" },
    { GT_KEY_UNDERSCORE, "underscore", "_" },
    { GT_KEY_SPACE, "space", "Space" },
    { GT_KEY_KP_MULTIPLY, "kp_multiply", "Keypad *" },
    { GT_KEY_KP_DIVIDE, "kp_divide", "Keypad /" },
    { GT_KEY_KP_PLUS, "kp_plus", "Keypad +" },
    { GT_KEY_KP_MINUS, "kp_minus", "Keypad -" },
    { GT_KEY_KP_7, "kp_7", "Keypad 7" }, { GT_KEY_KP_8, "kp_8", "Keypad 8" }, { GT_KEY_KP_9, "kp_9", "Keypad 9" },
    { GT_KEY_KP_EQUALS, "kp_equals", "Keypad =" },
    { GT_KEY_KP_4, "kp_4", "Keypad 4" }, { GT_KEY_KP_5, "kp_5", "Keypad 5" }, { GT_KEY_KP_6, "kp_6", "Keypad 6" },
    { GT_KEY_KP_1, "kp_1", "Keypad 1" }, { GT_KEY_KP_2, "kp_2", "Keypad 2" }, { GT_KEY_KP_3, "kp_3", "Keypad 3" },
    { GT_KEY_KP_ENTER, "kp_enter", "Keypad Enter" },
    { GT_KEY_KP_0, "kp_0", "Keypad 0" },
    { GT_KEY_KP_PERIOD, "kp_period", "Keypad ." },
    { GT_KEY_INSERT, "insert", "Insert" },
    { GT_KEY_KP_000, "kp_000", "Keypad 000" },
    { GT_KEY_DELETE, "delete", "Delete" },
    { GT_KEY_UP, "up", "Up" },
    { GT_KEY_HOME, "home", "Home" },
    { GT_KEY_LEFT, "left", "Left" },
    { GT_KEY_DOWN, "down", "Down" },
    { GT_KEY_RIGHT, "right", "Right" },
    { GT_KEY_CTRL, "ctrl", "CTRL" },
    { GT_KEY_SHIFT, "shift", "SHIFT" },
    { GT_KEY_CAPS, "caps", "CAP" },
    { GT_KEY_HIRAGANA, "hiragana", "Hiragana" },
    { GT_KEY_NO_CONVERT, "no_convert", "No Convert" },
    { GT_KEY_CONVERT, "convert", "Convert" },
    { GT_KEY_KANA_KANJI, "kana_kanji", "Kana Kanji" },
    { GT_KEY_KATAKANA, "katakana", "Katakana" },
    { GT_KEY_ALT, "alt", "ALT" },
    { GT_KEY_PF1, "pf1", "PF1" }, { GT_KEY_PF2, "pf2", "PF2" }, { GT_KEY_PF3, "pf3", "PF3" },
    { GT_KEY_PF4, "pf4", "PF4" }, { GT_KEY_PF5, "pf5", "PF5" }, { GT_KEY_PF6, "pf6", "PF6" },
    { GT_KEY_PF7, "pf7", "PF7" }, { GT_KEY_PF8, "pf8", "PF8" }, { GT_KEY_PF9, "pf9", "PF9" },
    { GT_KEY_PF10, "pf10", "PF10" }, { GT_KEY_PF11, "pf11", "PF11" }, { GT_KEY_PF12, "pf12", "PF12" },
    { GT_KEY_PF13, "pf13", "PF13" }, { GT_KEY_PF14, "pf14", "PF14" }, { GT_KEY_PF15, "pf15", "PF15" },
    { GT_KEY_PF16, "pf16", "PF16" }, { GT_KEY_PF17, "pf17", "PF17" }, { GT_KEY_PF18, "pf18", "PF18" },
    { GT_KEY_PF19, "pf19", "PF19" }, { GT_KEY_PF20, "pf20", "PF20" },
    { GT_KEY_KANJI_DICTIONARY, "kanji_dictionary", "Kanji Dictionary" },
    { GT_KEY_DELETE_WORD, "delete_word", "Delete Word" },
    { GT_KEY_ADD_WORD, "add_word", "Add Word" },
    { GT_KEY_PREVIOUS, "previous", "Previous Line" },
    { GT_KEY_NEXT, "next", "Next Line" },
    { GT_KEY_HALF_FULL, "half_full", "Half/Full Width" },
    { GT_KEY_CANCEL, "cancel", "Cancel" },
    { GT_KEY_EXECUTE, "execute", "Execute" },
    { GT_KEY_BREAK, "break", "BREAK" },
    { GT_KEY_COPY, "copy", "COPY" }
};

static const int k_key_info_count = (int)(sizeof(k_key_infos) / sizeof(k_key_infos[0]));

static const GT_Default_Key k_default_keys[] =
{
    { 0x04, GT_KEY_A }, { 0x05, GT_KEY_B }, { 0x06, GT_KEY_C }, { 0x07, GT_KEY_D }, { 0x08, GT_KEY_E },
    { 0x09, GT_KEY_F }, { 0x0A, GT_KEY_G }, { 0x0B, GT_KEY_H }, { 0x0C, GT_KEY_I }, { 0x0D, GT_KEY_J },
    { 0x0E, GT_KEY_K }, { 0x0F, GT_KEY_L }, { 0x10, GT_KEY_M }, { 0x11, GT_KEY_N }, { 0x12, GT_KEY_O },
    { 0x13, GT_KEY_P }, { 0x14, GT_KEY_Q }, { 0x15, GT_KEY_R }, { 0x16, GT_KEY_S }, { 0x17, GT_KEY_T },
    { 0x18, GT_KEY_U }, { 0x19, GT_KEY_V }, { 0x1A, GT_KEY_W }, { 0x1B, GT_KEY_X }, { 0x1C, GT_KEY_Y },
    { 0x1D, GT_KEY_Z },
    { 0x1E, GT_KEY_1 }, { 0x1F, GT_KEY_2 }, { 0x20, GT_KEY_3 }, { 0x21, GT_KEY_4 }, { 0x22, GT_KEY_5 },
    { 0x23, GT_KEY_6 }, { 0x24, GT_KEY_7 }, { 0x25, GT_KEY_8 }, { 0x26, GT_KEY_9 }, { 0x27, GT_KEY_0 },
    { 0x28, GT_KEY_RETURN },
    { 0x29, GT_KEY_BREAK },
    { 0x2A, GT_KEY_BACKSPACE },
    { 0x2B, GT_KEY_TAB },
    { 0x2C, GT_KEY_SPACE },
    { 0x2D, GT_KEY_MINUS },
    { 0x2E, GT_KEY_CARET },
    { 0x2F, GT_KEY_AT },
    { 0x30, GT_KEY_LEFT_BRACKET },
    { 0x31, GT_KEY_YEN },
    { 0x32, GT_KEY_RIGHT_BRACKET },
    { 0x33, GT_KEY_SEMICOLON },
    { 0x34, GT_KEY_COLON },
    { 0x35, GT_KEY_ESCAPE },
    { 0x36, GT_KEY_COMMA },
    { 0x37, GT_KEY_PERIOD },
    { 0x38, GT_KEY_SLASH },
    { 0x39, GT_KEY_CAPS },
    { 0x3A, GT_KEY_PF1 }, { 0x3B, GT_KEY_PF2 }, { 0x3C, GT_KEY_PF3 }, { 0x3D, GT_KEY_PF4 },
    { 0x3E, GT_KEY_PF5 }, { 0x3F, GT_KEY_PF6 }, { 0x40, GT_KEY_PF7 }, { 0x41, GT_KEY_PF8 },
    { 0x42, GT_KEY_PF9 }, { 0x43, GT_KEY_PF10 }, { 0x44, GT_KEY_PF11 }, { 0x45, GT_KEY_PF12 },
    { 0x46, GT_KEY_COPY },
    { 0x48, GT_KEY_BREAK },
    { 0x49, GT_KEY_INSERT },
    { 0x4A, GT_KEY_HOME },
    { 0x4B, GT_KEY_PREVIOUS },
    { 0x4C, GT_KEY_DELETE },
    { 0x4E, GT_KEY_NEXT },
    { 0x4F, GT_KEY_RIGHT },
    { 0x50, GT_KEY_LEFT },
    { 0x51, GT_KEY_DOWN },
    { 0x52, GT_KEY_UP },
    { 0x54, GT_KEY_KP_DIVIDE },
    { 0x55, GT_KEY_KP_MULTIPLY },
    { 0x56, GT_KEY_KP_MINUS },
    { 0x57, GT_KEY_KP_PLUS },
    { 0x58, GT_KEY_KP_ENTER },
    { 0x59, GT_KEY_KP_1 }, { 0x5A, GT_KEY_KP_2 }, { 0x5B, GT_KEY_KP_3 }, { 0x5C, GT_KEY_KP_4 },
    { 0x5D, GT_KEY_KP_5 }, { 0x5E, GT_KEY_KP_6 }, { 0x5F, GT_KEY_KP_7 }, { 0x60, GT_KEY_KP_8 },
    { 0x61, GT_KEY_KP_9 }, { 0x62, GT_KEY_KP_0 },
    { 0x63, GT_KEY_KP_PERIOD },
    { 0x64, GT_KEY_UNDERSCORE },
    { 0x65, GT_KEY_HIRAGANA },
    { 0x67, GT_KEY_KP_EQUALS },
    { 0x68, GT_KEY_PF13 }, { 0x69, GT_KEY_PF14 }, { 0x6A, GT_KEY_PF15 }, { 0x6B, GT_KEY_PF16 },
    { 0x6C, GT_KEY_PF17 }, { 0x6D, GT_KEY_PF18 }, { 0x6E, GT_KEY_PF19 }, { 0x6F, GT_KEY_PF20 },
    { 0x87, GT_KEY_UNDERSCORE },
    { 0x88, GT_KEY_KATAKANA },
    { 0x89, GT_KEY_YEN },
    { 0x8A, GT_KEY_CONVERT },
    { 0x8B, GT_KEY_NO_CONVERT },
    { 0x92, GT_KEY_KATAKANA },
    { 0x93, GT_KEY_HIRAGANA },
    { 0x94, GT_KEY_HALF_FULL },
    { 0xB1, GT_KEY_KP_000 },
    { 0xE0, GT_KEY_CTRL },
    { 0xE1, GT_KEY_SHIFT },
    { 0xE2, GT_KEY_ALT },
    { 0xE3, GT_KEY_NO_CONVERT },
    { 0xE4, GT_KEY_CTRL },
    { 0xE5, GT_KEY_SHIFT },
    { 0xE6, GT_KEY_KANA_KANJI },
    { 0xE7, GT_KEY_CONVERT }
};

static const int k_default_key_count = (int)(sizeof(k_default_keys) / sizeof(k_default_keys[0]));

static const GT_Keys k_modifier_keys[] =
{
    GT_KEY_CTRL, GT_KEY_SHIFT
};

static const int k_modifier_key_count = (int)(sizeof(k_modifier_keys) / sizeof(k_modifier_keys[0]));

#endif /* KEYBOARD_KEYS_H */
