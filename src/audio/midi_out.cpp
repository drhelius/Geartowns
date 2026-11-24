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

#include "midi_out.h"
#include "audio_common.h"
#include "../common/state_serializer.h"
#include "../common/trace_logger.h"

static const u8 k_midi_out_gm_on[6] = { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 };
static const u8 k_midi_out_xg_on[4] = { 0x00, 0x00, 0x7E, 0x00 };
static const char* const k_midi_out_gm_displays[2] = { "SCSCSCFY!           ", "Bye.                " };

MidiOut::MidiOut()
{
    m_trace_logger = TraceLogger::GetDisabled();
    m_device_known = false;
    m_capture = MIDI_OUT_CAPTURE_OFF;
    m_position = 0;
    m_played = false;
    m_reset = false;
    memset(&m_stats, 0, sizeof(m_stats));
    m_message_count = 0;
    m_data_size = 0;
    ClearSynth(m_device);
    Reset();
}

MidiOut::~MidiOut()
{
}

void MidiOut::Reset()
{
    m_state.status = 0;
    memset(m_state.message, 0, sizeof(m_state.message));
    m_state.count = 0;
    m_state.expected = 0;
    m_state.sysex = false;
    m_state.sysex_size = 0;
    memset(m_state.sysex_buffer, 0, sizeof(m_state.sysex_buffer));
    m_state.detected = -1;
    ClearSynth(m_state.machine);
    m_reset = true;
}

// Real Time goes out alone wherever it falls and leaves running status alone (SPEC p35). F9h and FDh are undefined, F4h
// and F5h too, and those two clear running status as System Common and SysEx do (SPEC p65). Any status byte but Real
// Time ends a SysEx (SPEC p80)
void MidiOut::Write(u8 value)
{
    if (value >= 0xF8)
    {
        if ((value != 0xF9) && (value != 0xFD))
            Emit(&value, 1);

        return;
    }

    if (m_state.sysex)
    {
        if (value < 0x80)
        {
            if (m_state.sysex_size < MIDI_OUT_SYSEX_SIZE)
                m_state.sysex_buffer[m_state.sysex_size] = value;

            if (m_state.sysex_size <= MIDI_OUT_SYSEX_SIZE)
                m_state.sysex_size++;

            return;
        }

        EndSysEx();

        if (value == 0xF7)
            return;
    }

    if (value >= 0xF0)
    {
        m_state.status = 0;
        m_state.count = 0;

        switch (value)
        {
            case 0xF0:
                m_state.sysex = true;
                m_state.sysex_buffer[0] = value;
                m_state.sysex_size = 1;
                break;
            case 0xF1:
            case 0xF2:
            case 0xF3:
                m_state.message[0] = value;
                m_state.count = 1;
                m_state.expected = value == 0xF2 ? 3 : 2;
                break;
            case 0xF6:
                Emit(&value, 1);
                break;
            default:
                break;
        }

        return;
    }

    if (value >= 0x80)
    {
        m_state.status = value;
        m_state.message[0] = value;
        m_state.count = 1;
        m_state.expected = k_midi_voice_lengths[(value >> 4) & 0x07];
        return;
    }

    if (m_state.count == 0)
    {
        if (m_state.status == 0)
            return;

        m_state.message[0] = m_state.status;
        m_state.count = 1;
        m_state.expected = k_midi_voice_lengths[(m_state.status >> 4) & 0x07];
    }

    m_state.message[m_state.count++] = value;

    if (m_state.count >= m_state.expected)
    {
        Emit(m_state.message, m_state.count);
        m_state.count = 0;
    }
}

// The pedal goes first, as All Notes Off waits for its release (SPEC p68), and the Note Offs cover a receiver that
// ignores 123 (SPEC p25)
MidiOut::MidiOut_Capture_Change MidiOut::UpdateCapture(bool enabled, bool quiet, bool builtin)
{
    MidiOut_Capture silent = builtin ? MIDI_OUT_CAPTURE_OFF : MIDI_OUT_CAPTURE_SILENT;
    MidiOut_Capture capture = enabled ? (quiet ? silent : MIDI_OUT_CAPTURE_ON) : MIDI_OUT_CAPTURE_OFF;
    MidiOut_Capture current = m_capture;
    m_capture = capture;

    if ((current == MIDI_OUT_CAPTURE_ON) && (capture == silent))
        return MIDI_OUT_CAPTURE_SILENCE;

    if ((current == silent) && (capture == MIDI_OUT_CAPTURE_ON))
        return MIDI_OUT_CAPTURE_SYNC;

    return MIDI_OUT_CAPTURE_KEEP;
}

