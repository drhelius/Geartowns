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

#ifndef CDROM_AUDIO_H
#define CDROM_AUDIO_H

#include <iostream>
#include "../common/common.h"

#define CDROM_AUDIO_SECTOR_SAMPLES 588

class CdRomMedia;
class StateSerializer;

class CdRomAudio
{
public:
    enum CdRomAudio_Play_State
    {
        CDROM_AUDIO_IDLE,
        CDROM_AUDIO_PLAYING,
        CDROM_AUDIO_PAUSED
    };

    struct CdRomAudio_State
    {
        CdRomAudio_Play_State play_state;
        u32 start_lba;
        u32 end_lba;
        u32 current_lba;
        u32 current_sample;
        u32 seek_samples;
        bool repeat;
    };

public:
    CdRomAudio(CdRomMedia* cdrom_media);
    ~CdRomAudio();
    void Init();
    void Reset();
    void Play(u32 start_lba, u32 end_lba, bool repeat);
    void Pause();
    void Resume();
    void Stop();
    bool IsPlaying() const;
    bool IsPaused() const;
    u32 GetCurrentLBA() const;
    void Sample(s16& left, s16& right);
    CdRomAudio_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void LoadSector();
    void NextSector();
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    CdRomMedia* m_cdrom_media;
    CdRomAudio_State m_state;
    s16 m_sector_cache[CDROM_AUDIO_SECTOR_SAMPLES * 2];
    u32 m_sector_cache_lba;
    bool m_sector_cache_valid;
};

#include "cdrom_audio_inline.h"

#endif /* CDROM_AUDIO_H */
