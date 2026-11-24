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

#if defined(GT_ENABLE_MIDI_PORT) && defined(__linux__)

#include <alsa/asoundlib.h>

#define MIDI_PORT_ENCODER_SIZE 16

struct Midi_Port_Address
{
    int client;
    int port;
};

static bool midi_port_open_sequencer(snd_seq_t** seq)
{
    if (snd_seq_open(seq, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0)
        return false;

    snd_seq_set_client_name(*seq, "Geartowns");
    return true;
}

// Ports that take events and subscriptions, named "client:port" without the numbers, which change between runs
static void midi_port_scan(snd_seq_t* seq, std::vector<MidiPortInfo>* ports, const char* id, Midi_Port_Address* found)
{
    snd_seq_client_info_t* client;
    snd_seq_port_info_t* port;
    snd_seq_client_info_alloca(&client);
    snd_seq_port_info_alloca(&port);
    snd_seq_client_info_set_client(client, -1);
    unsigned int caps = SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
    int self = snd_seq_client_id(seq);

    while (snd_seq_query_next_client(seq, client) >= 0)
    {
        int number = snd_seq_client_info_get_client(client);

        if ((number == SND_SEQ_CLIENT_SYSTEM) || (number == self))
            continue;

        snd_seq_port_info_set_client(port, number);
        snd_seq_port_info_set_port(port, -1);

        while (snd_seq_query_next_port(seq, port) >= 0)
        {
            if ((snd_seq_port_info_get_capability(port) & caps) != caps)
                continue;

            MidiPortInfo info;
            snprintf(info.id, sizeof(info.id), "%s:%s", snd_seq_client_info_get_name(client),
                snd_seq_port_info_get_name(port));

            if (IsValidPointer(ports))
                ports->push_back(info);

            if (IsValidPointer(found) && (found->client < 0) && (strcmp(info.id, id) == 0))
            {
                found->client = number;
                found->port = snd_seq_port_info_get_port(port);
            }
        }
    }
}

bool MidiPort::IsSupported()
{
    return true;
}

bool MidiPort::ListPorts(std::vector<MidiPortInfo>& ports)
{
    ports.clear();
    snd_seq_t* seq = NULL;

    if (!midi_port_open_sequencer(&seq))
        return false;

    midi_port_scan(seq, &ports, NULL, NULL);
    snd_seq_close(seq);
    return true;
}

bool MidiPort::OpenNative(const char* id)
{
    snd_seq_t* seq = NULL;

    if (!midi_port_open_sequencer(&seq))
        return false;

    Midi_Port_Address address = { -1, -1 };
    midi_port_scan(seq, NULL, id, &address);
    unsigned int caps = SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
    unsigned int type = SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION;
    int port = address.client >= 0 ? snd_seq_create_simple_port(seq, "MIDI Out", caps, type) : -1;
    snd_midi_event_t* parser = NULL;

    if ((port < 0) || (snd_seq_connect_to(seq, port, address.client, address.port) < 0) ||
        (snd_midi_event_new(MIDI_PORT_ENCODER_SIZE, &parser) < 0))
    {
        snd_seq_close(seq);
        return false;
    }

    m_handle = seq;
    m_parser = parser;
    m_port = (u32)port;
    m_client = (u32)address.client;
    m_endpoint = (u32)address.port;
    return true;
}

bool MidiPort::IsAlive()
{
    if (!m_open)
        return false;

    snd_seq_port_info_t* info;
    snd_seq_port_info_alloca(&info);
    return snd_seq_get_any_port_info((snd_seq_t*)m_handle, (int)m_client, (int)m_endpoint, info) >= 0;
}

void MidiPort::CloseNative()
{
    if (IsValidPointer(m_parser))
        snd_midi_event_free((snd_midi_event_t*)m_parser);

    if (IsValidPointer(m_handle))
        snd_seq_close((snd_seq_t*)m_handle);

    InitPointer(m_parser);
    InitPointer(m_handle);
    m_port = 0;
    m_client = 0;
    m_endpoint = 0;
}

bool MidiPort::Send(const u8* data, int size)
{
    if (!m_open || (size <= 0))
        return false;

    snd_seq_t* seq = (snd_seq_t*)m_handle;
    snd_seq_event_t event;
    snd_seq_ev_clear(&event);

    if (data[0] == 0xF0)
        snd_seq_ev_set_sysex(&event, size, (void*)data);
    else
    {
        snd_midi_event_t* parser = (snd_midi_event_t*)m_parser;
        snd_midi_event_reset_encode(parser);

        if ((snd_midi_event_encode(parser, data, size, &event) < size) || (event.type == SND_SEQ_EVENT_NONE))
            return false;
    }

    snd_seq_ev_set_source(&event, (int)m_port);
    snd_seq_ev_set_subs(&event);
    snd_seq_ev_set_direct(&event);
    return snd_seq_event_output_direct(seq, &event) >= 0;
}

#endif
