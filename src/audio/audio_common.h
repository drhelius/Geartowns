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

#ifndef AUDIO_COMMON_H
#define AUDIO_COMMON_H

#include "../common/common.h"

static const u8 k_midi_voice_lengths[8] = { 3, 3, 3, 3, 2, 2, 3, 0 };
static const u8 k_midi_mt32_reset[11] = { 0xF0, 0x41, 0x10, 0x16, 0x12, 0x7F, 0x00, 0x00, 0x00, 0x01, 0xF7 };

INLINE s16 clamp_s16(s64 value)
{
    return (s16)((value < -32768) ? -32768 : ((value > 32767) ? 32767 : value));
}

#endif /* AUDIO_COMMON_H */
