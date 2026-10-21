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

#ifndef GUI_HARD_DISKS_H
#define GUI_HARD_DISKS_H

#ifdef GUI_HARD_DISKS_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

EXTERN void gui_hard_disks_menu(void);
EXTERN void gui_hard_disks_popups(void);
EXTERN void gui_hard_disks_update(void);
EXTERN bool gui_hard_disks_load(const char* path);
EXTERN bool gui_hard_disks_is_pending(void);
EXTERN void gui_hard_disks_dialog_attach(int id, const char* path);
EXTERN void gui_hard_disks_dialog_create(int id, const char* path);
EXTERN void gui_hard_disks_open_quit_confirmation(void);

#undef GUI_HARD_DISKS_IMPORT
#undef EXTERN
#endif /* GUI_HARD_DISKS_H */
