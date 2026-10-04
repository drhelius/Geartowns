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

#include "cdrom_mock.h"
#include "../system/towns_pic.h"
#include "../system/scheduler.h"
#include "../common/state_serializer.h"

CDROMMock::CDROMMock()
{
    InitPointer(m_pic);
    InitPointer(m_scheduler);
    memset(&m_state, 0, sizeof(m_state));
}

CDROMMock::~CDROMMock()
{
}

void CDROMMock::Init(TownsPIC* pic, Scheduler* scheduler)
{
    m_pic = pic;
    m_scheduler = scheduler;
    Reset();
}

void CDROMMock::Reset()
{
    ResetController();
    m_state.enable_sirq = false;
    m_state.enable_dei = false;
    UpdateIRQ();
    UpdateNextEvent();
}

u8 CDROMMock::Read(u16 port, u64 clocks)
{
    Synchronize(clocks);

    switch (port)
    {
        case 0x04C0:
        {
            u8 status = m_state.sirq ? 0x80 : 0x00;
            status |= m_state.queue_count != 0 ? 0x02 : 0x00;
            status |= m_state.busy ? 0x00 : 0x01;
            return status;
        }
        case 0x04C2:
        {
            if (m_state.queue_count == 0)
                return 0xFF;

            u8 value = m_state.queue[0];
            m_state.queue_count--;
            memmove(m_state.queue, m_state.queue + 1, m_state.queue_count);

            // Commands that asked for an IRQ raise it again for each packet left
            if (m_state.queue_count != 0 && (m_state.queue_count & 0x03) == 0 && (m_state.command & 0x40) != 0)
                SetSIRQ();

            return value;
        }
        default:
            return 0xFF;
    }
}

void CDROMMock::Write(u16 port, u8 value, u64 clocks)
{
    Synchronize(clocks);

    switch (port)
    {
        case 0x04C0:
            if ((value & 0x80) != 0)
                m_state.sirq = false;

            if ((value & 0x04) != 0)
            {
                ResetController();
                UpdateNextEvent();
            }

            m_state.enable_sirq = (value & 0x02) != 0;
            m_state.enable_dei = (value & 0x01) != 0;
            UpdateIRQ();
            break;
        case 0x04C2:
            m_state.command = value;
            m_state.command_received = true;
            CheckCommand(clocks);
            break;
        case 0x04C4:
            if (m_state.param_count == 8)
            {
                memmove(m_state.params, m_state.params + 1, 7);
                m_state.param_count = 7;
            }

            m_state.params[m_state.param_count++] = value;
            CheckCommand(clocks);
            break;
        case 0x04C6:
            // Without a disc there is never a sector to transfer
            break;
    }
}

void CDROMMock::ResetController()
{
    m_state.command = 0;
    m_state.command_received = false;
    memset(m_state.params, 0, sizeof(m_state.params));
    m_state.param_count = 0;
    m_state.busy = false;
    m_state.execute_clocks = 0;
    m_state.queue_count = 0;
    m_state.sirq = false;
}

// The controller starts once it has the command and its eight parameters, a TOC read needs none
void CDROMMock::CheckCommand(u64 clocks)
{
    if (!m_state.command_received || (m_state.param_count < 8 && (m_state.command & 0x9F) != 0x05))
        return;

    m_state.command_received = false;
    m_state.param_count = 0;
    m_state.busy = true;
    m_state.execute_clocks = clocks + k_cdrom_mock_command_delay;
    UpdateNextEvent();
}

void CDROMMock::ExecuteCommand()
{
    m_state.busy = false;
    UpdateNextEvent();

    switch (m_state.command & 0x9F)
    {
        case 0x01:
        case 0x02:
        case 0x03:
            // Data reads fail because the drive is not ready
            PushStatus(0x21, 0x09);
            break;
        case 0x9F:
            PushStatus(0x21, 0x00);
            break;
        default:
            // Any other command runs and reports the drive as not ready
            if ((m_state.command & 0x20) != 0)
                PushStatus(0x00, 0x09);
            break;
    }

    if ((m_state.command & 0x40) != 0)
        SetSIRQ();
}

void CDROMMock::PushStatus(u8 status0, u8 status1)
{
    if (m_state.queue_count + 4 > CDROM_MOCK_QUEUE_SIZE)
    {
        Debug("CDROM: status queue full, dropping %02X %02X", status0, status1);
        return;
    }

    u8* packet = &m_state.queue[m_state.queue_count];
    packet[0] = status0;
    packet[1] = status1;
    packet[2] = 0x00;
    packet[3] = 0x00;
    m_state.queue_count += 4;
}

void CDROMMock::SetSIRQ()
{
    if (m_state.queue_count == 0)
        return;

    m_state.sirq = true;
    UpdateIRQ();
}

void CDROMMock::UpdateIRQ()
{
    m_pic->SetIRQLine(k_cdrom_mock_irq, m_state.sirq && m_state.enable_sirq);
}

void CDROMMock::UpdateNextEvent()
{
    m_scheduler->Schedule(SCHEDULER_EVENT_CDROM, m_state.busy ? m_state.execute_clocks : GT_NO_EVENT);
}

void CDROMMock::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void CDROMMock::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);

    m_state.param_count = MIN(m_state.param_count, 8);
    m_state.queue_count = MIN(m_state.queue_count, CDROM_MOCK_QUEUE_SIZE);
    UpdateIRQ();
    UpdateNextEvent();
}

void CDROMMock::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_state.command);
    G_SERIALIZE(serializer, m_state.command_received);
    G_SERIALIZE_ARRAY(serializer, m_state.params, 8);
    G_SERIALIZE(serializer, m_state.param_count);
    G_SERIALIZE(serializer, m_state.busy);
    G_SERIALIZE(serializer, m_state.execute_clocks);
    G_SERIALIZE_ARRAY(serializer, m_state.queue, CDROM_MOCK_QUEUE_SIZE);
    G_SERIALIZE(serializer, m_state.queue_count);
    G_SERIALIZE(serializer, m_state.sirq);
    G_SERIALIZE(serializer, m_state.enable_sirq);
    G_SERIALIZE(serializer, m_state.enable_dei);
}
