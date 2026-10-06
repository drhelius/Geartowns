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

#include "mb8877.h"
#include "../common/trace_logger.h"
#include "fdc.h"
#include "floppy_disk.h"
#include "floppy_image.h"
#include "../common/state_serializer.h"

// Track layout in byte cells, counted from the start of an ID address mark (the first A1 in MFM, FE in FM)
// Reads complete a byte at the end of its cell, writes latch the data register at the start of the cell
struct MB8877_Format
{
    u32 preamble;
    u32 sync;
    u32 overhead;
    u32 id_byte;
    u32 id_end;
    u32 data;
    u32 data_missing;
    u32 write_data;
    u8 gap;
    u8 gap3[4];
};

static const MB8877_Format k_mb8877_formats[2] =
{
    { 73, 6, 33, 2, 7, 19, 23, 18, 0xFF, { 27, 42, 58, 138 } },
    { 146, 12, 62, 5, 10, 39, 43, 38, 0x4E, { 27, 54, 84, 116 } }
};

static u16 crc16(u16 crc, const u8* data, u32 size)
{
    for (u32 i = 0; i < size; i++)
    {
        crc ^= (u16)(data[i] << 8);

        for (int bit = 0; bit < 8; bit++)
            crc = (crc & 0x8000) != 0 ? (u16)((crc << 1) ^ 0x1021) : (u16)(crc << 1);
    }

    return crc;
}

MB8877::MB8877()
{
    InitPointer(m_fdc);
    InitPointer(m_trace_logger);
    memset(&m_state, 0, sizeof(m_state));
    m_state.event_clocks = GT_NO_EVENT;
    m_state.index_clocks = GT_NO_EVENT;
}

MB8877::~MB8877()
{
}

void MB8877::Init(FDC* fdc)
{
    m_fdc = fdc;
}

void MB8877::Reset()
{
    memset(&m_state, 0, sizeof(m_state));
    m_state.command = 0x03;
    m_state.sector = 0x01;
    m_state.type1 = true;
    m_state.event_clocks = GT_NO_EVENT;
    m_state.index_clocks = GT_NO_EVENT;
    m_fdc->SetINTRQ(false);
    m_fdc->SetDRQ(false);
}

void MB8877::Run(u64 clocks)
{
    for (;;)
    {
        if (m_state.event_clocks <= clocks && m_state.event_clocks <= m_state.index_clocks)
            RunPhase(m_state.event_clocks);
        else if (m_state.index_clocks <= clocks)
            RunIndex(m_state.index_clocks);
        else
            break;
    }
}

u8 MB8877::ReadStatus(u64 clocks)
{
    if (m_state.intrq && (m_state.conditions & k_mb8877_interrupt_immediate) == 0)
        SetINTRQ(false);

    return PeekStatus(clocks);
}

u8 MB8877::PeekStatus(u64 clocks) const
{
    u8 value = m_state.status & (k_mb8877_busy | k_mb8877_crc_error | k_mb8877_not_found);

    if (m_state.type1)
    {
        if (m_fdc->IsIndex(clocks))
            value |= k_mb8877_index;

        if (m_fdc->IsTrackZero())
            value |= k_mb8877_track_zero;

        if (m_state.head_loaded)
            value |= k_mb8877_head_loaded;

        if (m_fdc->IsWriteProtected())
            value |= k_mb8877_write_protect;
    }
    else
    {
        value |= m_state.status & (k_mb8877_lost_data | k_mb8877_record_type | k_mb8877_write_protect);

        if (m_state.drq)
            value |= k_mb8877_drq;
    }

    if (!m_fdc->IsReady(clocks))
        value |= k_mb8877_not_ready;

    return value;
}

u8 MB8877::ReadData()
{
    DropDRQ();
    return m_state.data;
}

void MB8877::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

void MB8877::WriteCommand(u8 value, u64 clocks)
{
    SetINTRQ(false);

    if (IsValidPointer(m_trace_logger) && m_trace_logger->IsEnabled(TRACE_FDC))
    {
        GT_Trace_Entry* entry = m_trace_logger->Record(TRACE_FDC, TRACE_FDC_COMMAND);
        entry->fdc.command = value;
        entry->fdc.track = m_state.track;
        entry->fdc.sector = m_state.sector;
        entry->fdc.data = m_state.data;
        entry->fdc.status = m_state.status;
        entry->fdc.drive = (s8)m_fdc->GetSelectedDrive();
    }

    if ((value & 0xF0) == 0xD0)
    {
        ForceInterrupt(value, clocks);
        return;
    }

    if ((m_state.status & k_mb8877_busy) != 0)
        return;

    m_state.command = value;
    m_state.conditions = 0;
    m_state.index_clocks = GT_NO_EVENT;
    m_state.status |= k_mb8877_busy;
    DropDRQ();

    u32 cycles = m_fdc->IsDoubleDensity() ? k_mb8877_command_cycles : k_mb8877_command_cycles * 2;
    m_state.phase = MB8877_PHASE_COMMAND;
    m_state.event_clocks = clocks + GetCycles(cycles);
}

