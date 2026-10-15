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
#include <math.h>
#include <string>
#include <vector>
#include "libretro.h"
#include "geartowns.h"
#include "media/media_file.h"
#include "media/crc.h"
#include "drive/fdc.h"
#include "drive/floppy_disk.h"
#include "drive/floppy_image.h"
#include "system/msm58321.h"
#include "video/video.h"
#include "libretro_core_options.h"
#include "libretro_vfs_file.h"

#define RETRO_DEVICE_TOWNS_GAMEPAD    RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define RETRO_DEVICE_TOWNS_6_BUTTON   RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
#define RETRO_DEVICE_TOWNS_MARTY      RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 2)
#define RETRO_DEVICE_TOWNS_MOUSE      RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_MOUSE, 0)
#define RETRO_DEVICE_TOWNS_KEYBOARD   RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_KEYBOARD, 0)

#define MAX_PADS GT_MAX_GAMEPADS
#define AUTO_MOUSE_PORT 1
#define FRAMESKIP_MAX 4
#define RTC_FIXED_DATE 631152000
#define DISK_DRIVE_CDROM 0
#define DISK_DRIVE_FLOPPY_1 1
#define DISK_DRIVE_FLOPPY_2 2
#define DISK_DRIVE_COUNT (1 + FDC_DRIVES)
#define FLOPPY_IDLE_FRAMES 180
#define CMOS_SAVE_FRAMES 180
#define PLAYLIST_MAX_SIZE (1024 * 1024)
#define CONTENT_MAX_SIZE (k_floppy_max_image_size * 4)
#define INPUT_STATE_SIZE ((sizeof(float) * 2) + (sizeof(u16) * MAX_PADS))

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static struct retro_log_callback logging;
retro_log_printf_t log_cb;
static const retro_vfs_interface* vfs_interface = NULL;
static unsigned message_interface_version = 0;
static bool libretro_supports_bitmasks = false;
static bool libretro_supports_dupe = false;
static bool libretro_supports_keyboard = false;
static bool categories_supported = false;
static bool frameskip_threshold_visible = true;
static bool analog_mouse_speed_visible = true;

static char retro_system_directory[4096];
static char retro_save_directory[4096];
static char retro_game_path[4096];
static bool content_loaded = false;

static GeartownsCore* core;
static GT_Runtime_Info runtime_info;
static u8* frame_buffer;
static s16 audio_buf[GT_AUDIO_BUFFER_SIZE];
static int audio_sample_count = 0;
static size_t savestate_size = 0;
static int machine_ram_mb = 2;
static int machine_cpu_mhz = 16;
static int machine_floppy_drives = 2;
#if !defined(GT_DISABLE_DYNAREC)
static int dynarec_option = -1;
#endif

static int current_screen_width = 0;
static int current_screen_height = 0;
static float current_aspect_ratio = 0.0f;
static float current_fps = 60.0f;
static float aspect_ratio = 0.0f;
static int frameskip_type = 0;
static unsigned frameskip_threshold = 33;
static unsigned frameskip_counter = 0;
static bool frameskip_changed = false;
static bool audio_buffer_active = false;
static unsigned audio_buffer_occupancy = 0;
static bool audio_buffer_underrun = false;

static bool allow_up_down = false;
static int mouse_sensitivity = 5;
static int analog_mouse = 0;
static int analog_mouse_speed = 5;
static unsigned requested_device[MAX_PADS] = {
    RETRO_DEVICE_JOYPAD,
    RETRO_DEVICE_JOYPAD
};
static unsigned input_device[MAX_PADS] = {
    RETRO_DEVICE_TOWNS_GAMEPAD,
    RETRO_DEVICE_TOWNS_MOUSE
};
static u16 joypad_previous[MAX_PADS];
static float mouse_remainder_x = 0.0f;
static float mouse_remainder_y = 0.0f;
static bool retro_key_down[RETROK_LAST];
static int key_references[GT_KEY_COUNT];

static const u16 k_joypad_buttons[RETRO_DEVICE_ID_JOYPAD_R + 1] = {
    GT_GAMEPAD_B, GT_GAMEPAD_C, GT_GAMEPAD_SELECT, GT_GAMEPAD_RUN, GT_GAMEPAD_UP, GT_GAMEPAD_DOWN, GT_GAMEPAD_LEFT,
    GT_GAMEPAD_RIGHT, GT_GAMEPAD_A, GT_GAMEPAD_X, GT_GAMEPAD_Y, GT_GAMEPAD_Z
};
static const u16 k_6_button_joypad_buttons[RETRO_DEVICE_ID_JOYPAD_R + 1] = {
    GT_GAMEPAD_B, GT_GAMEPAD_A, GT_GAMEPAD_SELECT, GT_GAMEPAD_RUN, GT_GAMEPAD_UP, GT_GAMEPAD_DOWN, GT_GAMEPAD_LEFT,
    GT_GAMEPAD_RIGHT, GT_GAMEPAD_C, GT_GAMEPAD_Y, GT_GAMEPAD_X, GT_GAMEPAD_Z
};
static const u16 k_gamepad_buttons = GT_GAMEPAD_UP | GT_GAMEPAD_DOWN | GT_GAMEPAD_LEFT | GT_GAMEPAD_RIGHT |
    GT_GAMEPAD_SELECT | GT_GAMEPAD_RUN | GT_GAMEPAD_A | GT_GAMEPAD_B;
static const u16 k_6_button_buttons = k_gamepad_buttons | GT_GAMEPAD_C | GT_GAMEPAD_X | GT_GAMEPAD_Y | GT_GAMEPAD_Z;

struct DiskSetImage
{
    std::string path;
    std::string label;
    int disk;
    u32 crc;
    bool crc_known;
    bool cdrom;
    bool save_disk;
};

static std::vector<DiskSetImage> disk_images;
static int drive_entry[DISK_DRIVE_COUNT] = { -1, -1, -1 };
static unsigned disk_index = 0;
static bool disk_ejected = true;
static int open_drive = -1;
static int open_entry = -1;
static unsigned disk_initial_index = 0;
static std::string disk_initial_path;
static std::string floppy_working_path[FDC_DRIVES];
static u32 floppy_write_count[FDC_DRIVES];
static int floppy_idle_frames[FDC_DRIVES];
static bool floppy_flush_failed[FDC_DRIVES];
static int floppy_drive = 0;
static bool floppy_multidrive = false;
static bool save_disk = false;
static bool cmos_shared = true;
static bool cmos_shared_active = false;
static std::string cmos_path;
static u8 cmos_saved[GT_CMOS_SIZE];
static int cmos_frames = 0;
static bool cmos_save_failed = false;

static bool load_content(const char* path);
static void unload_content(void);
static bool load_bios(void);
static s64 get_firmware_size(const char* directory, GT_Firmware_Type type);
static void check_bios(void);
static void set_machine_config(void);
static void show_message(const char* message, unsigned priority, enum retro_log_level level);
static void get_system_av_info(struct retro_system_av_info* info);
static void audio_buffer_status(bool active, unsigned occupancy, bool underrun_likely);
static void update_frameskip(void);
static void set_controller_info(void);
static void set_input_descriptors(void);
static int add_input_descriptors(struct retro_input_descriptor* descriptors, int count, unsigned port,
    const struct retro_input_descriptor* source, int source_count);
static void clear_input_state(void);
static void clear_keyboard_state(void);
static void save_input_state(u8* data);
static void load_input_state(const u8* data);
static void sync_keyboard_state(void);
static void sync_key(GT_Keys key);
static void reset_controller_devices(void);
static int get_mouse_port(void);
static int get_requested_mouse_port(void);
static unsigned resolve_controller_device(unsigned port);
static void update_controller_devices(unsigned port);
static void apply_controller_device(unsigned port, unsigned device, bool log_device);
static void release_controller_input(unsigned port);
static void poll_input(void);
static void apply_input(void);
static void apply_gamepad(int port);
static u16 filter_opposing(u16 buttons, u16 previous, u16 first, u16 second);
static void apply_mouse(int port);
static void poll_analog_mouse(bool& left, bool& right);
static GT_Keys key_from_retro_key(unsigned keycode);
static void keyboard_event(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers);
static bool update_core_options_display(void);
static void check_variables(void);
static void update_floppy_drive(void);
static bool path_is_cdrom_uri(const char* path);
static bool path_is_cd_content(const char* path);
static bool disk_load(const char* path);
static bool disk_add_images(const char* path, std::vector<DiskSetImage>& images);
static bool disk_add_playlist(const char* path, std::vector<DiskSetImage>& images);
static bool disk_add_floppy_images(const char* path, std::vector<DiskSetImage>& images);
static bool disk_add_save_disk(const std::string& name, const char* label, const char* volume,
    std::vector<DiskSetImage>& images);
static int get_entry_drive(unsigned index);
static bool disk_insert(unsigned index, int drive);
static bool disk_eject(int drive);
static void disk_close_open_drive(int drive);
static void disk_sync(void);
static void disk_clear(void);
static void disk_shift_entry(int& entry, unsigned index, int delta);
static bool mount_state_media(const GT_SaveState_Media& media);
static int find_floppy_entry(u32 crc);
static int find_cdrom_entry(u32 crc);
static bool get_cdrom_crc(const char* path, u32* crc);
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
static bool read_content_file(const char* path, std::vector<u8>& data, s64 max_size);
static bool load_source(const char* path, std::vector<u8>& data);
static bool write_content_file(const char* path, const u8* data, u32 size);
static bool rename_file(const char* from, const char* to, bool vfs);
static void remove_file(const char* path, bool vfs);
static bool floppy_load_image(unsigned index, int floppy);
static bool floppy_flush(int floppy);
static void floppy_update(void);
static void cmos_load(void);
static void cmos_save(void);
static void cmos_update(void);

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
    (void)level;
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
}

