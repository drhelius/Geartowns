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

static const u8 k_scsi_phase_lines[SCSI_PHASE_COUNT] =
{
    /* Bus free    */ 0,
    /* Selection   */ 0,
    /* Command     */ k_scsi_status_cd,
    /* Data in     */ k_scsi_status_io,
    /* Data out    */ 0,
    /* Status      */ k_scsi_status_cd | k_scsi_status_io,
    /* Message in  */ k_scsi_status_cd | k_scsi_status_msg | k_scsi_status_io,
    /* Message out */ k_scsi_status_cd | k_scsi_status_msg
};

ScsiController::ScsiController()
{
    InitPointer(m_pic);
    InitPointer(m_scheduler);
    InitPointer(m_dma);
    InitPointer(m_trace_logger);
    memset(&m_state, 0, sizeof(m_state));
    m_state.target = -1;
    m_fitted = false;
    m_word_dma = false;
    m_word_port = false;
    InitPointer(m_read_context);
    InitPointer(m_read_callback);
}

ScsiController::~ScsiController()
{
}

void ScsiController::Init(PIC* pic, Scheduler* scheduler, UPD71071* dma)
{
    m_pic = pic;
    m_scheduler = scheduler;
    m_dma = dma;

    for (int i = 0; i < SCSI_TARGETS; i++)
        m_disks[i].Init(i);

    GT_DMA_Endpoint endpoint = { this, DMAReadCallback, DMAWriteCallback, NULL };
    m_dma->SetEndpoint(k_scsi_dma_channel, endpoint);

    Reset();
}

void ScsiController::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;

    for (int i = 0; i < SCSI_TARGETS; i++)
        m_disks[i].SetTraceLogger(trace_logger);
}

void ScsiController::SetFitted(bool fitted)
{
    m_fitted = fitted;
}

void ScsiController::SetFeatures(u32 features)
{
    m_word_dma = (features & GT_MACHINE_FEATURE_SCSI_WORD_DMA) != 0;
    m_word_port = (features & GT_MACHINE_FEATURE_SCSI_WORD_PORT) != 0;
}

bool ScsiController::Decodes(u16 port) const
{
    return (port != 0x0C34) || m_word_port;
}

void ScsiController::SetReadCallback(void* context, GT_Read_Callback callback)
{
    m_read_context = context;
    m_read_callback = callback;
}

void ScsiController::Reset()
{
    memset(&m_state, 0, sizeof(m_state));
    m_state.target = -1;
    m_state.event_clocks = GT_NO_EVENT;

    for (int i = 0; i < SCSI_TARGETS; i++)
        m_disks[i].Reset();

    m_scheduler->Schedule(SCHEDULER_EVENT_SCSI, GT_NO_EVENT);
    UpdateIRQ();
}

u8 ScsiController::Read(u16 port, u64 clocks)
{
    if (!m_fitted)
        return 0xFF;

    switch (port)
    {
        case 0x0C30:
            if (((m_state.signals & k_scsi_status_req) != 0) && IsInputPhase())
                return InputByte(clocks);

            return m_state.data;
        case 0x0C32:
            return ReadStatus();
        case 0x0C34:
            return m_word_port ? k_scsi_word_port : 0xFF;
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
        case 0x0C34:
            return m_word_port ? k_scsi_word_port : 0xFF;
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

            if (((m_state.control & k_scsi_control_wen) != 0) && ((m_state.signals & k_scsi_status_req) != 0) &&
                IsOutputPhase())
                OutputByte(value, clocks);
            break;
        case 0x0C32:
            WriteControl(value, clocks);
            break;
    }
}

void ScsiController::HandleEvent(u64 clocks)
{
    u8 event = m_state.event;
    m_state.event = SCSI_EVENT_NONE;
    m_state.event_clocks = GT_NO_EVENT;

    switch (event)
    {
        case SCSI_EVENT_BUSY:
            m_state.signals |= k_scsi_status_busy;
            break;
        case SCSI_EVENT_PHASE:
            EnterPhase(m_state.next_phase, clocks);
            break;
        case SCSI_EVENT_BUS_FREE:
            FreeBus();
            break;
        default:
            break;
    }
}

