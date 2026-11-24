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

#ifndef MIDI_OUT_H
#define MIDI_OUT_H

#include <iostream>
#include "../common/common.h"

#define MIDI_OUT_CHANNELS 16
#define MIDI_OUT_SYSEX_SIZE 0x1000
#define MIDI_OUT_LOG_SIZE 0x10000
#define MIDI_OUT_MESSAGES 0x4000
#define MIDI_OUT_DATA_SIZE 0x20000

class StateSerializer;

class TraceLogger;

class MidiOut
{
public:
    enum MidiOut_Capture
    {
        MIDI_OUT_CAPTURE_OFF = 0,
        MIDI_OUT_CAPTURE_ON,
        MIDI_OUT_CAPTURE_SILENT
    };

    enum MidiOut_Capture_Change
    {
        MIDI_OUT_CAPTURE_KEEP = 0,
        MIDI_OUT_CAPTURE_SILENCE,
        MIDI_OUT_CAPTURE_SYNC
    };

    struct MidiOut_Message
    {
        u32 data;
        u16 size;
        u16 offset;
    };

    struct MidiOut_Synth
    {
        u8 program[MIDI_OUT_CHANNELS];
        u8 pressure[MIDI_OUT_CHANNELS];
        u16 bend[MIDI_OUT_CHANNELS];
        u16 program_set;
        u16 pressure_set;
        u16 bend_set;
        u8 controllers[MIDI_OUT_CHANNELS][128];
        u32 controllers_set[MIDI_OUT_CHANNELS][4];
        u32 notes[MIDI_OUT_CHANNELS][4];
        u32 log_size;
        bool log_full;
        u8 log[MIDI_OUT_LOG_SIZE];
    };

    struct MidiOut_State
    {
        u8 status;
        u8 message[3];
        u8 count;
        u8 expected;
        bool sysex;
        u32 sysex_size;
        u8 sysex_buffer[MIDI_OUT_SYSEX_SIZE];
        int detected;
        MidiOut_Synth machine;
    };

    struct MidiOut_Stats
    {
        u32 messages;
        u32 bytes;
        u32 dropped;
    };

public:
    MidiOut();
    ~MidiOut();
    void Reset();
    void SetPosition(int position);
    void Write(u8 value);
    void SetTraceLogger(TraceLogger* trace_logger);
    void SetCapture(MidiOut_Capture capture);
    MidiOut_Capture GetCapture() const;
    MidiOut_Capture_Change UpdateCapture(bool enabled, bool quiet, bool builtin);
    void Silence();
    void Sync();
    void ForgetSynth();
    void ResetSynth();
    int GetMessageCount() const;
    const MidiOut_Message* GetMessages() const;
    const u8* GetData() const;
    void Clear();
    bool HasPlayed();
    bool IsPlaying() const;
    bool TakeReset();
    int GetDetected() const;
    const MidiOut_Stats* GetStats() const;
    void AddDropped(u32 count);
    MidiOut_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void EndSysEx();
    void Detect(const u8* data, int size);
    void Emit(const u8* data, int size);
    NO_INLINE void TraceMessage(const u8* data, int size);
    bool Send(const u8* data, int size, u16 offset);
    void SendShort(u8 status, u8 data1, u8 data2, int size);
    void SendChannel(const MidiOut_Synth& synth, int channel, bool all);
    void SendResets();
    void SendLog(const MidiOut_Synth& synth);
    void SilenceChannel(int channel);
    void Serialize(StateSerializer& serializer);
    void SanitizeState();
    static void Apply(MidiOut_Synth& synth, const u8* data, int size);
    static void ClearSynth(MidiOut_Synth& synth);
    static void ClearChannel(MidiOut_Synth& synth, int channel);
    static bool IsReset(const u8* data, int size);
    static bool IsMT32Only(const u8* data, int size);
    static bool HasNotes(const MidiOut_Synth& synth, int channel);

private:
    MidiOut_State m_state;
    TraceLogger* m_trace_logger;
    MidiOut_Synth m_device;
    bool m_device_known;
    MidiOut_Capture m_capture;
    int m_position;
    bool m_played;
    bool m_reset;
    MidiOut_Stats m_stats;
    int m_message_count;
    u32 m_data_size;
    MidiOut_Message m_messages[MIDI_OUT_MESSAGES];
    u8 m_data[MIDI_OUT_DATA_SIZE];
};

static const u16 k_midi_out_immediate = 0xFFFF;

#include "midi_out_inline.h"

#endif /* MIDI_OUT_H */
