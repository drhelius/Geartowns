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
#include <string>
#include <vector>
#include "libretro.h"
#include "geartowns.h"
#include "media/media_file.h"
#include "media/crc.h"
#include "drive/floppy_disk.h"
#include "drive/floppy_image.h"
#include "libretro_core_options.h"
#include "libretro_vfs_file.h"

#ifdef _WIN32
static const char slash = '\\';
#else
static const char slash = '/';
#endif

#define RETRO_DEVICE_TOWNS_GAMEPAD    RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define RETRO_DEVICE_TOWNS_6_BUTTON   RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
#define RETRO_DEVICE_TOWNS_MARTY      RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 2)
#define RETRO_DEVICE_TOWNS_MOUSE      RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_MOUSE, 0)

#define MAX_PADS GT_MAX_GAMEPADS
#define MAX_BUTTONS 12
#define BASE_SCREEN_WIDTH 640
#define BASE_SCREEN_HEIGHT 480
#define MAX_SCREEN_WIDTH 1024
#define MAX_SCREEN_HEIGHT 768

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
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
static float current_fps = 60.0f;

static float aspect_ratio = 0.0f;
static bool allow_up_down = false;
static int mouse_sensitivity = 5;
static int cdrom_speed = 1;
static bool libretro_supports_bitmasks = false;
static int joypad_current[MAX_PADS][MAX_BUTTONS];
static int joypad_old[MAX_PADS][MAX_BUTTONS];
struct MouseState
{
    int delta_x;
    int delta_y;
    float remainder_x;
    float remainder_y;
    int button_left;
    int button_right;
    bool delta_applied;
};

static MouseState mouse_current[MAX_PADS];
static bool retro_key_down[RETROK_LAST];
static int key_references[GT_KEY_COUNT];
static unsigned input_device[MAX_PADS] = {
    RETRO_DEVICE_TOWNS_GAMEPAD,
    RETRO_DEVICE_TOWNS_GAMEPAD
};

struct DiskSetImage
{
    std::string path;
    std::string label;
    int disk;
    bool cdrom;
};

static GeartownsCore* core;
static std::vector<DiskSetImage> disk_images;
static unsigned disk_index = 0;
static bool disk_ejected = true;
static int disk_ejected_entry = -1;
static unsigned disk_initial_index = 0;
static std::string disk_initial_path;
static std::string floppy_working_path;
static u32 floppy_base_crc = 0;
static GT_Runtime_Info runtime_info;
static const retro_vfs_interface* vfs_interface = NULL;

static void load_bios(void);
static void set_controller_info(void);
static void clear_input_state(void);
static void clear_keyboard_state(void);
static GT_Keys key_from_retro_key(unsigned keycode);
static void keyboard_event(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers);
static void reset_controller_devices(void);
static void apply_controller_device(unsigned port, unsigned device, bool log_device);
static void release_controller_input(unsigned port);
static void poll_input(void);
static void apply_input(void);
static bool categories_supported = false;
static void check_variables(void);
static bool path_is_cdrom_uri(const char* path);
static bool path_is_cd_content(const char* path);
static bool path_is_floppy_content(const char* path);
static bool read_content_file(const char* path, std::vector<u8>& data);
static bool write_content_file(const char* path, const u8* data, u32 size);
static bool disk_add_images(const char* path, std::vector<DiskSetImage>& images);
static bool disk_add_floppy_images(const char* path, std::vector<DiskSetImage>& images);
static bool disk_insert(unsigned index);
static bool disk_eject(unsigned index);
static void disk_clear(void);
static bool floppy_load_image(unsigned index);
static bool floppy_flush(void);
static void register_disk_control(void);
static bool disk_set_eject_state(bool ejected);
static bool disk_get_eject_state(void);
static unsigned disk_get_image_index(void);
static bool disk_set_image_index(unsigned index);
static unsigned disk_get_num_images(void);
static bool disk_replace_image_index(unsigned index, const struct retro_game_info* info);
static bool disk_add_image_index(void);
static bool disk_set_initial_image(unsigned index, const char* path);
static bool disk_get_image_path(unsigned index, char* path, size_t len);
static bool disk_get_image_label(unsigned index, char* label, size_t len);

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
        (device == RETRO_DEVICE_TOWNS_MARTY) ||
        (device == RETRO_DEVICE_TOWNS_6_BUTTON);
}

unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
    (void)cb;
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

    register_disk_control();
    set_controller_info();
    libretro_set_core_options(environ_cb, &categories_supported);
}

void retro_init(void)
{
    struct retro_vfs_interface_info vfs_interface_info = { };
    vfs_interface_info.required_interface_version = 2;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_interface_info) && vfs_interface_info.iface)
        vfs_interface = vfs_interface_info.iface;
    else
        vfs_interface = NULL;

    MediaFile::SetVfsInterface(vfs_interface);

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
    core->Init();
    core->GetRuntimeInfo(runtime_info);

    frame_buffer = new u8[MAX_SCREEN_WIDTH * MAX_SCREEN_HEIGHT * sizeof(u32)];

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
    MediaFile::SetVfsInterface(NULL);

    audio_sample_count = 0;
    current_screen_width = 0;
    current_screen_height = 0;
    current_width_scale = 1;
    current_aspect_ratio = 0.0f;
    aspect_ratio = 0.0f;
    current_fps = 60.0f;
    libretro_supports_bitmasks = false;

    reset_controller_devices();
    clear_input_state();
}

void retro_reset(void)
{
    if (log_cb)
        log_cb(RETRO_LOG_DEBUG, "Resetting...\n");

    check_variables();
    load_bios();
    core->ResetMedia();
    clear_keyboard_state();

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
    info->library_name     = GT_TITLE;
    info->library_version  = GT_VERSION;
    info->need_fullpath    = true;
    info->block_extract    = true;
    info->valid_extensions = "cue|chd|iso|bin|zip|d77|d88|hdm|xdf|img|m3u";
}

static void get_system_av_info(struct retro_system_av_info* info)
{
    runtime_info.frame_time = 0.0f;
    if (core)
        core->GetRuntimeInfo(runtime_info);

    info->geometry.base_width   = runtime_info.screen_width;
    info->geometry.base_height  = runtime_info.screen_height;
    info->geometry.max_width    = MAX_SCREEN_WIDTH;
    info->geometry.max_height   = MAX_SCREEN_HEIGHT;
    info->geometry.aspect_ratio = aspect_ratio == 0.0f ?
        (float)runtime_info.screen_width / (float)runtime_info.screen_height / (float)runtime_info.width_scale : aspect_ratio;
    info->timing.fps            = runtime_info.frame_time > 0.0f ? 1000.0f / runtime_info.frame_time : 60.0f;
    info->timing.sample_rate    = GT_AUDIO_SAMPLE_RATE;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    get_system_av_info(info);
    if (runtime_info.frame_time > 0.0f)
        current_fps = (float)info->timing.fps;
    info->timing.fps = current_fps;
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

    retro_system_av_info info;
    get_system_av_info(&info);
    bool fps_changed = fabsf((float)info.timing.fps - current_fps) > 0.1f;
    bool geometry_changed = (runtime_info.screen_width != current_screen_width) ||
                            (runtime_info.screen_height != current_screen_height) ||
                            (runtime_info.width_scale != current_width_scale) ||
                            (aspect_ratio != current_aspect_ratio);

    if (fps_changed || geometry_changed)
    {
        current_screen_width = runtime_info.screen_width;
        current_screen_height = runtime_info.screen_height;
        current_width_scale = runtime_info.width_scale;
        current_aspect_ratio = aspect_ratio;
        current_fps = (float)info.timing.fps;

        if (fps_changed)
        {
            log_cb(RETRO_LOG_INFO, "Refresh rate changed to %.2f Hz\n", current_fps);
            environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &info);
        }
        else
        {
            environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &info.geometry);
        }
    }

    video_cb(frame_buffer, runtime_info.screen_width, runtime_info.screen_height, runtime_info.screen_width * sizeof(u32));

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

    if (is_cd_content && log_cb)
        log_cb(RETRO_LOG_INFO, "Loading CD content.\n");

    load_bios();

    disk_clear();

    if (!disk_add_images(retro_game_path, disk_images))
        return false;

    bool initial = disk_initial_index < disk_images.size() && disk_images[disk_initial_index].path == disk_initial_path;
    disk_index = initial ? disk_initial_index : 0;


    for (size_t i = 0; i < disk_images.size(); i++)
    {
        if (disk_images[i].cdrom != disk_images[disk_index].cdrom)
        {
            if (!disk_insert((unsigned)i))
                return false;

            break;
        }
    }

    if (!disk_insert(disk_index))
        return false;

    disk_ejected = false;

    core->PowerOn();

    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;

    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
    {
        log_cb(RETRO_LOG_ERROR, "XRGB8888 is not supported.\n");
        retro_game_path[0] = 0;
        return false;
    }

    core->GetRuntimeInfo(runtime_info);
    clear_keyboard_state();

    struct retro_keyboard_callback keyboard = { keyboard_event };
    environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &keyboard);

    return true;
}

