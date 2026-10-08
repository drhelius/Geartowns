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
#include "common/common.h"

class Audio;
class CdRom;
class CdRomAudio;
class CdRomMedia;
class FDC;
class FloppyDisk;
class Keyboard;
class MSM58321;
class UPD71071;
class Firmware;
class I386;
class Input;
class Media;
class Memory;
class IO;
class PIC;
class PIT;
class SystemControl;
class Scheduler;
class Video;
class StateSerializer;
class TraceLogger;
class Profiler;
struct I386_Run_Result;

class GeartownsCore
{
public:
    struct GT_Debug_Run
    {
        bool step_debugger;
        bool step_over;
        bool stop_on_breakpoint;
        bool stop_on_run_to_breakpoint;
        bool skip_interrupts_on_step;
        bool stopped;
        bool breakpoint_hit;
    };

public:
    GeartownsCore();
    ~GeartownsCore();
    void Init();
    GT_Run_Result RunToFrame(u8* frame_buffer, s16* sample_buffer, int* sample_count, bool render = true);
    GT_Run_Result RunToFrame(u8* frame_buffer, s16* sample_buffer, int* sample_count, GT_Debug_Run* debug,
        bool render = true);

    bool PowerOn();
    void PowerOff();
    bool IsPoweredOn();
    void SetMachineConfig(const GT_Machine_Config& config);
    const GT_Machine_Config& GetMachineConfig();
    const GT_Machine_Config& GetPendingMachineConfig();
    bool IsMachineConfigPending();
    bool LoadBios(const char* directory_path);
    void UnloadBios();
    bool LoadMedia(const char* file_path);
#if defined(GT_ENABLE_PHYSICAL_CDROM)
    bool LoadPhysicalCdRom(const char* device_id);
#endif
    void EjectMedia();
    void ResetMedia();
    bool InsertFloppy(int drive, const u8* data, u32 size, bool write_protected, u32 base_crc = 0);
    void EjectFloppy(int drive);
    void SwapFloppies();

    void KeyPressed(GT_Keys key);
    void KeyReleased(GT_Keys key);
    void ReleaseAllKeys();
    void Pause(bool paused);
    bool IsPaused();

    bool SaveState(const char* path = NULL, int index = -1, bool screenshot = false);
    bool SaveState(u8* buffer, size_t& size, bool screenshot = false);
    bool GetMaxSaveStateSize(size_t& size);
    bool LoadState(const char* path = NULL, int index = -1);
    bool LoadState(const u8* buffer, size_t size);
    bool GetSaveStateHeader(int index, const char* path, GT_SaveState_Header* header);
    bool GetSaveStateScreenshot(int index, const char* path, GT_SaveState_Screenshot* screenshot);

    void GetRuntimeInfo(GT_Runtime_Info& runtime_info);

    Firmware* GetFirmware();
    Media* GetMedia();
    Audio* GetAudio();
    Input* GetInput();
    Memory* GetMemory();
    I386* GetI386();
    IO* GetIO();
    PIC* GetPIC();
    PIT* GetPIT();
    SystemControl* GetSystemControl();
    Scheduler* GetScheduler();
    Video* GetVideo();
    CdRom* GetCDROM();
    CdRomMedia* GetCDROMMedia();
    CdRomAudio* GetCDROMAudio();
    FDC* GetFDC();
    FloppyDisk* GetFloppy(int drive);
    Keyboard* GetKeyboard();
    TraceLogger* GetTraceLogger();
    Profiler* GetProfiler();
    MSM58321* GetRTC();
    UPD71071* GetDMA();

private:
    template<bool debugger>
    GT_Run_Result RunToFrameTemplate(u8* frame_buffer, s16* sample_buffer, int* sample_count, GT_Debug_Run* debug,
        bool render);
    void RunFrame(u64 frame_start);
    void RunDebuggerFrame(u64 frame_start, GT_Debug_Run* debug);
    void EndFrame(u64 frame_start, s16* sample_buffer, int* sample_count);
    bool IsFrameDone(u64 frame_start) const;
    u64 GetFrameLimit(u64 frame_start) const;
    GT_Bus_Access_Context BeginSlice() const;
    void CompleteSlice(const I386_Run_Result& result, GT_Bus_Access_Context& context, u32 slice);
    void DispatchEvents();
    void Reset();
    void ResetCPU();
    void ApplyMachineConfig();
    void InitMemoryMap();

    bool SaveState(std::ostream& stream, size_t& size, bool screenshot);
    bool LoadState(std::istream& stream);
    void Serialize(StateSerializer& serializer);
    void SanitizeState();
    void SanitizeMachineConfig(GT_Machine_Config& config);
    std::string GetSaveStatePath(const char* path, int index);

private:
    Audio* m_audio;
    Firmware* m_firmware;
    Input* m_input;
    Media* m_media;
    Memory* m_memory;
    I386* m_i386;
    IO* m_io;
    PIC* m_pic;
    PIT* m_pit;
    SystemControl* m_system_control;
    Scheduler* m_scheduler;
    Video* m_video;
    CdRomMedia* m_cdrom_media;
    CdRomAudio* m_cdrom_audio;
    CdRom* m_cdrom;
    FDC* m_fdc;
    Keyboard* m_keyboard;
    TraceLogger* m_trace_logger;
    Profiler* m_profiler;
    MSM58321* m_rtc;
    UPD71071* m_dma;

    GT_Machine_Config m_machine_config;
    GT_Machine_Config m_pending_machine_config;
    bool m_powered;
    bool m_paused;
    bool m_skip_interrupts;

    u8* m_frame_buffer;
};

#include "geartowns_core_inline.h"

#endif /* GEARTOWNS_CORE_H */