static bool IsJoypadDevice(unsigned device)
{
    return (device == RETRO_DEVICE_JOYPAD) ||
        (device == RETRO_DEVICE_TOWNS_GAMEPAD) ||
        (device == RETRO_DEVICE_TOWNS_MARTY) ||
        (device == RETRO_DEVICE_TOWNS_6_BUTTON);
}

static std::string get_file_name(const char* path, bool strip_extension)
{
    std::string name(path);
    size_t separator = name.find_last_of("/\\");

    if (separator != std::string::npos)
        name = name.substr(separator + 1);

    size_t dot = name.find_last_of('.');

    if (strip_extension && dot != std::string::npos)
        name = name.substr(0, dot);

    return name;
}

static std::string get_save_disk_name(const std::string& name)
{
    std::string sanitized(name.empty() ? "geartowns" : name);

    for (size_t i = 0; i < sanitized.size(); i++)
    {
        if ((unsigned char)sanitized[i] < 0x20 || strchr("<>:\"/\\|?*", sanitized[i]) != NULL)
            sanitized[i] = '_';
    }

    return sanitized;
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

    bool support_no_game = true;
    environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &support_no_game);

    register_disk_control();
    set_controller_info();
    libretro_set_core_options(environ_cb, &categories_supported);

    frameskip_threshold_visible = true;
    analog_mouse_speed_visible = true;
    struct retro_core_options_update_display_callback display_callback = { update_core_options_display };
    environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK, &display_callback);
    update_core_options_display();
}

void retro_init(void)
{
    struct retro_vfs_interface_info vfs_interface_info = { };
    vfs_interface_info.required_interface_version = 1;

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

    frame_buffer = new u8[GT_MAX_FRAME_BUFFER_WIDTH * GT_MAX_FRAME_BUFFER_HEIGHT * sizeof(u32)];

    clear_input_state();

    for (int i = 0; i < MAX_PADS; i++)
        apply_controller_device(i, input_device[i], false);

    libretro_supports_bitmasks = environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL);

    if (!environ_cb(RETRO_ENVIRONMENT_GET_CAN_DUPE, &libretro_supports_dupe))
        libretro_supports_dupe = false;

    if (!environ_cb(RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION, &message_interface_version))
        message_interface_version = 0;

    uint64_t quirks = RETRO_SERIALIZATION_QUIRK_ENDIAN_DEPENDENT;
    environ_cb(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS, &quirks);
}

void retro_deinit(void)
{
    if (content_loaded)
        unload_content();

    SafeDeleteArray(frame_buffer);
    SafeDelete(core);
    vfs_interface = NULL;
    MediaFile::SetVfsInterface(NULL);

    message_interface_version = 0;
    libretro_supports_bitmasks = false;
    libretro_supports_dupe = false;
    libretro_supports_keyboard = false;
    audio_sample_count = 0;
    machine_ram_mb = 2;
    machine_cpu_mhz = 16;
    machine_floppy_drives = 2;
#if !defined(GT_DISABLE_DYNAREC)
    dynarec_option = -1;
#endif
    current_screen_width = 0;
    current_screen_height = 0;
    current_aspect_ratio = 0.0f;
    current_fps = 60.0f;
    aspect_ratio = 0.0f;
    frameskip_type = 0;
    frameskip_threshold = 33;
    frameskip_counter = 0;
    frameskip_changed = false;
    audio_buffer_active = false;
    allow_up_down = false;
    mouse_sensitivity = 5;
    analog_mouse = 0;
    analog_mouse_speed = 5;
    disk_initial_index = 0;
    disk_initial_path.clear();
    floppy_drive = 0;
    floppy_multidrive = false;
    save_disk = false;
    cmos_shared = true;
    cmos_shared_active = false;
    cmos_path.clear();

    reset_controller_devices();
    clear_input_state();
}

void retro_reset(void)
{
    log_cb(RETRO_LOG_DEBUG, "Resetting...\n");

    check_variables();
    cmos_save();
    core->ResetMedia();
    clear_input_state();
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
    if (port >= MAX_PADS)
    {
        if (log_cb)
            log_cb(RETRO_LOG_DEBUG, "retro_set_controller_port_device invalid port number: %u\n", port);

        return;
    }

    if (device == RETRO_DEVICE_MOUSE)
        device = RETRO_DEVICE_TOWNS_MOUSE;

    int mouse_port = get_requested_mouse_port();

    if ((device == RETRO_DEVICE_TOWNS_MOUSE) && (mouse_port >= 0) && (mouse_port != (int)port))
    {
        if (log_cb)
            log_cb(RETRO_LOG_WARN, "Only one mouse is supported, controller port %u is unplugged\n", port + 1);

        device = RETRO_DEVICE_NONE;
    }

    requested_device[port] = device;
    update_controller_devices(port);
}

void retro_get_system_info(struct retro_system_info *info)
{
    memset(info, 0, sizeof(*info));
    info->library_name     = GT_TITLE;
    info->library_version  = GT_VERSION;
    info->need_fullpath    = true;
    info->block_extract    = true;
    info->valid_extensions = "cue|chd|iso|zip|d77|d88|hdm|xdf|img|m3u";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    get_system_av_info(info);
    current_fps = (float)info->timing.fps;
}

void retro_run(void)
{
    bool core_options_updated = false;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &core_options_updated) && core_options_updated)
        check_variables();

    if (frameskip_changed)
        update_frameskip();

    poll_input();
    apply_input();

    bool render = true;
    int av_enable = 0;

    if (environ_cb(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE, &av_enable))
        render = (av_enable & 1) != 0;

    if (render && frameskip_type > 0 && audio_buffer_active)
    {
        bool skip = frameskip_type == 1 ? audio_buffer_underrun : audio_buffer_occupancy < frameskip_threshold;

        if (skip && frameskip_counter < FRAMESKIP_MAX)
        {
            render = false;
            frameskip_counter++;
        }
        else
            frameskip_counter = 0;
    }

    audio_sample_count = 0;
    bool powered = core->IsPoweredOn();
    core->RunToFrame(frame_buffer, audio_buf, &audio_sample_count, render);
    floppy_update();
    cmos_update();

    if (powered && !core->IsPoweredOn())
    {
        show_message("Powered off by software", 2, RETRO_LOG_INFO);
        log_cb(RETRO_LOG_INFO, "Powered off by software\n");
        environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
    }

    retro_system_av_info info;
    get_system_av_info(&info);
    bool fps_changed = fabsf((float)info.timing.fps - current_fps) > 0.1f;
    bool geometry_changed = (runtime_info.screen_width != current_screen_width) ||
                            (runtime_info.screen_height != current_screen_height) ||
                            (info.geometry.aspect_ratio != current_aspect_ratio);

    if (fps_changed || geometry_changed)
    {
        current_screen_width = runtime_info.screen_width;
        current_screen_height = runtime_info.screen_height;
        current_aspect_ratio = info.geometry.aspect_ratio;
        current_fps = (float)info.timing.fps;

        if (fps_changed)
        {
            log_cb(RETRO_LOG_INFO, "Refresh rate changed to %.2f Hz\n", current_fps);
            environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &info);

            if (frameskip_type > 0)
                frameskip_changed = true;
        }
        else
        {
            environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &info.geometry);
        }
    }

    video_cb((render || !libretro_supports_dupe) ? frame_buffer : NULL, runtime_info.screen_width,
        runtime_info.screen_height, runtime_info.screen_width * sizeof(u32));

    if (audio_sample_count > 0)
        audio_batch_cb(audio_buf, audio_sample_count / 2);
}

bool retro_load_game(const struct retro_game_info *info)
{
    return load_content((info && info->path) ? info->path : "");
}

void retro_unload_game(void)
{
    unload_content();
}

unsigned retro_get_region(void)
{
    return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
    UNUSED(game_type);
    UNUSED(info);
    UNUSED(num_info);
    return false;
}

size_t retro_serialize_size(void)
{
    return savestate_size > 0 ? savestate_size + INPUT_STATE_SIZE : 0;
}

bool retro_serialize(void *data, size_t size)
{
    u8* buffer = reinterpret_cast<u8*>(data);
    size_t core_size = savestate_size;

    if ((savestate_size == 0) || (size < savestate_size + INPUT_STATE_SIZE) || !core->SaveState(buffer, core_size) ||
        (core_size != savestate_size))
        return false;

    save_input_state(buffer + savestate_size);
    return true;
}

bool retro_unserialize(const void *data, size_t size)
{
    const u8* buffer = reinterpret_cast<const u8*>(data);

    if ((savestate_size == 0) || (size < savestate_size + INPUT_STATE_SIZE))
        return false;

    GT_SaveState_Media media = {};
    bool loaded = core->LoadState(buffer, savestate_size, &media);

    if (!loaded && media.valid && mount_state_media(media))
    {
        loaded = core->LoadState(buffer, savestate_size);
        disk_sync();
    }

    if (loaded)
    {
        load_input_state(buffer + savestate_size);
        sync_keyboard_state();
    }

    return loaded;
}

