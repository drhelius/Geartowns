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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "libretro.h"
#include "geartowns.h"
#include "cdrom_file.h"
#include "libretro_core_options.h"
#include "libretro_vfs_file.h"

#ifdef _WIN32
static const char slash = '\\';
#else
static const char slash = '/';
#endif

#define RETRO_DEVICE_TOWNS_GAMEPAD    RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define RETRO_DEVICE_TOWNS_6_BUTTON   RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
#define RETRO_DEVICE_TOWNS_MOUSE      RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_MOUSE, 0)

#define MAX_PADS GT_MAX_GAMEPADS
#define MAX_BUTTONS 12
#define BASE_SCREEN_WIDTH 640
#define BASE_SCREEN_HEIGHT 480
#define MAX_SCREEN_WIDTH 1024
#define MAX_SCREEN_HEIGHT 768

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static struct retro_log_callback logging;
retro_log_printf_t log_cb;

static char retro_system_directory[4096];
static char retro_save_directory[4096];
static char retro_game_path[4096];

static s16 audio_buf[GT_AUDIO_BUFFER_SIZE];
static int audio_sample_count = 0;
static u8* frame_buffer;

static int current_screen_width = 0;
static int current_screen_height = 0;
static int current_width_scale = 1;
static float current_aspect_ratio = 0.0f;

static float aspect_ratio = 0.0f;
static bool allow_up_down = false;
static bool libretro_supports_bitmasks = false;
static int joypad_current[MAX_PADS][MAX_BUTTONS];
static int joypad_old[MAX_PADS][MAX_BUTTONS];
struct MouseState
{
    int delta_x;
    int delta_y;
    int button_left;
    int button_right;
    bool delta_applied;
};

static MouseState mouse_current[MAX_PADS];
static unsigned input_device[MAX_PADS] = {
    RETRO_DEVICE_TOWNS_GAMEPAD,
    RETRO_DEVICE_TOWNS_GAMEPAD
};

static GeartownsCore* core;
static GT_Runtime_Info runtime_info;
static const retro_vfs_interface* vfs_interface = NULL;

static void load_bios(void);
static void set_controller_info(void);
static void clear_input_state(void);
static void reset_controller_devices(void);
static void apply_controller_device(unsigned port, unsigned device, bool log_device);
static void release_controller_input(unsigned port);
static void poll_input(void);
static void apply_input(void);
static bool categories_supported = false;
static void check_variables(void);
static bool path_has_extension(const char* path, const char* extension);
static bool path_is_cdrom_uri(const char* path);
static bool path_is_cd_content(const char* path);

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
    (void)level;
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
}

static int IsButtonPressed(int joypad_bits, int button)
{
    return (joypad_bits & (1 << button)) ? 1 : 0;
}

static bool IsJoypadDevice(unsigned device)
{
    return (device == RETRO_DEVICE_JOYPAD) ||
           (device == RETRO_DEVICE_TOWNS_GAMEPAD) ||
           (device == RETRO_DEVICE_TOWNS_6_BUTTON);
}

unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
    audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
    audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
    input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
    input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
    video_cb = cb;
}

void retro_set_environment(retro_environment_t cb)
{
    environ_cb = cb;

    struct retro_vfs_interface_info vfs_interface_info = { };
    vfs_interface_info.required_interface_version = 2;
    vfs_interface_info.iface = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_interface_info) && vfs_interface_info.iface)
    {
        vfs_interface = vfs_interface_info.iface;
        CdRomFile::SetVfsInterface(vfs_interface);
    }
    else
    {
        vfs_interface = NULL;
        CdRomFile::SetVfsInterface(NULL);
    }

    static const struct retro_system_content_info_override content_overrides[] = {
        {
            "d77|rdd",  // extensions
            false,        // need_fullpath
            false         // persistent_data
        },
        { NULL, false, false }
    };

    environ_cb(RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE, (void*)content_overrides);

    set_controller_info();
    libretro_set_core_options(environ_cb, &categories_supported);
}

