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

#ifndef AUDIO_H
#define AUDIO_H

#include "common.h"

class Audio
{
public:
    Audio();
    ~Audio();
    void Init();
    void Reset();
    void Mute(bool mute);
    void SetMasterVolume(float volume);
    void Clock(u64 delta_ns);
    void EndFrame(s16* sample_buffer, int* sample_count);

private:
    bool m_mute;
    float m_master_volume;
};

#include "audio_inline.h"

#endif /* AUDIO_H */