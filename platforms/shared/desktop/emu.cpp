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

#define EMU_IMPORT
#include "emu.h"

#include <SDL3/SDL.h>
#include <atomic>
#include <math.h>
#include <new>
#include <string.h>
#include <thread>
#include "config.h"
#include "emu_floppy.h"
#include "events.h"
#include "mcp/mcp_manager.h"
#include "rewind.h"
#include "runahead.h"
#include "sound_queue.h"
#include "utils.h"
#include "video_recorder.h"
#include "video/sprite.h"
#include "video/video.h"
#if defined(GT_ENABLE_PHYSICAL_CDROM)
#include "cdrom/cdrom_drive.h"
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(_WIN32)
#define STBIW_WINDOWS_UTF8
#endif
#include "stb_image_write.h"

enum Loading_State
{
    Loading_State_None = 0,
    Loading_State_Loading,
    Loading_State_Finished
};

enum Loading_Request_Type
{
    Loading_Request_File = 0,
    Loading_Request_PhysicalCdRom
};

static GeartownsCore* geartowns = NULL;
static s16* audio_buffer = NULL;
static bool audio_enabled = true;
static McpManager* mcp_manager = NULL;
static Uint64 rewind_last_counter = 0;
static double rewind_pop_accumulator = 0.0;

static std::atomic<int> loading_state(Loading_State_None);
static std::thread loading_thread;
static bool loading_thread_active = false;
static bool loading_result = false;
static char loading_file_path[GT_MAX_PATH] = { };
static Loading_Request_Type loading_request_type = Loading_Request_File;

static void load_media_thread_func(void);
static void reset_buffers(void);
static void reset_run_state(void);
static const char* get_configurated_dir(int option, const char* path);
static void get_video_recording_size(const GT_Runtime_Info& runtime, int* width, int* height);
static void reset_rewind_timing(void);
static int get_rewind_pop_budget(void);
static bool unload_media(void);
static bool get_floppy_state_path(int index, char* path, size_t path_size);
static bool init_debug(void);
static void destroy_debug(void);
static void debug_decode(int buffer, const Emu_Debug_Buffer_Request* request, u8* output, int stride,
    Emu_Debug_Buffer_Info& info);
static u32 debug_rgba(u32 red, u32 green, u32 blue);
static u32 debug_direct_color(u16 value);
static u32 debug_checker(int x, int y);
static u32 debug_single_to_canonical(u32 offset);
static int debug_layer_format(const Video::Video_State* state, int layer);
static void debug_palette(const Video::Video_State* state, int palette, u32* colors);
#if defined(GT_ENABLE_PHYSICAL_CDROM)
static void stop_physical_cdrom_after_error(void);
#endif

bool emu_init(void)
{
    size_t frame_buffer_size = (size_t)GT_MAX_FRAME_BUFFER_WIDTH * GT_MAX_FRAME_BUFFER_HEIGHT * 4;
    emu_frame_buffer = new (std::nothrow) u8[frame_buffer_size];
    audio_buffer = new (std::nothrow) s16[GT_AUDIO_BUFFER_SIZE];

    if (!IsValidPointer(emu_frame_buffer) || !IsValidPointer(audio_buffer))
    {
        Error("Unable to allocate emulator buffers");
        SafeDeleteArray(emu_frame_buffer);
        SafeDeleteArray(audio_buffer);
        return false;
    }

    reset_buffers();

    if (!init_debug())
    {
        Error("Unable to allocate debugger buffers");
        SafeDeleteArray(emu_frame_buffer);
        SafeDeleteArray(audio_buffer);
        return false;
    }

    geartowns = new (std::nothrow) GeartownsCore();

    if (!IsValidPointer(geartowns))
    {
        Error("Unable to allocate Geartowns core");
        SafeDeleteArray(emu_frame_buffer);
        SafeDeleteArray(audio_buffer);
        return false;
    }

    geartowns->Init();
    geartowns->GetMedia()->SetTempPath(config_temp_path);
    emu_floppy_init();

    sound_queue_init();

    for (int i = 0; i < 5; i++)
        InitPointer(emu_savestates_screenshots[i].data);

    emu_savestates_generation = 0;
    emu_debug_command = Debug_Command_None;
    emu_debug_pc_changed = false;
    emu_debug_step_frames_pending = 0;
    emu_debug_disable_breakpoints = false;
    rewind_init();
    runahead_init();

    mcp_manager = new (std::nothrow) McpManager();

    if (!IsValidPointer(mcp_manager))
    {
        runahead_destroy();
        rewind_destroy();
        sound_queue_destroy();
        SafeDelete(geartowns);
        SafeDeleteArray(audio_buffer);
        SafeDeleteArray(emu_frame_buffer);
        return false;
    }

    mcp_manager->Init(geartowns);

    emu_frame_counter = 0;
    emu_audio_sync = true;
    audio_enabled = true;
    return true;
}

void emu_destroy(void)
{
    if (loading_thread_active)
    {
        loading_thread.join();
        loading_thread_active = false;
    }

    loading_state.store(Loading_State_None);
    emu_stop_video_recording();

    if (!emu_floppy_flush())
        Error("Unable to save one or more floppy disks on exit");

    runahead_destroy();
    rewind_destroy();
    SafeDelete(mcp_manager);
    sound_queue_destroy();
    SafeDelete(geartowns);
    SafeDeleteArray(audio_buffer);
    SafeDeleteArray(emu_frame_buffer);
    destroy_debug();

    for (int i = 0; i < 5; i++)
        SafeDeleteArray(emu_savestates_screenshots[i].data);
}

