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
#include "cdrom/cdrom.h"
#include "cdrom/cdrom_audio.h"
#include "cdrom/cdrom_media.h"
#include "drive/fdc.h"
#include "media/firmware.h"
#include "input/input.h"
#include "input/keyboard.h"
#include "common/memory_stream.h"
#include "common/state_serializer.h"
#include "media/media.h"
#include "system/memory.h"
#include "i386/i386.h"
#include "system/io.h"
#include "system/pic.h"
#include "system/pit.h"
#include "system/msm58321.h"
#include "system/system_control.h"
#include "system/scheduler.h"
#include "system/machine_profiles.h"
#include "system/upd71071.h"
#include "video/video.h"

GeartownsCore::GeartownsCore()
{
    InitPointer(m_audio);
    InitPointer(m_firmware);
    InitPointer(m_input);
    InitPointer(m_media);
    InitPointer(m_memory);
    InitPointer(m_i386);
    InitPointer(m_io);
    InitPointer(m_pic);
    InitPointer(m_pit);
    InitPointer(m_system_control);
    InitPointer(m_scheduler);
    InitPointer(m_video);
    InitPointer(m_cdrom_media);
    InitPointer(m_cdrom_audio);
    InitPointer(m_cdrom);
    InitPointer(m_fdc);
    InitPointer(m_keyboard);
    InitPointer(m_rtc);
    InitPointer(m_dma);
    InitPointer(m_frame_buffer);

    m_machine_config.model = GT_MACHINE_MODEL1_2;
    m_machine_config.cpu = GT_MACHINE_CPU_80386DX;
    m_machine_config.ram_size = GT_MAIN_RAM_SIZE;
    m_machine_config.floppy_drives = 2;
    m_machine_config.cpu_clock_rate = GT_CPU_CLOCK_RATE;
    m_pending_machine_config = m_machine_config;
    m_powered = false;
    m_paused = false;
    m_pixel_format = GT_PIXEL_RGBA8888;
}

GeartownsCore::~GeartownsCore()
{
    SafeDelete(m_audio);
    SafeDelete(m_input);
    SafeDelete(m_media);
    SafeDelete(m_i386);
    SafeDelete(m_io);
    SafeDelete(m_pic);
    SafeDelete(m_pit);
    SafeDelete(m_system_control);
    SafeDelete(m_scheduler);
    SafeDelete(m_video);
    SafeDelete(m_cdrom);
    SafeDelete(m_cdrom_audio);
    SafeDelete(m_fdc);
    SafeDelete(m_keyboard);
    SafeDelete(m_rtc);
    SafeDelete(m_dma);
    SafeDelete(m_memory);
    SafeDelete(m_firmware);
    SafeDelete(m_cdrom_media);
}

void GeartownsCore::Init(GT_Pixel_Format pixel_format)
{
    m_pixel_format = pixel_format;

    if (!IsValidPointer(m_cdrom_media))
        m_cdrom_media = new CdRomMedia();

    if (!IsValidPointer(m_cdrom_audio))
        m_cdrom_audio = new CdRomAudio(m_cdrom_media);

    if (!IsValidPointer(m_audio))
        m_audio = new Audio();

    if (!IsValidPointer(m_firmware))
        m_firmware = new Firmware();

    if (!IsValidPointer(m_input))
        m_input = new Input();

    if (!IsValidPointer(m_media))
        m_media = new Media(m_cdrom_media);

    if (!IsValidPointer(m_memory))
        m_memory = new Memory();

    if (!IsValidPointer(m_i386))
        m_i386 = new I386();

    if (!IsValidPointer(m_io))
        m_io = new IO();

    if (!IsValidPointer(m_pic))
        m_pic = new PIC();

    if (!IsValidPointer(m_pit))
        m_pit = new PIT();

    if (!IsValidPointer(m_system_control))
        m_system_control = new SystemControl();

    if (!IsValidPointer(m_scheduler))
        m_scheduler = new Scheduler();

    if (!IsValidPointer(m_video))
        m_video = new Video();

    if (!IsValidPointer(m_cdrom))
        m_cdrom = new CdRom(m_cdrom_media, m_cdrom_audio);

    if (!IsValidPointer(m_fdc))
        m_fdc = new FDC();

    if (!IsValidPointer(m_keyboard))
        m_keyboard = new Keyboard();

    if (!IsValidPointer(m_rtc))
        m_rtc = new MSM58321();

    if (!IsValidPointer(m_dma))
        m_dma = new UPD71071();

    m_firmware->Init();
    m_scheduler->Init();
    m_memory->Init();
    m_cdrom_media->Init();
    m_cdrom_audio->Init();
    m_audio->Init(m_scheduler, m_cdrom_audio);
    m_pic->Init();
    m_pit->Init(m_pic, m_scheduler);
    m_system_control->Init();
    m_video->Init(m_pic, m_pit, m_scheduler, m_firmware->GetFontRom(), m_pixel_format);
    m_dma->Init(m_memory, m_scheduler);
    m_cdrom->Init(m_pic, m_scheduler, m_dma, m_audio);
    m_fdc->Init(m_pic, m_scheduler, m_dma);
    m_keyboard->Init(m_pic, m_scheduler);
    m_rtc->Init();
    m_io->Init(m_audio, m_pic, m_pit, m_video, m_memory, m_system_control, m_cdrom, m_fdc, m_keyboard, m_rtc, m_dma);
    m_i386->Init(m_memory, m_io);
    m_input->Init();
    m_media->Init();
    Reset();
}

