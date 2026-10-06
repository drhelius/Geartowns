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

#ifndef GUI_FLOPPY_H
#define GUI_FLOPPY_H

#ifdef GUI_FLOPPY_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

EXTERN void gui_floppy_menu(int drive, const char* label, int drives);
EXTERN void gui_floppy_popups(void);
EXTERN void gui_floppy_insert(int drive, const char* path);
EXTERN void gui_floppy_dialog_insert(int drive, const char* path);
EXTERN void gui_floppy_dialog_save_as(int drive, const char* path);
EXTERN void gui_floppy_dialog_new_blank(int drive, const char* path);
EXTERN void gui_floppy_open_quit_confirmation(void);

#undef GUI_FLOPPY_IMPORT
#undef EXTERN
#endif /* GUI_FLOPPY_H */