void emu_update(void)
{
    if (loading_state.load() != Loading_State_None)
        return;

    emu_mcp_pump_commands();

#if defined(GT_ENABLE_PHYSICAL_CDROM)
    if (geartowns->GetMedia()->HasPhysicalCdRomError())
    {
        stop_physical_cdrom_after_error();
        return;
    }
#endif

    if (emu_is_empty())
        return;

    geartowns->GetAudio()->EnableChannelScopes(config_debug.debug && (config_debug.show_ym3438 || config_debug.show_rf5c68));

    int sample_count = 0;
    bool frame_executed = false;
    bool frame_completed = false;

    if (rewind_is_active())
    {
        int to_pop = get_rewind_pop_budget();

        if (to_pop > 0)
            rewind_pop(to_pop);

        int silence_count = GT_AUDIO_QUEUE_SIZE;
        memset(audio_buffer, 0, silence_count * sizeof(s16));
        sound_queue_write(audio_buffer, silence_count, false);

        if (config_debug.debug)
            emu_debug_update();

        return;
    }

    reset_rewind_timing();

    if (config_debug.debug)
    {
        bool stopped = false;
        bool frames_stepped = false;

        if (emu_debug_command != Debug_Command_None && !geartowns->IsPaused())
        {
            GeartownsCore::GT_Debug_Run debug_run;

            debug_run.step_debugger = emu_debug_command == Debug_Command_Step;
            debug_run.step_over = emu_debug_command == Debug_Command_StepOver;
            debug_run.stop_on_breakpoint = !emu_debug_disable_breakpoints;
            debug_run.stop_on_run_to_breakpoint = true;
            debug_run.stopped = false;
            debug_run.breakpoint_hit = false;

            rewind_commit_seek();

            GT_Run_Result result = geartowns->RunToFrame(emu_frame_buffer, audio_buffer, &sample_count, &debug_run);
            frame_executed = result != GT_RUN_NOT_READY;
            frame_completed = result == GT_RUN_FRAME_READY && !debug_run.stopped;
            stopped = debug_run.stopped;

            if (emu_debug_command == Debug_Command_StepOver && !debug_run.step_over)
                emu_debug_command = Debug_Command_Continue;
        }

        // Frame steps run whole frames with breakpoints active; a breakpoint ends the remaining frames
        if (stopped)
            emu_debug_step_frames_pending = 0;
        else if (emu_debug_command == Debug_Command_StepFrame && frame_completed)
        {
            emu_debug_step_frames_pending--;
            frames_stepped = emu_debug_step_frames_pending <= 0;
        }

        if (stopped || emu_debug_command == Debug_Command_Step || frames_stepped)
        {
            emu_debug_command = Debug_Command_None;
            emu_debug_step_frames_pending = 0;
            emu_debug_pc_changed = true;

            if (config_debug.dis_look_ahead_count > 0)
                geartowns->GetI386()->DisassembleAhead(config_debug.dis_look_ahead_count);
        }
        else if (emu_debug_command != Debug_Command_Continue && emu_debug_command != Debug_Command_StepFrame &&
            frame_executed)
            emu_debug_command = Debug_Command_None;
    }
    else if (!geartowns->IsPaused())
    {
        rewind_commit_seek();

        GT_Run_Result result = GT_RUN_NOT_READY;
        int runahead = geartowns->GetFirmware()->IsReady() ? runahead_get_frames() : 0;

        if (runahead > 0)
        {
            runahead_run(runahead, emu_frame_buffer, audio_buffer, &sample_count);
            result = GT_RUN_FRAME_READY;
        }
        else
            result = geartowns->RunToFrame(emu_frame_buffer, audio_buffer, &sample_count);

        frame_executed = result != GT_RUN_NOT_READY;
        frame_completed = result == GT_RUN_FRAME_READY;
    }

    if (frame_executed)
    {
        if (frame_completed)
            emu_frame_counter++;

        rewind_push();

        if (video_recorder_is_recording())
        {
            video_recorder_add_audio(audio_buffer, sample_count);

            if (frame_completed)
            {
                GT_Runtime_Info runtime;
                emu_get_runtime(runtime);
                video_recorder_add_video(emu_frame_buffer, runtime.screen_width, runtime.screen_height, 4);
            }
        }
    }

    if (sample_count > 0 && !geartowns->IsPaused())
        sound_queue_write(audio_buffer, sample_count, emu_audio_sync);
    else if (geartowns->IsPaused())
    {
        int silence_count = GT_AUDIO_QUEUE_SIZE;
        memset(audio_buffer, 0, silence_count * sizeof(s16));
        sound_queue_write(audio_buffer, silence_count, false);
    }

    if (config_debug.debug)
        emu_debug_update();
}

void emu_load_media_async(const char* file_path)
{
    if (!IsValidPointer(file_path) || file_path[0] == '\0' || loading_state.load() != Loading_State_None)
    {
        return;
    }

    if (loading_thread_active)
    {
        loading_thread.join();
        loading_thread_active = false;
    }

    strncpy_fit(loading_file_path, file_path, sizeof(loading_file_path));
    loading_request_type = Loading_Request_File;
    loading_result = false;
    loading_state.store(Loading_State_Loading);
    loading_thread = std::thread(load_media_thread_func);
    loading_thread_active = true;
}

void emu_load_physical_cdrom_async(const char* device_id)
{
#if defined(GT_ENABLE_PHYSICAL_CDROM)
    if (!IsValidPointer(device_id) || device_id[0] == '\0' || loading_state.load() != Loading_State_None)
    {
        Debug("Ignoring physical CD-ROM async load request while another media load is active");
        return;
    }

    if (loading_thread_active)
    {
        loading_thread.join();
        loading_thread_active = false;
    }

    Log("Queueing physical CD-ROM async load: %s", device_id);
    strncpy_fit(loading_file_path, device_id, sizeof(loading_file_path));
    loading_request_type = Loading_Request_PhysicalCdRom;
    loading_result = false;
    loading_state.store(Loading_State_Loading);
    loading_thread = std::thread(load_media_thread_func);
    loading_thread_active = true;
#else
    UNUSED(device_id);
#endif
}

bool emu_is_media_loading(void)
{
    return loading_state.load() == Loading_State_Loading;
}

bool emu_finish_media_loading(void)
{
    if (loading_state.load() != Loading_State_Finished)
        return false;

    if (loading_thread_active)
    {
        loading_thread.join();
        loading_thread_active = false;
    }

    loading_state.store(Loading_State_None);

    if (!loading_result)
    {
#if defined(GT_ENABLE_PHYSICAL_CDROM)
        if (loading_request_type == Loading_Request_PhysicalCdRom)
            Debug("Physical CD-ROM async load failed: %s", loading_file_path);
#endif
        return false;
    }

    emu_frame_counter = 0;
    reset_buffers();
    emu_audio_reset();
    update_savestates_data();
    rewind_reset();
    return true;
}

void emu_set_gamepad_state(GT_Controllers controller, const GT_GamePad_State& state)
{
    if (!IsValidPointer(geartowns))
        return;

    geartowns->GetInput()->SetGamePadState((int)controller, state);
}

void emu_key_pressed(GT_Keys key)
{
    if (IsValidPointer(geartowns))
        geartowns->KeyPressed(key);
}

void emu_key_released(GT_Keys key)
{
    if (IsValidPointer(geartowns))
        geartowns->KeyReleased(key);
}

void emu_release_all_keys(void)
{
    if (IsValidPointer(geartowns))
        geartowns->ReleaseAllKeys();
}