GT_Run_Result GeartownsCore::RunToFrame(u8* frame_buffer, s16* sample_buffer, int* sample_count, bool render)
{
    return RunToFrameTemplate<false>(frame_buffer, sample_buffer, sample_count, NULL, render);
}

GT_Run_Result GeartownsCore::RunToFrame(u8* frame_buffer, s16* sample_buffer, int* sample_count, GT_Debug_Run* debug,
    bool render)
{
#if defined(GT_DISABLE_DISASSEMBLER)
    return RunToFrameTemplate<false>(frame_buffer, sample_buffer, sample_count, debug, render);
#else
    return RunToFrameTemplate<true>(frame_buffer, sample_buffer, sample_count, debug, render);
#endif
}

template<bool debugger>
GT_Run_Result GeartownsCore::RunToFrameTemplate(u8* frame_buffer, s16* sample_buffer, int* sample_count,
    GT_Debug_Run* debug, bool render)
{
    m_frame_buffer = frame_buffer;

    if (sample_count != NULL)
        *sample_count = 0;

    if (m_paused)
        return GT_RUN_PAUSED;

    if (!m_powered || !IsValidPointer(m_firmware) || !m_firmware->IsReady())
        return GT_RUN_NOT_READY;

    u64 frame_start = m_scheduler->GetClocks();
    m_video->BeginFrame(frame_buffer, render);

    if (debugger && IsValidPointer(debug))
        RunDebuggerFrame(frame_start, debug);
    else
        RunFrame(frame_start);

    EndFrame(frame_start, sample_buffer, sample_count);

    return GT_RUN_FRAME_READY;
}

void GeartownsCore::RunFrame(u64 frame_start)
{
    while (!IsFrameDone(frame_start))
    {
        u32 slice = m_scheduler->GetSliceClocks(GetFrameLimit(frame_start));
        u32 cycles = m_scheduler->GetSliceCycles(slice);
        GT_Bus_Access_Context context = BeginSlice();
        I386_Run_Result result = m_i386->RunFor(cycles, context, false, m_pic->IsInterruptPending());
        CompleteSlice(result, context, slice);
    }
}

void GeartownsCore::RunDebuggerFrame(u64 frame_start, GT_Debug_Run* debug)
{
    debug->stopped = false;
    debug->breakpoint_hit = false;

    while (!IsFrameDone(frame_start))
    {
        if (m_i386->CheckDebuggerBreakpoints(debug->stop_on_breakpoint, debug->stop_on_run_to_breakpoint))
        {
            debug->stopped = true;
            debug->breakpoint_hit = true;
            break;
        }

        u32 slice = m_scheduler->GetSliceClocks(GetFrameLimit(frame_start));
        GT_Bus_Access_Context context = BeginSlice();
        m_i386->RunInstruction(context);

        if (debug->step_over)
        {
            u32 call_return_linear = 0;
            bool call = m_i386->GetStepCall(call_return_linear);
            debug->step_over = false;
            debug->step_debugger = !call;

            if (call)
                m_i386->AddRunToBreakpoint(call_return_linear);
        }

        CompleteSlice(m_i386->GetStepInfo(), context, slice);

        if (debug->step_debugger)
        {
            debug->stopped = true;
            break;
        }
    }
}

