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

#include "mt32_engine.h"
#include "audio_common.h"

#if !defined(GT_DISABLE_MT32)

#include <algorithm>
#include <vector>
#include "mt32emu.h"
#include "../media/media_file.h"

struct MT32Engine_Control
{
    const char* rom;
    const char* name;
    int model;
    const char* pcm;
};

struct MT32Engine_Image
{
    std::vector<u8> data[2];
    MT32Emu::ArrayFile* file[2];
    const MT32Emu::ROMImage* image;
};

struct MT32Engine_Candidate
{
    std::string path[2];
    std::string rom;
};

static const MT32Engine_Control k_mt32_engine_controls[] =
{
    { "ctrl_cm32l_1_02", "CM-32L 1.02", GT_MT32_MODEL_CM32L, "pcm_cm32l" },
    { "ctrl_cm32l_1_00", "CM-32L 1.00", GT_MT32_MODEL_CM32L, "pcm_cm32l" },
    { "ctrl_mt32_1_07", "MT-32 1.07", GT_MT32_MODEL_MT32_OLD, "pcm_mt32" },
    { "ctrl_mt32_1_06", "MT-32 1.06", GT_MT32_MODEL_MT32_OLD, "pcm_mt32" },
    { "ctrl_mt32_1_05", "MT-32 1.05", GT_MT32_MODEL_MT32_OLD, "pcm_mt32" },
    { "ctrl_mt32_1_04", "MT-32 1.04", GT_MT32_MODEL_MT32_OLD, "pcm_mt32" },
    { "ctrl_mt32_bluer", "MT-32 Blue Ridge", GT_MT32_MODEL_MT32_OLD, "pcm_mt32" },
    { "ctrl_mt32_2_07", "MT-32 2.07", GT_MT32_MODEL_MT32_NEW, "pcm_mt32" },
    { "ctrl_mt32_2_06", "MT-32 2.06", GT_MT32_MODEL_MT32_NEW, "pcm_mt32" },
    { "ctrl_mt32_2_04", "MT-32 2.04", GT_MT32_MODEL_MT32_NEW, "pcm_mt32" },
    { "ctrl_mt32_2_03", "MT-32 2.03", GT_MT32_MODEL_MT32_NEW, "pcm_mt32" },
    { "ctrl_cm32ln_1_00", "CM-32LN 1.00", GT_MT32_MODEL_CM32L, "pcm_cm32l" }
};

static const int k_mt32_engine_control_count = (int)(sizeof(k_mt32_engine_controls) / sizeof(MT32Engine_Control));
static const u32 k_mt32_engine_partials = 32;
static const u32 k_mt32_engine_queue_size = 32768;
static const int k_mt32_engine_max_frames = GT_AUDIO_BUFFER_SIZE / 2;
static const int k_mt32_engine_native_frames = k_mt32_engine_max_frames + (k_mt32_engine_max_frames / 4);
static const u32 k_mt32_engine_rate = 48000;
static const u32 k_mt32_engine_max_rom_size = 0x100000;

static bool is_rom_size(u64 size)
{
    return (size == 0x8000) || (size == 0x10000) || (size == 0x20000) || (size == 0x40000) || (size == 0x80000) ||
        (size == 0x100000);
}

static void free_image(MT32Engine_Image& image)
{
    if (IsValidPointer(image.image))
        MT32Emu::ROMImage::freeROMImage(image.image);

    InitPointer(image.image);
    SafeDelete(image.file[0]);
    SafeDelete(image.file[1]);
}

static bool make_image(const std::string* paths, MT32Engine_Image& image)
{
    InitPointer(image.image);
    InitPointer(image.file[0]);
    InitPointer(image.file[1]);
    int count = paths[1].empty() ? 1 : 2;

    for (int i = 0; i < count; i++)
    {
        if (!MediaFile::ReadFile(paths[i].c_str(), k_mt32_engine_max_rom_size, &image.data[i]))
            return false;

        image.file[i] = new MT32Emu::ArrayFile(&image.data[i][0], image.data[i].size());
    }

    if (count == 1)
        image.image = MT32Emu::ROMImage::makeROMImage(image.file[0], MT32Emu::ROMInfo::getFullROMInfos());
    else
        image.image = MT32Emu::ROMImage::makeROMImage(image.file[0], image.file[1]);

    if (!IsValidPointer(image.image) || !IsValidPointer(image.image->getROMInfo()))
    {
        free_image(image);
        return false;
    }

    return true;
}

static const MT32Engine_Candidate* find_candidate(const std::vector<MT32Engine_Candidate>& candidates,
    const char* rom)
{
    for (size_t i = 0; i < candidates.size(); i++)
    {
        if (candidates[i].rom == rom)
            return &candidates[i];
    }

    return NULL;
}

MT32Engine::MT32Engine()
{
    InitPointer(m_synth);
    InitPointer(m_native);
    m_phase = 0;
    m_previous[0] = m_previous[1] = 0;
    m_current[0] = m_current[1] = 0;
}

