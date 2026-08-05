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
#include "libretro.h"
#include "geartowns.h"
#include "libretro_core_options.h"
#include "libretro_vfs_file.h"

#define RETRO_DEVICE_TOWNS_GAMEPAD    RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define RETRO_DEVICE_TOWNS_6_BUTTON   RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)

#define MAX_PADS GT_MAX_GAMEPADS
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
static char retro_vfs_temp_path[4096];

static s16 audio_buf[GT_AUDIO_BUFFER_SIZE];
static int audio_sample_count = 0;
static u16* frame_buffer;

static float aspect_ratio = 0.0f;
static bool allow_up_down = false;
static bool libretro_supports_bitmasks = false;
static u16 joypad_current[MAX_PADS];
static u16 joypad_old[MAX_PADS];
static unsigned input_device[MAX_PADS] = {
    RETRO_DEVICE_TOWNS_GAMEPAD,
    RETRO_DEVICE_TOWNS_GAMEPAD
};

static GeartownsCore* core;
static const retro_vfs_interface* vfs_interface = NULL;

static void set_controller_info(void);
static void clear_input_state(void);
static void reset_controller_devices(void);
static void apply_controller_device(unsigned port, unsigned device, bool log_device);
static void release_controller_input(unsigned port);
static void poll_input(void);
static bool categories_supported = false;
static void check_variables(void);
static const char* get_path_extension(const char* path, size_t* length);
static bool path_has_extension(const char* path, const char* extension);
static bool path_requires_vfs_staging(const char* path);
static bool load_media(const char* path);
static void remove_vfs_temp_file(void);

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
    (void)level;
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
}

static bool IsButtonPressed(u16 joypad_bits, unsigned button)
{
    return (joypad_bits & (1U << button)) != 0;
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
    }
    else
    {
        vfs_interface = NULL;
    }

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

    retro_vfs_temp_path[0] = 0;

    log_cb(RETRO_LOG_INFO, "%s (%s) libretro\n", GT_TITLE, GT_VERSION);

    core = new GeartownsCore();
    core->Init(NULL, GT_PIXEL_RGB565);

    frame_buffer = new u16[MAX_SCREEN_WIDTH * MAX_SCREEN_HEIGHT];
    memset(frame_buffer, 0, MAX_SCREEN_WIDTH * MAX_SCREEN_HEIGHT * sizeof(u16));

    clear_input_state();

    for (int i = 0; i < MAX_PADS; i++)
        apply_controller_device(i, input_device[i], false);

    libretro_supports_bitmasks = environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL);
}

void retro_deinit(void)
{
    remove_vfs_temp_file();
    SafeDeleteArray(frame_buffer);
    SafeDelete(core);
    vfs_interface = NULL;

    audio_sample_count = 0;
    aspect_ratio = 0.0f;
    libretro_supports_bitmasks = false;

    reset_controller_devices();
    clear_input_state();
}

void retro_reset(void)
{
    if (!core)
        return;

    if (log_cb)
        log_cb(RETRO_LOG_DEBUG, "Resetting...\n");

    check_variables();
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
    info->geometry.base_width   = BASE_SCREEN_WIDTH;
    info->geometry.base_height  = BASE_SCREEN_HEIGHT;
    info->geometry.max_width    = MAX_SCREEN_WIDTH;
    info->geometry.max_height   = MAX_SCREEN_HEIGHT;
    info->geometry.aspect_ratio = aspect_ratio == 0.0f ? 4.0f / 3.0f : aspect_ratio;
    info->timing.fps            = 60.0;
    info->timing.sample_rate    = GT_AUDIO_SAMPLE_RATE;
}

void retro_run(void)
{
    bool core_options_updated = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &core_options_updated) && core_options_updated)
    {
        float previous_aspect_ratio = aspect_ratio;
        check_variables();

        if (aspect_ratio != previous_aspect_ratio)
        {
            struct retro_game_geometry geometry;
            geometry.base_width = BASE_SCREEN_WIDTH;
            geometry.base_height = BASE_SCREEN_HEIGHT;
            geometry.max_width = MAX_SCREEN_WIDTH;
            geometry.max_height = MAX_SCREEN_HEIGHT;
            geometry.aspect_ratio = aspect_ratio == 0.0f ? 4.0f / 3.0f : aspect_ratio;
            environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &geometry);
        }
    }

    poll_input();

    audio_sample_count = 0;
    if (core)
        core->RunToFrame((u8*)frame_buffer, audio_buf, &audio_sample_count);

    video_cb(frame_buffer, BASE_SCREEN_WIDTH, BASE_SCREEN_HEIGHT, BASE_SCREEN_WIDTH * sizeof(u16));

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

    core->LoadBios(retro_system_directory);
    if (!load_media(retro_game_path))
        return false;

    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
    {
        log_cb(RETRO_LOG_ERROR, "RGB565 is not supported.\n");
        core->GetMedia()->Reset();
        retro_game_path[0] = 0;
        remove_vfs_temp_file();
        return false;
    }

    return true;
}

