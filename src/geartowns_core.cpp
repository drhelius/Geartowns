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
#include "audio/audio.h"
#include "media/firmware.h"
#include "input/input.h"
#include "common/memory_stream.h"
#include "common/state_serializer.h"
#include "media/media.h"
#include "system/memory.h"
#include "i386/i386.h"
#include "system/towns_io.h"
#include "system/towns_pic.h"
#include "system/towns_pit.h"

GeartownsCore::GeartownsCore()
{
    InitPointer(m_audio);
    InitPointer(m_firmware);
    InitPointer(m_input);
    InitPointer(m_media);
    InitPointer(m_memory);
    InitPointer(m_i386);
    InitPointer(m_towns_io);
    InitPointer(m_pic);
    InitPointer(m_pit);
    InitPointer(m_frame_buffer);

    m_machine_time = 0;
    m_machine_time_remainder = 0;
    m_paused = false;
    m_pixel_format = GT_PIXEL_RGBA8888;
}

GeartownsCore::~GeartownsCore()
{
    SafeDelete(m_audio);
    SafeDelete(m_input);
    SafeDelete(m_media);
    SafeDelete(m_i386);
    SafeDelete(m_towns_io);
    SafeDelete(m_pic);
    SafeDelete(m_pit);
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

    if (!IsValidPointer(m_i386))
        m_i386 = new I386();

    if (!IsValidPointer(m_towns_io))
        m_towns_io = new TownsIO();

    if (!IsValidPointer(m_pic))
        m_pic = new TownsPIC();

    if (!IsValidPointer(m_pit))
        m_pit = new TownsPIT();

    m_firmware->Init();
    m_memory->Init();
    m_audio->Init();
    m_pic->Init();
    m_pit->Init(m_pic);
    m_towns_io->Init(m_audio, m_pic, m_pit);
    m_i386->Init(m_memory, m_towns_io);
    m_input->Init();
    m_media->Init();
    Reset();
}

GT_Run_Result GeartownsCore::RunToFrame(u8* frame_buffer, s16* sample_buffer, int* sample_count, bool render)
{
    return RunToFrameTemplate<false>(frame_buffer, sample_buffer, sample_count, NULL, render);
}

#if !defined(GT_DISABLE_DISASSEMBLER)
GT_Run_Result GeartownsCore::RunToFrame(u8* frame_buffer, s16* sample_buffer, int* sample_count, GT_Debug_Run* debug,
    bool render)
{
    return RunToFrameTemplate<true>(frame_buffer, sample_buffer, sample_count, debug, render);
}
#endif

template<bool debugger>
GT_Run_Result GeartownsCore::RunToFrameTemplate(u8* frame_buffer, s16* sample_buffer, int* sample_count,
    GT_Debug_Run* debug, bool render)
{
    m_frame_buffer = frame_buffer;
    UNUSED(render);

    if (sample_count != NULL)
        *sample_count = 0;

    if (m_paused)
        return GT_RUN_PAUSED;

    if (!IsValidPointer(m_firmware) || !m_firmware->IsReady())
        return GT_RUN_NOT_READY;

    u64 elapsed_clocks = 0;

#if !defined(GT_DISABLE_DISASSEMBLER)
    if (debugger && IsValidPointer(debug))
    {
        debug->stopped = false;
        debug->breakpoint_hit = false;

        while (elapsed_clocks < GT_CPU_CLOCKS_PER_FRAME)
        {
            I386_Debug_State debug_state;
            m_i386->CopyDebugState(debug_state);
            m_i386->Disassemble(debug_state.eip);

            if (m_i386->CheckDebuggerBreakpoints(debug->stop_on_breakpoint, debug->stop_on_run_to_breakpoint))
            {
                debug->stopped = true;
                debug->breakpoint_hit = true;
                break;
            }

            I386_State before;
            m_i386->CopyState(before);

            GT_Bus_Access_Context context = {};
            context.origin = GT_BUS_ORIGIN_CPU;
            context.time_ns = m_machine_time;
            m_i386->RunInstruction(context);

            I386_Run_Result result = m_i386->GetStepInfo();
            u32 call_return_linear = 0;
            bool call = m_i386->DebugInstructionCompleted(before, result, &call_return_linear);

            if (debug->step_over)
            {
                debug->step_over = false;
                debug->step_debugger = !call;

                if (call)
                    m_i386->AddRunToBreakpoint(call_return_linear);
            }

            u32 step_clocks = (u32)(result.clocks + context.wait_clocks);

            // A halted CPU idles until the next event
            if (result.steps == 0 && m_i386->Halted())
                step_clocks = GetBatchBudget(elapsed_clocks);

            elapsed_clocks += CompleteBatch(step_clocks, context);

            if (debug->step_debugger || m_i386->Shutdown())
            {
                debug->stopped = true;
                break;
            }
        }
    }
    else
#else
    UNUSED(debug);
#endif
    {
        while (elapsed_clocks < GT_CPU_CLOCKS_PER_FRAME)
        {
            GT_Bus_Access_Context context = {};
            context.origin = GT_BUS_ORIGIN_CPU;
            context.time_ns = m_machine_time;

            u32 budget = GetBatchBudget(elapsed_clocks);
            I386_Run_Result result = m_i386->RunFor(budget, context, false, m_pic->IsInterruptPending());
            u32 step_clocks = (u32)(result.clocks + context.wait_clocks);

            // A halted CPU idles until the next event
            if (result.steps == 0 && m_i386->Halted())
                step_clocks = budget;

            elapsed_clocks += CompleteBatch(step_clocks, context);

            if (m_i386->Shutdown())
                break;
        }
    }

    // Debugger memory views refresh once per executed frame or debugger step
    if (elapsed_clocks != 0)
        m_memory->InvalidateDebugSnapshot();

    if (IsValidPointer(m_audio))
        m_audio->EndFrame(sample_buffer, sample_count);
    else if (sample_count != NULL)
        *sample_count = 0;

    return GT_RUN_FRAME_READY;
}