MT32Engine::~MT32Engine()
{
    Close();
    SafeDeleteArray(m_native);
}

bool MT32Engine::IsAvailable()
{
    return true;
}

int MT32Engine::Scan(const char* folder, MidiSynth::MidiSynth_MT32_ROMs* sets, int max_sets)
{
    std::vector<std::string> folders;
    std::vector<std::string> files;

    if (!IsValidPointer(folder) || (folder[0] == 0) || !MediaFile::ListDirectory(folder, folders, files))
        return 0;

    std::sort(files.begin(), files.end());
    std::vector<MT32Engine_Candidate> candidates;
    std::vector<std::string> partial_paths;
    std::vector<const MT32Emu::ROMInfo*> partial_infos;

    for (size_t i = 0; i < files.size(); i++)
    {
        if (files[i][0] == '.')
            continue;

        std::string path(folder);
        append_path_component(path, files[i].c_str());
        u64 size = 0;
        s64 modified = 0;

        if (!MediaFile::GetFileInfo(path.c_str(), &size, &modified) || !is_rom_size(size))
            continue;

        std::vector<u8> data;

        if (!MediaFile::ReadFile(path.c_str(), k_mt32_engine_max_rom_size, &data))
            continue;

        MT32Emu::ArrayFile file(&data[0], data.size());
        const MT32Emu::ROMInfo* full = MT32Emu::ROMInfo::getROMInfo(&file, MT32Emu::ROMInfo::getFullROMInfos());
        const MT32Emu::ROMInfo* partial = MT32Emu::ROMInfo::getROMInfo(&file,
            MT32Emu::ROMInfo::getPartialROMInfos());

        if (IsValidPointer(full) && !IsValidPointer(find_candidate(candidates, full->shortName)))
        {
            MT32Engine_Candidate candidate;
            candidate.path[0] = path;
            candidate.rom = full->shortName;
            candidates.push_back(candidate);
        }

        if (IsValidPointer(partial))
        {
            partial_paths.push_back(path);
            partial_infos.push_back(partial);
        }
    }

    for (size_t i = 0; i < partial_infos.size(); i++)
    {
        for (size_t j = 0; j < partial_infos.size(); j++)
        {
            if ((i == j) || (partial_infos[j]->pairROMInfo != partial_infos[i]))
                continue;

            std::string paths[2] = { partial_paths[i], partial_paths[j] };
            MT32Engine_Image image;

            if (!make_image(paths, image))
                continue;

            const MT32Emu::ROMInfo* info = image.image->getROMInfo();

            if (!IsValidPointer(find_candidate(candidates, info->shortName)))
            {
                MT32Engine_Candidate candidate;
                candidate.path[0] = paths[0];
                candidate.path[1] = paths[1];
                candidate.rom = info->shortName;
                candidates.push_back(candidate);
            }

            free_image(image);
        }
    }

    int count = 0;

    for (int i = 0; (i < k_mt32_engine_control_count) && (count < max_sets); i++)
    {
        const MT32Engine_Control& control = k_mt32_engine_controls[i];
        const MT32Engine_Candidate* rom = find_candidate(candidates, control.rom);
        const MT32Engine_Candidate* pcm = find_candidate(candidates, control.pcm);

        if (!IsValidPointer(rom) || !IsValidPointer(pcm))
            continue;

        MidiSynth::MidiSynth_MT32_ROMs& set = sets[count++];

        for (int j = 0; j < 2; j++)
        {
            set.control[j] = rom->path[j];
            set.pcm[j] = pcm->path[j];
        }

        set.model = control.model;
        strncpy_fit(set.name, control.name, sizeof(set.name));
    }

    return count;
}

bool MT32Engine::Open(const MidiSynth::MidiSynth_MT32_ROMs& roms)
{
    Close();

    MT32Engine_Image control;
    MT32Engine_Image pcm;
    bool ok = make_image(roms.control, control);
    ok = make_image(roms.pcm, pcm) && ok;

    if (ok)
    {
        m_synth = new MT32Emu::Synth();
        m_synth->selectRendererType(MT32Emu::RendererType_BIT16S);
        ok = m_synth->open(*control.image, *pcm.image, k_mt32_engine_partials, MT32Emu::AnalogOutputMode_ACCURATE) &&
            (m_synth->getStereoOutputSampleRate() == k_mt32_engine_rate);
    }

    free_image(control);
    free_image(pcm);

    if (!ok)
    {
        Close();
        return false;
    }

    m_synth->setMIDIEventQueueSize(k_mt32_engine_queue_size);
    m_synth->setDACInputMode(MT32Emu::DACInputMode_NICE);
    m_synth->setMIDIDelayMode(MT32Emu::MIDIDelayMode_IMMEDIATE);

    if (!IsValidPointer(m_native))
        m_native = new s16[k_mt32_engine_native_frames * 2];

    m_phase = 0;
    m_previous[0] = m_previous[1] = 0;
    m_current[0] = m_current[1] = 0;
    return true;
}

void MT32Engine::Close()
{
    if (!IsValidPointer(m_synth))
        return;

    m_synth->close();
    SafeDelete(m_synth);
}