void emu_set_mouse_delta(GT_Controllers controller, int x, int y)
{
    if (IsValidPointer(geartowns))
        geartowns->GetInput()->SetMouseDelta((int)controller, x, y);
}

void emu_clear_mouse(GT_Controllers controller)
{
    if (IsValidPointer(geartowns))
        geartowns->GetInput()->ClearMouseInput((int)controller);
}

void emu_pause(void)
{
    if (IsValidPointer(geartowns))
        geartowns->Pause(true);
}

void emu_resume(void)
{
    if (IsValidPointer(geartowns))
        geartowns->Pause(false);
}

bool emu_is_paused(void)
{
    return IsValidPointer(geartowns) && geartowns->IsPaused();
}

bool emu_is_debug_idle(void)
{
    return config_debug.debug && emu_debug_command == Debug_Command_None;
}

bool emu_is_empty(void)
{
    if (loading_state.load() != Loading_State_None)
        return true;

    return !IsValidPointer(geartowns) || !geartowns->IsPoweredOn();
}

bool emu_power_on(void)
{
    if (!IsValidPointer(geartowns) || loading_state.load() != Loading_State_None)
        return false;

    if (!geartowns->PowerOn())
        return false;

    emu_floppy_check_drives();
    reset_run_state();
    update_savestates_data();
    return true;
}

void emu_power_off(void)
{
    if (!IsValidPointer(geartowns) || loading_state.load() != Loading_State_None)
        return;

    geartowns->PowerOff();
    reset_run_state();
    update_savestates_data();
}

void emu_reset(void)
{
    if (!IsValidPointer(geartowns))
        return;

    geartowns->ResetMedia();
    emu_floppy_check_drives();
    reset_run_state();
}

bool emu_eject_media(void)
{
    if (loading_state.load() != Loading_State_None)
    {
        Debug("Ignoring media eject request while media is loading");
        return false;
    }

#if defined(GT_ENABLE_PHYSICAL_CDROM)
    if (geartowns->GetMedia()->IsPhysicalCdRom())
    {
        char device_id[256];
        strncpy_fit(device_id, geartowns->GetMedia()->GetPhysicalCdRomDeviceId(), sizeof(device_id));
        unload_media();

        Log("Ejecting physical CD-ROM: %s", device_id);
        bool ejected = CdRomDrive::Eject(device_id);
        Debug("Physical CD-ROM eject finished: %s (%s)", device_id, ejected ? "success" : "failure");
        return ejected;
    }
#endif

    return unload_media();
}

void emu_set_preload_cdrom(bool enabled)
{
    if (IsValidPointer(geartowns))
        geartowns->GetMedia()->PreloadCdRom(enabled);
}

void emu_set_cdrom_speed(int speed)
{
    if (IsValidPointer(geartowns))
        geartowns->GetCDROM()->SetReadSpeed(speed);
}

void emu_apply_machine_settings(void)
{
    if (!IsValidPointer(geartowns))
        return;

    GT_Machine_Model model = (GT_Machine_Model)config_machine.model;
    int cpu_mhz = config_machine.cpu_mhz[model];

    GT_Machine_Config config;
    config.model = model;
    config.cpu = model == GT_MACHINE_CUSTOM ? (GT_Machine_CPU)config_machine.custom_cpu : k_machine_profiles[model].cpu;
    config.ram_size = (u32)config_machine.ram_mb[model] * 1024 * 1024;
    config.floppy_drives = config_machine.floppy_drives[model];
    config.cpu_clock_rate = cpu_mhz > 0 ? (u32)cpu_mhz * 1000000 : k_machine_profiles[model].cpu_clock_rate;
    geartowns->SetMachineConfig(config);
}

void emu_audio_mute(bool mute)
{
    audio_enabled = !mute;

    if (IsValidPointer(geartowns))
        geartowns->GetAudio()->Mute(mute);

    emu_audio_reset();
}

void emu_audio_set_master_volume(float volume)
{
    if (IsValidPointer(geartowns))
        geartowns->GetAudio()->SetMasterVolume(volume);
}

void emu_audio_reset(void)
{
    sound_queue_stop();

    if (audio_enabled)
        sound_queue_start(GT_AUDIO_SAMPLE_RATE, 2, GT_AUDIO_QUEUE_SIZE, config_audio.buffer_count);
}

bool emu_is_audio_enabled(void)
{
    return audio_enabled;
}

bool emu_is_audio_open(void)
{
    return sound_queue_is_open();
}

void emu_save_state_slot(int index)
{
    if (!emu_is_empty())
    {
        const char* dir = get_configurated_dir(config_emulator.savestates_dir_option,
            config_emulator.savestates_path.c_str());
        char path[GT_MAX_PATH];

        if (get_floppy_state_path(index, path, sizeof(path)))
            geartowns->SaveState(path, -1, true);
        else
            geartowns->SaveState(dir, index, true);

        update_savestates_data();
    }
}

void emu_load_state_slot(int index)
{
    if (!emu_is_empty())
    {
        const char* dir = get_configurated_dir(config_emulator.savestates_dir_option,
            config_emulator.savestates_path.c_str());
        char path[GT_MAX_PATH];
        bool loaded = get_floppy_state_path(index, path, sizeof(path)) ? geartowns->LoadState(path, -1) :
            geartowns->LoadState(dir, index);

        if (loaded)
        {
            emu_floppy_reconcile();
            emu_debug_state_restored();
            events_sync_input();
            rewind_reset();
        }
    }
}

void emu_save_state_file(const char* file_path)
{
    if (!emu_is_empty())
        geartowns->SaveState(file_path, -1, true);
}

void emu_load_state_file(const char* file_path)
{
    if (!emu_is_empty() && geartowns->LoadState(file_path))
    {
        emu_floppy_reconcile();
        emu_debug_state_restored();
        events_sync_input();
        rewind_reset();
    }
}

void emu_debug_state_restored(void)
{
    geartowns->GetI386()->ResetDebuggerExecutionState();
    emu_debug_command = Debug_Command_None;
    emu_debug_step_frames_pending = 0;
    emu_debug_pc_changed = true;
}

