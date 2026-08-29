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
#include <new>
#include <string.h>
#include <thread>
#include "config.h"
#include "events.h"
#include "mcp/mcp_manager.h"
#include "rewind.h"
#include "runahead.h"
#include "sound_queue.h"
#include "utils.h"

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

static void load_media_thread_func(void);
static void reset_buffers(void);
static const char* get_configurated_dir(int option, const char* path);
static void reset_rewind_timing(void);
static int get_rewind_pop_budget(void);

bool emu_init(void)
{
    size_t frame_buffer_size = (size_t)GT_MAX_FRAME_BUFFER_WIDTH *
        GT_MAX_FRAME_BUFFER_HEIGHT * 4;
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

    sound_queue_init();

    for (int i = 0; i < 5; i++)
        InitPointer(emu_savestates_screenshots[i].data);

    emu_savestates_generation = 0;
    emu_debug_command = Debug_Command_None;
    emu_debug_pc_changed = false;
    emu_debug_step_frames_pending = 0;
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
    runahead_destroy();
    rewind_destroy();
    SafeDelete(mcp_manager);
    sound_queue_destroy();
    SafeDelete(geartowns);
    SafeDeleteArray(audio_buffer);
    SafeDeleteArray(emu_frame_buffer);

    for (int i = 0; i < 5; i++)
        SafeDeleteArray(emu_savestates_screenshots[i].data);
}

void emu_update(void)
{
    if (loading_state.load() != Loading_State_None)
        return;

    emu_mcp_pump_commands();

    if (emu_is_empty())
        return;

    int sample_count = 0;
    bool frame_executed = false;
    bool frame_completed = false;

    if (rewind_is_active())
    {
        int to_pop = get_rewind_pop_budget();

        for (int i = 0; i < to_pop; i++)
        {
            if (!rewind_pop())
                break;
        }

        int silence_count = GT_AUDIO_QUEUE_SIZE;
        memset(audio_buffer, 0, silence_count * sizeof(s16));
        sound_queue_write(audio_buffer, silence_count, false);
        return;
    }

    reset_rewind_timing();

    if (config_debug.debug)
    {
        if (emu_debug_command != Debug_Command_None && !geartowns->IsPaused())
        {
            rewind_commit_seek();
            GT_Run_Result result = geartowns->RunToFrame(emu_frame_buffer,
                audio_buffer, &sample_count);
            frame_executed = result != GT_RUN_NOT_READY;
            frame_completed = result == GT_RUN_FRAME_READY;
        }

        if (emu_debug_command == Debug_Command_StepFrame &&
            emu_debug_step_frames_pending > 0)
        {
            emu_debug_step_frames_pending--;
            if (emu_debug_step_frames_pending == 0)
                emu_debug_command = Debug_Command_None;
        }
        else if (emu_debug_command != Debug_Command_Continue)
            emu_debug_command = Debug_Command_None;
    }
    else if (!geartowns->IsPaused())
    {
        rewind_commit_seek();

        GT_Run_Result result = GT_RUN_NOT_READY;
        int runahead = geartowns->GetFirmware()->IsReady() ?
            runahead_get_frames() : 0;

        if (runahead > 0)
        {
            runahead_run(runahead, emu_frame_buffer, audio_buffer, &sample_count);
            result = GT_RUN_FRAME_READY;
        }
        else
            result = geartowns->RunToFrame(emu_frame_buffer, audio_buffer,
                &sample_count);

        frame_executed = result != GT_RUN_NOT_READY;
        frame_completed = result == GT_RUN_FRAME_READY;
    }

    if (frame_executed)
    {
        if (frame_completed)
            emu_frame_counter++;
        rewind_push();
    }

    if (sample_count > 0 && !geartowns->IsPaused())
        sound_queue_write(audio_buffer, sample_count, emu_audio_sync);
    else if (geartowns->IsPaused())
    {
        int silence_count = GT_AUDIO_QUEUE_SIZE;
        memset(audio_buffer, 0, silence_count * sizeof(s16));
        sound_queue_write(audio_buffer, silence_count, false);
    }
}

