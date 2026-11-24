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

#ifndef MIDI_SYNTH_HOST_H
#define MIDI_SYNTH_HOST_H

#include <string>
#include <vector>
#include "../common/common.h"
#include "midi_synth.h"

class MidiSynthHost
{
public:
    enum MidiSynthHost_Open
    {
        MIDI_SYNTH_HOST_NONE = 0,
        MIDI_SYNTH_HOST_OPENED,
        MIDI_SYNTH_HOST_FAILED
    };

    MidiSynthHost();
    void ScanMT32(const std::vector<std::string>& folders);
    void FindSoundFont(const std::vector<std::string>& folders);
    void SetSoundFont(const std::string& path);
    bool IsAvailable(int engine) const;
    int FindMT32(int model) const;
    MidiSynthHost_Open OpenMT32(MidiSynth* synth, int index);
    MidiSynthHost_Open OpenSoundFont(MidiSynth* synth);
    void ForgetMT32();
    int GetMT32Count() const;
    int GetMT32Index() const;
    bool HasMT32Failed() const;
    bool HasSoundFontFailed() const;
    const MidiSynth::MidiSynth_MT32_ROMs& GetMT32(int index) const;
    const std::string& GetMT32Folder() const;
    const std::string& GetSoundFont() const;

private:
    MidiSynth::MidiSynth_MT32_ROMs m_mt32_sets[MIDI_SYNTH_MT32_SETS];
    int m_mt32_count;
    int m_mt32_index;
    bool m_mt32_failed;
    std::string m_mt32_folder;
    std::string m_soundfont;
    bool m_soundfont_failed;
};

#endif /* MIDI_SYNTH_HOST_H */