void MT32Engine::Reset()
{
    if (!IsValidPointer(m_synth))
        return;

    m_synth->flushMIDIQueue();
    m_synth->playSysexNow(k_midi_mt32_reset, sizeof(k_midi_mt32_reset));
}

bool MT32Engine::IsOpen() const
{
    return IsValidPointer(m_synth);
}

void MT32Engine::SetGain(float gain)
{
    if (!IsValidPointer(m_synth))
        return;

    m_synth->setOutputGain(gain);
    m_synth->setReverbOutputGain(gain);
}

u32 MT32Engine::Render(const MidiOut::MidiOut_Message* messages, int count, const u8* data, s16* buffer, int frames)
{
    frames = MIN(frames, k_mt32_engine_max_frames);
    u32 start = m_synth->getInternalRenderedSampleCount();
    u32 dropped = 0;
    int position = 0;

    for (int i = 0; i < count; i++)
    {
        const MidiOut::MidiOut_Message& message = messages[i];
        const u8* bytes = data + message.data;

        if (message.offset != k_midi_out_immediate)
            position = MAX(position, MIN((int)message.offset, frames));

        u32 timestamp = start + (u32)(((u64)position * MT32Emu::SAMPLE_RATE) / GT_AUDIO_SAMPLE_RATE);
        bool ok = true;

        if (bytes[0] == 0xF0)
            ok = m_synth->playSysex(bytes, message.size, timestamp);
        else if (bytes[0] < 0xF0)
        {
            u32 value = bytes[0];

            if (message.size > 1)
                value |= (u32)bytes[1] << 8;

            if (message.size > 2)
                value |= (u32)bytes[2] << 16;

            ok = m_synth->playMsg(value, timestamp);
        }

        if (!ok)
            dropped++;
    }

    u32 native = (u32)(((u64)m_phase + ((u64)frames * k_mt32_engine_rate)) / GT_AUDIO_SAMPLE_RATE);

    if (native > 0)
        m_synth->render(m_native, native);

    u32 index = 0;

    for (int i = 0; i < frames; i++)
    {
        m_phase += k_mt32_engine_rate;

        while ((m_phase >= GT_AUDIO_SAMPLE_RATE) && (index < native))
        {
            m_phase -= GT_AUDIO_SAMPLE_RATE;
            m_previous[0] = m_current[0];
            m_previous[1] = m_current[1];
            m_current[0] = m_native[(index * 2) + 0];
            m_current[1] = m_native[(index * 2) + 1];
            index++;
        }

        s32 phase = (s32)m_phase;
        buffer[(i * 2) + 0] = (s16)(m_previous[0] + (((s32)m_current[0] - m_previous[0]) * phase) /
            GT_AUDIO_SAMPLE_RATE);
        buffer[(i * 2) + 1] = (s16)(m_previous[1] + (((s32)m_current[1] - m_previous[1]) * phase) /
            GT_AUDIO_SAMPLE_RATE);
    }

    return dropped;
}

int MT32Engine::GetVoices() const
{
    if (!IsValidPointer(m_synth))
        return 0;

    MT32Emu::PartialState states[k_mt32_engine_partials];
    u32 partials = MIN(m_synth->getPartialCount(), k_mt32_engine_partials);
    int voices = 0;
    m_synth->getPartialStates(states);

    for (u32 i = 0; i < partials; i++)
    {
        if (states[i] != MT32Emu::PartialState_INACTIVE)
            voices++;
    }

    return voices;
}

int MT32Engine::GetMaxVoices() const
{
    return (int)k_mt32_engine_partials;
}

#else

MT32Engine::MT32Engine()
{
    InitPointer(m_synth);
    InitPointer(m_native);
    m_phase = 0;
    m_previous[0] = m_previous[1] = 0;
    m_current[0] = m_current[1] = 0;
}

MT32Engine::~MT32Engine()
{
}

bool MT32Engine::IsAvailable()
{
    return false;
}

int MT32Engine::Scan(const char* folder, MidiSynth::MidiSynth_MT32_ROMs* sets, int max_sets)
{
    UNUSED(folder);
    UNUSED(sets);
    UNUSED(max_sets);
    return 0;
}

bool MT32Engine::Open(const MidiSynth::MidiSynth_MT32_ROMs& roms)
{
    UNUSED(roms);
    return false;
}

void MT32Engine::Close()
{
}

void MT32Engine::Reset()
{
}

bool MT32Engine::IsOpen() const
{
    return false;
}

void MT32Engine::SetGain(float gain)
{
    UNUSED(gain);
}

u32 MT32Engine::Render(const MidiOut::MidiOut_Message* messages, int count, const u8* data, s16* buffer, int frames)
{
    UNUSED(messages);
    UNUSED(count);
    UNUSED(data);
    memset(buffer, 0, sizeof(s16) * frames * 2);
    return 0;
}

int MT32Engine::GetVoices() const
{
    return 0;
}

int MT32Engine::GetMaxVoices() const
{
    return 0;
}

#endif
