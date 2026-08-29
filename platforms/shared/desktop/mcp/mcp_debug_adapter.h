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

#ifndef MCP_DEBUG_ADAPTER_H
#define MCP_DEBUG_ADAPTER_H

#include <string>
#include "json.hpp"
#include "geartowns.h"

using json = nlohmann::json;

class DebugAdapter
{
public:
    DebugAdapter(GeartownsCore* core);

    void Pause();
    void Resume();
    void Reset();
    json GetDebugStatus();
    json GetScreenshot();
    json GetMediaInfo();
    json StartLoadMedia(const std::string& file_path);
    bool IsMediaLoading() const;
    json FinishLoadMedia(const std::string& file_path);
    json ControllerButton(int player, const std::string& button,
        const std::string& action);
    json GetInputState();
    void ClearControllerState();

private:
    u16 ButtonMask(const std::string& button) const;
    void ApplyControllerState(int player);

private:
    GeartownsCore* m_core;
    u16 m_buttons[GT_MAX_GAMEPADS];
};

#endif /* MCP_DEBUG_ADAPTER_H */