void ScsiController::WriteControl(u8 value, u64 clocks)
{
    u8 previous = GetBusOutputs(m_state.control);
    u8 outputs = GetBusOutputs(value);
    m_state.control = value;
    Trace(TRACE_SCSI_CONTROL, value);

    if ((outputs & ~previous & k_scsi_control_rst) != 0)
        ResetBus();
    else if ((outputs & k_scsi_control_rst) == 0)
    {
        if (((outputs & ~previous & k_scsi_control_sel) != 0) && (m_state.phase == SCSI_PHASE_BUS_FREE))
            Select(clocks);
        else if (((previous & ~outputs & k_scsi_control_sel) != 0) && (m_state.phase == SCSI_PHASE_SELECTION))
            EndSelection(clocks);
    }

    UpdateIRQ();
    UpdateDRQ();
}

void ScsiController::ResetBus()
{
    CancelEvent();

    for (int i = 0; i < SCSI_TARGETS; i++)
        m_disks[i].Reset();

    FreeBus();
    Trace(TRACE_SCSI_RESET, 0);
}

void ScsiController::Select(u64 clocks)
{
    m_state.target = -1;

    for (int i = 0; i < SCSI_TARGETS; i++)
    {
        if (((m_state.data & (1 << i)) != 0) && m_disks[i].IsAttached())
        {
            m_state.target = (s8)i;
            break;
        }
    }

    Trace(TRACE_SCSI_SELECTION, m_state.data);

    if (m_state.target < 0)
        return;

    m_state.phase = SCSI_PHASE_SELECTION;
    m_state.lun = 0;
    ScheduleEvent(SCSI_EVENT_BUSY, SCSI_PHASE_SELECTION, clocks + k_scsi_busy_clocks);
}

void ScsiController::EndSelection(u64 clocks)
{
    // SEL released before the target answered abandons the selection
    if ((m_state.signals & k_scsi_status_busy) == 0)
    {
        CancelEvent();
        m_state.phase = SCSI_PHASE_BUS_FREE;
        m_state.target = -1;
        return;
    }

    bool attention = (GetBusOutputs(m_state.control) & k_scsi_control_atn) != 0;
    m_state.command_count = 0;
    ScheduleEvent(SCSI_EVENT_PHASE, attention ? SCSI_PHASE_MESSAGE_OUT : SCSI_PHASE_COMMAND,
        clocks + k_scsi_phase_clocks);
}

void ScsiController::EnterPhase(u8 phase, u64 clocks)
{
    if (m_state.target < 0)
    {
        FreeBus();
        return;
    }

    ScsiDisk* disk = &m_disks[m_state.target];
    m_state.phase = phase;
    m_state.signals = k_scsi_status_busy | k_scsi_phase_lines[phase];
    Trace(TRACE_SCSI_PHASE, phase);

    switch (phase)
    {
        case SCSI_PHASE_DATA_IN:
            Trace(TRACE_SCSI_DATA_START, 0);

            if (!disk->ReadByte(m_state.data))
            {
                EndData(clocks);
                return;
            }
            break;
        case SCSI_PHASE_DATA_OUT:
            Trace(TRACE_SCSI_DATA_START, 0);
            break;
        case SCSI_PHASE_STATUS:
            m_state.data = disk->GetStatus();
            Trace(TRACE_SCSI_STATUS, m_state.data);
            break;
        case SCSI_PHASE_MESSAGE_IN:
            m_state.data = m_state.message;
            Trace(TRACE_SCSI_MESSAGE, m_state.message);
            break;
        default:
            break;
    }

    SetREQ(true);
}