void *retro_get_memory_data(unsigned id)
{
    switch (id)
    {
        case RETRO_MEMORY_SAVE_RAM:
            return cmos_shared_active ? NULL : core->GetMemory()->GetCMOS();
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
            return cmos_shared_active ? 0 : GT_CMOS_SIZE;
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

static bool load_content(const char* path)
{
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;

    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
    {
        log_cb(RETRO_LOG_ERROR, "XRGB8888 is not supported.\n");
        return false;
    }

    check_variables();

    if (frameskip_type > 0)
        frameskip_changed = true;

    snprintf(retro_game_path, sizeof(retro_game_path), "%s", path);
    log_cb(RETRO_LOG_INFO, "Loading %s\n", path[0] != 0 ? path : "no content");

    if (!load_bios())
    {
        unload_content();
        return false;
    }

    check_bios();
    set_machine_config();
    cmos_load();
    disk_clear();
    clear_input_state();

    if (!disk_load(path))
    {
        unload_content();
        return false;
    }

    if (!core->PowerOn())
    {
        log_cb(RETRO_LOG_ERROR, "Unable to power on the machine\n");
        unload_content();
        return false;
    }

    content_loaded = true;

    if (!core->GetMaxSaveStateSize(savestate_size))
        savestate_size = 0;

    log_cb(RETRO_LOG_INFO, "Save state size: %zu bytes\n", savestate_size);
    disk_sync();

    struct retro_memory_descriptor descriptors[2] = {};
    unsigned count = 0;
    u8* ram = core->GetMemory()->GetWorkingRAM();
    u32 ram_size = core->GetMemory()->GetWorkingRAMSize();

    for (u32 start = 0; (start < ram_size) && (count < 2); count++)
    {
        u32 length = 1;

        while (length * 2 <= ram_size - start)
            length *= 2;

        descriptors[count].flags = RETRO_MEMDESC_SYSTEM_RAM;
        descriptors[count].ptr = ram + start;
        descriptors[count].start = start;
        descriptors[count].len = length;
        start += length;
    }

    struct retro_memory_map memory_map = { descriptors, count };
    environ_cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &memory_map);

    bool achievements = true;
    environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS, &achievements);

    core->GetRuntimeInfo(runtime_info);

    struct retro_keyboard_callback keyboard = { keyboard_event };
    libretro_supports_keyboard = environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &keyboard);
    log_cb(RETRO_LOG_INFO, "Enable Game Focus (Scroll Lock) to type on the FM Towns keyboard\n");

    return true;
}

static void unload_content(void)
{
    for (int i = 0; i < FDC_DRIVES; i++)
    {
        if (!floppy_flush(i))
            log_cb(RETRO_LOG_ERROR, "Unable to save the floppy %d changes\n", i + 1);

        core->EjectFloppy(i);
    }

    cmos_save();
    cmos_shared_active = false;
    core->EjectMedia();
    core->PowerOff();
    disk_clear();
    clear_input_state();

    content_loaded = false;
    retro_game_path[0] = 0;
    savestate_size = 0;
    current_fps = 60.0f;
    memset(frame_buffer, 0, GT_MAX_FRAME_BUFFER_WIDTH * GT_MAX_FRAME_BUFFER_HEIGHT * sizeof(u32));
}

static bool load_bios(void)
{
    std::string directory(retro_system_directory);
    append_path_component(directory, "geartowns");

    if (core->LoadBios(directory.c_str()) || core->LoadBios(retro_system_directory))
        return true;

    std::string reported(directory);

    if (get_firmware_size(directory.c_str(), GT_FIRMWARE_SYSTEM) < 0 &&
        get_firmware_size(retro_system_directory, GT_FIRMWARE_SYSTEM) >= 0)
        reported = retro_system_directory;

    std::string missing;

    for (int i = 0; i < GT_FIRMWARE_COUNT; i++)
    {
        GT_Firmware_Type type = (GT_Firmware_Type)i;
        int expected = Firmware::GetExpectedSize(type);
        s64 size = get_firmware_size(reported.c_str(), type);

        if (!Firmware::IsRequired(type) || size == expected)
            continue;

        if (size < 0)
            log_cb(RETRO_LOG_ERROR, "Missing %s: %s or %s (%d bytes)\n", Firmware::GetComponentName(type),
                Firmware::GetFileName(type), Firmware::GetAlternativeFileName(type), expected);
        else
            log_cb(RETRO_LOG_ERROR, "Invalid %s: %s is %lld bytes, expected %d\n", Firmware::GetComponentName(type),
                Firmware::GetFilePath(reported.c_str(), type).c_str(), (long long)size, expected);

        if (!missing.empty())
            missing += ", ";

        missing += Firmware::GetFileName(type);
    }

    log_cb(RETRO_LOG_INFO, "FM Towns firmware is read from %s, or from %s when that folder has no complete set\n",
        directory.c_str(), retro_system_directory);

    static char message[256];

    if (missing.empty())
        snprintf(message, sizeof(message), "FM Towns firmware could not be loaded");
    else
        snprintf(message, sizeof(message), "Missing FM Towns firmware: %s", missing.c_str());

    show_message(message, 3, RETRO_LOG_ERROR);
    log_cb(RETRO_LOG_ERROR, "%s\n", message);
    return false;
}

static s64 get_firmware_size(const char* directory, GT_Firmware_Type type)
{
    MediaFile* file = MediaFile::OpenFile(Firmware::GetFilePath(directory, type).c_str());
    s64 size = IsValidPointer(file) ? file->GetSize() : -1;
    SafeDelete(file);
    return size;
}

static void check_bios(void)
{
    std::string unknown;
    Firmware* firmware = core->GetFirmware();

    for (int i = 0; i < GT_FIRMWARE_COUNT; i++)
    {
        GT_Firmware_Type type = (GT_Firmware_Type)i;
        const GT_Firmware_Info& info = firmware->GetInfo(type);

        if (!info.loaded || info.recognized)
            continue;

        char entry[64];
        snprintf(entry, sizeof(entry), "%s%s (CRC %08X)", unknown.empty() ? "" : ", ", Firmware::GetFileName(type),
            info.crc);
        unknown += entry;
    }

    if (unknown.empty())
        return;

    static char message[256];
    snprintf(message, sizeof(message), "Unknown FM Towns firmware: %s, the Model 1/2 set is recommended",
        unknown.c_str());
    show_message(message, 2, RETRO_LOG_WARN);
    log_cb(RETRO_LOG_WARN, "%s\n", message);
}

static void set_machine_config(void)
{
    GT_Machine_Config config = core->GetPendingMachineConfig();
    config.model = GT_MACHINE_MODEL1_2;
    config.ram_size = (u32)machine_ram_mb * 1024 * 1024;
    config.floppy_drives = machine_floppy_drives;
    config.cpu_clock_rate = (u32)machine_cpu_mhz * 1000000;
    core->SetMachineConfig(config);

    const GT_Machine_Config& pending = core->GetPendingMachineConfig();
    log_cb(RETRO_LOG_INFO, "Machine: %s, %u MB RAM, %u MHz, %d floppy drives\n", k_machine_profiles[pending.model].name,
        pending.ram_size / (1024 * 1024), pending.cpu_clock_rate / 1000000, pending.floppy_drives);

    if (floppy_drive == 1 && pending.floppy_drives < 2)
        log_cb(RETRO_LOG_WARN, "The machine has one floppy drive, disk control uses Floppy 1\n");
}

static void show_message(const char* message, unsigned priority, enum retro_log_level level)
{
    if (message_interface_version >= 1)
    {
        struct retro_message_ext msg = {};
        msg.msg = message;
        msg.duration = 6000;
        msg.priority = priority;
        msg.level = level;
        msg.target = RETRO_MESSAGE_TARGET_OSD;
        msg.type = RETRO_MESSAGE_TYPE_NOTIFICATION;
        msg.progress = -1;
        environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);
        return;
    }

    struct retro_message msg = {};
    msg.msg = message;
    msg.frames = 360;
    environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
}

static void get_system_av_info(struct retro_system_av_info* info)
{
    core->GetRuntimeInfo(runtime_info);

    info->geometry.base_width   = runtime_info.screen_width;
    info->geometry.base_height  = runtime_info.screen_height;
    info->geometry.max_width    = GT_MAX_FRAME_BUFFER_WIDTH;
    info->geometry.max_height   = GT_MAX_FRAME_BUFFER_HEIGHT;
    info->geometry.aspect_ratio = aspect_ratio == 0.0f ?
        (float)runtime_info.display_width / (float)runtime_info.display_height : aspect_ratio;
    info->timing.fps            = 1000.0f / runtime_info.frame_time;
    info->timing.sample_rate    = GT_AUDIO_SAMPLE_RATE;
}

static void audio_buffer_status(bool active, unsigned occupancy, bool underrun_likely)
{
    audio_buffer_active = active;
    audio_buffer_occupancy = occupancy;
    audio_buffer_underrun = underrun_likely;
}

static void update_frameskip(void)
{
    struct retro_audio_buffer_status_callback callback = { audio_buffer_status };
    unsigned latency = 0;
    frameskip_changed = false;
    frameskip_counter = 0;
    audio_buffer_active = false;

    if (frameskip_type > 0 && environ_cb(RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK, &callback))
    {
        latency = (unsigned)ceilf(6.0f * 1000.0f / current_fps);
        latency = ((latency + 31) / 32) * 32;
    }
    else
        environ_cb(RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK, NULL);

    environ_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &latency);
}