void retro_unload_game(void)
{
    if (core)
    {
        if (!floppy_flush())
            log_cb(RETRO_LOG_ERROR, "Unable to save the floppy changes\n");

        core->EjectFloppy(0);
        core->EjectFloppy(1);
        core->EjectMedia();
    }

    disk_clear();

    retro_game_path[0] = 0;
    current_fps = 60.0f;

    if (frame_buffer)
        memset(frame_buffer, 0, MAX_SCREEN_WIDTH * MAX_SCREEN_HEIGHT * sizeof(u32));
}

static void load_bios(void)
{
    if (!core->LoadBios(retro_system_directory))
    {
        struct retro_message msg = {};
        msg.msg = "FM Towns firmware not found";
        msg.frames = 360;
        environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
        log_cb(RETRO_LOG_ERROR, "%s\n", msg.msg);
    }
}

static bool path_is_cdrom_uri(const char* path)
{
    return path && (strncmp(path, "cdrom://", 8) == 0);
}

static bool path_is_cd_content(const char* path)
{
    return path_is_cdrom_uri(path) ||
        ends_with_no_case(path, ".cue") ||
        ends_with_no_case(path, ".chd") ||
        ends_with_no_case(path, ".iso");
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
    if (!core->LoadState(reinterpret_cast<const u8*>(data), size))
        return false;

    FloppyDisk* disk = core->GetFloppy(0);

    if (!disk->IsInserted() || disk->GetBaseCRC() != floppy_base_crc)
        floppy_working_path.clear();

    if (disk_index < disk_images.size() && !disk_images[disk_index].cdrom)
        disk_ejected = !disk->IsInserted();

    return true;
}