void ScsiController::FreeBus()
{
    m_state.phase = SCSI_PHASE_BUS_FREE;
    m_state.target = -1;
    m_state.lun = 0;
    m_state.signals = 0;
    SetREQ(false);
    Trace(TRACE_SCSI_PHASE, SCSI_PHASE_BUS_FREE);
}

u8 ScsiController::InputByte(u64 clocks)
{
    u8 value = m_state.data;

    switch (m_state.phase)
    {
        case SCSI_PHASE_DATA_IN:
            if (!m_disks[m_state.target].ReadByte(m_state.data))
                EndData(clocks);
            break;
        case SCSI_PHASE_STATUS:
            SetREQ(false);
            m_state.message = k_scsi_message_command_complete;
            m_state.after_message = SCSI_PHASE_BUS_FREE;
            ScheduleEvent(SCSI_EVENT_PHASE, SCSI_PHASE_MESSAGE_IN, clocks + k_scsi_phase_clocks);
            break;
        case SCSI_PHASE_MESSAGE_IN:
            SetREQ(false);
            m_state.command_count = 0;

            if (m_state.after_message == SCSI_PHASE_BUS_FREE)
                ScheduleEvent(SCSI_EVENT_BUS_FREE, SCSI_PHASE_BUS_FREE, clocks + k_scsi_phase_clocks);
            else
                ScheduleEvent(SCSI_EVENT_PHASE, m_state.after_message, clocks + k_scsi_phase_clocks);
            break;
        default:
            break;
    }

    return value;
}

void ScsiController::OutputByte(u8 value, u64 clocks)
{
    switch (m_state.phase)
    {
        case SCSI_PHASE_COMMAND:
            if (m_state.command_count < SCSI_COMMAND_SIZE)
                m_state.command[m_state.command_count++] = value;

            SetREQ(false);

            if (m_state.command_count < GetCommandLength())
                ScheduleEvent(SCSI_EVENT_PHASE, SCSI_PHASE_COMMAND, clocks + k_scsi_phase_clocks);
            else
                ExecuteCommand(clocks);
            break;
        case SCSI_PHASE_DATA_OUT:
        {
            ScsiDisk* disk = &m_disks[m_state.target];

            if (!disk->WriteByte(value) || disk->IsDataDone())
                EndData(clocks);
            break;
        }
        case SCSI_PHASE_MESSAGE_OUT:
            SetREQ(false);
            HandleMessage(value, clocks);
            break;
        default:
            break;
    }
}

void ScsiController::ExecuteCommand(u64 clocks)
{
    ScsiDisk* disk = &m_disks[m_state.target];
    Scsi_Data data = SCSI_DATA_NONE;
    Trace(TRACE_SCSI_COMMAND, m_state.command[0]);

    if (((m_state.command[0] == 0x08) || (m_state.command[0] == 0x28)) && IsValidPointer(m_read_callback))
        m_read_callback(m_read_context);

    // Logical units other than 0 get CHECK CONDITION
    if ((m_state.lun != 0) || ((m_state.command[1] >> 5) != 0))
        disk->RejectLUN();
    else
        data = disk->Execute(m_state.command);

    u8 phase = SCSI_PHASE_STATUS;

    if (data == SCSI_DATA_IN)
        phase = SCSI_PHASE_DATA_IN;
    else if (data == SCSI_DATA_OUT)
        phase = SCSI_PHASE_DATA_OUT;

    ScheduleEvent(SCSI_EVENT_PHASE, phase, clocks + k_scsi_phase_clocks);
}

