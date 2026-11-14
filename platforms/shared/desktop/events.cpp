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

#include <SDL3/SDL.h>
#include "application.h"
#include "config.h"
#include "emu.h"
#include "gamepad.h"
#include "gui.h"
#include "gui_actions.h"

#define EVENTS_IMPORT
#include "events.h"

static u16 input_last_buttons[GT_MAX_GAMEPADS] = { };
static bool input_updated = false;
static u16 mouse_buttons = 0;
static float mouse_remainder_x = 0.0f;
static float mouse_remainder_y = 0.0f;
static bool keyboard_scancode_down[SDL_SCANCODE_COUNT] = { };

static bool events_check_hotkey(const SDL_Event* event, const config_Hotkey& hotkey, bool allow_repeat);
static bool events_match_hotkey_scancode(const SDL_Event* event, const config_Hotkey& hotkey);
static void input_update(bool check_shortcuts);
static GT_GamePad_State input_build_state(int controller);
static u16 input_filter_opposing_directions(int controller, u16 buttons);
static void keyboard_send_key(SDL_Scancode scancode, bool pressed);
static bool keyboard_captures_event(const SDL_Event* event);
static bool keyboard_controller_uses_key(SDL_Scancode scancode);
static int mouse_get_controller(void);

bool events_shortcuts(const SDL_Event* event)
{
    bool keyboard_captured = config_emulator.capture_keyboard && keyboard_captures_event(event);

    if (event->type == SDL_EVENT_KEY_UP)
    {
        if (!keyboard_captured && events_match_hotkey_scancode(event, config_hotkeys[config_HotkeyIndex_Rewind]))
        {
            gui_action_rewind_released();
            return true;
        }

        return false;
    }

    if (event->type != SDL_EVENT_KEY_DOWN)
        return false;

    if (!keyboard_captured && events_check_hotkey(event, config_hotkeys[config_HotkeyIndex_Rewind], false))
    {
        gui_action_rewind_pressed();
        return true;
    }

    if (events_check_hotkey(event, config_hotkeys[config_HotkeyIndex_Quit], false))
    {
        application_trigger_quit();
        return true;
    }

    // While the keyboard is captured every other key belongs to the Towns keyboard
    if (keyboard_captured)
    {
        if (events_check_hotkey(event, config_hotkeys[config_HotkeyIndex_Fullscreen], false))
        {
            gui_shortcut(gui_ShortcutFullscreen);
            return true;
        }

        if (events_check_hotkey(event, config_hotkeys[config_HotkeyIndex_CaptureMouse], false))
        {
            gui_shortcut(gui_ShortcutCaptureMouse);
            return true;
        }

        if (events_check_hotkey(event, config_hotkeys[config_HotkeyIndex_CaptureKeyboard], false))
        {
            gui_shortcut(gui_ShortcutCaptureKeyboard);
            return true;
        }

        if (events_check_hotkey(event, config_hotkeys[config_HotkeyIndex_KeyboardMapper], false))
        {
            gui_shortcut(gui_ShortcutKeyboardMapper);
            return true;
        }

        if (events_check_hotkey(event, config_hotkeys[config_HotkeyIndex_KeyMapping], false))
        {
            gui_shortcut(gui_ShortcutKeyMapping);
            return true;
        }

        return false;
    }

    for (int i = 0; i < GUI_HOTKEY_MAP_COUNT; i++)
    {
        const gui_HotkeyMapping& mapping = gui_hotkey_map[i];

        if (events_check_hotkey(event, config_hotkeys[mapping.config_index], mapping.allow_repeat))
        {
            gui_shortcut(mapping.shortcut);
            return true;
        }
    }

    if (event->key.repeat == 0 && event->key.scancode == SDL_SCANCODE_ESCAPE &&
        config_emulator.fullscreen && !config_emulator.always_show_menu && !emu_is_video_recording())
    {
        application_trigger_fullscreen(false);
        return true;
    }

    return false;
}