void MB8877::WriteTrackRegister(u8 value, u64 clocks)
{
    m_state.track = value;

    if (m_state.phase == MB8877_PHASE_SEARCH)
        Search(clocks, false);
}

void MB8877::WriteSectorRegister(u8 value, u64 clocks)
{
    m_state.sector = value;

    if (m_state.phase == MB8877_PHASE_SEARCH)
        Search(clocks, false);
}

void MB8877::WriteData(u8 value)
{
    m_state.data = value;
    DropDRQ();
}

// DMA only moves a byte while DRQ is up
// otherwise the channel waits for the next request
bool MB8877::ReadDMA(u8& value)
{
    if (!m_state.drq)
        return false;

    value = m_state.data;
    DropDRQ();
    return true;
}

bool MB8877::WriteDMA(u8 value)
{
    if (!m_state.drq)
        return false;

    m_state.data = value;
    DropDRQ();
    return true;
}

void MB8877::ReadyChanged(bool ready)
{
    bool rising = ready && (m_state.conditions & k_mb8877_interrupt_ready) != 0;
    bool falling = !ready && (m_state.conditions & k_mb8877_interrupt_not_ready) != 0;

    if (!m_state.intrq && (rising || falling))
        SetINTRQ(true);
}

void MB8877::MediaChanged()
{
    m_state.media_valid = false;
}

void MB8877::RunPhase(u64 clocks)
{
    u32 byte_clocks = m_fdc->GetByteClocks();

    switch (m_state.phase)
    {
        case MB8877_PHASE_COMMAND:
            StartCommand(clocks);
            break;
        case MB8877_PHASE_STEP:
            FinishStep(clocks);
            break;
        case MB8877_PHASE_SETTLE:
            if (!m_fdc->IsReady(clocks))
            {
                m_state.status |= k_mb8877_not_found;
                End();
            }
            else
                Search(clocks, true);
            break;
        case MB8877_PHASE_HEAD_LOAD:
            ContinueTypeII(clocks);
            break;
        case MB8877_PHASE_WRITE_PROTECT:
            m_state.status |= k_mb8877_write_protect;
            End();
            break;
        case MB8877_PHASE_SEARCH:
            FinishSearch(clocks);
            break;
        case MB8877_PHASE_NO_DATA:
            m_state.status |= k_mb8877_not_found;
            End();
            break;
        case MB8877_PHASE_READ_DATA:
            ReadByte();
            m_state.byte_index++;

            if (m_state.byte_index < m_state.byte_count)
                m_state.event_clocks += byte_clocks;
            else
            {
                m_state.phase = MB8877_PHASE_READ_END;
                m_state.event_clocks += byte_clocks * 2;
            }
            break;
        case MB8877_PHASE_READ_END:
            if (m_state.sector_status == k_floppy_status_data_crc || m_state.sector_size != m_state.byte_count)
            {
                m_state.status |= k_mb8877_crc_error;
                End();
            }
            else if ((m_state.command & 0x10) != 0)
            {
                m_state.sector++;
                Search(clocks, true);
            }
            else
                End();
            break;
        case MB8877_PHASE_WRITE_REQUEST:
            SetDRQ();
            m_state.phase = MB8877_PHASE_WRITE_CHECK;
            m_state.event_clocks += byte_clocks * 8;
            break;
        case MB8877_PHASE_WRITE_CHECK:
            // The first byte must arrive within 8 byte times of the request or nothing gets written
            if (m_state.drq)
            {
                m_state.status |= k_mb8877_lost_data;
                End();
            }
            else
            {
                const MB8877_Format& format = k_mb8877_formats[m_fdc->IsDoubleDensity() ? 1 : 0];
                m_state.byte_index = 0;
                m_state.phase = MB8877_PHASE_WRITE_DATA;
                m_state.event_clocks += byte_clocks * (format.write_data - 10);
            }
            break;
        case MB8877_PHASE_WRITE_DATA:
            WriteByte();
            m_state.byte_index++;

            if (m_state.byte_index < m_state.byte_count)
            {
                SetDRQ();
                m_state.event_clocks += byte_clocks;
            }
            else
            {
                m_state.phase = MB8877_PHASE_WRITE_END;
                m_state.event_clocks += byte_clocks * 4;
            }
            break;
        case MB8877_PHASE_WRITE_END:
            FinishWriteSector(clocks);
            break;
        case MB8877_PHASE_READ_ADDRESS:
            m_state.data = m_state.id[m_state.byte_index];

            if (m_state.byte_index == 0)
                m_state.sector = m_state.data;

            SetDRQ();
            m_state.byte_index++;

            if (m_state.byte_index < 6)
                m_state.event_clocks += byte_clocks;
            else
            {
                if (m_state.sector_status == k_floppy_status_id_crc)
                    m_state.status |= k_mb8877_crc_error;

                End();
            }
            break;
        case MB8877_PHASE_TRACK_LOAD:
            if (m_state.drq)
            {
                m_state.status |= k_mb8877_lost_data;
                DropDRQ();
                End();
            }
            else
            {
                m_state.phase = MB8877_PHASE_TRACK_INDEX;
                m_state.event_clocks = GetNextIndex(clocks);
            }
            break;
        case MB8877_PHASE_TRACK_INDEX:
            StartTrack(clocks);
            break;
        case MB8877_PHASE_READ_TRACK:
            m_state.data = m_state.track_buffer[m_state.byte_index];
            SetDRQ();
            m_state.byte_index++;

            if (m_state.byte_index < m_state.track_size)
                m_state.event_clocks += byte_clocks;
            else
                End();
            break;
        case MB8877_PHASE_WRITE_TRACK:
            WriteTrackCell(clocks);
            break;
        default:
            m_state.phase = MB8877_PHASE_IDLE;
            m_state.event_clocks = GT_NO_EVENT;
            break;
    }
}

