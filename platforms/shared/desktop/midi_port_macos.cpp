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

#if defined(GT_ENABLE_MIDI_PORT) && defined(__APPLE__)

#include <CoreMIDI/CoreMIDI.h>
#include <dispatch/dispatch.h>
#include <pthread.h>

#define MIDI_PORT_PACKET_LIST_SIZE 8192

static MIDIClientRef midi_port_client = 0;
static bool midi_port_started = false;
static dispatch_semaphore_t midi_port_ready = NULL;

static void midi_port_idle(CFRunLoopTimerRef timer, void* info)
{
    UNUSED(timer);
    UNUSED(info);
}

// CoreMIDI only learns that a synth quit or started through the run loop its client was created on
static void* midi_port_run(void* data)
{
    UNUSED(data);
    MIDIClientRef client = 0;

    if (MIDIClientCreate(CFSTR("Geartowns"), NULL, NULL, &client) != noErr)
        client = 0;

    midi_port_client = client;
    dispatch_semaphore_signal(midi_port_ready);

    if (client == 0)
        return NULL;

    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + 1.0e10, 1.0e10, 0, 0,
        midi_port_idle, NULL);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
    CFRunLoopRun();
    return NULL;
}

static bool midi_port_start(void)
{
    if (!midi_port_started)
    {
        pthread_t thread;
        midi_port_started = true;
        midi_port_ready = dispatch_semaphore_create(0);

        if (pthread_create(&thread, NULL, midi_port_run, NULL) != 0)
            return false;

        pthread_detach(thread);
        dispatch_semaphore_wait(midi_port_ready, DISPATCH_TIME_FOREVER);
    }

    return midi_port_client != 0;
}

static bool midi_port_get_name(MIDIEndpointRef endpoint, char* name, int size)
{
    CFStringRef string = NULL;
    name[0] = 0;

    if ((MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &string) != noErr) || (string == NULL))
        return false;

    bool ok = CFStringGetCString(string, name, size, kCFStringEncodingUTF8);
    CFRelease(string);
    return ok && (name[0] != 0);
}

bool MidiPort::IsSupported()
{
    return true;
}

// Destinations include the virtual ones synth programs create, which belong to no device
bool MidiPort::ListPorts(std::vector<MidiPortInfo>& ports)
{
    ports.clear();

    if (!midi_port_start())
        return false;
    ItemCount count = MIDIGetNumberOfDestinations();

    for (ItemCount i = 0; i < count; i++)
    {
        MidiPortInfo info;

        if (midi_port_get_name(MIDIGetDestination(i), info.id, sizeof(info.id)))
            ports.push_back(info);
    }

    return true;
}

bool MidiPort::OpenNative(const char* id)
{
    if (!midi_port_start())
        return false;

    ItemCount count = MIDIGetNumberOfDestinations();
    MIDIEndpointRef endpoint = 0;
    char name[MIDI_PORT_NAME_SIZE];

    for (ItemCount i = 0; (i < count) && (endpoint == 0); i++)
    {
        MIDIEndpointRef destination = MIDIGetDestination(i);

        if (midi_port_get_name(destination, name, sizeof(name)) && (strcmp(name, id) == 0))
            endpoint = destination;
    }

    MIDIPortRef port = 0;

    if ((endpoint == 0) || (MIDIOutputPortCreate(midi_port_client, CFSTR("MIDI Out"), &port) != noErr))
        return false;

    m_port = port;
    m_endpoint = endpoint;
    return true;
}

// A synth that quits takes its destination with it, and sending to it still succeeds
bool MidiPort::IsAlive()
{
    SInt32 id = 0;

    if (!m_open || (MIDIObjectGetIntegerProperty(m_endpoint, kMIDIPropertyUniqueID, &id) != noErr))
        return false;

    ItemCount count = MIDIGetNumberOfDestinations();

    for (ItemCount i = 0; m_open && (i < count); i++)
    {
        if (MIDIGetDestination(i) == m_endpoint)
            return true;
    }

    return false;
}

void MidiPort::CloseNative()
{
    if (m_port != 0)
        MIDIPortDispose(m_port);

    m_port = 0;
    m_endpoint = 0;
}

bool MidiPort::Send(const u8* data, int size)
{
    if (!m_open || (size <= 0) || (size > (MIDI_PORT_PACKET_LIST_SIZE - 256)))
        return false;

    Byte buffer[MIDI_PORT_PACKET_LIST_SIZE];
    MIDIPacketList* list = (MIDIPacketList*)buffer;
    MIDIPacket* packet = MIDIPacketListInit(list);
    packet = MIDIPacketListAdd(list, sizeof(buffer), packet, 0, size, data);

    return (packet != NULL) && (MIDISend(m_port, m_endpoint, list) == noErr);
}

#endif
