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

#ifndef SOUNDFONT_ENGINE_H
#define SOUNDFONT_ENGINE_H

#include "../common/common.h"
#include "midi_out.h"

struct _fluid_hashtable_t;
struct _fluid_synth_t;

class SoundFontEngine
{
public:
    SoundFontEngine();
    ~SoundFontEngine();
    static bool IsAvailable();
    bool Open(const char* path);
    void Close();
    void Reset();
    bool IsOpen() const;
    void SetGain(float gain);
    u32 Render(const MidiOut::MidiOut_Message* messages, int count, const u8* data, s16* buffer, int frames);
    int GetVoices() const;
    int GetMaxVoices() const;

private:
    void Play(const u8* data, int size);
    void Write(s16* buffer, int frames);

private:
    _fluid_hashtable_t* m_settings;
    _fluid_synth_t* m_synth;
};

#endif /* SOUNDFONT_ENGINE_H */