void MidiOut::Silence()
{
    for (int i = 0; i < MIDI_OUT_CHANNELS; i++)
    {
        if (HasNotes(m_device, i) || (m_device.controllers[i][64] >= 64))
            SilenceChannel(i);
    }
}

void MidiOut::Sync()
{
    const MidiOut_Synth& machine = m_state.machine;

    Silence();

    bool log = !m_device_known || (m_device.log_size != machine.log_size) ||
        (memcmp(m_device.log, machine.log, machine.log_size) != 0);

    if (log)
    {
        if (!m_device_known || (m_device.log_size != 0))
            SendResets();

        SendLog(machine);
    }

    for (int i = 0; i < MIDI_OUT_CHANNELS; i++)
        SendChannel(machine, i, log);

    memcpy(&m_device, &machine, sizeof(m_device));
    memset(m_device.notes, 0, sizeof(m_device.notes));
    m_device_known = true;
}

void MidiOut::ForgetSynth()
{
    ClearSynth(m_device);
    m_device_known = false;
}

void MidiOut::ResetSynth()
{
    ClearSynth(m_device);
    m_device_known = true;
}

bool MidiOut::IsPlaying() const
{
    for (int i = 0; i < MIDI_OUT_CHANNELS; i++)
    {
        if (HasNotes(m_state.machine, i) || (m_state.machine.controllers[i][64] >= 64))
            return true;
    }

    return false;
}

bool MidiOut::HasPlayed()
{
    bool played = m_played;
    m_played = false;
    return played;
}

bool MidiOut::TakeReset()
{
    bool reset = m_reset;
    m_reset = false;
    return reset;
}

void MidiOut::EndSysEx()
{
    m_state.sysex = false;

    if (m_state.sysex_size >= MIDI_OUT_SYSEX_SIZE)
        m_stats.dropped++;
    else
    {
        m_state.sysex_buffer[m_state.sysex_size++] = 0xF7;
        Emit(m_state.sysex_buffer, (int)m_state.sysex_size);
    }

    m_state.sysex_size = 0;
}

void MidiOut::Detect(const u8* data, int size)
{
    if ((size == 6) && (data[1] == 0x7E) && (data[3] == 0x09) && (data[4] == 0x01))
    {
        m_state.detected = GT_MIDI_SYNTH_GM;
        return;
    }

    if ((size == 9) && (data[1] == 0x43) && ((data[2] & 0xF0) == 0x10) && (data[3] == 0x4C) &&
        (memcmp(data + 4, k_midi_out_xg_on, sizeof(k_midi_out_xg_on)) == 0))
    {
        m_state.detected = GT_MIDI_SYNTH_GM;
        return;
    }

    if ((size < 10) || (data[1] != 0x41))
        return;

    if (data[3] == 0x42)
    {
        if ((data[4] == 0x12) && (data[5] == 0x40) && (data[6] == 0x00) && (data[7] == 0x7F))
            m_state.detected = GT_MIDI_SYNTH_GM;

        return;
    }

    if ((data[3] != 0x16) && (data[3] != 0x14))
        return;

    if ((data[4] == 0x11) || ((data[4] == 0x12) && IsMT32Only(data, size)))
        m_state.detected = GT_MIDI_SYNTH_MT32;
}

void MidiOut::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = TraceLogger::Resolve(trace_logger);
}

void MidiOut::TraceMessage(const u8* data, int size)
{
    GT_Trace_Entry* entry = m_trace_logger->Begin(TRACE_MIDI, TRACE_MIDI_MESSAGE);
    entry->midi.length = (u16)MIN(size, 0xFFFF);
    entry->midi.count = (u8)MIN(size, 24);
    entry->midi.value = data[0];
    memcpy(entry->midi.bytes, data, entry->midi.count);
}

