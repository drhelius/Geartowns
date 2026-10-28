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

#ifndef GUI_MEMORY_CARD_H
#define GUI_MEMORY_CARD_H

#ifdef GUI_MEMORY_CARD_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

EXTERN void gui_memory_card_menu(void);
EXTERN void gui_memory_card_popups(void);
EXTERN void gui_memory_card_update(void);
EXTERN bool gui_memory_card_load(const char* path);
EXTERN bool gui_memory_card_insert(const char* path);
EXTERN void gui_memory_card_dialog_insert(const char* path);
EXTERN void gui_memory_card_dialog_create(const char* path);
EXTERN void gui_memory_card_open_quit_confirmation(void);

#undef GUI_MEMORY_CARD_IMPORT
#undef EXTERN
#endif /* GUI_MEMORY_CARD_H */
