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

#define GUI_MENUS_IMPORT
#include "gui_menus.h"
#include "gui.h"
#include "gui_filedialogs.h"
#include "gui_popups.h"
#include "gui_actions.h"
#include "gui_colors.h"
#include "gui_floppy.h"
#include "gui_game.h"
#include "gui_firmware.h"
#include "gui_hard_disks.h"
#include "gui_memory_card.h"
#include "gui_notifications.h"
#include "gui_shader.h"
#include "config.h"
#include "application.h"
#include "display.h"
#include "gamepad.h"
#include "sound_queue.h"
#include "emu.h"
#include "emu_game.h"
#include "emu_midi.h"
#include "emu_synth.h"
#include "midi_port.h"
#include "ogl_renderer.h"
#include "ogl_shader_chain.h"
#include "shader_preset.h"
#include "utils.h"
#include "geartowns.h"
#include "drive/hard_disk_image.h"
#include "drive/memory_card.h"
#include "system/system_control.h"
#include "video/video.h"
#include "rewind.h"
#include "events.h"
#include "game_focus.h"
#include "debug/gui_debug_i386.h"
#include "hotkeys.h"
#include "input_macro.h"

static bool open_rom = false;
static bool open_state = false;
static bool save_state = false;
static bool open_about = false;
static bool open_load_defaults = false;
static bool save_screenshot = false;
static bool save_vgm = false;
static bool save_video = false;
static bool choose_savestates_path = false;
static bool choose_savefiles_path = false;
static bool choose_mt32_path = false;
static bool choose_soundfont = false;
static std::vector<MidiPortInfo> midi_ports;
static bool midi_ports_pending = false;
static const GuiColor& midi_playing_color = green;
static const GuiColor& midi_stopped_color = red;
static bool choose_screenshots_path = false;
static bool choose_video_recordings_path = false;
#if defined(GT_ENABLE_PHYSICAL_CDROM)
static bool open_physical_cdrom = false;
#endif
static const GuiColor& machine_warning_color = amber;
static const GuiColor& service_mcp_http_color = green;
static const GuiColor& service_mcp_stdio_color = amber;
static const GuiColor& video_recording_color = red;
static const GuiColor& vgm_recording_color = red;
static const GuiColor& rewind_recording_color = green;
static const GuiColor& rewind_stopped_color = red;

static void menu_geartowns(void);
static void menu_rewind(void);
static void menu_drives(void);
static void menu_cdrom(void);
static void menu_emulator(void);
static void menu_machine(void);
static void menu_machine_model(void);
static void menu_machine_cpu_speed(const GT_Machine_Profile& profile);
static void menu_machine_ram(const GT_Machine_Profile& profile);
static void menu_machine_fpu(const GT_Machine_Profile& profile);
static void menu_machine_speed_mode(const GT_Machine_Profile& profile);
static void menu_machine_midi_card(const GT_Machine_Profile& profile);
static const char* get_machine_fpu_text(const GT_Machine_Profile& profile);
static void menu_machine_hardware(const GT_Machine_Config& config);
static void get_machine_hardware(const GT_Machine_Config& config, char* text, size_t text_size);
static void add_machine_line(char* text, size_t text_size, const char* format, ...);
static void menu_boot_device(void);
static void menu_default_boot_device(void);
static void draw_machine_profile_tooltip(GT_Machine_Model model);
static void get_machine_summary(const GT_Machine_Config& config, char* text, size_t text_size);
static void menu_video(void);
static void menu_input(void);
static void menu_input_controller(int port);
static void menu_audio(void);
static void menu_audio_source(const char* label, const char* id, int source, float* volume);
static void menu_audio_synth_volume(const char* label, const char* id, int engine, float* volume);
static bool volume_slider(const char* id, float* volume);
static void menu_audio_midi(void);
static void menu_audio_midi_out(const char* label, int synth, int* out, std::string* port);
static void menu_audio_midi_port(const char* id, std::string* port);
static void menu_audio_midi_game_music(void);
static void menu_audio_synths(void);
static void menu_audio_synth_mt32(void);
static void menu_audio_synth_mt32_model(void);
static void menu_audio_synth_gm(void);
static void draw_synth_location_tooltip(int engine, int option, const char* custom, const char* about);
static const char* get_builtin_label(int engine);
static int find_midi_port(const char* port);
static void update_midi_ports(void);
static void menu_debug(void);
static void menu_about(void);
static void draw_background_color_menu(const char* label, int theme);
static void draw_mcp_status(void);
static void draw_recording_status(const char* status, ImVec4 color, float dot_radius, float dot_width);
static void file_dialogs(void);
static bool media_menu_actions_enabled(void);
static const char* get_current_media_directory_text(void);
static void keyboard_configuration_item(const char* text, SDL_Scancode* key, int player);
static void gamepad_configuration_item(const char* text, int* button, int player);
static void hotkey_configuration_item(const char* text, config_Hotkey* hotkey);
static void hotkey_configuration_menu(bool debug);
static void gamepad_device_selector(int player);
static void draw_savestate_slot_info(int slot);

void gui_init_menus(void)
{
    gui_shortcut_open_rom = false;
}

void gui_main_menu(void)
{
    open_rom = false;
    open_state = false;
    save_state = false;
    open_about = false;
    open_load_defaults = false;
    save_screenshot = false;
    save_vgm = false;
    save_video = false;
    choose_savestates_path = false;
    choose_savefiles_path = false;
    choose_mt32_path = false;
    choose_soundfont = false;
    choose_screenshots_path = false;
    choose_video_recordings_path = false;
#if defined(GT_ENABLE_PHYSICAL_CDROM)
    open_physical_cdrom = false;
#endif

    if (application_show_menu && ImGui::BeginMainMenuBar())
    {
        menu_geartowns();
        menu_drives();
        menu_emulator();
        menu_video();
        menu_input();
        menu_audio();
        menu_debug();
        menu_about();
        draw_mcp_status();

        gui_main_menu_height = (int)ImGui::GetWindowSize().y;

        ImGui::EndMainMenuBar();
    }

    file_dialogs();
}