void retro_init(void)
{
    if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
        log_cb = logging.log;
    else
        log_cb = fallback_log;

    const char *dir = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
        snprintf(retro_system_directory, sizeof(retro_system_directory), "%s", dir);
    else
        snprintf(retro_system_directory, sizeof(retro_system_directory), "%s", ".");

    dir = NULL;
    if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir) && dir)
        snprintf(retro_save_directory, sizeof(retro_save_directory), "%s", dir);
    else
        snprintf(retro_save_directory, sizeof(retro_save_directory), "%s", ".");

    log_cb(RETRO_LOG_INFO, "%s (%s) libretro\n", GT_TITLE, GT_VERSION);

    core = new GeartownsCore();
    core->Init(NULL, GT_PIXEL_RGB565);
    core->GetRuntimeInfo(runtime_info);

    frame_buffer = new u8[MAX_SCREEN_WIDTH * MAX_SCREEN_HEIGHT * sizeof(u16)];

    clear_input_state();

    for (int i = 0; i < MAX_PADS; i++)
        apply_controller_device(i, input_device[i], false);

    libretro_supports_bitmasks = environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL);
}

void retro_deinit(void)
{
    SafeDeleteArray(frame_buffer);
    SafeDelete(core);
    vfs_interface = NULL;
    CdRomFile::SetVfsInterface(NULL);

    audio_sample_count = 0;
    current_screen_width = 0;
    current_screen_height = 0;
    current_width_scale = 1;
    current_aspect_ratio = 0.0f;
    aspect_ratio = 0.0f;
    libretro_supports_bitmasks = false;

    reset_controller_devices();
    clear_input_state();
}

void retro_reset(void)
{
    if (log_cb)
        log_cb(RETRO_LOG_DEBUG, "Resetting...\n");

    check_variables();
    if (core->GetMedia()->IsCDROM())
        load_bios();
    core->ResetMedia(true);

    for (int i = 0; i < MAX_PADS; i++)
        apply_controller_device(i, input_device[i], false);
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
    if (port >= MAX_PADS)
    {
        if (log_cb)
            log_cb(RETRO_LOG_DEBUG, "retro_set_controller_port_device invalid port number: %u\n", port);
        return;
    }

    if ((input_device[port] != device) && core)
        release_controller_input(port);

    input_device[port] = device;

    apply_controller_device(port, device, true);
}

void retro_get_system_info(struct retro_system_info *info)
{
    memset(info, 0, sizeof(*info));
    info->library_name     = "Geartowns";
    info->library_version  = GT_VERSION;
    info->need_fullpath    = true;
    info->block_extract    = true;
    info->valid_extensions = "d77|rdd|cue|chd|iso|bin|zip";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    info->geometry.base_width   = runtime_info.screen_width;
    info->geometry.base_height  = runtime_info.screen_height;
    info->geometry.max_width    = MAX_SCREEN_WIDTH;
    info->geometry.max_height   = MAX_SCREEN_HEIGHT;
    info->geometry.aspect_ratio = aspect_ratio == 0.0f ? (float)runtime_info.screen_width / (float)runtime_info.screen_height / (float)runtime_info.width_scale : aspect_ratio;
    info->timing.fps            = runtime_info.fps;
    info->timing.sample_rate    = GT_AUDIO_SAMPLE_RATE;
}

