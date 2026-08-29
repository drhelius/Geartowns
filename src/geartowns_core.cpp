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

#include <fstream>
#include <sstream>
#include <time.h>
#include "geartowns_core.h"
#include "audio.h"
#include "firmware.h"
#include "input.h"
#include "memory_stream.h"
#include "media.h"
#include "memory.h"
#include "i386.h"

GeartownsCore::GeartownsCore()
{
    InitPointer(m_audio);
    InitPointer(m_firmware);
    InitPointer(m_input);
    InitPointer(m_media);
    InitPointer(m_memory);
    InitPointer(m_cpu);
    InitPointer(m_frame_buffer);
    m_paused = false;
    m_pixel_format = GT_PIXEL_RGBA8888;
}

GeartownsCore::~GeartownsCore()
{
    SafeDelete(m_audio);
    SafeDelete(m_input);
    SafeDelete(m_media);
    SafeDelete(m_cpu);
    SafeDelete(m_memory);
    SafeDelete(m_firmware);
}

void GeartownsCore::Init(GT_Pixel_Format pixel_format)
{
    m_pixel_format = pixel_format;

    if (!IsValidPointer(m_audio))
        m_audio = new Audio();
    if (!IsValidPointer(m_firmware))
        m_firmware = new Firmware();
    if (!IsValidPointer(m_input))
        m_input = new Input();
    if (!IsValidPointer(m_media))
        m_media = new Media();
    if (!IsValidPointer(m_memory))
        m_memory = new Memory();
    if (!IsValidPointer(m_cpu))
        m_cpu = new I386();

    m_firmware->Init();
    m_memory->Init();
    m_cpu->Init(m_memory);
    m_audio->Init();
    m_input->Init();
    m_media->Init();
    Reset();
}

GT_Run_Result GeartownsCore::RunToFrame(u8* frame_buffer, s16* sample_buffer,
    int* sample_count, bool render)
{
    m_frame_buffer = frame_buffer;
    UNUSED(render);

    if (IsValidPointer(m_audio))
        m_audio->EndFrame(sample_buffer, sample_count);
    else if (sample_count != NULL)
        *sample_count = 0;

    if (m_paused)
        return GT_RUN_PAUSED;
    if (!IsValidPointer(m_firmware) || !m_firmware->IsReady())
        return GT_RUN_NOT_READY;

    return GT_RUN_FRAME_READY;
}

bool GeartownsCore::LoadBios(const char* directory_path)
{
    if (!IsValidPointer(m_firmware) || !m_firmware->LoadDirectory(directory_path))
        return false;

    Reset();
    return true;
}

void GeartownsCore::UnloadBios()
{
    if (IsValidPointer(m_firmware))
        m_firmware->Unload();
}

bool GeartownsCore::LoadMedia(const char* file_path)
{
    return IsValidPointer(m_media) && m_media->LoadMedia(file_path);
}

