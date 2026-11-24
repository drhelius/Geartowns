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

#ifndef GUI_GAME_H
#define GUI_GAME_H

#ifdef GUI_GAME_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

EXTERN bool gui_push_game_source_color(int setting);
EXTERN void gui_draw_game_source_tooltip(int setting);
EXTERN bool gui_begin_game_menu(const char* label, int setting);
EXTERN bool gui_begin_game_combo(const char* id, const char* preview, int setting);
EXTERN void gui_game_keep_disk_item(int disk);
EXTERN void gui_game_forget_item(void);

#undef GUI_GAME_IMPORT
#undef EXTERN
#endif /* GUI_GAME_H */