static void set_controller_info(void)
{
    static const struct retro_controller_description port_1[] = {
        { "Original gamepad", RETRO_DEVICE_JOYPAD },
        { "Marty gamepad", RETRO_DEVICE_TOWNS_MARTY },
        { "6 button gamepad", RETRO_DEVICE_TOWNS_6_BUTTON },
        { "Mouse", RETRO_DEVICE_TOWNS_MOUSE },
        { "Keyboard", RETRO_DEVICE_TOWNS_KEYBOARD }
    };

    static const struct retro_controller_description port_2[] = {
        { "Mouse", RETRO_DEVICE_JOYPAD },
        { "Original gamepad", RETRO_DEVICE_TOWNS_GAMEPAD },
        { "Marty gamepad", RETRO_DEVICE_TOWNS_MARTY },
        { "6 button gamepad", RETRO_DEVICE_TOWNS_6_BUTTON },
        { "Keyboard", RETRO_DEVICE_TOWNS_KEYBOARD }
    };

    static const struct retro_controller_info ports[] = {
        { port_1, 5 },
        { port_2, 5 },
        { NULL, 0 }
    };

    environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

    set_input_descriptors();
}

static void set_input_descriptors(void)
{
    static const struct retro_input_descriptor k_gamepad[] = {
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Up" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Run" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "A" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "B" }
    };

    static const struct retro_input_descriptor k_marty[] = {
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Zoom" }
    };

    static const struct retro_input_descriptor k_6_button[] = {
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Up" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Run" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "A" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "B" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "C" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "X" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,      "Y" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Z" }
    };

    static const struct retro_input_descriptor k_mouse[] = {
        { 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT,     "Mouse Left (A)" },
        { 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT,    "Mouse Right (B)" }
    };

    static const struct retro_input_descriptor k_analog_mouse[] = {
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "Analog Mouse Left" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "Analog Mouse Right" },
        { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog Mouse X (Left)" },
        { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Mouse Y (Left)" },
        { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "Analog Mouse X (Right)" },
        { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Mouse Y (Right)" }
    };

    struct retro_input_descriptor descriptors[64];
    int count = 0;

    for (unsigned port = 0; port < MAX_PADS; port++)
    {
        unsigned device = input_device[port];

        if (device == RETRO_DEVICE_TOWNS_6_BUTTON)
            count = add_input_descriptors(descriptors, count, port, k_6_button,
                (int)(sizeof(k_6_button) / sizeof(k_6_button[0])));
        else if (IsJoypadDevice(device))
            count = add_input_descriptors(descriptors, count, port, k_gamepad,
                (int)(sizeof(k_gamepad) / sizeof(k_gamepad[0])));

        if (device == RETRO_DEVICE_TOWNS_MARTY)
            count = add_input_descriptors(descriptors, count, port, k_marty,
                (int)(sizeof(k_marty) / sizeof(k_marty[0])));

        count = add_input_descriptors(descriptors, count, port, k_mouse, (int)(sizeof(k_mouse) / sizeof(k_mouse[0])));

        if (port == 0)
            count = add_input_descriptors(descriptors, count, port, k_analog_mouse,
                (int)(sizeof(k_analog_mouse) / sizeof(k_analog_mouse[0])));
    }

    memset(&descriptors[count], 0, sizeof(descriptors[count]));
    environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, descriptors);
}

static int add_input_descriptors(struct retro_input_descriptor* descriptors, int count, unsigned port,
    const struct retro_input_descriptor* source, int source_count)
{
    for (int i = 0; i < source_count; i++)
    {
        descriptors[count] = source[i];
        descriptors[count].port = port;
        count++;
    }

    return count;
}

static void clear_input_state(void)
{
    clear_keyboard_state();
    memset(joypad_previous, 0, sizeof(joypad_previous));
    mouse_remainder_x = 0.0f;
    mouse_remainder_y = 0.0f;
}

static void clear_keyboard_state(void)
{
    memset(retro_key_down, 0, sizeof(retro_key_down));
    memset(key_references, 0, sizeof(key_references));

    if (core)
        core->ReleaseAllKeys();
}

static void sync_keyboard_state(void)
{
    for (int i = GT_KEY_NONE + 1; i < GT_KEY_COUNT; i++)
    {
        if ((key_references[i] == 0) && (i != GT_KEY_CTRL) && (i != GT_KEY_SHIFT))
            core->KeyReleased((GT_Keys)i);
    }

    sync_key(GT_KEY_CTRL);
    sync_key(GT_KEY_SHIFT);

    for (int i = GT_KEY_NONE + 1; i < GT_KEY_COUNT; i++)
    {
        if (key_references[i] > 0)
            core->KeyPressed((GT_Keys)i);
    }
}

static void sync_key(GT_Keys key)
{
    if (key_references[key] > 0)
        core->KeyPressed(key);
    else
        core->KeyReleased(key);
}

static void save_input_state(u8* data)
{
    memcpy(data, &mouse_remainder_x, sizeof(float));
    memcpy(data + sizeof(float), &mouse_remainder_y, sizeof(float));
    memcpy(data + (sizeof(float) * 2), joypad_previous, sizeof(joypad_previous));
}

static void load_input_state(const u8* data)
{
    memcpy(&mouse_remainder_x, data, sizeof(float));
    memcpy(&mouse_remainder_y, data + sizeof(float), sizeof(float));
    memcpy(joypad_previous, data + (sizeof(float) * 2), sizeof(joypad_previous));

    if (!(fabsf(mouse_remainder_x) < 1.0f))
        mouse_remainder_x = 0.0f;

    if (!(fabsf(mouse_remainder_y) < 1.0f))
        mouse_remainder_y = 0.0f;
}

static void reset_controller_devices(void)
{
    for (int i = 0; i < MAX_PADS; i++)
        requested_device[i] = RETRO_DEVICE_JOYPAD;

    for (int i = 0; i < MAX_PADS; i++)
        input_device[i] = resolve_controller_device(i);
}

static int get_mouse_port(void)
{
    for (int i = 0; i < MAX_PADS; i++)
    {
        if (input_device[i] == RETRO_DEVICE_TOWNS_MOUSE)
            return i;
    }

    return -1;
}

static int get_requested_mouse_port(void)
{
    for (int i = 0; i < MAX_PADS; i++)
    {
        if (requested_device[i] == RETRO_DEVICE_TOWNS_MOUSE)
            return i;
    }

    return -1;
}

static unsigned resolve_controller_device(unsigned port)
{
    if (requested_device[port] != RETRO_DEVICE_JOYPAD)
        return requested_device[port];

    if ((port == AUTO_MOUSE_PORT) && (get_requested_mouse_port() < 0))
        return RETRO_DEVICE_TOWNS_MOUSE;

    return RETRO_DEVICE_TOWNS_GAMEPAD;
}

static void update_controller_devices(unsigned port)
{
    for (unsigned i = 0; i < MAX_PADS; i++)
    {
        unsigned device = resolve_controller_device(i);

        if ((i != port) && (input_device[i] == device))
            continue;

        if ((input_device[i] != device) && core)
            release_controller_input(i);

        if ((i == AUTO_MOUSE_PORT) && (requested_device[i] == RETRO_DEVICE_JOYPAD) &&
            (device == RETRO_DEVICE_TOWNS_GAMEPAD) && (input_device[i] != device))
            show_message("Port 2: Original gamepad, the mouse is in port 1", 1, RETRO_LOG_INFO);

        input_device[i] = device;
        apply_controller_device(i, device, true);
    }

    set_input_descriptors();
}

static void apply_controller_device(unsigned port, unsigned device, bool log_device)
{
    if (!core || port >= MAX_PADS)
        return;

    GT_Controller_Type type = GT_CONTROLLER_NONE;
    const char* name = "Unplugged";

    switch (device)
    {
        case RETRO_DEVICE_TOWNS_GAMEPAD:
            type = GT_CONTROLLER_ORIGINAL_GAMEPAD;
            name = "Original gamepad";
            break;
        case RETRO_DEVICE_TOWNS_MARTY:
            type = GT_CONTROLLER_MARTY_GAMEPAD;
            name = "Marty gamepad";
            break;
        case RETRO_DEVICE_TOWNS_6_BUTTON:
            type = GT_CONTROLLER_6_BUTTON_GAMEPAD;
            name = "6 button gamepad";
            break;
        case RETRO_DEVICE_TOWNS_MOUSE:
            type = GT_CONTROLLER_MOUSE;
            name = "Mouse";
            break;
        case RETRO_DEVICE_KEYBOARD:
        case RETRO_DEVICE_TOWNS_KEYBOARD:
            name = "Keyboard";
            break;
        case RETRO_DEVICE_NONE:
            break;
        default:
            name = "Unsupported device";
            break;
    }

    if (log_device && log_cb)
        log_cb(RETRO_LOG_INFO, "Controller %u: %s\n", port, name);

    core->GetInput()->SetControllerType((int)port, type);
}

static void release_controller_input(unsigned port)
{
    GT_GamePad_State state = { 0 };
    core->GetInput()->SetGamePadState((int)port, state);
    joypad_previous[port] = 0;

    if (input_device[port] == RETRO_DEVICE_TOWNS_MOUSE)
    {
        mouse_remainder_x = 0.0f;
        mouse_remainder_y = 0.0f;
        core->GetInput()->ClearMouseInput((int)port);
    }
}

