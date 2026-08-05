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
#include "geartowns.h"
#include "config.h"
#include "gui.h"
#include "emu.h"
#include "application.h"
#include "gamepad.h"

#define EVENTS_IMPORT
#include "events.h"

static bool input_updated = false;
static u16 input_last_state[GT_MAX_GAMEPADS] = { };

static bool events_check_hotkey(const SDL_Event* event, const config_Hotkey& hotkey, bool allow_repeat);
static u16 input_build_state(int controller);
static u16 input_filter_opposing_directions(int controller, u16 state);
static void input_apply_state(int controller, u16 state);

void events_shortcuts(const SDL_Event* event)
{
    if (event->type != SDL_EVENT_KEY_DOWN)
        return;

    // Check special case hotkeys first
    if (events_check_hotkey(event, config_hotkeys[config_HotkeyIndex_Quit], false))
    {
        application_trigger_quit();
        return;
    }

    // Check all hotkeys mapped to gui shortcuts
    for (int i = 0; i < GUI_HOTKEY_MAP_COUNT; i++)
    {
        if (events_check_hotkey(event, config_hotkeys[gui_hotkey_map[i].config_index], gui_hotkey_map[i].allow_repeat))
        {
            gui_shortcut(gui_hotkey_map[i].shortcut);
            return;
        }
    }

    int key = event->key.scancode;

    // ESC to exit fullscreen
    if (event->key.repeat == 0 && key == SDL_SCANCODE_ESCAPE)
    {
        if (config_emulator.fullscreen && !config_emulator.always_show_menu)
        {
            config_emulator.fullscreen = false;
            application_trigger_fullscreen(false);
        }
    }
}

void events_handle_emu_event(const SDL_Event* event)
{
    UNUSED(event);
}

void events_emu(void)
{
    if (input_updated || gui_in_use)
        return;
    input_updated = true;

    SDL_PumpEvents();

    for (int controller = 0; controller < GT_MAX_GAMEPADS; controller++)
    {
        u16 now = input_filter_opposing_directions(controller, input_build_state(controller));
        u16 before = input_last_state[controller];

        if (now != before)
            input_apply_state(controller, now);

        input_last_state[controller] = now;

        gamepad_check_shortcuts(controller);
    }
}

void events_sync_input(void)
{
    SDL_PumpEvents();

    for (int controller = 0; controller < GT_MAX_GAMEPADS; controller++)
    {
        u16 now = input_filter_opposing_directions(controller, input_build_state(controller));
        input_apply_state(controller, now);
        input_last_state[controller] = now;
    }
}

void events_reset_input(void)
{
    input_updated = false;
}

bool events_input_updated(void)
{
    return input_updated;
}

static u16 input_build_state(int controller)
{
    SDL_Keymod modifiers = SDL_GetModState();
    if (modifiers & (SDL_KMOD_CTRL | SDL_KMOD_SHIFT | SDL_KMOD_ALT | SDL_KMOD_GUI))
        return 0;

    const bool* keyboard = SDL_GetKeyboardState(NULL);
    const config_Input_Keyboard& keys = config_input_keyboard[controller];
    u16 state = 0;

    if (keyboard[keys.key_left]) state |= GT_GAMEPAD_LEFT;
    if (keyboard[keys.key_right]) state |= GT_GAMEPAD_RIGHT;
    if (keyboard[keys.key_up]) state |= GT_GAMEPAD_UP;
    if (keyboard[keys.key_down]) state |= GT_GAMEPAD_DOWN;
    if (keyboard[keys.key_start]) state |= GT_GAMEPAD_START;
    if (keyboard[keys.key_run]) state |= GT_GAMEPAD_RUN;
    if (keyboard[keys.key_A]) state |= GT_GAMEPAD_A;
    if (keyboard[keys.key_B]) state |= GT_GAMEPAD_B;
    if (keyboard[keys.key_C]) state |= GT_GAMEPAD_C;
    if (keyboard[keys.key_X]) state |= GT_GAMEPAD_X;
    if (keyboard[keys.key_Y]) state |= GT_GAMEPAD_Y;
    if (keyboard[keys.key_Z]) state |= GT_GAMEPAD_Z;

    SDL_Gamepad* gamepad = gamepad_controller[controller];
    if (gamepad)
    {
        const config_Input_Gamepad& mapping = config_input_gamepad[controller];
        if (gamepad_get_button(gamepad, mapping.gamepad_start)) state |= GT_GAMEPAD_START;
        if (gamepad_get_button(gamepad, mapping.gamepad_run)) state |= GT_GAMEPAD_RUN;
        if (gamepad_get_button(gamepad, mapping.gamepad_A)) state |= GT_GAMEPAD_A;
        if (gamepad_get_button(gamepad, mapping.gamepad_B)) state |= GT_GAMEPAD_B;
        if (gamepad_get_button(gamepad, mapping.gamepad_C)) state |= GT_GAMEPAD_C;
        if (gamepad_get_button(gamepad, mapping.gamepad_X)) state |= GT_GAMEPAD_X;
        if (gamepad_get_button(gamepad, mapping.gamepad_Y)) state |= GT_GAMEPAD_Y;
        if (gamepad_get_button(gamepad, mapping.gamepad_Z)) state |= GT_GAMEPAD_Z;

        if (mapping.gamepad_directional == 0)
        {
            if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) state |= GT_GAMEPAD_LEFT;
            if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) state |= GT_GAMEPAD_RIGHT;
            if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP)) state |= GT_GAMEPAD_UP;
            if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) state |= GT_GAMEPAD_DOWN;
        }
        else
        {
            const Sint16 dead_zone = 8000;
            Sint16 x = SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)mapping.gamepad_x_axis);
            Sint16 y = SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)mapping.gamepad_y_axis);
            if (mapping.gamepad_invert_x_axis) x = (Sint16)-x;
            if (mapping.gamepad_invert_y_axis) y = (Sint16)-y;
            if (x < -dead_zone) state |= GT_GAMEPAD_LEFT;
            if (x > dead_zone) state |= GT_GAMEPAD_RIGHT;
            if (y < -dead_zone) state |= GT_GAMEPAD_UP;
            if (y > dead_zone) state |= GT_GAMEPAD_DOWN;
        }
    }

    if (config_input.controller_type[controller] != GT_CONTROLLER_6_BUTTON_GAMEPAD)
        state &= (u16)~(GT_GAMEPAD_C | GT_GAMEPAD_X | GT_GAMEPAD_Y | GT_GAMEPAD_Z);

    return state;
}