void retro_unload_game(void)
{
    if (core)
        core->GetMedia()->Reset();
    retro_game_path[0] = 0;
    remove_vfs_temp_file();
    if (frame_buffer)
        memset(frame_buffer, 0, MAX_SCREEN_WIDTH * MAX_SCREEN_HEIGHT * sizeof(u16));
}

static bool load_media(const char* path)
{
    if (!path || !path[0])
        return false;

    if (!vfs_interface || !path_requires_vfs_staging(path))
        return core->LoadMedia(path);

    if (path_has_extension(path, "cue"))
    {
        log_cb(RETRO_LOG_ERROR, "VFS staging does not support CUE companion files: %s\n", path);
        return false;
    }

    LibretroVfsFile input(vfs_interface);
    if (!input.Open(path, RETRO_VFS_FILE_ACCESS_READ))
    {
        log_cb(RETRO_LOG_ERROR, "Failed to open VFS media: %s\n", path);
        return false;
    }

    s64 size = input.GetSize();
    if (size <= 0)
    {
        input.Close();
        log_cb(RETRO_LOG_ERROR, "Invalid VFS media size: %s\n", path);
        return false;
    }

    char extension[32] = ".bin";
    size_t extension_length = 0;
    const char* source_extension = get_path_extension(path, &extension_length);
    if (source_extension && extension_length < sizeof(extension))
    {
        memcpy(extension, source_extension, extension_length);
        extension[extension_length] = 0;
    }

    remove_vfs_temp_file();
    int path_length = snprintf(retro_vfs_temp_path, sizeof(retro_vfs_temp_path),
        "%s/geartowns-vfs-%p%s", retro_save_directory, (void*)core, extension);
    if (path_length < 0 || path_length >= (int)sizeof(retro_vfs_temp_path))
    {
        retro_vfs_temp_path[0] = 0;
        input.Close();
        log_cb(RETRO_LOG_ERROR, "VFS staging path is too long: %s\n", path);
        return false;
    }

    FILE* output = fopen(retro_vfs_temp_path, "wb");
    if (!output)
    {
        input.Close();
        log_cb(RETRO_LOG_ERROR, "Failed to create VFS staging file: %s\n", retro_vfs_temp_path);
        remove_vfs_temp_file();
        return false;
    }

    u8 buffer[64 * 1024];
    s64 remaining = size;
    bool copied = true;

    while (remaining > 0)
    {
        u64 chunk_size = remaining > (s64)sizeof(buffer) ? sizeof(buffer) : (u64)remaining;
        s64 bytes_read = input.Read(buffer, chunk_size);
        if (bytes_read <= 0 || (u64)bytes_read > chunk_size)
        {
            copied = false;
            break;
        }

        if (fwrite(buffer, 1, (size_t)bytes_read, output) != (size_t)bytes_read)
        {
            copied = false;
            break;
        }

        remaining -= bytes_read;
    }

    bool input_closed = input.Close();
    bool output_flushed = fflush(output) == 0;
    bool output_clean = ferror(output) == 0;
    bool output_closed = fclose(output) == 0;

    if (!copied || !input_closed || !output_flushed || !output_clean || !output_closed)
    {
        remove_vfs_temp_file();
        log_cb(RETRO_LOG_ERROR, "Failed to stage VFS media: %s\n", path);
        return false;
    }

    bool loaded = core->LoadMedia(retro_vfs_temp_path);
    if (!loaded)
        remove_vfs_temp_file();
    return loaded;
}

static const char* get_path_extension(const char* path, size_t* length)
{
    *length = 0;
    if (!path)
        return NULL;

    const char* end = path + strlen(path);
    const char* suffix = strpbrk(path, "?#");
    if (suffix)
        end = suffix;

    const char* position = end;
    while (position > path)
    {
        char character = position[-1];
        if (character == '.')
        {
            *length = (size_t)(end - (position - 1));
            return position - 1;
        }
        if (character == '/' || character == '\\')
            break;

        position--;
    }

    return NULL;
}

static bool path_has_extension(const char* path, const char* extension)
{
    if (!path || !extension)
        return false;

    size_t path_extension_length = 0;
    const char* path_extension = get_path_extension(path, &path_extension_length);
    size_t extension_length = strlen(extension);
    if (!path_extension || path_extension_length != extension_length + 1)
        return false;

    for (size_t i = 0; i < extension_length; i++)
    {
        if (tolower((unsigned char)path_extension[i + 1]) != tolower((unsigned char)extension[i]))
            return false;
    }

    return true;
}