void events_emu(const SDL_Event* event, bool shortcut_consumed)
{
    if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP)
    {
        SDL_Scancode scancode = event->key.scancode;

        // Keys already held by the Towns are released even when a menu has just taken focus
        if (event->type == SDL_EVENT_KEY_UP)
            keyboard_send_key(scancode, false);
        else if (keyboard_captures_event(event) && !shortcut_consumed && event->key.repeat == 0 &&
            !keyboard_controller_uses_key(scancode))
            keyboard_send_key(scancode, true);
    }

    if (gui_in_use && event->type != SDL_EVENT_MOUSE_BUTTON_UP)
        return;

    switch (event->type)
    {
        case SDL_EVENT_MOUSE_MOTION:
        {
            int controller = mouse_get_controller();

            if (controller < 0 || (!config_emulator.capture_mouse && !gui_main_window_hovered) || emu_key_mapper_is_open())
                break;

            float scale = (float)MAX(config_emulator.mouse_sensitivity, 1) / 6.0f;
            mouse_remainder_x += event->motion.xrel * scale;
            mouse_remainder_y += event->motion.yrel * scale;
            int x = (int)mouse_remainder_x;
            int y = (int)mouse_remainder_y;
            mouse_remainder_x -= (float)x;
            mouse_remainder_y -= (float)y;
            emu_set_mouse_delta((GT_Controllers)controller, x, y);
            break;
        }

        // Clicks outside the screen, like those on debugger windows, stay with the GUI
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            u16 button = 0;

            if (event->button.button == SDL_BUTTON_LEFT)
                button = GT_GAMEPAD_A;
            else if (event->button.button == SDL_BUTTON_RIGHT)
                button = GT_GAMEPAD_B;

            if (event->type == SDL_EVENT_MOUSE_BUTTON_UP)
                mouse_buttons &= (u16)~button;
            else if ((config_emulator.capture_mouse || gui_main_window_hovered) && !emu_key_mapper_is_open())
                mouse_buttons |= button;

            input_update(true);
            input_updated = true;
            break;
        }

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        {
            input_update(true);
            input_updated = true;
            break;
        }

        default:
            break;
    }
}

void events_emu(void)
{
    if (input_updated || gui_in_use)
        return;

    SDL_PumpEvents();
    input_update(true);
    input_updated = true;
}

void events_sync_input(void)
{
    events_release_keyboard();
    events_release_mouse();
    SDL_PumpEvents();
    input_update(false);
}

void events_release_keyboard(void)
{
    memset(keyboard_scancode_down, 0, sizeof(keyboard_scancode_down));
    emu_release_all_host_keys();
    emu_release_all_keys();
}

void events_release_mouse(void)
{
    int controller = mouse_get_controller();

    mouse_buttons = 0;
    mouse_remainder_x = 0.0f;
    mouse_remainder_y = 0.0f;

    if (controller >= 0)
        emu_clear_mouse((GT_Controllers)controller);
}

bool events_is_keyboard_active(void)
{
    return gui_main_window_focused && !gui_in_use && !gui_dialog_in_use && !emu_is_empty();
}

void events_reset_input(void)
{
    input_updated = false;
}

bool events_input_updated(void)
{
    return input_updated;
}

static void input_update(bool check_shortcuts)
{
    for (int controller = 0; controller < GT_MAX_GAMEPADS; controller++)
    {
        GT_GamePad_State state = { 0 };
        int type = emu_get_pad_type((GT_Controllers)controller);

        if (type != GT_CONTROLLER_NONE)
            state = input_build_state(controller);

        if (type == GT_CONTROLLER_MOUSE)
        {
            state.buttons &= GT_GAMEPAD_A | GT_GAMEPAD_B;

            if (controller == mouse_get_controller())
                state.buttons |= mouse_buttons;
        }

        state.buttons = input_filter_opposing_directions(controller, state.buttons);
        emu_set_gamepad_state((GT_Controllers)controller, state);
        input_last_buttons[controller] = state.buttons;

        if (check_shortcuts)
            gamepad_check_shortcuts(controller);
    }
}

