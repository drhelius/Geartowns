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
    void Init(GT_Pixel_Format pixel_format = GT_PIXEL_RGBA8888);
    bool RunFrame(u8* frame_buffer, s16* sample_buffer, int* sample_count, GT_Debug_Run* debug = NULL);
    bool LoadBios(const char* directory_path);
    bool LoadBiosFromBuffer(const u8* buffer, int size);
    bool LoadMedia(const char* file_path);
    void ResetMedia();
    void KeyPressed(GT_Keys key);
    void KeyReleased(GT_Keys key);
    void Pause(bool paused);
    bool IsPaused();
    void SaveRam();
    void SaveRam(const char* path, bool full_path = false);
    void LoadRam();
    void LoadRam(const char* path, bool full_path = false);
    bool SaveState(const char* path = NULL, int index = -1, bool screenshot = false);
    bool SaveState(u8* buffer, size_t& size, bool screenshot = false);
    bool LoadState(const char* path = NULL, int index = -1);
    bool LoadState(const u8* buffer, size_t size);
    bool GetSaveStateHeader(int index, const char* path, GG_SaveState_Header* header);
    bool GetSaveStateScreenshot(int index, const char* path, GG_SaveState_Screenshot* screenshot);
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