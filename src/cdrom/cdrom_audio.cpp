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

#include "cdrom_audio.h"
#include "cdrom_media.h"
#include "../common/state_serializer.h"

// First order fit of the 50/15 us de-emphasis curve at 44.1 kHz in Q16, within 0.25 dB up to 20 kHz
static const s64 k_cdrom_deemphasis_b0 = 30011;
static const s64 k_cdrom_deemphasis_b1 = -4478;
static const s64 k_cdrom_deemphasis_a1 = 40003;

CdRomAudio::CdRomAudio(CdRomMedia* cdrom_media)
{
    m_cdrom_media = cdrom_media;
    memset(&m_state, 0, sizeof(m_state));
    memset(m_sector_cache, 0, sizeof(m_sector_cache));
    m_sector_cache_lba = 0;
    m_sector_cache_valid = false;
    m_seek_scale = 1.0;
    m_deemphasis_end_valid = false;
    memset(m_deemphasis_end_input, 0, sizeof(m_deemphasis_end_input));
    memset(m_deemphasis_end_output, 0, sizeof(m_deemphasis_end_output));
}

CdRomAudio::~CdRomAudio()
{
}

void CdRomAudio::Init()
{
    Reset();
}

void CdRomAudio::Reset()
{
    m_state.play_state = CDROM_AUDIO_IDLE;
    m_state.start_lba = 0;
    m_state.end_lba = 0;
    m_state.current_lba = 0;
    m_state.current_sample = 0;
    m_state.seek_samples = 0;
    m_state.repeat = false;
    m_state.deemphasis_primed = false;
    memset(m_state.deemphasis_input, 0, sizeof(m_state.deemphasis_input));
    memset(m_state.deemphasis_output, 0, sizeof(m_state.deemphasis_output));
    m_sector_cache_valid = false;
    m_deemphasis_end_valid = false;
}

// The range is half open
// Playback stops with the position resting on its end
void CdRomAudio::Play(u32 start_lba, u32 end_lba, bool repeat)
{
    u32 sector_count = m_cdrom_media->GetSectorCount();
    end_lba = MIN(end_lba, sector_count);

    u32 seek_ms = (u32)(m_cdrom_media->SeekTime(m_cdrom_media->GetCurrentSector(), start_lba) * m_seek_scale);

    m_state.start_lba = start_lba;
    m_state.end_lba = end_lba;
    m_state.current_lba = start_lba;
    m_state.current_sample = 0;
    m_state.seek_samples = (seek_ms * GT_AUDIO_SAMPLE_RATE) / 1000;
    m_state.repeat = repeat;
    m_state.deemphasis_primed = false;
    m_sector_cache_valid = false;

    if (start_lba >= end_lba)
    {
        m_state.play_state = CDROM_AUDIO_IDLE;
        return;
    }

    m_state.play_state = CDROM_AUDIO_PLAYING;
    m_cdrom_media->SetCurrentSector(start_lba);

    s32 track = m_cdrom_media->FindTrackFromLBA(start_lba);

    if (track >= 0)
        m_cdrom_media->PreloadTrack((u32)track);

    Debug("CD AUDIO: Play LBA %u to %u, repeat %d, seek %u ms", start_lba, end_lba, repeat, seek_ms);
}

void CdRomAudio::SetSeekScale(double scale)
{
    m_seek_scale = scale;
}

void CdRomAudio::Pause()
{
    if (m_state.play_state == CDROM_AUDIO_PLAYING)
        m_state.play_state = CDROM_AUDIO_PAUSED;
}

void CdRomAudio::Resume()
{
    if (m_state.play_state == CDROM_AUDIO_PAUSED)
        m_state.play_state = CDROM_AUDIO_PLAYING;
}

void CdRomAudio::Stop()
{
    m_state.play_state = CDROM_AUDIO_IDLE;
    m_state.seek_samples = 0;
    m_sector_cache_valid = false;
}