void update_savestates_data(void)
{
    emu_savestates_generation++;

    for (int i = 0; i < 5; i++)
    {
        emu_savestates[i].rom_name[0] = 0;
        SafeDeleteArray(emu_savestates_screenshots[i].data);
        emu_savestates_screenshots[i].width = 0;
        emu_savestates_screenshots[i].height = 0;
        emu_savestates_screenshots[i].size = 0;
    }

    if (emu_is_empty())
        return;

    for (int i = 0; i < 5; i++)
    {
        const char* dir = get_configurated_dir(config_emulator.savestates_dir_option,
            config_emulator.savestates_path.c_str());
        char path[GT_MAX_PATH];
        int index = i + 1;

        if (get_floppy_state_path(index, path, sizeof(path)))
        {
            dir = path;
            index = -1;
        }

        if (!geartowns->GetSaveStateHeader(index, dir, &emu_savestates[i]))
            continue;

        if (emu_savestates[i].screenshot_size > 0)
        {
            emu_savestates_screenshots[i].data = new (std::nothrow) u8[emu_savestates[i].screenshot_size];

            if (!IsValidPointer(emu_savestates_screenshots[i].data))
                continue;

            emu_savestates_screenshots[i].size = emu_savestates[i].screenshot_size;
            geartowns->GetSaveStateScreenshot(index, dir, &emu_savestates_screenshots[i]);
        }
    }
}

void emu_get_runtime(GT_Runtime_Info& runtime)
{
    memset(&runtime, 0, sizeof(runtime));

    if (IsValidPointer(geartowns))
        geartowns->GetRuntimeInfo(runtime);
    else
    {
        runtime.screen_width = GT_FRAME_BUFFER_WIDTH;
        runtime.screen_height = GT_FRAME_BUFFER_HEIGHT;
        runtime.width_scale = 1;
        runtime.sample_rate = GT_AUDIO_SAMPLE_RATE;
    }
}

double emu_get_frame_rate(void)
{
    if (!IsValidPointer(geartowns))
        return 60.0;

    GT_Runtime_Info runtime;
    emu_get_runtime(runtime);

    return runtime.frame_time > 0.0f ? 1000.0 / runtime.frame_time : 60.0;
}

void emu_get_info(char* info, int buffer_size)
{
    if (!IsValidPointer(info) || buffer_size <= 0)
        return;

    if (emu_is_empty())
    {
        strncpy_fit(info, "No media selected", (size_t)buffer_size);
        return;
    }

    Media* media = geartowns->GetMedia();
    Firmware* firmware = geartowns->GetFirmware();

    if (media->IsCDROM())
    {
        CdRomMedia* cdrom_media = geartowns->GetCDROMMedia();
        GT_CdRomMSF length = cdrom_media->GetCdRomLength();
        snprintf(info, (size_t)buffer_size, "File Name: %s\nPath: %s\nCD-ROM Tracks: %d\nCD-ROM Length: %02d:%02d:%02d\n"
            "CRC: %08X\nFirmware Ready: %s", media->GetFileName(), media->GetFilePath(), cdrom_media->GetTrackCount(),
            length.minutes, length.seconds, length.frames, media->GetCRC(), firmware->IsReady() ? "Yes" : "No");
        return;
    }

    snprintf(info, (size_t)buffer_size, "File Name: %s\nPath: %s\nSize: %d bytes\nCRC: %08X\nFirmware Ready: %s",
        media->GetFileName(), media->GetFilePath(), media->GetSize(), media->GetCRC(),
        firmware->IsReady() ? "Yes" : "No");
}

GeartownsCore* emu_get_core(void)
{
    return geartowns;
}

void emu_debug_step_over(void)
{
    if (!IsValidPointer(geartowns))
        return;

    emu_debug_command = Debug_Command_StepOver;
    emu_resume();
}

void emu_debug_step_into(void)
{
    emu_debug_command = Debug_Command_Step;
    emu_resume();
}

void emu_debug_step_out(void)
{
    if (!IsValidPointer(geartowns))
        return;

    I386* cpu = geartowns->GetI386();
    const std::vector<I386_CallStackEntry>& call_stack = cpu->GetDisassemblerCallStack();

    if (!call_stack.empty())
    {
        cpu->AddRunToBreakpoint(call_stack.back().back_linear);
        emu_debug_command = Debug_Command_Continue;
    }
    else
        emu_debug_command = Debug_Command_Step;

    emu_resume();
}

void emu_debug_step_frame(void)
{
    emu_debug_step_frames(1);
}

void emu_debug_step_frames(int frames)
{
    if (frames < 1)
        frames = 1;

    emu_debug_step_frames_pending += frames;
    emu_debug_command = Debug_Command_StepFrame;
    emu_resume();
}

void emu_debug_break(void)
{
    emu_resume();
    emu_debug_step_frames_pending = 0;

    if (emu_debug_command == Debug_Command_Continue || emu_debug_command == Debug_Command_StepFrame)
        emu_debug_command = Debug_Command_Step;
    else
        emu_debug_command = Debug_Command_None;
}

void emu_debug_continue(void)
{
    emu_debug_command = Debug_Command_Continue;
    emu_resume();
}

void emu_set_disassembler_syntax(int syntax)
{
    UNUSED(syntax);
}

// Only the buffers of open windows are decoded
void emu_debug_update(void)
{
    if (emu_is_empty())
        return;

    if (config_debug.show_framebuffers)
    {
        Emu_Debug_Buffer_Request request;
        request.offset = (u32)config_debug.framebuffer_custom_offset;
        request.format = config_debug.framebuffer_custom_format + 1;
        request.width = config_debug.framebuffer_custom_width;
        request.height = config_debug.framebuffer_custom_height;
        request.palette = config_debug.framebuffer_custom_palette;

        int buffer = config_debug.framebuffer_tab;

        if (buffer == 2)
            buffer = config_debug.framebuffer_sprite_page == 0 ? Emu_Debug_Buffer_SpriteDisplay : Emu_Debug_Buffer_SpriteDraw;
        else if (buffer == 3)
            buffer = Emu_Debug_Buffer_Custom;

        emu_debug_decode_buffer(buffer, &request, emu_debug_framebuffer, emu_debug_framebuffer_info);
    }

    if (config_debug.show_sprites)
    {
        for (int i = 0; i < (int)k_sprite_entries; i++)
        {
            int x = (i & 31) * 16;
            int y = (i >> 5) * 16;
            emu_debug_decode_sprite(i, emu_debug_sprite_atlas + (y * EMU_DEBUG_SPRITE_ATLAS_SIZE + x) * 4,
                EMU_DEBUG_SPRITE_ATLAS_SIZE);
        }

        Emu_Debug_Buffer_Info info;
        debug_decode(Emu_Debug_Buffer_SpriteDisplay, NULL, emu_debug_sprite_page, EMU_DEBUG_SPRITE_PAGE_SIZE, info);
    }
}