void GeartownsCore::EndFrame(u64 frame_start, s16* sample_buffer, int* sample_count)
{
    m_video->EndFrame();

    // Debugger memory views refresh once per executed frame or debugger step
    if (m_scheduler->GetClocks() != frame_start)
        m_memory->InvalidateDebugSnapshot();

    m_audio->Synchronize(m_scheduler->GetClocks());
    m_audio->EndFrame(sample_buffer, sample_count);
}

INLINE bool GeartownsCore::IsFrameDone(u64 frame_start) const
{
    return m_video->IsFrameReady() || (m_scheduler->GetClocks() >= GetFrameLimit(frame_start));
}

INLINE u64 GeartownsCore::GetFrameLimit(u64 frame_start) const
{
    return frame_start + (m_video->IsRunning() ? GT_CPU_CLOCKS_PER_FRAME * 4 : GT_CPU_CLOCKS_PER_FRAME);
}

INLINE GT_Bus_Access_Context GeartownsCore::BeginSlice() const
{
    GT_Bus_Access_Context context = {};
    context.origin = GT_BUS_ORIGIN_CPU;
    context.clocks = m_scheduler->GetClocks();
    return context;
}

// A halted CPU idles through the slice
// Events due by the end of the slice run before the CPU samples INTR at its boundary
INLINE void GeartownsCore::CompleteSlice(const I386_Run_Result& result, GT_Bus_Access_Context& context, u32 slice)
{
    bool idle = (result.steps == 0) && m_i386->Halted();

    if (idle)
        m_scheduler->AddClocks(slice);
    else
        m_scheduler->AddCycles((u32)(result.clocks + context.wait_clocks));

    if (m_scheduler->IsEventDue())
        DispatchEvents();

    // The board answers a shutdown cycle by resetting the CPU
    if (unlikely(m_i386->Shutdown()))
        m_system_control->RequestCPUReset(k_system_control_reset_shutdown);

    if (unlikely(m_system_control->IsCPUResetPending()))
        ResetCPU();
    else if (m_pic->IsInterruptPending() && m_i386->CanAcceptMaskableInterrupt())
    {
        u32 interrupt_cycles = m_i386->EnterExternalInterrupt(m_pic->AcknowledgeInterrupt(), context);
        m_scheduler->AddCycles(interrupt_cycles);

        if (m_scheduler->IsEventDue())
            DispatchEvents();
    }
}

INLINE void GeartownsCore::DispatchEvents()
{
    while (m_scheduler->IsEventDue())
    {
        u64 clocks = m_scheduler->GetClocks();

        switch (m_scheduler->PopEvent())
        {
            case SCHEDULER_EVENT_PIT:
                m_pit->HandleEvent(clocks);
                break;
            case SCHEDULER_EVENT_VIDEO:
                m_video->HandleEvent(clocks);
                break;
            case SCHEDULER_EVENT_CDROM:
                m_cdrom->HandleEvent(clocks);
                break;
            case SCHEDULER_EVENT_FDC:
                m_fdc->HandleEvent(clocks);
                break;
            case SCHEDULER_EVENT_KEYBOARD:
                m_keyboard->HandleEvent(clocks);
                break;
            case SCHEDULER_EVENT_DMA:
                // The DMA owns the bus for each unit it moves, so the CPU loses that time
                m_scheduler->AddClocks(m_dma->HandleEvent(clocks));
                break;
            default:
                break;
        }
    }
}

bool GeartownsCore::PowerOn()
{
    if (!IsValidPointer(m_firmware) || !m_firmware->IsReady())
        return false;

    Reset();
    m_powered = true;
    return true;
}

void GeartownsCore::PowerOff()
{
    m_powered = false;
}

// The new hardware takes effect on the next reset
void GeartownsCore::SetMachineConfig(const GT_Machine_Config& config)
{
    m_pending_machine_config = config;
    SanitizeMachineConfig(m_pending_machine_config);
}

