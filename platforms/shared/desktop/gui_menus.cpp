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
#include "gui_debug_disassembler.h"
#include "config.h"
#include "application.h"
#include "display.h"
#include "gamepad.h"
#include "emu.h"
#include "ogl_renderer.h"
#include "ogl_shader_chain.h"
#include "shader_preset.h"
#include "utils.h"
#include "geartowns.h"
#include "rewind.h"

static bool open_rom = false;
static bool open_ram = false;
static bool save_ram = false;
static bool open_state = false;
static bool save_state = false;
static bool open_about = false;
static bool open_load_defaults = false;
static bool save_screenshot = false;
static bool choose_savestates_path = false;
static bool choose_screenshots_path = false;
static bool open_bios = false;
static bool save_debug_settings = false;
static bool load_debug_settings = false;
#if defined(GG_ENABLE_PHYSICAL_CDROM)
static bool open_physical_cdrom = false;
#endif
static ShaderPresetInfo shader_presets[SHADER_PRESET_MAX_DISCOVERED];
static int shader_preset_count = 0;

static void menu_geartowns(void);
static void menu_emulator(void);
static void menu_video(void);
static void menu_shader(void);
static void draw_shader_parameters(void);
static bool shader_parameter_is_toggle(const ShaderPresetParameter* parameter);
static bool shader_parameter_is_integer(const ShaderPresetParameter* parameter);
static int shader_parameter_round_to_int(float value);
static void menu_input(void);
static void menu_audio(void);
static void menu_debug(void);
static void menu_about(void);
static void draw_background_color_menu(const char* label, int theme);
static void draw_mcp_status(void);
static void file_dialogs(void);
static bool media_menu_actions_enabled(void);
static const char* get_current_media_directory_text(void);
static void keyboard_configuration_item(const char* text, SDL_Scancode* key, int player);
static void gamepad_configuration_item(const char* text, int* button, int player);
static void hotkey_configuration_item(const char* text, config_Hotkey* hotkey);
static void gamepad_device_selector(int player);
static void draw_savestate_slot_info(int slot);

void gui_init_menus(void)
{
    gui_shortcut_open_rom = false;
    shader_preset_count = shader_preset_scan_bundled(shader_presets, SHADER_PRESET_MAX_DISCOVERED);
}

