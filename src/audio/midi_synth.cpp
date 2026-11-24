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

#include <algorithm>
#include <chrono>
#include <vector>
#include "midi_synth.h"
#include "midi_out.h"
#include "mt32_engine.h"
#include "soundfont_engine.h"
#include "../media/media_file.h"

MidiSynth::MidiSynth()
{
    InitPointer(m_midi_out);
    InitPointer(m_mt32);
    InitPointer(m_soundfont);
    m_engine = GT_MIDI_ENGINE_NONE;

    for (int i = 0; i < GT_MIDI_ENGINE_COUNT; i++)
        m_open[i] = false;

    m_mt32_name[0] = 0;
    m_render_ms = 0.0f;
    m_dropped = 0;
}

MidiSynth::~MidiSynth()
{
    SafeDelete(m_soundfont);
    SafeDelete(m_mt32);
}

void MidiSynth::Init(MidiOut* midi_out)
{
    m_midi_out = midi_out;
}

bool MidiSynth::IsAvailable(int engine)
{
    switch (engine)
    {
        case GT_MIDI_ENGINE_MT32:
            return MT32Engine::IsAvailable();
        case GT_MIDI_ENGINE_SOUNDFONT:
            return SoundFontEngine::IsAvailable();
        default:
            return false;
    }
}

int MidiSynth::ScanMT32ROMs(const char* folder, MidiSynth_MT32_ROMs* sets, int max_sets)
{
    return MT32Engine::Scan(folder, sets, max_sets);
}

int MidiSynth::FindMT32ROMs(const MidiSynth_MT32_ROMs* sets, int count, int model)
{
    for (int i = 0; (model != GT_MT32_MODEL_AUTO) && (i < count); i++)
    {
        if (sets[i].model == model)
            return i;
    }

    return count > 0 ? 0 : -1;
}

bool MidiSynth::IsSoundFontFile(const char* path)
{
    return ends_with_no_case(path, ".sf2") || ends_with_no_case(path, ".sf3");
}

bool MidiSynth::FindSoundFont(const char* folder, std::string& path)
{
    std::vector<std::string> folders;
    std::vector<std::string> files;

    if (!IsValidPointer(folder) || (folder[0] == 0) || !MediaFile::ListDirectory(folder, folders, files))
        return false;

    std::sort(files.begin(), files.end());

    for (size_t i = 0; i < files.size(); i++)
    {
        if ((files[i][0] == '.') || !IsSoundFontFile(files[i].c_str()))
            continue;

        path = folder;
        append_path_component(path, files[i].c_str());
        return true;
    }

    return false;
}

bool MidiSynth::OpenMT32(const MidiSynth_MT32_ROMs& roms)
{
    if (!IsValidPointer(m_mt32))
        m_mt32 = new MT32Engine();

    m_open[GT_MIDI_ENGINE_MT32] = m_mt32->Open(roms);
    strncpy_fit(m_mt32_name, m_open[GT_MIDI_ENGINE_MT32] ? roms.name : "", sizeof(m_mt32_name));

    if (m_engine == GT_MIDI_ENGINE_MT32)
        Reset();

    return m_open[GT_MIDI_ENGINE_MT32];
}

void MidiSynth::CloseMT32()
{
    if (IsValidPointer(m_mt32))
        m_mt32->Close();

    m_open[GT_MIDI_ENGINE_MT32] = false;
    m_mt32_name[0] = 0;

    if (m_engine == GT_MIDI_ENGINE_MT32)
        Reset();
}

bool MidiSynth::OpenSoundFont(const char* path)
{
    if (!IsValidPointer(m_soundfont))
        m_soundfont = new SoundFontEngine();

    m_open[GT_MIDI_ENGINE_SOUNDFONT] = m_soundfont->Open(path);
    m_soundfont_path = m_open[GT_MIDI_ENGINE_SOUNDFONT] ? path : "";

    if (m_engine == GT_MIDI_ENGINE_SOUNDFONT)
        Reset();

    return m_open[GT_MIDI_ENGINE_SOUNDFONT];
}

void MidiSynth::CloseSoundFont()
{
    if (IsValidPointer(m_soundfont))
        m_soundfont->Close();

    m_open[GT_MIDI_ENGINE_SOUNDFONT] = false;
    m_soundfont_path.clear();

    if (m_engine == GT_MIDI_ENGINE_SOUNDFONT)
        Reset();
}

void MidiSynth::SetEngine(int engine)
{
    if ((engine < GT_MIDI_ENGINE_NONE) || (engine >= GT_MIDI_ENGINE_COUNT) || (engine == m_engine))
        return;

    bool was_active = IsActive();
    m_engine = engine;

    if (was_active || IsActive())
        Reset();
}

void MidiSynth::SetGain(int engine, float gain)
{
    if ((engine == GT_MIDI_ENGINE_MT32) && IsValidPointer(m_mt32))
        m_mt32->SetGain(gain);
    else if ((engine == GT_MIDI_ENGINE_SOUNDFONT) && IsValidPointer(m_soundfont))
        m_soundfont->SetGain(gain);
}

void MidiSynth::Render(s16* buffer, int frames)
{
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    int count = m_midi_out->GetMessageCount();
    const MidiOut::MidiOut_Message* messages = m_midi_out->GetMessages();
    const u8* data = m_midi_out->GetData();
    u32 dropped = 0;

    if (m_engine == GT_MIDI_ENGINE_MT32)
        dropped = m_mt32->Render(messages, count, data, buffer, frames);
    else
        dropped = m_soundfont->Render(messages, count, data, buffer, frames);

    m_midi_out->Clear();

    if (dropped > 0)
    {
        m_dropped += dropped;
        m_midi_out->AddDropped(dropped);
    }

    std::chrono::duration<float, std::milli> elapsed = std::chrono::steady_clock::now() - start;
    m_render_ms = elapsed.count();
}

void MidiSynth::GetStatus(MidiSynth_Status* status)
{
    status->engine = IsActive() ? m_engine : GT_MIDI_ENGINE_NONE;
    status->mt32_open = m_open[GT_MIDI_ENGINE_MT32];
    status->soundfont_open = m_open[GT_MIDI_ENGINE_SOUNDFONT];
    strncpy_fit(status->mt32_name, m_mt32_name, sizeof(status->mt32_name));
    status->soundfont_path = m_soundfont_path.c_str();
    status->voices = 0;
    status->max_voices = 0;
    status->render_ms = IsActive() ? m_render_ms : 0.0f;
    status->dropped = m_dropped;

    if (status->engine == GT_MIDI_ENGINE_MT32)
    {
        status->voices = m_mt32->GetVoices();
        status->max_voices = m_mt32->GetMaxVoices();
    }
    else if (status->engine == GT_MIDI_ENGINE_SOUNDFONT)
    {
        status->voices = m_soundfont->GetVoices();
        status->max_voices = m_soundfont->GetMaxVoices();
    }
}

void MidiSynth::Reset()
{
    m_midi_out->Clear();

    if (!IsActive())
    {
        m_midi_out->ForgetSynth();
        return;
    }

    if (m_engine == GT_MIDI_ENGINE_MT32)
        m_mt32->Reset();
    else
        m_soundfont->Reset();

    m_midi_out->ResetSynth();
    m_midi_out->Sync();
}