void *retro_get_memory_data(unsigned id)
{
    switch (id)
    {
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
        { "Marty gamepad", RETRO_DEVICE_TOWNS_MARTY },
        { "6 button gamepad", RETRO_DEVICE_TOWNS_6_BUTTON },
        { "Mouse", RETRO_DEVICE_TOWNS_MOUSE }
    };

    static const struct retro_controller_info ports[] = {
        { port, 4 },
        { port, 4 },
        { NULL, 0 }
    };

    environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

    struct retro_input_descriptor joypad[] = {
        #define button_ids(INDEX) \
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Up" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Run" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "A" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "B" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "C" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,      "X" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "Y" },\
        { INDEX, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Z / Zoom" },
        #define mouse_ids(INDEX) \
        { INDEX, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT,     "Mouse Left (A)" },\
        { INDEX, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT,    "Mouse Right (B)" },
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
    clear_keyboard_state();

    for (int i = 0; i < MAX_PADS; i++)
    {
        for (int j = 0; j < MAX_BUTTONS; j++)
        {
            joypad_current[i][j] = 0;
            joypad_old[i][j] = 0;
        }

        mouse_current[i].delta_x = 0;
        mouse_current[i].delta_y = 0;
        mouse_current[i].remainder_x = 0.0f;
        mouse_current[i].remainder_y = 0.0f;
        mouse_current[i].button_left = 0;
        mouse_current[i].button_right = 0;
        mouse_current[i].delta_applied = false;
    }
}

static void clear_keyboard_state(void)
{
    memset(retro_key_down, 0, sizeof(retro_key_down));
    memset(key_references, 0, sizeof(key_references));

    if (core)
        core->ReleaseAllKeys();
}

static GT_Keys key_from_retro_key(unsigned keycode)
{
    if (keycode >= RETROK_a && keycode <= RETROK_z)
    {
        static const GT_Keys letters[26] =
        {
            GT_KEY_A, GT_KEY_B, GT_KEY_C, GT_KEY_D, GT_KEY_E, GT_KEY_F, GT_KEY_G, GT_KEY_H, GT_KEY_I,
            GT_KEY_J, GT_KEY_K, GT_KEY_L, GT_KEY_M, GT_KEY_N, GT_KEY_O, GT_KEY_P, GT_KEY_Q, GT_KEY_R,
            GT_KEY_S, GT_KEY_T, GT_KEY_U, GT_KEY_V, GT_KEY_W, GT_KEY_X, GT_KEY_Y, GT_KEY_Z
        };

        return letters[keycode - RETROK_a];
    }

    if (keycode >= RETROK_1 && keycode <= RETROK_9)
        return (GT_Keys)(GT_KEY_1 + (keycode - RETROK_1));

    switch (keycode)
    {
        case RETROK_0: return GT_KEY_0;
        case RETROK_RETURN: return GT_KEY_RETURN;
        case RETROK_ESCAPE: return GT_KEY_BREAK;
        case RETROK_BACKSPACE: return GT_KEY_BACKSPACE;
        case RETROK_TAB: return GT_KEY_TAB;
        case RETROK_SPACE: return GT_KEY_SPACE;
        case RETROK_MINUS: return GT_KEY_MINUS;
        case RETROK_EQUALS: return GT_KEY_CARET;
        case RETROK_CARET: return GT_KEY_CARET;
        case RETROK_LEFTBRACKET: return GT_KEY_AT;
        case RETROK_AT: return GT_KEY_AT;
        case RETROK_RIGHTBRACKET: return GT_KEY_LEFT_BRACKET;
        case RETROK_BACKSLASH: return GT_KEY_YEN;
        case RETROK_SEMICOLON: return GT_KEY_SEMICOLON;
        case RETROK_QUOTE: return GT_KEY_COLON;
        case RETROK_COLON: return GT_KEY_COLON;
        case RETROK_BACKQUOTE: return GT_KEY_ESCAPE;
        case RETROK_COMMA: return GT_KEY_COMMA;
        case RETROK_PERIOD: return GT_KEY_PERIOD;
        case RETROK_SLASH: return GT_KEY_SLASH;
        case RETROK_UNDERSCORE: return GT_KEY_UNDERSCORE;
        case RETROK_OEM_102: return GT_KEY_UNDERSCORE;
        case RETROK_CAPSLOCK: return GT_KEY_CAPS;
        case RETROK_F1: return GT_KEY_PF1;
        case RETROK_F2: return GT_KEY_PF2;
        case RETROK_F3: return GT_KEY_PF3;
        case RETROK_F4: return GT_KEY_PF4;
        case RETROK_F5: return GT_KEY_PF5;
        case RETROK_F6: return GT_KEY_PF6;
        case RETROK_F7: return GT_KEY_PF7;
        case RETROK_F8: return GT_KEY_PF8;
        case RETROK_F9: return GT_KEY_PF9;
        case RETROK_F10: return GT_KEY_PF10;
        case RETROK_F11: return GT_KEY_PF11;
        case RETROK_F12: return GT_KEY_PF12;
        case RETROK_F13: return GT_KEY_PF13;
        case RETROK_F14: return GT_KEY_PF14;
        case RETROK_F15: return GT_KEY_PF15;
        case RETROK_PRINT: return GT_KEY_COPY;
        case RETROK_PAUSE: return GT_KEY_BREAK;
        case RETROK_BREAK: return GT_KEY_BREAK;
        case RETROK_INSERT: return GT_KEY_INSERT;
        case RETROK_HOME: return GT_KEY_HOME;
        case RETROK_PAGEUP: return GT_KEY_PREVIOUS;
        case RETROK_DELETE: return GT_KEY_DELETE;
        case RETROK_PAGEDOWN: return GT_KEY_NEXT;
        case RETROK_RIGHT: return GT_KEY_RIGHT;
        case RETROK_LEFT: return GT_KEY_LEFT;
        case RETROK_DOWN: return GT_KEY_DOWN;
        case RETROK_UP: return GT_KEY_UP;
        case RETROK_KP_DIVIDE: return GT_KEY_KP_DIVIDE;
        case RETROK_KP_MULTIPLY: return GT_KEY_KP_MULTIPLY;
        case RETROK_KP_MINUS: return GT_KEY_KP_MINUS;
        case RETROK_KP_PLUS: return GT_KEY_KP_PLUS;
        case RETROK_KP_ENTER: return GT_KEY_KP_ENTER;
        case RETROK_KP1: return GT_KEY_KP_1;
        case RETROK_KP2: return GT_KEY_KP_2;
        case RETROK_KP3: return GT_KEY_KP_3;
        case RETROK_KP4: return GT_KEY_KP_4;
        case RETROK_KP5: return GT_KEY_KP_5;
        case RETROK_KP6: return GT_KEY_KP_6;
        case RETROK_KP7: return GT_KEY_KP_7;
        case RETROK_KP8: return GT_KEY_KP_8;
        case RETROK_KP9: return GT_KEY_KP_9;
        case RETROK_KP0: return GT_KEY_KP_0;
        case RETROK_KP_PERIOD: return GT_KEY_KP_PERIOD;
        case RETROK_KP_EQUALS: return GT_KEY_KP_EQUALS;
        case RETROK_LCTRL: return GT_KEY_CTRL;
        case RETROK_RCTRL: return GT_KEY_CTRL;
        case RETROK_LSHIFT: return GT_KEY_SHIFT;
        case RETROK_RSHIFT: return GT_KEY_SHIFT;
        case RETROK_LALT: return GT_KEY_ALT;
        case RETROK_RALT: return GT_KEY_KANA_KANJI;
        default: return GT_KEY_NONE;
    }
}

static void keyboard_event(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers)
{
    UNUSED(character);
    UNUSED(key_modifiers);

    if (!core || keycode >= RETROK_LAST)
        return;

    GT_Keys key = key_from_retro_key(keycode);

    if (key == GT_KEY_NONE)
        return;

    if (down)
    {
        if (retro_key_down[keycode])
            return;

        retro_key_down[keycode] = true;

        if (key_references[key]++ == 0)
            core->KeyPressed(key);
    }
    else
    {
        if (!retro_key_down[keycode])
            return;

        retro_key_down[keycode] = false;

        if (key_references[key] > 0 && --key_references[key] == 0)
            core->KeyReleased(key);
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
        case RETRO_DEVICE_TOWNS_MARTY:
            type = GT_CONTROLLER_MARTY_GAMEPAD;

            if (log_device && log_cb)
                log_cb(RETRO_LOG_INFO, "Controller %u: Marty gamepad\n", port);

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
        GT_GamePad_State state = { 0 };
        core->GetInput()->SetGamePadState((int)port, state);
    }

    for (int i = 0; i < MAX_BUTTONS; i++)
    {
        joypad_current[port][i] = 0;
        joypad_old[port][i] = 0;
    }

    mouse_current[port].delta_x = 0;
    mouse_current[port].delta_y = 0;
    mouse_current[port].remainder_x = 0.0f;
    mouse_current[port].remainder_y = 0.0f;
    mouse_current[port].button_left = 0;
    mouse_current[port].button_right = 0;
    mouse_current[port].delta_applied = false;

    if ((input_device[port] == RETRO_DEVICE_TOWNS_MOUSE) && core)
        core->GetInput()->ClearMouseInput((int)port);
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
            float scale = (float)mouse_sensitivity / 6.0f;
            mouse_current[j].remainder_x += (float)input_state_cb(j, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X) * scale;
            mouse_current[j].remainder_y += (float)input_state_cb(j, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y) * scale;
            mouse_current[j].delta_x = (int)mouse_current[j].remainder_x;
            mouse_current[j].delta_y = (int)mouse_current[j].remainder_y;
            mouse_current[j].remainder_x -= (float)mouse_current[j].delta_x;
            mouse_current[j].remainder_y -= (float)mouse_current[j].delta_y;
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
                core->GetInput()->SetMouseDelta(j, mouse_current[j].delta_x, mouse_current[j].delta_y);
                mouse_current[j].delta_applied = true;
            }

            GT_GamePad_State state = { 0 };

            if (mouse_current[j].button_left)
                state.buttons |= GT_GAMEPAD_A;

            if (mouse_current[j].button_right)
                state.buttons |= GT_GAMEPAD_B;

            core->GetInput()->SetGamePadState(j, state);
            continue;
        }

        u16 buttons = 0;
        if (joypad_current[j][0]) buttons |= GT_GAMEPAD_UP;

        if (joypad_current[j][1]) buttons |= GT_GAMEPAD_DOWN;

        if (joypad_current[j][2]) buttons |= GT_GAMEPAD_LEFT;

        if (joypad_current[j][3]) buttons |= GT_GAMEPAD_RIGHT;

        if (joypad_current[j][4]) buttons |= GT_GAMEPAD_RUN;

        if (joypad_current[j][5]) buttons |= GT_GAMEPAD_SELECT;

        if (joypad_current[j][6]) buttons |= GT_GAMEPAD_A;

        if (joypad_current[j][7]) buttons |= GT_GAMEPAD_B;

        if (input_device[j] == RETRO_DEVICE_TOWNS_MARTY)
        {
            if (joypad_current[j][11]) buttons |= GT_GAMEPAD_ZOOM;
        }

        if (input_device[j] == RETRO_DEVICE_TOWNS_6_BUTTON)
        {
            if (joypad_current[j][8]) buttons |= GT_GAMEPAD_C;

            if (joypad_current[j][9]) buttons |= GT_GAMEPAD_X;

            if (joypad_current[j][10]) buttons |= GT_GAMEPAD_Y;

            if (joypad_current[j][11]) buttons |= GT_GAMEPAD_Z;
        }

        GT_GamePad_State state = { buttons };
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

    var.key = "geartowns_mouse_sensitivity";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        mouse_sensitivity = CLAMP(atoi(var.value), 1, 15);
    }

    var.key = "geartowns_cdrom_speed";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        cdrom_speed = CLAMP(atoi(var.value), 1, 8);

        if (core)
            core->GetCDROM()->SetReadSpeed(cdrom_speed);
    }

    var.key = "geartowns_cdrom_preload";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && core)
    {
        core->GetMedia()->PreloadCdRom(strcmp(var.value, "Enabled") == 0);
    }
}