void gui_main_menu(void)
{
    open_rom = false;
    open_ram = false;
    save_ram = false;
    open_state = false;
    save_state = false;
    open_about = false;
    open_load_defaults = false;
    save_screenshot = false;
    choose_savestates_path = false;
    choose_screenshots_path = false;
    gui_main_menu_hovered = false;
    open_bios = false;
    save_debug_settings = false;
    load_debug_settings = false;
#if defined(GG_ENABLE_PHYSICAL_CDROM)
    open_physical_cdrom = false;
#endif

    if (application_show_menu && ImGui::BeginMainMenuBar())
    {
        gui_main_menu_hovered = ImGui::IsWindowHovered();

        menu_geartowns();
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
        bool media_actions_enabled = media_menu_actions_enabled();

        if (ImGui::MenuItem("Open Media...", config_hotkeys[config_HotkeyIndex_OpenROM].str))
        {
            open_rom = true;
        }

#if defined(GG_ENABLE_PHYSICAL_CDROM)
        if (ImGui::BeginMenu("Physical CD-ROM"))
        {
            bool physical_cdrom_loaded = !emu_is_empty() && emu_get_core()->GetMedia()->IsPhysicalCdRom();
            if (ImGui::MenuItem("Open...", "", false, !physical_cdrom_loaded))
            {
                open_physical_cdrom = true;
            }

            if (ImGui::MenuItem("Eject", "", false, physical_cdrom_loaded))
            {
                gui_action_eject_physical_cdrom();
            }

            ImGui::EndMenu();
        }
#endif

        if (ImGui::BeginMenu("Open Recent"))
        {
            for (int i = 0; i < config_max_recent_roms; i++)
            {
                if (config_emulator.recent_roms[i].length() > 0)
                {
                    const char* shortcut = (i == 0) ? config_hotkeys[config_HotkeyIndex_ReloadROM].str : NULL;
                    if (ImGui::MenuItem(config_emulator.recent_roms[i].c_str(), shortcut))
                    {
                        char media_path[4096];
                        strncpy_fit(media_path, config_emulator.recent_roms[i].c_str(), sizeof(media_path));
                        gui_load_rom(media_path);
                    }
                }
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Reset", config_hotkeys[config_HotkeyIndex_Reset].str, false, media_actions_enabled))
        {
            gui_action_reset();
        }

        if (ImGui::MenuItem("Pause", config_hotkeys[config_HotkeyIndex_Pause].str, &config_emulator.paused, media_actions_enabled))
        {
            gui_action_pause();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Fast Forward", config_hotkeys[config_HotkeyIndex_FFWD].str, &config_emulator.ffwd, media_actions_enabled))
        {
            gui_action_ffwd();
        }

        if (ImGui::BeginMenu("Fast Forward Speed"))
        {
            ImGui::PushItemWidth(100.0f);
            ImGui::Combo("##fwd", &config_emulator.ffwd_speed, "X 1.5\0X 2\0X 2.5\0X 3\0Unlimited\0\0");
            ImGui::PopItemWidth();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Rewind"))
        {
            if (ImGui::MenuItem("Enabled", config_hotkeys[config_HotkeyIndex_Rewind].str, &config_rewind.enabled))
                rewind_reset();

            ImGui::PushItemWidth(140.0f);
            ImGui::SliderFloat("Speed", &config_rewind.speed, 1.0f, 8.0f, "%.0fx");
            ImGui::PopItemWidth();

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
                ImGui::Text("Ignored while fast-forwarding or with a physical CD-ROM.");
                ImGui::EndTooltip();
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save BRAM As...", "", false, media_actions_enabled))
        {
            save_ram = true;
        }

        if (ImGui::MenuItem("Load BRAM From...", "", false, media_actions_enabled))
        {
            open_ram = true;
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save State As...", "", false, media_actions_enabled))
        {
            save_state = true;
        }

        if (ImGui::MenuItem("Load State From...", "", false, media_actions_enabled))
        {
            open_state = true;
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Save State Slot"))
        {
            ImGui::PushItemWidth(100.0f);
            ImGui::Combo("##slot", &config_emulator.save_slot, "Slot 1\0Slot 2\0Slot 3\0Slot 4\0Slot 5\0\0");
            ImGui::PopItemWidth();

            ImGui::Separator();
            draw_savestate_slot_info(config_emulator.save_slot);

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Save State", config_hotkeys[config_HotkeyIndex_SaveState].str, false, media_actions_enabled))
        {
            std::string message("Saving state to slot ");
            message += std::to_string(config_emulator.save_slot + 1);
            gui_set_status_message(message.c_str(), 3000);
            emu_save_state_slot(config_emulator.save_slot + 1);
        }

        if (ImGui::MenuItem("Load State", config_hotkeys[config_HotkeyIndex_LoadState].str, false, media_actions_enabled))
        {
            std::string message("Loading state from slot ");
            message += std::to_string(config_emulator.save_slot + 1);
            gui_set_status_message(message.c_str(), 3000);
            emu_load_state_slot(config_emulator.save_slot + 1);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Slot: %d", config_emulator.save_slot + 1);
            ImGui::Separator();
            draw_savestate_slot_info(config_emulator.save_slot);
            ImGui::EndTooltip();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save Screenshot As...", "", false, media_actions_enabled))
        {
            save_screenshot = true;
        }

        if (ImGui::MenuItem("Save Screenshot", config_hotkeys[config_HotkeyIndex_Screenshot].str, false, media_actions_enabled))
        {
            gui_action_save_screenshot(NULL);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Load Default Settings"))
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

static bool media_menu_actions_enabled(void)
{
    if (emu_is_empty())
        return false;

#if defined(GG_ENABLE_PHYSICAL_CDROM)
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

        if (ImGui::BeginMenu("Save States Dir"))
        {
            ImGui::PushItemWidth(220.0f);
            if (ImGui::Combo("##savestate_option", &config_emulator.savestates_dir_option, "Default Location\0Same as ROM\0Custom Location\0\0"))
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
                    {
                        choose_savestates_path = true;
                    }

                    ImGui::PushItemWidth(450);
                    if (ImGui::InputText("##savestate_path", gui_savestates_path, IM_ARRAYSIZE(gui_savestates_path), ImGuiInputTextFlags_AutoSelectAll))
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

        if (ImGui::BeginMenu("Screenshots Dir"))
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

        ImGui::Separator();

        if (ImGui::BeginMenu("BIOS"))
        {
            if (ImGui::BeginMenu("..."))
            {
                if (ImGui::MenuItem("Load BIOS..."))
                {
                    open_syscard_bios = true;
                }
                ImGui::PushItemWidth(350);
                if (ImGui::InputText("##bios_path", gui_syscard_bios_path, IM_ARRAYSIZE(gui_syscard_bios_path), ImGuiInputTextFlags_AutoSelectAll))
                {
                    config_emulator.syscard_bios_path.assign(gui_syscard_bios_path);
                    gui_load_bios(gui_syscard_bios_path, true);
                }
                ImGui::PopItemWidth();

                ImGui::Separator();
                if (emu_get_core()->GetMedia()->IsValidBios(true))
                {
                    ImGui::TextColored(ImVec4(0.10f, 0.90f, 0.10f, 1.0f), "Valid BIOS: %s", emu_get_core()->GetMedia()->GetBiosName(true));
                }
                else
                {
                    ImGui::TextColored(ImVec4(0.98f, 0.15f, 0.45f, 1.0f), "System Card BIOS not loaded or invalid!");
                    ImGui::TextColored(ImVec4(0.98f, 0.15f, 0.45f, 1.0f), "System Card 3.0 recommended for most games.");
                }

                ImGui::EndMenu();
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

        ImGui::Separator();

        ImGui::MenuItem("Show ROM info", "", &config_emulator.show_info);
        ImGui::MenuItem("Status Messages", "", &config_emulator.status_messages);

        ImGui::Separator();

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

        if (ImGui::BeginMenu("Hotkeys"))
        {
            hotkey_configuration_item("Open Media:", &config_hotkeys[config_HotkeyIndex_OpenROM]);
            hotkey_configuration_item("Quit:", &config_hotkeys[config_HotkeyIndex_Quit]);
            hotkey_configuration_item("Reset:", &config_hotkeys[config_HotkeyIndex_Reset]);
            hotkey_configuration_item("Reload Media:", &config_hotkeys[config_HotkeyIndex_ReloadROM]);
            hotkey_configuration_item("Pause:", &config_hotkeys[config_HotkeyIndex_Pause]);
            hotkey_configuration_item("Fast Forward:", &config_hotkeys[config_HotkeyIndex_FFWD]);
            hotkey_configuration_item("Rewind:", &config_hotkeys[config_HotkeyIndex_Rewind]);
            hotkey_configuration_item("Save State:", &config_hotkeys[config_HotkeyIndex_SaveState]);
            hotkey_configuration_item("Load State:", &config_hotkeys[config_HotkeyIndex_LoadState]);
            hotkey_configuration_item("Save State Slot 1:", &config_hotkeys[config_HotkeyIndex_SelectSlot1]);
            hotkey_configuration_item("Save State Slot 2:", &config_hotkeys[config_HotkeyIndex_SelectSlot2]);
            hotkey_configuration_item("Save State Slot 3:", &config_hotkeys[config_HotkeyIndex_SelectSlot3]);
            hotkey_configuration_item("Save State Slot 4:", &config_hotkeys[config_HotkeyIndex_SelectSlot4]);
            hotkey_configuration_item("Save State Slot 5:", &config_hotkeys[config_HotkeyIndex_SelectSlot5]);
            hotkey_configuration_item("Screenshot:", &config_hotkeys[config_HotkeyIndex_Screenshot]);
            hotkey_configuration_item("Mute Audio:", &config_hotkeys[config_HotkeyIndex_Mute]);
            hotkey_configuration_item("Fullscreen:", &config_hotkeys[config_HotkeyIndex_Fullscreen]);
            hotkey_configuration_item("Show Main Menu:", &config_hotkeys[config_HotkeyIndex_ShowMainMenu]);
            hotkey_configuration_item("Capture Mouse:", &config_hotkeys[config_HotkeyIndex_CaptureMouse]);

            gui_popup_modal_hotkey();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug Hotkeys"))
        {
            hotkey_configuration_item("Step Into:", &config_hotkeys[config_HotkeyIndex_DebugStepInto]);
            hotkey_configuration_item("Step Over:", &config_hotkeys[config_HotkeyIndex_DebugStepOver]);
            hotkey_configuration_item("Step Out:", &config_hotkeys[config_HotkeyIndex_DebugStepOut]);
            hotkey_configuration_item("Step Frame:", &config_hotkeys[config_HotkeyIndex_DebugStepFrame]);
            hotkey_configuration_item("Continue:", &config_hotkeys[config_HotkeyIndex_DebugContinue]);
            hotkey_configuration_item("Break:", &config_hotkeys[config_HotkeyIndex_DebugBreak]);
            hotkey_configuration_item("Run to Cursor:", &config_hotkeys[config_HotkeyIndex_DebugRunToCursor]);
            hotkey_configuration_item("Toggle Breakpoint:", &config_hotkeys[config_HotkeyIndex_DebugBreakpoint]);
            hotkey_configuration_item("Go Back:", &config_hotkeys[config_HotkeyIndex_DebugGoBack]);

            gui_popup_modal_hotkey();

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

static void menu_video(void)
{
    if (ImGui::BeginMenu("Video"))
    {
        gui_in_use = true;

        if (ImGui::MenuItem("Full Screen", config_hotkeys[config_HotkeyIndex_Fullscreen].str, &config_emulator.fullscreen))
        {
            application_trigger_fullscreen(config_emulator.fullscreen);
        }

#if !defined(__APPLE__)
        if (ImGui::BeginMenu("Fullscreen Mode"))
        {
            ImGui::PushItemWidth(130.0f);
            ImGui::Combo("##fullscreen_mode", &config_emulator.fullscreen_mode, "Borderless\0Exclusive\0\0");
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

        if (ImGui::MenuItem("Resize Window to Content"))
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
            ImGui::Combo("##scale", &config_video.scale, "Integer Scale (Auto)\0Integer Scale (Manual)\0Scale to Window Height\0Scale to Window Width & Height\0\0");
            if (config_video.scale == 1)
                ImGui::SliderInt("##scale_manual", &config_video.scale_manual, 1, 16);
            ImGui::PopItemWidth();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Aspect Ratio"))
        {
            ImGui::PushItemWidth(190.0f);
            ImGui::Combo("##ratio", &config_video.ratio, "Square Pixels (1:1 PAR)\0Standard (4:3 DAR)\0Wide (16:9 DAR)\0Wide (16:10 DAR)\0PCE (6:5 DAR)\0\0");
            ImGui::PopItemWidth();
            ImGui::EndMenu();
        }


        ImGui::Separator();

        if (ImGui::BeginMenu("Vertical Sync"))
        {
            ImGui::PushItemWidth(240.0f);
#if defined(_WIN32)
            if (ImGui::Combo("##sync_mode", &config_video.sync_mode, "Disabled\0Fixed (60 Hz, 120 Hz, 240 Hz)\0Variable Refresh Rate (VRR)\0\0"))
#else
            if (ImGui::Combo("##sync_mode", &config_video.sync_mode, "Disabled\0Fixed (60 Hz, 120 Hz, 240 Hz)\0\0"))
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

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Disabled: do not synchronize presentation to the monitor.");
                ImGui::Text("Fixed: use normal VSync for 60 Hz, 120 Hz, and 240 Hz displays.");
#if defined(_WIN32)
                ImGui::Text("VRR: present at the emulator frame rate.");
                ImGui::Text("VRR requires fullscreen, a VRR display, and G-SYNC,");
                ImGui::Text("FreeSync, or Adaptive Sync enabled in your monitor and GPU driver settings.");
#endif
                ImGui::EndTooltip();
            }

            ImGui::EndMenu();
        }

        ImGui::MenuItem("Show FPS", "", &config_video.fps);

        ImGui::Separator();

        menu_shader();

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

static void menu_shader(void)
{
    if (!ImGui::BeginMenu("Shader"))
        return;

    bool has_preset = ogl_shader_chain_has_preset();
    int selected_index = 0;

    if (has_preset && config_video.shader_mode == config_ShaderMode_External)
    {
        for (int i = 0; i < shader_preset_count; i++)
        {
            if (shader_preset_config_path_matches(config_video.shader_preset_path.c_str(), shader_presets[i].path))
            {
                selected_index = i + 1;
                break;
            }
        }
    }

    const char* preview = selected_index == 0 ? "Pixel Perfect" : shader_presets[selected_index - 1].name;
    ImGui::PushItemWidth(240.0f);
    if (ImGui::BeginCombo("##ShaderPreset", preview))
    {
        bool selected = selected_index == 0;
        if (ImGui::Selectable("Pixel Perfect", selected))
        {
            if (selected_index != 0)
            {
                ogl_renderer_unload_shader_preset();
                gui_set_status_message("Shader preset: Pixel Perfect", 3000);
            }
        }
        if (selected)
            ImGui::SetItemDefaultFocus();

        for (int i = 0; i < shader_preset_count; i++)
        {
            selected = selected_index == i + 1;
            if (ImGui::Selectable(shader_presets[i].name, selected))
            {
                if (selected_index != i + 1)
                {
                    if (ogl_renderer_load_shader_preset(shader_presets[i].path))
                    {
                        std::string message("Shader preset loaded: ");
                        message += ogl_shader_chain_get_preset_name();
                        gui_set_status_message(message.c_str(), 3000);
                    }
                    else
                    {
                        std::string message("Shader preset failed: ");
                        message += ogl_shader_chain_get_last_error();
                        gui_set_status_message(message.c_str(), 5000);
                    }
                }
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    has_preset = ogl_shader_chain_has_preset();

    if (has_preset && ogl_shader_chain_get_parameter_count() > 0)
    {
        if (ImGui::BeginMenu("Parameters"))
        {
            draw_shader_parameters();
            ImGui::EndMenu();
        }
    }
    else if (ogl_shader_chain_get_last_error()[0] != '\0')
    {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.98f, 0.15f, 0.45f, 1.0f), "%s", ogl_shader_chain_get_last_error());
    }

    ImGui::EndMenu();
}

static void draw_shader_parameters(void)
{
    int count = ogl_shader_chain_get_parameter_count();
    if (count <= 0)
        return;

    const float parameter_width = 220.0f;

    if (ImGui::Button("Restore Defaults", ImVec2(parameter_width, 0.0f)))
    {
        if (ogl_shader_chain_restore_default_parameters())
        {
            ogl_renderer_save_shader_parameter_config();
            gui_set_status_message("Shader parameters restored", 3000);
        }
    }

    ImGui::PushItemWidth(parameter_width);

    for (int i = 0; i < count; i++)
    {
        const ShaderPresetParameter* parameter = ogl_shader_chain_get_parameter(i);
        if (!parameter)
            continue;

        float value = parameter->value;
        char label[160];
        snprintf(label, sizeof(label), "%s##shader_parameter_%d", parameter->label[0] != '\0' ? parameter->label : parameter->name, i);

        if (shader_parameter_is_toggle(parameter))
        {
            bool enabled = value >= 0.5f;
            if (ImGui::Checkbox(label, &enabled))
            {
                ogl_shader_chain_set_parameter(i, enabled ? 1.0f : 0.0f);
                ogl_renderer_save_shader_parameter_config();
            }
            continue;
        }

        if (shader_parameter_is_integer(parameter))
        {
            int int_value = shader_parameter_round_to_int(value);
            int min_value = shader_parameter_round_to_int(parameter->minimum);
            int max_value = shader_parameter_round_to_int(parameter->maximum);
            if (ImGui::SliderInt(label, &int_value, min_value, max_value))
            {
                ogl_shader_chain_set_parameter(i, (float)int_value);
                ogl_renderer_save_shader_parameter_config();
            }
            continue;
        }

        if (ImGui::SliderFloat(label, &value, parameter->minimum, parameter->maximum, "%.3f"))
        {
            ogl_shader_chain_set_parameter(i, value);
            ogl_renderer_save_shader_parameter_config();
        }
    }

    ImGui::PopItemWidth();
}

static bool shader_parameter_is_toggle(const ShaderPresetParameter* parameter)
{
    return parameter && parameter->minimum == 0.0f && parameter->maximum == 1.0f && parameter->step >= 1.0f;
}

static bool shader_parameter_is_integer(const ShaderPresetParameter* parameter)
{
    return parameter && parameter->step >= 1.0f;
}

static int shader_parameter_round_to_int(float value)
{
    return value >= 0.0f ? (int)(value + 0.5f) : (int)(value - 0.5f);
}

static void menu_input(void)
{
    if (ImGui::BeginMenu("Input"))
    {
        gui_in_use = true;


        if (ImGui::BeginMenu("Controller"))
        {
            for (int i = 0; i < GT_MAX_GAMEPADS; i++)
            {
                char player_name[32];
                snprintf(player_name, sizeof(player_name), "Player %d", i + 1);

                if (ImGui::BeginMenu(player_name))
                {
                    ImGui::PushItemWidth(200.0f);
                    if (ImGui::Combo("##controller", &config_input.controller_type[i], "None\0Original Gamepad\0" "6 Button Gamepad\0\0"))
                        emu_set_pad_type(i, (GT_Controller_Type)config_input.controller_type[i]);
                    ImGui::PopItemWidth();
                    ImGui::EndMenu();
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Mouse"))
        {
            ImGui::MenuItem("Capture Mouse", config_hotkeys[config_HotkeyIndex_CaptureMouse].str, &config_emulator.capture_mouse);
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("When enabled, the mouse will be captured inside");
                ImGui::Text("the emulator window to use the mouse freely.");
                ImGui::Text("Press %s to release the mouse.", config_hotkeys[config_HotkeyIndex_CaptureMouse].str);
                ImGui::EndTooltip();
            }

            ImGui::SliderInt("##mouse_sensitivity", &config_emulator.mouse_sensitivity, 1, 15, "Sensitivity = %d");

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Keyboard"))
        {
            for (int i = 0; i < GT_MAX_GAMEPADS; i++)
            {
                char keyboard_name[32];
                snprintf(keyboard_name, sizeof(keyboard_name), "Player %d", i + 1);

                if (ImGui::BeginMenu(keyboard_name))
                {
                    ImGui::TextDisabled("Keyboard %s", keyboard_name);
                    ImGui::Separator();
                    keyboard_configuration_item("Left:", &config_input_keyboard[i].key_left, i);
                    keyboard_configuration_item("Right:", &config_input_keyboard[i].key_right, i);
                    keyboard_configuration_item("Up:", &config_input_keyboard[i].key_up, i);
                    keyboard_configuration_item("Down:", &config_input_keyboard[i].key_down, i);
                    keyboard_configuration_item("Start:", &config_input_keyboard[i].key_start, i);
                    keyboard_configuration_item("Run:", &config_input_keyboard[i].key_run, i);
                    keyboard_configuration_item("A:", &config_input_keyboard[i].key_A, i);
                    keyboard_configuration_item("B:", &config_input_keyboard[i].key_B, i);
                    keyboard_configuration_item("C:", &config_input_keyboard[i].key_C, i);
                    ImGui::Separator();
                    ImGui::TextDisabled("6 Button Gamepad:");
                    keyboard_configuration_item("X:", &config_input_keyboard[i].key_X, i);
                    keyboard_configuration_item("Y:", &config_input_keyboard[i].key_Y, i);
                    keyboard_configuration_item("Z:", &config_input_keyboard[i].key_Z, i);

                    gui_popup_modal_keyboard();

                    ImGui::EndMenu();
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Gamepads"))
        {
            for (int i = 0; i < GT_MAX_GAMEPADS; i++)
            {
                char gamepad_name[32];
                snprintf(gamepad_name, sizeof(gamepad_name), "Player %d", i + 1);

                if (ImGui::BeginMenu(gamepad_name))
                {
                    if (!gamepad_controller[i])
                        ImGui::TextDisabled("This gamepad is not detected");
                    else
                        ImGui::TextDisabled("Gamepad detected for Player %d", i + 1);
                    ImGui::Separator();

                    if (ImGui::BeginMenu("Device"))
                    {
                        gamepad_device_selector(i);
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("Directional Controls"))
                    {
                        ImGui::PushItemWidth(150.0f);
                        ImGui::Combo("##directional", &config_input_gamepad[i].gamepad_directional, "D-pad\0Left Analog Stick\0\0");
                        ImGui::PopItemWidth();
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("Button Configuration"))
                    {
                        ImGui::TextDisabled("Gamepad %s", gamepad_name);
                        ImGui::Separator();
                        gamepad_configuration_item("Start:", &config_input_gamepad[i].gamepad_start, i);
                        gamepad_configuration_item("Run:", &config_input_gamepad[i].gamepad_run, i);
                        gamepad_configuration_item("A:", &config_input_gamepad[i].gamepad_A, i);
                        gamepad_configuration_item("B:", &config_input_gamepad[i].gamepad_B, i);
                        gamepad_configuration_item("C:", &config_input_gamepad[i].gamepad_C, i);
                        ImGui::Separator();
                        ImGui::TextDisabled("6 Button Gamepad:");
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

                        gamepad_configuration_item("Save State:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_SaveState], i);
                        gamepad_configuration_item("Load State:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_LoadState], i);
                        gamepad_configuration_item("Save State Slot 1:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_SelectSlot1], i);
                        gamepad_configuration_item("Save State Slot 2:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_SelectSlot2], i);
                        gamepad_configuration_item("Save State Slot 3:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_SelectSlot3], i);
                        gamepad_configuration_item("Save State Slot 4:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_SelectSlot4], i);
                        gamepad_configuration_item("Save State Slot 5:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_SelectSlot5], i);

                        ImGui::Separator();

                        gamepad_configuration_item("Reset:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_Reset], i);
                        gamepad_configuration_item("Pause:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_Pause], i);
                        gamepad_configuration_item("Fast Forward:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_FFWD], i);
                        gamepad_configuration_item("Rewind:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_Rewind], i);
                        gamepad_configuration_item("Screenshot:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_Screenshot], i);
                        gamepad_configuration_item("Mute Audio:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_Mute], i);
                        gamepad_configuration_item("Fullscreen:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_Fullscreen], i);
                        gamepad_configuration_item("Capture Mouse:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_CaptureMouse], i);
                        gamepad_configuration_item("Show Main Menu:", &config_input_gamepad_shortcuts[i].gamepad_shortcuts[config_HotkeyIndex_ShowMainMenu], i);

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

        ImGui::EndMenu();
    }
}

static void menu_audio(void)
{
    if (ImGui::BeginMenu("Audio"))
    {
        gui_in_use = true;

        if (ImGui::MenuItem("Enable Audio", config_hotkeys[config_HotkeyIndex_Mute].str, &config_audio.enable))
        {
            emu_audio_mute(!config_audio.enable);
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Master Volume", config_audio.enable))
        {
            ImGui::PushItemWidth(200.0f);
            if (ImGui::SliderFloat("##master_volume", &config_audio.master_volume, 0.0f, 2.0f, "Volume = %.2f", ImGuiSliderFlags_AlwaysClamp))
            {
                emu_audio_set_master_volume(config_audio.master_volume);
            }
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Anything above 1.00 may cause clipping.");
                ImGui::EndTooltip();
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();



        ImGui::MenuItem("Audio Sync", "", &config_audio.sync, config_audio.enable);

        if (ImGui::BeginMenu("Buffer Size", config_audio.enable))
        {
            ImGui::PushItemWidth(150.0f);
            if (ImGui::SliderInt("##buffer_count", &config_audio.buffer_count, 2, 5, "Buffers = %d"))
            {
                emu_audio_reset();
            }
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered())
            {
                float latency_ms = (config_audio.buffer_count * GT_AUDIO_QUEUE_SIZE) / (float)(GG_AUDIO_SAMPLE_RATE * 2) * 1000.0f;
                ImGui::BeginTooltip();
                ImGui::Text("Lower values reduce audio latency.");
                ImGui::Text("Higher values prevent audio underruns.");
                ImGui::Text("Enabling VSync may force higher buffer counts.");
                ImGui::Text("Current audio latency: %.0f ms", latency_ms);
                ImGui::EndTooltip();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }
}

static void menu_debug(void)
{
#if !defined(GG_DISABLE_DISASSEMBLER)
    if (ImGui::BeginMenu("Debug"))
    {
        gui_in_use = true;

        if (ImGui::MenuItem("Enable", "", &config_debug.debug))
        {
            emu_set_overscan(config_debug.debug ? 0 : config_video.overscan);
            emu_set_scanline_start_end(
                config_debug.debug ? 0 : config_video.scanline_start,
                config_debug.debug ? 241 : config_video.scanline_end);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save Debug Settings...", "", false, config_debug.debug))
        {
            save_debug_settings = true;
        }

        if (ImGui::MenuItem("Load Debug Settings...", "", false, config_debug.debug))
        {
            load_debug_settings = true;
        }

        ImGui::MenuItem("Auto Save/Load Debug Settings", "", &config_debug.auto_debug_settings, config_debug.debug);

        ImGui::Separator();

        if (ImGui::MenuItem("Reload ROM", config_hotkeys[config_HotkeyIndex_ReloadROM].str, false, config_debug.debug && !emu_is_empty()))
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
                ImGui::TextColored(ImVec4(0.90f, 0.70f, 0.10f, 1.0f), "STDIO mode active");
            else if (http_running)
                ImGui::TextColored(ImVec4(0.10f, 0.90f, 0.10f, 1.0f), "Listening on %s:%d", config_emulator.mcp_http_address.c_str(), config_emulator.mcp_tcp_port);
            else
                ImGui::TextColored(ImVec4(0.98f, 0.15f, 0.45f, 1.0f), "Stopped");

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



        ImGui::MenuItem("Show Output Screen", "", &config_debug.show_screen, config_debug.debug);

        if (ImGui::BeginMenu("Output Scale", config_debug.debug))
        {
            ImGui::PushItemWidth(200.0f);
            ImGui::SliderInt("##debug_scale", &config_debug.scale, 1, 10);
            ImGui::PopItemWidth();
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::MenuItem("Show Disassembler", "", &config_debug.show_disassembler, config_debug.debug);
        ImGui::MenuItem("Show Memory Editor", "", &config_debug.show_memory, config_debug.debug);
        ImGui::MenuItem("Show Trace Logger", "", &config_debug.show_trace_logger, config_debug.debug);

        ImGui::Separator();


        ImGui::Separator();

        ImGui::MenuItem("Show Rewind", "", &config_debug.show_rewind, config_debug.debug);

#if defined(__APPLE__) || defined(_WIN32)
        ImGui::Separator();
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

        if (ImGui::BeginMenu("Font Size", config_debug.debug))
        {
            ImGui::PushItemWidth(110.0f);
            if (ImGui::Combo("##font", &config_debug.font_size, "Very Small\0Small\0Medium\0Large\0\0"))
            {
                gui_default_font = gui_default_fonts[config_debug.font_size];
            }
            ImGui::PopItemWidth();
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
    if (!emu_mcp_is_running())
        return;

    char status[128];
    ImVec4 color(0.10f, 0.90f, 0.10f, 1.0f);

    int transport_mode = emu_mcp_get_transport_mode();
    if (transport_mode == 0)
    {
        snprintf(status, sizeof(status), "MCP: STDIO");
        color = ImVec4(0.90f, 0.70f, 0.10f, 1.0f);
    }
    else if (transport_mode == 1)
    {
        snprintf(status, sizeof(status), "MCP: HTTP (%s:%d)", config_emulator.mcp_http_address.c_str(), config_emulator.mcp_tcp_port);
    }
    else
    {
        return;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    float text_width = ImGui::CalcTextSize(status).x;
    float status_x = ImGui::GetWindowWidth() - text_width - style.ItemSpacing.x - 10.0f;
    float cursor_x = ImGui::GetCursorPosX();

    if (status_x <= cursor_x + style.ItemSpacing.x)
        return;

    ImGui::SameLine(status_x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(color, "%s", status);
}

static void file_dialogs(void)
{
    gui_file_dialog_process_results();

    if (open_rom || gui_shortcut_open_rom)
    {
        gui_shortcut_open_rom = false;
        gui_file_dialog_open_rom();
    }
    if (open_ram)
        gui_file_dialog_load_ram();
    if (save_ram)
        gui_file_dialog_save_ram();
    if (open_state)
        gui_file_dialog_load_state();
    if (save_state)
        gui_file_dialog_save_state();
    if (save_screenshot)
        gui_file_dialog_save_screenshot();
    if (save_vgm)
        gui_file_dialog_save_vgm();
    if (choose_savestates_path)
        gui_file_dialog_choose_savestate_path();
    if (choose_screenshots_path)
        gui_file_dialog_choose_screenshot_path();
    if (choose_backup_ram_path)
        gui_file_dialog_choose_backup_ram_path();
    if (choose_mb128_path)
        gui_file_dialog_choose_mb128_path();
    if (open_syscard_bios)
        gui_file_dialog_load_bios(true);
    if (open_gameexpress_bios)
        gui_file_dialog_load_bios(false);
    if (save_debug_settings)
        gui_file_dialog_save_debug_settings();
    if (load_debug_settings)
        gui_file_dialog_load_debug_settings();
#if defined(GG_ENABLE_PHYSICAL_CDROM)
    if (open_physical_cdrom)
    {
        Debug("Opening physical CD-ROM popup from menu request");
        gui_popup_open_physical_cdrom();
    }
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
#if defined(GG_ENABLE_PHYSICAL_CDROM)
    gui_popup_modal_physical_cdrom();
#endif
}

static const char* get_current_media_directory_text(void)
{
#if defined(GG_ENABLE_PHYSICAL_CDROM)
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
    snprintf(button_label, 256, "%s##%s%d", SDL_GetKeyName(SDL_GetKeyFromScancode(*key, SDL_KMOD_NONE, false)), text, player);

    if (ImGui::Button(button_label, ImVec2(90,0)))
    {
        gui_configured_key = key;
        ImGui::OpenPopup("Keyboard Configuration");
    }

    ImGui::SameLine();

    char remove_label[256];
    snprintf(remove_label, sizeof(remove_label), "X##rk%s%d", text, player);

    if (ImGui::Button(remove_label))
    {
        *key = SDL_SCANCODE_UNKNOWN;
    }
}

static void gamepad_configuration_item(const char* text, int* button, int player)
{
    ImGui::Text("%s", text);
    ImGui::SameLine(130);

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
    snprintf(button_label, sizeof(button_label), "%s##%s%d", button_name, text, player);

    if (ImGui::Button(button_label, ImVec2(70,0)))
    {
        gui_configured_button = button;
        ImGui::OpenPopup("Gamepad Configuration");
    }

    ImGui::SameLine();

    char remove_label[256];
    snprintf(remove_label, sizeof(remove_label), "X##rg%s%d", text, player);

    if (ImGui::Button(remove_label))
    {
        *button = SDL_GAMEPAD_BUTTON_INVALID;
    }
}

static void hotkey_configuration_item(const char* text, config_Hotkey* hotkey)
{
    ImGui::Text("%s", text);
    ImGui::SameLine(150);

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

static void gamepad_device_selector(int player)
{
    if (player < 0 || player >= GG_MAX_GAMEPADS)
        return;

    const int max_detected_gamepads = 32;
    SDL_JoystickID id_map[max_detected_gamepads];
    id_map[0] = 0;
    int count = 1;

    std::string items;
    items.reserve(4096);
    items.append("<None>");
    items.push_back('\0');

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
        count++;
    }

    items.push_back('\0');

    char label[32];
    snprintf(label, sizeof(label), "##device_player%d", player + 1);

    if (ImGui::Combo(label, &selected, items.c_str()))
    {
        SDL_JoystickID instance_id = id_map[selected];
        gamepad_assign(player, instance_id);
    }
}

static void draw_savestate_slot_info(int slot)
{
    if (emu_savestates[slot].rom_name[0] != 0)
    {
        if (emu_savestates[slot].version < GG_SAVESTATE_MIN_VERSION || emu_savestates[slot].version > GG_SAVESTATE_VERSION)
        {
            ImGui::TextColored(ImVec4(0.98f, 0.15f, 0.45f, 1.0f), "This savestate is from an older version and will not work" );
            if (emu_savestates[slot].emu_build[0] != 0)
                ImGui::TextColored(ImVec4(0.98f, 0.15f, 0.45f, 1.0f), "Use %s - %s", GG_TITLE, emu_savestates[slot].emu_build);
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
            ImGui::Image((ImTextureID)(intptr_t)ogl_renderer_emu_savestates, ImVec2((height / 3.0f) * 4.0f, height), ImVec2(0, 0), ImVec2(width / 2048.0f, height / 256.0f));
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(0.50f, 0.50f, 0.50f, 1.0f), "Slot %d is empty", slot + 1);
    }
}