static void poll_input(void)
{
    input_poll_cb();

    for (unsigned i = 0; i < RETROK_LAST; i++)
    {
        if (!retro_key_down[i] && libretro_supports_keyboard)
            continue;

        bool down = input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, i) != 0;

        if (down != retro_key_down[i])
            keyboard_event(down, i, 0, 0);
    }
}

static void apply_input(void)
{
    int mouse_port = get_mouse_port();

    for (int i = 0; i < MAX_PADS; i++)
    {
        if (i == mouse_port)
            apply_mouse(i);
        else
            apply_gamepad(i);
    }
}

static void apply_gamepad(int port)
{
    u16 buttons = 0;

    if (IsJoypadDevice(input_device[port]))
    {
        int bits = 0;

        if (libretro_supports_bitmasks)
            bits = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
        else
        {
            for (int i = 0; i <= RETRO_DEVICE_ID_JOYPAD_R; i++)
                bits |= input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
        }

        const u16* table = input_device[port] == RETRO_DEVICE_TOWNS_6_BUTTON ? k_6_button_joypad_buttons :
            k_joypad_buttons;

        for (int i = 0; i <= RETRO_DEVICE_ID_JOYPAD_R; i++)
        {
            if (bits & (1 << i))
                buttons |= table[i];
        }
    }

    if (!allow_up_down)
    {
        buttons = filter_opposing(buttons, joypad_previous[port], GT_GAMEPAD_UP, GT_GAMEPAD_DOWN);
        buttons = filter_opposing(buttons, joypad_previous[port], GT_GAMEPAD_LEFT, GT_GAMEPAD_RIGHT);
    }

    joypad_previous[port] = buttons;

    if (input_device[port] == RETRO_DEVICE_TOWNS_6_BUTTON)
        buttons &= k_6_button_buttons;
    else if (input_device[port] == RETRO_DEVICE_TOWNS_MARTY)
        buttons = (u16)((buttons & k_gamepad_buttons) | ((buttons & GT_GAMEPAD_Z) ? GT_GAMEPAD_ZOOM : 0));
    else
        buttons &= k_gamepad_buttons;

    GT_GamePad_State state = { buttons };
    core->GetInput()->SetGamePadState(port, state);
}

static u16 filter_opposing(u16 buttons, u16 previous, u16 first, u16 second)
{
    if ((buttons & (first | second)) != (first | second))
        return buttons;

    bool second_held = (previous & second) && !(previous & first);
    return (u16)(buttons & ~(second_held ? first : second));
}

static void apply_mouse(int port)
{
    float scale = (float)mouse_sensitivity / 6.0f;
    mouse_remainder_x += (float)input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X) * scale;
    mouse_remainder_y += (float)input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y) * scale;
    bool left = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) != 0;
    bool right = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT) != 0;

    if (analog_mouse > 0)
        poll_analog_mouse(left, right);

    int delta_x = (int)mouse_remainder_x;
    int delta_y = (int)mouse_remainder_y;
    mouse_remainder_x -= (float)delta_x;
    mouse_remainder_y -= (float)delta_y;
    core->GetInput()->SetMouseDelta(port, delta_x, delta_y);

    GT_GamePad_State state = { 0 };

    if (left)
        state.buttons |= GT_GAMEPAD_A;

    if (right)
        state.buttons |= GT_GAMEPAD_B;

    core->GetInput()->SetGamePadState(port, state);
}

static void poll_analog_mouse(bool& left, bool& right)
{
    unsigned index = analog_mouse == 1 ? RETRO_DEVICE_INDEX_ANALOG_LEFT : RETRO_DEVICE_INDEX_ANALOG_RIGHT;
    float x = (float)input_state_cb(0, RETRO_DEVICE_ANALOG, index, RETRO_DEVICE_ID_ANALOG_X) / 32767.0f;
    float y = (float)input_state_cb(0, RETRO_DEVICE_ANALOG, index, RETRO_DEVICE_ID_ANALOG_Y) / 32767.0f;
    float magnitude = sqrtf((x * x) + (y * y));
    const float deadzone = 0.15f;

    if (magnitude > deadzone)
    {
        float speed = (float)analog_mouse_speed * 1.6f;
        float scale = ((MIN(magnitude, 1.0f) - deadzone) / (1.0f - deadzone)) * speed / magnitude;
        mouse_remainder_x += x * scale;
        mouse_remainder_y += y * scale;
    }

    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2))
        left = true;

    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2))
        right = true;
}

static GT_Keys key_from_retro_key(unsigned keycode)
{
    switch (keycode)
    {
        case RETROK_a: return GT_KEY_A;
        case RETROK_b: return GT_KEY_B;
        case RETROK_c: return GT_KEY_C;
        case RETROK_d: return GT_KEY_D;
        case RETROK_e: return GT_KEY_E;
        case RETROK_f: return GT_KEY_F;
        case RETROK_g: return GT_KEY_G;
        case RETROK_h: return GT_KEY_H;
        case RETROK_i: return GT_KEY_I;
        case RETROK_j: return GT_KEY_J;
        case RETROK_k: return GT_KEY_K;
        case RETROK_l: return GT_KEY_L;
        case RETROK_m: return GT_KEY_M;
        case RETROK_n: return GT_KEY_N;
        case RETROK_o: return GT_KEY_O;
        case RETROK_p: return GT_KEY_P;
        case RETROK_q: return GT_KEY_Q;
        case RETROK_r: return GT_KEY_R;
        case RETROK_s: return GT_KEY_S;
        case RETROK_t: return GT_KEY_T;
        case RETROK_u: return GT_KEY_U;
        case RETROK_v: return GT_KEY_V;
        case RETROK_w: return GT_KEY_W;
        case RETROK_x: return GT_KEY_X;
        case RETROK_y: return GT_KEY_Y;
        case RETROK_z: return GT_KEY_Z;
        case RETROK_1: return GT_KEY_1;
        case RETROK_2: return GT_KEY_2;
        case RETROK_3: return GT_KEY_3;
        case RETROK_4: return GT_KEY_4;
        case RETROK_5: return GT_KEY_5;
        case RETROK_6: return GT_KEY_6;
        case RETROK_7: return GT_KEY_7;
        case RETROK_8: return GT_KEY_8;
        case RETROK_9: return GT_KEY_9;
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
        case RETROK_HASH: return GT_KEY_RIGHT_BRACKET;
        case RETROK_SEMICOLON: return GT_KEY_SEMICOLON;
        case RETROK_QUOTE: return GT_KEY_COLON;
        case RETROK_COLON: return GT_KEY_COLON;
        case RETROK_BACKQUOTE: return GT_KEY_ESCAPE;
        case RETROK_COMMA: return GT_KEY_COMMA;
        case RETROK_PERIOD: return GT_KEY_PERIOD;
        case RETROK_SLASH: return GT_KEY_SLASH;
        case RETROK_OEM_102: return GT_KEY_UNDERSCORE;
        case RETROK_UNDERSCORE: return GT_KEY_UNDERSCORE;
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
        case RETROK_RCTRL: return GT_KEY_KATAKANA;
        case RETROK_LSHIFT: return GT_KEY_SHIFT;
        case RETROK_RSHIFT: return GT_KEY_SHIFT;
        case RETROK_LALT: return GT_KEY_ALT;
        case RETROK_RALT: return GT_KEY_KANA_KANJI;
        case RETROK_LSUPER: return GT_KEY_NO_CONVERT;
        case RETROK_RSUPER: return GT_KEY_CONVERT;
        case RETROK_MENU: return GT_KEY_HIRAGANA;
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

static bool update_core_options_display(void)
{
    struct retro_variable var = { "geartowns_frameskip", NULL };
    bool threshold_visible = environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value &&
        (strcmp(var.value, "Manual") == 0);

    var.key = "geartowns_analog_mouse";
    var.value = NULL;
    bool speed_visible = environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value &&
        (strcmp(var.value, "Disabled") != 0);

    if ((threshold_visible == frameskip_threshold_visible) && (speed_visible == analog_mouse_speed_visible))
        return false;

    struct retro_core_option_display display = { "geartowns_frameskip_threshold", threshold_visible };
    environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &display);

    display.key = "geartowns_analog_mouse_speed";
    display.visible = speed_visible;
    environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &display);

    frameskip_threshold_visible = threshold_visible;
    analog_mouse_speed_visible = speed_visible;
    return true;
}

