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

#ifndef EMU_SYNTH_H
#define EMU_SYNTH_H

#include <string>
#include <vector>
#include "geartowns.h"

#ifdef EMU_SYNTH_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

EXTERN void emu_synth_init(void);
EXTERN MidiSynth* emu_synth_get_midi_synth(void);
EXTERN void emu_synth_scan_mt32(void);
EXTERN void emu_synth_find_soundfont(void);
EXTERN void emu_synth_update(void);
EXTERN void emu_synth_get_default_folders(int engine, std::vector<std::string>& folders);
EXTERN bool emu_synth_is_compiled(int engine);
EXTERN bool emu_synth_is_available(int engine);
EXTERN bool emu_synth_activate(int engine);
EXTERN int emu_synth_get_engine(int synth);
EXTERN const char* emu_synth_get_label(int engine);
EXTERN const char* emu_synth_get_playing_name(int engine);
EXTERN const char* emu_synth_get_mt32_folder(void);
EXTERN const char* emu_synth_get_mt32_status(void);
EXTERN const char* emu_synth_get_soundfont(void);
EXTERN const char* emu_synth_get_soundfont_status(void);

#undef EMU_SYNTH_IMPORT
#undef EXTERN

#endif /* EMU_SYNTH_H */
