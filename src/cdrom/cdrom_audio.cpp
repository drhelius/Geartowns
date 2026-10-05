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

CdRomAudio::CdRomAudio(CdRomMedia* cdrom_media)
{
    m_cdrom_media = cdrom_media;
    memset(&m_state, 0, sizeof(m_state));
    memset(m_sector_cache, 0, sizeof(m_sector_cache));
    m_sector_cache_lba = 0;
    m_sector_cache_valid = false;
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
    m_sector_cache_valid = false;
}

// The range is half open
// Playback stops with the position resting on its end
void CdRomAudio::Play(u32 start_lba, u32 end_lba, bool repeat)
{
    u32 sector_count = m_cdrom_media->GetSectorCount();
    end_lba = MIN(end_lba, sector_count);

    u32 seek_ms = m_cdrom_media->SeekTime(m_cdrom_media->GetCurrentSector(), start_lba);

    m_state.start_lba = start_lba;
    m_state.end_lba = end_lba;
    m_state.current_lba = start_lba;
    m_state.current_sample = 0;
    m_state.seek_samples = (seek_ms * GT_AUDIO_SAMPLE_RATE) / 1000;
    m_state.repeat = repeat;
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
void CdRomAudio::LoadSector()
{
    m_sector_cache_lba = m_state.current_lba;
    m_sector_cache_valid = true;

    if (!m_cdrom_media->IsAudioSector(m_state.current_lba) ||
        !m_cdrom_media->ReadSamples(m_state.current_lba, 0, m_sector_cache, CDROM_AUDIO_SECTOR_SAMPLES * 2))
        memset(m_sector_cache, 0, sizeof(m_sector_cache));
}

void CdRomAudio::NextSector()
{
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
}

void CdRomAudio::SanitizeState()
{
    if (m_state.play_state > CDROM_AUDIO_PAUSED)
        m_state.play_state = CDROM_AUDIO_IDLE;

    if (m_state.current_sample >= CDROM_AUDIO_SECTOR_SAMPLES)
        m_state.current_sample = 0;

    m_sector_cache_valid = false;

    if (m_state.play_state != CDROM_AUDIO_IDLE)
    {
        s32 track = m_cdrom_media->FindTrackFromLBA(m_state.current_lba);

        if (track >= 0)
            m_cdrom_media->PreloadTrack((u32)track);
    }
}
