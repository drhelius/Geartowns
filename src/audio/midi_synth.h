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

#ifndef MIDI_SYNTH_H
#define MIDI_SYNTH_H

#include <string>
#include "../common/common.h"

#define MIDI_SYNTH_MT32_SETS 16
#define MIDI_SYNTH_NAME_SIZE 48

class MidiOut;
class MT32Engine;
class SoundFontEngine;

class MidiSynth
{
public:
    struct MidiSynth_MT32_ROMs
    {
        std::string control[2];
        std::string pcm[2];
        int model;
        char name[MIDI_SYNTH_NAME_SIZE];
    };

    struct MidiSynth_Status
    {
        int engine;
        bool mt32_open;
        bool soundfont_open;
        char mt32_name[MIDI_SYNTH_NAME_SIZE];
        const char* soundfont_path;
        int voices;
        int max_voices;
        float render_ms;
        u32 dropped;
    };

public:
    MidiSynth();
    ~MidiSynth();
    void Init(MidiOut* midi_out);
    static bool IsAvailable(int engine);
    static int ScanMT32ROMs(const char* folder, MidiSynth_MT32_ROMs* sets, int max_sets);
    static int FindMT32ROMs(const MidiSynth_MT32_ROMs* sets, int count, int model);
    static bool IsSoundFontFile(const char* path);
    static bool FindSoundFont(const char* folder, std::string& path);
    bool OpenMT32(const MidiSynth_MT32_ROMs& roms);
    void CloseMT32();
    bool OpenSoundFont(const char* path);
    void CloseSoundFont();
    bool IsOpen(int engine) const;
    const char* GetMT32Name() const;
    const char* GetSoundFontPath() const;
    void SetEngine(int engine);
    int GetEngine() const;
    bool IsActive() const;
    void SetGain(int engine, float gain);
    void Render(s16* buffer, int frames);
    void GetStatus(MidiSynth_Status* status);
    void Reset();

private:
    MidiOut* m_midi_out;
    MT32Engine* m_mt32;
    SoundFontEngine* m_soundfont;
    int m_engine;
    bool m_open[GT_MIDI_ENGINE_COUNT];
    char m_mt32_name[MIDI_SYNTH_NAME_SIZE];
    std::string m_soundfont_path;
    float m_render_ms;
    u32 m_dropped;
};

#include "midi_synth_inline.h"

#endif /* MIDI_SYNTH_H */