void ScsiController::HandleMessage(u8 value, u64 clocks)
{
    Trace(TRACE_SCSI_MESSAGE, value);
    bool attention = (GetBusOutputs(m_state.control) & k_scsi_control_atn) != 0;
    u8 next = attention ? SCSI_PHASE_MESSAGE_OUT : SCSI_PHASE_COMMAND;

    if ((value & k_scsi_message_identify) != 0)
        m_state.lun = value & 0x07;
    else if ((value == k_scsi_message_abort) || (value == k_scsi_message_device_reset))
    {
        if (value == k_scsi_message_device_reset)
            m_disks[m_state.target].Reset();

        ScheduleEvent(SCSI_EVENT_BUS_FREE, SCSI_PHASE_BUS_FREE, clocks + k_scsi_phase_clocks);
        return;
    }
    else if (value != k_scsi_message_no_operation)
    {
        m_state.message = k_scsi_message_reject;
        m_state.after_message = next;
        ScheduleEvent(SCSI_EVENT_PHASE, SCSI_PHASE_MESSAGE_IN, clocks + k_scsi_phase_clocks);
        return;
    }

    m_state.command_count = 0;
    ScheduleEvent(SCSI_EVENT_PHASE, next, clocks + k_scsi_phase_clocks);
}

void ScsiController::EndData(u64 clocks)
{
    SetREQ(false);
    Trace(TRACE_SCSI_DATA_END, 0);
    ScheduleEvent(SCSI_EVENT_PHASE, SCSI_PHASE_STATUS, clocks + k_scsi_phase_clocks);
}

void ScsiController::ScheduleEvent(u8 event, u8 phase, u64 clocks)
{
    m_state.event = event;
    m_state.next_phase = phase;
    m_state.event_clocks = clocks;
    m_scheduler->Schedule(SCHEDULER_EVENT_SCSI, clocks);
}

void ScsiController::CancelEvent()
{
    m_state.event = SCSI_EVENT_NONE;
    m_state.event_clocks = GT_NO_EVENT;
    m_scheduler->Schedule(SCHEDULER_EVENT_SCSI, GT_NO_EVENT);
}

void ScsiController::SetREQ(bool active)
{
    if (active)
        m_state.signals |= k_scsi_status_req;
    else
        m_state.signals &= (u8)~k_scsi_status_req;

    m_state.interrupt = active && ((m_state.signals & k_scsi_status_cd) != 0);
    UpdateIRQ();
    UpdateDRQ();
}

void ScsiController::UpdateIRQ()
{
    m_pic->SetIRQLine(k_scsi_irq, m_state.interrupt && (m_state.control & k_scsi_control_imsk) != 0);
}

void ScsiController::UpdateDRQ()
{
    bool data = (m_state.phase == SCSI_PHASE_DATA_IN) || (m_state.phase == SCSI_PHASE_DATA_OUT);
    bool drq = data && ((m_state.signals & k_scsi_status_req) != 0) && ((m_state.control & k_scsi_control_dmae) != 0);

    if (drq == m_state.drq)
        return;

    m_state.drq = drq;
    m_dma->SetRequest(k_scsi_dma_channel, drq);
}

u32 ScsiController::GetCommandLength() const
{
    switch (m_state.command[0] >> 5)
    {
        case 1:
        case 2:
            return 10;
        case 4:
            return 16;
        case 5:
            return 12;
        default:
            return 6;
    }
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
    memcpy(entry.scsi.command, m_state.command, sizeof(entry.scsi.command));
    entry.scsi.count = GetCommandLength();

    if (m_state.target >= 0)
    {
        const ScsiDisk::ScsiDisk_State* disk = m_disks[m_state.target].GetState();
        entry.scsi.key = disk->sense_key;
        entry.scsi.code = disk->sense_code;

        if ((event == TRACE_SCSI_DATA_START) || (event == TRACE_SCSI_DATA_END))
        {
            entry.scsi.lba = disk->length;
            entry.scsi.count = disk->position;
        }
    }

    m_trace_logger->TraceLog(entry);
}