void retro_run(void)
{
    bool core_options_updated = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &core_options_updated) && core_options_updated)
    {
        check_variables();
    }

    poll_input();
    apply_input();

    audio_sample_count = 0;
    core->RunToFrame(frame_buffer, audio_buf, &audio_sample_count);

    core->GetRuntimeInfo(runtime_info);

    if ((runtime_info.screen_width != current_screen_width) ||
        (runtime_info.screen_height != current_screen_height) ||
        (runtime_info.width_scale != current_width_scale) ||
        (aspect_ratio != current_aspect_ratio))
    {
        current_screen_width = runtime_info.screen_width;
        current_screen_height = runtime_info.screen_height;
        current_width_scale = runtime_info.width_scale;
        current_aspect_ratio = aspect_ratio;

        retro_system_av_info info;
        info.geometry.base_width = runtime_info.screen_width;
        info.geometry.base_height = runtime_info.screen_height;
        info.geometry.max_width = MAX_SCREEN_WIDTH;
        info.geometry.max_height = MAX_SCREEN_HEIGHT;
        info.geometry.aspect_ratio = aspect_ratio == 0.0f ?
            ((float)runtime_info.screen_width / (float)runtime_info.width_scale) / (float)runtime_info.screen_height : aspect_ratio;

        environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &info.geometry);
    }

    video_cb(frame_buffer, runtime_info.screen_width, runtime_info.screen_height, runtime_info.screen_width * sizeof(u16));

    if (audio_sample_count > 0)
        audio_batch_cb(audio_buf, audio_sample_count / 2);
}

bool retro_load_game(const struct retro_game_info *info)
{
    if (!info || !core)
    {
        if (log_cb)
            log_cb(RETRO_LOG_ERROR, "retro_load_game received invalid state.\n");
        return false;
    }

    check_variables();

    const char* load_path = info->path ? info->path : "";
    snprintf(retro_game_path, sizeof(retro_game_path), "%s", load_path);
    log_cb(RETRO_LOG_INFO, "retro_load_game: %s\n", retro_game_path);

    bool is_cd_content = path_is_cd_content(retro_game_path);

    if (path_is_cdrom_uri(retro_game_path))
        log_cb(RETRO_LOG_INFO, "Loading CD-ROM through libretro VFS: %s\n", retro_game_path);

    if (is_cd_content)
        load_bios();

    if (!core->LoadMedia(retro_game_path))
        return false;

    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
    {
        log_cb(RETRO_LOG_ERROR, "RGB565 is not supported.\n");
        retro_game_path[0] = 0;
        return false;
    }

    core->GetRuntimeInfo(runtime_info);

    return true;
}

void retro_unload_game(void)
{
    if (core)
        core->GetMedia()->Reset();
    retro_game_path[0] = 0;
    if (frame_buffer)
        memset(frame_buffer, 0, MAX_SCREEN_WIDTH * MAX_SCREEN_HEIGHT * sizeof(u16));
}

static void load_bios(void)
{
    core->UnloadBios();

    if (!core->LoadBios(retro_system_directory))
    {
        struct retro_message msg = {};
        msg.msg = "FM Towns BIOS not found";
        msg.frames = 360;
        environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
        log_cb(RETRO_LOG_ERROR, "%s\n", msg.msg);
    }
}

static bool path_has_extension(const char* path, const char* extension)
{
    if (!path || !extension)
        return false;

    const char* dot = strrchr(path, '.');
    if (!dot || !dot[1])
        return false;

    dot++;

    while (*dot && *extension)
    {
        if (tolower((unsigned char)*dot) != tolower((unsigned char)*extension))
            return false;

        dot++;
        extension++;
    }

    return (*dot == 0) && (*extension == 0);
}

static bool path_is_cdrom_uri(const char* path)
{
    return path && (strncmp(path, "cdrom://", 8) == 0);
}

static bool path_is_cd_content(const char* path)
{
    return path_is_cdrom_uri(path) || path_has_extension(path, "cue") ||
        path_has_extension(path, "chd") || path_has_extension(path, "iso");
}

unsigned retro_get_region(void)
{
    return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
    (void)game_type;
    (void)info;
    (void)num_info;
    return false;
}

size_t retro_serialize_size(void)
{
    size_t size = 0;
    core->SaveState(NULL, size);
    return size;
}

bool retro_serialize(void *data, size_t size)
{
    return core->SaveState(reinterpret_cast<u8*>(data), size);
}

bool retro_unserialize(const void *data, size_t size)
{
    return core->LoadState(reinterpret_cast<const u8*>(data), size);
}

