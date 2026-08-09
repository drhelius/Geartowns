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

INLINE void Input::KeyPressed(GT_Keys key)
{
    if (key > GT_KEY_NONE && key < GT_KEY_COUNT)
        m_keys[key] = true;
}

INLINE void Input::KeyReleased(GT_Keys key)
{
    if (key > GT_KEY_NONE && key < GT_KEY_COUNT)
        m_keys[key] = false;
}

INLINE bool Input::IsKeyPressed(GT_Keys key) const
{
    if (key <= GT_KEY_NONE || key >= GT_KEY_COUNT)
        return false;

    return m_keys[key];
}

INLINE void Input::SetMouseDelta(s32 x, s32 y)
{
    m_mouse_x += x;
    m_mouse_y += y;
}

INLINE void Input::SetMouseButtons(bool left, bool right)
{
    m_mouse_left = left;
    m_mouse_right = right;
}

INLINE void Input::SetControllerType(int port, GT_Controller_Type type)
{
    if (port < 0 || port >= GT_MAX_GAMEPADS)
        return;

    m_controller_type[port] = type;
}

INLINE GT_Controller_Type Input::GetControllerType(int port) const
{
    if (port < 0 || port >= GT_MAX_GAMEPADS)
        return GT_CONTROLLER_NONE;

    return m_controller_type[port];
}