static u16 input_filter_opposing_directions(int controller, u16 state)
{
    if (config_input.allow_up_down)
        return state;

    u16 previous = input_last_state[controller];

    if ((state & GT_GAMEPAD_UP) && (state & GT_GAMEPAD_DOWN))
    {
        if (previous & GT_GAMEPAD_UP)
            state = (u16)(state & ~GT_GAMEPAD_DOWN);
        else if (previous & GT_GAMEPAD_DOWN)
            state = (u16)(state & ~GT_GAMEPAD_UP);
        else
            state = (u16)(state & ~GT_GAMEPAD_DOWN);
    }

    if ((state & GT_GAMEPAD_LEFT) && (state & GT_GAMEPAD_RIGHT))
    {
        if (previous & GT_GAMEPAD_LEFT)
            state = (u16)(state & ~GT_GAMEPAD_RIGHT);
        else if (previous & GT_GAMEPAD_RIGHT)
            state = (u16)(state & ~GT_GAMEPAD_LEFT);
        else
            state = (u16)(state & ~GT_GAMEPAD_RIGHT);
    }

    return state;
}

static void input_apply_state(int controller, u16 state)
{
    GT_GamePad_State gamepad_state = {state, 0, 0};
    emu_set_gamepad_state(controller, gamepad_state);
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

    SDL_Keymod mods_normalized = (SDL_Keymod)0;
    if (mods & (SDL_KMOD_LCTRL | SDL_KMOD_RCTRL)) mods_normalized = (SDL_Keymod)(mods_normalized | SDL_KMOD_CTRL);
    if (mods & (SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT)) mods_normalized = (SDL_Keymod)(mods_normalized | SDL_KMOD_SHIFT);
    if (mods & (SDL_KMOD_LALT | SDL_KMOD_RALT)) mods_normalized = (SDL_Keymod)(mods_normalized | SDL_KMOD_ALT);
    if (mods & (SDL_KMOD_LGUI | SDL_KMOD_RGUI)) mods_normalized = (SDL_Keymod)(mods_normalized | SDL_KMOD_GUI);

    SDL_Keymod expected_normalized = (SDL_Keymod)0;
    if (expected & (SDL_KMOD_LCTRL | SDL_KMOD_RCTRL | SDL_KMOD_CTRL)) expected_normalized = (SDL_Keymod)(expected_normalized | SDL_KMOD_CTRL);
    if (expected & (SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT | SDL_KMOD_SHIFT)) expected_normalized = (SDL_Keymod)(expected_normalized | SDL_KMOD_SHIFT);
    if (expected & (SDL_KMOD_LALT | SDL_KMOD_RALT | SDL_KMOD_ALT)) expected_normalized = (SDL_Keymod)(expected_normalized | SDL_KMOD_ALT);
    if (expected & (SDL_KMOD_LGUI | SDL_KMOD_RGUI | SDL_KMOD_GUI)) expected_normalized = (SDL_Keymod)(expected_normalized | SDL_KMOD_GUI);

    return mods_normalized == expected_normalized;
}
