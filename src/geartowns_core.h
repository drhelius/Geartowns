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
class HardDiskImage;
class MemoryCard;
class ScsiController;
class Keyboard;
class MSM58321;
class UPD71071;
class Firmware;
class I386;
class X87;
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
struct GT_DB_Entry;

enum GT_Debug_Step
{
    GT_DEBUG_STEP_NONE = 0,
    GT_DEBUG_STEP_INSTRUCTION,
    GT_DEBUG_STEP_RETURN
};

class GeartownsCore
{
public:
    struct GT_Debug_Run
    {
        bool step_debugger;
        bool step_over;
        bool step_start;
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
    void SetMachineSettings(const GT_Machine_Settings& settings);
    void LatchAutoMedia();
    const GT_Machine_Settings& GetMachineSettings();
    const GT_Machine_Config& GetMachineConfig();
    const GT_Machine_Config& GetPendingMachineConfig();
    const GT_Machine_Report& GetMachineReport();
    const GT_Machine_Report& GetPendingMachineReport();
    bool IsMachineConfigPending();
    bool GetFloppyNotice(GT_Floppy_Notice& notice);
    bool GetDatabaseController(int port, GT_Controller_Type& type) const;
    bool LoadBios(const char* directory_path);
    void UnloadBios();
    void SetModelFirmwareFolder(GT_Machine_Model model, const char* directory_path);
    void SearchFirmware();
    bool ReloadFirmware();
    const GT_Firmware_Source& GetFirmwareSource(GT_Machine_Model model);
    bool HasFirmware(GT_Machine_Model model);
    bool LoadMedia(const char* file_path);
#if defined(GT_ENABLE_PHYSICAL_CDROM)
    bool LoadPhysicalCdRom(const char* device_id);
#endif
    void EjectMedia();
    void ResetMedia();
    bool InsertFloppy(int drive, const u8* data, u32 size, bool write_protected, u32 base_crc = 0, u32 data_crc = 0, const u8* source = NULL, u32 source_size = 0);
    void EjectFloppy(int drive);
    void SwapFloppies();
    bool AttachHardDisk(int id, const char* path, bool write_protected = false, const char* working_path = NULL);
    void DetachHardDisk(int id);
    bool FlushHardDisks();
    bool IsHardDiskPending(int id) const;
    const char* GetPendingHardDiskPath(int id) const;
    bool IsPendingHardDiskWriteProtected(int id) const;
    u32 GetHardDiskStateChanges() const;
    bool InsertMemoryCard(const u8* data, u32 size, bool write_protected);
    void EjectMemoryCard();
    bool IsMemoryCardStateChanged() const;
    void SetBootDevice(GT_Boot_Device device);
    GT_Boot_Device GetBootDevice() const;
    void EnableDriveLetterAssignment(bool enabled);
    void LockRAMSize(bool locked);

    void KeyPressed(GT_Keys key);
    void KeyReleased(GT_Keys key);
    void ReleaseAllKeys();
    void Pause(bool paused);
    bool IsPaused();

    bool SaveState(const char* path = NULL, int index = -1, bool screenshot = false);
    bool SaveState(u8* buffer, size_t& size, bool screenshot = false, bool media = false);
    bool GetMaxSaveStateSize(size_t& size);
    bool LoadState(const char* path = NULL, int index = -1);
    bool LoadState(const u8* buffer, size_t size, GT_SaveState_Media* media = NULL);
    bool GetSaveStateHeader(int index, const char* path, GT_SaveState_Header* header);
    bool GetSaveStateScreenshot(int index, const char* path, GT_SaveState_Screenshot* screenshot);

    void GetRuntimeInfo(GT_Runtime_Info& runtime_info);

