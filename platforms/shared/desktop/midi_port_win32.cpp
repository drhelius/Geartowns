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

#include "midi_port.h"

#if defined(GT_ENABLE_MIDI_PORT) && defined(_WIN32)

#include <mmsystem.h>

#define MIDI_PORT_SYSEX_TIMEOUT_MS 5000

// Devices with the same name get " #2" and on, in the order Windows lists them
static void midi_port_names(std::vector<MidiPortInfo>& ports)
{
    ports.clear();
    std::vector<MidiPortInfo> names;
    UINT count = midiOutGetNumDevs();

    for (UINT i = 0; i < count; i++)
    {
        MIDIOUTCAPSA caps;
        MidiPortInfo info;

        if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
            caps.szPname[0] = 0;

        int same = 0;

        for (size_t j = 0; j < names.size(); j++)
        {
            if (strcmp(names[j].id, caps.szPname) == 0)
                same++;
        }

        snprintf(info.id, sizeof(info.id), "%s", caps.szPname);
        names.push_back(info);

        if (same == 0)
            snprintf(info.id, sizeof(info.id), "%s", caps.szPname);
        else
            snprintf(info.id, sizeof(info.id), "%s #%d", caps.szPname, same + 1);

        ports.push_back(info);
    }
}

bool MidiPort::IsSupported()
{
    return true;
}

bool MidiPort::ListPorts(std::vector<MidiPortInfo>& ports)
{
    midi_port_names(ports);
    return true;
}

bool MidiPort::OpenNative(const char* id)
{
    std::vector<MidiPortInfo> ports;
    midi_port_names(ports);

    for (size_t i = 0; i < ports.size(); i++)
    {
        if (strcmp(ports[i].id, id) != 0)
            continue;

        HMIDIOUT handle = NULL;

        if (midiOutOpen(&handle, (UINT)i, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
            return false;

        m_handle = handle;
        m_endpoint = (u32)i;
        return true;
    }

    return false;
}

bool MidiPort::IsAlive()
{
    std::vector<MidiPortInfo> ports;
    midi_port_names(ports);
    return m_open && (m_endpoint < ports.size()) && (strcmp(ports[m_endpoint].id, m_id) == 0);
}

void MidiPort::CloseNative()
{
    HMIDIOUT handle = (HMIDIOUT)m_handle;

    if (handle != NULL)
    {
        midiOutReset(handle);
        midiOutClose(handle);
    }

    InitPointer(m_handle);
    m_endpoint = 0;
}

bool MidiPort::Send(const u8* data, int size)
{
    HMIDIOUT handle = (HMIDIOUT)m_handle;

    if (!m_open || (handle == NULL) || (size <= 0))
        return false;

    if (data[0] != 0xF0)
    {
        DWORD message = data[0] | (size > 1 ? data[1] << 8 : 0) | (size > 2 ? data[2] << 16 : 0);
        return midiOutShortMsg(handle, message) == MMSYSERR_NOERROR;
    }

    MIDIHDR header;
    memset(&header, 0, sizeof(header));
    header.lpData = (LPSTR)data;
    header.dwBufferLength = (DWORD)size;
    header.dwBytesRecorded = (DWORD)size;

    if (midiOutPrepareHeader(handle, &header, sizeof(header)) != MMSYSERR_NOERROR)
        return false;

    bool ok = midiOutLongMsg(handle, &header, sizeof(header)) == MMSYSERR_NOERROR;

    for (int waited = 0; ok && ((header.dwFlags & MHDR_DONE) == 0) && (waited < MIDI_PORT_SYSEX_TIMEOUT_MS); waited++)
        Sleep(1);

    while (midiOutUnprepareHeader(handle, &header, sizeof(header)) == MIDIERR_STILLPLAYING)
        Sleep(1);

    return ok;
}

#endif
