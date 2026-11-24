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

#ifndef MIDI_PORT_H
#define MIDI_PORT_H

#include <vector>
#include "geartowns.h"

#define MIDI_PORT_NAME_SIZE 256

struct MidiPortInfo
{
    char id[MIDI_PORT_NAME_SIZE];
};

class MidiPort
{
public:
    MidiPort();
    ~MidiPort();
    static bool IsSupported();
    static bool ListPorts(std::vector<MidiPortInfo>& ports);
    static bool IsSamePort(const char* a, const char* b);
    static bool FindPort(const char* id, char* found, int size);
    bool Open(const char* id);
    void Close();
    bool IsOpen() const;
    bool IsAlive();
    const char* GetId() const;
    bool Send(const u8* data, int size);

private:
    bool OpenNative(const char* id);
    void CloseNative();

private:
    bool m_open;
    char m_id[MIDI_PORT_NAME_SIZE];
    void* m_handle;
    u32 m_client;
    u32 m_port;
    u32 m_endpoint;
    void* m_parser;
};

#endif /* MIDI_PORT_H */
