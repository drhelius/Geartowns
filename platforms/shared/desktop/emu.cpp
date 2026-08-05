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

#include <thread>
#include <atomic>
#include <string.h>
#include "geartowns.h"
#include "sound_queue.h"
#include "config.h"
#include "mcp/mcp_manager.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(_WIN32)
#define STBIW_WINDOWS_UTF8
#endif
#include "stb_image_write.h"

static const int k_screen_width = 640;
static const int k_screen_height = 480;

static GeartownsCore* geartowns;
static s16* audio_buffer;
static bool audio_enabled;
static McpManager* mcp_manager;

enum Loading_State
{
    Loading_State_None = 0,
    Loading_State_Loading,
    Loading_State_Finished
};

static std::atomic<int> loading_state(Loading_State_None);
static std::thread loading_thread;
static bool loading_thread_active;
static bool loading_result;
static char loading_file_path[4096];

bool emu_init(GT_Input_Pump_Fn input_pump_fn)
{
    emu_frame_buffer = new u8[k_screen_width * k_screen_height * 4];
    memset(emu_frame_buffer, 0, k_screen_width * k_screen_height * 4);

    audio_buffer = new s16[GT_AUDIO_BUFFER_SIZE];
    memset(audio_buffer, 0, GT_AUDIO_BUFFER_SIZE * sizeof(s16));

    geartowns = new GeartownsCore();
    geartowns->Init(input_pump_fn);
    geartowns->GetMedia()->SetTempPath(config_temp_path);

    audio_enabled = true;
    emu_audio_sync = true;
    emu_frame_counter = 0;
    emu_fps = 60.0f;

    sound_queue_init();

    mcp_manager = new McpManager();
    mcp_manager->Init(geartowns);

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
    SafeDelete(mcp_manager);
    sound_queue_destroy();
    SafeDelete(geartowns);
    SafeDeleteArray(audio_buffer);
    SafeDeleteArray(emu_frame_buffer);
}

static void load_media_thread_func(void)
{
    loading_result = geartowns->LoadMedia(loading_file_path);
    loading_state.store(Loading_State_Finished);
}

void emu_update(void)
{
    emu_mcp_pump_commands();

    if (!geartowns || emu_is_media_loading() || emu_is_empty() || geartowns->IsPaused())
        return;

    int sampleCount = 0;
    geartowns->RunToFrame(emu_frame_buffer, audio_buffer, &sampleCount);
    emu_frame_counter++;

    if ((sampleCount > 0) && !geartowns->IsPaused())
    {
        sound_queue_write(audio_buffer, sampleCount, emu_audio_sync);
    }
}

void emu_load_media_async(const char* file_path)
{
    if (!geartowns || !file_path || loading_state.load() != Loading_State_None)
        return;

    strncpy_fit(loading_file_path, file_path, sizeof(loading_file_path));
    loading_result = false;
    loading_state.store(Loading_State_Loading);

    if (loading_thread_active)
        loading_thread.join();

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

    emu_audio_reset();

    return true;
}

void emu_key_pressed(GT_Keys key)
{
    if (geartowns)
        geartowns->KeyPressed(key);
}

void emu_key_released(GT_Keys key)
{
    if (geartowns)
        geartowns->KeyReleased(key);
}

void emu_set_gamepad_state(int port, const GT_GamePad_State& state)
{
    if (geartowns && geartowns->GetInput())
        geartowns->GetInput()->SetGamePadState(port, state);
}

void emu_pause(void)
{
    if (geartowns)
        geartowns->Pause(true);
}

void emu_resume(void)
{
    if (geartowns)
        geartowns->Pause(false);
}

bool emu_is_paused(void)
{
    return !geartowns || geartowns->IsPaused();
}

bool emu_is_debug_idle(void)
{
    return false;
}

bool emu_is_empty(void)
{
    return !geartowns || emu_is_media_loading() || !geartowns->GetMedia()->IsReady();
}

void emu_reset(void)
{
    if (geartowns)
    {
        emu_audio_reset();
        geartowns->ResetMedia(false);
    }
}

void emu_audio_mute(bool mute)
{
    audio_enabled = !mute;
    if (geartowns)
        geartowns->GetAudio()->Mute(mute);
}

void emu_audio_set_master_volume(float volume)
{
    if (geartowns)
        geartowns->GetAudio()->SetMasterVolume(volume);
}

