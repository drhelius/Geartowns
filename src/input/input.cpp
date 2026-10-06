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
    memset(&m_state, 0, sizeof(m_state));
    memset(m_physical_gamepads, 0, sizeof(m_physical_gamepads));
    memset(m_injected_gamepads, 0, sizeof(m_injected_gamepads));
    m_state.output = k_input_output_reset;

    for (int i = 0; i < GT_MAX_GAMEPADS; i++)
        m_state.mouse_phase[i] = k_input_mouse_y_low;
}

u8 Input::Read(u16 port) const
{
    int index = port == 0x04D2 ? 1 : 0;
    bool com = (m_state.output & (k_input_com << index)) != 0;
    u8 trig = (u8)((m_state.output >> (index * 2)) & 0x03);
    u8 value = 0x3F;

    switch (m_controller_type[index])
    {
        // Marty's Zoom is not decoded yet, its line and polarity are unknown
        case GT_CONTROLLER_ORIGINAL_GAMEPAD:
        case GT_CONTROLLER_MARTY_GAMEPAD:
            value = ReadGamePad(index, false);
            break;
        case GT_CONTROLLER_6_BUTTON_GAMEPAD:
            return (u8)(ReadGamePad(index, com) | (com ? k_input_com_return : 0) | k_input_six_button_id);
        case GT_CONTROLLER_MOUSE:
            value = ReadMouse(index);
            break;
        default:
            break;
    }

    if (com)
        value |= k_input_com_return;

    return value & (u8)(~k_input_trig_lines | (trig << 4));
}

void Input::Write(u16 port, u8 value, u64 clocks)
{
    if (port != 0x04D6)
        return;

    u8 changed = m_state.output ^ value;
    m_state.output = value;

    for (int i = 0; i < GT_MAX_GAMEPADS; i++)
    {
        if ((changed & (k_input_com << i)) != 0)
            MouseEdge(i, clocks);
    }
}

void Input::ClearMouseInput(int port)
{
    if (port < 0 || port >= GT_MAX_GAMEPADS)
        return;

    m_state.mouse_x[port] = 0;
    m_state.mouse_y[port] = 0;
    m_state.mouse_sample_x[port] = 0;
    m_state.mouse_sample_y[port] = 0;
    m_state.mouse_phase[port] = k_input_mouse_y_low;
}

u8 Input::ReadGamePad(int port, bool com) const
{
    u16 buttons = m_state.gamepads[port].buttons;
    u8 lines = 0;

    if (com)
    {
        lines |= (buttons & GT_GAMEPAD_Z) ? 0x01 : 0;
        lines |= (buttons & GT_GAMEPAD_Y) ? 0x02 : 0;
        lines |= (buttons & GT_GAMEPAD_X) ? 0x04 : 0;
        lines |= (buttons & GT_GAMEPAD_C) ? 0x08 : 0;
    }
    else
    {
        lines |= (buttons & GT_GAMEPAD_UP) ? 0x01 : 0;
        lines |= (buttons & GT_GAMEPAD_DOWN) ? 0x02 : 0;
        lines |= (buttons & GT_GAMEPAD_LEFT) ? 0x04 : 0;
        lines |= (buttons & GT_GAMEPAD_RIGHT) ? 0x08 : 0;
        lines |= (buttons & GT_GAMEPAD_RUN) ? 0x0C : 0;
        lines |= (buttons & GT_GAMEPAD_SELECT) ? 0x03 : 0;
    }

    lines |= (buttons & GT_GAMEPAD_A) ? 0x10 : 0;
    lines |= (buttons & GT_GAMEPAD_B) ? 0x20 : 0;
    return (u8)(~lines & 0x3F);
}

u8 Input::ReadMouse(int port) const
{
    u8 value = 0;

    switch (m_state.mouse_phase[port])
    {
        case k_input_mouse_x_high:
            value = m_state.mouse_sample_x[port] >> 4;
            break;
        case k_input_mouse_x_low:
            value = m_state.mouse_sample_x[port] & 0x0F;
            break;
        case k_input_mouse_y_high:
            value = m_state.mouse_sample_y[port] >> 4;
            break;
        default:
            value = m_state.mouse_sample_y[port] & 0x0F;
            break;
    }

    u16 buttons = m_state.gamepads[port].buttons;

    if ((buttons & GT_GAMEPAD_A) == 0)
        value |= 0x10;

    if ((buttons & GT_GAMEPAD_B) == 0)
        value |= 0x20;

    return value;
}

// Each COM edge presents the next nibble, and the first edge after a pause longer than the timeout starts a packet
// The packet takes the motion at that edge and the rest waits for the next one
// Each axis stops at 127 counts both ways because Towns OS negates the byte and would turn -128 around
void Input::MouseEdge(int port, u64 clocks)
{
    if (clocks - m_state.mouse_edge_clocks[port] > k_input_mouse_timeout_clocks)
        m_state.mouse_phase[port] = k_input_mouse_y_low;

    m_state.mouse_phase[port] = (m_state.mouse_phase[port] + 1) & 0x03;
    m_state.mouse_edge_clocks[port] = clocks;

    if (m_state.mouse_phase[port] != k_input_mouse_x_high)
        return;

    s32 x = CLAMP(m_state.mouse_x[port], -127, 127);
    s32 y = CLAMP(m_state.mouse_y[port], -127, 127);
    m_state.mouse_x[port] -= x;
    m_state.mouse_y[port] -= y;
    m_state.mouse_sample_x[port] = (u8)x;
    m_state.mouse_sample_y[port] = (u8)y;
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
    G_SERIALIZE_ARRAY(serializer, m_state.mouse_x, GT_MAX_GAMEPADS);
    G_SERIALIZE_ARRAY(serializer, m_state.mouse_y, GT_MAX_GAMEPADS);
    G_SERIALIZE_ARRAY(serializer, m_state.mouse_phase, GT_MAX_GAMEPADS);
    G_SERIALIZE_ARRAY(serializer, m_state.mouse_sample_x, GT_MAX_GAMEPADS);
    G_SERIALIZE_ARRAY(serializer, m_state.mouse_sample_y, GT_MAX_GAMEPADS);
    G_SERIALIZE_ARRAY(serializer, m_state.mouse_edge_clocks, GT_MAX_GAMEPADS);
    G_SERIALIZE_ARRAY(serializer, m_state.gamepads, GT_MAX_GAMEPADS);
    G_SERIALIZE(serializer, m_state.output);
}

void Input::SanitizeState()
{
    for (int i = 0; i < GT_MAX_GAMEPADS; i++)
    {
        m_state.mouse_x[i] = CLAMP(m_state.mouse_x[i], -k_input_mouse_limit, k_input_mouse_limit);
        m_state.mouse_y[i] = CLAMP(m_state.mouse_y[i], -k_input_mouse_limit, k_input_mouse_limit);
        m_state.mouse_phase[i] &= 0x03;
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
