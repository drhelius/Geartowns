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

#ifndef MT32_ENGINE_H
#define MT32_ENGINE_H

#include "../common/common.h"
#include "midi_out.h"
#include "midi_synth.h"

namespace MT32Emu
{
    class Synth;
}

class MT32Engine
{
public:
    MT32Engine();
    ~MT32Engine();
    static bool IsAvailable();
    static int Scan(const char* folder, MidiSynth::MidiSynth_MT32_ROMs* sets, int max_sets);
    bool Open(const MidiSynth::MidiSynth_MT32_ROMs& roms);
    void Close();
    void Reset();
    bool IsOpen() const;
    void SetGain(float gain);
    u32 Render(const MidiOut::MidiOut_Message* messages, int count, const u8* data, s16* buffer, int frames);
    int GetVoices() const;
    int GetMaxVoices() const;

private:
    MT32Emu::Synth* m_synth;
    s16* m_native;
    u32 m_phase;
    s16 m_previous[2];
    s16 m_current[2];
};

#endif /* MT32_ENGINE_H */