void GeartownsCore::ResetMedia()
{
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

bool GeartownsCore::SaveState(const char* path, int index, bool screenshot)
{
    using namespace std;

    string full_path = GetSaveStatePath(path, index);
    Debug("Saving state to %s...", full_path.c_str());

    ofstream stream;
    open_ofstream_utf8(stream, full_path.c_str(), ios::out | ios::binary);

    if (!stream.is_open())
    {
        Error("Failed to open save state file for writing: %s", full_path.c_str());
        return false;
    }

    size_t size = 0;
    if (!SaveState(stream, size, screenshot))
    {
        stream.close();
        Error("Failed to save state to file: %s", full_path.c_str());
        return false;
    }

    stream.close();

    if (!stream.good())
    {
        Error("Failed to write save state file: %s", full_path.c_str());
        return false;
    }

    Log("Saved state to %s", full_path.c_str());
    return true;
}

bool GeartownsCore::SaveState(u8* buffer, size_t& size, bool screenshot)
{
    using namespace std;

    Debug("Saving state to buffer [%zu bytes]...", size);

    if (!IsValidPointer(m_media) || !m_media->IsReady())
    {
        Error("Media is not ready when trying to save state");
        return false;
    }

    if (!IsValidPointer(buffer))
    {
        stringstream stream;
        if (!SaveState(stream, size, screenshot))
        {
            Error("Failed to save state to stream to calculate size");
            return false;
        }
        return true;
    }

    memory_stream direct_stream(reinterpret_cast<char*>(buffer), size);

    if (!SaveState(direct_stream, size, screenshot))
    {
        Error("Failed to save state to buffer");
        return false;
    }

    if (!direct_stream.good())
    {
        Error("Failed to save state to buffer: output buffer is too small");
        return false;
    }

    size = direct_stream.size();
    return true;
}

bool GeartownsCore::GetMaxSaveStateSize(size_t& size)
{
    return SaveState(NULL, size, false);
}

bool GeartownsCore::SaveState(std::ostream& stream, size_t& size, bool screenshot)
{
    if (!IsValidPointer(m_media) || !m_media->IsReady())
    {
        Error("Media is not ready when trying to save state");
        return false;
    }

    Debug("Serializing save state...");

    m_audio->SaveState(stream);
    m_input->SaveState(stream);

    if (stream.fail())
    {
        Error("Failed to serialize save state");
        return false;
    }

#if defined(__LIBRETRO__)
    GT_SaveState_Header_Libretro header;
    header.magic = GT_SAVESTATE_MAGIC;
    header.version = GT_SAVESTATE_VERSION;
    Debug("Save state header magic: 0x%08X", header.magic);
    Debug("Save state header version: %u", header.version);
#else
    GT_SaveState_Header header;
    header.magic = GT_SAVESTATE_MAGIC;
    header.version = GT_SAVESTATE_VERSION;
    header.timestamp = (s64)time(NULL);
    strncpy_fit(header.rom_name, m_media->GetFileName(), sizeof(header.rom_name));
    header.rom_crc = m_media->GetCRC();
    strncpy_fit(header.emu_build, GT_VERSION, sizeof(header.emu_build));

    Debug("Save state header magic: 0x%08X", header.magic);
    Debug("Save state header version: %u", header.version);
    Debug("Save state header timestamp: %lld", (long long)header.timestamp);
    Debug("Save state header rom name: %s", header.rom_name);
    Debug("Save state header rom crc: 0x%08X", header.rom_crc);
    Debug("Save state header emu build: %s", header.emu_build);

    if (screenshot && IsValidPointer(m_frame_buffer))
    {
        header.screenshot_width = GT_FRAME_BUFFER_WIDTH;
        header.screenshot_height = GT_FRAME_BUFFER_HEIGHT;
        int bytes_per_pixel = m_pixel_format == GT_PIXEL_RGBA8888 ? 4 : 2;
        header.screenshot_size = header.screenshot_width * header.screenshot_height * bytes_per_pixel;
        stream.write(reinterpret_cast<const char*>(m_frame_buffer), header.screenshot_size);
    }
    else
    {
        header.screenshot_size = 0;
        header.screenshot_width = 0;
        header.screenshot_height = 0;
    }

    Debug("Save state header screenshot size: %u", header.screenshot_size);
    Debug("Save state header screenshot width: %u", header.screenshot_width);
    Debug("Save state header screenshot height: %u", header.screenshot_height);
#endif

    std::streampos position = stream.tellp();
    if (position == std::streampos(-1))
    {
        Error("Failed to calculate save state size");
        return false;
    }

    size = static_cast<size_t>(position) + sizeof(header);

#if !defined(__LIBRETRO__)
    header.size = static_cast<u32>(size);
    Debug("Save state header size: %u", header.size);
#endif

    stream.write(reinterpret_cast<const char*>(&header), sizeof(header));

    if (stream.fail())
    {
        Error("Failed to write save state header");
        return false;
    }

    return true;
}

bool GeartownsCore::LoadState(const char* path, int index)
{
    using namespace std;

    bool ret = false;
    string full_path = GetSaveStatePath(path, index);
    Debug("Loading state from %s...", full_path.c_str());

    ifstream stream;
    open_ifstream_utf8(stream, full_path.c_str(), ios::in | ios::binary);

    if (!stream.fail())
    {
        ret = LoadState(stream);

        if (ret)
            Log("Loaded state from %s", full_path.c_str());
        else
            Error("Failed to load state from %s", full_path.c_str());
    }
    else
        Error("Load state file doesn't exist: %s", full_path.c_str());

    stream.close();
    return ret;
}

bool GeartownsCore::LoadState(const u8* buffer, size_t size)
{
    Debug("Loading state from buffer [%zu bytes]...", size);

    if (!IsValidPointer(m_media) || !m_media->IsReady())
    {
        Error("Media is not ready when trying to load state");
        return false;
    }

    if (!IsValidPointer(buffer) || size == 0)
    {
        Error("Invalid load state buffer");
        return false;
    }

    memory_input_stream direct_stream(reinterpret_cast<const char*>(buffer), size);
    return LoadState(direct_stream);
}

bool GeartownsCore::LoadState(std::istream& stream)
{
    using namespace std;

    if (!IsValidPointer(m_media) || !m_media->IsReady())
    {
        Error("Media is not ready when trying to load state");
        return false;
    }

    GT_SaveState_Header_Libretro header = {};
#if !defined(__LIBRETRO__)
    bool is_desktop_savestate = false;
#endif

    stream.seekg(0, ios::end);
    size_t size = static_cast<size_t>(stream.tellg());

    GT_SaveState_Header desktop_header = {};
    if (size >= sizeof(desktop_header))
    {
        stream.seekg(size - sizeof(desktop_header), ios::beg);
        stream.read(reinterpret_cast<char*>(&desktop_header), sizeof(desktop_header));

        if (desktop_header.magic == GT_SAVESTATE_MAGIC)
        {
            header.magic = desktop_header.magic;
            header.version = desktop_header.version;
#if !defined(__LIBRETRO__)
            is_desktop_savestate = true;
#endif
            Debug("Loading desktop save state");
        }
    }

    if (header.magic != GT_SAVESTATE_MAGIC && size >= sizeof(header))
    {
        stream.seekg(size - sizeof(header), ios::beg);
        stream.read(reinterpret_cast<char*>(&header), sizeof(header));
    }

    stream.clear();
    stream.seekg(0, ios::beg);

    Debug("Load state header magic: 0x%08X", header.magic);
    Debug("Load state header version: %u", header.version);

    if (header.magic != GT_SAVESTATE_MAGIC)
    {
        Log("Invalid save state: 0x%08X", header.magic);
        return false;
    }

    if (header.version < GT_SAVESTATE_MIN_VERSION || header.version > GT_SAVESTATE_VERSION)
    {
        Error("Invalid save state version: %u", header.version);
        return false;
    }

#if !defined(__LIBRETRO__)
    if (is_desktop_savestate)
    {
        Debug("Load state header size: %u", desktop_header.size);
        Debug("Load state header timestamp: %lld", (long long)desktop_header.timestamp);
        Debug("Load state header rom name: %s", desktop_header.rom_name);
        Debug("Load state header rom crc: 0x%08X", desktop_header.rom_crc);
        Debug("Load state header screenshot size: %u", desktop_header.screenshot_size);
        Debug("Load state header screenshot width: %u", desktop_header.screenshot_width);
        Debug("Load state header screenshot height: %u", desktop_header.screenshot_height);
        Debug("Load state header emu build: %s", desktop_header.emu_build);

        if (desktop_header.size != size)
        {
            Error("Invalid save state size: %u", desktop_header.size);
            return false;
        }

        if (desktop_header.rom_crc != m_media->GetCRC())
        {
            Error("Invalid save state media crc: 0x%08X", desktop_header.rom_crc);
            return false;
        }
    }
#endif

    Debug("Unserializing save state...");

    m_audio->LoadState(stream);
    m_input->LoadState(stream);

    if (stream.fail())
    {
        Error("Failed to unserialize save state");
        return false;
    }

    return true;
}

bool GeartownsCore::GetSaveStateHeader(int index, const char* path, GT_SaveState_Header* header)
{
    using namespace std;

    if (!IsValidPointer(header))
        return false;

    string full_path = GetSaveStatePath(path, index);
    Debug("Loading state header from %s...", full_path.c_str());

    ifstream stream;
    open_ifstream_utf8(stream, full_path.c_str(), ios::in | ios::binary);

    if (stream.fail())
    {
        Debug("Save state file doesn't exist: %s", full_path.c_str());
        stream.close();
        return false;
    }

    stream.seekg(0, ios::end);
    size_t savestate_size = static_cast<size_t>(stream.tellg());

    if (savestate_size < sizeof(GT_SaveState_Header))
    {
        Error("Invalid save state file size: %zu", savestate_size);
        stream.close();
        return false;
    }

    stream.seekg(savestate_size - sizeof(GT_SaveState_Header), ios::beg);
    stream.read(reinterpret_cast<char*>(header), sizeof(GT_SaveState_Header));

    if (stream.fail())
    {
        Error("Failed to read save state header from %s", full_path.c_str());
        stream.close();
        return false;
    }

    stream.close();

    if (header->magic != GT_SAVESTATE_MAGIC)
    {
        Error("Invalid save state magic: 0x%08X", header->magic);
        return false;
    }

    if (header->size != savestate_size)
    {
        Error("Invalid save state size: %u", header->size);
        return false;
    }

    return true;
}

bool GeartownsCore::GetSaveStateScreenshot(int index, const char* path,
    GT_SaveState_Screenshot* screenshot)
{
    using namespace std;

    if (!IsValidPointer(screenshot) || !IsValidPointer(screenshot->data) || screenshot->size == 0)
    {
        Error("Invalid save state screenshot buffer");
        return false;
    }

    string full_path = GetSaveStatePath(path, index);
    Debug("Loading state screenshot from %s...", full_path.c_str());

    ifstream stream;
    open_ifstream_utf8(stream, full_path.c_str(), ios::in | ios::binary);

    if (stream.fail())
    {
        Error("Save state file doesn't exist: %s", full_path.c_str());
        stream.close();
        return false;
    }

    GT_SaveState_Header header;
    if (!GetSaveStateHeader(index, path, &header))
    {
        Error("Invalid save state header");
        stream.close();
        return false;
    }

    if (header.screenshot_size == 0)
    {
        Debug("No screenshot data");
        stream.close();
        return false;
    }

    if (screenshot->size < header.screenshot_size)
    {
        Error("Invalid screenshot buffer size %u < %u", screenshot->size, header.screenshot_size);
        stream.close();
        return false;
    }

    if (header.size < sizeof(header) + header.screenshot_size)
    {
        Error("Invalid screenshot offset");
        stream.close();
        return false;
    }

    screenshot->size = header.screenshot_size;
    screenshot->width = header.screenshot_width;
    screenshot->height = header.screenshot_height;

    stream.seekg(header.size - sizeof(header) - screenshot->size, ios::beg);
    stream.read(reinterpret_cast<char*>(screenshot->data), screenshot->size);
    bool success = !stream.fail();
    stream.close();
    return success;
}

std::string GeartownsCore::GetSaveStatePath(const char* path, int index)
{
    using namespace std;

    if (index < 0)
    {
        if (IsValidPointer(path))
            return path;

        string full_path = m_media->GetFilePath();
        string::size_type dot_index = full_path.rfind('.');

        if (dot_index != string::npos)
            full_path.replace(dot_index + 1, full_path.length() - dot_index - 1, "state");

        return full_path;
    }

    string full_path;

    if (IsValidPointer(path))
    {
        full_path = path;
        append_path_component(full_path, m_media->GetFileName());
    }
    else
        full_path = m_media->GetFilePath();

    string::size_type dot_index = full_path.rfind('.');

    if (dot_index != string::npos)
        full_path.replace(dot_index + 1, full_path.length() - dot_index - 1, "state");

    stringstream ss;
    ss << index;
    full_path += ss.str();
    return full_path;
}

void GeartownsCore::GetRuntimeInfo(GT_Runtime_Info& runtime_info)
{
    runtime_info.screen_width = GT_FRAME_BUFFER_WIDTH;
    runtime_info.screen_height = GT_FRAME_BUFFER_HEIGHT;
    runtime_info.width_scale = 1;
    runtime_info.sample_rate = GT_AUDIO_SAMPLE_RATE;
    runtime_info.media_ready = IsValidPointer(m_media) && m_media->IsReady();
    runtime_info.bios_ready = IsValidPointer(m_firmware) && m_firmware->IsReady();
    runtime_info.paused = m_paused;
}

void GeartownsCore::Reset()
{
    m_paused = false;

    if (IsValidPointer(m_memory))
        m_memory->Reset();
    if (IsValidPointer(m_cpu))
        m_cpu->Reset();
    if (IsValidPointer(m_audio))
        m_audio->Reset();
    if (IsValidPointer(m_input))
        m_input->Reset();
}