bool GeartownsCore::IsMachineConfigPending()
{
    return (m_pending_machine_config.model != m_machine_config.model) ||
        (m_pending_machine_config.cpu != m_machine_config.cpu) ||
        (m_pending_machine_config.ram_size != m_machine_config.ram_size) ||
        (m_pending_machine_config.floppy_drives != m_machine_config.floppy_drives) ||
        (m_pending_machine_config.cpu_clock_rate != m_machine_config.cpu_clock_rate);
}

// New firmware restarts a running machine, a powered off one waits for PowerOn
bool GeartownsCore::LoadBios(const char* directory_path)
{
    if (!IsValidPointer(m_firmware) || !m_firmware->LoadDirectory(directory_path))
        return false;

    if (IsValidPointer(m_i386))
        m_i386->ResetDisassembler();

    if (m_powered)
        Reset();

    return true;
}

void GeartownsCore::UnloadBios()
{
    m_powered = false;

    if (IsValidPointer(m_firmware))
        m_firmware->Unload();

    if (IsValidPointer(m_i386))
        m_i386->ResetDisassembler();
}

// Loading media is a disc swap on a running machine
// Even when it fails the old disc is gone
bool GeartownsCore::LoadMedia(const char* file_path)
{
    if (!IsValidPointer(m_media))
        return false;

    bool ok = m_media->LoadMedia(file_path);
    m_cdrom->NotifyMediaChanged();
    return ok;
}

#if defined(GT_ENABLE_PHYSICAL_CDROM)
bool GeartownsCore::LoadPhysicalCdRom(const char* device_id)
{
    if (!IsValidPointer(m_media))
        return false;

    bool ok = m_media->LoadPhysicalCdRom(device_id);
    m_cdrom->NotifyMediaChanged();
    return ok;
}
#endif

// The machine keeps running with an empty drive
void GeartownsCore::EjectMedia()
{
    if (!IsValidPointer(m_media))
        return;

    m_media->Reset();
    m_cdrom->NotifyMediaChanged();
}

void GeartownsCore::ResetMedia()
{
    Reset();
}

bool GeartownsCore::InsertFloppy(int drive, const u8* data, u32 size, bool write_protected, u32 base_crc)
{
    return IsValidPointer(m_fdc) && m_fdc->InsertDisk(drive, data, size, write_protected, base_crc);
}

void GeartownsCore::EjectFloppy(int drive)
{
    if (IsValidPointer(m_fdc))
        m_fdc->EjectDisk(drive);
}

void GeartownsCore::SwapFloppies()
{
    if (IsValidPointer(m_fdc))
        m_fdc->SwapDisks();
}

FloppyDisk* GeartownsCore::GetFloppy(int drive)
{
    return IsValidPointer(m_fdc) ? m_fdc->GetDisk(drive) : NULL;
}

void GeartownsCore::KeyPressed(GT_Keys key)
{
    if (IsValidPointer(m_keyboard))
        m_keyboard->KeyPressed(key);
}

void GeartownsCore::KeyReleased(GT_Keys key)
{
    if (IsValidPointer(m_keyboard))
        m_keyboard->KeyReleased(key);
}

