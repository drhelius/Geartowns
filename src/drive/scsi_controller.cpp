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

#include "scsi_controller.h"
#include "../system/pic.h"
#include "../system/scheduler.h"
#include "../system/upd71071.h"
#include "../common/trace_logger.h"
#include "../common/state_serializer.h"

ScsiController::ScsiController()
{
    InitPointer(m_pic);
    InitPointer(m_scheduler);
    InitPointer(m_dma);
    InitPointer(m_trace_logger);
    memset(&m_state, 0, sizeof(m_state));
    m_state.target = -1;
    m_fitted = false;
}

ScsiController::~ScsiController()
{
}

void ScsiController::Init(PIC* pic, Scheduler* scheduler, UPD71071* dma)
{
    m_pic = pic;
    m_scheduler = scheduler;
    m_dma = dma;

    GT_DMA_Endpoint endpoint = { this, DMAReadCallback, DMAWriteCallback, NULL };
    m_dma->SetEndpoint(k_scsi_dma_channel, endpoint);

    Reset();
}

void ScsiController::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

void ScsiController::SetFitted(bool fitted)
{
    m_fitted = fitted;
}

void ScsiController::Reset()
{
    memset(&m_state, 0, sizeof(m_state));
    m_state.target = -1;
    m_scheduler->Schedule(SCHEDULER_EVENT_SCSI, GT_NO_EVENT);
    UpdateIRQ();
}

u8 ScsiController::Read(u16 port, u64 clocks)
{
    UNUSED(clocks);

    if (!m_fitted)
        return 0xFF;

    switch (port)
    {
        case 0x0C30:
            return m_state.data;
        case 0x0C32:
            return ReadStatus();
        default:
            return 0xFF;
    }
}

u8 ScsiController::Peek(u16 port) const
{
    if (!m_fitted)
        return 0xFF;

    switch (port)
    {
        case 0x0C30:
            return m_state.data;
        case 0x0C32:
            return ReadStatus();
        default:
            return 0xFF;
    }
}

void ScsiController::Write(u16 port, u8 value, u64 clocks)
{
    if (!m_fitted)
        return;

    switch (port)
    {
        case 0x0C30:
            m_state.data = value;
            break;
        case 0x0C32:
        {
            u8 previous = GetBusOutputs(m_state.control);
            u8 outputs = GetBusOutputs(value);
            m_state.control = value;
            Trace(TRACE_SCSI_CONTROL, value);

            if ((outputs & ~previous & k_scsi_control_rst) != 0)
                ResetBus(clocks);

            if ((outputs & ~previous & k_scsi_control_sel) != 0 && (outputs & k_scsi_control_rst) == 0 &&
                m_state.phase == SCSI_PHASE_BUS_FREE)
                Select(clocks);

            UpdateIRQ();
            break;
        }
    }
}

void ScsiController::HandleEvent(u64 clocks)
{
    UNUSED(clocks);
}

void ScsiController::ResetBus(u64 clocks)
{
    UNUSED(clocks);
    m_state.signals = 0;
    m_state.interrupt = false;
    m_state.phase = SCSI_PHASE_BUS_FREE;
    m_state.target = -1;
    m_scheduler->Schedule(SCHEDULER_EVENT_SCSI, GT_NO_EVENT);
    Trace(TRACE_SCSI_RESET, 0);
}

void ScsiController::Select(u64 clocks)
{
    UNUSED(clocks);
    Trace(TRACE_SCSI_SELECTION, m_state.data);
}

void ScsiController::UpdateIRQ()
{
    m_pic->SetIRQLine(k_scsi_irq, m_state.interrupt && (m_state.control & k_scsi_control_imsk) != 0);
}

void ScsiController::Trace(u8 event, u8 value)
{
    if (!IsValidPointer(m_trace_logger) || !m_trace_logger->IsEventEnabled(TRACE_SCSI, event))
        return;

    GT_Trace_Entry entry = {};
    entry.type = TRACE_SCSI;
    entry.event = event;
    entry.scsi.control = m_state.control;
    entry.scsi.status = ReadStatus();
    entry.scsi.value = value;
    entry.scsi.phase = m_state.phase;
    entry.scsi.target = m_state.target;
    m_trace_logger->TraceLog(entry);
}

bool ScsiController::DMAReadCallback(void* device, u16& value, bool word)
{
    UNUSED(device);
    UNUSED(value);
    UNUSED(word);
    return false;
}

bool ScsiController::DMAWriteCallback(void* device, u16 value, bool word)
{
    UNUSED(device);
    UNUSED(value);
    UNUSED(word);
    return false;
}

void ScsiController::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer, GT_SAVESTATE_VERSION);
}

void ScsiController::LoadState(std::istream& stream, int version)
{
    StateSerializer serializer(stream);
    Serialize(serializer, version);
    SanitizeState();
}

void ScsiController::Serialize(StateSerializer& serializer, int version)
{
    // States from before the interface leave the bus free
    if (version < 15)
    {
        memset(&m_state, 0, sizeof(m_state));
        m_state.target = -1;
        return;
    }

    G_SERIALIZE(serializer, m_state.control);
    G_SERIALIZE(serializer, m_state.data);
    G_SERIALIZE(serializer, m_state.signals);
    G_SERIALIZE(serializer, m_state.interrupt);
    G_SERIALIZE(serializer, m_state.phase);
    G_SERIALIZE(serializer, m_state.target);
}

void ScsiController::SanitizeState()
{
    if (m_state.phase >= SCSI_PHASE_COUNT)
        m_state.phase = SCSI_PHASE_BUS_FREE;

    if (m_state.target < -1 || m_state.target >= k_scsi_targets)
        m_state.target = -1;

    m_scheduler->Schedule(SCHEDULER_EVENT_SCSI, GT_NO_EVENT);
    UpdateIRQ();
}