// Pages are whole VRAM halves for layers, the sprite work halves, or a custom view of the two-page VRAM
void emu_debug_get_buffer_info(int buffer, const Emu_Debug_Buffer_Request* request, Emu_Debug_Buffer_Info& info)
{
    memset(&info, 0, sizeof(info));

    if (!IsValidPointer(geartowns))
        return;

    Video* video = geartowns->GetVideo();
    Video::Video_State* state = video->GetState();
    bool two_page = video->IsTwoPage();

    if (buffer == Emu_Debug_Buffer_Layer0 || buffer == Emu_Debug_Buffer_Layer1)
    {
        int layer = buffer;
        u32 unit = two_page ? 4 : 8;
        const u16* crtc = state->crtc;

        info.format = debug_layer_format(state, layer);
        info.single_page = !two_page;
        info.page_base = two_page ? (u32)layer << 18 : 0;
        info.page_size = two_page ? 0x40000 : 0x80000;
        info.stride = (u32)crtc[k_video_crtc_lo0 + layer * 4] * unit;

        if (info.stride == 0)
            info.stride = info.page_size / EMU_DEBUG_FRAMEBUFFER_HEIGHT;

        info.start = (u32)crtc[k_video_crtc_fa0 + layer * 4] * unit;

        if (layer == 0 && state->fmr_display_page)
            info.start += 0x20000;
        else if (layer == 1)
            info.start += video->GetSprite()->GetDisplayOffset();

        info.start &= info.page_size - 1;

        if (info.format == Video::VIDEO_LAYER_OFF)
            return;

        u32 pixels = info.format == Video::VIDEO_LAYER_4BPP ? info.stride * 2 :
            info.format == Video::VIDEO_LAYER_8BPP ? info.stride : info.stride / 2;
        info.width = (int)MIN(pixels, (u32)EMU_DEBUG_FRAMEBUFFER_WIDTH);
        info.height = (int)MIN(info.page_size / info.stride, (u32)EMU_DEBUG_FRAMEBUFFER_HEIGHT);

        u32 zoom = (u32)crtc[k_video_crtc_zoom] >> (layer * 8);
        u32 zoom_x = (zoom & 0x0F) + 1;
        u32 haj = crtc[k_video_crtc_haj0 + layer * 4];
        u32 hds = MAX((u32)crtc[k_video_crtc_hds0 + layer * 2], haj);
        u32 hde = crtc[k_video_crtc_hde0 + layer * 2];
        u32 vds = crtc[k_video_crtc_vds0 + layer * 2];
        u32 vde = crtc[k_video_crtc_vde0 + layer * 2];

        if (hde > hds && vde > vds)
        {
            info.window = true;
            info.window_x = (int)((info.start % info.stride) * pixels / info.stride + (hds - haj) / zoom_x);
            info.window_y = (int)(info.start / info.stride);
            info.window_width = (int)((hde - hds) / zoom_x);
            info.window_height = (int)(((vde - vds) / 2) / (((zoom >> 4) & 0x0F) + 1));
        }

        return;
    }

    if (buffer == Emu_Debug_Buffer_SpriteDisplay || buffer == Emu_Debug_Buffer_SpriteDraw)
    {
        bool draw_half = video->GetSprite()->GetPage();
        u32 half = buffer == Emu_Debug_Buffer_SpriteDraw ? (draw_half ? k_sprite_half_size : 0) :
            video->GetSprite()->GetDisplayOffset();

        info.format = Video::VIDEO_LAYER_16BPP;
        info.page_base = k_sprite_work_base + half;
        info.page_size = k_sprite_half_size;
        info.stride = 512;
        info.width = 256;
        info.height = 256;
        return;
    }

    if (!IsValidPointer(request))
        return;

    info.format = CLAMP(request->format, 1, 3);
    info.page_base = 0;
    info.page_size = VIDEO_VRAM_SIZE;
    info.start = request->offset & (VIDEO_VRAM_SIZE - 1);
    info.width = CLAMP(request->width, 1, EMU_DEBUG_FRAMEBUFFER_WIDTH);
    info.height = CLAMP(request->height, 1, EMU_DEBUG_FRAMEBUFFER_HEIGHT);
    info.stride = info.format == Video::VIDEO_LAYER_4BPP ? (u32)(info.width + 1) / 2 :
        info.format == Video::VIDEO_LAYER_8BPP ? (u32)info.width : (u32)info.width * 2;
}

void emu_debug_decode_buffer(int buffer, const Emu_Debug_Buffer_Request* request, u8* output,
    Emu_Debug_Buffer_Info& info)
{
    debug_decode(buffer, request, output, EMU_DEBUG_FRAMEBUFFER_WIDTH, info);
}

void emu_debug_get_sprite(int index, Emu_Debug_Sprite& sprite)
{
    memset(&sprite, 0, sizeof(sprite));

    if (!IsValidPointer(geartowns) || index < 0 || index >= (int)k_sprite_entries)
        return;

    Video* video = geartowns->GetVideo();
    Sprite::Sprite_State* state = video->GetSprite()->GetState();
    const u8* entry = video->GetSpriteRAM() + index * 8;
    u16 first = (u16)(((state->registers[k_sprite_control1] & 0x03) << 8) | state->registers[k_sprite_control0]);

    sprite.index = index;
    sprite.address = (u32)index * 8;
    sprite.x = read_u16_le(entry);
    sprite.y = read_u16_le(entry + 2);
    sprite.attributes = read_u16_le(entry + 4);
    sprite.color = read_u16_le(entry + 6);
    sprite.offset = (sprite.attributes & k_sprite_attribute_offset) != 0;
    sprite.swap = (sprite.attributes & k_sprite_attribute_swap) != 0;
    sprite.flip_x = (sprite.attributes & k_sprite_attribute_flip_x) != 0;
    sprite.flip_y = (sprite.attributes & k_sprite_attribute_flip_y) != 0;
    sprite.half_x = (sprite.attributes & k_sprite_attribute_half_x) != 0;
    sprite.half_y = (sprite.attributes & k_sprite_attribute_half_y) != 0;
    sprite.table = (sprite.color & k_sprite_color_table) != 0;
    sprite.through = (sprite.color & k_sprite_color_through) != 0;
    sprite.hide = (sprite.color & k_sprite_color_hide) != 0;
    sprite.pattern = (u16)(sprite.attributes & (sprite.table ? 0x03FF : 0x03FC));
    sprite.pattern_address = (u32)sprite.pattern << 7;
    sprite.color_table = (u16)(sprite.color & 0x0FFF);
    sprite.color_table_address = (u32)sprite.color_table << 5;

    u32 x = sprite.x;
    u32 y = sprite.y;

    if (sprite.offset)
    {
        x += ((state->registers[k_sprite_offset_x + 1] & 0x01) << 8) | state->registers[k_sprite_offset_x];
        y += ((state->registers[k_sprite_offset_y + 1] & 0x01) << 8) | state->registers[k_sprite_offset_y];
    }

    sprite.screen_x = (int)(x & 0x1FF);
    sprite.screen_y = (int)(y & 0x1FF);
    sprite.drawn = index >= first;
    sprite.visible = sprite.drawn && !sprite.hide && !((sprite.screen_x >= 256 && sprite.screen_x + 15 < 512) ||
        (sprite.screen_y >= 256 && sprite.screen_y + 15 < 512 + 2));
}

