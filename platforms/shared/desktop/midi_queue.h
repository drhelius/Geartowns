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

#ifndef MIDI_QUEUE_H
#define MIDI_QUEUE_H

#include "geartowns.h"

#ifdef MIDI_QUEUE_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

EXTERN void midi_queue_init(void);
EXTERN void midi_queue_destroy(void);
EXTERN bool midi_queue_prepare(void);
EXTERN bool midi_queue_open(const char* id);
EXTERN void midi_queue_close(void);
EXTERN bool midi_queue_is_open(void);
EXTERN bool midi_queue_is_lost(void);
EXTERN void midi_queue_check(void);
EXTERN const char* midi_queue_get_port(void);
EXTERN void midi_queue_frame(MidiOut* midi, int frame_samples);
EXTERN void midi_queue_flush(MidiOut* midi);
EXTERN void midi_queue_drain(int timeout_ms);
EXTERN u32 midi_queue_get_dropped(void);

#undef MIDI_QUEUE_IMPORT
#undef EXTERN

#endif /* MIDI_QUEUE_H */