void *retro_get_memory_data(unsigned id)
{
    switch (id)
    {
        case RETRO_MEMORY_SAVE_RAM:
            return core->GetMemory()->GetBackupRAM();
        case RETRO_MEMORY_SYSTEM_RAM:
            return core->GetMemory()->GetWorkingRAM();
        case RETRO_MEMORY_VIDEO_RAM:
            return core->GetMemory()->GetVideoRAM();
    }

    return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
    switch (id)
    {
        case RETRO_MEMORY_SAVE_RAM:
            return core->GetMemory()->GetBackupRAMSize();
        case RETRO_MEMORY_SYSTEM_RAM:
            return core->GetMemory()->GetWorkingRAMSize();
        case RETRO_MEMORY_VIDEO_RAM:
            return core->GetMemory()->GetVideoRAMSize();
    }

    return 0;
}

void retro_cheat_reset(void)
{
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
    (void)index;
    (void)enabled;
    (void)code;
}

static void set_controller_info(void)
{
    static const struct retro_controller_description port[] = {
        { "Original gamepad", RETRO_DEVICE_TOWNS_GAMEPAD },
        { "6 button gamepad", RETRO_DEVICE_TOWNS_6_BUTTON },
        { "Mouse", RETRO_DEVICE_TOWNS_MOUSE }
    };

    static const struct retro_controller_info ports[] = {
        { port, 3 },
        { port, 3 },
        { NULL, 0 }
    };

    environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

    struct retro_input_descriptor joypad[] = {
        #define button_ids(INDEX) \
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Up" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Run" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "A" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "B" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "C" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,      "X" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "Y" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Z" },
        #define mouse_ids(INDEX) \
        { INDEX, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT,     "Mouse Left" },\
        { INDEX, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT,    "Mouse Right" },
        button_ids(0)
        mouse_ids(0)
        button_ids(1)
        mouse_ids(1)
        { 0, 0, 0, 0, NULL }
    };

    environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, joypad);
}

static void clear_input_state(void)
{
    for (int i = 0; i < MAX_PADS; i++)
    {
        for (int j = 0; j < MAX_BUTTONS; j++)
        {
            joypad_current[i][j] = 0;
            joypad_old[i][j] = 0;
        }

        mouse_current[i].delta_x = 0;
        mouse_current[i].delta_y = 0;
        mouse_current[i].button_left = 0;
        mouse_current[i].button_right = 0;
        mouse_current[i].delta_applied = false;
    }
}

static void reset_controller_devices(void)
{
    for (int i = 0; i < MAX_PADS; i++)
        input_device[i] = RETRO_DEVICE_TOWNS_GAMEPAD;
}

static void apply_controller_device(unsigned port, unsigned device, bool log_device)
{
    if (!core || port >= MAX_PADS)
        return;

    GT_Controller_Type type = GT_CONTROLLER_NONE;

    switch (device)
    {
        case RETRO_DEVICE_JOYPAD:
        case RETRO_DEVICE_TOWNS_GAMEPAD:
            type = GT_CONTROLLER_ORIGINAL_GAMEPAD;
            if (log_device && log_cb)
                log_cb(RETRO_LOG_INFO, "Controller %u: Original gamepad\n", port);
            break;
        case RETRO_DEVICE_TOWNS_6_BUTTON:
            type = GT_CONTROLLER_6_BUTTON_GAMEPAD;
            if (log_device && log_cb)
                log_cb(RETRO_LOG_INFO, "Controller %u: 6 button gamepad\n", port);
            break;
        case RETRO_DEVICE_TOWNS_MOUSE:
            type = GT_CONTROLLER_MOUSE;
            if (log_device && log_cb)
                log_cb(RETRO_LOG_INFO, "Controller %u: Mouse\n", port);
            break;
        case RETRO_DEVICE_NONE:
            if (log_device && log_cb)
                log_cb(RETRO_LOG_INFO, "Controller %u: Unplugged\n", port);
            break;
        default:
            if (log_device && log_cb)
                log_cb(RETRO_LOG_DEBUG, "Controller %u: Unsupported device\n", port);
            break;
    }

    core->GetInput()->SetControllerType((int)port, type);
}

