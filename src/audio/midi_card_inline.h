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
#ifndef MIDI_CARD_INLINE_H
#define MIDI_CARD_INLINE_H

#include "midi_card.h"

INLINE bool MidiCard::IsFitted() const
{
    return m_fitted;
}

INLINE I8251* MidiCard::GetUART(int output)
{
    return &m_uart[output];
}

INLINE I8253* MidiCard::GetTimer()
{
    return &m_timer;
}

INLINE MidiCard::MidiCard_State* MidiCard::GetState()
{
    return &m_state;
}

#endif /* MIDI_CARD_INLINE_H */