static bool path_is_floppy_content(const char* path)
{
    if (FloppyImage::IsImageName(path))
        return true;

    if (!ends_with_no_case(path, ".zip"))
        return false;

    std::vector<u8> archive;
    u8* data = NULL;
    u32 size = 0;

    if (!read_content_file(path, archive) ||
        !FloppyImage::ExtractFromZip(archive.data(), archive.size(), &data, &size, NULL, 0))
        return false;

    SafeDeleteArray(data);
    return true;
}

static bool read_content_file(const char* path, std::vector<u8>& data)
{
    MediaFile* file = MediaFile::OpenFile(path);

    if (!IsValidPointer(file))
        return false;

    s64 size = file->GetSize();
    bool ok = size > 0 && size <= (s64)k_floppy_max_image_size * 4;

    if (ok)
    {
        data.resize((size_t)size);
        ok = file->ReadExact(data.data(), (u64)size);
    }

    SafeDelete(file);
    return ok;
}

// The frontend file system when it has one, the working copy is replaced only once fully written
static bool write_content_file(const char* path, const u8* data, u32 size)
{
    std::string temporary(path);
    temporary += ".tmp";

    if (IsValidPointer(vfs_interface) && IsValidPointer(vfs_interface->rename) && IsValidPointer(vfs_interface->remove))
    {
        LibretroVfsFile file(vfs_interface);

        if (!file.Open(temporary.c_str(), RETRO_VFS_FILE_ACCESS_WRITE) || !file.WriteAll(data, size) ||
            !file.Close())
        {
            vfs_interface->remove(temporary.c_str());
            return false;
        }

        vfs_interface->remove(path);
        return vfs_interface->rename(temporary.c_str(), path) == 0;
    }

    FILE* file = fopen(temporary.c_str(), "wb");

    if (!IsValidPointer(file))
        return false;

    bool written = fwrite(data, 1, size, file) == size;
    written = (fclose(file) == 0) && written;
    remove(path);

    if (!written || rename(temporary.c_str(), path) != 0)
    {
        remove(temporary.c_str());
        return false;
    }

    return true;
}

