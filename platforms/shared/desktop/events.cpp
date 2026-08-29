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

static bool events_check_hotkey(const SDL_Event* event, const config_Hotkey& hotkey,
    bool allow_repeat);
static bool events_match_hotkey_scancode(const SDL_Event* event,
    const config_Hotkey& hotkey);
static void input_update(bool check_shortcuts);
static GT_GamePad_State input_build_state(int controller);
static u16 input_filter_opposing_directions(int controller, u16 buttons);

void events_shortcuts(const SDL_Event* event)
{
    if (event->type == SDL_EVENT_KEY_UP)
    {
        if (events_match_hotkey_scancode(event,
            config_hotkeys[config_HotkeyIndex_Rewind]))
        {
            gui_action_rewind_released();
        }
        return;
    }

    if (event->type != SDL_EVENT_KEY_DOWN)
        return;

    if (events_check_hotkey(event, config_hotkeys[config_HotkeyIndex_Rewind], false))
    {
        gui_action_rewind_pressed();
        return;
    }

    if (events_check_hotkey(event, config_hotkeys[config_HotkeyIndex_Quit], false))
    {
        application_trigger_quit();
        return;
    }

    for (int i = 0; i < GUI_HOTKEY_MAP_COUNT; i++)
    {
        const gui_HotkeyMapping& mapping = gui_hotkey_map[i];
        if (events_check_hotkey(event, config_hotkeys[mapping.config_index],
            mapping.allow_repeat))
        {
            gui_shortcut(mapping.shortcut);
            return;
        }
    }

    if (event->key.repeat == 0 && event->key.scancode == SDL_SCANCODE_ESCAPE &&
        config_emulator.fullscreen && !config_emulator.always_show_menu)
    {
        application_trigger_fullscreen(false);
    }
}

void events_emu(const SDL_Event* event)
{
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
    SDL_PumpEvents();
    input_update(false);
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
        GT_GamePad_State state = { 0, 0, 0 };
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
    GT_GamePad_State state = { 0, 0, 0 };
    SDL_Keymod mods = SDL_GetModState();

    if ((mods & (SDL_KMOD_CTRL | SDL_KMOD_SHIFT | SDL_KMOD_ALT | SDL_KMOD_GUI)) == 0)
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

    if (mapping.gamepad_directional == 0)
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
    else
    {
        int x = SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)mapping.gamepad_x_axis);
        int y = SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)mapping.gamepad_y_axis);

        if (mapping.gamepad_invert_x_axis) x = -x;
        if (mapping.gamepad_invert_y_axis) y = -y;

        state.axis_x = (s16)x;
        state.axis_y = (s16)y;

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

static bool events_check_hotkey(const SDL_Event* event, const config_Hotkey& hotkey,
    bool allow_repeat)
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

static bool events_match_hotkey_scancode(const SDL_Event* event,
    const config_Hotkey& hotkey)
{
    if (event->type != SDL_EVENT_KEY_UP && event->type != SDL_EVENT_KEY_DOWN)
        return false;
    if (hotkey.key == SDL_SCANCODE_UNKNOWN)
        return false;
    return event->key.scancode == hotkey.key;
}