static void menu_geartowns(void)
{
    if (ImGui::BeginMenu(GT_TITLE))
    {
        gui_in_use = true;
        GeartownsCore* core = emu_get_core();
        bool media_actions_enabled = media_menu_actions_enabled();
        bool loading = gui_is_rom_loading() || emu_is_media_loading();
        bool powered_on = !emu_is_empty();
        bool firmware_ready = core->GetFirmware()->IsReady();

        if (ImGui::MenuItem("Power On", config_hotkeys[config_HotkeyIndex_PowerOn].str, false,
            !loading && !powered_on && firmware_ready))
        {
            gui_action_power_on();
        }

        if (ImGui::MenuItem("Power Off", config_hotkeys[config_HotkeyIndex_PowerOff].str, false, !loading && powered_on))
        {
            gui_action_power_off();
        }

        if (ImGui::MenuItem("Reset", config_hotkeys[config_HotkeyIndex_Reset].str, false, media_actions_enabled))
        {
            gui_action_reset();
        }

        if (ImGui::BeginMenu("Reset and Boot From", media_actions_enabled))
        {
            menu_boot_device();
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Pause", config_hotkeys[config_HotkeyIndex_Pause].str, &config_emulator.paused, media_actions_enabled))
        {
            gui_action_pause();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Fast Forward"))
        {
            if (ImGui::MenuItem("Enabled", config_hotkeys[config_HotkeyIndex_FFWD].str, &config_emulator.ffwd, media_actions_enabled))
            {
                gui_action_ffwd();
            }

            if (ImGui::BeginMenu("Speed"))
            {
                ImGui::PushItemWidth(100.0f);
                ImGui::Combo("##fwd", &config_emulator.ffwd_speed, "X 1.5\0X 2\0X 2.5\0X 3\0Unlimited\0\0");
                ImGui::PopItemWidth();
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Rewind"))
        {
            menu_rewind();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Run-Ahead"))
        {
            ImGui::PushItemWidth(140.0f);
            ImGui::Combo("##runahead", &config_emulator.runahead, "Disabled\0" "1 Frame\0" "2 Frames\0" "3 Frames\0\0");
            ImGui::PopItemWidth();

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Reduces input lag by speculatively running extra frames each update.");
                ImGui::Text("Every frame multiplies CPU cost, so use the lowest value that feels right.");
                ImGui::Text("Ignored while fast-forwarding or using the debugger.");
                ImGui::EndTooltip();
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Savestates"))
        {
            if (ImGui::MenuItem("Save As...", "", false, media_actions_enabled))
                save_state = true;

            if (ImGui::MenuItem("Load From...", "", false, media_actions_enabled))
                open_state = true;

            ImGui::Separator();

            if (ImGui::BeginMenu("Slot"))
            {
                ImGui::PushItemWidth(100.0f);
                ImGui::Combo("##slot", &config_emulator.save_slot, "Slot 1\0Slot 2\0Slot 3\0Slot 4\0Slot 5\0\0");
                ImGui::PopItemWidth();

                ImGui::Separator();
                draw_savestate_slot_info(config_emulator.save_slot);
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Save", config_hotkeys[config_HotkeyIndex_SaveState].str, false, media_actions_enabled))
            {
                gui_action_save_state(NULL);
            }

            if (ImGui::MenuItem("Load", config_hotkeys[config_HotkeyIndex_LoadState].str, false, media_actions_enabled))
            {
                gui_action_load_state(NULL);
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Slot: %d", config_emulator.save_slot + 1);
                ImGui::Separator();
                draw_savestate_slot_info(config_emulator.save_slot);
                ImGui::EndTooltip();
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Screenshots"))
        {
            if (ImGui::MenuItem("Save As...", "", false, media_actions_enabled))
            {
                save_screenshot = true;
            }

            if (ImGui::MenuItem("Save", config_hotkeys[config_HotkeyIndex_Screenshot].str, false, media_actions_enabled))
            {
                gui_action_save_screenshot(NULL);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Video Recording"))
        {
            bool is_recording = emu_is_video_recording();

            if (ImGui::MenuItem("Start Recording As...", "", false, !is_recording && media_actions_enabled))
            {
                save_video = true;
            }

            if (ImGui::MenuItem("Start Recording", config_hotkeys[config_HotkeyIndex_VideoRecording].str, false,
                !is_recording && media_actions_enabled))
            {
                gui_action_start_video_recording(NULL);
            }

            if (ImGui::MenuItem("Stop Recording", config_hotkeys[config_HotkeyIndex_VideoRecording].str, false,
                is_recording))
            {
                gui_action_stop_video_recording();
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Scale", !is_recording))
            {
                ImGui::PushItemWidth(100.0f);
                ImGui::SliderInt("##video_recording_scale", &config_video.recording_scale, 1, 20, "%dx");
                ImGui::PopItemWidth();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Aspect Ratio", !is_recording))
            {
                ImGui::PushItemWidth(190.0f);
                ImGui::Combo("##video_recording_ratio", &config_video.recording_ratio,
                    "Follow Screen\0Square Pixels (1:1 PAR)\0Standard (4:3 DAR)\0Wide (16:9 DAR)\0Wide (16:10 DAR)\0\0");
                ImGui::PopItemWidth();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Quality", !is_recording))
            {
                ImGui::PushItemWidth(200.0f);
                ImGui::Combo("##video_recording_quality", &config_video.recording_quality,
                    "Low\0Medium\0High\0Lossless (Uncompressed)\0Lossless (Compressed)\0\0");
                ImGui::PopItemWidth();
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("Low and Medium halve the color resolution, best used at 2x or higher.");
                    ImGui::Text("Lossless (Uncompressed) produces very large files.");
                    ImGui::Text("Lossless (Compressed) stores PNG frames, it needs an FFmpeg based player like VLC or mpv.");
                    ImGui::EndTooltip();
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Load Default Settings", NULL, false, !gui_is_rom_loading() && !emu_is_media_loading()))
        {
            open_load_defaults = true;
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Quit", config_hotkeys[config_HotkeyIndex_Quit].str))
        {
            application_trigger_quit();
        }

        ImGui::EndMenu();
    }
}

static void menu_rewind(void)
{
    if (ImGui::MenuItem("Enabled", NULL, &config_rewind.enabled))
    {
        rewind_reset();
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        if (config_hotkeys[config_HotkeyIndex_Rewind].str[0] != 0)
            ImGui::Text("Hold %s to rewind.", config_hotkeys[config_HotkeyIndex_Rewind].str);
        ImGui::Text("Rewind uses save states while it runs, which costs performance.");
        ImGui::Text("If you don't use rewind, keep it disabled.");
        ImGui::EndTooltip();
    }

    int max_steps = REWIND_MEMORY_MAX_MB / REWIND_MEMORY_STEP_MB;
    int steps = CLAMP(config_rewind.memory_mb / REWIND_MEMORY_STEP_MB, 1, max_steps);
    char memory_format[32];
    snprintf(memory_format, sizeof(memory_format), "%d MB", steps * REWIND_MEMORY_STEP_MB);
    const char* rate_format = (config_rewind.frames_per_snapshot == 1) ? "Every frame" : "Every %d frames";
    ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput;

    ImGui::PushItemWidth(ImGui::CalcTextSize("Every 60 frames").x + (ImGui::GetStyle().FramePadding.x * 4.0f));
    ImGui::SliderFloat("Speed", &config_rewind.speed, 1.0f, 8.0f, "%.0fx");

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("How fast time goes back while rewinding.");
        ImGui::Text("At 2x, each second held rewinds two seconds of play.");
        ImGui::EndTooltip();
    }

    ImGui::SliderInt("Rate", &config_rewind.frames_per_snapshot, 1, REWIND_MAX_FRAMES_PER_SNAPSHOT, rate_format, flags);

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("How often a snapshot is taken while playing.");
        ImGui::Text("Fewer snapshots rewind further with the same memory.");
        ImGui::Text("Debug mode takes one every frame and every stop.");
        ImGui::EndTooltip();
    }

    if (ImGui::SliderInt("Memory", &steps, 1, max_steps, memory_format, flags))
        config_rewind.memory_mb = steps * REWIND_MEMORY_STEP_MB;

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("The most memory rewind can use.");
        ImGui::Text("When it fills up, the oldest snapshots go.");
        ImGui::Text("Debug mode always uses %d MB.", REWIND_DEBUG_MEMORY_MB);
        ImGui::EndTooltip();
    }

    ImGui::PopItemWidth();
    ImGui::Separator();

    int required = rewind_get_required_memory_mb();
    int used = (int)((rewind_get_memory_usage() + (512 * 1024)) / (1024 * 1024));

    if (!config_rewind.enabled)
        ImGui::TextColored(rewind_stopped_color, "Disabled");
    else if (required > 0)
        ImGui::TextColored(rewind_stopped_color, "Needs %d MB of memory", required);
    else
        ImGui::TextColored(rewind_recording_color, "%.1f s recorded%s, %d MB", rewind_get_recorded_seconds(),
            config_debug.debug ? " frame by frame" : "", used);
}

static void menu_drives(void)
{
    if (ImGui::BeginMenu("Drives"))
    {
        gui_in_use = true;
        GeartownsCore* core = emu_get_core();
        int floppy_drives = emu_is_empty() ? core->GetPendingMachineConfig().floppy_drives :
            core->GetMachineConfig().floppy_drives;

        menu_cdrom();

        if (floppy_drives > 0)
            gui_floppy_menu(floppy_drives);

        gui_hard_disks_menu();
        gui_memory_card_menu();

        ImGui::Separator();

        menu_default_boot_device();

        ImGui::EndMenu();
    }
}

static void menu_cdrom(void)
{
    if (!ImGui::BeginMenu("CD-ROM"))
        return;

    Media* media = emu_get_core()->GetMedia();
    bool loading = gui_is_rom_loading() || emu_is_media_loading();
    bool inserted = !loading && media->IsReady();
    bool can_insert = !loading && !media->IsPhysicalCdRom();

    if (ImGui::MenuItem("Insert Image...", config_hotkeys[config_HotkeyIndex_OpenROM].str, false, can_insert))
    {
        open_rom = true;
    }

#if defined(GT_ENABLE_PHYSICAL_CDROM)
    if (ImGui::MenuItem("Insert Physical Disc...", NULL, false, !loading && !inserted))
    {
        open_physical_cdrom = true;
    }
#endif

    if (ImGui::MenuItem("Eject", NULL, false, inserted))
    {
        gui_action_eject_media();
    }

    int discs = emu_cdrom_playlist_get_count();

    if ((discs > 0) && ImGui::BeginMenu("Disc in Playlist", can_insert))
    {
        int selected = inserted ? emu_cdrom_playlist_get_index() : -1;

        for (int i = 0; i < discs; i++)
        {
            ImGui::PushID(i);

            if (ImGui::MenuItem(emu_cdrom_playlist_get_name(i), NULL, i == selected) && (i != selected))
            {
                loading = gui_load_playlist_disc(i);
                can_insert = !loading;
            }

            ImGui::PopID();
        }

        ImGui::EndMenu();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Preload CD-ROM in RAM", "", &config_emulator.preload_cdrom))
    {
        emu_set_preload_cdrom(config_emulator.preload_cdrom);
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("This option will preload all CD-ROM tracks in RAM.");
        ImGui::Text("Load a new CD-ROM image to apply changes.");
        ImGui::EndTooltip();
    }

    if (gui_begin_game_menu("Read Speed", Emu_Game_Setting_CDROMSpeed))
    {
        int speed = emu_game_get(Emu_Game_Setting_CDROMSpeed);
        ImGui::PushItemWidth(140.0f);

        if (ImGui::Combo("##cdrom_speed", &speed, "Auto\0" "1x\0" "2x\0" "4x\0" "8x\0\0"))
            emu_game_set(Emu_Game_Setting_CDROMSpeed, speed);

        ImGui::PopItemWidth();

        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Auto reads at the speed of the model's drive: 2x on");
            ImGui::Text("the ME, MA, MX, MF/Fresh, EA, Fresh E, HC and Fresh FS in high-speed mode,");
            ImGui::Text("1x on the rest.");
            ImGui::Text("Faster speeds may break software that doesn't expect them.");
            gui_draw_game_source_tooltip(Emu_Game_Setting_CDROMSpeed);
            ImGui::EndTooltip();
        }

        ImGui::EndMenu();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Fast Forward Loads", "", &config_emulator.ffwd_loads))
    {
        emu_set_ffwd_loads(config_emulator.ffwd_loads);
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Run at maximum speed while the drive reads data and");
        ImGui::Text("the game shows a still, silent screen.");
        ImGui::EndTooltip();
    }

    ImGui::Separator();

    if (loading)
        ImGui::TextDisabled("Loading...");
    else if (!inserted)
        ImGui::TextDisabled("Empty");
    else
    {
        const char* name = media->GetFileName();
        const char* path = media->GetFilePath();

#if defined(GT_ENABLE_PHYSICAL_CDROM)
        if (media->IsPhysicalCdRom())
            name = path = media->GetPhysicalCdRomDeviceId();
#endif

        ImGui::Text("%.32s%s", name, strlen(name) > 32 ? "..." : "");

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", path);
    }

    ImGui::Separator();

    if (ImGui::BeginMenu("Recent", can_insert))
    {
        for (int i = 0; i < config_max_recent_roms; i++)
        {
            if (config_emulator.recent_roms[i].length() > 0)
            {
                if (ImGui::MenuItem(config_emulator.recent_roms[i].c_str()))
                {
                    char media_path[4096];
                    strncpy_fit(media_path, config_emulator.recent_roms[i].c_str(), sizeof(media_path));
                    gui_load_rom(media_path);
                }
            }
        }

        ImGui::EndMenu();
    }

    ImGui::EndMenu();
}

static bool media_menu_actions_enabled(void)
{
    if (emu_is_empty())
        return false;

#if defined(GT_ENABLE_PHYSICAL_CDROM)
    if (emu_get_core()->GetMedia()->HasPhysicalCdRomError())
        return false;
#endif

    return true;
}

static void menu_emulator(void)
{
    if (ImGui::BeginMenu("Emulator"))
    {
        gui_in_use = true;

        menu_machine();

        if (ImGui::MenuItem("Firmware..."))
            gui_firmware_open();

        ImGui::Separator();

        if (ImGui::BeginMenu("Directories"))
        {
            if (ImGui::BeginMenu("Save States"))
            {
                ImGui::PushItemWidth(220.0f);

                if (ImGui::Combo("##savestate_option", &config_emulator.savestates_dir_option,
                    "Default Location\0Same as ROM\0Custom Location\0\0"))
                {
                    update_savestates_data();
                }

                switch ((Directory_Location)config_emulator.savestates_dir_option)
                {
                    case Directory_Location_Default:
                    {
                        ImGui::Text("%s", config_root_path);
                        break;
                    }

                    case Directory_Location_ROM:
                    {
                        if (!emu_is_empty())
                            ImGui::Text("%s", get_current_media_directory_text());

                        break;
                    }

                    case Directory_Location_Custom:
                    {
                        if (ImGui::MenuItem("Choose..."))
                            choose_savestates_path = true;

                        ImGui::PushItemWidth(450);

                        if (ImGui::InputText("##savestate_path", gui_savestates_path, IM_ARRAYSIZE(gui_savestates_path),
                            ImGuiInputTextFlags_AutoSelectAll))
                        {
                            config_emulator.savestates_path.assign(gui_savestates_path);
                            update_savestates_data();
                        }

                        ImGui::PopItemWidth();
                        break;
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Save Files"))
            {
                ImGui::PushItemWidth(220.0f);
                ImGui::Combo("##savefiles_option", &config_emulator.savefiles_dir_option,
                    "Default Location\0Same as Disk\0Custom Location\0\0");

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Where working copies of floppies and CHD hard disks, and the CMOS of each machine, are saved.\nThe CMOS uses the default location with Same as Disk.");

                switch ((Directory_Location)config_emulator.savefiles_dir_option)
                {
                    case Directory_Location_Default:
                    {
                        ImGui::Text("%s", config_root_path);
                        break;
                    }

                    case Directory_Location_ROM:
                        break;

                    case Directory_Location_Custom:
                    {
                        if (ImGui::MenuItem("Choose..."))
                            choose_savefiles_path = true;

                        ImGui::PushItemWidth(450);

                        if (ImGui::InputText("##savefiles_path", gui_savefiles_path, IM_ARRAYSIZE(gui_savefiles_path),
                            ImGuiInputTextFlags_AutoSelectAll))
                        {
                            config_emulator.savefiles_path.assign(gui_savefiles_path);
                        }

                        ImGui::PopItemWidth();
                        break;
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Screenshots"))
            {
                ImGui::PushItemWidth(220.0f);
                ImGui::Combo("##screenshots_option", &config_emulator.screenshots_dir_option, "Default Location\0Same as ROM\0Custom Location\0\0");

                switch ((Directory_Location)config_emulator.screenshots_dir_option)
                {
                    case Directory_Location_Default:
                    {
                        ImGui::Text("%s", config_root_path);
                        break;
                    }

                    case Directory_Location_ROM:
                    {
                        if (!emu_is_empty())
                            ImGui::Text("%s", get_current_media_directory_text());

                        break;
                    }

                    case Directory_Location_Custom:
                    {
                        if (ImGui::MenuItem("Choose..."))
                        {
                            choose_screenshots_path = true;
                        }

                        ImGui::PushItemWidth(450);

                        if (ImGui::InputText("##screenshots_path", gui_screenshots_path, IM_ARRAYSIZE(gui_screenshots_path), ImGuiInputTextFlags_AutoSelectAll))
                        {
                            config_emulator.screenshots_path.assign(gui_screenshots_path);
                        }

                        ImGui::PopItemWidth();
                        break;
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Video Recordings"))
            {
                ImGui::PushItemWidth(220.0f);
                ImGui::Combo("##video_recordings_option", &config_emulator.video_recordings_dir_option, "Default Location\0Same as ROM\0Custom Location\0\0");

                switch ((Directory_Location)config_emulator.video_recordings_dir_option)
                {
                    case Directory_Location_Default:
                    {
                        ImGui::Text("%s", config_root_path);
                        break;
                    }

                    case Directory_Location_ROM:
                    {
                        if (!emu_is_empty())
                            ImGui::Text("%s", get_current_media_directory_text());

                        break;
                    }

                    case Directory_Location_Custom:
                    {
                        if (ImGui::MenuItem("Choose..."))
                        {
                            choose_video_recordings_path = true;
                        }

                        ImGui::PushItemWidth(450);

                        if (ImGui::InputText("##video_recordings_path", gui_video_recordings_path, IM_ARRAYSIZE(gui_video_recordings_path), ImGuiInputTextFlags_AutoSelectAll))
                        {
                            config_emulator.video_recordings_path.assign(gui_video_recordings_path);
                        }

                        ImGui::PopItemWidth();
                        break;
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        ImGui::MenuItem("Power On at Startup", "", &config_emulator.power_on_startup);
        ImGui::MenuItem("Start Paused", "", &config_emulator.start_paused);
        ImGui::MenuItem("Pause When Inactive", "", &config_emulator.pause_when_inactive);

        if (ImGui::MenuItem("Allow Screen Saver", "", &config_emulator.allow_screensaver))
        {
            if (config_emulator.allow_screensaver)
                SDL_EnableScreenSaver();
            else
                SDL_DisableScreenSaver();
        }

        ImGui::Separator();

        //ImGui::MenuItem("Show Media Info", "", &config_emulator.show_info);

        if (ImGui::BeginMenu("Notifications"))
        {
            ImGui::MenuItem("Enabled", "", &config_emulator.show_notifications);

            if (ImGui::BeginMenu("Duration"))
            {
                ImGui::SliderInt("##notification_duration", &config_emulator.notification_duration, 3, 10, "Seconds = %d",
                    ImGuiSliderFlags_AlwaysClamp);
                ImGui::EndMenu();
            }

            ImGui::Separator();
            ImGui::MenuItem("Drive Activity", "", &config_emulator.show_drive_activity);
            ImGui::MenuItem("Fast Forward Loads", "", &config_emulator.ffwd_loads_notification, config_emulator.ffwd_loads);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::BeginTooltip();
                ImGui::Text("Show an icon while a CD-ROM load runs at maximum speed.");
                ImGui::Text("Needs Drives > CD-ROM > Fast Forward Loads.");
                ImGui::EndTooltip();
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Hotkeys"))
        {
            hotkey_configuration_menu(false);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug Hotkeys"))
        {
            hotkey_configuration_menu(true);
            ImGui::EndMenu();
        }

        ImGui::Separator();

        ImGui::MenuItem("Single Instance", "", &config_debug.single_instance);

        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("RESTART REQUIRED");
            ImGui::NewLine();
            ImGui::Text("When enabled, opening media while another instance is running");
            ImGui::Text("will send the media to the running instance instead of");
            ImGui::Text("starting a new one.");
            ImGui::EndTooltip();
        }

        ImGui::EndMenu();
    }
}

static void menu_machine(void)
{
    if (!ImGui::BeginMenu("Machine"))
        return;

    GeartownsCore* core = emu_get_core();
    bool running = !emu_is_empty();
    const GT_Machine_Config& machine = running ? core->GetMachineConfig() : core->GetPendingMachineConfig();
    const GT_Machine_Report& report = core->GetPendingMachineReport();
    const GT_Machine_Profile& profile = k_machine_profiles[core->GetPendingMachineConfig().model];
    GT_Machine_Model database_model = GT_MACHINE_MODEL1_2;
    char summary[128];

    get_machine_summary(machine, summary, sizeof(summary));
    ImGui::TextDisabled("%s", summary);

    if (running && core->IsMachineConfigPending())
    {
        char pending[128];
        get_machine_summary(core->GetPendingMachineConfig(), pending, sizeof(pending));

        if (strcmp(pending, summary) != 0)
            ImGui::TextDisabled("After reset: %s", pending);
    }

    if (report.model == GT_MACHINE_SOURCE_DATABASE)
        ImGui::TextDisabled("Game detected: %s", report.title);
    else if (report.model == GT_MACHINE_SOURCE_FLOPPY)
        ImGui::TextDisabled("Auto: 1.44 MB floppy disk");
    else if (report.model == GT_MACHINE_SOURCE_DEFAULT)
        ImGui::TextDisabled("Auto: game not detected");
    else if ((emu_game_get_source(Emu_Game_Setting_Model) == Emu_Game_Source_User) &&
        core->GetDatabaseModel(database_model) && (database_model != config_machine.model))
        ImGui::TextDisabled("General setting: %s, database: %s", k_machine_profiles[config_machine.model].name,
            k_machine_profiles[database_model].name);

    if (report.ram == GT_MACHINE_SOURCE_HARD_DISK)
        ImGui::TextDisabled("Auto: 4 MB for the hard disk");

    for (int i = 0; i < report.warning_count; i++)
        ImGui::TextColored(machine_warning_color, "%s", report.warnings[i]);

    if (emu_game_is_open())
        ImGui::TextDisabled("Settings for %s", emu_game_get_name());
    else
        ImGui::TextDisabled("General settings");

    ImGui::Separator();

    menu_machine_model();
    menu_machine_cpu_speed(profile);
    menu_machine_ram(profile);
    menu_machine_fpu(profile);
    menu_machine_speed_mode(profile);
    menu_machine_midi_card(profile);
    menu_machine_hardware(machine);

    ImGui::Separator();

    gui_game_forget_item();

#if !defined(GT_DISABLE_DYNAREC)

    ImGui::Separator();

    if (ImGui::MenuItem("CPU Dynarec", "", &config_emulator.dynarec))
        emu_set_dynarec(config_emulator.dynarec);

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Use dynamic recompilation.");
        ImGui::Text("Most games will run faster but some games may run slower.");
        ImGui::Text("The debugger runs the interpreter while a breakpoint, trace or profile is on.");

        if (config_emulator.dynarec && !emu_is_dynarec_enabled())
            ImGui::TextColored(red, "Not available on this system, the interpreter is running.");

        ImGui::EndTooltip();
    }
#endif

    if (running && core->IsMachineConfigPending())
    {
        ImGui::Separator();

        if (ImGui::MenuItem("Reset to Apply Changes"))
            gui_action_reset();
    }

    ImGui::EndMenu();
}

static void menu_machine_model(void)
{
    if (!gui_begin_game_menu("Model", Emu_Game_Setting_Model))
        return;

    GeartownsCore* core = emu_get_core();
    int selected = emu_game_get(Emu_Game_Setting_Model);
    float name_width = 0.0f;
    float cpu_width = 0.0f;
    char cpu[32];
    char preview[64];

    for (int i = 0; i < GT_MACHINE_RELEASE_ORDER_COUNT; i++)
    {
        const GT_Machine_Profile& profile = k_machine_profiles[k_machine_release_order[i]];
        snprintf(cpu, sizeof(cpu), "%s %d MHz", k_machine_cpus[profile.cpu].name, (int)(profile.cpu_clock_rate / 1000000));
        name_width = MAX(name_width, ImGui::CalcTextSize(profile.name).x);
        cpu_width = MAX(cpu_width, ImGui::CalcTextSize(cpu).x);
    }

    float cpu_column = name_width + ImGui::GetStyle().ItemSpacing.x * 4.0f;

    if (selected == GT_MACHINE_AUTO)
        snprintf(preview, sizeof(preview), "Auto (%s)", k_machine_profiles[core->GetPendingMachineConfig().model].name);
    else
        snprintf(preview, sizeof(preview), "%s", k_machine_profiles[selected].name);

    ImGui::PushItemWidth(gui_get_combo_width(MAX(cpu_column + cpu_width, ImGui::CalcTextSize(preview).x)));

    if (gui_begin_game_combo("##machine_model", preview, Emu_Game_Setting_Model))
    {
        if (ImGui::Selectable(selected == GT_MACHINE_AUTO ? preview : "Auto", selected == GT_MACHINE_AUTO))
        {
            emu_game_set(Emu_Game_Setting_Model, GT_MACHINE_AUTO);
        }

        ImGui::Separator();

        for (int i = 0; i < GT_MACHINE_RELEASE_ORDER_COUNT; i++)
        {
            GT_Machine_Model model = k_machine_release_order[i];
            const GT_Machine_Profile& profile = k_machine_profiles[model];
            snprintf(cpu, sizeof(cpu), "%s %d MHz", k_machine_cpus[profile.cpu].name, (int)(profile.cpu_clock_rate / 1000000));

            ImGui::BeginDisabled(!profile.emulated || !core->HasFirmware(model));

            if (ImGui::Selectable(profile.name, selected == model))
            {
                emu_game_set(Emu_Game_Setting_Model, model);
            }

            bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
            ImGui::SameLine(cpu_column);
            ImGui::TextDisabled("%s", cpu);
            ImGui::EndDisabled();

            if (hovered)
                draw_machine_profile_tooltip(model);
        }

        ImGui::EndCombo();
    }
    else if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("It is recommended to leave this option on Auto.");
        ImGui::Text("Auto picks the machine and RAM from game database");
        ImGui::Text("or FM Towns (Model 1/2) if not detected.");
        ImGui::Text("Reset the emulator to apply changes.");
        gui_draw_game_source_tooltip(Emu_Game_Setting_Model);
        ImGui::EndTooltip();
    }

    ImGui::PopItemWidth();
    ImGui::EndMenu();
}

static void menu_machine_cpu_speed(const GT_Machine_Profile& profile)
{
    if (!gui_begin_game_menu("CPU Speed", Emu_Game_Setting_CPUSpeed))
        return;

    int cpu_mhz = emu_game_get(Emu_Game_Setting_CPUSpeed);
    int original = (int)(profile.cpu_clock_rate / 1000000);
    int in_use = (int)(emu_get_core()->GetPendingMachineConfig().cpu_clock_rate / 1000000);
    float text_width = 0.0f;
    char labels[GT_MACHINE_CPU_SPEED_COUNT][32];
    char auto_label[32];
    snprintf(auto_label, sizeof(auto_label), "Auto (%d MHz)", cpu_mhz == GT_MACHINE_AUTO ? in_use : original);
    const char* preview = auto_label;

    for (int i = 0; i < GT_MACHINE_CPU_SPEED_COUNT; i++)
    {
        int speed = k_machine_cpu_speeds_mhz[i];
        snprintf(labels[i], sizeof(labels[i]), speed == original ? "%d MHz (Original)" : "%d MHz", speed);
        text_width = MAX(text_width, ImGui::CalcTextSize(labels[i]).x);

        if (speed == cpu_mhz)
            preview = labels[i];
    }

    ImGui::PushItemWidth(gui_get_combo_width(text_width));

    if (gui_begin_game_combo("##machine_cpu_speed", preview, Emu_Game_Setting_CPUSpeed))
    {
        if (ImGui::Selectable(auto_label, cpu_mhz == GT_MACHINE_AUTO))
        {
            emu_game_set(Emu_Game_Setting_CPUSpeed, GT_MACHINE_AUTO);
        }

        ImGui::Separator();

        for (int i = 0; i < GT_MACHINE_CPU_SPEED_COUNT; i++)
        {
            if (ImGui::Selectable(labels[i], cpu_mhz == k_machine_cpu_speeds_mhz[i]))
            {
                emu_game_set(Emu_Game_Setting_CPUSpeed, k_machine_cpu_speeds_mhz[i]);
            }
        }

        ImGui::EndCombo();
    }
    else if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("It is recommended to leave this option on Auto.");
        ImGui::Text("Auto picks the CPU clock from game database");
        ImGui::Text("or runs the CPU at the model's original clock.");
        ImGui::Text("Reset the emulator to apply changes.");
        gui_draw_game_source_tooltip(Emu_Game_Setting_CPUSpeed);
        ImGui::EndTooltip();
    }

    ImGui::PopItemWidth();
    ImGui::EndMenu();
}

static void menu_machine_ram(const GT_Machine_Profile& profile)
{
    if (!gui_begin_game_menu("Main RAM", Emu_Game_Setting_RAM))
        return;

    GeartownsCore* core = emu_get_core();
    int ram_mb = emu_game_get(Emu_Game_Setting_RAM);
    int in_use = (int)(core->GetPendingMachineConfig().ram_size / (1024 * 1024));
    float text_width = 0.0f;
    char labels[GT_MACHINE_RAM_SIZE_COUNT][32];
    char auto_label[32];
    char preview[96];
    snprintf(auto_label, sizeof(auto_label), "Auto (%d MB)", ram_mb == GT_MACHINE_AUTO ? in_use : profile.ram_default_mb);
    snprintf(preview, sizeof(preview), "%s", auto_label);

    for (int i = 0; i < GT_MACHINE_RAM_SIZE_COUNT; i++)
    {
        int size = k_machine_ram_sizes_mb[i];
        snprintf(labels[i], sizeof(labels[i]), size == profile.ram_default_mb ? "%d MB (Original)" : "%d MB", size);
        text_width = MAX(text_width, ImGui::CalcTextSize(labels[i]).x);

        if (size == ram_mb)
            snprintf(preview, sizeof(preview), "%s", labels[i]);
    }

    if ((ram_mb != GT_MACHINE_AUTO) && (ram_mb != in_use))
        snprintf(preview, sizeof(preview), "%d MB, %d MB on the %s", ram_mb, in_use, profile.name);

    ImGui::PushItemWidth(gui_get_combo_width(MAX(text_width, ImGui::CalcTextSize(preview).x)));

    if (gui_begin_game_combo("##machine_ram", preview, Emu_Game_Setting_RAM))
    {
        if (ImGui::Selectable(auto_label, ram_mb == GT_MACHINE_AUTO))
        {
            emu_game_set(Emu_Game_Setting_RAM, GT_MACHINE_AUTO);
        }

        ImGui::Separator();

        for (int i = 0; i < GT_MACHINE_RAM_SIZE_COUNT; i++)
        {
            if (ImGui::Selectable(labels[i], ram_mb == k_machine_ram_sizes_mb[i]))
            {
                emu_game_set(Emu_Game_Setting_RAM, k_machine_ram_sizes_mb[i]);
            }
        }

        ImGui::EndCombo();
    }
    else if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("It is recommended to leave this option on Auto.");
        ImGui::Text("Auto picks the RAM from game database");
        ImGui::Text("or model's original RAM if not detected.");
        ImGui::Text("Reset the emulator to apply changes.");
        gui_draw_game_source_tooltip(Emu_Game_Setting_RAM);
        ImGui::EndTooltip();
    }

    ImGui::PopItemWidth();
    ImGui::EndMenu();
}

static void menu_machine_fpu(const GT_Machine_Profile& profile)
{
    if (!gui_begin_game_menu("FPU", Emu_Game_Setting_FPU))
        return;

    int fpu = emu_game_get(Emu_Game_Setting_FPU);
    bool in_use = emu_get_core()->GetPendingMachineConfig().fpu;
    bool integrated = profile.fpu == GT_MACHINE_FPU_INTEGRATED;
    const char* labels[2] = { integrated ? "None" : "None (Original)", get_machine_fpu_name(profile.cpu) };
    char auto_label[32];
    char preview[96];
    snprintf(auto_label, sizeof(auto_label), "Auto (%s)", in_use ? labels[1] : "None");
    snprintf(preview, sizeof(preview), "%s", fpu == GT_MACHINE_AUTO ? auto_label : labels[fpu != 0 ? 1 : 0]);

    if ((fpu > 0) && !in_use)
        snprintf(preview, sizeof(preview), "%s, none on the %s", labels[1], profile.name);

    if ((fpu == 0) && integrated)
        snprintf(preview, sizeof(preview), "None, built into the %s", profile.name);

    float text_width = MAX(ImGui::CalcTextSize(labels[0]).x, ImGui::CalcTextSize(preview).x);
    ImGui::PushItemWidth(gui_get_combo_width(text_width));

    if (gui_begin_game_combo("##machine_fpu", preview, Emu_Game_Setting_FPU))
    {
        if (ImGui::Selectable(auto_label, fpu == GT_MACHINE_AUTO))
        {
            emu_game_set(Emu_Game_Setting_FPU, GT_MACHINE_AUTO);
        }

        ImGui::Separator();

        ImGui::BeginDisabled(integrated);

        if (ImGui::Selectable(labels[0], fpu == 0))
        {
            emu_game_set(Emu_Game_Setting_FPU, 0);
        }

        ImGui::EndDisabled();
        ImGui::BeginDisabled(profile.fpu == GT_MACHINE_FPU_NONE);

        if (ImGui::Selectable(labels[1], fpu > 0))
        {
            emu_game_set(Emu_Game_Setting_FPU, 1);
        }

        ImGui::EndDisabled();
        ImGui::EndCombo();
    }
    else if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("It is recommended to leave this option on Auto.");
        ImGui::Text("Auto fits an 80387 or a 487SX if the game database asks for one");
        ImGui::Text("or keeps the model's original: none, or the built-in one of the DX2, DX4 and Pentium.");
        ImGui::Text("Reset the emulator to apply changes.");
        gui_draw_game_source_tooltip(Emu_Game_Setting_FPU);
        ImGui::EndTooltip();
    }

    ImGui::PopItemWidth();
    ImGui::EndMenu();
}

static void menu_machine_speed_mode(const GT_Machine_Profile& profile)
{
    if (!gui_begin_game_menu("Speed Mode", Emu_Game_Setting_SpeedMode))
        return;

    int speed_mode = emu_game_get(Emu_Game_Setting_SpeedMode);
    bool speed_control = (profile.features & GT_MACHINE_FEATURE_SPEED_CONTROL) != 0;
    bool database = emu_get_core()->GetPendingMachineReport().speed_mode == GT_MACHINE_SOURCE_DATABASE;
    const char* labels[2] = { "Compatible", "High Speed" };
    const char* auto_label = !speed_control ? "Auto (Compatible)" : database ? "Auto (High Speed)" : "Auto (CMOS)";
    char preview[96];
    snprintf(preview, sizeof(preview), "%s", speed_mode == GT_MACHINE_AUTO ? auto_label :
        labels[speed_mode != 0 ? 1 : 0]);

    if ((speed_mode > 0) && !speed_control)
        snprintf(preview, sizeof(preview), "%s, none on the %s", labels[1], profile.name);

    float text_width = MAX(ImGui::CalcTextSize(auto_label).x, ImGui::CalcTextSize(preview).x);
    ImGui::PushItemWidth(gui_get_combo_width(text_width));

    if (gui_begin_game_combo("##machine_speed_mode", preview, Emu_Game_Setting_SpeedMode))
    {
        if (ImGui::Selectable(auto_label, speed_mode == GT_MACHINE_AUTO))
        {
            emu_game_set(Emu_Game_Setting_SpeedMode, GT_MACHINE_AUTO);
        }

        ImGui::Separator();

        if (ImGui::Selectable(labels[0], speed_mode == 0))
        {
            emu_game_set(Emu_Game_Setting_SpeedMode, 0);
        }

        ImGui::BeginDisabled(!speed_control);

        if (ImGui::Selectable(labels[1], speed_mode > 0))
        {
            emu_game_set(Emu_Game_Setting_SpeedMode, 1);
        }

        ImGui::EndDisabled();
        ImGui::EndCombo();
    }
    else if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("It is recommended to leave this option on Auto.");
        ImGui::Text("The FM Towns II models, except the UX, run in compatible or high-speed mode.");
        ImGui::Text("High speed cuts memory waits and turns on the 486 cache and the 2x CD-ROM where fitted.");
        ImGui::Text("The system ROM sets the mode at reset from the CMOS, where TownsMENU saves it.");
        ImGui::Text("Auto uses high speed when Model is on Auto and the game database picks a 486 model,");
        ImGui::Text("otherwise it keeps the CMOS setting, compatible on a new CMOS.");
        ImGui::Text("Some software misbehaves in high-speed mode.");
        ImGui::Text("Reset the emulator to apply changes.");
        gui_draw_game_source_tooltip(Emu_Game_Setting_SpeedMode);
        ImGui::EndTooltip();
    }

    ImGui::PopItemWidth();
    ImGui::EndMenu();
}

static void menu_machine_midi_card(const GT_Machine_Profile& profile)
{
    if (!gui_begin_game_menu("MIDI Card", Emu_Game_Setting_MidiCard))
        return;

    int midi_card = emu_game_get(Emu_Game_Setting_MidiCard);
    bool in_use = emu_get_core()->GetPendingMachineConfig().midi_card;
    bool slots = &profile != &k_machine_profiles[GT_MACHINE_MARTY];
    const char* labels[2] = { "None", "FMT-402" };
    char auto_label[32];
    char preview[96];
    snprintf(auto_label, sizeof(auto_label), "Auto (%s)", labels[in_use ? 1 : 0]);
    snprintf(preview, sizeof(preview), "%s", midi_card == GT_MACHINE_AUTO ? auto_label : labels[midi_card != 0 ? 1 : 0]);

    if ((midi_card > 0) && !in_use)
        snprintf(preview, sizeof(preview), "%s, none on the %s", labels[1], profile.name);

    float text_width = MAX(ImGui::CalcTextSize(auto_label).x, ImGui::CalcTextSize(preview).x);
    ImGui::PushItemWidth(gui_get_combo_width(text_width));

    if (gui_begin_game_combo("##machine_midi_card", preview, Emu_Game_Setting_MidiCard))
    {
        if (ImGui::Selectable(auto_label, midi_card == GT_MACHINE_AUTO))
        {
            emu_game_set(Emu_Game_Setting_MidiCard, GT_MACHINE_AUTO);
        }

        ImGui::Separator();

        if (ImGui::Selectable(labels[0], midi_card == 0))
        {
            emu_game_set(Emu_Game_Setting_MidiCard, 0);
        }

        ImGui::BeginDisabled(!slots);

        if (ImGui::Selectable(labels[1], midi_card > 0))
        {
            emu_game_set(Emu_Game_Setting_MidiCard, 1);
        }

        ImGui::EndDisabled();
        ImGui::EndCombo();
    }
    else if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("It is recommended to leave this option on Auto.");
        ImGui::Text("Auto fits a MIDI card only for the games that need one.");
        ImGui::Text("Other games with MIDI music offer it in their own options with a card fitted.");
        ImGui::Text("A card changes the music tempo source of many games, with or without MIDI.");
        ImGui::Text("The Marty has no slot for it.");
        ImGui::Text("Reset the emulator to apply changes.");
        gui_draw_game_source_tooltip(Emu_Game_Setting_MidiCard);
        ImGui::EndTooltip();
    }

    ImGui::PopItemWidth();
    ImGui::EndMenu();
}

static const char* get_machine_fpu_text(const GT_Machine_Profile& profile)
{
    if (profile.fpu == GT_MACHINE_FPU_INTEGRATED)
        return "integrated";

    if (profile.fpu == GT_MACHINE_FPU_NONE)
        return "none, no socket";

    return profile.cpu == GT_MACHINE_CPU_80486SX ? "487SX card, optional" : "80387 card, optional";
}

static void menu_machine_hardware(const GT_Machine_Config& config)
{
    if (!ImGui::BeginMenu("Summary"))
        return;

    if (!emu_is_empty() && emu_get_core()->IsMachineConfigPending())
    {
        ImGui::TextColored(machine_warning_color, "Reset the emulator to apply the pending changes");
        ImGui::Separator();
    }

    char hardware[4096];
    get_machine_hardware(config, hardware, sizeof(hardware));
    ImVec2 size = ImGui::CalcTextSize(hardware);
    ImVec2 padding = ImGui::GetStyle().FramePadding;

    ImGui::InputTextMultiline("##hardware", hardware, sizeof(hardware),
        ImVec2(size.x + (padding.x * 2.0f), size.y + (padding.y * 2.0f)), ImGuiInputTextFlags_ReadOnly);

    ImGui::EndMenu();
}

static void get_machine_hardware(const GT_Machine_Config& config, char* text, size_t text_size)
{
    GeartownsCore* core = emu_get_core();
    const GT_Machine_Profile& profile = k_machine_profiles[config.model];
    char firmware[GT_MAX_PATH];
    gui_get_firmware_status(config.model, firmware, sizeof(firmware));
    u32 registers = profile.features & ~(u32)GT_MACHINE_FEATURE_SCSI;
    u32 mhz = config.cpu_clock_rate / 1000000;
    u32 original_mhz = profile.cpu_clock_rate / 1000000;
    int ram_mb = (int)(config.ram_size / (1024 * 1024));
    int max_ram_mb = profile.ram_default_mb;
    bool optional_fpu = profile.fpu == GT_MACHINE_FPU_SOCKET;
    bool hires = (profile.features & GT_MACHINE_FEATURE_HIRES_CRTC) != 0;
    text[0] = 0;

    for (int i = 0; profile.ram_sizes_mb[i] != 0; i++)
        max_ram_mb = MAX(max_ram_mb, (int)profile.ram_sizes_mb[i]);

    add_machine_line(text, text_size, "Model: %s", profile.name);
    add_machine_line(text, text_size, "Machine ID: %04Xh", profile.machine_id);

    if (mhz == original_mhz)
        add_machine_line(text, text_size, "CPU: %s at %u MHz", k_machine_cpus[config.cpu].name, mhz);
    else
        add_machine_line(text, text_size, "CPU: %s at %u MHz, originally %u MHz", k_machine_cpus[config.cpu].name, mhz,
            original_mhz);

    if (optional_fpu && config.fpu)
        add_machine_line(text, text_size, "FPU: %s card", get_machine_fpu_name(config.cpu));
    else
        add_machine_line(text, text_size, "FPU: %s%s", optional_fpu ? "none, " : "", get_machine_fpu_text(profile));

    if ((profile.features & GT_MACHINE_FEATURE_SPEED_CONTROL) == 0)
        add_machine_line(text, text_size, "Speed mode: compatible only");
    else if (!emu_is_empty())
        add_machine_line(text, text_size, "Speed mode: %s",
            core->GetSystemControl()->GetState()->high_speed ? "high speed" : "compatible");
    else
        add_machine_line(text, text_size, "Speed mode: %s", config.speed_mode == GT_MACHINE_AUTO ? "from CMOS" :
            config.speed_mode != 0 ? "high speed" : "compatible");

    if (ram_mb == profile.ram_default_mb)
        add_machine_line(text, text_size, "Main RAM: %d MB, up to %d MB", ram_mb, max_ram_mb);
    else
        add_machine_line(text, text_size, "Main RAM: %d MB, originally %d MB, up to %d MB", ram_mb,
            profile.ram_default_mb, max_ram_mb);

    add_machine_line(text, text_size, "Video RAM: %d KB",
        (hires ? VIDEO_HIRES_VRAM_SIZE : VIDEO_VRAM_SIZE) / 1024);
    add_machine_line(text, text_size, "Memory map: %s", k_machine_maps[profile.map].name);
    add_machine_line(text, text_size, "16-bit PCM: %s",
        (profile.features & GT_MACHINE_FEATURE_PCM16) != 0 ? "built in" : "none");
    add_machine_line(text, text_size, "MIDI card: %s", config.midi_card ? "FMT-402" :
        config.model == GT_MACHINE_MARTY ? "none, no slot" : "none");
    add_machine_line(text, text_size, "CD-ROM drive: %s",
        (profile.features & GT_MACHINE_FEATURE_CD_DOUBLE_SPEED) != 0 ? "1x, 2x in high-speed mode" : "1x");
    add_machine_line(text, text_size, "Floppy drives: %d%s", config.floppy_drives,
        profile.three_mode_floppy ? ", three-mode" : "");
    add_machine_line(text, text_size, "SCSI: %s", (profile.features & GT_MACHINE_FEATURE_SCSI) != 0 ? "built in" :
        (config.model != GT_MACHINE_MODEL1_2) ? "none" : config.scsi ? "card" :
        "none, a card is fitted while a hard disk is attached");

    char disks[64] = "";

    for (int i = 0; i < GT_MAX_HARD_DISKS; i++)
    {
        if (!core->GetHardDisk(i)->IsOpen())
            continue;

        char id[16];
        snprintf(id, sizeof(id), "%s%d", disks[0] != 0 ? ", " : "ID ", i);
        strncat_fit(disks, id, sizeof(disks));
    }

    add_machine_line(text, text_size, "Hard disks: %s", disks[0] != 0 ? disks : "none");

    u32 card_size = core->GetMemoryCard()->IsInserted() ? core->GetMemoryCard()->GetSize() / 1024 : 0;

    if (card_size == 0)
        add_machine_line(text, text_size, "IC memory card: none");
    else if ((card_size % 1024) != 0)
        add_machine_line(text, text_size, "IC memory card: %u KB", card_size);
    else
        add_machine_line(text, text_size, "IC memory card: %u MB", card_size / 1024);

    add_machine_line(text, text_size, "Firmware: %s", firmware);
    add_machine_line(text, text_size, "Towns II registers:%s", registers == 0 ? " none" : "");

    for (int i = 0; i < GT_MACHINE_FEATURE_COUNT; i++)
    {
        if (IS_SET_BIT(registers, i))
            add_machine_line(text, text_size, "  %s", k_machine_feature_names[i]);
    }
}

static void add_machine_line(char* text, size_t text_size, const char* format, ...)
{
    char line[GT_MAX_PATH + 32];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    if (text[0] != 0)
        strncat_fit(text, "\n", text_size);

    strncat_fit(text, line, text_size);
}

static void draw_machine_profile_tooltip(GT_Machine_Model model)
{
    const GT_Machine_Profile& profile = k_machine_profiles[model];
    char firmware[GT_MAX_PATH];
    gui_get_firmware_status(model, firmware, sizeof(firmware));
    char ram[96] = "";

    for (int i = 0; profile.ram_sizes_mb[i] != 0; i++)
    {
        char size[8];
        snprintf(size, sizeof(size), i == 0 ? "%d" : ", %d", profile.ram_sizes_mb[i]);
        strncat_fit(ram, size, sizeof(ram));
    }

    ImGui::BeginTooltip();
    ImGui::Text("Models: %s", profile.models);
    ImGui::Text("Released: %s", profile.released);
    ImGui::Text("CPU: %s at %d MHz", k_machine_cpus[profile.cpu].name, (int)(profile.cpu_clock_rate / 1000000));
    ImGui::Text("FPU: %s", get_machine_fpu_text(profile));
    ImGui::Text("RAM: %s MB", ram);
    ImGui::Text("Floppy drives: %d%s", profile.floppy_drives, profile.three_mode_floppy ? ", three-mode" : "");
    ImGui::Text("Firmware:");
    ImGui::SameLine();
    ImGui::TextColored(gui_get_firmware_status_color(model), "%s", firmware);

    if (!profile.emulated)
    {
        ImGui::Separator();
        ImGui::TextDisabled("Not emulated yet");
    }
    else if (!emu_get_core()->HasFirmware(model))
    {
        ImGui::Separator();
        ImGui::TextDisabled("No firmware found, see Emulator > Firmware...");
    }

    ImGui::EndTooltip();
}

static void menu_boot_device(void)
{
    static const char* const keys[GT_BOOT_DEVICE_COUNT] = { "", "C and D", "F and 0", "F and 1", "H and 0", "H and 1",
        "H and 2", "H and 3", "H and 4", "I, C and M" };
    GeartownsCore* core = emu_get_core();
    const GT_Machine_Config& config = core->GetPendingMachineConfig();
    bool supported = core->IsBootDeviceSupported(config.model);

    for (int i = GT_BOOT_DEVICE_CDROM; i < GT_BOOT_DEVICE_COUNT; i++)
    {
        bool enabled = supported;

        if ((i == GT_BOOT_DEVICE_FLOPPY_0) || (i == GT_BOOT_DEVICE_FLOPPY_1))
            enabled = enabled && ((i - GT_BOOT_DEVICE_FLOPPY_0) < config.floppy_drives);
        else if (i == GT_BOOT_DEVICE_MEMORY_CARD)
            enabled = enabled && core->GetMemoryCard()->IsInserted();
        else if (i >= GT_BOOT_DEVICE_HARD_DISK_0)
        {
            int id = i - GT_BOOT_DEVICE_HARD_DISK_0;
            bool attached = core->IsHardDiskPending(id) ? (core->GetPendingHardDiskPath(id)[0] != 0) :
                core->GetHardDisk(id)->IsOpen();
            enabled = enabled && attached;
        }

        if ((i == GT_BOOT_DEVICE_FLOPPY_0) || (i == GT_BOOT_DEVICE_HARD_DISK_0) || (i == GT_BOOT_DEVICE_MEMORY_CARD))
            ImGui::Separator();

        if (ImGui::MenuItem(emu_get_boot_device_name((GT_Boot_Device)i), NULL, false, enabled))
        {
            core->SetBootDevice((GT_Boot_Device)i);
            gui_action_reset();
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            if (supported)
                ImGui::SetTooltip("Holds %s while the system ROM picks the boot device", keys[i]);
            else
                ImGui::SetTooltip("This model's system ROM doesn't read boot keys");
        }
    }
}

static void menu_default_boot_device(void)
{
    if (!ImGui::BeginMenu("Default Boot Device"))
        return;

    GeartownsCore* core = emu_get_core();
    GT_Boot_Device device = GT_BOOT_DEVICE_AUTO;

    ImGui::PushItemWidth(160.0f);

    if (ImGui::Combo("##default_boot_device", &config_emulator.default_boot_device,
        "Auto\0" "CD-ROM\0" "Floppy 1\0" "Floppy 2\0" "Hard Disk ID 0\0" "Hard Disk ID 1\0" "Hard Disk ID 2\0"
        "Hard Disk ID 3\0" "Hard Disk ID 4\0" "IC Memory Card\0\0", GT_BOOT_DEVICE_COUNT))
    {
        emu_apply_boot_device();
    }

    ImGui::PopItemWidth();

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("The device each power-on and reset boots from.");
        ImGui::Text("Auto follows the system ROM's normal boot order.");
        ImGui::Text("Changes apply at the next power-on or reset.");
        ImGui::EndTooltip();
    }

    bool overridden = emu_is_boot_device_overridden(&device);
    bool supported = core->IsBootDeviceSupported(core->GetPendingMachineConfig().model);

    if (overridden || !supported)
        ImGui::Separator();

    if (overridden)
        ImGui::TextDisabled("Command line: %s", emu_get_boot_device_name(device));

    if (!supported)
        ImGui::TextDisabled("NOTE: Selected model\ndoesn't read boot keys");

    ImGui::EndMenu();
}

static void get_machine_summary(const GT_Machine_Config& config, char* text, size_t text_size)
{
    char cpu[96];
    get_machine_cpu_text(config, cpu, sizeof(cpu));
    snprintf(text, text_size, "%s · %s · %u MB", k_machine_profiles[config.model].name, cpu,
        config.ram_size / (1024 * 1024));
}

static void menu_video(void)
{
    if (ImGui::BeginMenu("Video"))
    {
        gui_in_use = true;

        bool is_recording = emu_is_video_recording();

        if (ImGui::MenuItem("Full Screen", config_hotkeys[config_HotkeyIndex_Fullscreen].str, &config_emulator.fullscreen, !is_recording))
        {
            application_trigger_fullscreen(config_emulator.fullscreen);
        }

#if !defined(__APPLE__)
        if (ImGui::BeginMenu("Fullscreen Mode", !is_recording))
        {
            ImGui::PushItemWidth(130.0f);
            if (ImGui::Combo("##fullscreen_mode", &config_emulator.fullscreen_mode, "Borderless\0Exclusive\0\0") &&
                config_emulator.fullscreen)
            {
                application_trigger_fullscreen(true);
            }
            ImGui::PopItemWidth();
            ImGui::EndMenu();
        }
#endif

        ImGui::Separator();

        ImGui::MenuItem("Always Show Menu", config_hotkeys[config_HotkeyIndex_ShowMainMenu].str, &config_emulator.always_show_menu);

        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("This option will enable menu even in fullscreen.");
            ImGui::Text("Menu always shows in debug mode.");
            ImGui::EndTooltip();
        }

        if (ImGui::MenuItem("Resize Window to Content", NULL, false, !is_recording))
        {
            if (!config_debug.debug)
            {
                application_trigger_fit_to_content(gui_main_window_width, gui_main_window_height + gui_main_menu_height);
            }
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Scale"))
        {
            ImGui::PushItemWidth(250.0f);
            ImGui::Combo("##scale", &config_video.scale,
                "Integer Scale (Auto)\0Integer Scale (Manual)\0"
                "Scale to Window Height\0Scale to Window Width & Height\0\0");

            if (config_video.scale == 1)
                ImGui::SliderInt("##scale_manual", &config_video.scale_manual, 1, 20);

            ImGui::PopItemWidth();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Aspect Ratio"))
        {
            ImGui::PushItemWidth(190.0f);
            ImGui::Combo("##ratio", &config_video.ratio,
                "Square Pixels (1:1 PAR)\0Standard (4:3 DAR)\0Wide (16:9 DAR)\0Wide (16:10 DAR)\0\0");
            ImGui::PopItemWidth();
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Native Resolution", "", &config_video.native_resolution))
            emu_set_native_resolution(config_video.native_resolution);

        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Output one pixel per game pixel, as shaders expect.");
            ImGui::Text("When disabled, zoomed modes are output at monitor resolution.");
            ImGui::EndTooltip();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Vertical Sync"))
        {
#if defined(_WIN32)
            ImGui::PushItemWidth(220.0f);

            if (ImGui::Combo("##sync_mode", &config_video.sync_mode, "Disabled\0Fixed Vertical Sync\0Variable Refresh Rate (VRR)\0\0"))
#else
            ImGui::PushItemWidth(100.0f);

            if (ImGui::Combo("##sync_mode", &config_video.sync_mode, "Disabled\0Enabled\0\0"))
#endif
            {
                if (config_video.sync_mode != config_VideoSync_Disabled)
                {
                    config_audio.sync = true;
                    config_emulator.ffwd = false;
                    emu_audio_reset();
                }

                display_use_vsync_if_enabled();
            }

            ImGui::PopItemWidth();

#if defined(_WIN32)
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Disabled: do not synchronize presentation to the monitor.");
                ImGui::Text("Fixed Vertical Sync: use normal VSync.");
                ImGui::Text("VRR: present at the emulator frame rate.");
                ImGui::Text("\nVRR requires fullscreen, a VRR display, and G-SYNC,");
                ImGui::Text("FreeSync, or Adaptive Sync enabled in your monitor and GPU driver settings.");
                ImGui::EndTooltip();
            }
#endif
            ImGui::EndMenu();
        }

        ImGui::MenuItem("Show FPS", "", &config_video.fps);

        ImGui::Separator();

        gui_shader_menu();

        ImGui::Separator();

        if (ImGui::BeginMenu("Theme"))
        {
            ImGui::PushItemWidth(100.0f);

            if (ImGui::Combo("##theme", &config_emulator.theme, "Light\0Dark\0\0"))
            {
                gui_set_style();
            }

            ImGui::PopItemWidth();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Background Color"))
        {
            draw_background_color_menu("Dark Theme", config_Theme_Dark);
            draw_background_color_menu("Light Theme", config_Theme_Light);

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }
}

static void draw_background_color_menu(const char* label, int theme)
{
    if (ImGui::BeginMenu(label))
    {
        ImGui::PushID(theme);

        ImGui::ColorEdit3("##normal_bg", config_video.background_color[theme], ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float);
        ImGui::SameLine();
        ImGui::Text("Normal Background");

        ImGui::Separator();

        if (ImGui::ColorEdit3("##debugger_bg", config_video.background_color_debugger[theme], ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float))
        {
            if (config_emulator.theme == theme)
                gui_set_style();
        }

        ImGui::SameLine();
        ImGui::Text("Debugger Background");

        ImGui::PopID();
        ImGui::EndMenu();
    }
}

static void menu_input_controller(int port)
{
    int setting = Emu_Game_Setting_Port1 + port;
    int controller = emu_game_get(setting);
    bool automatic = controller == config_ControllerType_Auto;
    char preview[64];

    if (automatic)
        snprintf(preview, sizeof(preview), "Auto (%s)", emu_get_pad_type_name(emu_get_pad_type((GT_Controllers)port)));
    else
        snprintf(preview, sizeof(preview), "%s", emu_get_pad_type_name(controller));

    if (gui_begin_game_combo("##controller", preview, setting))
    {
        if (ImGui::Selectable(automatic ? preview : "Auto", automatic))
        {
            emu_game_set(setting, config_ControllerType_Auto);
        }

        ImGui::Separator();

        for (int type = GT_CONTROLLER_NONE; type <= GT_CONTROLLER_MOUSE; type++)
        {
            if (ImGui::Selectable(emu_get_pad_type_name(type), controller == type))
            {
                emu_game_set(setting, type);
            }
        }

        ImGui::EndCombo();
    }
    else if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("It is recommended to leave this option on Auto.");
        ImGui::Text("Auto picks the controller from game database or");
        ImGui::Text("Original Gamepad (port 1) and Mouse (port 2) if not detected.");
        gui_draw_game_source_tooltip(setting);
        ImGui::EndTooltip();
    }
}

static void menu_input(void)
{
    if (ImGui::BeginMenu("Input"))
    {
        gui_in_use = true;

        if (ImGui::MenuItem("Game Focus", config_hotkeys[config_HotkeyIndex_GameFocus].str, false, !emu_is_empty()))
            game_focus_set(true);

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::BeginTooltip();
            ImGui::Text("Locks the mouse in the window and turns hotkeys off,");
            ImGui::Text("except Game Focus, Fullscreen and the keyboard mapper's.");
            ImGui::Text("Going fullscreen also enters it.");
            ImGui::EndTooltip();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("FM Towns Ports"))
        {
            for (int i = 0; i < GT_MAX_GAMEPADS; i++)
            {
                char player_name[32];
                snprintf(player_name, sizeof(player_name), "Port %d", i + 1);

                if (ImGui::BeginMenu(player_name))
                {
                    ImGui::PushItemWidth(200.0f);
                    menu_input_controller(i);
                    ImGui::PopItemWidth();

                    if (gui_begin_game_menu("Keyboard Mode", Emu_Game_Setting_Keyboard1 + i))
                    {
                        int keyboard_mode = emu_game_get(Emu_Game_Setting_Keyboard1 + i);
                        ImGui::PushItemWidth(170.0f);

                        if (ImGui::Combo("##keyboard_mode", &keyboard_mode, "Gamepad\0FM Towns Keyboard\0\0"))
                            emu_game_set(Emu_Game_Setting_Keyboard1 + i, keyboard_mode);

                        ImGui::PopItemWidth();

                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::Text("Gamepad: the Port %d bindings from Input > Keyboard control this gamepad.", i + 1);
                            ImGui::Text("FM Towns Keyboard: the Port %d bindings type on the FM Towns keyboard.", i + 1);
                            ImGui::Text("Other keys always type on the FM Towns keyboard.");
                            gui_draw_game_source_tooltip(Emu_Game_Setting_Keyboard1 + i);
                            ImGui::EndTooltip();
                        }

                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Keyboard"))
        {
            if (ImGui::MenuItem("Keyboard Mapper...", config_hotkeys[config_HotkeyIndex_KeyboardMapper].str,
                emu_key_mapper_is_open(), !emu_is_empty()))
                emu_key_mapper_toggle();

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::BeginTooltip();
                ImGui::Text("Shows the FM Towns keyboard to bind host keys to it,");
                ImGui::Text("for this game only. The Towns stops while it's open.");
                ImGui::EndTooltip();
            }

            bool key_mapping = emu_key_mapping_is_enabled();

            if (ImGui::MenuItem("Key Mapping", config_hotkeys[config_HotkeyIndex_KeyMapping].str, &key_mapping,
                !emu_is_empty()))
                gui_action_toggle_key_mapping();

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::BeginTooltip();
                ImGui::Text("Off: every key types its own, to type text in a");
                ImGui::Text("game whose keys were moved.");
                ImGui::EndTooltip();
            }

            if (ImGui::BeginMenu("Layout"))
            {
                ImGui::PushItemWidth(170.0f);

                if (ImGui::Combo("##keyboard_layout", &config_input.keyboard_layout, "Host Layout\0Japanese Positions\0\0"))
                    emu_set_keyboard_layout(config_input.keyboard_layout);

                ImGui::PopItemWidth();

                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("Host Layout: keys type what they show on this keyboard.");
                    ImGui::Text("Japanese Positions: keys type what a JIS keyboard has in");
                    ImGui::Text("the same place, for kana typing by position.");
                    ImGui::EndTooltip();
                }

                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (input_macro_get_owner() == Input_Macro_Owner_GUI)
            {
                if (ImGui::MenuItem("Stop Pasting"))
                    gui_action_stop_pasting();
            }
            else
            {
                if (ImGui::MenuItem("Paste as Keystrokes", NULL, false, !emu_is_empty() &&
                    input_macro_get_owner() == Input_Macro_Owner_None))
                    gui_action_paste_keystrokes();

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("Types the clipboard text on the FM Towns keyboard,");
                    ImGui::Text("up to %d characters. Characters the JIS keyboard", k_input_macro_max_paste);
                    ImGui::Text("can't type, such as kana and kanji, are skipped.");
                    ImGui::EndTooltip();
                }
            }

            ImGui::Separator();

            for (int i = 0; i < GT_MAX_GAMEPADS; i++)
            {
                char keyboard_name[32];
                snprintf(keyboard_name, sizeof(keyboard_name), "Port %d Mapping", i + 1);

                if (ImGui::BeginMenu(keyboard_name))
                {
                    ImGui::TextDisabled("Keyboard %s", keyboard_name);
                    ImGui::Separator();
                    keyboard_configuration_item("Left:", &config_input_keyboard[i].key_left, i);
                    keyboard_configuration_item("Right:", &config_input_keyboard[i].key_right, i);
                    keyboard_configuration_item("Up:", &config_input_keyboard[i].key_up, i);
                    keyboard_configuration_item("Down:", &config_input_keyboard[i].key_down, i);
                    keyboard_configuration_item("Select:", &config_input_keyboard[i].key_select, i);
                    keyboard_configuration_item("Run:", &config_input_keyboard[i].key_run, i);
                    keyboard_configuration_item("A:", &config_input_keyboard[i].key_A, i);
                    keyboard_configuration_item("B:", &config_input_keyboard[i].key_B, i);
                    ImGui::Separator();
                    ImGui::TextDisabled("Marty Gamepad:");
                    keyboard_configuration_item("Zoom:", &config_input_keyboard[i].key_zoom, i);
                    ImGui::Separator();
                    ImGui::TextDisabled("6 Button Gamepad:");
                    keyboard_configuration_item("A:", &config_input_keyboard[i].key_6_A, i);
                    keyboard_configuration_item("B:", &config_input_keyboard[i].key_6_B, i);
                    keyboard_configuration_item("C:", &config_input_keyboard[i].key_C, i);
                    keyboard_configuration_item("X:", &config_input_keyboard[i].key_X, i);
                    keyboard_configuration_item("Y:", &config_input_keyboard[i].key_Y, i);
                    keyboard_configuration_item("Z:", &config_input_keyboard[i].key_Z, i);

                    gui_popup_modal_keyboard();

                    ImGui::EndMenu();
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Mouse"))
        {
            ImGui::SliderInt("##mouse_sensitivity", &config_emulator.mouse_sensitivity, 1, 15, "Sensitivity = %d");

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Host Gamepads"))
        {
            for (int i = 0; i < GT_MAX_GAMEPADS; i++)
            {
                char gamepad_name[32];
                snprintf(gamepad_name, sizeof(gamepad_name), "Port %d", i + 1);

                if (ImGui::BeginMenu(gamepad_name))
                {
                    if (!gamepad_controller[i])
                        ImGui::TextDisabled("This gamepad is not detected");
                    else
                        ImGui::TextDisabled("Gamepad detected for Port %d", i + 1);

                    ImGui::Separator();

                    if (ImGui::BeginMenu("Device"))
                    {
                        gamepad_device_selector(i);
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("Directional Controls"))
                    {
                        ImGui::PushItemWidth(200.0f);
                        ImGui::Combo("##directional", &config_input_gamepad[i].gamepad_directional,
                            "D-pad\0Left Analog Stick\0D-pad + Left Analog Stick\0\0");
                        ImGui::PopItemWidth();
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("Button Configuration"))
                    {
                        ImGui::TextDisabled("Gamepad %s", gamepad_name);
                        ImGui::Separator();
                        gamepad_configuration_item("Select:", &config_input_gamepad[i].gamepad_select, i);
                        gamepad_configuration_item("Run:", &config_input_gamepad[i].gamepad_run, i);
                        gamepad_configuration_item("A:", &config_input_gamepad[i].gamepad_A, i);
                        gamepad_configuration_item("B:", &config_input_gamepad[i].gamepad_B, i);
                        ImGui::Separator();
                        ImGui::TextDisabled("Marty Gamepad:");
                        gamepad_configuration_item("Zoom:", &config_input_gamepad[i].gamepad_zoom, i);
                        ImGui::Separator();
                        ImGui::TextDisabled("6 Button Gamepad:");
                        gamepad_configuration_item("A:", &config_input_gamepad[i].gamepad_6_A, i);
                        gamepad_configuration_item("B:", &config_input_gamepad[i].gamepad_6_B, i);
                        gamepad_configuration_item("C:", &config_input_gamepad[i].gamepad_C, i);
                        gamepad_configuration_item("X:", &config_input_gamepad[i].gamepad_X, i);
                        gamepad_configuration_item("Y:", &config_input_gamepad[i].gamepad_Y, i);
                        gamepad_configuration_item("Z:", &config_input_gamepad[i].gamepad_Z, i);

                        gui_popup_modal_gamepad(i);

                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("Shortcut Configuration"))
                    {
                        ImGui::TextDisabled("Gamepad %s - Shortcuts", gamepad_name);
                        ImGui::Separator();

                        for (int j = 0; j < config_HotkeyIndex_COUNT; j++)
                        {
                            if ((hotkey_definitions[j].flags & HOTKEY_FLAG_GAMEPAD) == 0)
                                continue;

                            char label[64];
                            snprintf(label, sizeof(label), "%s:", hotkey_definitions[j].label);
                            gamepad_configuration_item(label, &config_input_gamepad_shortcuts[i].gamepad_shortcuts[j], i);
                        }

                        gui_popup_modal_gamepad(i);

                        ImGui::EndMenu();
                    }

                    ImGui::EndMenu();
                }
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        ImGui::MenuItem("Allow Up+Down / Left+Right", "", &config_input.allow_up_down);

        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Allow pressing, quickly alternating, or holding");
            ImGui::Text("both left and right or up and down directions.");
            ImGui::Text("This may cause movement glitches in certain games.");
            ImGui::EndTooltip();
        }

        if (ImGui::MenuItem("Allow Shared Gamepads", "", &config_input.allow_shared_gamepads))
        {
            if (!config_input.allow_shared_gamepads)
                gamepad_remove_duplicates();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Let one gamepad control several ports at the same time.");
            ImGui::Text("Choose it in the Device menu of each port.");
            ImGui::EndTooltip();
        }

        ImGui::EndMenu();
    }
}

static void menu_audio(void)
{
    if (ImGui::BeginMenu("Audio"))
    {
        gui_in_use = true;

        bool mute = !config_audio.enable;

        if (ImGui::MenuItem("Mute All Audio", config_hotkeys[config_HotkeyIndex_Mute].str, &mute))
        {
            config_audio.enable = !mute;
            emu_audio_mute(mute);
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Master Volume", config_audio.enable))
        {
            ImGui::PushItemWidth(200.0f);

            if (volume_slider("##master_volume", &config_audio.master_volume))
            {
                emu_audio_set_master_volume(config_audio.master_volume);
            }

            ImGui::PopItemWidth();

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Anything above 100%% may cause clipping.");
                ImGui::EndTooltip();
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        menu_audio_source("FM Volume", "##fm_volume", Audio::AUDIO_SOURCE_FM, &config_audio.fm_volume);
        menu_audio_source("PCM Volume", "##pcm_volume", Audio::AUDIO_SOURCE_PCM, &config_audio.pcm_volume);
        menu_audio_source("CD-DA Volume", "##cdda_volume", Audio::AUDIO_SOURCE_CDDA, &config_audio.cdda_volume);
        menu_audio_source("Buzzer Volume", "##buzzer_volume", Audio::AUDIO_SOURCE_BUZZER, &config_audio.buzzer_volume);
        menu_audio_source("16-bit PCM Volume", "##pcm16_volume", Audio::AUDIO_SOURCE_PCM16, &config_audio.pcm16_volume);
        menu_audio_synth_volume("Built-in MT-32 Volume", "##mt32_volume", GT_MIDI_ENGINE_MT32, &config_audio.mt32_volume);
        menu_audio_synth_volume("Built-in General MIDI Volume", "##gm_volume", GT_MIDI_ENGINE_SOUNDFONT,
            &config_audio.gm_volume);

        ImGui::Separator();

        menu_audio_midi();
        menu_audio_synths();

        ImGui::Separator();

        if (ImGui::BeginMenu("Buffer Size", config_audio.enable))
        {
            ImGui::PushItemWidth(150.0f);

            if (ImGui::SliderInt("##buffer_count", &config_audio.buffer_count, 2, 5, "Buffers = %d"))
            {
                emu_audio_restart();
            }

            ImGui::PopItemWidth();

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Audio latency: %.0f ms", sound_queue_get_target_latency_ms());
                ImGui::Text("\nLower values reduce audio latency.");
                ImGui::Text("Higher values prevent audio underruns.");
                ImGui::Text("Enabling VSync may force higher buffer counts.");

                ImGui::EndTooltip();
            }

            ImGui::EndMenu();
        }

#ifndef GT_DISABLE_VGMRECORDER
        ImGui::Separator();

        if (ImGui::BeginMenu("VGM Recorder"))
        {
            bool is_recording = emu_is_vgm_recording();

            if (ImGui::MenuItem("Start Recording...", "", false, !is_recording && !emu_is_empty()))
            {
                save_vgm = true;
            }

            if (ImGui::MenuItem("Stop Recording", "", false, is_recording))
            {
                emu_stop_vgm_recording();
                gui_notify(gui_NotificationInfo, ICON_MD_STOP, "VGM recording stopped", NULL, "vgm");
            }

            ImGui::EndMenu();
        }
#endif

        ImGui::EndMenu();
    }
}

static void menu_audio_source(const char* label, const char* id, int source, float* volume)
{
    if (ImGui::BeginMenu(label, config_audio.enable))
    {
        bool mute = emu_audio_is_source_muted(source);

        if (ImGui::MenuItem("Mute", "", &mute))
            emu_audio_set_source_mute(source, mute);

        ImGui::Separator();
        ImGui::PushItemWidth(200.0f);

        if (volume_slider(id, volume))
            emu_audio_set_source_volume(source, *volume);

        ImGui::PopItemWidth();

        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Anything above 100%% may cause clipping.");
            ImGui::EndTooltip();
        }

        ImGui::EndMenu();
    }
}

static void menu_audio_synth_volume(const char* label, const char* id, int engine, float* volume)
{
    if (!ImGui::BeginMenu(label, config_audio.enable && emu_synth_is_compiled(engine)))
        return;

    bool mute = emu_audio_is_source_muted(Audio::AUDIO_SOURCE_MIDI);

    if (ImGui::MenuItem("Mute", "", &mute))
        emu_audio_set_source_mute(Audio::AUDIO_SOURCE_MIDI, mute);

    ImGui::Separator();
    ImGui::PushItemWidth(200.0f);

    if (volume_slider(id, volume))
        emu_audio_set_synth_volume(engine, *volume);

    ImGui::PopItemWidth();

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Anything above 100%% may cause clipping.");
        ImGui::EndTooltip();
    }

    ImGui::EndMenu();
}

static bool volume_slider(const char* id, float* volume)
{
    int percent = (int)((*volume * 100.0f) + 0.5f);

    if (!ImGui::SliderInt(id, &percent, 0, 200, "Volume = %d%%", ImGuiSliderFlags_AlwaysClamp))
        return false;

    *volume = (float)percent / 100.0f;
    return true;
}

static void menu_audio_midi(void)
{
    bool builtin = emu_synth_is_compiled(GT_MIDI_ENGINE_MT32) || emu_synth_is_compiled(GT_MIDI_ENGINE_SOUNDFONT);

    if (!ImGui::BeginMenu("MIDI Out", MidiPort::IsSupported() || builtin))
        return;

    emu_midi_prepare();

    menu_audio_midi_out("General MIDI", GT_MIDI_SYNTH_GM, &config_audio.midi_gm_out, &config_audio.midi_gm_port);
    menu_audio_midi_out("MT-32", GT_MIDI_SYNTH_MT32, &config_audio.midi_mt32_out, &config_audio.midi_mt32_port);
    ImGui::Separator();
    menu_audio_midi_game_music();
    ImGui::Separator();

    MidiSynth* synth = emu_get_core()->GetAudio()->GetMidiSynth();
    const char* port = emu_midi_get_port();
    const char* wanted = emu_midi_get_wanted_port();

    if (!emu_get_core()->GetAudio()->GetMidiCard()->IsFitted())
        ImGui::TextColored(midi_stopped_color, "No MIDI card, choose one in Machine > MIDI Card");
    else if (synth->IsActive())
        ImGui::TextColored(midi_playing_color, "Playing on %s", emu_synth_get_label(synth->GetEngine()));
    else if (port[0] != 0)
        ImGui::TextColored(midi_playing_color, "Playing on %s", port);
    else if (wanted[0] != 0)
        ImGui::TextColored(midi_stopped_color, "%s not found", wanted);
    else
        ImGui::TextColored(midi_stopped_color, "No MIDI output");

    ImGui::EndMenu();
}

static void menu_audio_midi_out(const char* label, int synth, int* out, std::string* port)
{
    if (!ImGui::BeginMenu(label))
        return;

    int engine = emu_synth_get_engine(synth);
    bool mt32 = synth == GT_MIDI_SYNTH_MT32;
    bool shown[GT_MIDI_OUT_COUNT] = { emu_synth_is_compiled(engine), MidiPort::IsSupported(), true };
    const char* labels[GT_MIDI_OUT_COUNT] = { get_builtin_label(engine), k_midi_outs[GT_MIDI_OUT_EXTERNAL],
        k_midi_outs[GT_MIDI_OUT_NONE] };
    int current = CLAMP(*out, 0, GT_MIDI_OUT_COUNT - 1);
    float text_width = 0.0f;

    if (!shown[current])
        current = GT_MIDI_OUT_NONE;

    for (int i = 0; i < GT_MIDI_OUT_COUNT; i++)
    {
        if (shown[i])
            text_width = MAX(text_width, ImGui::CalcTextSize(labels[i]).x);
    }

    ImGui::PushItemWidth(gui_get_combo_width(text_width));

    if (ImGui::BeginCombo(mt32 ? "##midi_mt32_out" : "##midi_gm_out", labels[current]))
    {
        for (int i = 0; i < GT_MIDI_OUT_COUNT; i++)
        {
            if (shown[i] && ImGui::Selectable(labels[i], current == i))
            {
                *out = i;
                emu_midi_update_port();
            }
        }

        ImGui::EndCombo();
    }
    else if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Where %s music plays.", mt32 ? "MT-32" : "General MIDI");
        ImGui::Text("External Port: a synth outside Geartowns.");
        ImGui::Text("None or no %s: the %s output plays it.", mt32 ? "ROMs" : "sound bank",
            mt32 ? "General MIDI" : "MT-32");
        ImGui::EndTooltip();
    }

    ImGui::PopItemWidth();

    if (current == GT_MIDI_OUT_EXTERNAL)
        menu_audio_midi_port(mt32 ? "##midi_mt32_port" : "##midi_gm_port", port);

    ImGui::EndMenu();
}

// The list is read again each time the submenu opens, so a synth started after Geartowns shows up
static void menu_audio_midi_port(const char* id, std::string* port)
{
    if (!ImGui::BeginMenu("Port"))
        return;

    update_midi_ports();

    char missing[MIDI_PORT_NAME_SIZE + 16];
    const char* preview = k_midi_outs[GT_MIDI_OUT_NONE];
    float text_width = ImGui::CalcTextSize(preview).x;
    int selected = find_midi_port(port->c_str());

    for (int i = 0; i < (int)midi_ports.size(); i++)
        text_width = MAX(text_width, ImGui::CalcTextSize(midi_ports[i].id).x);

    if (selected >= 0)
        preview = midi_ports[selected].id;
    else if (!port->empty())
    {
        snprintf(missing, sizeof(missing), "%s (not found)", port->c_str());
        preview = missing;
        text_width = MAX(text_width, ImGui::CalcTextSize(missing).x);
    }

    ImGui::PushItemWidth(gui_get_combo_width(text_width));

    if (ImGui::BeginCombo(id, preview))
    {
        if (ImGui::Selectable(k_midi_outs[GT_MIDI_OUT_NONE], port->empty()))
        {
            port->clear();
            emu_midi_update_port();
        }

        for (int i = 0; i < (int)midi_ports.size(); i++)
        {
            ImGui::PushID(i);

            if (ImGui::Selectable(midi_ports[i].id, i == selected))
            {
                *port = midi_ports[i].id;
                emu_midi_update_port();
            }

            ImGui::PopID();
        }

        if (!port->empty() && (selected < 0))
            ImGui::Selectable(missing, true, ImGuiSelectableFlags_Disabled);

        ImGui::EndCombo();
    }
    else if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("A synth outside Geartowns.");
        ImGui::Text("Start it before opening this menu.");
        ImGui::EndTooltip();
    }

    ImGui::PopItemWidth();
    ImGui::EndMenu();
}

static void menu_audio_midi_game_music(void)
{
    if (!gui_begin_game_menu("Game Music", Emu_Game_Setting_MidiMusic))
        return;

    const char* labels[GT_MIDI_SYNTH_COUNT + 1] = { "Auto", k_midi_synths[GT_MIDI_SYNTH_GM].name,
        k_midi_synths[GT_MIDI_SYNTH_MT32].name };
    int music = emu_game_get(Emu_Game_Setting_MidiMusic);
    int known = emu_midi_get_known_synth();
    char preview[64];
    float text_width = 0.0f;
    snprintf(preview, sizeof(preview), "%s", labels[music]);

    if ((music == 0) && (known >= 0))
        snprintf(preview, sizeof(preview), "Auto (%s)", k_midi_synths[known].name);

    for (int i = 0; i <= GT_MIDI_SYNTH_COUNT; i++)
        text_width = MAX(text_width, ImGui::CalcTextSize(labels[i]).x);

    ImGui::PushItemWidth(gui_get_combo_width(MAX(text_width, ImGui::CalcTextSize(preview).x)));

    if (gui_begin_game_combo("##midi_game_music", preview, Emu_Game_Setting_MidiMusic))
    {
        for (int i = 0; i <= GT_MIDI_SYNTH_COUNT; i++)
        {
            if (ImGui::Selectable(labels[i], music == i))
            {
                emu_game_set(Emu_Game_Setting_MidiMusic, i);
                emu_midi_update_port();
            }
        }

        ImGui::EndCombo();
    }
    else if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("The game's music: General MIDI or MT-32.");
        ImGui::Text("Auto: from the game database or detected from what the game sends.");
        ImGui::Text("It plays on the matching output above.");
        gui_draw_game_source_tooltip(Emu_Game_Setting_MidiMusic);
        ImGui::EndTooltip();
    }

    ImGui::PopItemWidth();
    ImGui::EndMenu();
}

static void menu_audio_synths(void)
{
    bool mt32 = emu_synth_is_compiled(GT_MIDI_ENGINE_MT32);
    bool gm = emu_synth_is_compiled(GT_MIDI_ENGINE_SOUNDFONT);

    if (!ImGui::BeginMenu("Built-in Synths", mt32 || gm))
        return;

    menu_audio_synth_mt32();
    menu_audio_synth_gm();
    ImGui::EndMenu();
}

static void menu_audio_synth_mt32(void)
{
    if (!ImGui::BeginMenu("MT-32", emu_synth_is_compiled(GT_MIDI_ENGINE_MT32)))
        return;

    if (ImGui::BeginMenu("ROMs"))
    {
        const char* locations = "Default Location\0Custom Location\0\0";
        ImGui::PushItemWidth(gui_get_combo_width(ImGui::CalcTextSize("Custom Location").x));

        if (ImGui::Combo("##mt32_option", &config_audio.mt32_rom_dir_option, locations))
        {
            emu_synth_scan_mt32();
            emu_midi_update_port();
        }

        draw_synth_location_tooltip(GT_MIDI_ENGINE_MT32, config_audio.mt32_rom_dir_option,
            config_audio.mt32_rom_path.c_str(), "MT-32 or CM-32L control and PCM ROMs.\n"
            "Found by content, any file names.");
        ImGui::PopItemWidth();

        if (config_audio.mt32_rom_dir_option == 1)
        {
            if (ImGui::MenuItem("Choose..."))
                choose_mt32_path = true;

            ImGui::PushItemWidth(450);

            if (ImGui::InputText("##mt32_path", gui_mt32_path, IM_ARRAYSIZE(gui_mt32_path),
                ImGuiInputTextFlags_AutoSelectAll))
            {
                config_audio.mt32_rom_path.assign(gui_mt32_path);
            }

            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                emu_synth_scan_mt32();
                emu_midi_update_port();
            }

            ImGui::PopItemWidth();
        }

        ImGui::EndMenu();
    }

    menu_audio_synth_mt32_model();
    ImGui::Separator();
    bool found = emu_synth_is_available(GT_MIDI_ENGINE_MT32);
    ImGui::TextColored(found ? midi_playing_color : midi_stopped_color, "%s", emu_synth_get_mt32_status());

    if (found && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", emu_synth_get_mt32_folder());

    ImGui::EndMenu();
}

static void menu_audio_synth_mt32_model(void)
{
    if (!ImGui::BeginMenu("Model"))
        return;

    float text_width = 0.0f;

    for (int i = 0; i < GT_MT32_MODEL_COUNT; i++)
        text_width = MAX(text_width, ImGui::CalcTextSize(k_mt32_models[i].name).x);

    ImGui::PushItemWidth(gui_get_combo_width(text_width));

    if (ImGui::BeginCombo("##mt32_model", k_mt32_models[config_audio.mt32_model].name))
    {
        for (int i = 0; i < GT_MT32_MODEL_COUNT; i++)
        {
            if (ImGui::Selectable(k_mt32_models[i].name, config_audio.mt32_model == i))
            {
                config_audio.mt32_model = i;
                emu_synth_update();
            }
        }

        ImGui::EndCombo();
    }
    else if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Auto: CM-32L if found, otherwise MT-32.");
        ImGui::Text("Old: MT-32 ROMs 1.0x. New: 2.0x.");
        ImGui::Text("A model without its ROMs plays as Auto.");
        ImGui::EndTooltip();
    }

    ImGui::PopItemWidth();
    ImGui::EndMenu();
}

static void menu_audio_synth_gm(void)
{
    if (!ImGui::BeginMenu("General MIDI", emu_synth_is_compiled(GT_MIDI_ENGINE_SOUNDFONT)))
        return;

    if (ImGui::BeginMenu("Sound Bank"))
    {
        const char* locations = "Default Location\0Custom Location\0\0";
        ImGui::PushItemWidth(gui_get_combo_width(ImGui::CalcTextSize("Custom Location").x));

        if (ImGui::Combo("##soundfont_option", &config_audio.soundfont_option, locations))
        {
            emu_synth_find_soundfont();
            emu_midi_update_port();
        }

        draw_synth_location_tooltip(GT_MIDI_ENGINE_SOUNDFONT, config_audio.soundfont_option,
            config_audio.soundfont.c_str(), "A .sf2 or .sf3 General MIDI sound bank.\n"
            "From a folder, the first by name.");
        ImGui::PopItemWidth();

        if (config_audio.soundfont_option == 1)
        {
            if (ImGui::MenuItem("Choose..."))
                choose_soundfont = true;

            ImGui::PushItemWidth(450);

            if (ImGui::InputText("##soundfont_path", gui_soundfont_path, IM_ARRAYSIZE(gui_soundfont_path),
                ImGuiInputTextFlags_AutoSelectAll))
            {
                config_audio.soundfont.assign(gui_soundfont_path);
            }

            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                emu_synth_find_soundfont();
                emu_midi_update_port();
            }

            ImGui::PopItemWidth();
        }

        ImGui::EndMenu();
    }

    ImGui::Separator();
    bool found = emu_synth_is_available(GT_MIDI_ENGINE_SOUNDFONT);
    ImGui::TextColored(found ? midi_playing_color : midi_stopped_color, "%s", emu_synth_get_soundfont_status());

    if (found && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", emu_synth_get_soundfont());

    ImGui::EndMenu();
}

static void draw_synth_location_tooltip(int engine, int option, const char* custom, const char* about)
{
    if (!ImGui::IsItemHovered())
        return;

    ImGui::BeginTooltip();
    ImGui::Text("%s", about);

    if (option == 1)
        ImGui::Text("%s", custom);
    else
    {
        std::vector<std::string> folders;
        emu_synth_get_default_folders(engine, folders);

        for (size_t i = 0; i < folders.size(); i++)
            ImGui::Text("%s", folders[i].c_str());
    }

    ImGui::EndTooltip();
}

static const char* get_builtin_label(int engine)
{
    static char labels[GT_MIDI_ENGINE_COUNT][64];
    const char* label = emu_synth_get_label(engine);

    if (!emu_synth_is_compiled(engine) || emu_synth_is_available(engine))
        return label;

    snprintf(labels[engine], sizeof(labels[engine]), "%s (%s)", label,
        engine == GT_MIDI_ENGINE_MT32 ? "no ROMs" : "no sound bank");
    return labels[engine];
}

static int find_midi_port(const char* port)
{
    for (int i = 0; (port[0] != 0) && (i < (int)midi_ports.size()); i++)
    {
        if (MidiPort::IsSamePort(port, midi_ports[i].id))
            return i;
    }

    return -1;
}

static void update_midi_ports(void)
{
    if (ImGui::IsWindowAppearing())
        midi_ports_pending = true;

    if (midi_ports_pending && emu_midi_prepare())
    {
        MidiPort::ListPorts(midi_ports);
        midi_ports_pending = false;
    }
}

static void menu_debug(void)
{
#if !defined(GT_DISABLE_DISASSEMBLER)
    if (ImGui::BeginMenu("Debug"))
    {
        gui_in_use = true;

        ImGui::MenuItem("Enable", "", &config_debug.debug);

        ImGui::Separator();

        if (ImGui::MenuItem("Save Debug Settings...", "", false, config_debug.debug))
        {
            gui_file_dialog_save_debug_settings();
        }

        if (ImGui::MenuItem("Load Debug Settings...", "", false, config_debug.debug))
        {
            gui_file_dialog_load_debug_settings();
        }

        ImGui::MenuItem("Auto Save/Load Debug Settings", "", &config_debug.auto_debug_settings, config_debug.debug);

        ImGui::Separator();

        bool can_reload = config_debug.debug && !emu_is_empty() && emu_get_core()->GetMedia()->IsReady();

        if (ImGui::MenuItem("Reload CD-ROM and Reset", config_hotkeys[config_HotkeyIndex_ReloadROM].str, false, can_reload))
        {
            gui_action_reload_rom();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("MCP Server", config_debug.debug))
        {
            bool mcp_running = emu_mcp_is_running();
            int transport_mode = emu_mcp_get_transport_mode();
            bool http_running = mcp_running && (transport_mode == 1);
            bool stdio_running = mcp_running && (transport_mode == 0);

            if (ImGui::MenuItem("Start HTTP Server", "", false, !mcp_running))
            {
                if (strlen(gui_mcp_http_address) == 0)
                    strncpy_fit(gui_mcp_http_address, "127.0.0.1", sizeof(gui_mcp_http_address));

                config_emulator.mcp_http_address = gui_mcp_http_address;
                emu_mcp_set_transport(1, config_emulator.mcp_tcp_port, config_emulator.mcp_http_address.c_str());
                emu_mcp_start();
            }

            if (ImGui::MenuItem("Stop HTTP Server", "", false, http_running))
            {
                emu_mcp_stop();
            }

            ImGui::Separator();

            if (stdio_running)
                ImGui::TextColored(service_mcp_stdio_color, "STDIO mode active");
            else if (http_running)
                ImGui::TextColored(service_mcp_http_color, "Listening on %s:%d", emu_mcp_get_http_address(),
                    emu_mcp_get_http_port());
            else
                ImGui::TextColored(red, "Stopped");

            ImGui::Separator();

            ImGui::Text("HTTP Address:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);

            if (ImGui::InputText("##mcp_address", gui_mcp_http_address, IM_ARRAYSIZE(gui_mcp_http_address), ImGuiInputTextFlags_AutoSelectAll))
                config_emulator.mcp_http_address = gui_mcp_http_address;

            ImGui::Text("HTTP Port:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50);

            if (ImGui::InputInt("##mcp_port", &config_emulator.mcp_tcp_port, 0, 0))
            {
                if (config_emulator.mcp_tcp_port < 1)
                    config_emulator.mcp_tcp_port = 1;

                if (config_emulator.mcp_tcp_port > 65535)
                    config_emulator.mcp_tcp_port = 65535;
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Debug Output Screen", config_debug.debug))
        {
            ImGui::MenuItem("Show Output Screen", "", &config_debug.show_screen, config_debug.debug);

            if (ImGui::BeginMenu("Scale", config_debug.debug))
            {
                ImGui::PushItemWidth(200.0f);
                ImGui::SliderInt("##debug_scale", &config_debug.scale, 1, 10);
                ImGui::PopItemWidth();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Aspect Ratio", config_debug.debug))
            {
                ImGui::PushItemWidth(190.0f);
                ImGui::Combo("##debug_ratio", &config_debug.ratio,
                    "Square Pixels (1:1 PAR)\0Standard (4:3 DAR)\0Wide (16:9 DAR)\0Wide (16:10 DAR)\0\0");
                ImGui::PopItemWidth();
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::MenuItem("Show Disassembler", "", &config_debug.show_disassembler, config_debug.debug);
        ImGui::MenuItem("Show Memory Workspace", "", &config_debug.show_memory, config_debug.debug);
        ImGui::MenuItem("Show Trace Logger", "", &config_debug.show_trace_logger, config_debug.debug);
        ImGui::MenuItem("Show Profiler", "", &config_debug.show_profiler, config_debug.debug);

        ImGui::Separator();

        char cpu_menu[64];
        snprintf(cpu_menu, sizeof(cpu_menu), "%s###debug_cpu", gui_debug_i386_cpu_name());

        if (ImGui::BeginMenu(cpu_menu, config_debug.debug))
        {
            ImGui::MenuItem("Show Status", "", &config_debug.show_processor);
            ImGui::MenuItem("Show System Registers", "", &config_debug.show_processor_details);
            ImGui::MenuItem("Show Descriptor Tables", "", &config_debug.show_i386_descriptors);
            ImGui::MenuItem("Show Paging", "", &config_debug.show_i386_paging);
            ImGui::MenuItem("Show Call Stack", "", &config_debug.show_call_stack);
            ImGui::MenuItem("Show Breakpoints", "", &config_debug.show_breakpoints);
            ImGui::MenuItem("Show Symbols", "", &config_debug.show_symbols);
            ImGui::EndMenu();
        }

        char fpu_menu[64];
        snprintf(fpu_menu, sizeof(fpu_menu), "%s###debug_fpu", gui_debug_i386_fpu_name());

        if (ImGui::BeginMenu(fpu_menu, config_debug.debug))
        {
            ImGui::MenuItem("Show Status", "", &config_debug.show_x87);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Video", config_debug.debug))
        {
            ImGui::MenuItem("Show CRTC", "", &config_debug.show_crtc);
            ImGui::MenuItem("Show CRTC Registers", "", &config_debug.show_crtc_registers);
            ImGui::MenuItem("Show High-Res CRTC", "", &config_debug.show_hires_crtc);
            ImGui::MenuItem("Show Output Control", "", &config_debug.show_video_output);
            ImGui::MenuItem("Show Palettes", "", &config_debug.show_palettes);
            ImGui::MenuItem("Show Framebuffers", "", &config_debug.show_framebuffers);
            ImGui::MenuItem("Show Sprites", "", &config_debug.show_sprites);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Audio", config_debug.debug))
        {
            ImGui::MenuItem("Show YM3438 FM", "", &config_debug.show_ym3438);
            ImGui::MenuItem("Show YM3438 Registers", "", &config_debug.show_ym3438_registers);
            ImGui::MenuItem("Show RF5C68 PCM", "", &config_debug.show_rf5c68);
            ImGui::MenuItem("Show 16-bit PCM", "", &config_debug.show_pcm16);
            ImGui::MenuItem("Show MIDI", "", &config_debug.show_midi);
            ImGui::MenuItem("Show Sound Control", "", &config_debug.show_sound_control);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("CD-ROM", config_debug.debug))
        {
            ImGui::MenuItem("Show Controller", "", &config_debug.show_cdrom);
            ImGui::MenuItem("Show TOC", "", &config_debug.show_cdrom_toc);
            ImGui::MenuItem("Show CD Audio", "", &config_debug.show_cdrom_audio);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Floppy", config_debug.debug))
        {
            ImGui::MenuItem("Show FDC", "", &config_debug.show_fdc);
            ImGui::MenuItem("Show Drives", "", &config_debug.show_floppy_drives);
            ImGui::MenuItem("Show Disk Viewer", "", &config_debug.show_disk_viewer);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("SCSI", config_debug.debug))
        {
            ImGui::MenuItem("Show Controller", "", &config_debug.show_scsi);
            ImGui::MenuItem("Show Disks", "", &config_debug.show_scsi_disks);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("System", config_debug.debug))
        {
            ImGui::MenuItem("Show Interrupts", "", &config_debug.show_pic);
            ImGui::MenuItem("Show Timers", "", &config_debug.show_pit);
            ImGui::MenuItem("Show DMA", "", &config_debug.show_dma);
            ImGui::MenuItem("Show RTC", "", &config_debug.show_rtc);
            ImGui::MenuItem("Show System Control", "", &config_debug.show_system_control);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Input", config_debug.debug))
        {
            ImGui::MenuItem("Show Keyboard", "", &config_debug.show_keyboard);
            ImGui::MenuItem("Show Game Ports", "", &config_debug.show_game_ports);
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::MenuItem("Show Rewind", "", &config_debug.show_rewind, config_debug.debug);

        ImGui::Separator();

#if defined(__APPLE__) || defined(_WIN32)
        ImGui::MenuItem("Multi-Viewport", "", &config_debug.multi_viewport, config_debug.debug);

        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("RESTART REQUIRED");
            ImGui::NewLine();
            ImGui::Text("Enables docking of debug windows outside of main window.");
            ImGui::EndTooltip();
        }
#endif

        ImGui::Separator();

        if (ImGui::BeginMenu("Font", config_debug.debug))
        {
            if (ImGui::BeginMenu("Type"))
            {
                ImGui::PushItemWidth(140.0f);

                if (ImGui::Combo("##font_type", &config_debug.font_type, "ProggyForever\0ProggyClean\0\0"))
                {
                    gui_default_font = gui_default_fonts[config_debug.font_type][config_debug.font_size];
                }

                ImGui::PopItemWidth();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Size"))
            {
                ImGui::PushItemWidth(110.0f);

                if (ImGui::Combo("##font_size", &config_debug.font_size, "Small\0Medium\0Big\0Large\0\0"))
                {
                    gui_default_font = gui_default_fonts[config_debug.font_type][config_debug.font_size];
                }

                ImGui::PopItemWidth();
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }
#endif
}

static void menu_about(void)
{
    if (ImGui::BeginMenu("About"))
    {
        gui_in_use = true;

        if (ImGui::MenuItem("About " GT_TITLE " " GT_VERSION " ..."))
        {
            open_about = true;
        }

        ImGui::EndMenu();
    }
}

static void draw_mcp_status(void)
{
    bool video_recording = emu_is_video_recording();
    bool vgm_recording = emu_is_vgm_recording();
    bool mcp_running = emu_mcp_is_running();

    if (!video_recording && !vgm_recording && !mcp_running)
        return;

    const char* video_recording_status = "REC VIDEO";
    const char* vgm_recording_status = "REC VGM";
    char mcp_status[128];
    ImVec4 mcp_color = service_mcp_http_color;

    if (mcp_running)
    {
        int transport_mode = emu_mcp_get_transport_mode();

        if (transport_mode == 0)
        {
            snprintf(mcp_status, sizeof(mcp_status), "MCP: STDIO");
            mcp_color = service_mcp_stdio_color;
        }
        else
        {
            snprintf(mcp_status, sizeof(mcp_status), "MCP: HTTP (%s:%d)", emu_mcp_get_http_address(), emu_mcp_get_http_port());
        }
    }

    ImGuiStyle& style = ImGui::GetStyle();
    float spacing = style.ItemSpacing.x * 2.0f;
    float dot_radius = ImGui::GetFontSize() * 0.22f;
    float dot_width = (dot_radius * 2.0f) + style.ItemInnerSpacing.x;
    float text_width = 0.0f;

    if (video_recording)
        text_width += dot_width + ImGui::CalcTextSize(video_recording_status).x;
    if (vgm_recording)
    {
        if (text_width > 0.0f)
            text_width += spacing;
        text_width += dot_width + ImGui::CalcTextSize(vgm_recording_status).x;
    }
    if (mcp_running)
    {
        if (text_width > 0.0f)
            text_width += spacing;
        text_width += ImGui::CalcTextSize(mcp_status).x;
    }

    float status_x = ImGui::GetWindowWidth() - text_width -
        style.ItemSpacing.x - 10.0f;
    float cursor_x = ImGui::GetCursorPosX();

    if (status_x <= cursor_x + style.ItemSpacing.x)
        return;

    ImGui::SameLine(status_x);
    ImGui::AlignTextToFramePadding();

    if (video_recording)
        draw_recording_status(video_recording_status, video_recording_color, dot_radius, dot_width);

    if (vgm_recording)
    {
        if (video_recording)
            ImGui::SameLine(0.0f, spacing);

        draw_recording_status(vgm_recording_status, vgm_recording_color, dot_radius, dot_width);
    }

    if (mcp_running)
    {
        if (video_recording || vgm_recording)
            ImGui::SameLine(0.0f, spacing);

        ImGui::TextColored(mcp_color, "%s", mcp_status);
    }
}

static void draw_recording_status(const char* status, ImVec4 color, float dot_radius, float dot_width)
{
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + dot_width);
    ImGui::TextColored(color, "%s", status);

    ImVec2 text_min = ImGui::GetItemRectMin();
    ImVec2 text_max = ImGui::GetItemRectMax();
    ImVec2 dot_center = ImVec2(text_min.x - dot_width + dot_radius, (text_min.y + text_max.y) * 0.5f);
    ImGui::GetWindowDrawList()->AddCircleFilled(dot_center, dot_radius, ImGui::GetColorU32(color));
}

static void file_dialogs(void)
{
    gui_file_dialog_process_results();

    if (open_rom || gui_shortcut_open_rom)
    {
        gui_shortcut_open_rom = false;
        gui_file_dialog_open_rom();
    }

    if (open_state)
        gui_file_dialog_load_state();

    if (save_state)
        gui_file_dialog_save_state();

    if (save_screenshot)
        gui_file_dialog_save_screenshot();

    if (save_vgm)
        gui_file_dialog_save_vgm();

    if (save_video)
        gui_file_dialog_save_video();

    if (choose_savestates_path)
        gui_file_dialog_choose_savestate_path();

    if (choose_savefiles_path)
        gui_file_dialog_choose_savefiles_path();

    if (choose_mt32_path)
        gui_file_dialog_choose_mt32_path();

    if (choose_soundfont)
        gui_file_dialog_choose_soundfont();

    if (choose_screenshots_path)
        gui_file_dialog_choose_screenshot_path();

    if (choose_video_recordings_path)
        gui_file_dialog_choose_video_recording_path();

#if defined(GT_ENABLE_PHYSICAL_CDROM)
    if (open_physical_cdrom)
        gui_popup_open_physical_cdrom();
#endif

    if (open_about)
    {
        gui_dialog_in_use = true;
        ImGui::OpenPopup("About " GT_TITLE);
    }

    if (open_load_defaults)
    {
        gui_dialog_in_use = true;
        ImGui::OpenPopup("Load Default Settings");
    }

    gui_popup_modal_about();
    gui_popup_modal_load_defaults();
    gui_floppy_popups();
    gui_hard_disks_popups();
    gui_memory_card_popups();
#if defined(GT_ENABLE_PHYSICAL_CDROM)
    gui_popup_modal_physical_cdrom();
#endif
}

static const char* get_current_media_directory_text(void)
{
#if defined(GT_ENABLE_PHYSICAL_CDROM)
    if (!emu_is_empty() && emu_get_core()->GetMedia()->IsPhysicalCdRom())
        return config_root_path;
#endif

    return emu_get_core()->GetMedia()->GetFileDirectory();
}

static void keyboard_configuration_item(const char* text, SDL_Scancode* key, int player)
{
    ImGui::Text("%s", text);
    ImGui::SameLine(120);

    char button_label[256];
    snprintf(button_label, 256, "%s##%p%d", SDL_GetKeyName(SDL_GetKeyFromScancode(*key, SDL_KMOD_NONE, false)), (void*)key, player);

    if (ImGui::Button(button_label, ImVec2(90,0)))
    {
        gui_configured_key = key;
        ImGui::OpenPopup("Keyboard Configuration");
    }

    ImGui::SameLine();

    char remove_label[256];
    snprintf(remove_label, sizeof(remove_label), "X##rk%p%d", (void*)key, player);

    if (ImGui::Button(remove_label))
    {
        *key = SDL_SCANCODE_UNKNOWN;
    }
}

static void gamepad_configuration_item(const char* text, int* button, int player)
{
    ImGui::Text("%s", text);
    ImGui::SameLine(140);

    const char* button_name = "";

    if (*button == SDL_GAMEPAD_BUTTON_INVALID)
    {
        button_name = "";
    }
    else if (*button >= 0 && *button < SDL_GAMEPAD_BUTTON_COUNT)
    {
        static const char* gamepad_names[21] = {"A", "B", "X" ,"Y", "BACK", "GUIDE", "START", "L3", "R3", "L1", "R1", "UP", "DOWN", "LEFT", "RIGHT", "MISC", "PAD1", "PAD2", "PAD3", "PAD4", "TOUCH"};
        button_name = gamepad_names[*button];
    }
    else if (*button >= GAMEPAD_VBTN_AXIS_BASE)
    {
        int axis = *button - GAMEPAD_VBTN_AXIS_BASE;

        if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
            button_name = "L2";
        else if (axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
            button_name = "R2";
        else
            button_name = "??";
    }

    char button_label[256];
    snprintf(button_label, sizeof(button_label), "%s##%p%d", button_name, (void*)button, player);

    if (ImGui::Button(button_label, ImVec2(70,0)))
    {
        gui_configured_button = button;
        ImGui::OpenPopup("Gamepad Configuration");
    }

    ImGui::SameLine();

    char remove_label[256];
    snprintf(remove_label, sizeof(remove_label), "X##rg%p%d", (void*)button, player);

    if (ImGui::Button(remove_label))
    {
        *button = SDL_GAMEPAD_BUTTON_INVALID;
    }
}

static void hotkey_configuration_item(const char* text, config_Hotkey* hotkey)
{
    ImGui::Text("%s", text);
    ImGui::SameLine(200);

    char button_label[256];
    snprintf(button_label, sizeof(button_label), "%s##%s", hotkey->str[0] != '\0' ? hotkey->str : "<None>", text);

    if (ImGui::Button(button_label, ImVec2(150,0)))
    {
        gui_configured_hotkey = hotkey;
        ImGui::OpenPopup("Hotkey Configuration");
    }

    ImGui::SameLine();

    char remove_label[256];
    snprintf(remove_label, sizeof(remove_label), "X##rh%s", text);

    if (ImGui::Button(remove_label))
    {
        hotkey->key = SDL_SCANCODE_UNKNOWN;
        hotkey->mod = SDL_KMOD_NONE;
        config_update_hotkey_string(hotkey);
    }
}

static void hotkey_configuration_menu(bool debug)
{
    for (int i = 0; i < config_HotkeyIndex_COUNT; i++)
    {
        if (((hotkey_definitions[i].flags & HOTKEY_FLAG_DEBUG) != 0) != debug)
            continue;

        char label[64];
        snprintf(label, sizeof(label), "%s:", hotkey_definitions[i].label);
        hotkey_configuration_item(label, &config_hotkeys[i]);
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Reset to Defaults"))
        hotkeys_reset_defaults(debug);

    gui_popup_modal_hotkey();
}

static void gamepad_device_selector(int player)
{
    if (player < 0 || player >= GT_MAX_GAMEPADS)
        return;

    const int max_detected_gamepads = 32;
    SDL_JoystickID id_map[max_detected_gamepads];
    id_map[0] = 0;
    int count = 1;

    std::string items;
    items.reserve(4096);
    items.append("<None>");
    items.push_back('\0');
    float text_width = ImGui::CalcTextSize("<None>").x;

    Gamepad_Detected_Info detected[max_detected_gamepads];
    int num_detected = gamepad_get_detected(detected, max_detected_gamepads);

    SDL_JoystickID current_id = 0;

    if (IsValidPointer(gamepad_controller[player]))
        current_id = SDL_GetJoystickID(SDL_GetGamepadJoystick(gamepad_controller[player]));

    int selected = 0;

    for (int i = 0; i < num_detected && count < max_detected_gamepads; i++)
    {
        const char* name = detected[i].name;

        if (!IsValidPointer(name))
            name = "Unknown Gamepad";

        id_map[count] = detected[i].id;

        if (current_id == detected[i].id)
            selected = count;

        size_t len = strlen(detected[i].guid_str);
        const char* id_8 = detected[i].guid_str + (len > 8 ? len - 8 : 0);

        char label[192];
        snprintf(label, sizeof(label), "%s (ID: %s)", name, id_8);

        items.append(label);
        items.push_back('\0');
        text_width = MAX(text_width, ImGui::CalcTextSize(label).x);
        count++;
    }

    items.push_back('\0');

    char label[32];
    snprintf(label, sizeof(label), "##device_player%d", player + 1);

    ImGui::PushItemWidth(gui_get_combo_width(text_width));

    if (ImGui::Combo(label, &selected, items.c_str()))
    {
        SDL_JoystickID instance_id = id_map[selected];
        gamepad_assign(player, instance_id);
    }

    ImGui::PopItemWidth();
}

static void draw_savestate_slot_info(int slot)
{
    if (slot < 0 || slot >= 5)
        return;

    if (emu_savestates[slot].magic == GT_SAVESTATE_MAGIC)
    {
        if (emu_savestates[slot].version < GT_SAVESTATE_MIN_VERSION || emu_savestates[slot].version > GT_SAVESTATE_VERSION)
        {
            ImGui::TextColored(red, "This savestate is from an older version and will not work");

            if (emu_savestates[slot].emu_build[0] != 0)
            {
                ImGui::TextColored(red, "Use %s - %s", GT_TITLE, emu_savestates[slot].emu_build);
            }

            ImGui::Separator();
        }

        ImGui::Text("%s", emu_savestates[slot].rom_name);
        char date[64];
        get_date_time_string(emu_savestates[slot].timestamp, date, sizeof(date));
        ImGui::Text("%s", date);

        if (IsValidPointer(emu_savestates_screenshots[slot].data))
        {
            float width = (float)emu_savestates_screenshots[slot].width;
            float height = (float)emu_savestates_screenshots[slot].height;
            ImGui::Image((ImTextureID)(intptr_t)ogl_renderer_emu_savestates, ImVec2(320.0f, 240.0f),
                ImVec2(0, 0), ImVec2(width / (float)SYSTEM_TEXTURE_WIDTH, height / (float)SYSTEM_TEXTURE_HEIGHT));
        }
    }
    else
    {
        ImGui::TextColored(gray, "Slot %d is empty", slot + 1);
    }
}
