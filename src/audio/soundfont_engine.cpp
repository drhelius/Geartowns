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

#include "soundfont_engine.h"

#if !defined(GT_DISABLE_SOUNDFONT)

#include <stdio.h>
#include <fluidsynth.h>
#include "../media/media_file.h"

static const int k_soundfont_engine_polyphony = 256;

static bool soundfont_engine_logging = false;

static void log_message(int level, const char* message, void* data)
{
    UNUSED(level);
    UNUSED(data);
    Log("SoundFont synth: %s", message);
}

static void* file_open(const char* path)
{
    return MediaFile::OpenFile(path);
}

static int file_read(void* buffer, fluid_long_long_t count, void* handle)
{
    MediaFile* file = (MediaFile*)handle;
    return (count >= 0) && file->ReadExact(buffer, (u64)count) ? FLUID_OK : FLUID_FAILED;
}

static int file_seek(void* handle, fluid_long_long_t offset, int origin)
{
    MediaFile* file = (MediaFile*)handle;
    s64 position = offset;

    if (origin == SEEK_CUR)
        position += file->Tell();
    else if (origin == SEEK_END)
        position += file->GetSize();

    return (position >= 0) && (position <= file->GetSize()) && file->Seek(position) ? FLUID_OK : FLUID_FAILED;
}

static int file_close(void* handle)
{
    MediaFile* file = (MediaFile*)handle;
    SafeDelete(file);
    return FLUID_OK;
}

static fluid_long_long_t file_tell(void* handle)
{
    MediaFile* file = (MediaFile*)handle;
    return file->Tell();
}

SoundFontEngine::SoundFontEngine()
{
    InitPointer(m_settings);
    InitPointer(m_synth);
}

SoundFontEngine::~SoundFontEngine()
{
    Close();
}

bool SoundFontEngine::IsAvailable()
{
    return true;
}

bool SoundFontEngine::Open(const char* path)
{
    Close();

    if (!soundfont_engine_logging)
    {
        soundfont_engine_logging = true;
        fluid_set_log_function(FLUID_PANIC, log_message, NULL);
        fluid_set_log_function(FLUID_ERR, log_message, NULL);
        fluid_set_log_function(FLUID_WARN, NULL, NULL);
        fluid_set_log_function(FLUID_INFO, NULL, NULL);
        fluid_set_log_function(FLUID_DBG, NULL, NULL);
    }

    m_settings = new_fluid_settings();

    if (!IsValidPointer(m_settings))
        return false;

    fluid_settings_setnum(m_settings, "synth.sample-rate", (double)GT_AUDIO_SAMPLE_RATE);
    fluid_settings_setint(m_settings, "synth.polyphony", k_soundfont_engine_polyphony);
    fluid_settings_setint(m_settings, "synth.threadsafe-api", 0);
    m_synth = new_fluid_synth(m_settings);

    if (!IsValidPointer(m_synth))
    {
        Close();
        return false;
    }

    fluid_sfloader_t* loader = new_fluid_defsfloader(m_settings);

    if (IsValidPointer(loader))
    {
        fluid_sfloader_set_callbacks(loader, file_open, file_read, file_seek, file_tell, file_close);
        fluid_synth_add_sfloader(m_synth, loader);
    }

    if (fluid_synth_sfload(m_synth, path, 1) == FLUID_FAILED)
    {
        Close();
        return false;
    }

    return true;
}

void SoundFontEngine::Close()
{
    if (IsValidPointer(m_synth))
    {
        delete_fluid_synth(m_synth);
        InitPointer(m_synth);
    }

    if (IsValidPointer(m_settings))
    {
        delete_fluid_settings(m_settings);
        InitPointer(m_settings);
    }
}

void SoundFontEngine::Reset()
{
    if (IsValidPointer(m_synth))
        fluid_synth_system_reset(m_synth);
}

bool SoundFontEngine::IsOpen() const
{
    return IsValidPointer(m_synth);
}

