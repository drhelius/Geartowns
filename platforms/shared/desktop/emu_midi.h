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

#ifndef EMU_MIDI_H
#define EMU_MIDI_H

#include "geartowns.h"

#ifdef EMU_MIDI_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

EXTERN void emu_midi_init(void);
EXTERN void emu_midi_destroy(void);
EXTERN void emu_midi_begin_frame(void);
EXTERN void emu_midi_end_frame(int sample_count);
EXTERN void emu_midi_idle(void);
EXTERN void emu_midi_rewind(void);
EXTERN void emu_midi_state_loaded(void);
EXTERN void emu_midi_update_port(void);
EXTERN bool emu_midi_prepare(void);
EXTERN int emu_midi_get_synth(void);
EXTERN int emu_midi_get_known_synth(void);
EXTERN const char* emu_midi_get_synth_label(int synth);
EXTERN int emu_midi_get_wanted_engine(void);
EXTERN const char* emu_midi_get_wanted_port(void);
EXTERN const char* emu_midi_get_port(void);

#undef EMU_MIDI_IMPORT
#undef EXTERN

#endif /* EMU_MIDI_H */