// 16x16 RGBA as the engine places it, transparent pixels as a checkerboard, stride in pixels
void emu_debug_decode_sprite(int index, u8* output, int stride)
{
    Emu_Debug_Sprite sprite;
    emu_debug_get_sprite(index, sprite);

    if (!IsValidPointer(output) || !IsValidPointer(geartowns))
        return;

    const u8* ram = geartowns->GetVideo()->GetSpriteRAM();
    const u8* pattern = ram + sprite.pattern_address;
    const u8* table = ram + sprite.color_table_address;
    u32* pixels = (u32*)output;
    int flip_x = sprite.flip_x ? 0x0F : 0x00;
    int flip_y = sprite.flip_y ? 0x0F : 0x00;

    for (int py = 0; py < 16; py++)
    {
        for (int px = 0; px < 16; px++)
        {
            int x = (sprite.swap ? py : px) ^ flip_x;
            int y = (sprite.swap ? px : py) ^ flip_y;
            u32 color = debug_checker(x, y);

            if (sprite.table)
            {
                u8 data = pattern[(py << 3) + (px >> 1)];
                int entry = (px & 0x01) != 0 ? data >> 4 : data & 0x0F;

                if (entry != 0)
                    color = debug_direct_color(read_u16_le(table + entry * 2));
            }
            else
            {
                u16 value = read_u16_le(pattern + (py << 5) + (px << 1));

                if ((value & 0x8000) == 0)
                    color = debug_direct_color(value);
            }

            pixels[y * stride + x] = color;
        }
    }
}

int emu_get_debug_buffer_png(int buffer, const Emu_Debug_Buffer_Request* request, unsigned char** out_buffer)
{
    *out_buffer = NULL;
    u8* pixels = new (std::nothrow) u8[EMU_DEBUG_FRAMEBUFFER_WIDTH * EMU_DEBUG_FRAMEBUFFER_HEIGHT * 4];

    if (!IsValidPointer(pixels))
        return 0;

    Emu_Debug_Buffer_Info info;
    emu_debug_decode_buffer(buffer, request, pixels, info);
    int size = 0;

    if (info.width > 0 && info.height > 0)
        *out_buffer = stbi_write_png_to_mem(pixels, EMU_DEBUG_FRAMEBUFFER_WIDTH * 4, info.width, info.height, 4, &size);

    SafeDeleteArray(pixels);
    return size;
}

int emu_get_sprite_png(int index, int scale, unsigned char** out_buffer)
{
    u32 sprite[16 * 16];
    scale = CLAMP(scale, 1, 16);
    int size = 16 * scale;
    u32* pixels = new (std::nothrow) u32[size * size];
    *out_buffer = NULL;

    if (!IsValidPointer(pixels))
        return 0;

    emu_debug_decode_sprite(index, (u8*)sprite, 16);

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
            pixels[y * size + x] = sprite[(y / scale) * 16 + (x / scale)];
    }

    int length = 0;
    *out_buffer = stbi_write_png_to_mem((const unsigned char*)pixels, size * 4, size, size, 4, &length);
    SafeDeleteArray(pixels);
    return length;
}

void emu_set_pad_type(GT_Controllers controller, GT_Controller_Type type)
{
    if (IsValidPointer(geartowns))
        geartowns->GetInput()->SetControllerType((int)controller, type);
}

GT_Controller_Type emu_get_pad_type(GT_Controllers controller)
{
    if (!IsValidPointer(geartowns))
        return GT_CONTROLLER_NONE;

    return geartowns->GetInput()->GetControllerType((int)controller);
}

bool emu_save_screenshot(const char* file_path)
{
    if (!IsValidPointer(file_path) || file_path[0] == '\0' || !IsValidPointer(emu_frame_buffer))
    {
        return false;
    }

    GT_Runtime_Info runtime;
    emu_get_runtime(runtime);
    int result = stbi_write_png(file_path, runtime.screen_width, runtime.screen_height, 4, emu_frame_buffer,
        runtime.screen_width * 4);

    if (result != 0)
        Log("Screenshot saved to %s", file_path);
    else
        Error("Unable to save screenshot to %s", file_path);

    return result != 0;
}

int emu_get_screenshot_png(unsigned char** out_buffer)
{
    if (!IsValidPointer(out_buffer) || !IsValidPointer(emu_frame_buffer))
        return 0;

    *out_buffer = NULL;
    GT_Runtime_Info runtime;
    emu_get_runtime(runtime);
    int size = 0;
    *out_buffer = stbi_write_png_to_mem(emu_frame_buffer, runtime.screen_width * 4, runtime.screen_width,
        runtime.screen_height, 4, &size);
    return size;
}

bool emu_start_video_recording(const char* file_path)
{
    if (emu_is_empty())
        return false;

    if (video_recorder_is_recording())
        emu_stop_video_recording();

    GT_Runtime_Info runtime;
    emu_get_runtime(runtime);

    int width = 0;
    int height = 0;
    get_video_recording_size(runtime, &width, &height);

    if (!video_recorder_start(file_path, width, height, emu_get_frame_rate(), GT_AUDIO_SAMPLE_RATE,
        (Video_Recorder_Quality)config_video.recording_quality))
        return false;

    Log("Video recording started: %s (%dx%d)", file_path, width, height);
    return true;
}

void emu_stop_video_recording(void)
{
    if (video_recorder_is_recording())
    {
        video_recorder_stop();
        Log("Video recording stopped");
    }
}

bool emu_is_video_recording(void)
{
    return video_recorder_is_recording();
}

