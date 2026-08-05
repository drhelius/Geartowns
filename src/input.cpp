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

#include "input.h"

Input::Input()
{
    for (int i = 0; i < GT_MAX_GAMEPADS; i++)
        m_controller_type[i] = GT_CONTROLLER_ORIGINAL_GAMEPAD;

    Reset();
}

void Input::Init()
{
    Reset();
}

void Input::Reset()
{
    memset(m_keys, 0, sizeof(m_keys));
    m_mouse_x = 0;
    m_mouse_y = 0;
    m_mouse_left = false;
    m_mouse_right = false;
    memset(m_gamepads, 0, sizeof(m_gamepads));
    memset(m_injected_gamepads, 0, sizeof(m_injected_gamepads));
}