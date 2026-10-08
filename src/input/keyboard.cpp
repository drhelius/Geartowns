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

#include "keyboard.h"
#include "../system/pic.h"
#include "../system/scheduler.h"
#include "../common/trace_logger.h"
#include "../common/state_serializer.h"

// Answer to a keyboard reset seen on a Towns II MX with a JIS keyboard
static const u8 k_keyboard_reset_response[4] = { 0xB0, 0x7F, 0xE8, 0x25 };

Keyboard::Keyboard()
{
    InitPointer(m_pic);
    InitPointer(m_scheduler);
    InitPointer(m_trace_logger);
    memset(&m_state, 0, sizeof(m_state));
}

Keyboard::~Keyboard()
{
}

void Keyboard::Init(PIC* pic, Scheduler* scheduler)
{
    m_pic = pic;
    m_scheduler = scheduler;
    Reset();
}

void Keyboard::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

void Keyboard::Reset()
{
    memset(&m_state, 0, sizeof(m_state));
    m_state.repeat_delay = k_keyboard_repeat_delay;
    m_state.repeat_interval = k_keyboard_repeat_interval;
    UpdateIRQ();
    UpdateNextEvent();
}

u8 Keyboard::Read(u16 port, u64 clocks)
{
    Synchronize(clocks);

    switch (port)
    {
        case 0x0600:
        {
            u8 value = 0x00;

            if (m_state.fifo_count != 0)
            {
                value = m_state.fifo[m_state.fifo_read];
                m_state.fifo_read = (m_state.fifo_read + 1) & (KEYBOARD_FIFO_SIZE - 1);
                m_state.fifo_count--;
            }

            // Reading acknowledges the request, the next one comes a byte time later while data remains
            m_state.kbint = false;
            m_state.rearm_pending = m_state.fifo_count != 0 && m_state.irq_enabled;
            m_state.rearm_clocks = clocks + k_keyboard_rearm_clocks;
            UpdateIRQ();
            UpdateNextEvent();
            TraceEvent(TRACE_KEYBOARD_READ, value, 0, 0);
            return value;
        }
        case 0x0602:
            return m_state.fifo_count != 0 ? 0x01 : 0x00;
        case 0x0604:
            return m_state.kbint ? 0x01 : 0x00;
        default:
            return 0xFF;
    }
}

// The next queued byte stays queued
u8 Keyboard::Peek(u16 port) const
{
    switch (port)
    {
        case 0x0600:
            return m_state.fifo_count != 0 ? m_state.fifo[m_state.fifo_read] : 0x00;
        case 0x0602:
            return m_state.fifo_count != 0 ? 0x01 : 0x00;
        case 0x0604:
            return m_state.kbint ? 0x01 : 0x00;
        default:
            return 0xFF;
    }
}

void Keyboard::Write(u16 port, u8 value, u64 clocks)
{
    Synchronize(clocks);

    switch (port)
    {
        case 0x0600:
            TraceEvent(TRACE_KEYBOARD_COMMAND, value, 0, 0);

            // FM-OASYS and the keyboard BIOS reset the keyboard here, keeping the IRQ enable
            if (value == 0xA1 || value == 0xA2)
                SendResetResponse(4);
            break;
        case 0x0602:
            TraceEvent(TRACE_KEYBOARD_COMMAND, value, 0x02, 0);
            WriteCommand(value);
            break;
        case 0x0604:
            TraceEvent(TRACE_KEYBOARD_IRQ_ENABLE, value, 0, 0);
            m_state.irq_enabled = (value & 0x01) != 0;

            if (m_state.irq_enabled && m_state.fifo_count != 0 && !m_state.rearm_pending)
                m_state.kbint = true;

            UpdateIRQ();
            break;
    }
}

void Keyboard::Synchronize(u64 clocks)
{
    bool changed = false;

    if (m_state.rearm_pending && clocks >= m_state.rearm_clocks)
    {
        m_state.rearm_pending = false;
        changed = true;

        if (m_state.fifo_count != 0 && m_state.irq_enabled)
        {
            m_state.kbint = true;
            UpdateIRQ();
        }
    }

    // Typematic repeat sends the held key again with bits 7-4 set
    if (m_state.repeat_key != GT_KEY_NONE && clocks >= m_state.repeat_clocks)
    {
        PushEvent(m_state.repeat_key, k_keyboard_typematic);
        m_state.repeat_clocks += ((u64)m_state.repeat_interval * GT_CPU_CLOCK_RATE) / 1000;
        changed = true;
    }

    if (changed)
        UpdateNextEvent();
}

void Keyboard::KeyPressed(GT_Keys key)
{
    if (!IsValidKey(key) || m_state.keys[key])
        return;

    Synchronize(m_scheduler->GetClocks());
    m_state.keys[key] = true;
    PushEvent((u8)key, k_keyboard_make);

    if (IsRepeatKey((u8)key))
    {
        m_state.repeat_key = (u8)key;
        m_state.repeat_clocks = m_scheduler->GetClocks() + ((u64)m_state.repeat_delay * GT_CPU_CLOCK_RATE) / 1000;
    }

    UpdateNextEvent();
}

void Keyboard::KeyReleased(GT_Keys key)
{
    if (!IsValidKey(key) || !m_state.keys[key])
        return;

    Synchronize(m_scheduler->GetClocks());
    m_state.keys[key] = false;
    PushEvent((u8)key, k_keyboard_break);

    if (m_state.repeat_key == key)
        m_state.repeat_key = GT_KEY_NONE;

    UpdateNextEvent();
}

