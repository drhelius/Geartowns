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

#include "midi_synth_host.h"

MidiSynthHost::MidiSynthHost()
{
    m_mt32_count = 0;
    m_mt32_index = -1;
    m_mt32_failed = false;
    m_soundfont_failed = false;
}

void MidiSynthHost::ScanMT32(const std::vector<std::string>& folders)
{
    m_mt32_count = 0;
    m_mt32_index = -1;
    m_mt32_failed = false;
    m_mt32_folder.clear();

    for (size_t i = 0; MidiSynth::IsAvailable(GT_MIDI_ENGINE_MT32) && (i < folders.size()); i++)
    {
        m_mt32_count = MidiSynth::ScanMT32ROMs(folders[i].c_str(), m_mt32_sets, MIDI_SYNTH_MT32_SETS);

        if (m_mt32_count > 0)
        {
            m_mt32_folder = folders[i];
            return;
        }
    }
}

void MidiSynthHost::FindSoundFont(const std::vector<std::string>& folders)
{
    std::string path;

    for (size_t i = 0; MidiSynth::IsAvailable(GT_MIDI_ENGINE_SOUNDFONT) && (i < folders.size()) && path.empty(); i++)
        MidiSynth::FindSoundFont(folders[i].c_str(), path);

    SetSoundFont(path);
}

void MidiSynthHost::SetSoundFont(const std::string& path)
{
    m_soundfont = MidiSynth::IsAvailable(GT_MIDI_ENGINE_SOUNDFONT) ? path : "";
    m_soundfont_failed = false;
}

bool MidiSynthHost::IsAvailable(int engine) const
{
    switch (engine)
    {
        case GT_MIDI_ENGINE_MT32:
            return (m_mt32_count > 0) && !m_mt32_failed;
        case GT_MIDI_ENGINE_SOUNDFONT:
            return !m_soundfont.empty() && !m_soundfont_failed;
        default:
            return false;
    }
}

int MidiSynthHost::FindMT32(int model) const
{
    return MidiSynth::FindMT32ROMs(m_mt32_sets, m_mt32_count, model);
}

MidiSynthHost::MidiSynthHost_Open MidiSynthHost::OpenMT32(MidiSynth* synth, int index)
{
    m_mt32_index = index;

    if ((index < 0) || m_mt32_failed)
        return MIDI_SYNTH_HOST_NONE;

    if (synth->OpenMT32(m_mt32_sets[index]))
        return MIDI_SYNTH_HOST_OPENED;

    m_mt32_failed = true;
    return MIDI_SYNTH_HOST_FAILED;
}

MidiSynthHost::MidiSynthHost_Open MidiSynthHost::OpenSoundFont(MidiSynth* synth)
{
    if (!IsAvailable(GT_MIDI_ENGINE_SOUNDFONT))
        return MIDI_SYNTH_HOST_NONE;

    if (synth->OpenSoundFont(m_soundfont.c_str()))
        return MIDI_SYNTH_HOST_OPENED;

    m_soundfont_failed = true;
    return MIDI_SYNTH_HOST_FAILED;
}

void MidiSynthHost::ForgetMT32()
{
    m_mt32_index = -1;
}

int MidiSynthHost::GetMT32Count() const
{
    return m_mt32_count;
}

int MidiSynthHost::GetMT32Index() const
{
    return m_mt32_index;
}

bool MidiSynthHost::HasMT32Failed() const
{
    return m_mt32_failed;
}

bool MidiSynthHost::HasSoundFontFailed() const
{
    return m_soundfont_failed;
}

const MidiSynth::MidiSynth_MT32_ROMs& MidiSynthHost::GetMT32(int index) const
{
    return m_mt32_sets[CLAMP(index, 0, MIDI_SYNTH_MT32_SETS - 1)];
}

const std::string& MidiSynthHost::GetMT32Folder() const
{
    return m_mt32_folder;
}

const std::string& MidiSynthHost::GetSoundFont() const
{
    return m_soundfont;
}