// Batches stop at the next device event so its IRQ is sampled on time
u32 GeartownsCore::GetBatchBudget(u64 elapsed_clocks) const
{
    u64 budget = GT_CPU_CLOCKS_PER_FRAME - elapsed_clocks;
    u64 next_event = m_pit->GetNextEventTime();

    if (next_event == GT_NO_EVENT)
        return (u32)budget;

    u64 clocks = 1;

    if (next_event > m_machine_time)
    {
        u64 scaled = (next_event - m_machine_time) * GT_CPU_CLOCK_RATE - m_machine_time_remainder;
        clocks = (scaled + 999999999ULL) / 1000000000ULL;
    }

    return (u32)MIN(budget, clocks);
}

void GeartownsCore::AdvanceMachineTime(u32 clocks)
{
    u64 scaled = (u64)clocks * 1000000000ULL + m_machine_time_remainder;
    m_machine_time += scaled / GT_CPU_CLOCK_RATE;
    m_machine_time_remainder = (u32)(scaled % GT_CPU_CLOCK_RATE);
}

// Devices catch up with the batch before the CPU samples INTR at its boundary
u32 GeartownsCore::CompleteBatch(u32 clocks, GT_Bus_Access_Context& context)
{
    AdvanceMachineTime(clocks);
    m_pit->Synchronize(m_machine_time);

    if (m_pic->IsInterruptPending() && m_i386->CanAcceptMaskableInterrupt())
    {
        u32 interrupt_clocks = m_i386->EnterExternalInterrupt(m_pic->AcknowledgeInterrupt(), context);
        AdvanceMachineTime(interrupt_clocks);
        clocks += interrupt_clocks;
    }

    m_audio->Clock(clocks);
    return clocks;
}

bool GeartownsCore::LoadBios(const char* directory_path)
{
    if (!IsValidPointer(m_firmware) || !m_firmware->LoadDirectory(directory_path))
        return false;

#if !defined(GT_DISABLE_DISASSEMBLER)
    if (IsValidPointer(m_i386))
        m_i386->ResetDisassembler();
#endif

    Reset();
    return true;
}

void GeartownsCore::UnloadBios()
{
    if (IsValidPointer(m_firmware))
        m_firmware->Unload();

#if !defined(GT_DISABLE_DISASSEMBLER)
    if (IsValidPointer(m_i386))
        m_i386->ResetDisassembler();
#endif
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

    StateSerializer serializer(stream);
    Serialize(serializer);
    m_i386->SaveState(stream);
    m_audio->SaveState(stream);
    m_input->SaveState(stream);
    m_pic->SaveState(stream);
    m_pit->SaveState(stream);

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

    if (header.version != GT_SAVESTATE_VERSION)
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

    StateSerializer serializer(stream);
    Serialize(serializer);
    m_machine_time_remainder %= GT_CPU_CLOCK_RATE;
    m_i386->LoadState(stream);
    m_audio->LoadState(stream);
    m_input->LoadState(stream);
    m_pic->LoadState(stream);
    m_pit->LoadState(stream);

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

bool GeartownsCore::GetSaveStateScreenshot(int index, const char* path, GT_SaveState_Screenshot* screenshot)
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

void GeartownsCore::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_machine_time);
    G_SERIALIZE(serializer, m_machine_time_remainder);
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
    m_machine_time = 0;
    m_machine_time_remainder = 0;

    if (IsValidPointer(m_memory))
        m_memory->Reset();

    if (IsValidPointer(m_towns_io))
        m_towns_io->Reset();

    if (IsValidPointer(m_pic))
        m_pic->Reset();

    if (IsValidPointer(m_pit))
        m_pit->Reset();

    InitMemoryMap();

    if (IsValidPointer(m_i386))
        m_i386->Reset();

    if (IsValidPointer(m_audio))
        m_audio->Reset();

    if (IsValidPointer(m_input))
        m_input->Reset();
}

void GeartownsCore::InitMemoryMap()
{
    if (!IsValidPointer(m_memory) || !IsValidPointer(m_firmware) || !m_firmware->IsReady())
        return;

    u32 rom_flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_EXECUTABLE | GT_DEBUG_REGION_ROM | GT_DEBUG_REGION_MAPPED;
    u32 ram_flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_WRITABLE | GT_DEBUG_REGION_EXECUTABLE |
        GT_DEBUG_REGION_MAPPED;
    const u8* system_rom = m_firmware->GetSystemRom();
    const u8* boot_rom = system_rom + GT_FIRMWARE_SYSTEM_SIZE - GT_FIRMWARE_SYSTEM_BOOT_SIZE;
    u8* main_ram = m_memory->GetMainRAM();

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_SYSTEM_ROM, "System ROM", system_rom, NULL,
        GT_FIRMWARE_SYSTEM_SIZE, 0xFFFC0000U, rom_flags))
        Error("Unable to map the system ROM");

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_SYSTEM_ROM_LOW_ALIAS, "System ROM (low boot window)", boot_rom,
        NULL, GT_FIRMWARE_SYSTEM_BOOT_SIZE, 0x000F8000U, rom_flags))
        Error("Unable to map the low system ROM alias");

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_MAIN_RAM, "Main RAM", main_ram, main_ram, GT_MAIN_RAM_SIZE, 0,
        ram_flags))
        Error("Unable to map main RAM");
}