static GT_GamePad_State input_build_state(int controller)
{
    GT_GamePad_State state = { 0 };
    SDL_Keymod mods = SDL_GetModState();
    bool six_button = emu_get_pad_type((GT_Controllers)controller) == GT_CONTROLLER_6_BUTTON_GAMEPAD;

    if (config_input.keyboard_mode[controller] == config_KeyboardMode_Gamepad &&
        (mods & (SDL_KMOD_CTRL | SDL_KMOD_SHIFT | SDL_KMOD_ALT | SDL_KMOD_GUI)) == 0)
    {
        const bool* keyboard = SDL_GetKeyboardState(NULL);
        const config_Input_Keyboard& keys = config_input_keyboard[controller];
        SDL_Scancode key_A = six_button ? keys.key_6_A : keys.key_A;
        SDL_Scancode key_B = six_button ? keys.key_6_B : keys.key_B;

        if (keyboard[keys.key_left]) state.buttons |= GT_GAMEPAD_LEFT;
        if (keyboard[keys.key_right]) state.buttons |= GT_GAMEPAD_RIGHT;
        if (keyboard[keys.key_up]) state.buttons |= GT_GAMEPAD_UP;
        if (keyboard[keys.key_down]) state.buttons |= GT_GAMEPAD_DOWN;
        if (keyboard[keys.key_select]) state.buttons |= GT_GAMEPAD_SELECT;
        if (keyboard[keys.key_run]) state.buttons |= GT_GAMEPAD_RUN;
        if (keyboard[key_A]) state.buttons |= GT_GAMEPAD_A;
        if (keyboard[key_B]) state.buttons |= GT_GAMEPAD_B;
        if (keyboard[keys.key_C]) state.buttons |= GT_GAMEPAD_C;
        if (keyboard[keys.key_X]) state.buttons |= GT_GAMEPAD_X;
        if (keyboard[keys.key_Y]) state.buttons |= GT_GAMEPAD_Y;
        if (keyboard[keys.key_Z]) state.buttons |= GT_GAMEPAD_Z;
        if (keyboard[keys.key_zoom]) state.buttons |= GT_GAMEPAD_ZOOM;
    }

    SDL_Gamepad* gamepad = gamepad_controller[controller];

    if (!IsValidPointer(gamepad))
        return state;

    const config_Input_Gamepad& mapping = config_input_gamepad[controller];
    int button_A = six_button ? mapping.gamepad_6_A : mapping.gamepad_A;
    int button_B = six_button ? mapping.gamepad_6_B : mapping.gamepad_B;

    if (gamepad_get_button(gamepad, mapping.gamepad_select)) state.buttons |= GT_GAMEPAD_SELECT;
    if (gamepad_get_button(gamepad, mapping.gamepad_run)) state.buttons |= GT_GAMEPAD_RUN;
    if (gamepad_get_button(gamepad, button_A)) state.buttons |= GT_GAMEPAD_A;
    if (gamepad_get_button(gamepad, button_B)) state.buttons |= GT_GAMEPAD_B;
    if (gamepad_get_button(gamepad, mapping.gamepad_C)) state.buttons |= GT_GAMEPAD_C;
    if (gamepad_get_button(gamepad, mapping.gamepad_X)) state.buttons |= GT_GAMEPAD_X;
    if (gamepad_get_button(gamepad, mapping.gamepad_Y)) state.buttons |= GT_GAMEPAD_Y;
    if (gamepad_get_button(gamepad, mapping.gamepad_Z)) state.buttons |= GT_GAMEPAD_Z;
    if (gamepad_get_button(gamepad, mapping.gamepad_zoom)) state.buttons |= GT_GAMEPAD_ZOOM;

    if (mapping.gamepad_directional == 0 || mapping.gamepad_directional == 2)
    {
        if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))
            state.buttons |= GT_GAMEPAD_LEFT;

        if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
            state.buttons |= GT_GAMEPAD_RIGHT;

        if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP))
            state.buttons |= GT_GAMEPAD_UP;

        if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))
            state.buttons |= GT_GAMEPAD_DOWN;
    }

    if (mapping.gamepad_directional == 1 || mapping.gamepad_directional == 2)
    {
        int x = SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)mapping.gamepad_x_axis);
        int y = SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)mapping.gamepad_y_axis);

        if (mapping.gamepad_invert_x_axis) x = -x;
        if (mapping.gamepad_invert_y_axis) y = -y;

        const int dead_zone = 8000;

        if (x < -dead_zone) state.buttons |= GT_GAMEPAD_LEFT;
        if (x > dead_zone) state.buttons |= GT_GAMEPAD_RIGHT;
        if (y < -dead_zone) state.buttons |= GT_GAMEPAD_UP;
        if (y > dead_zone) state.buttons |= GT_GAMEPAD_DOWN;
    }

    return state;
}

static u16 input_filter_opposing_directions(int controller, u16 buttons)
{
    if (config_input.allow_up_down)
        return buttons;

    u16 previous = input_last_buttons[controller];

    if ((buttons & GT_GAMEPAD_UP) && (buttons & GT_GAMEPAD_DOWN))
    {
        if (previous & GT_GAMEPAD_UP)
            buttons = (u16)(buttons & ~GT_GAMEPAD_DOWN);
        else if (previous & GT_GAMEPAD_DOWN)
            buttons = (u16)(buttons & ~GT_GAMEPAD_UP);
        else
            buttons = (u16)(buttons & ~GT_GAMEPAD_DOWN);
    }

    if ((buttons & GT_GAMEPAD_LEFT) && (buttons & GT_GAMEPAD_RIGHT))
    {
        if (previous & GT_GAMEPAD_LEFT)
            buttons = (u16)(buttons & ~GT_GAMEPAD_RIGHT);
        else if (previous & GT_GAMEPAD_RIGHT)
            buttons = (u16)(buttons & ~GT_GAMEPAD_LEFT);
        else
            buttons = (u16)(buttons & ~GT_GAMEPAD_RIGHT);
    }

    return buttons;
}