void MidiOut::Emit(const u8* data, int size)
{
    if (unlikely(m_trace_logger->IsEventEnabled(TRACE_MIDI, TRACE_MIDI_MESSAGE)))
        TraceMessage(data, size);

    if (data[0] == 0xF0)
        Detect(data, size);

    Apply(m_state.machine, data, size);

    bool note_on = ((data[0] & 0xF0) == 0x90) && (size == 3) && (data[2] != 0);

    if (note_on)
        m_played = true;

    if ((m_capture == MIDI_OUT_CAPTURE_OFF) || (note_on && (m_capture == MIDI_OUT_CAPTURE_SILENT)))
        return;

    u16 offset = m_capture == MIDI_OUT_CAPTURE_ON ? (u16)CLAMP(m_position, 0, 0xFFFE) : k_midi_out_immediate;

    if (Send(data, size, offset))
        Apply(m_device, data, size);
}

bool MidiOut::Send(const u8* data, int size, u16 offset)
{
    if ((m_message_count >= MIDI_OUT_MESSAGES) || ((m_data_size + (u32)size) > MIDI_OUT_DATA_SIZE))
    {
        m_stats.dropped++;
        return false;
    }

    MidiOut_Message& message = m_messages[m_message_count++];
    message.data = m_data_size;
    message.size = (u16)size;
    message.offset = offset;
    memcpy(m_data + m_data_size, data, size);
    m_data_size += size;
    m_stats.messages++;
    m_stats.bytes += size;
    return true;
}

void MidiOut::SendShort(u8 status, u8 data1, u8 data2, int size)
{
    u8 message[3] = { status, data1, data2 };
    Send(message, size, k_midi_out_immediate);
}

// Data entry goes after the parameter numbers it applies to, and increments are never sent again. A new bank only
// takes effect with a program change
void MidiOut::SendChannel(const MidiOut_Synth& machine, int channel, bool all)
{
    u16 bit = (u16)(1 << channel);
    const u32* set = machine.controllers_set[channel];
    const u32* device_set = m_device.controllers_set[channel];
    const u8* values = machine.controllers[channel];
    bool extra = ((m_device.bend_set & ~machine.bend_set & bit) != 0) ||
        ((m_device.pressure_set & ~machine.pressure_set & bit) != 0);

    for (int i = 0; i < 4; i++)
        extra = extra || ((device_set[i] & ~set[i]) != 0);

    bool reset = !m_device_known || extra;
    bool full = all || reset;
    bool bank = false;

    if (reset)
        SendShort((u8)(0xB0 | channel), 121, 0, 3);

    for (int pass = 0; pass < 2; pass++)
    {
        for (int i = 0; i < 120; i++)
        {
            bool entry = (i == 6) || (i == 38);

            if ((entry != (pass == 1)) || (i == 96) || (i == 97))
                continue;

            u32 mask = 1u << (i & 31);

            if ((set[i >> 5] & mask) == 0)
                continue;

            if (!full && ((device_set[i >> 5] & mask) != 0) && (m_device.controllers[channel][i] == values[i]))
                continue;

            SendShort((u8)(0xB0 | channel), (u8)i, values[i], 3);
            bank = bank || (i == 0) || (i == 32);
        }
    }

    if (((machine.program_set & bit) != 0) && (full || bank || ((m_device.program_set & bit) == 0) ||
        (m_device.program[channel] != machine.program[channel])))
        SendShort((u8)(0xC0 | channel), machine.program[channel], 0, 2);

    if (((machine.pressure_set & bit) != 0) && (full || ((m_device.pressure_set & bit) == 0) ||
        (m_device.pressure[channel] != machine.pressure[channel])))
        SendShort((u8)(0xD0 | channel), machine.pressure[channel], 0, 2);

    if (((machine.bend_set & bit) != 0) && (full || ((m_device.bend_set & bit) == 0) ||
        (m_device.bend[channel] != machine.bend[channel])))
        SendShort((u8)(0xE0 | channel), (u8)(machine.bend[channel] & 0x7F), (u8)(machine.bend[channel] >> 7), 3);
}

