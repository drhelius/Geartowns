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

#include <iostream>
#include "common.h"

class Audio;
class Firmware;
class I386;
class Input;
class Media;
class Memory;

class GeartownsCore
{
public:
    GeartownsCore();
    ~GeartownsCore();
    void Init(GT_Pixel_Format pixel_format = GT_PIXEL_RGBA8888);
    GT_Run_Result RunToFrame(u8* frame_buffer, s16* sample_buffer, int* sample_count,
        bool render = true);
    bool LoadBios(const char* directory_path);
    void UnloadBios();
    bool LoadMedia(const char* file_path);
    void ResetMedia();
    void KeyPressed(GT_Keys key);
    void KeyReleased(GT_Keys key);
    void Pause(bool paused);
    bool IsPaused();
    bool SaveState(const char* path = NULL, int index = -1, bool screenshot = false);
    bool SaveState(u8* buffer, size_t& size, bool screenshot = false);
    bool GetMaxSaveStateSize(size_t& size);
    bool LoadState(const char* path = NULL, int index = -1);
    bool LoadState(const u8* buffer, size_t size);
    bool GetSaveStateHeader(int index, const char* path, GT_SaveState_Header* header);
    bool GetSaveStateScreenshot(int index, const char* path, GT_SaveState_Screenshot* screenshot);
    void ResetSound();
    void GetRuntimeInfo(GT_Runtime_Info& runtime_info);
    Firmware* GetFirmware();
    Media* GetMedia();
    Audio* GetAudio();
    Input* GetInput();
    Memory* GetMemory();
    I386* GetI386();

private:
    void Reset();
    bool SaveState(std::ostream& stream, size_t& size, bool screenshot);
    bool LoadState(std::istream& stream);
    std::string GetSaveStatePath(const char* path, int index);

private:
    Audio* m_audio;
    Firmware* m_firmware;
    Input* m_input;
    Media* m_media;
    Memory* m_memory;
    I386* m_cpu;
    bool m_paused;
    GT_Pixel_Format m_pixel_format;
    u8* m_frame_buffer;
};

#include "geartowns_core_inline.h"

#endif /* GEARTOWNS_CORE_H */
