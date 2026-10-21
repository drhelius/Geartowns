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

#ifndef SCSI_CONTROLLER_H
#define SCSI_CONTROLLER_H

#include <iostream>
#include "../common/common.h"
#include "scsi_disk.h"

#define SCSI_TARGETS GT_MAX_HARD_DISKS
#define SCSI_COMMAND_SIZE 16

class PIC;
class Scheduler;
class UPD71071;
class StateSerializer;
class TraceLogger;

enum Scsi_Phase
{
    SCSI_PHASE_BUS_FREE = 0,
    SCSI_PHASE_SELECTION,
    SCSI_PHASE_COMMAND,
    SCSI_PHASE_DATA_IN,
    SCSI_PHASE_DATA_OUT,
    SCSI_PHASE_STATUS,
    SCSI_PHASE_MESSAGE_IN,
    SCSI_PHASE_MESSAGE_OUT,
    SCSI_PHASE_COUNT
};

enum Scsi_Event
{
    SCSI_EVENT_NONE = 0,
    SCSI_EVENT_BUSY,
    SCSI_EVENT_PHASE,
    SCSI_EVENT_BUS_FREE,
    SCSI_EVENT_COUNT
};

class ScsiController
{
public:
    struct ScsiController_State
    {
        u8 control;
        u8 data;
        u8 signals;
        bool interrupt;
        bool drq;
        u8 phase;
        s8 target;
        u8 lun;
        u8 command[SCSI_COMMAND_SIZE];
        u8 command_count;
        u8 message;
        u8 after_message;
        u8 event;
        u8 next_phase;
        u64 event_clocks;
    };

public:
    ScsiController();
    ~ScsiController();
    void Init(PIC* pic, Scheduler* scheduler, UPD71071* dma);
    void SetTraceLogger(TraceLogger* trace_logger);
    void SetFitted(bool fitted);
    bool IsFitted() const;
    void SetFeatures(u32 features);
    bool Decodes(u16 port) const;
    void Reset();
    u8 Read(u16 port, u64 clocks);
    u8 Peek(u16 port) const;
    void Write(u16 port, u8 value, u64 clocks);
    void HandleEvent(u64 clocks);
    ScsiDisk* GetDisk(int id);
    ScsiController_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream, int version = GT_SAVESTATE_VERSION);

private:
    u8 ReadStatus() const;
    u8 GetBusOutputs(u8 control) const;
    bool IsInputPhase() const;
    bool IsOutputPhase() const;
    bool IsWordDMA() const;
    void WriteControl(u8 value, u64 clocks);
    void ResetBus();
    void Select(u64 clocks);
    void EndSelection(u64 clocks);
    void EnterPhase(u8 phase, u64 clocks);
    void FreeBus();
    u8 InputByte(u64 clocks);
    void OutputByte(u8 value, u64 clocks);
    void ExecuteCommand(u64 clocks);
    void HandleMessage(u8 value, u64 clocks);
    void EndData(u64 clocks);
    void ScheduleEvent(u8 event, u8 phase, u64 clocks);
    void CancelEvent();
    void SetREQ(bool active);
    void UpdateIRQ();
    void UpdateDRQ();
    u32 GetCommandLength() const;
    void Trace(u8 event, u8 value);
    static bool DMAReadCallback(void* device, u16& value, bool word);
    static bool DMAWriteCallback(void* device, u16 value, bool word);
    void Serialize(StateSerializer& serializer, int version);
    void SanitizeState();

private:
    PIC* m_pic;
    Scheduler* m_scheduler;
    UPD71071* m_dma;
    TraceLogger* m_trace_logger;
    ScsiDisk m_disks[SCSI_TARGETS];
    ScsiController_State m_state;
    bool m_fitted;
    bool m_word_dma;
    bool m_word_port;
};

static const int k_scsi_irq = 8;
static const int k_scsi_dma_channel = 1;

static const u8 k_scsi_control_wen = 0x80;
static const u8 k_scsi_control_imsk = 0x40;
static const u8 k_scsi_control_atn = 0x10;
static const u8 k_scsi_control_wb = 0x08;
static const u8 k_scsi_control_sel = 0x04;
static const u8 k_scsi_control_dmae = 0x02;
static const u8 k_scsi_control_rst = 0x01;

static const u8 k_scsi_status_req = 0x80;
static const u8 k_scsi_status_io = 0x40;
static const u8 k_scsi_status_msg = 0x20;
static const u8 k_scsi_status_cd = 0x10;
static const u8 k_scsi_status_busy = 0x08;
static const u8 k_scsi_status_int = 0x02;

static const u8 k_scsi_message_command_complete = 0x00;
static const u8 k_scsi_message_abort = 0x06;
static const u8 k_scsi_message_reject = 0x07;
static const u8 k_scsi_message_no_operation = 0x08;
static const u8 k_scsi_message_device_reset = 0x0C;
static const u8 k_scsi_message_identify = 0x80;

static const u64 k_scsi_busy_clocks = GT_MACHINE_CLOCK_RATE / 200000;
static const u64 k_scsi_phase_clocks = GT_MACHINE_CLOCK_RATE / 2000;
static const u32 k_scsi_fast_dma_fifths = (u32)(((u64)GT_MACHINE_CLOCK_RATE * 165 * 5) / 100000000);
static const u8 k_scsi_word_port = 0x7F;

#include "scsi_controller_inline.h"

#endif /* SCSI_CONTROLLER_H */