// A synth may still hold another game's SysEx, as a module does until it's switched off. Each kind ignores the other's
// reset: GM System On (GM p6) and the MT-32's all parameter reset, any data at 7F xx xx (MT-32 p42, p43)
void MidiOut::SendResets()
{
    Send(k_midi_out_gm_on, sizeof(k_midi_out_gm_on), k_midi_out_immediate);
    Send(k_midi_mt32_reset, sizeof(k_midi_mt32_reset), k_midi_out_immediate);
}

void MidiOut::SendLog(const MidiOut_Synth& synth)
{
    u32 start = 0;

    for (u32 i = 0; i < synth.log_size; i++)
    {
        if (synth.log[i] != 0xF7)
            continue;

        Send(synth.log + start, (int)(i + 1 - start), k_midi_out_immediate);
        start = i + 1;
    }
}

void MidiOut::SilenceChannel(int channel)
{
    u8 status = (u8)(0xB0 | channel);

    SendShort(status, 64, 0, 3);

    for (int note = 0; note < 128; note++)
    {
        if ((m_device.notes[channel][note >> 5] & (1u << (note & 31))) != 0)
            SendShort((u8)(0x80 | channel), (u8)note, 0, 3);
    }

    SendShort(status, 123, 0, 3);
    memset(m_device.notes[channel], 0, sizeof(m_device.notes[channel]));
    m_device.controllers[channel][64] = 0;
}

// 121 resets controllers, bend and pressure, not the bank (SPEC p30). 120 and 123 to 127 end the notes (SPEC p25), and
// System Reset only running status and the notes here, as the MT-32 doesn't recognize it (MT-32 p46)
void MidiOut::Apply(MidiOut_Synth& synth, const u8* data, int size)
{
    u8 status = data[0];

    if (status == 0xF0)
    {
        if (IsReset(data, size))
            ClearSynth(synth);

        if (synth.log_full || ((synth.log_size + (u32)size) > MIDI_OUT_LOG_SIZE))
        {
            synth.log_full = true;
            return;
        }

        memcpy(synth.log + synth.log_size, data, size);
        synth.log_size += size;
        return;
    }

    if (status == 0xFF)
    {
        memset(synth.notes, 0, sizeof(synth.notes));
        return;
    }

    if (status >= 0xF0)
        return;

    int channel = status & 0x0F;
    u16 bit = (u16)(1 << channel);
    u8 data1 = size > 1 ? data[1] : 0;
    u8 data2 = size > 2 ? data[2] : 0;
    u32* notes = synth.notes[channel];
    u32 note_mask = 1u << (data1 & 31);

    switch (status & 0xF0)
    {
        case 0x80:
            notes[data1 >> 5] &= ~note_mask;
            break;
        case 0x90:
            if (data2 != 0)
                notes[data1 >> 5] |= note_mask;
            else
                notes[data1 >> 5] &= ~note_mask;
            break;
        case 0xB0:
            if (data1 < 120)
            {
                synth.controllers[channel][data1] = data2;
                synth.controllers_set[channel][data1 >> 5] |= note_mask;
            }
            else if (data1 == 121)
            {
                u32* set = synth.controllers_set[channel];
                set[0] &= 0x00000001;
                set[1] &= 0x00000001;
                set[2] = 0;
                set[3] = 0;
                synth.bend[channel] = 0x2000;
                synth.pressure[channel] = 0;
                synth.bend_set &= (u16)~bit;
                synth.pressure_set &= (u16)~bit;
            }
            else if (data1 != 122)
                memset(notes, 0, sizeof(synth.notes[channel]));
            break;
        case 0xC0:
            synth.program[channel] = data1;
            synth.program_set |= bit;
            break;
        case 0xD0:
            synth.pressure[channel] = data1;
            synth.pressure_set |= bit;
            break;
        case 0xE0:
            synth.bend[channel] = (u16)(data1 | (data2 << 7));
            synth.bend_set |= bit;
            break;
        default:
            break;
    }
}

void MidiOut::ClearSynth(MidiOut_Synth& synth)
{
    memset(&synth, 0, sizeof(synth));

    for (int i = 0; i < MIDI_OUT_CHANNELS; i++)
        synth.bend[i] = 0x2000;
}