// Data sectors inside the range and unreadable sectors play as silence
// Tracks mastered with pre-emphasis go through the de-emphasis filter once per sector
void CdRomAudio::LoadSector()
{
    m_sector_cache_lba = m_state.current_lba;
    m_sector_cache_valid = true;
    m_deemphasis_end_valid = false;

    if (!m_cdrom_media->IsAudioSector(m_state.current_lba) ||
        !m_cdrom_media->ReadSamples(m_state.current_lba, 0, m_sector_cache, CDROM_AUDIO_SECTOR_SAMPLES * 2))
    {
        memset(m_sector_cache, 0, sizeof(m_sector_cache));
        m_state.deemphasis_primed = false;
        return;
    }

    s32 track = m_cdrom_media->FindTrackFromLBA(m_state.current_lba);

    if ((track >= 0) && ((m_cdrom_media->GetTracks()[track].control_flags & k_cdrom_control_pre_emphasis) != 0))
        Deemphasize();
    else
        m_state.deemphasis_primed = false;
}

void CdRomAudio::Deemphasize()
{
    if (!m_state.deemphasis_primed)
    {
        for (int c = 0; c < 2; c++)
        {
            m_state.deemphasis_input[c] = m_sector_cache[c];
            m_state.deemphasis_output[c] = (s64)m_sector_cache[c] * 65536;
        }

        m_state.deemphasis_primed = true;
    }

    for (int c = 0; c < 2; c++)
    {
        s64 input = m_state.deemphasis_input[c];
        s64 output = m_state.deemphasis_output[c];

        for (int i = c; i < CDROM_AUDIO_SECTOR_SAMPLES * 2; i += 2)
        {
            s64 sample = m_sector_cache[i];
            output = (k_cdrom_deemphasis_b0 * sample) + (k_cdrom_deemphasis_b1 * input) +
                ((k_cdrom_deemphasis_a1 * output) >> 16);
            input = sample;
            m_sector_cache[i] = (s16)CLAMP((output + 0x8000) >> 16, -32768, 32767);
        }

        m_deemphasis_end_input[c] = (s32)input;
        m_deemphasis_end_output[c] = output;
    }

    m_deemphasis_end_valid = true;
}

void CdRomAudio::NextSector()
{
    if (m_deemphasis_end_valid)
    {
        for (int c = 0; c < 2; c++)
        {
            m_state.deemphasis_input[c] = m_deemphasis_end_input[c];
            m_state.deemphasis_output[c] = m_deemphasis_end_output[c];
        }

        m_deemphasis_end_valid = false;
    }

    m_state.current_sample = 0;
    m_state.current_lba++;

    if (m_state.current_lba >= m_state.end_lba)
    {
        if (m_state.repeat)
            m_state.current_lba = m_state.start_lba;
        else
            m_state.play_state = CDROM_AUDIO_IDLE;
    }

    m_cdrom_media->SetCurrentSector(m_state.current_lba);
}

void CdRomAudio::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void CdRomAudio::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void CdRomAudio::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_state.play_state);
    G_SERIALIZE(serializer, m_state.start_lba);
    G_SERIALIZE(serializer, m_state.end_lba);
    G_SERIALIZE(serializer, m_state.current_lba);
    G_SERIALIZE(serializer, m_state.current_sample);
    G_SERIALIZE(serializer, m_state.seek_samples);
    G_SERIALIZE(serializer, m_state.repeat);
    G_SERIALIZE(serializer, m_state.deemphasis_primed);
    G_SERIALIZE_ARRAY(serializer, m_state.deemphasis_input, 2);
    G_SERIALIZE_ARRAY(serializer, m_state.deemphasis_output, 2);
}

void CdRomAudio::SanitizeState()
{
    if (m_state.play_state > CDROM_AUDIO_PAUSED)
        m_state.play_state = CDROM_AUDIO_IDLE;

    if (m_state.current_sample >= CDROM_AUDIO_SECTOR_SAMPLES)
        m_state.current_sample = 0;

    m_sector_cache_valid = false;
    m_deemphasis_end_valid = false;

    if (m_state.play_state != CDROM_AUDIO_IDLE)
    {
        s32 track = m_cdrom_media->FindTrackFromLBA(m_state.current_lba);

        if (track >= 0)
            m_cdrom_media->PreloadTrack((u32)track);
    }
}
