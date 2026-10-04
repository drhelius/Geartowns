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

#ifndef TOWNS_KEYBOARD_INLINE_H
#define TOWNS_KEYBOARD_INLINE_H

#include "towns_keyboard.h"

INLINE void TownsKeyboard::HandleEvent(u64 clocks)
{
    Synchronize(clocks);
    UpdateNextEvent();
}

INLINE bool TownsKeyboard::IsKeyPressed(GT_Keys key) const
{
    return IsValidKey(key) && m_state.keys[key];
}

// Code 7Fh only appears in reset responses
INLINE bool TownsKeyboard::IsValidKey(GT_Keys key) const
{
    return key > GT_KEY_NONE && key < 0x7F;
}

INLINE TownsKeyboard::TownsKeyboard_State* TownsKeyboard::GetState()
{
    return &m_state;
}

#endif /* TOWNS_KEYBOARD_INLINE_H */
