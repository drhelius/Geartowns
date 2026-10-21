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

class ScsiController
{
public:
    struct ScsiController_State
    {
        u8 control;
        u8 data;
        u8 signals;
        bool interrupt;
        u8 phase;
        s8 target;
    };

public:
    ScsiController();
    ~ScsiController();
    void Init(PIC* pic, Scheduler* scheduler, UPD71071* dma);
    void SetTraceLogger(TraceLogger* trace_logger);
    void SetFitted(bool fitted);
    bool IsFitted() const;
    void Reset();
    u8 Read(u16 port, u64 clocks);
    u8 Peek(u16 port) const;
    void Write(u16 port, u8 value, u64 clocks);
    void HandleEvent(u64 clocks);
    ScsiController_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream, int version = GT_SAVESTATE_VERSION);

private:
    u8 ReadStatus() const;
    u8 GetBusOutputs(u8 control) const;
    void ResetBus(u64 clocks);
    void Select(u64 clocks);
    void UpdateIRQ();
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
    ScsiController_State m_state;
    bool m_fitted;
};

static const int k_scsi_irq = 8;
static const int k_scsi_dma_channel = 1;
static const int k_scsi_targets = 7;

static const u8 k_scsi_control_wen = 0x80;
static const u8 k_scsi_control_imsk = 0x40;
static const u8 k_scsi_control_atn = 0x10;
static const u8 k_scsi_control_sel = 0x04;
static const u8 k_scsi_control_dmae = 0x02;
static const u8 k_scsi_control_rst = 0x01;

static const u8 k_scsi_status_req = 0x80;
static const u8 k_scsi_status_io = 0x40;
static const u8 k_scsi_status_msg = 0x20;
static const u8 k_scsi_status_cd = 0x10;
static const u8 k_scsi_status_busy = 0x08;
static const u8 k_scsi_status_int = 0x02;

#include "scsi_controller_inline.h"

#endif /* SCSI_CONTROLLER_H */
