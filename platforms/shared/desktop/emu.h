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

#ifndef EMU_H
#define EMU_H

#include "geartowns.h"

#ifdef EMU_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

EXTERN u8* emu_frame_buffer;
EXTERN u64 emu_frame_counter;
EXTERN float emu_fps;
EXTERN bool emu_audio_sync;

EXTERN bool emu_init(GT_Input_Pump_Fn input_pump_fn);
EXTERN void emu_destroy(void);
EXTERN void emu_update(void);
EXTERN void emu_load_media_async(const char* file_path);
EXTERN bool emu_is_media_loading(void);
EXTERN bool emu_finish_media_loading(void);
EXTERN void emu_key_pressed(GT_Keys key);
EXTERN void emu_key_released(GT_Keys key);
EXTERN void emu_set_gamepad_state(int port, const GT_GamePad_State& state);
EXTERN void emu_pause(void);
EXTERN void emu_resume(void);
EXTERN bool emu_is_paused(void);
EXTERN bool emu_is_debug_idle(void);
EXTERN bool emu_is_empty(void);
EXTERN void emu_reset(void);
EXTERN void emu_audio_mute(bool mute);
EXTERN void emu_audio_set_master_volume(float volume);
EXTERN void emu_audio_fm_volume(float volume);
EXTERN void emu_audio_pcm_volume(float volume);
EXTERN void emu_audio_cd_volume(float volume);
EXTERN void emu_audio_highres_pcm_volume(float volume);
EXTERN void emu_audio_reset(void);
EXTERN bool emu_is_audio_enabled(void);
EXTERN bool emu_is_audio_open(void);
EXTERN void emu_get_runtime(GT_Runtime_Info& runtime);
EXTERN void emu_get_info(char* info, int buffer_size);
EXTERN GeartownsCore* emu_get_core(void);
EXTERN void emu_set_pad_type(int port, GT_Controller_Type type);
EXTERN GT_Controller_Type emu_get_pad_type(int port);
EXTERN void emu_save_screenshot(const char* file_path);
EXTERN int emu_get_screenshot_png(unsigned char** out_buffer);
EXTERN bool emu_load_bios(const char* path);
EXTERN void emu_mcp_set_transport(int mode, int tcp_port, const char* tcp_address);
EXTERN void emu_mcp_start(void);
EXTERN void emu_mcp_stop(void);
EXTERN bool emu_mcp_is_running(void);
EXTERN int emu_mcp_get_transport_mode(void);
EXTERN void emu_mcp_pump_commands(void);

#undef EMU_IMPORT
#undef EXTERN
#endif /* EMU_H */
