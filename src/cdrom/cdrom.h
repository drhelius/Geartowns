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

#ifndef CDROM_H
#define CDROM_H

#include <iostream>
#include "../common/common.h"

#define CDROM_PARAM_COUNT 8
#define CDROM_STATUS_QUEUE_SIZE 1024
#define CDROM_SECTOR_SIZE 2352

class Audio;
class CdRomAudio;
class CdRomMedia;
class Scheduler;
class StateSerializer;
class PIC;
class TraceLogger;
class UPD71071;

class CdRom
{
public:
    enum CdRom_Command
    {
        CDROM_COMMAND_SEEK = 0x00,
        CDROM_COMMAND_MODE2_READ = 0x01,
        CDROM_COMMAND_MODE1_READ = 0x02,
        CDROM_COMMAND_RAW_READ = 0x03,
        CDROM_COMMAND_CDDA_PLAY = 0x04,
        CDROM_COMMAND_TOC_READ = 0x05,
        CDROM_COMMAND_SUBQ_READ = 0x06,
        CDROM_COMMAND_UNKNOWN_1F = 0x1F,
        CDROM_COMMAND_GET_STATE = 0x80,
        CDROM_COMMAND_CDDA_SET = 0x81,
        CDROM_COMMAND_CDDA_STOP = 0x84,
        CDROM_COMMAND_CDDA_PAUSE = 0x85,
        CDROM_COMMAND_UNKNOWN_86 = 0x86,
        CDROM_COMMAND_CDDA_RESUME = 0x87,
        CDROM_COMMAND_UNKNOWN_9F = 0x9F
    };

    enum CdRom_Event
    {
        CDROM_EVENT_NONE,
        CDROM_EVENT_EXECUTE,
        CDROM_EVENT_SEEK_DONE,
        CDROM_EVENT_SECTOR,
        CDROM_EVENT_LOST_DATA,
        CDROM_EVENT_CDDA_STOP
    };

    enum CdRom_Transfer
    {
        CDROM_TRANSFER_NONE,
        CDROM_TRANSFER_READY,
        CDROM_TRANSFER_DMA,
        CDROM_TRANSFER_CPU
    };

    struct CdRom_State
    {
        u8 command;
        bool command_received;
        u8 params[CDROM_PARAM_COUNT];
        u8 param_count;
        u8 active_command;
        u8 active_params[CDROM_PARAM_COUNT];
        u8 status[CDROM_STATUS_QUEUE_SIZE];
        u16 status_head;
        u16 status_count;
        bool sirq;
        bool sirq_irq;
        bool dei;
        bool enable_sirq;
        bool enable_dei;
        bool dry;
        bool disc_changed;
        CdRom_Event event;
        u64 event_clocks;
        CdRom_Transfer transfer;
        u32 read_lba;
        u32 read_end_lba;
        u32 head_lba;
        bool buffer_valid;
        u8 buffer_capacity;
        u32 buffer_lba;
        u32 prefetch_lba;
        u64 prefetch_clocks;
        u16 sector_position;
        u16 sector_end;
        u8 sector[CDROM_SECTOR_SIZE];
    };

public:
    CdRom(CdRomMedia* cdrom_media, CdRomAudio* cdrom_audio);
    ~CdRom();
    void Init(PIC* pic, Scheduler* scheduler, UPD71071* dma, Audio* audio);
    void SetTraceLogger(TraceLogger* trace_logger);
    void Reset();
    void NotifyMediaChanged();
    u8 Read(u16 port, u64 clocks);
    void Write(u16 port, u8 value, u64 clocks);
    u8 Peek(u16 port) const;
    void Synchronize(u64 clocks);
    void HandleEvent(u64 clocks);
    void SetReadSpeed(int speed);
    int GetReadSpeed() const;
    CdRom_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

    static bool DMAReadCallback(void* device, u16& value, bool word);
    static void DMAEndCallback(void* device, bool terminal_count);

private:
    void ResetController();
    void WriteControl(u8 value);
    void WriteCommand(u8 value, u64 clocks);
    void WriteParameter(u8 value, u64 clocks);
    void WriteTransferControl(u8 value);
    u8 ReadStatus();
    u8 ReadData();
    bool DMARead(u16& value, bool word);
    void DMAEnd();
    void CheckCommand(u64 clocks);
    void RunEvent(u64 clocks);
    void ExecuteCommand(u64 clocks);
    void CommandSeek(u64 clocks);
    void CommandRead(u64 clocks);
    void CommandCDDAPlay();
    void CommandTOCRead();
    void CommandSubQRead();
    void CommandUnknown1F();
    void CommandGetState();
    void CommandCDDASet();
    void CommandCDDAStop(u64 clocks);
    void CommandCDDAPause();
    void CommandCDDAResume();
    void CommandUnknown9F();
    void SeekDone();
    void SectorReady(u64 clocks);
    void LostData();
    void ReadDone();
    void StopCDDA();
    bool LoadSector();
    void FinishSector();
    u64 StartBuffer(u64 clocks);
    void UpdateBuffer(u64 clocks);
    void ReleaseBuffer(u32 lba, u64 clocks);
    u64 GetArrivalClocks(u32 lba, u64 clocks) const;
    void InvalidateBuffer();
    void AbortTransfer();
    bool PushErrorStatus();
    void PushStatus(u8 status0, u8 status1, u8 status2 = 0x00, u8 status3 = 0x00);
    void ClearStatus();
    u8 GetStatusSecondByte();
    bool GetRange(u32& start, u32& end) const;
    void RaiseSIRQ(bool irq);
    void UpdateIRQ();
    void ScheduleEvent(CdRom_Event event, u64 clocks);
    void UpdateNextEvent();
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    PIC* m_pic;
    Scheduler* m_scheduler;
    UPD71071* m_dma;
    Audio* m_audio;
    CdRomMedia* m_cdrom_media;
    CdRomAudio* m_cdrom_audio;
    TraceLogger* m_trace_logger;
    CdRom_State m_state;
    int m_read_speed;
    u64 m_sector_clocks;
    double m_seek_scale;
};

static const int k_cdrom_irq = 9;
static const int k_cdrom_dma_channel = 3;
static const u8 k_cdrom_flag_irq = 0x40;
static const u8 k_cdrom_flag_status = 0x20;
static const u8 k_cdrom_command_mask = 0x9F;
static const u64 k_cdrom_command_clocks = GT_CPU_CLOCK_RATE / 1000;
static const u64 k_cdrom_sector_clocks = GT_CPU_CLOCK_RATE / 75;
static const u64 k_cdrom_notify_clocks = GT_CPU_CLOCK_RATE / 1000;
static const u64 k_cdrom_seek_clocks = GT_CPU_CLOCK_RATE / 10;
static const u64 k_cdrom_lost_data_clocks = GT_CPU_CLOCK_RATE / 10;
static const u64 k_cdrom_cdda_stop_clocks = GT_CPU_CLOCK_RATE / 1000;
static const u32 k_cdrom_buffer_size = 8192;

#include "cdrom_inline.h"

#endif /* CDROM_H */