static void get_video_recording_size(const GT_Runtime_Info& runtime, int* width, int* height)
{
    int selected_ratio = config_video.ratio;
    float ratio = 0.0f;

    if (config_video.recording_ratio > 0)
        selected_ratio = config_video.recording_ratio - 1;

    switch (selected_ratio)
    {
        case 1:
            ratio = 4.0f / 3.0f;
            break;
        case 2:
            ratio = 16.0f / 9.0f;
            break;
        case 3:
            ratio = 16.0f / 10.0f;
            break;
        case 4:
            ratio = 6.0f / 5.0f;
            break;
        default:
            ratio = ((float)runtime.screen_width / (float)runtime.width_scale) / (float)runtime.screen_height;
    }

    *height = runtime.screen_height * config_video.recording_scale;
    *width = (int)roundf((float)*height * ratio);
    *width += *width & 1;
    *height += *height & 1;
}

bool emu_load_bios(const char* path)
{
    if (!IsValidPointer(geartowns))
        return false;

    return geartowns->LoadBios(path);
}

void emu_mcp_set_transport(int mode, int tcp_port, const char* tcp_address)
{
    if (IsValidPointer(mcp_manager))
        mcp_manager->SetTransportMode((McpTransportMode)mode, tcp_port, tcp_address);
}

void emu_mcp_start(void)
{
    if (IsValidPointer(mcp_manager))
        mcp_manager->Start();
}

void emu_mcp_stop(void)
{
    if (IsValidPointer(mcp_manager))
        mcp_manager->Stop();
}

bool emu_mcp_is_running(void)
{
    return IsValidPointer(mcp_manager) && mcp_manager->IsRunning();
}

int emu_mcp_get_transport_mode(void)
{
    return IsValidPointer(mcp_manager) ? mcp_manager->GetTransportMode() : 0;
}

const char* emu_mcp_get_http_address(void)
{
    return IsValidPointer(mcp_manager) ? mcp_manager->GetTcpAddress() : "127.0.0.1";
}

int emu_mcp_get_http_port(void)
{
    return IsValidPointer(mcp_manager) ? mcp_manager->GetTcpPort() : 7777;
}

void emu_mcp_pump_commands(void)
{
    if (IsValidPointer(mcp_manager))
        mcp_manager->PumpCommands(geartowns);
}

void emu_reset_rewind_timing(void)
{
    reset_rewind_timing();
}

static void load_media_thread_func(void)
{
#if defined(GT_ENABLE_PHYSICAL_CDROM)
    if (loading_request_type == Loading_Request_PhysicalCdRom)
        loading_result = geartowns->LoadPhysicalCdRom(loading_file_path);
    else
#endif
        loading_result = geartowns->LoadMedia(loading_file_path);

    loading_state.store(Loading_State_Finished);
}

static void reset_buffers(void)
{
    size_t frame_buffer_size = (size_t)GT_MAX_FRAME_BUFFER_WIDTH * GT_MAX_FRAME_BUFFER_HEIGHT * 4;
    memset(emu_frame_buffer, 0, frame_buffer_size);
    memset(audio_buffer, 0, GT_AUDIO_BUFFER_SIZE * sizeof(s16));
}

static void reset_run_state(void)
{
    emu_debug_command = Debug_Command_None;
    emu_debug_pc_changed = true;
    emu_frame_counter = 0;
    reset_buffers();
    emu_audio_reset();
    rewind_reset();
}

static const char* get_configurated_dir(int location, const char* path)
{
    switch ((Directory_Location)location)
    {
        default:
        case Directory_Location_Default:
            return config_root_path;
        case Directory_Location_ROM:
#if defined(GT_ENABLE_PHYSICAL_CDROM)
            if (geartowns->GetMedia()->IsPhysicalCdRom())
                return config_root_path;
#endif
            return NULL;
        case Directory_Location_Custom:
            return path;
    }
}

static bool unload_media(void)
{
    if (loading_state.load() != Loading_State_None)
        return false;

    if (!geartowns->GetMedia()->IsReady())
        return false;

    emu_debug_command = Debug_Command_None;
    reset_buffers();
    emu_audio_reset();
    geartowns->EjectMedia();
    rewind_reset();
    update_savestates_data();
    return true;
}

static bool get_floppy_state_path(int index, char* path, size_t path_size)
{
    if (geartowns->GetMedia()->IsReady())
        return false;

    const char* content = emu_floppy_get_content_path();
    char directory[GT_MAX_PATH];
    char name[GT_MAX_PATH];
    char file_name[GT_MAX_PATH];

    switch ((Directory_Location)config_emulator.savestates_dir_option)
    {
        case Directory_Location_ROM:
            if (content[0] != '\0')
                get_directory(content, directory, sizeof(directory));
            else
                strncpy_fit(directory, config_root_path, sizeof(directory));
            break;
        case Directory_Location_Custom:
            strncpy_fit(directory, config_emulator.savestates_path.c_str(), sizeof(directory));
            break;
        default:
            strncpy_fit(directory, config_root_path, sizeof(directory));
            break;
    }

    if (content[0] != '\0')
        get_filename_without_extension(content, name, sizeof(name));
    else
        strncpy_fit(name, "FM Towns", sizeof(name));

    snprintf(file_name, sizeof(file_name), "%s.state%d", name, index);
    return join_path(directory, file_name, path, path_size);
}

#if defined(GT_ENABLE_PHYSICAL_CDROM)
static void stop_physical_cdrom_after_error(void)
{
    char device_id[256];
    strncpy_fit(device_id, geartowns->GetMedia()->GetPhysicalCdRomDeviceId(), sizeof(device_id));

    if (!unload_media())
        return;

    Error("Physical CD-ROM media error on %s, disc ejected", device_id);
}
#endif

static void reset_rewind_timing(void)
{
    rewind_last_counter = 0;
    rewind_pop_accumulator = 0.0;
}

static int get_rewind_pop_budget(void)
{
    Uint64 now = SDL_GetPerformanceCounter();

    if (rewind_last_counter == 0)
    {
        rewind_last_counter = now;
        return 0;
    }

    double elapsed = (double)(now - rewind_last_counter) / (double)SDL_GetPerformanceFrequency();
    rewind_last_counter = now;

    if (elapsed < 0.0)
        elapsed = 0.0;
    else if (elapsed > 0.25)
        elapsed = 0.25;

    int frames_per_snapshot = rewind_get_frames_per_snapshot();

    if (frames_per_snapshot < 1)
        frames_per_snapshot = 1;

    double snapshots_per_second = (60.0 * (double)config_rewind.speed) / (double)frames_per_snapshot;
    rewind_pop_accumulator += elapsed * snapshots_per_second;

    int to_pop = (int)rewind_pop_accumulator;

    if (to_pop > 0)
        rewind_pop_accumulator -= (double)to_pop;

    return to_pop;
}