static void check_variables(void)
{
    struct retro_variable var = { };

    var.key = "geartowns_aspect_ratio";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        if (strcmp(var.value, "4:3 DAR") == 0)
            aspect_ratio = 4.0f / 3.0f;
        else if (strcmp(var.value, "16:9 DAR") == 0)
            aspect_ratio = 16.0f / 9.0f;
        else if (strcmp(var.value, "16:10 DAR") == 0)
            aspect_ratio = 16.0f / 10.0f;
        else
            aspect_ratio = 0.0f;
    }

    var.key = "geartowns_native_resolution";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        core->GetVideo()->EnableNativeResolution(strcmp(var.value, "Enabled") == 0);
    }

    var.key = "geartowns_frameskip";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        int type = strcmp(var.value, "Auto") == 0 ? 1 : strcmp(var.value, "Manual") == 0 ? 2 : 0;

        if (type != frameskip_type)
        {
            frameskip_type = type;
            frameskip_changed = true;
        }
    }

    var.key = "geartowns_frameskip_threshold";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        frameskip_threshold = (unsigned)CLAMP(atoi(var.value), 15, 60);
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

    var.key = "geartowns_analog_mouse";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        analog_mouse = strcmp(var.value, "Left stick") == 0 ? 1 : strcmp(var.value, "Right stick") == 0 ? 2 : 0;
    }

    var.key = "geartowns_analog_mouse_speed";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        analog_mouse_speed = CLAMP(atoi(var.value), 1, 10);
    }

    var.key = "geartowns_fm_volume";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        core->GetAudio()->SetFMVolume((float)CLAMP(atoi(var.value), 0, 200) / 100.0f);
    }

    var.key = "geartowns_pcm_volume";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        core->GetAudio()->SetPCMVolume((float)CLAMP(atoi(var.value), 0, 200) / 100.0f);
    }

    var.key = "geartowns_cdda_volume";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        core->GetAudio()->SetCDDAVolume((float)CLAMP(atoi(var.value), 0, 200) / 100.0f);
    }

    var.key = "geartowns_buzzer_volume";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        core->GetAudio()->SetBuzzerVolume((float)CLAMP(atoi(var.value), 0, 200) / 100.0f);
    }

    var.key = "geartowns_cdrom_speed";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        core->GetCDROM()->SetReadSpeed(CLAMP(atoi(var.value), 1, 8));
    }

    var.key = "geartowns_cdrom_preload";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        core->GetMedia()->PreloadCdRom(strcmp(var.value, "Enabled") == 0);
    }

    update_floppy_drive();

    var.key = "geartowns_floppy_multidrive";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        floppy_multidrive = (strcmp(var.value, "Enabled") == 0);
    }

    var.key = "geartowns_save_disk";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        save_disk = (strcmp(var.value, "Enabled") == 0);
    }

    var.key = "geartowns_ram_size";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        machine_ram_mb = CLAMP(atoi(var.value), 1, 6);
    }

    var.key = "geartowns_cpu_speed";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        machine_cpu_mhz = CLAMP(atoi(var.value), 16, 100);
    }

    var.key = "geartowns_rtc";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        core->GetRTC()->SetStartTime(strcmp(var.value, "Fixed date") == 0 ? RTC_FIXED_DATE : 0);
    }

    var.key = "geartowns_floppy_drives";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        machine_floppy_drives = CLAMP(atoi(var.value), 1, 2);
    }

    var.key = "geartowns_cmos";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        cmos_shared = (strcmp(var.value, "Shared") == 0);
    }

#if !defined(GT_DISABLE_DYNAREC)
    var.key = "geartowns_dynarec";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        int dynarec = strcmp(var.value, "Enabled") == 0 ? 1 : 0;

        if (dynarec != dynarec_option)
        {
            dynarec_option = dynarec;
            core->GetI386()->EnableDynarec(dynarec == 1);

            if (dynarec == 1 && !core->GetI386()->IsDynarecEnabled())
                log_cb(RETRO_LOG_WARN, "Dynarec unavailable, the CPU runs in the interpreter\n");
        }
    }
#endif
}