static bool disk_add_images(const char* path, std::vector<DiskSetImage>& images)
{
    if (ends_with_no_case(path, ".m3u"))
    {
        std::vector<u8> data;
        std::vector<std::string> entries;

        if (!read_content_file(path, data) ||
            !Media::ParsePlaylist(path, reinterpret_cast<const char*>(data.data()), data.size(), entries))
            return false;

        for (size_t i = 0; i < entries.size(); i++)
        {
            if (ends_with_no_case(entries[i].c_str(), ".m3u") || !disk_add_images(entries[i].c_str(), images))
            {
                log_cb(RETRO_LOG_ERROR, "Playlist entry is missing or not a disk image: %s\n", entries[i].c_str());
                return false;
            }
        }

        return true;
    }

    if (!path_is_cd_content(path) && path_is_floppy_content(path))
        return disk_add_floppy_images(path, images);

    if (!path_is_cdrom_uri(path))
    {
        MediaFile* file = MediaFile::OpenFile(path);

        if (!IsValidPointer(file))
            return false;

        SafeDelete(file);
    }

    std::string label(path);
    size_t separator = label.find_last_of("/\\");

    if (separator != std::string::npos)
        label = label.substr(separator + 1);

    DiskSetImage image;
    image.path = path;
    image.label = label;
    image.disk = 0;
    image.cdrom = true;
    images.push_back(image);
    return true;
}

