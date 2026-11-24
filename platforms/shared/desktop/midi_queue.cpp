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

#define MIDI_QUEUE_IMPORT
#include "midi_queue.h"
#include <SDL3/SDL.h>
#include "midi_port.h"
#include "sound_queue.h"

#define MIDI_QUEUE_ENTRIES 0x4000
#define MIDI_QUEUE_DATA_SIZE 0x40000

static const u64 k_midi_queue_smooth_ns = 20000000;

struct Midi_Queue_Entry
{
    u64 time;
    u32 data;
    u32 size;
};

static SDL_Thread* midi_queue_thread = NULL;
static SDL_Mutex* midi_queue_mutex = NULL;
static SDL_Mutex* midi_queue_port_mutex = NULL;
static SDL_Condition* midi_queue_condition = NULL;
static bool midi_queue_quit = false;
static bool midi_queue_preparing = false;
static MidiPort midi_queue_port;
static SDL_AtomicInt midi_queue_lost;
static SDL_AtomicInt midi_queue_ready;
static Midi_Queue_Entry midi_queue_entries[MIDI_QUEUE_ENTRIES];
static u8 midi_queue_data[MIDI_QUEUE_DATA_SIZE];
static int midi_queue_head = 0;
static int midi_queue_count = 0;
static u32 midi_queue_data_head = 0;
static u32 midi_queue_data_used = 0;
static u32 midi_queue_dropped = 0;
static u64 midi_queue_next_frame = 0;

static int SDLCALL midi_queue_run(void* data);
static void midi_queue_push(MidiOut* midi, u64 base);
static void midi_queue_clear(void);

void midi_queue_init(void)
{
    midi_queue_mutex = SDL_CreateMutex();
    midi_queue_port_mutex = SDL_CreateMutex();
    midi_queue_condition = SDL_CreateCondition();
    midi_queue_quit = false;
    midi_queue_preparing = false;
    SDL_SetAtomicInt(&midi_queue_lost, 0);
    SDL_SetAtomicInt(&midi_queue_ready, 0);
    midi_queue_clear();

    if (IsValidPointer(midi_queue_mutex) && IsValidPointer(midi_queue_port_mutex) && IsValidPointer(midi_queue_condition))
        midi_queue_thread = SDL_CreateThread(midi_queue_run, "MIDI Out", NULL);

    if (!IsValidPointer(midi_queue_thread))
        Log("MIDI Out: unable to start the sending thread: %s", SDL_GetError());
}

void midi_queue_destroy(void)
{
    if (IsValidPointer(midi_queue_thread))
    {
        SDL_LockMutex(midi_queue_mutex);
        midi_queue_quit = true;
        SDL_SignalCondition(midi_queue_condition);
        SDL_UnlockMutex(midi_queue_mutex);
        SDL_WaitThread(midi_queue_thread, NULL);
        midi_queue_thread = NULL;
    }

    midi_queue_port.Close();

    if (IsValidPointer(midi_queue_condition))
        SDL_DestroyCondition(midi_queue_condition);

    if (IsValidPointer(midi_queue_port_mutex))
        SDL_DestroyMutex(midi_queue_port_mutex);

    if (IsValidPointer(midi_queue_mutex))
        SDL_DestroyMutex(midi_queue_mutex);

    midi_queue_condition = NULL;
    midi_queue_port_mutex = NULL;
    midi_queue_mutex = NULL;
}

bool midi_queue_prepare(void)
{
    if (!IsValidPointer(midi_queue_thread) || (SDL_GetAtomicInt(&midi_queue_ready) != 0))
        return true;

    SDL_LockMutex(midi_queue_mutex);
    midi_queue_preparing = true;
    SDL_SignalCondition(midi_queue_condition);
    SDL_UnlockMutex(midi_queue_mutex);
    return false;
}

bool midi_queue_open(const char* id)
{
    if (!IsValidPointer(midi_queue_thread))
        return false;

    midi_queue_close();
    SDL_LockMutex(midi_queue_port_mutex);
    bool opened = midi_queue_port.Open(id);
    SDL_UnlockMutex(midi_queue_port_mutex);
    SDL_SetAtomicInt(&midi_queue_lost, 0);
    midi_queue_next_frame = 0;

    if (opened)
        Log("MIDI Out: %s", midi_queue_port.GetId());

    return opened;
}

void midi_queue_close(void)
{
    if (!IsValidPointer(midi_queue_thread))
        return;

    SDL_LockMutex(midi_queue_mutex);
    midi_queue_clear();
    SDL_UnlockMutex(midi_queue_mutex);
    SDL_LockMutex(midi_queue_port_mutex);
    midi_queue_port.Close();
    SDL_UnlockMutex(midi_queue_port_mutex);
}

bool midi_queue_is_open(void)
{
    return midi_queue_port.IsOpen();
}

bool midi_queue_is_lost(void)
{
    return SDL_GetAtomicInt(&midi_queue_lost) != 0;
}

void midi_queue_check(void)
{
    if (!midi_queue_is_open())
        return;

    SDL_LockMutex(midi_queue_port_mutex);
    bool alive = midi_queue_port.IsAlive();
    SDL_UnlockMutex(midi_queue_port_mutex);

    if (!alive)
        SDL_SetAtomicInt(&midi_queue_lost, 1);
}

const char* midi_queue_get_port(void)
{
    return midi_queue_port.GetId();
}

