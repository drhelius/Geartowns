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
static bool mouse_left = false;
static bool mouse_right = false;
static bool keyboard_scancode_down[SDL_SCANCODE_COUNT] = { };
static GT_Keys keyboard_pressed_keys[SDL_SCANCODE_COUNT];
static int keyboard_key_references[GT_KEY_COUNT] = { };

static bool events_check_hotkey(const SDL_Event* event, const config_Hotkey& hotkey, bool allow_repeat);
static bool events_match_hotkey_scancode(const SDL_Event* event, const config_Hotkey& hotkey);
static void input_update(bool check_shortcuts);
static GT_GamePad_State input_build_state(int controller);
static u16 input_filter_opposing_directions(int controller, u16 buttons);
static GT_Keys keyboard_key_from_scancode(SDL_Scancode scancode);
static void keyboard_send_key(SDL_Scancode scancode, bool pressed);
static bool keyboard_captures_event(const SDL_Event* event);
static bool keyboard_controller_uses_key(SDL_Scancode scancode);

bool events_shortcuts(const SDL_Event* event)
{
    bool keyboard_captured = keyboard_captures_event(event);

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

    // While the emulator window has focus every other key belongs to the Towns keyboard
    if (keyboard_captured)
    {
        if (events_check_hotkey(event, config_hotkeys[config_HotkeyIndex_Fullscreen], false))
        {
            gui_shortcut(gui_ShortcutFullscreen);
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
        config_emulator.fullscreen && !config_emulator.always_show_menu)
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
            if (!config_emulator.capture_mouse && !gui_main_window_hovered)
                break;

            int sensitivity = MAX(config_emulator.mouse_sensitivity, 1);
            int x = (int)(event->motion.xrel * ((float)sensitivity / 6.0f));
            int y = (int)(event->motion.yrel * ((float)sensitivity / 6.0f));
            emu_set_mouse_delta(x, y);
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            bool pressed = event->type == SDL_EVENT_MOUSE_BUTTON_DOWN;

            if (event->button.button == SDL_BUTTON_LEFT)
                mouse_left = pressed;
            else if (event->button.button == SDL_BUTTON_RIGHT)
                mouse_right = pressed;

            emu_set_mouse_buttons(mouse_left, mouse_right);
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
    SDL_PumpEvents();
    input_update(false);
}

void events_release_keyboard(void)
{
    memset(keyboard_scancode_down, 0, sizeof(keyboard_scancode_down));
    memset(keyboard_key_references, 0, sizeof(keyboard_key_references));
    emu_release_all_keys();
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

        if (config_input.controller_type[controller] != GT_CONTROLLER_NONE)
            state = input_build_state(controller);

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

    if (config_input.use_keyboard[controller] && (mods & (SDL_KMOD_CTRL | SDL_KMOD_SHIFT | SDL_KMOD_ALT | SDL_KMOD_GUI)) == 0)
    {
        const bool* keyboard = SDL_GetKeyboardState(NULL);
        const config_Input_Keyboard& keys = config_input_keyboard[controller];

        if (keyboard[keys.key_left]) state.buttons |= GT_GAMEPAD_LEFT;
        if (keyboard[keys.key_right]) state.buttons |= GT_GAMEPAD_RIGHT;
        if (keyboard[keys.key_up]) state.buttons |= GT_GAMEPAD_UP;
        if (keyboard[keys.key_down]) state.buttons |= GT_GAMEPAD_DOWN;
        if (keyboard[keys.key_start]) state.buttons |= GT_GAMEPAD_START;
        if (keyboard[keys.key_run]) state.buttons |= GT_GAMEPAD_RUN;
        if (keyboard[keys.key_A]) state.buttons |= GT_GAMEPAD_A;
        if (keyboard[keys.key_B]) state.buttons |= GT_GAMEPAD_B;
        if (keyboard[keys.key_C]) state.buttons |= GT_GAMEPAD_C;
        if (keyboard[keys.key_X]) state.buttons |= GT_GAMEPAD_X;
        if (keyboard[keys.key_Y]) state.buttons |= GT_GAMEPAD_Y;
        if (keyboard[keys.key_Z]) state.buttons |= GT_GAMEPAD_Z;
    }

    SDL_Gamepad* gamepad = gamepad_controller[controller];

    if (!IsValidPointer(gamepad))
        return state;

    const config_Input_Gamepad& mapping = config_input_gamepad[controller];

    if (gamepad_get_button(gamepad, mapping.gamepad_start)) state.buttons |= GT_GAMEPAD_START;
    if (gamepad_get_button(gamepad, mapping.gamepad_run)) state.buttons |= GT_GAMEPAD_RUN;
    if (gamepad_get_button(gamepad, mapping.gamepad_A)) state.buttons |= GT_GAMEPAD_A;
    if (gamepad_get_button(gamepad, mapping.gamepad_B)) state.buttons |= GT_GAMEPAD_B;
    if (gamepad_get_button(gamepad, mapping.gamepad_C)) state.buttons |= GT_GAMEPAD_C;
    if (gamepad_get_button(gamepad, mapping.gamepad_X)) state.buttons |= GT_GAMEPAD_X;
    if (gamepad_get_button(gamepad, mapping.gamepad_Y)) state.buttons |= GT_GAMEPAD_Y;
    if (gamepad_get_button(gamepad, mapping.gamepad_Z)) state.buttons |= GT_GAMEPAD_Z;

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

    if (pressed)
    {
        if (keyboard_scancode_down[scancode])
            return;

        GT_Keys key = keyboard_key_from_scancode(scancode);

        if (key == GT_KEY_NONE)
            return;

        keyboard_scancode_down[scancode] = true;
        keyboard_pressed_keys[scancode] = key;

        if (keyboard_key_references[key]++ == 0)
            emu_key_pressed(key);
    }
    else
    {
        if (!keyboard_scancode_down[scancode])
            return;

        GT_Keys key = keyboard_pressed_keys[scancode];
        keyboard_scancode_down[scancode] = false;

        if (keyboard_key_references[key] > 0 && --keyboard_key_references[key] == 0)
            emu_key_released(key);
    }
}

// Keys bound to a gamepad driven by the keyboard never reach the Towns keyboard
static bool keyboard_controller_uses_key(SDL_Scancode scancode)
{
    for (int controller = 0; controller < GT_MAX_GAMEPADS; controller++)
    {
        int type = config_input.controller_type[controller];

        if (!config_input.use_keyboard[controller] || type == GT_CONTROLLER_NONE)
            continue;

        const config_Input_Keyboard& keys = config_input_keyboard[controller];
        const SDL_Scancode mapped[12] =
        {
            keys.key_left, keys.key_right, keys.key_up, keys.key_down, keys.key_start, keys.key_run,
            keys.key_A, keys.key_B, keys.key_C, keys.key_X, keys.key_Y, keys.key_Z
        };
        int count = type == GT_CONTROLLER_6_BUTTON_GAMEPAD ? 12 : 9;

        for (int i = 0; i < count; i++)
        {
            if (mapped[i] == scancode)
                return true;
        }
    }

    return false;
}

static GT_Keys keyboard_key_from_scancode(SDL_Scancode scancode)
{
    switch (scancode)
    {
        case SDL_SCANCODE_A: return GT_KEY_A;
        case SDL_SCANCODE_B: return GT_KEY_B;
        case SDL_SCANCODE_C: return GT_KEY_C;
        case SDL_SCANCODE_D: return GT_KEY_D;
        case SDL_SCANCODE_E: return GT_KEY_E;
        case SDL_SCANCODE_F: return GT_KEY_F;
        case SDL_SCANCODE_G: return GT_KEY_G;
        case SDL_SCANCODE_H: return GT_KEY_H;
        case SDL_SCANCODE_I: return GT_KEY_I;
        case SDL_SCANCODE_J: return GT_KEY_J;
        case SDL_SCANCODE_K: return GT_KEY_K;
        case SDL_SCANCODE_L: return GT_KEY_L;
        case SDL_SCANCODE_M: return GT_KEY_M;
        case SDL_SCANCODE_N: return GT_KEY_N;
        case SDL_SCANCODE_O: return GT_KEY_O;
        case SDL_SCANCODE_P: return GT_KEY_P;
        case SDL_SCANCODE_Q: return GT_KEY_Q;
        case SDL_SCANCODE_R: return GT_KEY_R;
        case SDL_SCANCODE_S: return GT_KEY_S;
        case SDL_SCANCODE_T: return GT_KEY_T;
        case SDL_SCANCODE_U: return GT_KEY_U;
        case SDL_SCANCODE_V: return GT_KEY_V;
        case SDL_SCANCODE_W: return GT_KEY_W;
        case SDL_SCANCODE_X: return GT_KEY_X;
        case SDL_SCANCODE_Y: return GT_KEY_Y;
        case SDL_SCANCODE_Z: return GT_KEY_Z;
        case SDL_SCANCODE_1: return GT_KEY_1;
        case SDL_SCANCODE_2: return GT_KEY_2;
        case SDL_SCANCODE_3: return GT_KEY_3;
        case SDL_SCANCODE_4: return GT_KEY_4;
        case SDL_SCANCODE_5: return GT_KEY_5;
        case SDL_SCANCODE_6: return GT_KEY_6;
        case SDL_SCANCODE_7: return GT_KEY_7;
        case SDL_SCANCODE_8: return GT_KEY_8;
        case SDL_SCANCODE_9: return GT_KEY_9;
        case SDL_SCANCODE_0: return GT_KEY_0;
        case SDL_SCANCODE_RETURN: return GT_KEY_RETURN;
        case SDL_SCANCODE_ESCAPE: return GT_KEY_BREAK;
        case SDL_SCANCODE_BACKSPACE: return GT_KEY_BACKSPACE;
        case SDL_SCANCODE_TAB: return GT_KEY_TAB;
        case SDL_SCANCODE_SPACE: return GT_KEY_SPACE;
        case SDL_SCANCODE_MINUS: return GT_KEY_MINUS;
        case SDL_SCANCODE_EQUALS: return GT_KEY_CARET;
        case SDL_SCANCODE_LEFTBRACKET: return GT_KEY_AT;
        case SDL_SCANCODE_RIGHTBRACKET: return GT_KEY_LEFT_BRACKET;
        case SDL_SCANCODE_BACKSLASH: return GT_KEY_YEN;
        case SDL_SCANCODE_NONUSHASH: return GT_KEY_RIGHT_BRACKET;
        case SDL_SCANCODE_SEMICOLON: return GT_KEY_SEMICOLON;
        case SDL_SCANCODE_APOSTROPHE: return GT_KEY_COLON;
        case SDL_SCANCODE_GRAVE: return GT_KEY_ESCAPE;
        case SDL_SCANCODE_COMMA: return GT_KEY_COMMA;
        case SDL_SCANCODE_PERIOD: return GT_KEY_PERIOD;
        case SDL_SCANCODE_SLASH: return GT_KEY_SLASH;
        case SDL_SCANCODE_NONUSBACKSLASH: return GT_KEY_UNDERSCORE;
        case SDL_SCANCODE_INTERNATIONAL1: return GT_KEY_UNDERSCORE;
        case SDL_SCANCODE_INTERNATIONAL3: return GT_KEY_YEN;
        case SDL_SCANCODE_CAPSLOCK: return GT_KEY_CAPS;
        case SDL_SCANCODE_F1: return GT_KEY_PF1;
        case SDL_SCANCODE_F2: return GT_KEY_PF2;
        case SDL_SCANCODE_F3: return GT_KEY_PF3;
        case SDL_SCANCODE_F4: return GT_KEY_PF4;
        case SDL_SCANCODE_F5: return GT_KEY_PF5;
        case SDL_SCANCODE_F6: return GT_KEY_PF6;
        case SDL_SCANCODE_F7: return GT_KEY_PF7;
        case SDL_SCANCODE_F8: return GT_KEY_PF8;
        case SDL_SCANCODE_F9: return GT_KEY_PF9;
        case SDL_SCANCODE_F10: return GT_KEY_PF10;
        case SDL_SCANCODE_F11: return GT_KEY_PF11;
        case SDL_SCANCODE_F12: return GT_KEY_PF12;
        case SDL_SCANCODE_F13: return GT_KEY_PF13;
        case SDL_SCANCODE_F14: return GT_KEY_PF14;
        case SDL_SCANCODE_F15: return GT_KEY_PF15;
        case SDL_SCANCODE_F16: return GT_KEY_PF16;
        case SDL_SCANCODE_F17: return GT_KEY_PF17;
        case SDL_SCANCODE_F18: return GT_KEY_PF18;
        case SDL_SCANCODE_F19: return GT_KEY_PF19;
        case SDL_SCANCODE_F20: return GT_KEY_PF20;
        case SDL_SCANCODE_PRINTSCREEN: return GT_KEY_COPY;
        case SDL_SCANCODE_PAUSE: return GT_KEY_BREAK;
        case SDL_SCANCODE_INSERT: return GT_KEY_INSERT;
        case SDL_SCANCODE_HOME: return GT_KEY_HOME;
        case SDL_SCANCODE_PAGEUP: return GT_KEY_PREVIOUS;
        case SDL_SCANCODE_DELETE: return GT_KEY_DELETE;
        case SDL_SCANCODE_PAGEDOWN: return GT_KEY_NEXT;
        case SDL_SCANCODE_RIGHT: return GT_KEY_RIGHT;
        case SDL_SCANCODE_LEFT: return GT_KEY_LEFT;
        case SDL_SCANCODE_DOWN: return GT_KEY_DOWN;
        case SDL_SCANCODE_UP: return GT_KEY_UP;
        case SDL_SCANCODE_KP_DIVIDE: return GT_KEY_KP_DIVIDE;
        case SDL_SCANCODE_KP_MULTIPLY: return GT_KEY_KP_MULTIPLY;
        case SDL_SCANCODE_KP_MINUS: return GT_KEY_KP_MINUS;
        case SDL_SCANCODE_KP_PLUS: return GT_KEY_KP_PLUS;
        case SDL_SCANCODE_KP_ENTER: return GT_KEY_KP_ENTER;
        case SDL_SCANCODE_KP_1: return GT_KEY_KP_1;
        case SDL_SCANCODE_KP_2: return GT_KEY_KP_2;
        case SDL_SCANCODE_KP_3: return GT_KEY_KP_3;
        case SDL_SCANCODE_KP_4: return GT_KEY_KP_4;
        case SDL_SCANCODE_KP_5: return GT_KEY_KP_5;
        case SDL_SCANCODE_KP_6: return GT_KEY_KP_6;
        case SDL_SCANCODE_KP_7: return GT_KEY_KP_7;
        case SDL_SCANCODE_KP_8: return GT_KEY_KP_8;
        case SDL_SCANCODE_KP_9: return GT_KEY_KP_9;
        case SDL_SCANCODE_KP_0: return GT_KEY_KP_0;
        case SDL_SCANCODE_KP_PERIOD: return GT_KEY_KP_PERIOD;
        case SDL_SCANCODE_KP_EQUALS: return GT_KEY_KP_EQUALS;
        case SDL_SCANCODE_KP_000: return GT_KEY_KP_000;
        case SDL_SCANCODE_LCTRL: return GT_KEY_CTRL;
        case SDL_SCANCODE_RCTRL: return GT_KEY_CTRL;
        case SDL_SCANCODE_LSHIFT: return GT_KEY_SHIFT;
        case SDL_SCANCODE_RSHIFT: return GT_KEY_SHIFT;
        case SDL_SCANCODE_LALT: return GT_KEY_ALT;
        case SDL_SCANCODE_RALT: return GT_KEY_KANA_KANJI;
        case SDL_SCANCODE_INTERNATIONAL2: return GT_KEY_KATAKANA;
        case SDL_SCANCODE_INTERNATIONAL4: return GT_KEY_CONVERT;
        case SDL_SCANCODE_INTERNATIONAL5: return GT_KEY_NO_CONVERT;
        case SDL_SCANCODE_LANG3: return GT_KEY_KATAKANA;
        case SDL_SCANCODE_LANG4: return GT_KEY_HIRAGANA;
        case SDL_SCANCODE_LANG5: return GT_KEY_HALF_FULL;
        default: return GT_KEY_NONE;
    }
}