void MB8877::RunIndex(u64 clocks)
{
    if (!m_state.intrq && m_fdc->IsSpinning())
        SetINTRQ(true);

    m_state.index_clocks = GetNextIndex(clocks);
}

void MB8877::StartCommand(u64 clocks)
{
    switch (m_state.command >> 4)
    {
        case 0x8:
        case 0x9:
        case 0xA:
        case 0xB:
        case 0xC:
        case 0xE:
        case 0xF:
            StartTypeII(clocks);
            break;
        default:
            StartTypeI(clocks);
            break;
    }
}

void MB8877::StartTypeI(u64 clocks)
{
    u8 operation = m_state.command >> 4;
    m_state.status &= ~(k_mb8877_crc_error | k_mb8877_not_found);
    m_state.type1 = true;
    m_state.head_loaded = (m_state.command & 0x08) != 0;
    m_state.steps = 0;

    switch (operation)
    {
        case 0x0:
            m_state.step_in = false;

            // At TR00 already it waits one step time without a pulse
            if (m_fdc->IsTrackZero())
            {
                m_state.phase = MB8877_PHASE_STEP;
                m_state.event_clocks = clocks + GetCycles(k_mb8877_step_cycles[m_state.command & 0x03]);
            }
            else
                Pulse(clocks);
            break;
        case 0x1:
            m_state.step_in = m_state.data > m_state.track;

            if (m_state.track == m_state.data)
                FinishSeek(clocks);
            else
                Pulse(clocks);
            break;
        case 0x4:
        case 0x5:
            m_state.step_in = true;
            Pulse(clocks);
            break;
        case 0x6:
        case 0x7:
            m_state.step_in = false;
            Pulse(clocks);
            break;
        default:
            Pulse(clocks);
            break;
    }
}

void MB8877::Pulse(u64 clocks)
{
    u8 operation = m_state.command >> 4;
    m_fdc->Step(m_state.step_in);

    if (operation >= 0x2 && (m_state.command & 0x10) != 0)
        m_state.track = (u8)(m_state.track + (m_state.step_in ? 1 : -1));

    m_state.steps++;
    m_state.phase = MB8877_PHASE_STEP;
    m_state.event_clocks = clocks + GetCycles(k_mb8877_step_cycles[m_state.command & 0x03]);
}

void MB8877::FinishStep(u64 clocks)
{
    u8 operation = m_state.command >> 4;
    bool done = true;

    if (operation == 0x0)
        done = m_fdc->IsTrackZero();
    else if (operation == 0x1)
    {
        m_state.track = (u8)(m_state.track + (m_state.step_in ? 1 : -1));
        done = m_state.track == m_state.data;
    }

    if (!done && m_state.steps < 255)
    {
        Pulse(clocks);
        return;
    }

    if (operation == 0x0)
    {
        if (done)
            m_state.track = 0;
        else
            m_state.status |= k_mb8877_not_found;
    }

    FinishSeek(clocks);
}

void MB8877::FinishSeek(u64 clocks)
{
    if ((m_state.command & 0x04) == 0)
    {
        End();
        return;
    }

    m_state.head_loaded = true;
    m_state.media_valid = true;
    m_state.phase = MB8877_PHASE_SETTLE;
    m_state.event_clocks = clocks + GetCycles(k_mb8877_settle_cycles);
}

