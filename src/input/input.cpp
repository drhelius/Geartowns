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
#include "../common/state_serializer.h"

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
    memset(m_state.keys, 0, sizeof(m_state.keys));
    m_state.mouse_x = 0;
    m_state.mouse_y = 0;
    m_state.mouse_left = false;
    m_state.mouse_right = false;
    memset(m_physical_gamepads, 0, sizeof(m_physical_gamepads));
    memset(m_injected_gamepads, 0, sizeof(m_injected_gamepads));
    memset(m_state.gamepads, 0, sizeof(m_state.gamepads));
}

void Input::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void Input::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void Input::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE_ARRAY(serializer, m_state.keys, GT_KEY_COUNT);
    G_SERIALIZE(serializer, m_state.mouse_x);
    G_SERIALIZE(serializer, m_state.mouse_y);
    G_SERIALIZE(serializer, m_state.mouse_left);
    G_SERIALIZE(serializer, m_state.mouse_right);
    G_SERIALIZE_ARRAY(serializer, m_state.gamepads, GT_MAX_GAMEPADS);
}

void Input::SanitizeState()
{
    for (int i = 0; i < GT_MAX_GAMEPADS; i++)
    {
        m_physical_gamepads[i] = m_state.gamepads[i];
        memset(&m_injected_gamepads[i], 0, sizeof(m_injected_gamepads[i]));
    }
}

void Input::UpdateGamePadState(int port)
{
    if (port < 0 || port >= GT_MAX_GAMEPADS)
        return;

    m_state.gamepads[port] = m_physical_gamepads[port];
    m_state.gamepads[port].buttons |= m_injected_gamepads[port].buttons;
}
