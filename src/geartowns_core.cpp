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

#include "geartowns_core.h"
#include "audio.h"
#include "input.h"
#include "media.h"

GeartownsCore::GeartownsCore()
{
    InitPointer(m_audio);
    InitPointer(m_input);
    InitPointer(m_media);
    m_paused = false;
}

GeartownsCore::~GeartownsCore()
{
    SafeDelete(m_audio);
    SafeDelete(m_input);
    SafeDelete(m_media);
}

void GeartownsCore::Init(GT_Input_Pump_Fn input_pump_fn, GT_Pixel_Format pixel_format)
{
    UNUSED(input_pump_fn);
    UNUSED(pixel_format);

    if (!IsValidPointer(m_audio))
        m_audio = new Audio();
    if (!IsValidPointer(m_input))
        m_input = new Input();
    if (!IsValidPointer(m_media))
        m_media = new Media();

    m_audio->Init();
    m_input->Init();
    m_media->Init();
    Reset();
}

bool GeartownsCore::RunToFrame(u8* frame_buffer, s16* sample_buffer, int* sample_count, GT_Debug_Run* debug)
{
    UNUSED(frame_buffer);
    UNUSED(debug);

    if (IsValidPointer(m_audio))
        m_audio->EndFrame(sample_buffer, sample_count);
    else if (sample_count != NULL)
        *sample_count = 0;

    return false;
}

bool GeartownsCore::LoadBios(const char* directory_path)
{
    return IsValidPointer(m_media) && m_media->LoadBios(directory_path);
}

bool GeartownsCore::LoadBiosFromBuffer(const u8* buffer, int size)
{
    return IsValidPointer(m_media) && m_media->LoadBiosFromBuffer(buffer, size);
}

void GeartownsCore::UnloadBios()
{
    if (IsValidPointer(m_media))
        m_media->UnloadBios();
}

bool GeartownsCore::LoadMedia(const char* file_path)
{
    return IsValidPointer(m_media) && m_media->LoadMedia(file_path);
}

void GeartownsCore::ResetMedia(bool preserve_cmos)
{
    UNUSED(preserve_cmos);
    Reset();
}

void GeartownsCore::KeyPressed(GT_Keys key)
{
    if (IsValidPointer(m_input))
        m_input->KeyPressed(key);
}

void GeartownsCore::KeyReleased(GT_Keys key)
{
    if (IsValidPointer(m_input))
        m_input->KeyReleased(key);
}

void GeartownsCore::ResetSound()
{
    if (IsValidPointer(m_audio))
        m_audio->Reset();
}

void GeartownsCore::Reset()
{
    m_paused = false;

    if (IsValidPointer(m_audio))
        m_audio->Reset();
    if (IsValidPointer(m_input))
        m_input->Reset();
}