static void update_floppy_drive(void)
{
    struct retro_variable var = { "geartowns_floppy_drive", NULL };

    if (!environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || !var.value)
        return;

    int drive = strcmp(var.value, "Floppy 2") == 0 ? 1 : 0;

    if (drive != floppy_drive)
    {
        floppy_drive = drive;
        disk_sync();
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

static bool disk_load(const char* path)
{
    if (path[0] != 0 && !disk_add_images(path, disk_images))
        return false;

    if (save_disk)
    {
        std::string name = get_save_disk_name(get_file_name(retro_game_path, true)) + ".save.d77";

        if (!disk_add_save_disk(name, "Save Disk", NULL, disk_images))
            return false;
    }

    unsigned boot = 0;

    if (disk_initial_index < disk_images.size() && disk_images[disk_initial_index].cdrom &&
        disk_images[disk_initial_index].path == disk_initial_path)
        boot = disk_initial_index;

    disk_initial_index = 0;
    disk_initial_path.clear();

    if (disk_images.empty() || disk_images[boot].save_disk)
        return true;

    if (!disk_insert(boot, disk_images[boot].cdrom ? DISK_DRIVE_CDROM : DISK_DRIVE_FLOPPY_1))
        return false;

    for (size_t i = 0; i < disk_images.size(); i++)
    {
        if (disk_images[i].cdrom != disk_images[boot].cdrom && !disk_images[i].save_disk)
        {
            if (!disk_insert((unsigned)i, disk_images[i].cdrom ? DISK_DRIVE_CDROM : DISK_DRIVE_FLOPPY_1))
                return false;

            break;
        }
    }

    int boot_floppy = drive_entry[DISK_DRIVE_FLOPPY_1];
    bool multidrive = floppy_multidrive && boot_floppy >= 0 && core->GetPendingMachineConfig().floppy_drives > 1;

    for (size_t i = (size_t)(boot_floppy + 1); multidrive && i < disk_images.size(); i++)
    {
        if (!disk_images[i].cdrom && !disk_images[i].save_disk)
        {
            if (!disk_insert((unsigned)i, DISK_DRIVE_FLOPPY_2))
                log_cb(RETRO_LOG_WARN, "Floppy MultiDrive: %s is not inserted in Floppy 2\n",
                    disk_images[i].path.c_str());

            break;
        }
    }

    disk_index = boot;
    disk_ejected = false;
    return true;
}

static bool disk_add_images(const char* path, std::vector<DiskSetImage>& images)
{
    if (ends_with_no_case(path, ".m3u"))
        return disk_add_playlist(path, images);

    if (FloppyImage::IsImageName(path) || ends_with_no_case(path, ".zip"))
    {
        if (disk_add_floppy_images(path, images))
            return true;

        if (ends_with_no_case(path, ".zip"))
            show_message("This ZIP file has no single floppy disk image, zipped CD-ROM images are not supported, "
                "use CHD instead", 3, RETRO_LOG_ERROR);

        log_cb(RETRO_LOG_ERROR, "Unable to read the floppy disk image %s\n", path);
        return false;
    }

    if (!path_is_cd_content(path))
    {
        log_cb(RETRO_LOG_ERROR, "Not a CD-ROM (CUE, CHD, ISO) or floppy disk image: %s\n", path);
        return false;
    }

    if (!path_is_cdrom_uri(path))
    {
        MediaFile* file = MediaFile::OpenFile(path);

        if (!IsValidPointer(file))
            return false;

        SafeDelete(file);
    }

    DiskSetImage image;
    image.path = path;
    image.label = get_file_name(path, false);
    image.disk = 0;
    image.crc = 0;
    image.crc_known = false;
    image.cdrom = true;
    image.save_disk = false;
    images.push_back(image);
    return true;
}

static bool disk_add_playlist(const char* path, std::vector<DiskSetImage>& images)
{
    std::vector<u8> data;
    std::vector<Media::PlaylistEntry> entries;

    if (!read_content_file(path, data, PLAYLIST_MAX_SIZE) ||
        !Media::ParsePlaylist(path, reinterpret_cast<const char*>(data.data()), data.size(), entries))
        return false;

    std::string save_name = get_save_disk_name(get_file_name(path, true));
    int save_disks = 0;

    for (size_t i = 0; i < entries.size(); i++)
    {
        const Media::PlaylistEntry& entry = entries[i];

        if (entry.save_disk)
        {
            char suffix[32];
            char label[32];
            snprintf(suffix, sizeof(suffix), ".save%d.d77", save_disks);
            snprintf(label, sizeof(label), "Save Disk %d", save_disks);

            if (!disk_add_save_disk(save_name + suffix, label, entry.label.c_str(), images))
                return false;

            save_disks++;
            continue;
        }

        size_t first = images.size();

        if (ends_with_no_case(entry.path.c_str(), ".m3u") || !disk_add_images(entry.path.c_str(), images))
        {
            log_cb(RETRO_LOG_ERROR, "Playlist entry is missing or not a disk image: %s\n", entry.path.c_str());
            return false;
        }

        size_t name_length = get_file_name(entry.path.c_str(), false).length();

        for (size_t j = first; entry.has_label && j < images.size(); j++)
            images[j].label = entry.label.empty() ? "" : entry.label + images[j].label.substr(name_length);
    }

    return true;
}

static bool disk_add_floppy_images(const char* path, std::vector<DiskSetImage>& images)
{
    std::vector<u8> data;

    if (!load_source(path, data))
        return false;

    int count = FloppyImage::GetDiskCount(data.data(), (u32)data.size());
    std::string file_name = get_file_name(path, false);

    for (int i = 0; i < count; i++)
    {
        u32 offset = 0;
        u32 size = 0;

        if (!FloppyImage::GetDisk(data.data(), (u32)data.size(), i, &offset, &size))
            return false;

        DiskSetImage image;
        image.path = path;
        image.label = file_name;
        image.disk = i;
        image.crc = CalculateCRC32(0, data.data() + offset, (int)size);
        image.crc_known = true;
        image.cdrom = false;
        image.save_disk = false;

        if (count > 1)
        {
            char name[32];
            char suffix[64];
            FloppyImage::GetDiskName(data.data() + offset, size, name, sizeof(name));
            snprintf(suffix, sizeof(suffix), name[0] ? " (Disk %d: %s)" : " (Disk %d)", i + 1, name);
            image.label += suffix;
        }

        images.push_back(image);
    }

    return count > 0;
}

static bool disk_add_save_disk(const std::string& name, const char* label, const char* volume,
    std::vector<DiskSetImage>& images)
{
    std::string path(retro_save_directory);
    append_path_component(path, name.c_str());
    MediaFile* file = MediaFile::OpenFile(path.c_str());
    bool exists = IsValidPointer(file);
    SafeDelete(file);

    if (!exists)
    {
        u32 size = 0;
        u8* blank = FloppyImage::CreateBlank(FLOPPY_BLANK_2HD_1232, true, &size);

        if (IsValidPointer(blank))
            FloppyImage::SetDiskName(blank, size, volume);

        bool created = IsValidPointer(blank) && write_content_file(path.c_str(), blank, size);
        SafeDeleteArray(blank);

        if (!created)
        {
            log_cb(RETRO_LOG_ERROR, "Unable to create the save disk %s\n", path.c_str());
            return false;
        }

        log_cb(RETRO_LOG_INFO, "Save disk created: %s\n", path.c_str());
    }

    DiskSetImage image;
    image.path = path;
    image.label = label;
    image.disk = 0;
    image.crc = CalculateCRC32(0, reinterpret_cast<const u8*>(name.c_str()), (int)name.size());
    image.crc_known = true;
    image.cdrom = false;
    image.save_disk = true;
    images.push_back(image);
    return true;
}

static int get_entry_drive(unsigned index)
{
    if (index < disk_images.size() && disk_images[index].cdrom)
        return DISK_DRIVE_CDROM;

    return DISK_DRIVE_FLOPPY_1 + (core->GetPendingMachineConfig().floppy_drives > 1 ? floppy_drive : 0);
}

static bool disk_insert(unsigned index, int drive)
{
    if (index >= disk_images.size() || disk_images[index].path.empty())
        return false;

    DiskSetImage& image = disk_images[index];

    if (drive == DISK_DRIVE_CDROM)
    {
        bool loaded = core->LoadMedia(image.path.c_str());
        drive_entry[DISK_DRIVE_CDROM] = loaded ? (int)index : -1;

        if (loaded)
        {
            image.crc = core->GetMedia()->GetCRC();
            image.crc_known = true;
        }

        return loaded;
    }

    for (int i = DISK_DRIVE_FLOPPY_1; i < DISK_DRIVE_COUNT; i++)
    {
        if (i != drive && drive_entry[i] >= 0 && disk_images[drive_entry[i]].path == image.path &&
            disk_images[drive_entry[i]].disk == image.disk)
        {
            static char message[64];
            snprintf(message, sizeof(message), "This floppy disk is already in Floppy %d", i);
            show_message(message, 2, RETRO_LOG_WARN);
            return false;
        }
    }

    int floppy = drive - DISK_DRIVE_FLOPPY_1;

    if (!floppy_flush(floppy))
        return false;

    if (floppy_load_image(index, floppy))
        return true;

    if (!core->GetFloppy(floppy)->IsInserted())
    {
        drive_entry[drive] = -1;
        floppy_working_path[floppy].clear();
    }

    return false;
}

static bool disk_eject(int drive)
{
    if (drive == DISK_DRIVE_CDROM)
        core->EjectMedia();
    else
    {
        int floppy = drive - DISK_DRIVE_FLOPPY_1;

        if (!floppy_flush(floppy))
        {
            show_message("Unable to save the floppy disk changes, the disk stays in the drive", 3, RETRO_LOG_ERROR);
            return false;
        }

        core->EjectFloppy(floppy);
        floppy_working_path[floppy].clear();
    }

    drive_entry[drive] = -1;
    return true;
}

static void disk_close_open_drive(int drive)
{
    if (open_drive < 0 || open_drive == drive)
        return;

    if (open_entry >= 0 && drive_entry[open_drive] < 0)
        disk_insert((unsigned)open_entry, open_drive);

    open_drive = -1;
    open_entry = -1;
}

static void disk_sync(void)
{
    if (disk_ejected || disk_index >= disk_images.size())
        return;

    int drive = get_entry_drive(disk_index);

    if (drive_entry[drive] >= 0)
    {
        disk_index = (unsigned)drive_entry[drive];
        return;
    }

    disk_ejected = true;
    open_drive = drive;
    open_entry = -1;
}

static void disk_clear(void)
{
    disk_images.clear();
    disk_index = 0;
    disk_ejected = true;
    open_drive = -1;
    open_entry = -1;

    for (int i = 0; i < DISK_DRIVE_COUNT; i++)
        drive_entry[i] = -1;

    for (int i = 0; i < FDC_DRIVES; i++)
    {
        floppy_working_path[i].clear();
        floppy_idle_frames[i] = 0;
        floppy_flush_failed[i] = false;
    }
}

static void disk_shift_entry(int& entry, unsigned index, int delta)
{
    if (entry < (int)index)
        return;

    entry = (delta < 0 && entry == (int)index) ? -1 : entry + delta;
}

static bool mount_state_media(const GT_SaveState_Media& media)
{
    int entries[DISK_DRIVE_COUNT];
    bool changed = false;

    for (int i = 0; i < DISK_DRIVE_COUNT; i++)
        entries[i] = drive_entry[i];

    if (media.cdrom_inserted && (!core->GetMedia()->IsReady() || core->GetMedia()->GetCRC() != media.cdrom_crc))
    {
        entries[DISK_DRIVE_CDROM] = find_cdrom_entry(media.cdrom_crc);
        changed = true;

        if (entries[DISK_DRIVE_CDROM] < 0)
            return false;
    }

    for (int i = 0; i < FDC_DRIVES; i++)
    {
        FloppyDisk* disk = core->GetFloppy(i);

        if (!media.floppy_inserted[i] || (disk->IsInserted() && disk->GetBaseCRC() == media.floppy_crc[i]))
            continue;

        entries[DISK_DRIVE_FLOPPY_1 + i] = find_floppy_entry(media.floppy_crc[i]);
        changed = true;

        if (entries[DISK_DRIVE_FLOPPY_1 + i] < 0)
            return false;
    }

    if (!changed)
        return false;

    for (int i = DISK_DRIVE_FLOPPY_1; i < DISK_DRIVE_COUNT; i++)
    {
        if (entries[i] != drive_entry[i] && drive_entry[i] >= 0 && !disk_eject(i))
            return false;
    }

    for (int i = 0; i < DISK_DRIVE_COUNT; i++)
    {
        if (entries[i] != drive_entry[i] && !disk_insert((unsigned)entries[i], i))
            return false;
    }

    if (disk_ejected && disk_index < disk_images.size() && drive_entry[get_entry_drive(disk_index)] >= 0)
    {
        disk_ejected = false;
        open_drive = -1;
        open_entry = -1;
    }

    log_cb(RETRO_LOG_INFO, "Disks changed to match the save state\n");
    return true;
}

static int find_floppy_entry(u32 crc)
{
    for (size_t i = 0; i < disk_images.size(); i++)
    {
        if (!disk_images[i].cdrom && disk_images[i].crc_known && disk_images[i].crc == crc)
            return (int)i;
    }

    return -1;
}

static int find_cdrom_entry(u32 crc)
{
    for (size_t i = 0; i < disk_images.size(); i++)
    {
        DiskSetImage& image = disk_images[i];

        if (!image.cdrom)
            continue;

        if (!image.crc_known)
            image.crc_known = get_cdrom_crc(image.path.c_str(), &image.crc);

        if (image.crc_known && image.crc == crc)
            return (int)i;
    }

    return -1;
}

static bool get_cdrom_crc(const char* path, u32* crc)
{
    CdRomMedia media;
    media.Init();
    bool loaded = false;

    if (ends_with_no_case(path, ".chd"))
        loaded = media.LoadChdFromFile(path, false);
    else if (ends_with_no_case(path, ".iso"))
        loaded = media.LoadIsoFromFile(path, false);
    else
        loaded = media.LoadCueFromFile(path, false);

    if (loaded)
        *crc = media.GetCRC();

    return loaded;
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
    update_floppy_drive();

    if (disk_ejected == ejected)
        return true;

    if (ejected)
    {
        if (disk_index < disk_images.size())
        {
            int drive = get_entry_drive(disk_index);
            int entry = drive_entry[drive];

            if (entry >= 0 && !disk_eject(drive))
                return false;

            open_drive = drive;
            open_entry = entry;
        }

        disk_ejected = true;
        return true;
    }

    if (disk_index < disk_images.size())
    {
        int drive = get_entry_drive(disk_index);
        disk_close_open_drive(drive);

        if (!disk_insert(disk_index, drive))
            return false;
    }

    open_drive = -1;
    open_entry = -1;
    disk_ejected = false;
    return true;
}

static bool disk_get_eject_state(void)
{
    update_floppy_drive();
    return disk_ejected;
}

static unsigned disk_get_image_index(void)
{
    update_floppy_drive();
    return disk_index;
}

static bool disk_set_image_index(unsigned index)
{
    update_floppy_drive();

    if (!disk_ejected || index > disk_images.size())
        return false;

    if (index < disk_images.size())
        disk_close_open_drive(get_entry_drive(index));

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

    for (int i = 0; i < DISK_DRIVE_COUNT; i++)
    {
        if (drive_entry[i] == (int)index)
            return false;
    }

    if (!info || !info->path)
    {
        disk_images.erase(disk_images.begin() + index);

        for (int i = 0; i < DISK_DRIVE_COUNT; i++)
            disk_shift_entry(drive_entry[i], index, -1);

        disk_shift_entry(open_entry, index, -1);

        if (disk_index > index || (disk_index >= disk_images.size() && disk_index > 0))
            disk_index--;

        return true;
    }

    std::vector<DiskSetImage> images;

    if (!disk_add_images(info->path, images) || images.empty())
        return false;

    if (open_entry == (int)index)
        open_entry = -1;

    int added = (int)images.size() - 1;
    disk_images[index] = images[0];
    disk_images.insert(disk_images.begin() + index + 1, images.begin() + 1, images.end());

    for (int i = 0; i < DISK_DRIVE_COUNT; i++)
        disk_shift_entry(drive_entry[i], index + 1, added);

    disk_shift_entry(open_entry, index + 1, added);

    if (disk_index > index)
        disk_index += added;

    return true;
}

static bool disk_add_image_index(void)
{
    if (!disk_ejected)
        return false;

    DiskSetImage image;
    image.disk = 0;
    image.crc = 0;
    image.crc_known = false;
    image.cdrom = false;
    image.save_disk = false;
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

    snprintf(label, len, "%s", disk_images[index].label.c_str());
    return true;
}

static bool read_content_file(const char* path, std::vector<u8>& data, s64 max_size)
{
    MediaFile* file = MediaFile::OpenFile(path);

    if (!IsValidPointer(file))
        return false;

    s64 size = file->GetSize();
    bool ok = size > 0 && size <= max_size;

    if (ok)
    {
        data.resize((size_t)size);
        ok = file->ReadExact(data.data(), (u64)size);
    }

    SafeDelete(file);
    return ok;
}

static bool load_source(const char* path, std::vector<u8>& data)
{
    if (!read_content_file(path, data, CONTENT_MAX_SIZE))
        return false;

    if (!ends_with_no_case(path, ".zip"))
        return true;

    u8* extracted = NULL;
    u32 size = 0;

    if (!FloppyImage::ExtractFromZip(data.data(), data.size(), &extracted, &size, NULL, 0))
        return false;

    data.assign(extracted, extracted + size);
    SafeDeleteArray(extracted);
    return true;
}

static bool write_content_file(const char* path, const u8* data, u32 size)
{
    std::string temporary = std::string(path) + ".tmp";
    std::string backup = std::string(path) + ".bak";
    bool vfs = IsValidPointer(vfs_interface) && IsValidPointer(vfs_interface->rename) &&
        IsValidPointer(vfs_interface->remove);
    bool written = false;

    if (vfs)
    {
        LibretroVfsFile file(vfs_interface);

        if (file.Open(temporary.c_str(), RETRO_VFS_FILE_ACCESS_WRITE))
        {
            written = file.WriteAll(data, size);
            written = file.Close() && written;
        }
    }
    else
    {
        FILE* file = fopen(temporary.c_str(), "wb");

        if (IsValidPointer(file))
        {
            written = fwrite(data, 1, size, file) == size;
            written = (fclose(file) == 0) && written;
        }
    }

    if (!written)
    {
        remove_file(temporary.c_str(), vfs);
        return false;
    }

    remove_file(backup.c_str(), vfs);
    bool backed_up = rename_file(path, backup.c_str(), vfs);

    if (!rename_file(temporary.c_str(), path, vfs))
    {
        if (backed_up)
            rename_file(backup.c_str(), path, vfs);

        remove_file(temporary.c_str(), vfs);
        return false;
    }

    remove_file(backup.c_str(), vfs);
    return true;
}

static bool rename_file(const char* from, const char* to, bool vfs)
{
    return vfs ? vfs_interface->rename(from, to) == 0 : rename(from, to) == 0;
}

static void remove_file(const char* path, bool vfs)
{
    if (vfs)
        vfs_interface->remove(path);
    else
        remove(path);
}

static bool floppy_load_image(unsigned index, int floppy)
{
    const DiskSetImage& image = disk_images[index];
    std::vector<u8> file;
    std::vector<u8> copy;
    std::string working(image.path);
    const u8* data = NULL;
    u32 size = 0;

    if (image.save_disk)
    {
        if (!read_content_file(image.path.c_str(), file, CONTENT_MAX_SIZE))
            return false;

        data = file.data();
        size = (u32)file.size();
    }
    else
    {
        u32 offset = 0;

        if (!load_source(image.path.c_str(), file) ||
            !FloppyImage::GetDisk(file.data(), (u32)file.size(), image.disk, &offset, &size))
            return false;

        char suffix[64];

        if (FloppyImage::HasEarlierCopy(file.data(), (u32)file.size(), image.disk))
            snprintf(suffix, sizeof(suffix), ".%08X.%d.geartowns.d77", image.crc, image.disk + 1);
        else
            snprintf(suffix, sizeof(suffix), ".%08X.geartowns.d77", image.crc);

        working = retro_save_directory;
        append_path_component(working, (get_file_name(image.path.c_str(), true) + suffix).c_str());
        data = file.data() + offset;

        if (read_content_file(working.c_str(), copy, CONTENT_MAX_SIZE) &&
            FloppyImage::IsD77(copy.data(), (u32)copy.size()))
        {
            log_cb(RETRO_LOG_INFO, "Loading floppy working copy: %s\n", working.c_str());
            data = copy.data();
            size = (u32)copy.size();
        }
    }

    if (!core->InsertFloppy(floppy, data, size, false, image.crc))
        return false;

    floppy_working_path[floppy] = working;
    floppy_idle_frames[floppy] = 0;
    floppy_flush_failed[floppy] = false;
    drive_entry[DISK_DRIVE_FLOPPY_1 + floppy] = (int)index;
    log_cb(RETRO_LOG_INFO, "Floppy %d: %s\n", floppy + 1, image.path.c_str());
    return true;
}

static bool floppy_flush(int floppy)
{
    FloppyDisk* disk = core->GetFloppy(floppy);

    if (!disk->IsInserted() || !disk->IsDirty())
        return true;

    if (floppy_working_path[floppy].empty() ||
        !write_content_file(floppy_working_path[floppy].c_str(), disk->GetImage(), disk->GetImageSize()))
        return false;

    disk->ClearDirty();
    log_cb(RETRO_LOG_INFO, "Floppy changes saved to %s\n", floppy_working_path[floppy].c_str());
    return true;
}

static void floppy_update(void)
{
    for (int i = 0; i < FDC_DRIVES; i++)
    {
        FloppyDisk* disk = core->GetFloppy(i);

        if (!disk->IsInserted() || !disk->IsDirty())
            continue;

        if (disk->GetWriteCount() != floppy_write_count[i])
        {
            floppy_write_count[i] = disk->GetWriteCount();
            floppy_idle_frames[i] = 0;
            continue;
        }

        if (++floppy_idle_frames[i] != FLOPPY_IDLE_FRAMES)
            continue;

        bool saved = floppy_flush(i);

        if (!saved && !floppy_flush_failed[i])
        {
            log_cb(RETRO_LOG_ERROR, "Unable to save the floppy %d changes\n", i + 1);
            show_message("Unable to save the floppy disk changes", 3, RETRO_LOG_ERROR);
        }

        if (!saved)
            floppy_idle_frames[i] = 0;

        floppy_flush_failed[i] = !saved;
    }
}

static void cmos_load(void)
{
    Memory* memory = core->GetMemory();
    std::vector<u8> data;
    std::string name(k_machine_profiles[core->GetPendingMachineConfig().model].short_name);

    memory->ResetCMOS();
    cmos_shared_active = cmos_shared;
    cmos_frames = 0;
    cmos_save_failed = false;
    cmos_path = retro_save_directory;
    append_path_component(cmos_path, (name + ".geartowns.cmos").c_str());

    if (cmos_shared_active && read_content_file(cmos_path.c_str(), data, CONTENT_MAX_SIZE))
    {
        if (data.size() == GT_CMOS_SIZE)
        {
            memcpy(memory->GetCMOS(), data.data(), GT_CMOS_SIZE);
            log_cb(RETRO_LOG_INFO, "CMOS loaded from %s\n", cmos_path.c_str());
        }
        else
        {
            log_cb(RETRO_LOG_ERROR, "Invalid CMOS file: %s\n", cmos_path.c_str());
        }
    }

    memcpy(cmos_saved, memory->GetCMOS(), GT_CMOS_SIZE);
}

static void cmos_save(void)
{
    const u8* cmos = core->GetMemory()->GetCMOS();

    if (!cmos_shared_active || memcmp(cmos, cmos_saved, GT_CMOS_SIZE) == 0)
        return;

    bool saved = write_content_file(cmos_path.c_str(), cmos, GT_CMOS_SIZE);

    if (saved)
    {
        memcpy(cmos_saved, cmos, GT_CMOS_SIZE);
        log_cb(RETRO_LOG_INFO, "CMOS saved to %s\n", cmos_path.c_str());
    }
    else if (!cmos_save_failed)
    {
        log_cb(RETRO_LOG_ERROR, "Unable to save the CMOS to %s\n", cmos_path.c_str());
        show_message("Unable to save the CMOS", 3, RETRO_LOG_ERROR);
    }

    cmos_save_failed = !saved;
}

static void cmos_update(void)
{
    if (++cmos_frames < CMOS_SAVE_FRAMES)
        return;

    cmos_frames = 0;
    cmos_save();
}