static bool path_requires_vfs_staging(const char* path)
{
    return path && strstr(path, "://");
}

static void remove_vfs_temp_file(void)
{
    if (retro_vfs_temp_path[0])
    {
        remove(retro_vfs_temp_path);
        retro_vfs_temp_path[0] = 0;
    }
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
    return 0;
}

bool retro_serialize(void *data, size_t size)
{
    (void)data;
    (void)size;
    return false;
}

bool retro_unserialize(const void *data, size_t size)
{
    (void)data;
    (void)size;
    return false;
}

void *retro_get_memory_data(unsigned id)
{
    (void)id;
    return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
    (void)id;
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
        { "6 button gamepad", RETRO_DEVICE_TOWNS_6_BUTTON }
    };

    static const struct retro_controller_info ports[] = {
        { port, 2 },
        { port, 2 },
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
        button_ids(0)
        button_ids(1)
        { 0, 0, 0, 0, NULL }
    };

    environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, joypad);
}

static void clear_input_state(void)
{
    for (int i = 0; i < MAX_PADS; i++)
    {
        joypad_current[i] = 0;
        joypad_old[i] = 0;
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

    joypad_current[port] = 0;
    joypad_old[port] = 0;
}

static void poll_input(void)
{
    if (!input_poll_cb || !input_state_cb || !core)
        return;

    input_poll_cb();

    for (int port = 0; port < MAX_PADS; port++)
    {
        joypad_old[port] = joypad_current[port];
        joypad_current[port] = 0;

        if (!IsJoypadDevice(input_device[port]))
        {
            GT_GamePad_State state = { 0, 0, 0 };
            core->GetInput()->SetGamePadState(port, state);
            continue;
        }

        u16 bits = 0;
        if (libretro_supports_bitmasks)
        {
            bits = (u16)input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
        }
        else
        {
            for (unsigned button = 0; button <= RETRO_DEVICE_ID_JOYPAD_R3; button++)
            {
                if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, button))
                    bits |= (u16)(1U << button);
            }
        }

        bool up = IsButtonPressed(bits, RETRO_DEVICE_ID_JOYPAD_UP);
        bool down = IsButtonPressed(bits, RETRO_DEVICE_ID_JOYPAD_DOWN);
        bool left = IsButtonPressed(bits, RETRO_DEVICE_ID_JOYPAD_LEFT);
        bool right = IsButtonPressed(bits, RETRO_DEVICE_ID_JOYPAD_RIGHT);

        if (!allow_up_down)
        {
            if (up && down)
            {
                down = (joypad_old[port] & GT_GAMEPAD_DOWN) != 0;
                up = !down;
            }

            if (left && right)
            {
                right = (joypad_old[port] & GT_GAMEPAD_RIGHT) != 0;
                left = !right;
            }
        }

        u16 buttons = 0;
        if (up) buttons |= GT_GAMEPAD_UP;
        if (down) buttons |= GT_GAMEPAD_DOWN;
        if (left) buttons |= GT_GAMEPAD_LEFT;
        if (right) buttons |= GT_GAMEPAD_RIGHT;
        if (IsButtonPressed(bits, RETRO_DEVICE_ID_JOYPAD_START)) buttons |= GT_GAMEPAD_START;
        if (IsButtonPressed(bits, RETRO_DEVICE_ID_JOYPAD_SELECT)) buttons |= GT_GAMEPAD_RUN;
        if (IsButtonPressed(bits, RETRO_DEVICE_ID_JOYPAD_A)) buttons |= GT_GAMEPAD_A;
        if (IsButtonPressed(bits, RETRO_DEVICE_ID_JOYPAD_B)) buttons |= GT_GAMEPAD_B;

        if (input_device[port] == RETRO_DEVICE_TOWNS_6_BUTTON)
        {
            if (IsButtonPressed(bits, RETRO_DEVICE_ID_JOYPAD_Y)) buttons |= GT_GAMEPAD_C;
            if (IsButtonPressed(bits, RETRO_DEVICE_ID_JOYPAD_X)) buttons |= GT_GAMEPAD_X;
            if (IsButtonPressed(bits, RETRO_DEVICE_ID_JOYPAD_L)) buttons |= GT_GAMEPAD_Y;
            if (IsButtonPressed(bits, RETRO_DEVICE_ID_JOYPAD_R)) buttons |= GT_GAMEPAD_Z;
        }

        joypad_current[port] = buttons;

        GT_GamePad_State state = { buttons, 0, 0 };
        core->GetInput()->SetGamePadState(port, state);
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