void MB8877::StartTypeII(u64 clocks)
{
    if (!m_fdc->IsReady(clocks))
    {
        End();
        return;
    }

    m_state.status &= ~(k_mb8877_crc_error | k_mb8877_lost_data | k_mb8877_not_found | k_mb8877_record_type |
        k_mb8877_write_protect);
    m_state.type1 = false;
    m_state.head_loaded = true;
    m_state.media_valid = true;

    if ((m_state.command & 0x04) != 0)
    {
        m_state.phase = MB8877_PHASE_HEAD_LOAD;
        m_state.event_clocks = clocks + GetCycles(k_mb8877_settle_cycles);
    }
    else
        ContinueTypeII(clocks);
}

void MB8877::ContinueTypeII(u64 clocks)
{
    u8 operation = m_state.command >> 4;
    bool write = operation == 0xA || operation == 0xB || operation == 0xF;

    if (write && m_fdc->IsWriteProtected())
    {
        m_state.phase = MB8877_PHASE_WRITE_PROTECT;
        m_state.event_clocks = clocks + GetCycles(k_mb8877_write_protect_cycles);
        return;
    }

    switch (operation)
    {
        case 0xE:
            m_state.phase = MB8877_PHASE_TRACK_INDEX;
            m_state.event_clocks = GetNextIndex(clocks);
            break;
        case 0xF:
            SetDRQ();
            m_state.phase = MB8877_PHASE_TRACK_LOAD;
            m_state.event_clocks = clocks + GetCycles(k_mb8877_track_load_cycles);
            break;
        default:
            Search(clocks, true);
            break;
    }
}

void MB8877::ForceInterrupt(u8 value, u64 clocks)
{
    if (m_state.phase == MB8877_PHASE_WRITE_DATA || m_state.phase == MB8877_PHASE_WRITE_END)
    {
        m_state.deferred = value;
        return;
    }

    m_state.deferred = 0;

    if ((m_state.status & k_mb8877_busy) != 0)
    {
        m_state.phase = MB8877_PHASE_IDLE;
        m_state.event_clocks = GT_NO_EVENT;
        m_state.status &= ~k_mb8877_busy;
    }
    else
    {
        m_state.type1 = true;
        DropDRQ();
    }

    m_state.conditions = value & 0x0F;

    if (!m_state.intrq && (value & k_mb8877_interrupt_immediate) != 0)
        SetINTRQ(true);

    m_state.index_clocks = (value & k_mb8877_interrupt_index) != 0 ? GetNextIndex(clocks) : GT_NO_EVENT;
}

void MB8877::Search(u64 clocks, bool restart)
{
    u64 rpm = m_fdc->GetRPM();
    u64 angle = (clocks * rpm) % k_mb8877_turn;

    if (restart)
        m_state.deadline_clocks = clocks + (k_mb8877_turn - angle + k_mb8877_turn * 4 + rpm - 1) / rpm;

    const MB8877_Format& format = k_mb8877_formats[m_fdc->IsDoubleDensity() ? 1 : 0];
    bool address = (m_state.command & 0xF0) == 0xC0;
    u64 cell_units = (u64)m_fdc->GetByteClocks() * rpm;
    u32 target = address ? format.id_byte : format.id_end;
    FloppyDisk_Sector sectors[k_floppy_max_sectors];
    u32 positions[k_floppy_max_sectors];
    u32 cells = 0;
    int count = GetTrackLayout(sectors, positions, &cells);
    int best = -1;
    u64 best_delta = GT_NO_EVENT;
    bool bad = false;

    for (int i = 0; i < count; i++)
    {
        if (sectors[i].fm == m_fdc->IsDoubleDensity() || sectors[i].status == k_floppy_status_no_id ||
            !Matches(sectors[i]))
            continue;

        u64 delta = ((u64)positions[i] * cell_units + k_mb8877_turn - angle) % k_mb8877_turn;

        if (!address && sectors[i].status == k_floppy_status_id_crc)
            bad = true;
        else if (delta < best_delta)
        {
            best_delta = delta;
            best = i;
        }
    }

    m_state.phase = MB8877_PHASE_SEARCH;
    m_state.event_clocks = m_state.deadline_clocks;
    m_state.found = false;
    m_state.crc_seen = bad;

    if (best < 0)
        return;

    u64 found_clocks = clocks + (best_delta + target * cell_units + rpm - 1) / rpm;

    if (found_clocks > m_state.deadline_clocks)
        return;

    const FloppyDisk_Sector& sector = sectors[best];
    u8 mark[8] = { 0xA1, 0xA1, 0xA1, 0xFE, sector.id[0], sector.id[1], sector.id[2], sector.id[3] };
    u16 crc = m_fdc->IsDoubleDensity() ? crc16(0xFFFF, mark, 8) : crc16(0xFFFF, mark + 3, 5);

    if (sector.status == k_floppy_status_id_crc)
        crc = (u16)~crc;

    m_state.found = true;
    m_state.event_clocks = found_clocks;
    m_state.sector_header = sector.header;
    m_state.sector_size = sector.size;
    m_state.sector_status = sector.status;
    m_state.sector_deleted = sector.deleted;
    memcpy(m_state.id, sector.id, 4);
    write_u16_be(m_state.id + 4, crc);
}

