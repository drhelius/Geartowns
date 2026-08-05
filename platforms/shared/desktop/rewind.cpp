/*
 * Geartowns - FM Towns Emulator
 * Copyright (C) 2026  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#define REWIND_IMPORT
#include "rewind.h"

bool rewind_init(void) { return true; }
void rewind_destroy(void) {}
void rewind_reset(void) {}
void rewind_push(void) {}
bool rewind_pop(void) { return false; }
bool rewind_seek(int age) { UNUSED(age); return false; }
void rewind_commit_seek(void) {}
void rewind_set_active(bool a) { UNUSED(a); }
bool rewind_is_active(void) { return false; }
int rewind_get_snapshot_count(void) { return 0; }
int rewind_get_capacity(void) { return 0; }
int rewind_get_frames_per_snapshot(void) { return 1; }
size_t rewind_get_memory_usage(void) { return 0; }