// A frame's first sample plays once the audio queued ahead of it has played. Frames follow each other, so their start
// times are kept apart by their lengths and only drift slowly toward the measured times, away from the queue's jitter
void midi_queue_frame(MidiOut* midi, int frame_samples)
{
    if (!midi_queue_is_open() || (midi->GetMessageCount() == 0 && frame_samples <= 0))
    {
        midi->Clear();
        return;
    }

    u64 frame_ns = ((u64)MAX(frame_samples, 0) * 1000000000ULL) / GT_AUDIO_SAMPLE_RATE;
    u64 now = SDL_GetTicksNS();
    u64 queued = sound_queue_is_open() ? (u64)(sound_queue_get_queued_seconds() * 1000000000.0) : frame_ns * 3;
    u64 measured = now + (queued > frame_ns ? queued - frame_ns : 0);
    s64 error = (s64)(measured - midi_queue_next_frame);
    u64 base = measured;

    if ((midi_queue_next_frame != 0) && ((u64)(error < 0 ? -error : error) < k_midi_queue_smooth_ns))
        base = midi_queue_next_frame + error / 16;

    midi_queue_next_frame = base + frame_ns;
    midi_queue_push(midi, base);
}

void midi_queue_flush(MidiOut* midi)
{
    if (midi_queue_is_open())
        midi_queue_push(midi, 0);
    else
        midi->Clear();
}

void midi_queue_drain(int timeout_ms)
{
    if (!IsValidPointer(midi_queue_thread))
        return;

    for (int i = 0; i < timeout_ms; i++)
    {
        SDL_LockMutex(midi_queue_mutex);
        bool empty = midi_queue_count == 0;
        SDL_UnlockMutex(midi_queue_mutex);

        if (empty)
            return;

        SDL_Delay(1);
    }
}

u32 midi_queue_get_dropped(void)
{
    return midi_queue_dropped;
}

static void midi_queue_push(MidiOut* midi, u64 base)
{
    int count = midi->GetMessageCount();
    const MidiOut::MidiOut_Message* messages = midi->GetMessages();
    const u8* data = midi->GetData();

    if (count == 0)
        return;

    SDL_LockMutex(midi_queue_mutex);

    for (int i = 0; i < count; i++)
    {
        const MidiOut::MidiOut_Message& message = messages[i];

        if ((midi_queue_count >= MIDI_QUEUE_ENTRIES) || ((midi_queue_data_used + message.size) > MIDI_QUEUE_DATA_SIZE))
        {
            midi_queue_dropped++;
            continue;
        }

        Midi_Queue_Entry& entry = midi_queue_entries[(midi_queue_head + midi_queue_count) % MIDI_QUEUE_ENTRIES];
        entry.time = (base == 0) || (message.offset == k_midi_out_immediate) ? 0 :
            base + ((u64)message.offset * 1000000000ULL) / GT_AUDIO_SAMPLE_RATE;
        entry.data = (midi_queue_data_head + midi_queue_data_used) % MIDI_QUEUE_DATA_SIZE;
        entry.size = message.size;

        for (u32 j = 0; j < message.size; j++)
            midi_queue_data[(entry.data + j) % MIDI_QUEUE_DATA_SIZE] = data[message.data + j];

        midi_queue_data_used += message.size;
        midi_queue_count++;
    }

    SDL_SignalCondition(midi_queue_condition);
    SDL_UnlockMutex(midi_queue_mutex);
    midi->Clear();
}

static void midi_queue_clear(void)
{
    midi_queue_head = 0;
    midi_queue_count = 0;
    midi_queue_data_head = 0;
    midi_queue_data_used = 0;
}

static int SDLCALL midi_queue_run(void* data)
{
    UNUSED(data);
    u8 buffer[MIDI_OUT_SYSEX_SIZE];

    SDL_LockMutex(midi_queue_mutex);

    while (!midi_queue_quit)
    {
        if (midi_queue_preparing)
        {
            SDL_UnlockMutex(midi_queue_mutex);
            std::vector<MidiPortInfo> ports;
            MidiPort::ListPorts(ports);
            SDL_SetAtomicInt(&midi_queue_ready, 1);
            SDL_LockMutex(midi_queue_mutex);
            midi_queue_preparing = false;
            continue;
        }

        if (midi_queue_count == 0)
        {
            SDL_WaitConditionTimeout(midi_queue_condition, midi_queue_mutex, 100);
            continue;
        }

        Midi_Queue_Entry entry = midi_queue_entries[midi_queue_head];
        u64 now = SDL_GetTicksNS();

        if (entry.time > now)
        {
            Sint32 wait = (Sint32)MIN((entry.time - now + 999999) / 1000000, (u64)100);
            SDL_WaitConditionTimeout(midi_queue_condition, midi_queue_mutex, wait);
            continue;
        }

        u32 size = MIN(entry.size, (u32)sizeof(buffer));

        for (u32 i = 0; i < size; i++)
            buffer[i] = midi_queue_data[(entry.data + i) % MIDI_QUEUE_DATA_SIZE];

        midi_queue_head = (midi_queue_head + 1) % MIDI_QUEUE_ENTRIES;
        midi_queue_count--;
        midi_queue_data_head = (midi_queue_data_head + entry.size) % MIDI_QUEUE_DATA_SIZE;
        midi_queue_data_used -= entry.size;
        SDL_UnlockMutex(midi_queue_mutex);

        SDL_LockMutex(midi_queue_port_mutex);
        bool sent = !midi_queue_port.IsOpen() || midi_queue_port.Send(buffer, (int)size);
        SDL_UnlockMutex(midi_queue_port_mutex);

        if (!sent)
            SDL_SetAtomicInt(&midi_queue_lost, 1);

        SDL_LockMutex(midi_queue_mutex);
    }

    SDL_UnlockMutex(midi_queue_mutex);
    return 0;
}