void MB8877::FinishSearch(u64 clocks)
{
    if (m_state.crc_seen)
        m_state.status |= k_mb8877_crc_error;

    if (!m_state.found)
    {
        m_state.status |= k_mb8877_not_found;
        End();
        return;
    }

    u8 operation = m_state.command >> 4;
    const MB8877_Format& format = k_mb8877_formats[m_fdc->IsDoubleDensity() ? 1 : 0];
    u32 byte_clocks = m_fdc->GetByteClocks();
    m_state.byte_index = 0;
    m_state.byte_count = 128U << (m_state.id[3] & 0x03);

    switch (operation)
    {
        case 0x8:
        case 0x9:
            if (m_state.sector_status == k_floppy_status_no_data)
            {
                m_state.phase = MB8877_PHASE_NO_DATA;
                m_state.event_clocks = clocks + byte_clocks * format.data_missing;
                break;
            }

            if (m_state.sector_deleted)
                m_state.status |= k_mb8877_record_type;

            m_state.phase = MB8877_PHASE_READ_DATA;
            m_state.event_clocks = clocks + byte_clocks * format.data;
            break;
        case 0xA:
        case 0xB:
            m_state.phase = MB8877_PHASE_WRITE_REQUEST;
            m_state.event_clocks = clocks + byte_clocks * 2;
            break;
        case 0xC:
            m_state.phase = MB8877_PHASE_READ_ADDRESS;
            m_state.event_clocks = clocks;
            RunPhase(clocks);
            break;
        default:
            End();
            break;
    }
}

void MB8877::ReadByte()
{
    FloppyDisk* disk = m_state.media_valid ? m_fdc->GetSelectedDisk() : NULL;
    u8 gap = k_mb8877_formats[m_fdc->IsDoubleDensity() ? 1 : 0].gap;
    u8 value = 0x00;

    if (IsValidPointer(disk))
    {
        value = m_state.byte_index < m_state.sector_size ?
            disk->ReadByte(m_state.sector_header + k_floppy_sector_header_size + m_state.byte_index, gap) : gap;
    }

    m_state.data = value;
    SetDRQ();
}

void MB8877::WriteByte()
{
    u8 value = m_state.data;

    if (m_state.drq)
    {
        m_state.status |= k_mb8877_lost_data;
        value = 0x00;
    }

    FloppyDisk* disk = m_state.media_valid ? m_fdc->GetSelectedDisk() : NULL;

    if (IsValidPointer(disk) && m_state.byte_index < m_state.sector_size)
        disk->WriteByte(m_state.sector_header + k_floppy_sector_header_size + m_state.byte_index, value);
}

void MB8877::FinishWriteSector(u64 clocks)
{
    FloppyDisk* disk = m_state.media_valid ? m_fdc->GetSelectedDisk() : NULL;

    if (IsValidPointer(disk))
    {
        u8 status = m_state.sector_size == m_state.byte_count ? 0x00 : k_floppy_status_data_crc;
        disk->SetSectorStatus(m_state.sector_header, (m_state.command & 0x01) != 0, status);
    }

    if (m_state.deferred != 0)
    {
        u8 deferred = m_state.deferred;
        m_state.phase = MB8877_PHASE_IDLE;
        ForceInterrupt(deferred, clocks);
        return;
    }

    if ((m_state.command & 0x10) != 0)
    {
        m_state.sector++;
        Search(clocks, true);
    }
    else
        End();
}

void MB8877::StartTrack(u64 clocks)
{
    u32 byte_clocks = m_fdc->GetByteClocks();
    u32 cells = (u32)(k_mb8877_turn / ((u64)byte_clocks * m_fdc->GetRPM()));
    m_state.track_size = 0;
    m_state.byte_index = 0;
    m_state.byte_count = MIN(cells, (u32)MB8877_TRACK_BUFFER_SIZE);

    if ((m_state.command & 0xF0) == 0xE0)
    {
        BuildTrack();
        m_state.track_size = m_state.byte_count;
        m_state.phase = MB8877_PHASE_READ_TRACK;
        m_state.event_clocks = clocks + byte_clocks;
    }
    else
    {
        m_state.crc_pending = false;
        m_state.phase = MB8877_PHASE_WRITE_TRACK;
        m_state.event_clocks = clocks;
        WriteTrackCell(clocks);
    }
}