static bool disk_add_floppy_images(const char* path, std::vector<DiskSetImage>& images)
{
    std::vector<u8> data;

    if (!read_content_file(path, data))
        return false;

    if (ends_with_no_case(path, ".zip"))
    {
        u8* extracted = NULL;
        u32 size = 0;

        if (!FloppyImage::ExtractFromZip(data.data(), data.size(), &extracted, &size, NULL, 0))
            return false;

        data.assign(extracted, extracted + size);
        SafeDeleteArray(extracted);
    }

    int count = FloppyImage::GetDiskCount(data.data(), (u32)data.size());
    std::string file_name(path);
    size_t separator = file_name.find_last_of("/\\");

    if (separator != std::string::npos)
        file_name = file_name.substr(separator + 1);

    for (int i = 0; i < count; i++)
    {
        DiskSetImage image;
        image.path = path;
        image.disk = i;
        image.label = file_name;
        image.cdrom = false;

        if (count > 1)
        {
            char label[32];
            snprintf(label, sizeof(label), " (Disk %d)", i + 1);
            image.label += label;
        }

        images.push_back(image);
    }

    return count > 0;
}

static bool disk_insert(unsigned index)
{
    if (index >= disk_images.size() || disk_images[index].path.empty())
        return false;

    if (disk_images[index].cdrom)
        return core->LoadMedia(disk_images[index].path.c_str());

    return floppy_flush() && floppy_load_image(index);
}

static bool disk_eject(unsigned index)
{
    if (index >= disk_images.size())
        return true;

    if (disk_images[index].cdrom)
    {
        core->EjectMedia();
        return true;
    }

    if (!floppy_flush())
        return false;

    core->EjectFloppy(0);
    floppy_working_path.clear();
    return true;
}

static void disk_clear(void)
{
    disk_images.clear();
    disk_index = 0;
    disk_ejected = true;
    disk_ejected_entry = -1;
    floppy_working_path.clear();
    floppy_base_crc = 0;
}

static bool floppy_load_image(unsigned index)
{
    const DiskSetImage& image = disk_images[index];
    std::vector<u8> file;
    u32 offset = 0;
    u32 size = 0;

    if (!read_content_file(image.path.c_str(), file))
        return false;

    if (ends_with_no_case(image.path.c_str(), ".zip"))
    {
        u8* extracted = NULL;
        u32 extracted_size = 0;

        if (!FloppyImage::ExtractFromZip(file.data(), file.size(), &extracted, &extracted_size, NULL, 0))
            return false;

        file.assign(extracted, extracted + extracted_size);
        SafeDeleteArray(extracted);
    }

    if (!FloppyImage::GetDisk(file.data(), (u32)file.size(), image.disk, &offset, &size))
        return false;

    u32 base_crc = CalculateCRC32(0, file.data() + offset, (int)size);
    std::string name(image.path);
    size_t separator = name.find_last_of("/\\");

    if (separator != std::string::npos)
        name = name.substr(separator + 1);

    size_t dot = name.find_last_of('.');

    if (dot != std::string::npos)
        name = name.substr(0, dot);

    char working[4096];
    snprintf(working, sizeof(working), "%s%c%s.%08X.geartowns.d77", retro_save_directory, slash, name.c_str(),
        base_crc);

    std::vector<u8> copy;
    const u8* data = file.data() + offset;

    if (read_content_file(working, copy) && FloppyImage::IsD77(copy.data(), (u32)copy.size()))
    {
        log_cb(RETRO_LOG_INFO, "Loading floppy working copy: %s\n", working);
        data = copy.data();
        size = (u32)copy.size();
    }

    if (!core->InsertFloppy(0, data, size, false, base_crc))
        return false;

    floppy_working_path = working;
    floppy_base_crc = base_crc;
    return true;
}

