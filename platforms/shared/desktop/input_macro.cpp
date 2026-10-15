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

#include <algorithm>
#include <ctype.h>
#include <string.h>
#include <vector>
#include "emu.h"

#define INPUT_MACRO_IMPORT
#include "input_macro.h"

enum Input_Macro_Step_Type
{
    Input_Macro_Step_Press,
    Input_Macro_Step_Release,
    Input_Macro_Step_Wait
};

struct Input_Macro_Step
{
    Input_Macro_Step_Type type;
    GT_Keys key;
    int frames;
};

static std::vector<Input_Macro_Step> macro_steps;
static std::vector<GT_Keys> macro_held_keys;
static size_t macro_step_index = 0;
static bool macro_waiting = false;
static u64 macro_wait_target_frame = 0;
static Input_Macro_Owner macro_owner = Input_Macro_Owner_None;

static int typed_key(const char* text, GT_Keys* key, bool* shift);
static void ascii_key(char character, GT_Keys* key, bool* shift);
static void add_step(Input_Macro_Step_Type type, GT_Keys key, int frames);
static void reset(void);

int input_macro_count_characters(const char* text)
{
    int count = 0;

    for (const u8* p = (const u8*)text; *p != 0; p++)
    {
        if ((*p & 0xC0) != 0x80)
            count++;
    }

    return count;
}

int input_macro_find_untypeable(const char* text, int* count)
{
    int first = -1;
    int untypeable = 0;
    int index = 0;

    while (*text != 0)
    {
        GT_Keys key = GT_KEY_NONE;
        bool shift = false;
        text += typed_key(text, &key, &shift);

        if (key == GT_KEY_NONE)
        {
            if (first < 0)
                first = index;

            untypeable++;
        }

        index++;
    }

    if (IsValidPointer(count))
        *count = untypeable;

    return first;
}

int input_macro_type_text(const char* text, int frames_per_key, Input_Macro_Owner owner)
{
    input_macro_stop();

    int typed = 0;

    while (*text != 0)
    {
        GT_Keys key = GT_KEY_NONE;
        bool shift = false;
        text += typed_key(text, &key, &shift);

        if (key == GT_KEY_NONE)
            continue;

        if (shift)
            add_step(Input_Macro_Step_Press, GT_KEY_SHIFT, 0);

        add_step(Input_Macro_Step_Press, key, 0);
        add_step(Input_Macro_Step_Wait, GT_KEY_NONE, frames_per_key);
        add_step(Input_Macro_Step_Release, key, 0);

        if (shift)
            add_step(Input_Macro_Step_Release, GT_KEY_SHIFT, 0);

        add_step(Input_Macro_Step_Wait, GT_KEY_NONE, frames_per_key);
        typed++;
    }

    if (typed > 0)
        macro_owner = owner;
    else
        reset();

    return typed;
}

bool input_macro_update(Input_Macro_Owner owner)
{
    if (owner == Input_Macro_Owner_None || owner != macro_owner)
        return false;

    if (owner == Input_Macro_Owner_GUI && emu_is_empty())
    {
        input_macro_stop();
        return false;
    }

    if (macro_waiting)
    {
        if (emu_frame_counter < macro_wait_target_frame)
            return true;

        macro_waiting = false;
    }

    while (macro_step_index < macro_steps.size())
    {
        const Input_Macro_Step& step = macro_steps[macro_step_index];
        macro_step_index++;

        if (step.type == Input_Macro_Step_Wait)
        {
            macro_wait_target_frame = emu_frame_counter + (u64)step.frames;
            macro_waiting = true;
            return true;
        }

        if (step.type == Input_Macro_Step_Press)
        {
            emu_key_pressed(step.key);
            macro_held_keys.push_back(step.key);
        }
        else
        {
            emu_key_released(step.key);
            macro_held_keys.erase(std::remove(macro_held_keys.begin(), macro_held_keys.end(), step.key),
                macro_held_keys.end());
        }
    }

    reset();
    return false;
}

void input_macro_stop(void)
{
    for (size_t i = 0; i < macro_held_keys.size(); i++)
        emu_key_released(macro_held_keys[i]);

    reset();
}

Input_Macro_Owner input_macro_get_owner(void)
{
    return macro_owner;
}

u64 input_macro_get_frames_left(void)
{
    if (!macro_waiting || emu_frame_counter >= macro_wait_target_frame)
        return 0;

    return macro_wait_target_frame - emu_frame_counter;
}

static int typed_key(const char* text, GT_Keys* key, bool* shift)
{
    const u8* bytes = (const u8*)text;
    *key = GT_KEY_NONE;
    *shift = false;

    if (bytes[0] >= 0x80)
    {
        int size = 1;

        while ((bytes[size] & 0xC0) == 0x80)
            size++;

        if (size == 2 && bytes[0] == 0xC2 && bytes[1] == 0xA5)
            *key = GT_KEY_YEN;

        return size;
    }

    if (bytes[0] == '\r' && bytes[1] == '\n')
    {
        *key = GT_KEY_RETURN;
        return 2;
    }

    ascii_key(text[0], key, shift);
    return 1;
}

static void ascii_key(char character, GT_Keys* key, bool* shift)
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

    *shift = false;

    if (character >= '1' && character <= '9')
    {
        *key = (GT_Keys)(GT_KEY_1 + (character - '1'));
        return;
    }

    if (character == '0')
    {
        *key = GT_KEY_0;
        return;
    }

    if (character == '\n' || character == '\r')
    {
        *key = GT_KEY_RETURN;
        return;
    }

    if (character == '\t' || character == '\b')
    {
        *key = character == '\t' ? GT_KEY_TAB : GT_KEY_BACKSPACE;
        return;
    }

    for (int row = 0; row < 3; row++)
    {
        const char* found = strchr(k_rows[row], toupper((unsigned char)character));

        if (isalpha((unsigned char)character) && IsValidPointer(found))
        {
            *key = (GT_Keys)(k_row_start[row] + (found - k_rows[row]));
            *shift = isupper((unsigned char)character) != 0;
            return;
        }
    }

    const char* digit = strchr(k_shifted_digits, character);

    if (character != 0 && IsValidPointer(digit))
    {
        *key = (GT_Keys)(GT_KEY_1 + (digit - k_shifted_digits));
        *shift = true;
        return;
    }

    const char* symbol = strchr(k_symbols, character);

    if (character != 0 && IsValidPointer(symbol))
    {
        *key = k_symbol_keys[symbol - k_symbols];
        return;
    }

    const char* shifted = strchr(k_shifted_symbols, character);

    if (character != 0 && IsValidPointer(shifted))
    {
        *key = k_shifted_symbol_keys[shifted - k_shifted_symbols];
        *shift = true;
    }
}

static void add_step(Input_Macro_Step_Type type, GT_Keys key, int frames)
{
    Input_Macro_Step step;
    step.type = type;
    step.key = key;
    step.frames = frames;
    macro_steps.push_back(step);
}

static void reset(void)
{
    macro_steps.clear();
    macro_held_keys.clear();
    macro_step_index = 0;
    macro_waiting = false;
    macro_wait_target_frame = 0;
    macro_owner = Input_Macro_Owner_None;
}