void MB8877::WriteTrackCell(u64 clocks)
{
    UNUSED(clocks);

    if (m_state.crc_pending)
        m_state.crc_pending = false;
    else
    {
        u8 value = m_state.data;

        if (m_state.drq)
        {
            m_state.status |= k_mb8877_lost_data;
            value = 0x00;
        }

        m_state.track_buffer[m_state.track_size++] = value;
        m_state.crc_pending = value == 0xF7;
        SetDRQ();
    }

    m_state.byte_index++;

    if (m_state.byte_index < m_state.byte_count)
        m_state.event_clocks += m_fdc->GetByteClocks();
    else
        FinishWriteTrack();
}

void MB8877::FinishWriteTrack()
{
    FloppyDisk* disk = m_state.media_valid ? m_fdc->GetSelectedDisk() : NULL;
    int track = m_fdc->GetSelectedTrack();

    if (IsValidPointer(disk) && track >= 0)
    {
        u8 run[k_floppy_max_sectors * k_floppy_sector_header_size + MB8877_TRACK_BUFFER_SIZE];
        u32 size = m_fdc->IsSelectedFormattable() ? ParseTrack(run) : 0;
        disk->ReplaceTrack(track, run, size, m_fdc->GetRPM());
    }

    DropDRQ();
    End();
}

void MB8877::End()
{
    m_state.phase = MB8877_PHASE_IDLE;
    m_state.event_clocks = GT_NO_EVENT;
    m_state.status &= ~k_mb8877_busy;

    if (IsValidPointer(m_trace_logger) && m_trace_logger->IsEnabled(TRACE_FDC))
    {
        GT_Trace_Entry* entry = m_trace_logger->Record(TRACE_FDC, TRACE_FDC_END);
        entry->fdc.command = m_state.command;
        entry->fdc.track = m_state.track;
        entry->fdc.sector = m_state.sector;
        entry->fdc.data = m_state.data;
        entry->fdc.status = m_state.status;
        entry->fdc.drive = (s8)m_fdc->GetSelectedDrive();
    }

    if (m_state.drq && (m_state.status & k_mb8877_lost_data) != 0)
    {
        m_state.drq = false;
        m_fdc->SetDRQ(false);
    }

    if (!m_state.drq)
        SetINTRQ(true);
}

void MB8877::SetDRQ()
{
    if (m_state.drq)
        m_state.status |= k_mb8877_lost_data;
    else if ((m_state.status & k_mb8877_lost_data) == 0)
    {
        m_state.drq = true;
        m_fdc->SetDRQ(true);
    }
}

void MB8877::DropDRQ()
{
    if (!m_state.drq)
        return;

    if ((m_state.status & k_mb8877_busy) == 0 && !m_state.intrq)
        SetINTRQ(true);

    m_state.drq = false;
    m_fdc->SetDRQ(false);
}

void MB8877::SetINTRQ(bool active)
{
    m_state.intrq = active;
    m_fdc->SetINTRQ(active);
}

bool MB8877::Matches(const FloppyDisk_Sector& sector) const
{
    u8 operation = m_state.command >> 4;

    if (operation == 0xC)
        return true;

    if (sector.id[0] != m_state.track)
        return false;

    if (operation < 0x8)
        return true;

    if (sector.id[2] != m_state.sector)
        return false;

    return (m_state.command & 0x02) == 0 || (sector.id[1] & 0x01) == ((m_state.command >> 3) & 0x01);
}

int MB8877::GetTrackLayout(FloppyDisk_Sector* sectors, u32* positions, u32* cells) const
{
    *cells = (u32)(k_mb8877_turn / ((u64)m_fdc->GetByteClocks() * m_fdc->GetRPM()));

    if (!m_state.media_valid || !m_fdc->IsSelectedReadable())
        return 0;

    FloppyDisk* disk = m_fdc->GetSelectedDisk();
    int track = m_fdc->GetSelectedTrack();

    if (!IsValidPointer(disk) || track < 0)
        return 0;

    const MB8877_Format& format = k_mb8877_formats[m_fdc->IsDoubleDensity() ? 1 : 0];
    int count = disk->GetSectors(track, sectors);
    u32 fixed = format.preamble;
    u32 gaps = 0;

    for (int i = 0; i < count; i++)
    {
        fixed += format.overhead + sectors[i].size;
        gaps += format.gap3[sectors[i].id[3] & 0x03];
    }

    bool squeeze = fixed + gaps > *cells;
    u32 squeezed = (count > 0 && fixed < *cells) ? (*cells - fixed) / count : 0;
    u32 position = format.preamble;

    for (int i = 0; i < count; i++)
    {
        positions[i] = (position + format.sync) % *cells;
        position += format.overhead + sectors[i].size + (squeeze ? squeezed : format.gap3[sectors[i].id[3] & 0x03]);
    }

    return count;
}

