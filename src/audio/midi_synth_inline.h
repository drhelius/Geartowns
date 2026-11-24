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

#ifndef MIDI_SYNTH_INLINE_H
#define MIDI_SYNTH_INLINE_H

#include "midi_synth.h"

INLINE bool MidiSynth::IsOpen(int engine) const
{
    return (engine > GT_MIDI_ENGINE_NONE) && (engine < GT_MIDI_ENGINE_COUNT) && m_open[engine];
}

INLINE const char* MidiSynth::GetMT32Name() const
{
    return m_mt32_name;
}

INLINE const char* MidiSynth::GetSoundFontPath() const
{
    return m_soundfont_path.c_str();
}

INLINE int MidiSynth::GetEngine() const
{
    return m_engine;
}

INLINE bool MidiSynth::IsActive() const
{
    return m_open[m_engine];
}

#endif /* MIDI_SYNTH_INLINE_H */
