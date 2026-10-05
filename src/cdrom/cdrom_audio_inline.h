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

#ifndef CDROM_AUDIO_INLINE_H
#define CDROM_AUDIO_INLINE_H

#include "cdrom_audio.h"

INLINE bool CdRomAudio::IsPlaying() const
{
    return m_state.play_state == CDROM_AUDIO_PLAYING;
}

INLINE bool CdRomAudio::IsPaused() const
{
    return m_state.play_state == CDROM_AUDIO_PAUSED;
}

INLINE u32 CdRomAudio::GetCurrentLBA() const
{
    return m_state.current_lba;
}

// The mixer calls it once per output sample at the 44.1 kHz disc rate
INLINE void CdRomAudio::Sample(s16& left, s16& right)
{
    left = 0;
    right = 0;

    if (m_state.play_state != CDROM_AUDIO_PLAYING)
        return;

    // The pickup is still moving to the start of the range
    if (m_state.seek_samples > 0)
    {
        m_state.seek_samples--;
        return;
    }

    if (!m_sector_cache_valid || (m_sector_cache_lba != m_state.current_lba))
        LoadSector();

    u32 offset = m_state.current_sample * 2;
    left = m_sector_cache[offset + 0];
    right = m_sector_cache[offset + 1];

    m_state.current_sample++;

    if (m_state.current_sample == CDROM_AUDIO_SECTOR_SAMPLES)
        NextSector();
}

INLINE CdRomAudio::CdRomAudio_State* CdRomAudio::GetState()
{
    return &m_state;
}

#endif /* CDROM_AUDIO_INLINE_H */
