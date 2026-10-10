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

#include <iostream>
#include "../common/common.h"

class StateSerializer;
class TraceLogger;

class Input
{
public:
    struct Input_State
    {
        s32 mouse_x[GT_MAX_GAMEPADS];
        s32 mouse_y[GT_MAX_GAMEPADS];
        u8 mouse_phase[GT_MAX_GAMEPADS];
        u8 mouse_sample_x[GT_MAX_GAMEPADS];
        u8 mouse_sample_y[GT_MAX_GAMEPADS];
        u64 mouse_edge_clocks[GT_MAX_GAMEPADS];
        GT_GamePad_State gamepads[GT_MAX_GAMEPADS];
        u8 output;
    };

public:
    Input();
    void Init();
    void SetTraceLogger(TraceLogger* trace_logger);
    void Reset();
    u8 Read(u16 port);
    u8 Peek(u16 port) const;
    void Write(u16 port, u8 value, u64 clocks);
    void SetMouseDelta(int port, s32 x, s32 y);
    void ClearMouseInput(int port);
    void SetGamePadState(int port, const GT_GamePad_State& state);
    void SetInjectedGamePadState(int port, const GT_GamePad_State& state);
    const GT_GamePad_State& GetGamePadState(int port) const;
    void SetControllerType(int port, GT_Controller_Type type);
    GT_Controller_Type GetControllerType(int port) const;
    Input_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    u8 ReadGamePad(int port, bool com) const;
    u8 ReadMouse(int port) const;
    void MouseEdge(int port, u64 clocks);
    void UpdateGamePadState(int port);
    void TraceEvent(u8 event, int port, u8 value, u16 previous);
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    Input_State m_state;
    GT_GamePad_State m_physical_gamepads[GT_MAX_GAMEPADS];
    GT_GamePad_State m_injected_gamepads[GT_MAX_GAMEPADS];
    GT_Controller_Type m_controller_type[GT_MAX_GAMEPADS];
    TraceLogger* m_trace_logger;
};

static const u8 k_input_output_reset = 0x0F;
static const u8 k_input_com = 0x10;
static const u8 k_input_com_return = 0x40;
static const u8 k_input_trig_lines = 0x30;
static const u8 k_input_six_button_id = 0x80;
static const u8 k_input_mouse_x_high = 0;
static const u8 k_input_mouse_x_low = 1;
static const u8 k_input_mouse_y_high = 2;
static const u8 k_input_mouse_y_low = 3;
static const u64 k_input_mouse_timeout_clocks = GT_CPU_CLOCK_RATE / 1000;
static const u64 k_input_mouse_t2_timeout_clocks = ((u64)GT_CPU_CLOCK_RATE * 150) / 1000000;
static const s32 k_input_mouse_limit = 0x3FFFFFFF;

#include "input_inline.h"

#endif /* INPUT_H */
