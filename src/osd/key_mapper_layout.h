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

#ifndef KEY_MAPPER_LAYOUT_H
#define KEY_MAPPER_LAYOUT_H

#include "../common/common.h"

struct GT_Key_Mapper_Key
{
    u8 x;
    u8 y;
    u8 width;
    u8 height;
    GT_Keys key;
    const char* label;
};

static const int k_key_mapper_width = 640;
static const int k_key_mapper_height = 480;
static const int k_key_mapper_display_width = 640;
static const int k_key_mapper_display_height = 480;
static const int k_key_mapper_grid = 8;
static const int k_key_mapper_unit_x = 26;
static const int k_key_mapper_unit_y = 26;
static const int k_key_mapper_origin_x = 10;
static const int k_key_mapper_origin_y = 32;

static const GT_Key_Mapper_Key k_key_mapper_layout[] =
{
    { 0, 0, 8, 8, GT_KEY_BREAK, "Brk" },
    { 8, 0, 8, 8, GT_KEY_COPY, "Cpy" },
    { 20, 0, 7, 8, GT_KEY_PF1, "PF\n1" },
    { 27, 0, 7, 8, GT_KEY_PF2, "PF\n2" },
    { 34, 0, 7, 8, GT_KEY_PF3, "PF\n3" },
    { 41, 0, 7, 8, GT_KEY_PF4, "PF\n4" },
    { 48, 0, 7, 8, GT_KEY_PF5, "PF\n5" },
    { 64, 0, 7, 8, GT_KEY_PF6, "PF\n6" },
    { 71, 0, 7, 8, GT_KEY_PF7, "PF\n7" },
    { 78, 0, 7, 8, GT_KEY_PF8, "PF\n8" },
    { 85, 0, 7, 8, GT_KEY_PF9, "PF\n9" },
    { 92, 0, 7, 8, GT_KEY_PF10, "PF\n10" },
    { 107, 0, 7, 8, GT_KEY_PF11, "PF\n11" },
    { 114, 0, 7, 8, GT_KEY_PF12, "PF\n12" },
    { 128, 0, 8, 8, GT_KEY_KANJI_DICTIONARY, "Kan\nDic" },
    { 136, 0, 8, 8, GT_KEY_DELETE_WORD, "Del\nWrd" },
    { 144, 0, 8, 8, GT_KEY_ADD_WORD, "Add\nWrd" },
    { 159, 0, 8, 8, GT_KEY_PF13, "PF\n13" },
    { 167, 0, 8, 8, GT_KEY_PF14, "PF\n14" },
    { 175, 0, 8, 8, GT_KEY_PF15, "PF\n15" },
    { 183, 0, 8, 8, GT_KEY_PF16, "PF\n16" },
    { 128, 8, 8, 8, GT_KEY_PREVIOUS, "Prv" },
    { 136, 8, 8, 8, GT_KEY_HOME, "Hom" },
    { 144, 8, 8, 8, GT_KEY_NEXT, "Nxt" },
    { 159, 8, 8, 8, GT_KEY_PF17, "PF\n17" },
    { 167, 8, 8, 8, GT_KEY_PF18, "PF\n18" },
    { 175, 8, 8, 8, GT_KEY_PF19, "PF\n19" },
    { 183, 8, 8, 8, GT_KEY_PF20, "PF\n20" },
    { 0, 24, 12, 8, GT_KEY_ESCAPE, "ESC" },
    { 12, 24, 8, 8, GT_KEY_1, "1" },
    { 20, 24, 8, 8, GT_KEY_2, "2" },
    { 28, 24, 8, 8, GT_KEY_3, "3" },
    { 36, 24, 8, 8, GT_KEY_4, "4" },
    { 44, 24, 8, 8, GT_KEY_5, "5" },
    { 52, 24, 8, 8, GT_KEY_6, "6" },
    { 60, 24, 8, 8, GT_KEY_7, "7" },
    { 68, 24, 8, 8, GT_KEY_8, "8" },
    { 76, 24, 8, 8, GT_KEY_9, "9" },
    { 84, 24, 8, 8, GT_KEY_0, "0" },
    { 92, 24, 8, 8, GT_KEY_MINUS, "-" },
    { 100, 24, 8, 8, GT_KEY_CARET, "^" },
    { 108, 24, 8, 8, GT_KEY_YEN, "\x9D" },
    { 116, 24, 8, 8, GT_KEY_BACKSPACE, "BS" },
    { 128, 24, 8, 8, GT_KEY_HALF_FULL, "Han\nZen" },
    { 136, 24, 8, 8, GT_KEY_DELETE, "Del" },
    { 144, 24, 8, 8, GT_KEY_INSERT, "Ins" },
    { 0, 32, 17, 8, GT_KEY_TAB, "TAB" },
    { 17, 32, 8, 8, GT_KEY_Q, "Q" },
    { 25, 32, 8, 8, GT_KEY_W, "W" },
    { 33, 32, 8, 8, GT_KEY_E, "E" },
    { 41, 32, 8, 8, GT_KEY_R, "R" },
    { 49, 32, 8, 8, GT_KEY_T, "T" },
    { 57, 32, 8, 8, GT_KEY_Y, "Y" },
    { 65, 32, 8, 8, GT_KEY_U, "U" },
    { 73, 32, 8, 8, GT_KEY_I, "I" },
    { 81, 32, 8, 8, GT_KEY_O, "O" },
    { 89, 32, 8, 8, GT_KEY_P, "P" },
    { 97, 32, 8, 8, GT_KEY_AT, "@" },
    { 105, 32, 8, 8, GT_KEY_LEFT_BRACKET, "[" },
    { 115, 32, 9, 16, GT_KEY_RETURN, "Ret" },
    { 159, 32, 8, 8, GT_KEY_KP_MULTIPLY, "*" },
    { 167, 32, 8, 8, GT_KEY_KP_DIVIDE, "/" },
    { 175, 32, 8, 8, GT_KEY_KP_PLUS, "+" },
    { 183, 32, 8, 8, GT_KEY_KP_MINUS, "-" },
    { 0, 40, 19, 8, GT_KEY_CTRL, "CTRL" },
    { 19, 40, 8, 8, GT_KEY_A, "A" },
    { 27, 40, 8, 8, GT_KEY_S, "S" },
    { 35, 40, 8, 8, GT_KEY_D, "D" },
    { 43, 40, 8, 8, GT_KEY_F, "F" },
    { 51, 40, 8, 8, GT_KEY_G, "G" },
    { 59, 40, 8, 8, GT_KEY_H, "H" },
    { 67, 40, 8, 8, GT_KEY_J, "J" },
    { 75, 40, 8, 8, GT_KEY_K, "K" },
    { 83, 40, 8, 8, GT_KEY_L, "L" },
    { 91, 40, 8, 8, GT_KEY_SEMICOLON, ";" },
    { 99, 40, 8, 8, GT_KEY_COLON, ":" },
    { 107, 40, 8, 8, GT_KEY_RIGHT_BRACKET, "]" },
    { 159, 40, 8, 8, GT_KEY_KP_7, "7" },
    { 167, 40, 8, 8, GT_KEY_KP_8, "8" },
    { 175, 40, 8, 8, GT_KEY_KP_9, "9" },
    { 183, 40, 8, 8, GT_KEY_KP_EQUALS, "=" },
    { 0, 48, 24, 8, GT_KEY_SHIFT, "SHIFT" },
    { 24, 48, 8, 8, GT_KEY_Z, "Z" },
    { 32, 48, 8, 8, GT_KEY_X, "X" },
    { 40, 48, 8, 8, GT_KEY_C, "C" },
    { 48, 48, 8, 8, GT_KEY_V, "V" },
    { 56, 48, 8, 8, GT_KEY_B, "B" },
    { 64, 48, 8, 8, GT_KEY_N, "N" },
    { 72, 48, 8, 8, GT_KEY_M, "M" },
    { 80, 48, 8, 8, GT_KEY_COMMA, "," },
    { 88, 48, 8, 8, GT_KEY_PERIOD, "." },
    { 96, 48, 8, 8, GT_KEY_SLASH, "/" },
    { 104, 48, 8, 8, GT_KEY_UNDERSCORE, "_" },
    { 112, 48, 12, 8, GT_KEY_SHIFT, "SHF" },
    { 136, 48, 8, 8, GT_KEY_UP, "\x18" },
    { 128, 52, 8, 8, GT_KEY_LEFT, "\x1B" },
    { 144, 52, 8, 8, GT_KEY_RIGHT, "\x1A" },
    { 136, 56, 8, 8, GT_KEY_DOWN, "\x19" },
    { 159, 48, 8, 8, GT_KEY_KP_4, "4" },
    { 167, 48, 8, 8, GT_KEY_KP_5, "5" },
    { 175, 48, 8, 8, GT_KEY_KP_6, "6" },
    { 183, 48, 8, 8, GT_KEY_KP_PERIOD, "." },
    { 0, 56, 16, 8, GT_KEY_HIRAGANA, "Hira" },
    { 16, 56, 24, 8, GT_KEY_CAPS, "CAP" },
    { 40, 56, 44, 8, GT_KEY_SPACE, "Space" },
    { 84, 56, 16, 8, GT_KEY_KANA_KANJI, "Kana\nKanji" },
    { 100, 56, 16, 8, GT_KEY_KATAKANA, "Kata" },
    { 116, 56, 8, 8, GT_KEY_CANCEL, "Can" },
    { 159, 56, 8, 8, GT_KEY_KP_1, "1" },
    { 167, 56, 8, 8, GT_KEY_KP_2, "2" },
    { 175, 56, 8, 8, GT_KEY_KP_3, "3" },
    { 183, 56, 8, 16, GT_KEY_KP_ENTER, "Ent" },
    { 43, 64, 12, 8, GT_KEY_NO_CONVERT, "No\nCnv" },
    { 55, 64, 12, 8, GT_KEY_CONVERT, "Cnv" },
    { 128, 64, 24, 8, GT_KEY_EXECUTE, "Execute" },
    { 159, 64, 16, 8, GT_KEY_KP_0, "0" },
    { 175, 64, 8, 8, GT_KEY_KP_000, "000" }
};

static const int k_key_mapper_key_count = (int)(sizeof(k_key_mapper_layout) / sizeof(k_key_mapper_layout[0]));

#endif /* KEY_MAPPER_LAYOUT_H */