void emu_audio_fm_volume(float volume)
{
    if (geartowns)
        geartowns->GetAudio()->SetFMVolume(volume);
}

void emu_audio_pcm_volume(float volume)
{
    if (geartowns)
        geartowns->GetAudio()->SetPCMVolume(volume);
}

void emu_audio_cd_volume(float volume)
{
    if (geartowns)
        geartowns->GetAudio()->SetCDVolume(volume);
}

void emu_audio_highres_pcm_volume(float volume)
{
    if (geartowns)
        geartowns->GetAudio()->SetHighResPCMVolume(volume);
}

void emu_audio_reset(void)
{
    sound_queue_stop();
    int buffer_count = CLAMP(config_audio.buffer_count, 1, 8);
    sound_queue_start(GT_AUDIO_SAMPLE_RATE, 2, GT_AUDIO_QUEUE_SIZE, buffer_count);
}

bool emu_is_audio_enabled(void)
{
    return audio_enabled;
}

bool emu_is_audio_open(void)
{
    return sound_queue_is_open();
}

void emu_get_runtime(GT_Runtime_Info& runtime)
{
    runtime.screen_width = k_screen_width;
    runtime.screen_height = k_screen_height;
    runtime.width_scale = 1;
    runtime.sample_rate = GT_AUDIO_SAMPLE_RATE;
    runtime.media_ready = !emu_is_empty();
    runtime.cdrom_ready = geartowns && geartowns->GetMedia()->IsCdRomReady();
    runtime.paused = emu_is_paused();
    runtime.towns_time_ns = 0;
}

void emu_get_info(char* info, int buffer_size)
{
    if (!info || buffer_size <= 0)
        return;

    const char* file_name = (!emu_is_media_loading() && geartowns) ? geartowns->GetMedia()->GetFileName() : "";
    snprintf(info, (size_t)buffer_size, "File Name: %s\nScreen Resolution: %dx%d", file_name, k_screen_width, k_screen_height);
}

GeartownsCore* emu_get_core(void)
{
    return geartowns;
}

void emu_set_pad_type(int port, GT_Controller_Type type)
{
    if (geartowns && geartowns->GetInput())
        geartowns->GetInput()->SetControllerType(port, type);
}

GT_Controller_Type emu_get_pad_type(int port)
{
    if (!geartowns || !geartowns->GetInput())
        return GT_CONTROLLER_NONE;

    return geartowns->GetInput()->GetControllerType(port);
}

void emu_save_screenshot(const char* file_path)
{
    if (!file_path || !emu_frame_buffer || !geartowns || !geartowns->GetMedia() || !geartowns->GetMedia()->IsReady())
        return;

    stbi_write_png(file_path, k_screen_width, k_screen_height, 4, emu_frame_buffer, k_screen_width * 4);

    Log("Screenshot saved to %s", file_path);
}

int emu_get_screenshot_png(unsigned char** out_buffer)
{
    if (!out_buffer || !emu_frame_buffer || !geartowns || !geartowns->GetMedia() || !geartowns->GetMedia()->IsReady())
        return 0;

    int stride = k_screen_width * 4;
    int len = 0;

    *out_buffer = stbi_write_png_to_mem(emu_frame_buffer, stride,
                                        k_screen_width, k_screen_height,
                                        4, &len);

    return len;
}

bool emu_load_bios(const char* path)
{
    return geartowns && geartowns->LoadBios(path);
}

void emu_mcp_set_transport(int mode, int tcp_port, const char* tcp_address)
{
    if (mcp_manager)
        mcp_manager->SetTransportMode((McpTransportMode)mode, tcp_port, tcp_address);
}

void emu_mcp_start(void)
{
    if (mcp_manager)
        mcp_manager->Start();
}

void emu_mcp_stop(void)
{
    if (mcp_manager)
        mcp_manager->Stop();
}

bool emu_mcp_is_running(void)
{
    return mcp_manager && mcp_manager->IsRunning();
}

int emu_mcp_get_transport_mode(void)
{
    return mcp_manager ? mcp_manager->GetTransportMode() : -1;
}

void emu_mcp_pump_commands(void)
{
    if (mcp_manager && geartowns && mcp_manager->IsRunning())
        mcp_manager->PumpCommands(geartowns);
}
