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

#ifndef REWIND_H
#define REWIND_H

#include "geartowns.h"

#ifdef REWIND_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

#define REWIND_KEYFRAME_STATES          4
#define REWIND_DIFF_CHUNK               256
#define REWIND_ENCODE_INITIAL_SIZE      (1024 * 1024)
#define REWIND_INITIAL_SLOTS            256
#define REWIND_MEMORY_STEP_MB           128
#define REWIND_MAX_FRAMES_PER_SNAPSHOT  60
#define REWIND_SCREENSHOT_WIDTH         GT_MAX_FRAME_BUFFER_WIDTH
#define REWIND_SCREENSHOT_HEIGHT        GT_MAX_FRAME_BUFFER_HEIGHT
#define REWIND_CLOCK_RATE               GT_MACHINE_CLOCK_RATE

EXTERN bool rewind_init(void);
EXTERN void rewind_destroy(void);
EXTERN void rewind_reset(void);
EXTERN void rewind_push(bool frame_completed);
EXTERN bool rewind_pop(double seconds);
EXTERN bool rewind_seek(int age);
EXTERN void rewind_commit_seek(void);
EXTERN void rewind_set_active(bool a);
EXTERN bool rewind_is_active(void);
EXTERN int rewind_get_snapshot_count(void);
EXTERN int rewind_get_seek_age(void);
EXTERN int rewind_get_frames_per_snapshot(void);
EXTERN double rewind_get_recorded_seconds(void);
EXTERN double rewind_get_age_seconds(int age);
EXTERN size_t rewind_get_memory_usage(void);
EXTERN size_t rewind_get_memory_budget(void);
EXTERN int rewind_get_max_memory_mb(void);
EXTERN int rewind_get_required_memory_mb(void);

#undef REWIND_IMPORT
#undef EXTERN
#endif /* REWIND_H */