static bool floppy_flush(void)
{
    FloppyDisk* disk = core->GetFloppy(0);

    if (!disk->IsInserted() || !disk->IsDirty())
        return true;

    if (floppy_working_path.empty() ||
        !write_content_file(floppy_working_path.c_str(), disk->GetImage(), disk->GetImageSize()))
        return false;

    disk->ClearDirty();
    log_cb(RETRO_LOG_INFO, "Floppy changes saved to %s\n", floppy_working_path.c_str());
    return true;
}

static void register_disk_control(void)
{
    struct retro_disk_control_ext_callback disk_control = {
        disk_set_eject_state,
        disk_get_eject_state,
        disk_get_image_index,
        disk_set_image_index,
        disk_get_num_images,
        disk_replace_image_index,
        disk_add_image_index,
        disk_set_initial_image,
        disk_get_image_path,
        disk_get_image_label
    };

    if (!environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, (void*)&disk_control))
    {
        struct retro_disk_control_callback legacy = {
            disk_set_eject_state,
            disk_get_eject_state,
            disk_get_image_index,
            disk_set_image_index,
            disk_get_num_images,
            disk_replace_image_index,
            disk_add_image_index
        };

        environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, (void*)&legacy);
    }
}

static bool disk_set_eject_state(bool ejected)
{
    if (!core || disk_ejected == ejected)
        return true;

    if (ejected)
    {
        if (!disk_eject(disk_index))
            return false;

        disk_ejected_entry = (int)disk_index;
        disk_ejected = true;
        return true;
    }

    if (!disk_insert(disk_index))
        return false;

    if (disk_ejected_entry >= 0 && disk_ejected_entry < (int)disk_images.size() &&
        disk_images[disk_ejected_entry].cdrom != disk_images[disk_index].cdrom)
        disk_insert((unsigned)disk_ejected_entry);

    disk_ejected_entry = -1;
    disk_ejected = false;
    return true;
}

static bool disk_get_eject_state(void)
{
    return disk_ejected;
}

static unsigned disk_get_image_index(void)
{
    return disk_index;
}

static bool disk_set_image_index(unsigned index)
{
    if (!disk_ejected || index > disk_images.size())
        return false;

    disk_index = index;
    return true;
}

static unsigned disk_get_num_images(void)
{
    return (unsigned)disk_images.size();
}

static bool disk_replace_image_index(unsigned index, const struct retro_game_info* info)
{
    if (!disk_ejected || index >= disk_images.size())
        return false;

    if (!info || !info->path)
    {
        disk_images.erase(disk_images.begin() + index);

        if (disk_ejected_entry == (int)index)
            disk_ejected_entry = -1;
        else if (disk_ejected_entry > (int)index)
            disk_ejected_entry--;

        if (disk_index >= disk_images.size() && disk_index > 0)
            disk_index--;

        return true;
    }

    std::vector<DiskSetImage> images;

    if (!disk_add_images(info->path, images) || images.empty())
        return false;

    disk_images[index] = images[0];
    return true;
}

static bool disk_add_image_index(void)
{
    if (!disk_ejected)
        return false;

    DiskSetImage image;
    image.disk = 0;
    image.cdrom = false;
    disk_images.push_back(image);
    return true;
}

static bool disk_set_initial_image(unsigned index, const char* path)
{
    if (!path || !path[0])
        return false;

    disk_initial_index = index;
    disk_initial_path = path;
    return true;
}

static bool disk_get_image_path(unsigned index, char* path, size_t len)
{
    if (!path || len == 0 || index >= disk_images.size() || disk_images[index].path.empty())
        return false;

    snprintf(path, len, "%s", disk_images[index].path.c_str());
    return true;
}

static bool disk_get_image_label(unsigned index, char* label, size_t len)
{
    if (!label || len == 0 || index >= disk_images.size())
        return false;

    const DiskSetImage& image = disk_images[index];
    snprintf(label, len, "%s", !image.label.empty() ? image.label.c_str() : image.cdrom ? "CD-ROM" : "Floppy disk");
    return true;
}