bool ScsiController::DMAReadCallback(void* device, u16& value, bool word)
{
    ScsiController* scsi = (ScsiController*)device;
    u64 clocks = scsi->m_scheduler->GetClocks();

    if (!scsi->m_state.drq || (scsi->m_state.phase != SCSI_PHASE_DATA_IN))
        return false;

    value = scsi->InputByte(clocks);

    if (word && scsi->IsWordDMA())
        value |= (u16)((scsi->m_state.drq && (scsi->m_state.phase == SCSI_PHASE_DATA_IN) ?
            scsi->InputByte(clocks) : 0xFF) << 8);

    return true;
}

bool ScsiController::DMAWriteCallback(void* device, u16 value, bool word)
{
    ScsiController* scsi = (ScsiController*)device;
    u64 clocks = scsi->m_scheduler->GetClocks();

    if (!scsi->m_state.drq || (scsi->m_state.phase != SCSI_PHASE_DATA_OUT))
        return false;

    scsi->OutputByte((u8)value, clocks);

    if (word && scsi->IsWordDMA() && scsi->m_state.drq && (scsi->m_state.phase == SCSI_PHASE_DATA_OUT))
        scsi->OutputByte((u8)(value >> 8), clocks);

    return true;
}

bool ScsiController::IsWordDMA() const
{
    return m_word_dma && ((m_state.control & k_scsi_control_wb) != 0);
}

void ScsiController::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer, GT_SAVESTATE_VERSION);

    for (int i = 0; i < SCSI_TARGETS; i++)
        m_disks[i].SaveState(stream);
}

void ScsiController::LoadState(std::istream& stream, int version)
{
    StateSerializer serializer(stream);
    Serialize(serializer, version);

    for (int i = 0; i < SCSI_TARGETS; i++)
        m_disks[i].LoadState(stream, version);

    SanitizeState();
}

void ScsiController::Serialize(StateSerializer& serializer, int version)
{
    // States from before the interface leave the bus free
    if (version < 15)
    {
        memset(&m_state, 0, sizeof(m_state));
        m_state.target = -1;
        m_state.event_clocks = GT_NO_EVENT;
        return;
    }

    G_SERIALIZE(serializer, m_state.control);
    G_SERIALIZE(serializer, m_state.data);
    G_SERIALIZE(serializer, m_state.signals);
    G_SERIALIZE(serializer, m_state.interrupt);
    G_SERIALIZE(serializer, m_state.drq);
    G_SERIALIZE(serializer, m_state.phase);
    G_SERIALIZE(serializer, m_state.target);
    G_SERIALIZE(serializer, m_state.lun);
    G_SERIALIZE_ARRAY(serializer, m_state.command, SCSI_COMMAND_SIZE);
    G_SERIALIZE(serializer, m_state.command_count);
    G_SERIALIZE(serializer, m_state.message);
    G_SERIALIZE(serializer, m_state.after_message);
    G_SERIALIZE(serializer, m_state.event);
    G_SERIALIZE(serializer, m_state.next_phase);
    G_SERIALIZE(serializer, m_state.event_clocks);
}

void ScsiController::SanitizeState()
{
    if ((m_state.phase >= SCSI_PHASE_COUNT) || (m_state.target < 0) || (m_state.target >= SCSI_TARGETS))
    {
        m_state.phase = SCSI_PHASE_BUS_FREE;
        m_state.target = -1;
        m_state.signals = 0;
        m_state.event = SCSI_EVENT_NONE;
    }

    if ((m_state.event >= SCSI_EVENT_COUNT) || (m_state.next_phase >= SCSI_PHASE_COUNT) ||
        (m_state.after_message >= SCSI_PHASE_COUNT))
        m_state.event = SCSI_EVENT_NONE;

    m_state.command_count = (u8)MIN((int)m_state.command_count, SCSI_COMMAND_SIZE);
    m_state.lun &= 0x07;
    m_scheduler->Schedule(SCHEDULER_EVENT_SCSI, m_state.event != SCSI_EVENT_NONE ? m_state.event_clocks : GT_NO_EVENT);
    UpdateIRQ();
}
