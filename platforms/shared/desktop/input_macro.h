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

#ifndef INPUT_MACRO_H
#define INPUT_MACRO_H

#include "geartowns.h"

#ifdef INPUT_MACRO_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

enum Input_Macro_Owner
{
    Input_Macro_Owner_None = 0,
    Input_Macro_Owner_GUI,
    Input_Macro_Owner_MCP
};

static const int k_input_macro_frames_per_key = 3;
static const int k_input_macro_max_paste = 4096;

EXTERN int input_macro_count_characters(const char* text);
EXTERN int input_macro_find_untypeable(const char* text, int* count);
EXTERN int input_macro_type_text(const char* text, int frames_per_key, Input_Macro_Owner owner);
EXTERN bool input_macro_update(Input_Macro_Owner owner);
EXTERN void input_macro_stop(void);
EXTERN Input_Macro_Owner input_macro_get_owner(void);
EXTERN u64 input_macro_get_frames_left(void);

#undef INPUT_MACRO_IMPORT
#undef EXTERN
#endif /* INPUT_MACRO_H */
