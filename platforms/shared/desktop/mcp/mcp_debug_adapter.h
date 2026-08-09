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
    json ListRecentMedia();


private:
private:
    GeartownsCore* m_core;
};

#endif /* MCP_DEBUG_ADAPTER_H */