    Firmware* GetFirmware();
    Media* GetMedia();
    Audio* GetAudio();
    Input* GetInput();
    Memory* GetMemory();
    I386* GetI386();
    X87* GetX87();
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
    ScsiController* GetSCSI();
    HardDiskImage* GetHardDisk(int id);
    MemoryCard* GetMemoryCard();
    Keyboard* GetKeyboard();
    TraceLogger* GetTraceLogger();
    Profiler* GetProfiler();
    MSM58321* GetRTC();
    UPD71071* GetDMA();

private:
    template<bool debugger>
    GT_Run_Result RunToFrameTemplate(u8* frame_buffer, s16* sample_buffer, int* sample_count, GT_Debug_Run* debug, bool render);
    void RunFrame(u64 frame_start);
    void RunDebuggerFrame(u64 frame_start, GT_Debug_Run* debug);
    void RunDebuggerSteps(u64 frame_start, GT_Debug_Run* debug);
    bool BeginDebuggerFrame(GT_Debug_Run* debug);
    bool BreakpointStopsDebugger(GT_Debug_Run* debug);
    bool RunDebuggerStep(u64 frame_start, GT_Debug_Run* debug);
    bool RunDeferredInterrupt(GT_Debug_Run* debug);
    bool StepInterrupt(GT_Debug_Run* debug, u32 return_linear, u16 stack_selector, u32 stack_pointer, bool resume);
    bool AcceptDebuggerHit(GT_Debug_Run* debug);
    void StopDebugger(GT_Debug_Run* debug, bool breakpoint);
    void EndFrame(u64 frame_start, s16* sample_buffer, int* sample_count);
    bool IsFrameDone(u64 frame_start) const;
    u64 GetFrameLimit(u64 frame_start) const;
    GT_Bus_Access_Context BeginSlice();
    bool CompleteSlice(const I386_Run_Result& result, GT_Bus_Access_Context& context, u32 slice, bool hold_on_hit);
    bool EnterPendingInterrupt(GT_Bus_Access_Context& context);
    static GT_Synchronize_Result SynchronizeIOCallback(void* core, GT_Bus_Access_Context& context, u32 start_cycles, u32 access_cycles);
    static void SynchronizeMemoryCallback(void* core, GT_Bus_Access_Context& context);
    static u32 PollPortCallback(void* core, u16 port, int width);
    static void DMADebugCallback(void* cpu, u32 address, u32 size, bool write);
    void DispatchEvents();
    void Reset();
    void ResetCPU();
    void ResolveMachineConfig();
    bool HasHardDisks();
    void ApplyHardDisks();
    void AssignDriveLetters();
    bool CanAutoSelect(GT_Machine_Model model);
    bool LoadMachineFirmware(bool force, bool report);
    void ApplyMachineConfig();
    void ApplyBusTiming();
    static void BusModeCallback(void* context);
    static void BootReadCallback(void* context);
    void ApplyBootDevice();
    void InitMemoryMap();

    bool SaveState(std::ostream& stream, size_t& size, bool screenshot, bool media);
    bool LoadState(std::istream& stream, GT_SaveState_Media* media = NULL);
    void Serialize(StateSerializer& serializer, GT_Machine_Config& config, int version);
    void SerializeMedia(StateSerializer& serializer, GT_SaveState_Media& media, u32 version);
    void SanitizeMachineConfig(GT_Machine_Config& config);
    std::string GetSaveStatePath(const char* path, int index);

private:
    Audio* m_audio;
    Firmware* m_firmware;
    Input* m_input;
    Media* m_media;
    Memory* m_memory;
    I386* m_i386;
    X87* m_x87;
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
    ScsiController* m_scsi;
    MemoryCard* m_memory_card;
    Keyboard* m_keyboard;
    TraceLogger* m_trace_logger;
    Profiler* m_profiler;
    MSM58321* m_rtc;
    UPD71071* m_dma;

    GT_Machine_Settings m_machine_settings;
    const GT_DB_Entry* m_auto_entry;
    bool m_auto_three_mode;
    std::string m_firmware_folder;
    std::string m_model_firmware_folders[GT_MACHINE_COUNT];
    GT_Firmware_Source m_firmware_sources[GT_MACHINE_COUNT];
    GT_Machine_Config m_machine_config;
    GT_Machine_Config m_pending_machine_config;
    GT_Machine_Report m_machine_report;
    GT_Machine_Report m_pending_machine_report;
    std::string m_hard_disk_paths[GT_MAX_HARD_DISKS];
    std::string m_hard_disk_working_paths[GT_MAX_HARD_DISKS];
    bool m_hard_disk_write_protected[GT_MAX_HARD_DISKS];
    bool m_hard_disk_pending[GT_MAX_HARD_DISKS];
    u32 m_hard_disk_state_changes;
    bool m_memory_card_state_changed;
    GT_Boot_Device m_boot_device;
    bool m_drive_letter_assignment;
    bool m_ram_size_locked;
    bool m_powered;
    bool m_paused;
    GT_Debug_Step m_debug_step;
    bool m_debug_step_resume;
    bool m_debug_interrupt_deferred;

    u8* m_frame_buffer;
};

#include "geartowns_core_inline.h"

#endif /* GEARTOWNS_CORE_H */