void emu_load_media_async(const char* file_path)
{
    if (!IsValidPointer(file_path) || file_path[0] == '\0' ||
        loading_state.load() != Loading_State_None)
    {
        return;
    }

    if (loading_thread_active)
    {
        loading_thread.join();
        loading_thread_active = false;
    }

    strncpy_fit(loading_file_path, file_path, sizeof(loading_file_path));
    loading_result = false;
    loading_state.store(Loading_State_Loading);
    loading_thread = std::thread(load_media_thread_func);
    loading_thread_active = true;
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
        return false;

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

void emu_set_mouse_delta(int x, int y)
{
    if (IsValidPointer(geartowns))
        geartowns->GetInput()->SetMouseDelta(x, y);
}

void emu_set_mouse_buttons(bool left, bool right)
{
    if (IsValidPointer(geartowns))
        geartowns->GetInput()->SetMouseButtons(left, right);
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

    return !IsValidPointer(geartowns) || !geartowns->GetMedia()->IsReady();
}

void emu_reset(void)
{
    if (!IsValidPointer(geartowns))
        return;

    geartowns->ResetMedia();
    emu_debug_command = Debug_Command_None;
    emu_frame_counter = 0;
    reset_buffers();
    emu_audio_reset();
    rewind_reset();
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

    if (IsValidPointer(geartowns))
        geartowns->ResetSound();

    if (audio_enabled)
        sound_queue_start(GT_AUDIO_SAMPLE_RATE, 2, GT_AUDIO_QUEUE_SIZE,
            config_audio.buffer_count);
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
        if (geartowns->LoadState(dir, index))
        {
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
        events_sync_input();
        rewind_reset();
    }
}

void update_savestates_data(void)
{
    emu_savestates_generation++;

    if (emu_is_empty())
        return;

    for (int i = 0; i < 5; i++)
    {
        emu_savestates[i].rom_name[0] = 0;
        SafeDeleteArray(emu_savestates_screenshots[i].data);
        emu_savestates_screenshots[i].width = 0;
        emu_savestates_screenshots[i].height = 0;
        emu_savestates_screenshots[i].size = 0;

        const char* dir = get_configurated_dir(config_emulator.savestates_dir_option,
            config_emulator.savestates_path.c_str());

        if (!geartowns->GetSaveStateHeader(i + 1, dir, &emu_savestates[i]))
            continue;

        if (emu_savestates[i].screenshot_size > 0)
        {
            emu_savestates_screenshots[i].data =
                new (std::nothrow) u8[emu_savestates[i].screenshot_size];

            if (!IsValidPointer(emu_savestates_screenshots[i].data))
                continue;

            emu_savestates_screenshots[i].size = emu_savestates[i].screenshot_size;
            geartowns->GetSaveStateScreenshot(i + 1, dir,
                &emu_savestates_screenshots[i]);
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
    return 60.0;
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
    snprintf(info, (size_t)buffer_size,
        "File Name: %s\nPath: %s\nSize: %d bytes\nCRC: %08X\nFirmware Ready: %s",
        media->GetFileName(), media->GetFilePath(), media->GetSize(), media->GetCRC(),
        firmware->IsReady() ? "Yes" : "No");
}

GeartownsCore* emu_get_core(void)
{
    return geartowns;
}

void emu_debug_step_over(void)
{
    emu_debug_command = Debug_Command_Step;
}

void emu_debug_step_into(void)
{
    emu_debug_command = Debug_Command_Step;
}

void emu_debug_step_out(void)
{
    emu_debug_command = Debug_Command_Step;
}

void emu_debug_step_frame(void)
{
    emu_debug_step_frames(1);
}

void emu_debug_step_frames(int frames)
{
    if (frames <= 0)
        return;

    emu_debug_step_frames_pending = frames;
    emu_debug_command = Debug_Command_StepFrame;
}

void emu_debug_break(void)
{
    emu_debug_command = Debug_Command_None;
}

void emu_debug_continue(void)
{
    emu_debug_command = Debug_Command_Continue;
}

void emu_set_disassembler_syntax(int syntax)
{
    UNUSED(syntax);
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
    if (!IsValidPointer(file_path) || file_path[0] == '\0' ||
        !IsValidPointer(emu_frame_buffer))
    {
        return false;
    }

    GT_Runtime_Info runtime;
    emu_get_runtime(runtime);
    int result = stbi_write_png(file_path, runtime.screen_width, runtime.screen_height,
        4, emu_frame_buffer, runtime.screen_width * 4);

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
    *out_buffer = stbi_write_png_to_mem(emu_frame_buffer, runtime.screen_width * 4,
        runtime.screen_width, runtime.screen_height, 4, &size);
    return size;
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
    loading_result = geartowns->LoadMedia(loading_file_path);
    loading_state.store(Loading_State_Finished);
}

static void reset_buffers(void)
{
    size_t frame_buffer_size = (size_t)GT_MAX_FRAME_BUFFER_WIDTH *
        GT_MAX_FRAME_BUFFER_HEIGHT * 4;
    memset(emu_frame_buffer, 0, frame_buffer_size);
    memset(audio_buffer, 0, GT_AUDIO_BUFFER_SIZE * sizeof(s16));
}

static const char* get_configurated_dir(int location, const char* path)
{
    switch ((Directory_Location)location)
    {
        default:
        case Directory_Location_Default:
            return config_root_path;
        case Directory_Location_ROM:
            return NULL;
        case Directory_Location_Custom:
            return path;
    }
}

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

    double elapsed = (double)(now - rewind_last_counter) /
        (double)SDL_GetPerformanceFrequency();
    rewind_last_counter = now;

    if (elapsed < 0.0)
        elapsed = 0.0;
    else if (elapsed > 0.25)
        elapsed = 0.25;

    int frames_per_snapshot = rewind_get_frames_per_snapshot();
    if (frames_per_snapshot < 1)
        frames_per_snapshot = 1;

    double snapshots_per_second = (60.0 * (double)config_rewind.speed) /
        (double)frames_per_snapshot;
    rewind_pop_accumulator += elapsed * snapshots_per_second;

    int to_pop = (int)rewind_pop_accumulator;
    if (to_pop > 0)
        rewind_pop_accumulator -= (double)to_pop;

    return to_pop;
}
