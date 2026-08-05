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

#define RUNAHEAD_IMPORT
#include "runahead.h"

void runahead_init(void)
{
}

void runahead_destroy(void)
{
}

int runahead_get_frames(void)
{
    return 0;
}

void runahead_run(int frames, u8* frame_buffer, s16* sample_buffer, int* sample_count)
{
    UNUSED(frames);
    UNUSED(frame_buffer);
    UNUSED(sample_buffer);
    if (sample_count)
        *sample_count = 0;
}
