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

#include <ctype.h>
#include "midi_port.h"

MidiPort::MidiPort()
{
    m_open = false;
    m_id[0] = 0;
    InitPointer(m_handle);
    m_client = 0;
    m_port = 0;
    m_endpoint = 0;
    InitPointer(m_parser);
}

MidiPort::~MidiPort()
{
    Close();
}

// A synth that names its port after its process, as "Name (1234)", is the same port at its next launch
static size_t midi_port_base_length(const char* name)
{
    size_t length = strlen(name);

    if ((length < 4) || (name[length - 1] != ')'))
        return length;

    size_t i = length - 1;

    while ((i > 0) && isdigit((unsigned char)name[i - 1]))
        i--;

    if ((i < length - 1) && (i >= 2) && (name[i - 1] == '(') && (name[i - 2] == ' '))
        return i - 2;

    return length;
}

bool MidiPort::IsSamePort(const char* a, const char* b)
{
    size_t length = midi_port_base_length(a);
    return (length == midi_port_base_length(b)) && (strncmp(a, b, length) == 0);
}

bool MidiPort::FindPort(const char* id, char* found, int size)
{
    std::vector<MidiPortInfo> ports;

    if ((id[0] == 0) || !ListPorts(ports))
        return false;

    int match = -1;

    for (int i = 0; (i < (int)ports.size()) && (match < 0); i++)
    {
        if (strcmp(ports[i].id, id) == 0)
            match = i;
    }

    for (int i = 0; (i < (int)ports.size()) && (match < 0); i++)
    {
        if (IsSamePort(ports[i].id, id))
            match = i;
    }

    if (match < 0)
        return false;

    snprintf(found, size, "%s", ports[match].id);
    return true;
}

bool MidiPort::Open(const char* id)
{
    Close();
    char found[MIDI_PORT_NAME_SIZE];

    if (!FindPort(id, found, sizeof(found)) || !OpenNative(found))
        return false;

    snprintf(m_id, sizeof(m_id), "%s", found);
    m_open = true;
    return true;
}

void MidiPort::Close()
{
    if (!m_open)
        return;

    CloseNative();
    m_open = false;
    m_id[0] = 0;
}

bool MidiPort::IsOpen() const
{
    return m_open;
}

const char* MidiPort::GetId() const
{
    return m_id;
}

#if !defined(GT_ENABLE_MIDI_PORT)

bool MidiPort::IsSupported()
{
    return false;
}

bool MidiPort::ListPorts(std::vector<MidiPortInfo>& ports)
{
    ports.clear();
    return false;
}

bool MidiPort::OpenNative(const char* id)
{
    UNUSED(id);
    return false;
}

bool MidiPort::IsAlive()
{
    return false;
}

void MidiPort::CloseNative()
{
}

bool MidiPort::Send(const u8* data, int size)
{
    UNUSED(data);
    UNUSED(size);
    return false;
}

#endif
