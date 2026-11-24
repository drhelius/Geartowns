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

#ifndef MIDI_OUT_INLINE_H
#define MIDI_OUT_INLINE_H

#include "midi_out.h"

INLINE void MidiOut::SetPosition(int position)
{
    m_position = position;
}

INLINE void MidiOut::SetCapture(MidiOut_Capture capture)
{
    m_capture = capture;
}

INLINE MidiOut::MidiOut_Capture MidiOut::GetCapture() const
{
    return m_capture;
}

INLINE int MidiOut::GetMessageCount() const
{
    return m_message_count;
}

INLINE const MidiOut::MidiOut_Message* MidiOut::GetMessages() const
{
    return m_messages;
}

INLINE const u8* MidiOut::GetData() const
{
    return m_data;
}

INLINE void MidiOut::Clear()
{
    m_message_count = 0;
    m_data_size = 0;
}

INLINE int MidiOut::GetDetected() const
{
    return m_state.detected;
}

INLINE const MidiOut::MidiOut_Stats* MidiOut::GetStats() const
{
    return &m_stats;
}

INLINE void MidiOut::AddDropped(u32 count)
{
    m_stats.dropped += count;
}

INLINE MidiOut::MidiOut_State* MidiOut::GetState()
{
    return &m_state;
}

#endif /* MIDI_OUT_INLINE_H */