void MB8877::BuildTrack()
{
    bool mfm = m_fdc->IsDoubleDensity();
    const MB8877_Format& format = k_mb8877_formats[mfm ? 1 : 0];
    FloppyDisk_Sector sectors[k_floppy_max_sectors];
    u32 positions[k_floppy_max_sectors];
    u32 cells = 0;
    int count = GetTrackLayout(sectors, positions, &cells);
    FloppyDisk* disk = m_fdc->GetSelectedDisk();
    u32 size = m_state.byte_count;
    u8* buffer = m_state.track_buffer;
    u32 position = 0;

    memset(buffer, format.gap, size);

    u32 sync_start = mfm ? 80 : 40;
    memset(buffer + sync_start, 0x00, format.sync);
    position = sync_start + format.sync;

    if (mfm)
    {
        buffer[position++] = 0xC2;
        buffer[position++] = 0xC2;
        buffer[position++] = 0xC2;
    }

    buffer[position++] = 0xFC;

    for (int i = 0; i < count && IsValidPointer(disk); i++)
    {
        const FloppyDisk_Sector& sector = sectors[i];

        if (sector.fm == mfm)
            continue;

        u32 data_size = 128U << (sector.id[3] & 0x03);
        u32 mark = positions[i];
        u32 needed = format.overhead + data_size;

        if (mark < format.sync || mark - format.sync + needed > size)
            continue;

        u8 field[8] = { 0xA1, 0xA1, 0xA1, 0xFE, sector.id[0], sector.id[1], sector.id[2], sector.id[3] };
        u32 start = mfm ? 0 : 3;
        u32 length = 8 - start;
        position = mark - format.sync;
        memset(buffer + position, 0x00, format.sync);
        position += format.sync;

        if (sector.status != k_floppy_status_no_id)
        {
            u16 crc = crc16(0xFFFF, field + start, length);

            if (sector.status == k_floppy_status_id_crc)
                crc = (u16)~crc;

            memcpy(buffer + position, field + start, length);
            write_u16_be(buffer + position + length, crc);
        }

        position += length + 2 + (mfm ? 22 : 11);
        memset(buffer + position, 0x00, format.sync);
        position += format.sync;

        if (sector.status == k_floppy_status_no_data)
            continue;

        u8* data_mark = buffer + position;
        field[3] = sector.deleted ? 0xF8 : 0xFB;
        memcpy(data_mark, field + start, length - 4);
        position += length - 4;

        for (u32 j = 0; j < data_size; j++)
        {
            buffer[position + j] = j < sector.size ?
                disk->ReadByte(sector.header + k_floppy_sector_header_size + j, format.gap) : format.gap;
        }

        u16 crc = crc16(0xFFFF, data_mark, length - 4 + data_size);

        if (sector.status == k_floppy_status_data_crc || sector.size != data_size)
            crc = (u16)~crc;

        position += data_size;
        write_u16_be(buffer + position, crc);
    }
}

u32 MB8877::ParseTrack(u8* run) const
{
    bool mfm = m_fdc->IsDoubleDensity();
    const u8* stream = m_state.track_buffer;
    u32 length = m_state.track_size;
    u32 size = 0;
    u16 count = 0;
    u32 i = 0;

    while (i < length && count < k_floppy_max_sectors)
    {
        u32 mark = i;

        if (mfm)
        {
            if (stream[i] != 0xF5)
            {
                i++;
                continue;
            }

            while (mark < length && stream[mark] == 0xF5)
                mark++;
        }

        if (mark + 4 >= length || stream[mark] != 0xFE)
        {
            i = MAX(mark, i + 1);
            continue;
        }

        const u8* id = stream + mark + 1;
        u32 data_size = 128U << (id[3] & 0x03);
        u32 position = mark + 5;
        u32 data_start = 0;
        bool deleted = false;

        for (u32 k = position; k < length && k < position + 64; k++)
        {
            u8 value = stream[k];

            if (mfm && value != 0xF5)
                continue;

            u32 next = k;

            if (mfm)
            {
                while (next < length && stream[next] == 0xF5)
                    next++;

                if (next >= length)
                    break;

                value = stream[next];
            }

            if (value == 0xFE)
                break;

            if (value == 0xFB || value == 0xF8 || (!mfm && (value == 0xF9 || value == 0xFA)))
            {
                data_start = next + 1;
                deleted = value != 0xFB;
                break;
            }

            k = next;
        }

        u8* header = run + size;
        memset(header, 0, k_floppy_sector_header_size);
        memcpy(header, id, 4);
        header[6] = mfm ? 0x00 : k_floppy_density_fm;
        size += k_floppy_sector_header_size;

        if (data_start == 0)
        {
            header[8] = k_floppy_status_no_data;
            i = position;
        }
        else
        {
            u32 available = MIN(data_size, length - data_start);
            header[7] = deleted ? k_floppy_deleted : 0x00;
            header[8] = available < data_size ? k_floppy_status_data_crc : 0x00;
            write_u16_le(header + 14, (u16)data_size);
            memcpy(run + size, stream + data_start, available);
            memset(run + size + available, 0x00, data_size - available);
            size += data_size;
            i = data_start + available;
        }

        count++;
    }

    for (u32 p = 0; p < size; p += k_floppy_sector_header_size + read_u16_le(run + p + 14))
        write_u16_le(run + p + 4, count);

    return size;
}