void SoundFontEngine::SetGain(float gain)
{
    if (IsValidPointer(m_synth))
        fluid_synth_set_gain(m_synth, gain);
}

u32 SoundFontEngine::Render(const MidiOut::MidiOut_Message* messages, int count, const u8* data, s16* buffer,
    int frames)
{
    int position = 0;

    for (int i = 0; i < count; i++)
    {
        const MidiOut::MidiOut_Message& message = messages[i];

        if (message.offset != k_midi_out_immediate)
        {
            int target = MIN((int)message.offset, frames);

            if (target > position)
            {
                Write(buffer + (position * 2), target - position);
                position = target;
            }
        }

        Play(data + message.data, message.size);
    }

    if (position < frames)
        Write(buffer + (position * 2), frames - position);

    return 0;
}

int SoundFontEngine::GetVoices() const
{
    return IsValidPointer(m_synth) ? fluid_synth_get_active_voice_count(m_synth) : 0;
}

int SoundFontEngine::GetMaxVoices() const
{
    return IsValidPointer(m_synth) ? fluid_synth_get_polyphony(m_synth) : 0;
}

void SoundFontEngine::Play(const u8* data, int size)
{
    u8 status = data[0];

    if (status == 0xF0)
    {
        int length = size - (data[size - 1] == 0xF7 ? 2 : 1);

        if (length > 0)
            fluid_synth_sysex(m_synth, (const char*)data + 1, length, NULL, NULL, NULL, 0);

        return;
    }

    if (status == 0xFF)
    {
        fluid_synth_system_reset(m_synth);
        return;
    }

    if (status >= 0xF0)
        return;

    int channel = status & 0x0F;
    int data1 = size > 1 ? data[1] : 0;
    int data2 = size > 2 ? data[2] : 0;

    switch (status & 0xF0)
    {
        case 0x80:
            fluid_synth_noteoff(m_synth, channel, data1);
            break;
        case 0x90:
            fluid_synth_noteon(m_synth, channel, data1, data2);
            break;
        case 0xA0:
            fluid_synth_key_pressure(m_synth, channel, data1, data2);
            break;
        case 0xB0:
            fluid_synth_cc(m_synth, channel, data1, data2);
            break;
        case 0xC0:
            fluid_synth_program_change(m_synth, channel, data1);
            break;
        case 0xD0:
            fluid_synth_channel_pressure(m_synth, channel, data1);
            break;
        case 0xE0:
            fluid_synth_pitch_bend(m_synth, channel, data1 | (data2 << 7));
            break;
        default:
            break;
    }
}

void SoundFontEngine::Write(s16* buffer, int frames)
{
    fluid_synth_write_s16(m_synth, frames, buffer, 0, 2, buffer, 1, 2);
}

#else

SoundFontEngine::SoundFontEngine()
{
    InitPointer(m_settings);
    InitPointer(m_synth);
}

SoundFontEngine::~SoundFontEngine()
{
}

bool SoundFontEngine::IsAvailable()
{
    return false;
}

bool SoundFontEngine::Open(const char* path)
{
    UNUSED(path);
    return false;
}

void SoundFontEngine::Close()
{
}

void SoundFontEngine::Reset()
{
}

bool SoundFontEngine::IsOpen() const
{
    return false;
}

void SoundFontEngine::SetGain(float gain)
{
    UNUSED(gain);
}

u32 SoundFontEngine::Render(const MidiOut::MidiOut_Message* messages, int count, const u8* data, s16* buffer,
    int frames)
{
    UNUSED(messages);
    UNUSED(count);
    UNUSED(data);
    memset(buffer, 0, sizeof(s16) * frames * 2);
    return 0;
}

int SoundFontEngine::GetVoices() const
{
    return 0;
}

int SoundFontEngine::GetMaxVoices() const
{
    return 0;
}

void SoundFontEngine::Play(const u8* data, int size)
{
    UNUSED(data);
    UNUSED(size);
}

void SoundFontEngine::Write(s16* buffer, int frames)
{
    UNUSED(buffer);
    UNUSED(frames);
}

#endif
