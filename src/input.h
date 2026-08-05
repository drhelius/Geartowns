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

#ifndef INPUT_H
#define INPUT_H

#include "common.h"

class Input
{
public:
    Input();
    void Init();
    void Reset();
    void KeyPressed(GT_Keys key);
    void KeyReleased(GT_Keys key);
    bool IsKeyPressed(GT_Keys key) const;
    void SetMouseDelta(s32 x, s32 y);
    void SetMouseButtons(bool left, bool right);
    void SetGamePadState(int port, const GT_GamePad_State& state);
    void SetInjectedGamePadState(int port, const GT_GamePad_State& state);
    GT_GamePad_State GetGamePadState(int port) const;
    void SetControllerType(int port, GT_Controller_Type type);
    GT_Controller_Type GetControllerType(int port) const;

private:
    bool m_keys[GT_KEY_COUNT];
    s32 m_mouse_x;
    s32 m_mouse_y;
    bool m_mouse_left;
    bool m_mouse_right;
    GT_GamePad_State m_gamepads[GT_MAX_GAMEPADS];
    GT_GamePad_State m_injected_gamepads[GT_MAX_GAMEPADS];
    GT_Controller_Type m_controller_type[GT_MAX_GAMEPADS];
};

#include "input_inline.h"

#endif /* INPUT_H */