u64 MB8877::GetNextIndex(u64 clocks) const
{
    u64 rpm = m_fdc->GetRPM();
    u64 angle = (clocks * rpm) % k_mb8877_turn;
    return clocks + (k_mb8877_turn - angle + rpm - 1) / rpm;
}

u64 MB8877::GetCycles(u32 cycles) const
{
    return (u64)cycles * m_fdc->GetCycleClocks();
}

void MB8877::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void MB8877::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void MB8877::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_state.command);
    G_SERIALIZE(serializer, m_state.track);
    G_SERIALIZE(serializer, m_state.sector);
    G_SERIALIZE(serializer, m_state.data);
    G_SERIALIZE(serializer, m_state.status);
    G_SERIALIZE(serializer, m_state.phase);
    G_SERIALIZE(serializer, m_state.conditions);
    G_SERIALIZE(serializer, m_state.deferred);
    G_SERIALIZE(serializer, m_state.type1);
    G_SERIALIZE(serializer, m_state.head_loaded);
    G_SERIALIZE(serializer, m_state.step_in);
    G_SERIALIZE(serializer, m_state.drq);
    G_SERIALIZE(serializer, m_state.intrq);
    G_SERIALIZE(serializer, m_state.media_valid);
    G_SERIALIZE(serializer, m_state.found);
    G_SERIALIZE(serializer, m_state.crc_seen);
    G_SERIALIZE(serializer, m_state.crc_pending);
    G_SERIALIZE(serializer, m_state.steps);
    G_SERIALIZE(serializer, m_state.event_clocks);
    G_SERIALIZE(serializer, m_state.index_clocks);
    G_SERIALIZE(serializer, m_state.deadline_clocks);
    G_SERIALIZE(serializer, m_state.sector_header);
    G_SERIALIZE(serializer, m_state.sector_size);
    G_SERIALIZE(serializer, m_state.sector_status);
    G_SERIALIZE(serializer, m_state.sector_deleted);
    G_SERIALIZE_ARRAY(serializer, m_state.id, 6);
    G_SERIALIZE(serializer, m_state.byte_index);
    G_SERIALIZE(serializer, m_state.byte_count);
    G_SERIALIZE(serializer, m_state.track_size);

    if (serializer.IsLoading())
        m_state.track_size = MIN(m_state.track_size, (u32)MB8877_TRACK_BUFFER_SIZE);

    if (m_state.track_size > 0)
        G_SERIALIZE_ARRAY(serializer, m_state.track_buffer, m_state.track_size);
}

void MB8877::SanitizeState()
{
    if (m_state.phase >= MB8877_PHASE_COUNT)
    {
        m_state.phase = MB8877_PHASE_IDLE;
        m_state.event_clocks = GT_NO_EVENT;
    }

    m_state.byte_count = MIN(m_state.byte_count, (u32)MB8877_TRACK_BUFFER_SIZE);
    m_state.byte_index = MIN(m_state.byte_index, m_state.byte_count);

    if (m_state.phase == MB8877_PHASE_READ_ADDRESS)
        m_state.byte_index = MIN(m_state.byte_index, 5U);

    if (m_state.phase == MB8877_PHASE_READ_TRACK)
        m_state.byte_index = MIN(m_state.byte_index, m_state.track_size > 0 ? m_state.track_size - 1 : 0);

    if (m_state.phase == MB8877_PHASE_WRITE_TRACK)
        m_state.track_size = MIN(m_state.track_size, (u32)MB8877_TRACK_BUFFER_SIZE - 1);

    m_fdc->SetINTRQ(m_state.intrq);
}
