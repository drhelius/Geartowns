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

#ifndef GEARTOWNS_CORE_H
#define GEARTOWNS_CORE_H

#include "common.h"

class Audio;
class Input;
class Media;

class GeartownsCore
{
public:
    struct GT_Debug_Run
    {
        bool step_debugger;
        bool stop_on_breakpoint;
        bool stop_on_irq;
        bool stop_on_frame;
    };

public:
    GeartownsCore();
    ~GeartownsCore();
    void Init(GT_Input_Pump_Fn input_pump_fn, GT_Pixel_Format pixel_format = GT_PIXEL_RGBA8888);
    bool RunToFrame(u8* frame_buffer, s16* sample_buffer, int* sample_count, GT_Debug_Run* debug = NULL);
    bool LoadBios(const char* directory_path);
    bool LoadBiosFromBuffer(const u8* buffer, int size);
    void UnloadBios();
    bool LoadMedia(const char* file_path);
    void ResetMedia(bool preserve_cmos);
    void KeyPressed(GT_Keys key);
    void KeyReleased(GT_Keys key);
    void Pause(bool paused);
    bool IsPaused();
    void ResetSound();
    Media* GetMedia();
    Audio* GetAudio();
    Input* GetInput();

private:
    void Reset();

private:
    Audio* m_audio;
    Input* m_input;
    Media* m_media;
    bool m_paused;
};

#include "geartowns_core_inline.h"

#endif /* GEARTOWNS_CORE_H */