static void release_controller_input(unsigned port)
{
    if (core)
    {
        GT_GamePad_State state = { 0, 0, 0 };
        core->GetInput()->SetGamePadState((int)port, state);
    }

    for (int i = 0; i < MAX_BUTTONS; i++)
    {
        joypad_current[port][i] = 0;
        joypad_old[port][i] = 0;
    }

    mouse_current[port].delta_x = 0;
    mouse_current[port].delta_y = 0;
    mouse_current[port].button_left = 0;
    mouse_current[port].button_right = 0;
    mouse_current[port].delta_applied = false;

    if ((input_device[port] == RETRO_DEVICE_TOWNS_MOUSE) && core)
    {
        core->GetInput()->SetMouseDelta(0, 0);
        core->GetInput()->SetMouseButtons(false, false);
    }
}

static void poll_input(void)
{
    int joypad_bits[MAX_PADS];

    if (!input_poll_cb || !input_state_cb || !core)
        return;

    input_poll_cb();

    if (libretro_supports_bitmasks)
    {
        for (int j = 0; j < MAX_PADS; j++)
        {
            if (IsJoypadDevice(input_device[j]))
                joypad_bits[j] = input_state_cb(j, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
            else
                joypad_bits[j] = 0;
        }
    }
    else
    {
        for (int j = 0; j < MAX_PADS; j++)
        {
            joypad_bits[j] = 0;
            if (IsJoypadDevice(input_device[j]))
            {
                for (int i = 0; i < (RETRO_DEVICE_ID_JOYPAD_R3 + 1); i++)
                    joypad_bits[j] |= input_state_cb(j, RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
            }
        }
    }

    for (int j = 0; j < MAX_PADS; j++)
    {
        mouse_current[j].delta_x = 0;
        mouse_current[j].delta_y = 0;
        mouse_current[j].button_left = 0;
        mouse_current[j].button_right = 0;
        mouse_current[j].delta_applied = false;

        if (input_device[j] == RETRO_DEVICE_TOWNS_MOUSE)
        {
            mouse_current[j].delta_x = input_state_cb(j, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
            mouse_current[j].delta_y = input_state_cb(j, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
            mouse_current[j].button_left = input_state_cb(j, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) ? 1 : 0;
            mouse_current[j].button_right = input_state_cb(j, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT) ? 1 : 0;
        }
    }

    for (int j = 0; j < MAX_PADS; j++)
    {
        for (int i = 0; i < MAX_BUTTONS; i++)
            joypad_old[j][i] = joypad_current[j][i];
    }

    for (int j = 0; j < MAX_PADS; j++)
    {
        int up_pressed = IsButtonPressed(joypad_bits[j], RETRO_DEVICE_ID_JOYPAD_UP);
        int down_pressed = IsButtonPressed(joypad_bits[j], RETRO_DEVICE_ID_JOYPAD_DOWN);
        int left_pressed = IsButtonPressed(joypad_bits[j], RETRO_DEVICE_ID_JOYPAD_LEFT);
        int right_pressed = IsButtonPressed(joypad_bits[j], RETRO_DEVICE_ID_JOYPAD_RIGHT);

        if (allow_up_down)
        {
            joypad_current[j][0] = up_pressed;
            joypad_current[j][1] = down_pressed;
            joypad_current[j][2] = left_pressed;
            joypad_current[j][3] = right_pressed;
        }
        else
        {
            int up = up_pressed;
            int down = down_pressed;
            int left = left_pressed;
            int right = right_pressed;

            if (up_pressed && down_pressed)
            {
                if (joypad_old[j][0])
                {
                    up = 1;
                    down = 0;
                }
                else if (joypad_old[j][1])
                {
                    up = 0;
                    down = 1;
                }
                else
                {
                    up = 1;
                    down = 0;
                }
            }

            if (left_pressed && right_pressed)
            {
                if (joypad_old[j][2])
                {
                    left = 1;
                    right = 0;
                }
                else if (joypad_old[j][3])
                {
                    left = 0;
                    right = 1;
                }
                else
                {
                    left = 1;
                    right = 0;
                }
            }

            joypad_current[j][0] = up;
            joypad_current[j][1] = down;
            joypad_current[j][2] = left;
            joypad_current[j][3] = right;
        }

        joypad_current[j][4] = IsButtonPressed(joypad_bits[j], RETRO_DEVICE_ID_JOYPAD_START);
        joypad_current[j][5] = IsButtonPressed(joypad_bits[j], RETRO_DEVICE_ID_JOYPAD_SELECT);
        joypad_current[j][6] = IsButtonPressed(joypad_bits[j], RETRO_DEVICE_ID_JOYPAD_A);
        joypad_current[j][7] = IsButtonPressed(joypad_bits[j], RETRO_DEVICE_ID_JOYPAD_B);
        joypad_current[j][8] = IsButtonPressed(joypad_bits[j], RETRO_DEVICE_ID_JOYPAD_Y);
        joypad_current[j][9] = IsButtonPressed(joypad_bits[j], RETRO_DEVICE_ID_JOYPAD_X);
        joypad_current[j][10] = IsButtonPressed(joypad_bits[j], RETRO_DEVICE_ID_JOYPAD_L);
        joypad_current[j][11] = IsButtonPressed(joypad_bits[j], RETRO_DEVICE_ID_JOYPAD_R);
    }
}

static void apply_input(void)
{
    int mouse_port = -1;

    for (int j = 0; j < MAX_PADS; j++)
    {
        if (input_device[j] == RETRO_DEVICE_TOWNS_MOUSE)
        {
            mouse_port = j;
            break;
        }
    }

    for (int j = 0; j < MAX_PADS; j++)
    {
        if (j == mouse_port)
        {
            if (!mouse_current[j].delta_applied)
            {
                core->GetInput()->SetMouseDelta(mouse_current[j].delta_x, mouse_current[j].delta_y);
                core->GetInput()->SetMouseButtons(mouse_current[j].button_left, mouse_current[j].button_right);
                mouse_current[j].delta_applied = true;
            }

            GT_GamePad_State state = { 0, 0, 0 };
            core->GetInput()->SetGamePadState(j, state);
            continue;
        }

        u16 buttons = 0;
        if (joypad_current[j][0]) buttons |= GT_GAMEPAD_UP;
        if (joypad_current[j][1]) buttons |= GT_GAMEPAD_DOWN;
        if (joypad_current[j][2]) buttons |= GT_GAMEPAD_LEFT;
        if (joypad_current[j][3]) buttons |= GT_GAMEPAD_RIGHT;
        if (joypad_current[j][4]) buttons |= GT_GAMEPAD_START;
        if (joypad_current[j][5]) buttons |= GT_GAMEPAD_RUN;
        if (joypad_current[j][6]) buttons |= GT_GAMEPAD_A;
        if (joypad_current[j][7]) buttons |= GT_GAMEPAD_B;

        if (input_device[j] == RETRO_DEVICE_TOWNS_6_BUTTON)
        {
            if (joypad_current[j][8]) buttons |= GT_GAMEPAD_C;
            if (joypad_current[j][9]) buttons |= GT_GAMEPAD_X;
            if (joypad_current[j][10]) buttons |= GT_GAMEPAD_Y;
            if (joypad_current[j][11]) buttons |= GT_GAMEPAD_Z;
        }

        GT_GamePad_State state = { buttons, 0, 0 };
        core->GetInput()->SetGamePadState(j, state);
    }
}

static void check_variables(void)
{
    if (!environ_cb)
        return;

    struct retro_variable var = { };

    var.key = "geartowns_aspect_ratio";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        if (strcmp(var.value, "4:3") == 0)
            aspect_ratio = 4.0f / 3.0f;
        else if (strcmp(var.value, "16:9") == 0)
            aspect_ratio = 16.0f / 9.0f;
        else
            aspect_ratio = 0.0f;
    }

    var.key = "geartowns_up_down_allowed";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        allow_up_down = (strcmp(var.value, "Enabled") == 0);
    }
}