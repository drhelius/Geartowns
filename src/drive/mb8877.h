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

#ifndef MB8877_H
#define MB8877_H

#include <iostream>
#include "../common/common.h"

#define MB8877_TRACK_BUFFER_SIZE 16384

class FDC;
class StateSerializer;
struct FloppyDisk_Sector;

enum MB8877_Phase
{
    MB8877_PHASE_IDLE = 0,
    MB8877_PHASE_COMMAND,
    MB8877_PHASE_STEP,
    MB8877_PHASE_SETTLE,
    MB8877_PHASE_HEAD_LOAD,
    MB8877_PHASE_WRITE_PROTECT,
    MB8877_PHASE_SEARCH,
    MB8877_PHASE_NO_DATA,
    MB8877_PHASE_READ_DATA,
    MB8877_PHASE_READ_END,
    MB8877_PHASE_WRITE_REQUEST,
    MB8877_PHASE_WRITE_CHECK,
    MB8877_PHASE_WRITE_DATA,
    MB8877_PHASE_WRITE_END,
    MB8877_PHASE_READ_ADDRESS,
    MB8877_PHASE_TRACK_LOAD,
    MB8877_PHASE_TRACK_INDEX,
    MB8877_PHASE_READ_TRACK,
    MB8877_PHASE_WRITE_TRACK,
    MB8877_PHASE_COUNT
};

class MB8877
{
public:
    struct MB8877_State
    {
        u8 command;
        u8 track;
        u8 sector;
        u8 data;
        u8 status;
        u8 phase;
        u8 conditions;
        u8 deferred;
        bool type1;
        bool head_loaded;
        bool step_in;
        bool drq;
        bool intrq;
        bool media_valid;
        bool found;
        bool crc_seen;
        bool crc_pending;
        u16 steps;
        u64 event_clocks;
        u64 index_clocks;
        u64 deadline_clocks;
        u32 sector_header;
        u16 sector_size;
        u8 sector_status;
        bool sector_deleted;
        u8 id[6];
        u32 byte_index;
        u32 byte_count;
        u32 track_size;
        u8 track_buffer[MB8877_TRACK_BUFFER_SIZE];
    };

public:
    MB8877();
    ~MB8877();
    void Init(FDC* fdc);
    void Reset();
    void Run(u64 clocks);
    u64 GetEventClocks() const;
    u8 ReadStatus(u64 clocks);
    u8 ReadTrackRegister() const;
    u8 ReadSectorRegister() const;
    u8 ReadData();
    void WriteCommand(u8 value, u64 clocks);
    void WriteTrackRegister(u8 value, u64 clocks);
    void WriteSectorRegister(u8 value, u64 clocks);
    void WriteData(u8 value);
    bool ReadDMA(u8& value);
    bool WriteDMA(u8 value);
    void ReadyChanged(bool ready);
    void MediaChanged();
    MB8877_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void RunPhase(u64 clocks);
    void RunIndex(u64 clocks);
    void StartCommand(u64 clocks);
    void StartTypeI(u64 clocks);
    void Pulse(u64 clocks);
    void FinishStep(u64 clocks);
    void FinishSeek(u64 clocks);
    void StartTypeII(u64 clocks);
    void ContinueTypeII(u64 clocks);
    void ForceInterrupt(u8 value, u64 clocks);
    void Search(u64 clocks, bool restart);
    void FinishSearch(u64 clocks);
    void ReadByte();
    void WriteByte();
    void FinishWriteSector(u64 clocks);
    void StartTrack(u64 clocks);
    void WriteTrackCell(u64 clocks);
    void FinishWriteTrack();
    void End();
    void SetDRQ();
    void DropDRQ();
    void SetINTRQ(bool active);
    bool Matches(const FloppyDisk_Sector& sector) const;
    int GetTrackLayout(FloppyDisk_Sector* sectors, u32* positions, u32* cells) const;
    void BuildTrack();
    u32 ParseTrack(u8* run) const;
    u64 GetNextIndex(u64 clocks) const;
    u64 GetCycles(u32 cycles) const;
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    FDC* m_fdc;
    MB8877_State m_state;
};

static const u8 k_mb8877_busy = 0x01;
static const u8 k_mb8877_index = 0x02;
static const u8 k_mb8877_drq = 0x02;
static const u8 k_mb8877_track_zero = 0x04;
static const u8 k_mb8877_lost_data = 0x04;
static const u8 k_mb8877_crc_error = 0x08;
static const u8 k_mb8877_not_found = 0x10;
static const u8 k_mb8877_head_loaded = 0x20;
static const u8 k_mb8877_record_type = 0x20;
static const u8 k_mb8877_write_protect = 0x40;
static const u8 k_mb8877_not_ready = 0x80;

static const u8 k_mb8877_interrupt_ready = 0x01;
static const u8 k_mb8877_interrupt_not_ready = 0x02;
static const u8 k_mb8877_interrupt_index = 0x04;
static const u8 k_mb8877_interrupt_immediate = 0x08;

static const u32 k_mb8877_step_cycles[4] = { 6000, 12000, 20000, 30000 };
static const u32 k_mb8877_settle_cycles = 30000;
static const u32 k_mb8877_command_cycles = 12;
static const u32 k_mb8877_write_protect_cycles = 145;
static const u32 k_mb8877_track_load_cycles = 192;

static const u64 k_mb8877_turn = (u64)GT_CPU_CLOCK_RATE * 60;

#include "mb8877_inline.h"

#endif /* MB8877_H */