// GM System On (GM p6), GS Reset at 40 00 7F (SC-55 p78) and the MT-32's all parameter reset at 7F xx xx (MT-32 p42),
// whatever their device ID
bool MidiOut::IsReset(const u8* data, int size)
{
    if ((size == 6) && (data[1] == 0x7E) && (data[3] == 0x09) && (data[4] == 0x01))
        return true;

    if ((size < 10) || (data[1] != 0x41) || (data[4] != 0x12))
        return false;

    if (data[3] == 0x42)
        return (data[5] == 0x40) && (data[6] == 0x00) && (data[7] == 0x7F);

    return (data[3] == 0x16) && (data[5] == 0x7F);
}

bool MidiOut::IsMT32Only(const u8* data, int size)
{
    switch (data[5])
    {
        case 0x10:
        case 0x7F:
            return false;
        case 0x05:
            return (size != 11) && (size != 12);
        case 0x20:
            for (int i = 0; i < 2; i++)
            {
                if ((size == 30) && (memcmp(data + 8, k_midi_out_gm_displays[i], 20) == 0))
                    return false;
            }
            return true;
        default:
            return true;
    }
}

bool MidiOut::HasNotes(const MidiOut_Synth& synth, int channel)
{
    const u32* notes = synth.notes[channel];
    return (notes[0] | notes[1] | notes[2] | notes[3]) != 0;
}

void MidiOut::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void MidiOut::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void MidiOut::Serialize(StateSerializer& serializer)
{
    MidiOut_Synth& machine = m_state.machine;

    G_SERIALIZE(serializer, m_state.status);
    G_SERIALIZE_ARRAY(serializer, m_state.message, 3);
    G_SERIALIZE(serializer, m_state.count);
    G_SERIALIZE(serializer, m_state.expected);
    G_SERIALIZE(serializer, m_state.sysex);
    G_SERIALIZE(serializer, m_state.sysex_size);
    G_SERIALIZE_ARRAY(serializer, m_state.sysex_buffer, MIDI_OUT_SYSEX_SIZE);
    G_SERIALIZE(serializer, m_state.detected);
    G_SERIALIZE_ARRAY(serializer, machine.program, MIDI_OUT_CHANNELS);
    G_SERIALIZE_ARRAY(serializer, machine.pressure, MIDI_OUT_CHANNELS);
    G_SERIALIZE_ARRAY(serializer, machine.bend, MIDI_OUT_CHANNELS);
    G_SERIALIZE(serializer, machine.program_set);
    G_SERIALIZE(serializer, machine.pressure_set);
    G_SERIALIZE(serializer, machine.bend_set);
    G_SERIALIZE_ARRAY(serializer, &machine.controllers[0][0], MIDI_OUT_CHANNELS * 128);
    G_SERIALIZE_ARRAY(serializer, &machine.controllers_set[0][0], MIDI_OUT_CHANNELS * 4);
    G_SERIALIZE_ARRAY(serializer, &machine.notes[0][0], MIDI_OUT_CHANNELS * 4);
    G_SERIALIZE(serializer, machine.log_size);
    G_SERIALIZE(serializer, machine.log_full);
    G_SERIALIZE_ARRAY(serializer, machine.log, MIDI_OUT_LOG_SIZE);
}

void MidiOut::SanitizeState()
{
    MidiOut_Synth& machine = m_state.machine;

    if ((m_state.count > 2) || (m_state.expected > 3))
        m_state.count = 0;

    m_state.sysex_size = MIN(m_state.sysex_size, (u32)(MIDI_OUT_SYSEX_SIZE + 1));

    if ((m_state.detected < -1) || (m_state.detected >= GT_MIDI_SYNTH_COUNT))
        m_state.detected = -1;
    machine.log_size = MIN(machine.log_size, (u32)MIDI_OUT_LOG_SIZE);

    for (int i = 0; i < MIDI_OUT_CHANNELS; i++)
    {
        machine.program[i] &= 0x7F;
        machine.pressure[i] &= 0x7F;
        machine.bend[i] &= 0x3FFF;

        for (int j = 0; j < 128; j++)
            machine.controllers[i][j] &= 0x7F;
    }
}
