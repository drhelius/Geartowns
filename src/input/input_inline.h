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

// Right and down are negative on the wire
INLINE void Input::SetMouseDelta(int port, s32 x, s32 y)
{
    if (port < 0 || port >= GT_MAX_GAMEPADS)
        return;

    m_state.mouse_x[port] = (s32)CLAMP((s64)m_state.mouse_x[port] - x, -k_input_mouse_limit, k_input_mouse_limit);
    m_state.mouse_y[port] = (s32)CLAMP((s64)m_state.mouse_y[port] - y, -k_input_mouse_limit, k_input_mouse_limit);
}

INLINE void Input::SetGamePadState(int port, const GT_GamePad_State& state)
{
    if (port < 0 || port >= GT_MAX_GAMEPADS)
        return;

    m_physical_gamepads[port] = state;
    UpdateGamePadState(port);
}

INLINE void Input::SetInjectedGamePadState(int port, const GT_GamePad_State& state)
{
    if (port < 0 || port >= GT_MAX_GAMEPADS)
        return;

    m_injected_gamepads[port] = state;
    UpdateGamePadState(port);
}

INLINE const GT_GamePad_State& Input::GetGamePadState(int port) const
{
    static const GT_GamePad_State empty = { 0 };

    if (port < 0 || port >= GT_MAX_GAMEPADS)
        return empty;

    return m_state.gamepads[port];
}

INLINE void Input::SetControllerType(int port, GT_Controller_Type type)
{
    if (port < 0 || port >= GT_MAX_GAMEPADS)
        return;

    if (m_controller_type[port] != type)
        ClearMouseInput(port);

    m_controller_type[port] = type;
}

INLINE GT_Controller_Type Input::GetControllerType(int port) const
{
    if (port < 0 || port >= GT_MAX_GAMEPADS)
        return GT_CONTROLLER_NONE;

    return m_controller_type[port];
}

INLINE Input::Input_State* Input::GetState()
{
    return &m_state;
}