static bool events_check_hotkey(const SDL_Event* event, const config_Hotkey& hotkey, bool allow_repeat)
{
    if (event->type != SDL_EVENT_KEY_DOWN)
        return false;

    if (!allow_repeat && event->key.repeat != 0)
        return false;

    if (event->key.scancode != hotkey.key)
        return false;

    SDL_Keymod mods = event->key.mod;
    SDL_Keymod expected = hotkey.mod;
    SDL_Keymod normalized = (SDL_Keymod)0;
    SDL_Keymod expected_normalized = (SDL_Keymod)0;

    if (mods & (SDL_KMOD_LCTRL | SDL_KMOD_RCTRL))
        normalized = (SDL_Keymod)(normalized | SDL_KMOD_CTRL);

    if (mods & (SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT))
        normalized = (SDL_Keymod)(normalized | SDL_KMOD_SHIFT);

    if (mods & (SDL_KMOD_LALT | SDL_KMOD_RALT))
        normalized = (SDL_Keymod)(normalized | SDL_KMOD_ALT);

    if (mods & (SDL_KMOD_LGUI | SDL_KMOD_RGUI))
        normalized = (SDL_Keymod)(normalized | SDL_KMOD_GUI);

    if (expected & (SDL_KMOD_CTRL | SDL_KMOD_LCTRL | SDL_KMOD_RCTRL))
        expected_normalized = (SDL_Keymod)(expected_normalized | SDL_KMOD_CTRL);

    if (expected & (SDL_KMOD_SHIFT | SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT))
        expected_normalized = (SDL_Keymod)(expected_normalized | SDL_KMOD_SHIFT);

    if (expected & (SDL_KMOD_ALT | SDL_KMOD_LALT | SDL_KMOD_RALT))
        expected_normalized = (SDL_Keymod)(expected_normalized | SDL_KMOD_ALT);

    if (expected & (SDL_KMOD_GUI | SDL_KMOD_LGUI | SDL_KMOD_RGUI))
        expected_normalized = (SDL_Keymod)(expected_normalized | SDL_KMOD_GUI);

    return normalized == expected_normalized;
}

static bool events_match_hotkey_scancode(const SDL_Event* event, const config_Hotkey& hotkey)
{
    if (event->type != SDL_EVENT_KEY_UP && event->type != SDL_EVENT_KEY_DOWN)
        return false;

    if (hotkey.key == SDL_SCANCODE_UNKNOWN)
        return false;

    return event->key.scancode == hotkey.key;
}

static bool keyboard_captures_event(const SDL_Event* event)
{
    if (event->type != SDL_EVENT_KEY_DOWN && event->type != SDL_EVENT_KEY_UP)
        return false;

    if (!events_is_keyboard_active())
        return false;

    return (gui_main_window_sdl_window_id == 0) || (event->key.windowID == gui_main_window_sdl_window_id);
}

static void keyboard_send_key(SDL_Scancode scancode, bool pressed)
{
    if (scancode <= SDL_SCANCODE_UNKNOWN || scancode >= SDL_SCANCODE_COUNT)
        return;

    if (keyboard_scancode_down[scancode] == pressed)
        return;

    keyboard_scancode_down[scancode] = pressed;

    if (pressed)
        emu_host_key_pressed((int)scancode);
    else
        emu_host_key_released((int)scancode);
}

// Keys bound to a gamepad driven by the keyboard never reach the Towns keyboard
static bool keyboard_controller_uses_key(SDL_Scancode scancode)
{
    for (int controller = 0; controller < GT_MAX_GAMEPADS; controller++)
    {
        int type = emu_get_pad_type((GT_Controllers)controller);

        if (config_input.keyboard_mode[controller] != config_KeyboardMode_Gamepad || type == GT_CONTROLLER_NONE)
            continue;

        const config_Input_Keyboard& keys = config_input_keyboard[controller];
        bool six_button = type == GT_CONTROLLER_6_BUTTON_GAMEPAD;
        const SDL_Scancode mapped[12] =
        {
            six_button ? keys.key_6_A : keys.key_A, six_button ? keys.key_6_B : keys.key_B, keys.key_left,
            keys.key_right, keys.key_up, keys.key_down, keys.key_select, keys.key_run, keys.key_C, keys.key_X,
            keys.key_Y, keys.key_Z
        };
        int count = six_button ? 12 : (type == GT_CONTROLLER_MOUSE ? 2 : 9);

        for (int i = 0; i < count; i++)
        {
            if (mapped[i] == scancode)
                return true;
        }

        if (type == GT_CONTROLLER_MARTY_GAMEPAD && keys.key_zoom == scancode)
            return true;
    }

    return false;
}

static int mouse_get_controller(void)
{
    for (int i = 0; i < GT_MAX_GAMEPADS; i++)
    {
        if (emu_get_pad_type((GT_Controllers)i) == GT_CONTROLLER_MOUSE)
            return i;
    }

    return -1;
}