void GeartownsCore::ReleaseAllKeys()
{
    if (IsValidPointer(m_keyboard))
        m_keyboard->ReleaseAllKeys();
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

    if (!m_powered)
    {
        Error("Machine is not powered on when trying to save state");
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
    if (!m_powered)
    {
        Error("Machine is not powered on when trying to save state");
        return false;
    }

    Debug("Serializing save state...");

    StateSerializer serializer(stream);
    Serialize(serializer);

    for (int i = 0; i < FDC_DRIVES; i++)
    {
        FloppyDisk* disk = m_fdc->GetDisk(i);
        bool inserted = disk->IsInserted();
        u32 base_crc = disk->GetBaseCRC();
        G_SERIALIZE(serializer, inserted);
        G_SERIALIZE(serializer, base_crc);
    }

    m_scheduler->SaveState(stream);
    m_memory->SaveState(stream);
    m_i386->SaveState(stream);
    m_audio->SaveState(stream);
    m_input->SaveState(stream);
    m_pic->SaveState(stream);
    m_pit->SaveState(stream);
    m_system_control->SaveState(stream);
    m_video->SaveState(stream);
    m_cdrom->SaveState(stream);
    m_cdrom_audio->SaveState(stream);
    m_fdc->SaveState(stream);
    m_keyboard->SaveState(stream);
    m_rtc->SaveState(stream);
    m_dma->SaveState(stream);

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

    if (!m_powered)
    {
        Error("Machine is not powered on when trying to load state");
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

    if (!m_powered)
    {
        Error("Machine is not powered on when trying to load state");
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

    GT_Machine_Config machine_config = m_machine_config;
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();

    // The CPU speed may differ, the hardware may not
    // The running machine keeps its own configuration either way
    bool same_machine = (m_machine_config.model == machine_config.model) &&
        (m_machine_config.cpu == machine_config.cpu) && (m_machine_config.ram_size == machine_config.ram_size) &&
        (m_machine_config.floppy_drives == machine_config.floppy_drives);
    m_machine_config = machine_config;

    if (!same_machine)
    {
        Error("Save state is for another machine configuration");
        return false;
    }

    for (int i = 0; i < FDC_DRIVES; i++)
    {
        FloppyDisk* disk = m_fdc->GetDisk(i);
        bool inserted = false;
        u32 base_crc = 0;
        G_SERIALIZE(serializer, inserted);
        G_SERIALIZE(serializer, base_crc);

        if (inserted && disk->IsInserted() && base_crc != disk->GetBaseCRC())
        {
            Error("Save state is for another disk in floppy drive %d", i + 1);
            return false;
        }
    }

    m_scheduler->LoadState(stream);
    m_memory->LoadState(stream);
    m_i386->LoadState(stream);
    m_audio->LoadState(stream);
    m_input->LoadState(stream);
    m_pic->LoadState(stream);
    m_pit->LoadState(stream);
    m_system_control->LoadState(stream);
    m_video->LoadState(stream);
    m_cdrom->LoadState(stream);
    m_cdrom_audio->LoadState(stream);
    m_fdc->LoadState(stream);
    m_keyboard->LoadState(stream);
    m_rtc->LoadState(stream);
    m_dma->LoadState(stream);

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
    G_SERIALIZE(serializer, m_machine_config.model);
    G_SERIALIZE(serializer, m_machine_config.cpu);
    G_SERIALIZE(serializer, m_machine_config.ram_size);
    G_SERIALIZE(serializer, m_machine_config.floppy_drives);
    G_SERIALIZE(serializer, m_machine_config.cpu_clock_rate);
}

void GeartownsCore::SanitizeState()
{
    SanitizeMachineConfig(m_machine_config);
}

void GeartownsCore::SanitizeMachineConfig(GT_Machine_Config& config)
{
    bool known_model = ((int)config.model >= 0) && ((int)config.model < GT_MACHINE_COUNT);

    if (!known_model || !k_machine_profiles[config.model].emulated)
        config.model = GT_MACHINE_MODEL1_2;

    const GT_Machine_Profile& profile = k_machine_profiles[config.model];
    u32 megabyte = 1024 * 1024;

    if (config.model != GT_MACHINE_CUSTOM)
        config.cpu = profile.cpu;

    bool known_cpu = ((int)config.cpu >= 0) && ((int)config.cpu < GT_MACHINE_CPU_COUNT);

    if (!known_cpu || !k_machine_cpus[config.cpu].emulated)
        config.cpu = GT_MACHINE_CPU_80386DX;

    config.ram_size = CLAMP(config.ram_size, profile.ram_min_mb * megabyte, profile.ram_max_mb * megabyte);
    config.floppy_drives = CLAMP(config.floppy_drives, profile.floppy_min, profile.floppy_max);
    config.cpu_clock_rate = MAX(config.cpu_clock_rate, profile.cpu_clock_rate);
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
    runtime_info.screen_width = IsValidPointer(m_video) ? m_video->GetFrameWidth() : GT_FRAME_BUFFER_WIDTH;
    runtime_info.screen_height = IsValidPointer(m_video) ? m_video->GetFrameHeight() : GT_FRAME_BUFFER_HEIGHT;
    runtime_info.width_scale = 1;
    runtime_info.frame_time = (float)((GT_CPU_CLOCKS_PER_FRAME * 1000.0) / GT_CPU_CLOCK_RATE);

    // A running CRTC paces frames at its VSYNC rate
    if (IsValidPointer(m_video) && m_video->IsRunning())
        runtime_info.frame_time = m_video->GetFrameTime();

    runtime_info.sample_rate = GT_AUDIO_SAMPLE_RATE;
    runtime_info.media_ready = IsValidPointer(m_media) && m_media->IsReady();
    runtime_info.bios_ready = IsValidPointer(m_firmware) && m_firmware->IsReady();
    runtime_info.paused = m_paused;
}

void GeartownsCore::Reset()
{
    m_paused = false;
    ApplyMachineConfig();

    if (IsValidPointer(m_scheduler))
        m_scheduler->Reset();

    if (IsValidPointer(m_memory))
        m_memory->Reset();

    if (IsValidPointer(m_io))
        m_io->Reset();

    if (IsValidPointer(m_pic))
        m_pic->Reset();

    if (IsValidPointer(m_pit))
        m_pit->Reset();

    if (IsValidPointer(m_system_control))
        m_system_control->Reset();

    if (IsValidPointer(m_video))
        m_video->Reset();

    if (IsValidPointer(m_cdrom))
        m_cdrom->Reset();

    if (IsValidPointer(m_cdrom_audio))
        m_cdrom_audio->Reset();

    if (IsValidPointer(m_fdc))
        m_fdc->Reset();

    if (IsValidPointer(m_keyboard))
        m_keyboard->Reset();

    if (IsValidPointer(m_rtc))
        m_rtc->Reset(m_scheduler->GetClocks());

    if (IsValidPointer(m_dma))
        m_dma->Reset();

    InitMemoryMap();

    if (IsValidPointer(m_i386))
        m_i386->Reset();

    if (IsValidPointer(m_audio))
        m_audio->Reset();

    if (IsValidPointer(m_input))
        m_input->Reset();
}

// Only the CPU restarts and the low memory view returns to its boot setup, memory and devices keep their state
void GeartownsCore::ResetCPU()
{
    m_system_control->AcknowledgeCPUReset();
    m_memory->ResetMapping();
    m_video->ResetFMRView();
    m_i386->Reset();
}

void GeartownsCore::ApplyMachineConfig()
{
    m_machine_config = m_pending_machine_config;

    if (IsValidPointer(m_scheduler))
        m_scheduler->SetCPUClockRate(m_machine_config.cpu_clock_rate);

    if (IsValidPointer(m_memory))
        m_memory->SetMainRAMSize(m_machine_config.ram_size);

    if (IsValidPointer(m_fdc))
        m_fdc->SetInternalDrives(m_machine_config.floppy_drives);
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

    u32 boot_rom_flags = (rom_flags & ~GT_DEBUG_REGION_MAPPED) | GT_DEBUG_REGION_OVERLAY;

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_SYSTEM_ROM_LOW_ALIAS, "System ROM (low boot window)", boot_rom,
        NULL, GT_FIRMWARE_SYSTEM_BOOT_SIZE, 0x000F8000U, boot_rom_flags))
        Error("Unable to register the low system ROM alias");

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_MAIN_RAM, "Main RAM", main_ram, main_ram,
        m_memory->GetMainRAMSize(), 0, ram_flags))
        Error("Unable to map main RAM");

    u32 vram_flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_WRITABLE | GT_DEBUG_REGION_VIDEO;
    u32 sprite_ram_flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_WRITABLE | GT_DEBUG_REGION_MAPPED;
    u8* vram = m_video->GetVRAM();
    u8* sprite_ram = m_video->GetSpriteRAM();

    // Canonical VRAM for raw debugger access, the bus reaches it through the two views
    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_VRAM, "VRAM", vram, vram, VIDEO_VRAM_SIZE, 0, vram_flags))
        Error("Unable to register VRAM");

    if (!m_memory->RegisterHandlerRegion(GT_DEBUG_REGION_VRAM_TWO_PAGE, "VRAM (two-page view)", VIDEO_VRAM_SIZE,
        0x80000000U, vram_flags | GT_DEBUG_REGION_MAPPED, m_video, Video::ReadVRAMTwoPageCallback,
        Video::WriteVRAMTwoPageCallback, NULL))
        Error("Unable to map the two-page VRAM view");

    if (!m_memory->RegisterHandlerRegion(GT_DEBUG_REGION_VRAM_SINGLE_PAGE, "VRAM (single-page view)", VIDEO_VRAM_SIZE,
        0x80100000U, vram_flags | GT_DEBUG_REGION_MAPPED, m_video, Video::ReadVRAMSinglePageCallback,
        Video::WriteVRAMSinglePageCallback, NULL))
        Error("Unable to map the single-page VRAM view");

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_SPRITE_RAM, "Sprite RAM", sprite_ram, sprite_ram,
        VIDEO_SPRITE_RAM_SIZE, 0x81000000U, sprite_ram_flags))
        Error("Unable to map sprite RAM");

    u32 data_rom_flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_ROM | GT_DEBUG_REGION_MAPPED;

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_OS_ROM, "OS ROM", m_firmware->GetOsRom(), NULL,
        GT_FIRMWARE_OS_SIZE, 0xC2000000U, rom_flags))
        Error("Unable to map the OS ROM");

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_DICTIONARY_ROM, "Dictionary ROM", m_firmware->GetDictionaryRom(),
        NULL, GT_FIRMWARE_DICTIONARY_SIZE, 0xC2080000U, data_rom_flags))
        Error("Unable to map the dictionary ROM");

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_FONT_ROM, "Font ROM", m_firmware->GetFontRom(), NULL,
        GT_FIRMWARE_FONT_SIZE, 0xC2100000U, data_rom_flags))
        Error("Unable to map the font ROM");

    u8* cmos = m_memory->GetCMOS();

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_CMOS, "CMOS RAM", cmos, cmos, GT_CMOS_SIZE, 0xC2140000U,
        GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_WRITABLE | GT_DEBUG_REGION_MAPPED))
        Error("Unable to map CMOS RAM");

    if (!m_memory->RegisterHandlerRegion(GT_DEBUG_REGION_PCM_WINDOW, "PCM wave RAM window", 0x1000, 0xC2200000U,
        GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_WRITABLE | GT_DEBUG_REGION_MAPPED | GT_DEBUG_REGION_AUDIO,
        m_audio, Audio::ReadWaveWindowCallback, Audio::WriteWaveWindowCallback, NULL))
        Error("Unable to map the PCM wave RAM window");

    // The low windows are overlays that the mapping latches switch on and off
    u32 overlay_flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_WRITABLE | GT_DEBUG_REGION_OVERLAY;

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_DICTIONARY_ROM_LOW_WINDOW, "Dictionary ROM (low window)",
        m_firmware->GetDictionaryRom(), NULL, 0x8000, 0x000D0000U,
        GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_ROM | GT_DEBUG_REGION_OVERLAY))
        Error("Unable to register the low dictionary ROM window");

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_CMOS_LOW_WINDOW, "CMOS RAM (low window)", cmos, cmos,
        GT_CMOS_SIZE, 0x000D8000U, overlay_flags))
        Error("Unable to register the low CMOS RAM window");

    if (!m_memory->RegisterHandlerRegion(GT_DEBUG_REGION_FMR_PLANES, "FM-R VRAM planes", 0x8000, 0x000C0000U,
        overlay_flags | GT_DEBUG_REGION_VIDEO, m_video, Video::ReadFMRPlanesCallback, Video::WriteFMRPlanesCallback,
        NULL))
        Error("Unable to register the FM-R VRAM planes");

    if (!m_memory->RegisterHandlerRegion(GT_DEBUG_REGION_FMR_TEXT, "FM-R text RAM and ANK font", 0x7000, 0x000C8000U,
        overlay_flags | GT_DEBUG_REGION_VIDEO, m_video, Video::ReadFMRTextCallback, Video::WriteFMRTextCallback, NULL))
        Error("Unable to register the FM-R text window");

    if (!m_memory->RegisterHandlerRegion(GT_DEBUG_REGION_FMR_REGISTERS, "FM-R registers", 0x1000, 0x000CF000U,
        overlay_flags | GT_DEBUG_REGION_MMIO, m_video, Video::ReadFMRRegisterCallback, Video::WriteFMRRegisterCallback,
        Video::PeekFMRRegisterCallback))
        Error("Unable to register the FM-R registers");

    // Registered after the dictionary and CMOS windows so they take priority over it
    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_FMR_VIEW, "FM-R view (unmapped)", NULL, NULL, 0x20000,
        0x000D0000U, GT_DEBUG_REGION_OVERLAY))
        Error("Unable to register the FM-R view");

    m_memory->ResetMapping();
}