static bool init_debug(void)
{
    emu_debug_framebuffer = new (std::nothrow) u8[EMU_DEBUG_FRAMEBUFFER_WIDTH * EMU_DEBUG_FRAMEBUFFER_HEIGHT * 4];
    emu_debug_sprite_atlas = new (std::nothrow) u8[EMU_DEBUG_SPRITE_ATLAS_SIZE * EMU_DEBUG_SPRITE_ATLAS_SIZE * 4];
    emu_debug_sprite_page = new (std::nothrow) u8[EMU_DEBUG_SPRITE_PAGE_SIZE * EMU_DEBUG_SPRITE_PAGE_SIZE * 4];
    memset(&emu_debug_framebuffer_info, 0, sizeof(emu_debug_framebuffer_info));

    if (!IsValidPointer(emu_debug_framebuffer) || !IsValidPointer(emu_debug_sprite_atlas) ||
        !IsValidPointer(emu_debug_sprite_page))
    {
        destroy_debug();
        return false;
    }

    memset(emu_debug_framebuffer, 0, EMU_DEBUG_FRAMEBUFFER_WIDTH * EMU_DEBUG_FRAMEBUFFER_HEIGHT * 4);
    memset(emu_debug_sprite_atlas, 0, EMU_DEBUG_SPRITE_ATLAS_SIZE * EMU_DEBUG_SPRITE_ATLAS_SIZE * 4);
    memset(emu_debug_sprite_page, 0, EMU_DEBUG_SPRITE_PAGE_SIZE * EMU_DEBUG_SPRITE_PAGE_SIZE * 4);
    return true;
}

static void destroy_debug(void)
{
    SafeDeleteArray(emu_debug_framebuffer);
    SafeDeleteArray(emu_debug_sprite_atlas);
    SafeDeleteArray(emu_debug_sprite_page);
}

static u32 debug_rgba(u32 red, u32 green, u32 blue)
{
    return red | (green << 8) | (blue << 16) | 0xFF000000U;
}

// GRB555, green on top
static u32 debug_direct_color(u16 value)
{
    u32 green = (value >> 10) & 0x1F;
    u32 red = (value >> 5) & 0x1F;
    u32 blue = value & 0x1F;
    return debug_rgba((red << 3) | (red >> 2), (green << 3) | (green >> 2), (blue << 3) | (blue >> 2));
}

static u32 debug_checker(int x, int y)
{
    return ((x >> 2) + (y >> 2)) & 1 ? debug_rgba(0x60, 0x60, 0x60) : debug_rgba(0x90, 0x90, 0x90);
}

static u32 debug_single_to_canonical(u32 offset)
{
    return ((offset & 0x00004) << 16) | ((offset & 0x7FFF8) >> 1) | (offset & 0x00003);
}

// The format the output controller selects, whether or not FDA0 shows the layer
static int debug_layer_format(const Video::Video_State* state, int layer)
{
    u8 output = state->output[0];

    if ((output & 0x10) != 0)
    {
        switch ((output >> (layer * 2)) & 0x03)
        {
            case 0x01: return Video::VIDEO_LAYER_4BPP;
            case 0x03: return Video::VIDEO_LAYER_16BPP;
            default: return Video::VIDEO_LAYER_OFF;
        }
    }

    if (layer != 0)
        return Video::VIDEO_LAYER_OFF;

    switch (output & 0x0F)
    {
        case 0x0A: return Video::VIDEO_LAYER_8BPP;
        case 0x0F: return Video::VIDEO_LAYER_16BPP;
        default: return Video::VIDEO_LAYER_OFF;
    }
}

// Palettes 0 and 1 are the 16 color banks, 2 the 256 color one, components in port order blue, red, green
static void debug_palette(const Video::Video_State* state, int palette, u32* colors)
{
    if (palette == 2)
    {
        for (int i = 0; i < 256; i++)
        {
            const u8* color = state->palette256[i];
            colors[i] = debug_rgba(color[1], color[2], color[0]);
        }

        return;
    }

    for (int i = 0; i < 16; i++)
    {
        const u8* color = state->palette16[palette & 1][i];
        colors[i] = debug_rgba(color[1] | (color[1] >> 4), color[2] | (color[2] >> 4), color[0] | (color[0] >> 4));
    }
}

static void debug_decode(int buffer, const Emu_Debug_Buffer_Request* request, u8* output, int stride,
    Emu_Debug_Buffer_Info& info)
{
    emu_debug_get_buffer_info(buffer, request, info);

    if (!IsValidPointer(output) || !IsValidPointer(geartowns))
        return;

    Video::Video_State* state = geartowns->GetVideo()->GetState();
    const u8* vram = state->vram;
    u32* pixels = (u32*)output;
    u32 colors[256];
    int palette = buffer == Emu_Debug_Buffer_Custom && IsValidPointer(request) ? request->palette : buffer == 1 ? 1 : 0;

    debug_palette(state, info.format == Video::VIDEO_LAYER_8BPP ? 2 : palette, colors);

    bool custom = buffer == Emu_Debug_Buffer_Custom;
    bool sprites = buffer == Emu_Debug_Buffer_SpriteDisplay || buffer == Emu_Debug_Buffer_SpriteDraw;
    u32 base = custom ? info.start : 0;

    for (int y = 0; y < info.height; y++)
    {
        u32* line = pixels + y * stride;

        for (int x = 0; x < info.width; x++)
        {
            u32 offset = base + (u32)y * info.stride;

            if (info.format == Video::VIDEO_LAYER_4BPP)
                offset += (u32)x >> 1;
            else if (info.format == Video::VIDEO_LAYER_8BPP)
                offset += (u32)x;
            else
                offset += (u32)x * 2;

            u32 canonical = info.single_page ? debug_single_to_canonical(offset & (info.page_size - 1)) :
                info.page_base + (offset & (info.page_size - 1));

            if (info.format == Video::VIDEO_LAYER_4BPP)
            {
                u8 data = vram[canonical];
                line[x] = colors[(x & 1) ? data >> 4 : data & 0x0F];
            }
            else if (info.format == Video::VIDEO_LAYER_8BPP)
                line[x] = colors[vram[canonical]];
            else
            {
                u32 next = info.single_page ? debug_single_to_canonical((offset + 1) & (info.page_size - 1)) :
                    info.page_base + ((offset + 1) & (info.page_size - 1));
                u16 value = (u16)(vram[canonical] | (vram[next] << 8));
                line[x] = sprites && (value & 0x8000) != 0 ? debug_checker(x, y) : debug_direct_color(value);
            }
        }
    }
}