// Modifiers go last so the other breaks still carry them
void Keyboard::ReleaseAllKeys()
{
    for (int i = GT_KEY_NONE + 1; i < GT_KEY_COUNT; i++)
    {
        if (i != GT_KEY_CTRL && i != GT_KEY_SHIFT)
            KeyReleased((GT_Keys)i);
    }

    KeyReleased(GT_KEY_CTRL);
    KeyReleased(GT_KEY_SHIFT);
}

void Keyboard::WriteCommand(u8 value)
{
    switch (value)
    {
        case 0xA0:
            ResetController();
            SendResetResponse(4);
            break;
        case 0xA1:
            // Only a reset that follows A0h repeats the whole answer
            ResetController();
            SendResetResponse(m_state.last_command == 0xA0 ? 4 : 2);
            break;
        case 0xA9:
            m_state.repeat_delay = 400;
            break;
        case 0xAA:
            m_state.repeat_delay = 500;
            break;
        case 0xAB:
            m_state.repeat_delay = 300;
            break;
        case 0xAC:
            m_state.repeat_interval = 50;
            break;
        case 0xAD:
            m_state.repeat_interval = 30;
            break;
        case 0xAE:
            m_state.repeat_interval = 20;
            break;
        default:
            break;
    }

    m_state.last_command = value;
}

void Keyboard::ResetController()
{
    m_state.irq_enabled = false;
    m_state.repeat_key = GT_KEY_NONE;
    m_state.repeat_delay = k_keyboard_repeat_delay;
    m_state.repeat_interval = k_keyboard_repeat_interval;
}

// The answer replaces anything still queued
void Keyboard::SendResetResponse(int count)
{
    memcpy(m_state.fifo, k_keyboard_reset_response, count);
    m_state.fifo_read = 0;
    m_state.fifo_count = (u8)count;
    m_state.rearm_pending = false;
    m_state.kbint = true;
    UpdateIRQ();
    UpdateNextEvent();
}

// A JIS keyboard message: make, break or typematic flags with CTRL and SHIFT, then the key code
void Keyboard::PushEvent(u8 key, u8 flags)
{
    if (m_state.fifo_count + 2 > KEYBOARD_FIFO_SIZE)
        return;

    if (m_state.keys[GT_KEY_CTRL])
        flags |= 0x08;

    if (m_state.keys[GT_KEY_SHIFT])
        flags |= 0x04;

    int write = m_state.fifo_read + m_state.fifo_count;
    m_state.fifo[write & (KEYBOARD_FIFO_SIZE - 1)] = flags;
    m_state.fifo[(write + 1) & (KEYBOARD_FIFO_SIZE - 1)] = key;
    m_state.fifo_count += 2;
    m_state.kbint = true;
    TraceEvent(TRACE_KEYBOARD_KEY, 0, flags, key);
    UpdateIRQ();
}

void Keyboard::TraceEvent(u8 event, u8 value, u8 flags, u8 key)
{
    if (!IsValidPointer(m_trace_logger) || !m_trace_logger->IsEventEnabled(TRACE_KEYBOARD, event))
        return;

    GT_Trace_Entry entry = {};
    entry.type = TRACE_KEYBOARD;
    entry.event = event;
    entry.keyboard.value = value;
    entry.keyboard.flags = flags;
    entry.keyboard.key = key;
    entry.keyboard.pending = m_state.fifo_count;
    m_trace_logger->TraceLog(entry);
}

bool Keyboard::IsRepeatKey(u8 key) const
{
    return key != GT_KEY_CTRL && key != GT_KEY_SHIFT && key != GT_KEY_ALT;
}

void Keyboard::UpdateIRQ()
{
    m_pic->SetIRQLine(k_keyboard_irq, m_state.kbint && m_state.irq_enabled);
}

void Keyboard::UpdateNextEvent()
{
    u64 next = GT_NO_EVENT;

    if (m_state.rearm_pending)
        next = m_state.rearm_clocks;

    if (m_state.repeat_key != GT_KEY_NONE)
        next = MIN(next, m_state.repeat_clocks);

    m_scheduler->Schedule(SCHEDULER_EVENT_KEYBOARD, next);
}

void Keyboard::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void Keyboard::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void Keyboard::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE_ARRAY(serializer, m_state.fifo, KEYBOARD_FIFO_SIZE);
    G_SERIALIZE(serializer, m_state.fifo_read);
    G_SERIALIZE(serializer, m_state.fifo_count);
    G_SERIALIZE(serializer, m_state.irq_enabled);
    G_SERIALIZE(serializer, m_state.kbint);
    G_SERIALIZE(serializer, m_state.last_command);
    G_SERIALIZE(serializer, m_state.rearm_pending);
    G_SERIALIZE(serializer, m_state.rearm_clocks);
    G_SERIALIZE_ARRAY(serializer, m_state.keys, GT_KEY_COUNT);
    G_SERIALIZE(serializer, m_state.repeat_key);
    G_SERIALIZE(serializer, m_state.repeat_clocks);
    G_SERIALIZE(serializer, m_state.repeat_delay);
    G_SERIALIZE(serializer, m_state.repeat_interval);
}

void Keyboard::SanitizeState()
{
    m_state.fifo_read &= KEYBOARD_FIFO_SIZE - 1;
    m_state.fifo_count = (u8)MIN(m_state.fifo_count, KEYBOARD_FIFO_SIZE);

    if (!IsValidKey((GT_Keys)m_state.repeat_key))
        m_state.repeat_key = GT_KEY_NONE;

    // A zero interval would repeat the key at the same clock forever
    if (m_state.repeat_interval == 0)
        m_state.repeat_interval = k_keyboard_repeat_interval;

    UpdateIRQ();
    UpdateNextEvent();
}
