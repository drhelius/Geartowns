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

#include <math.h>
#include "cdrom.h"
#include "../common/trace_logger.h"
#include "cdrom_audio.h"
#include "cdrom_media.h"
#include "../audio/audio.h"
#include "../system/scheduler.h"
#include "../system/pic.h"
#include "../system/upd71071.h"
#include "../common/state_serializer.h"

CdRom::CdRom(CdRomMedia* cdrom_media, CdRomAudio* cdrom_audio)
{
    InitPointer(m_pic);
    InitPointer(m_scheduler);
    InitPointer(m_dma);
    InitPointer(m_audio);
    InitPointer(m_trace_logger);
    m_cdrom_media = cdrom_media;
    m_cdrom_audio = cdrom_audio;
    memset(&m_state, 0, sizeof(m_state));
    m_read_speed = 1;
    m_sector_clocks = k_cdrom_sector_clocks;
    m_seek_scale = 1.0;
}

CdRom::~CdRom()
{
}

void CdRom::Init(PIC* pic, Scheduler* scheduler, UPD71071* dma, Audio* audio)
{
    m_pic = pic;
    m_scheduler = scheduler;
    m_dma = dma;
    m_audio = audio;

    GT_DMA_Endpoint endpoint = { this, DMAReadCallback, NULL, DMAEndCallback };
    m_dma->SetEndpoint(k_cdrom_dma_channel, endpoint);

    Reset();
}

void CdRom::Reset()
{
    ResetController();
    m_state.enable_sirq = false;
    m_state.enable_dei = false;
    m_state.disc_changed = false;
    m_state.active_command = 0;
    memset(m_state.active_params, 0, sizeof(m_state.active_params));
    UpdateIRQ();
}

// Swapping discs drops a read from the old disc and latches the change for the next status
void CdRom::NotifyMediaChanged()
{
    m_state.disc_changed = true;
    m_cdrom_audio->Stop();
    InvalidateBuffer();

    bool reading = (m_state.transfer != CDROM_TRANSFER_NONE) || (m_state.event == CDROM_EVENT_SECTOR) ||
        (m_state.event == CDROM_EVENT_LOST_DATA);

    if (!reading)
        return;

    AbortTransfer();
    m_state.event = CDROM_EVENT_NONE;
    m_state.dry = true;
    UpdateNextEvent();
}

u8 CdRom::Read(u16 port, u64 clocks)
{
    Synchronize(clocks);

    switch (port)
    {
        case 0x04C2:
            return ReadStatus();
        case 0x04C4:
            return ReadData();
        default:
            return Peek(port);
    }
}

void CdRom::Write(u16 port, u8 value, u64 clocks)
{
    Synchronize(clocks);

    switch (port)
    {
        case 0x04C0:
            WriteControl(value);
            break;
        case 0x04C2:
            WriteCommand(value, clocks);
            break;
        case 0x04C4:
            WriteParameter(value, clocks);
            break;
        case 0x04C6:
            WriteTransferControl(value);
            break;
        default:
            break;
    }
}

u8 CdRom::Peek(u16 port) const
{
    switch (port)
    {
        case 0x04C0:
        {
            u8 value = m_state.dry ? 0x01 : 0x00;
            value |= (m_state.status_count != 0) ? 0x02 : 0x00;
            value |= (m_state.transfer == CDROM_TRANSFER_DMA) ? 0x10 : 0x00;
            value |= (m_state.transfer == CDROM_TRANSFER_CPU) ? 0x20 : 0x00;
            value |= m_state.dei ? 0x40 : 0x00;
            value |= m_state.sirq ? 0x80 : 0x00;
            return value;
        }
        case 0x04C2:
            return (m_state.status_count != 0) ? m_state.status[m_state.status_head] : 0xFF;
        case 0x04C4:
            return (m_state.transfer == CDROM_TRANSFER_CPU) ? m_state.sector[m_state.sector_position] : 0x00;
        case 0x04CC:
            // No subcode byte is ever ready because subcode is not streamed
        case 0x04CD:
            return 0x00;
        default:
            return 0xFF;
    }
}

bool CdRom::DMAReadCallback(void* device, u16& value, bool word)
{
    CdRom* cdrom = (CdRom*)device;
    return cdrom->DMARead(value, word);
}

void CdRom::DMAEndCallback(void* device, bool terminal_count)
{
    UNUSED(terminal_count);
    CdRom* cdrom = (CdRom*)device;
    cdrom->DMAEnd();
}

// SRST restarts the sub-MPU interface
// CD-DA keeps playing
void CdRom::ResetController()
{
    AbortTransfer();
    InvalidateBuffer();
    m_state.command = 0;
    m_state.command_received = false;
    memset(m_state.params, 0, sizeof(m_state.params));
    m_state.param_count = 0;
    m_state.status_head = 0;
    m_state.status_count = 0;
    m_state.sirq = false;
    m_state.sirq_irq = false;
    m_state.dei = false;
    m_state.dry = true;
    m_state.event = CDROM_EVENT_NONE;
    m_state.event_clocks = 0;
    m_state.read_lba = 0;
    m_state.read_end_lba = 0;
    m_state.sector_position = 0;
    m_state.sector_end = 0;
    UpdateNextEvent();
}

// The two low bits load both masks on every write
// The acknowledge and reset bits only act when set
void CdRom::WriteControl(u8 value)
{
    if ((value & 0x80) != 0)
    {
        m_state.sirq = false;
        m_state.sirq_irq = false;
    }

    if ((value & 0x40) != 0)
        m_state.dei = false;

    if ((value & 0x04) != 0)
        ResetController();

    m_state.enable_sirq = (value & 0x02) != 0;
    m_state.enable_dei = (value & 0x01) != 0;
    UpdateIRQ();
}

void CdRom::WriteCommand(u8 value, u64 clocks)
{
    // Command written before the previous SIRQ is acknowledged inherit its STATUS and IRQ requests
    // Fractal Engine Demo issues 00h this way and still waits for the status and the IRQ
    if (m_state.sirq)
        value |= m_state.command & (k_cdrom_flag_irq | k_cdrom_flag_status);

    m_state.command = value;
    m_state.command_received = true;
    CheckCommand(clocks);
}

// Extra parameters slide the eight byte window and the oldest one is lost
void CdRom::WriteParameter(u8 value, u64 clocks)
{
    if (m_state.param_count == CDROM_PARAM_COUNT)
    {
        memmove(m_state.params, m_state.params + 1, CDROM_PARAM_COUNT - 1);
        m_state.param_count = CDROM_PARAM_COUNT - 1;
    }

    m_state.params[m_state.param_count++] = value;
    CheckCommand(clocks);
}

// DMA wins when both transfer bits are written
void CdRom::WriteTransferControl(u8 value)
{
    if ((value & 0x18) == 0)
        return;

    if (m_state.transfer != CDROM_TRANSFER_READY)
    {
        Debug("CDROM: transfer start %02X without a sector ready", value);
        return;
    }

    // A sector that starts draining can no longer be lost
    if (m_state.event == CDROM_EVENT_LOST_DATA)
    {
        m_state.event = CDROM_EVENT_NONE;
        UpdateNextEvent();
    }

    if ((value & 0x10) != 0)
    {
        m_state.transfer = CDROM_TRANSFER_DMA;
        m_dma->SetRequest(k_cdrom_dma_channel, true);
    }
    else
        m_state.transfer = CDROM_TRANSFER_CPU;
}

u8 CdRom::ReadStatus()
{
    if (m_state.status_count == 0)
        return 0xFF;

    u8 value = m_state.status[m_state.status_head];
    m_state.status_head = (m_state.status_head + 1) & (CDROM_STATUS_QUEUE_SIZE - 1);
    m_state.status_count--;

    // Commands that asked for an IRQ raise it again for each packet left
    if ((m_state.status_count != 0) && ((m_state.status_head & 0x03) == 0) &&
        ((m_state.active_command & k_cdrom_flag_irq) != 0))
        RaiseSIRQ(true);

    return value;
}

u8 CdRom::ReadData()
{
    if (m_state.transfer != CDROM_TRANSFER_CPU)
    {
        Debug("CDROM: data read without a CPU transfer");
        return 0x00;
    }

    u8 value = m_state.sector[m_state.sector_position++];

    if (m_state.sector_position >= m_state.sector_end)
        FinishSector();

    return value;
}

bool CdRom::DMARead(u16& value, bool word)
{
    if (m_state.transfer != CDROM_TRANSFER_DMA)
        return false;

    value = m_state.sector[m_state.sector_position++];

    if (word && (m_state.sector_position < m_state.sector_end))
        value |= (u16)(m_state.sector[m_state.sector_position++] << 8);

    if (m_state.sector_position >= m_state.sector_end)
        FinishSector();

    return true;
}

// A count that runs out before the sector ends drops the rest of it
void CdRom::DMAEnd()
{
    if (m_state.transfer == CDROM_TRANSFER_DMA)
        FinishSector();
}

// The sub-MPU takes the command once it has all eight parameters
// Puzznic sends a TOC read without parameters
// Only command and parameter writes check it because Awesome acknowledges 04C0h constantly
void CdRom::CheckCommand(u64 clocks)
{
    if (!m_state.command_received)
        return;

    if ((m_state.param_count < CDROM_PARAM_COUNT) &&
        ((m_state.command & k_cdrom_command_mask) != CDROM_COMMAND_TOC_READ))
        return;

    m_state.active_command = m_state.command;
    memcpy(m_state.active_params, m_state.params, CDROM_PARAM_COUNT);
    m_state.command_received = false;
    m_state.param_count = 0;
    m_state.dry = false;

    // The answer comes later because Fractal Engine Demo acknowledges SIRQ right after the command
    ScheduleEvent(CDROM_EVENT_EXECUTE, clocks + k_cdrom_command_clocks);
}

void CdRom::RunEvent(u64 clocks)
{
    CdRom_Event event = m_state.event;
    m_state.event = CDROM_EVENT_NONE;

    // CD-DA state must be current before the drive answers about it
    m_audio->Synchronize(clocks);

    switch (event)
    {
        case CDROM_EVENT_EXECUTE:
            ExecuteCommand(clocks);
            break;
        case CDROM_EVENT_SEEK_DONE:
            SeekDone();
            break;
        case CDROM_EVENT_SECTOR:
            SectorReady(clocks);
            break;
        case CDROM_EVENT_LOST_DATA:
            LostData();
            break;
        case CDROM_EVENT_CDDA_STOP:
            StopCDDA();
            break;
        default:
            break;
    }

    UpdateIRQ();
    UpdateNextEvent();
}

void CdRom::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

void CdRom::ExecuteCommand(u64 clocks)
{
    if (IsValidPointer(m_trace_logger) && m_trace_logger->IsEnabled(TRACE_CDROM))
    {
        GT_Trace_Entry* entry = m_trace_logger->Record(TRACE_CDROM, TRACE_CDROM_COMMAND);
        entry->cdrom.command = m_state.active_command;
        memcpy(entry->cdrom.bytes, m_state.active_params, CDROM_PARAM_COUNT);
    }

    // A new command takes over from a read that was still running
    AbortTransfer();
    m_state.dry = true;

    switch (m_state.active_command & k_cdrom_command_mask)
    {
        case CDROM_COMMAND_SEEK:
            CommandSeek(clocks);
            break;
        case CDROM_COMMAND_MODE2_READ:
        case CDROM_COMMAND_MODE1_READ:
        case CDROM_COMMAND_RAW_READ:
            CommandRead(clocks);
            break;
        case CDROM_COMMAND_CDDA_PLAY:
            CommandCDDAPlay();
            break;
        case CDROM_COMMAND_TOC_READ:
            CommandTOCRead();
            break;
        case CDROM_COMMAND_SUBQ_READ:
            CommandSubQRead();
            break;
        case CDROM_COMMAND_UNKNOWN_1F:
            CommandUnknown1F();
            break;
        case CDROM_COMMAND_GET_STATE:
            CommandGetState();
            break;
        case CDROM_COMMAND_CDDA_SET:
            CommandCDDASet();
            break;
        case CDROM_COMMAND_CDDA_STOP:
            CommandCDDAStop(clocks);
            break;
        case CDROM_COMMAND_CDDA_PAUSE:
            CommandCDDAPause();
            break;
        case CDROM_COMMAND_CDDA_RESUME:
            CommandCDDAResume();
            break;
        case CDROM_COMMAND_UNKNOWN_9F:
            CommandUnknown9F();
            break;
        default:
            Debug("CDROM: unsupported command %02X", m_state.active_command);
            break;
    }
}

// DRY stays set while the head moves
// The 2F ROM waits for 00h and then for 04h
void CdRom::CommandSeek(u64 clocks)
{
    ClearStatus();
    InvalidateBuffer();

    if (((m_state.active_command & k_cdrom_flag_status) != 0) && !PushErrorStatus())
    {
        PushStatus(0x00, GetStatusSecondByte());

        if ((m_state.active_command & k_cdrom_flag_irq) != 0)
            RaiseSIRQ(true);
    }

    ScheduleEvent(CDROM_EVENT_SEEK_DONE, clocks + k_cdrom_seek_clocks);
}

void CdRom::CommandRead(u64 clocks)
{
    u32 start = 0;
    u32 end = 0;

    // The pickup leaves CD-DA to read data
    m_cdrom_audio->Stop();

    // Without a disc the read itself fails instead of reporting not ready
    if (!m_cdrom_media->IsReady())
    {
        PushStatus(0x21, 0x09);
        return;
    }

    // The range is inclusive and absolute
    // 00:02:00 is the first sector
    if (!GetRange(start, end) || (start < 150) || (start > end) || ((end - 150) >= m_cdrom_media->GetSectorCount()))
    {
        Debug("CDROM: invalid read range %02X:%02X:%02X-%02X:%02X:%02X", m_state.active_params[0],
            m_state.active_params[1], m_state.active_params[2], m_state.active_params[3], m_state.active_params[4],
            m_state.active_params[5]);
        PushStatus(0x21, 0x01);
        return;
    }

    m_state.read_lba = start - 150;
    m_state.read_end_lba = end - 150;

    // The acceptance comes with or without STATUS
    // The 2F ROM checks it before Data Ready
    PushStatus(0x00, GetStatusSecondByte());

    if (m_state.enable_sirq)
        RaiseSIRQ(true);

    m_state.dry = false;
    ScheduleEvent(CDROM_EVENT_SECTOR, StartBuffer(clocks));
}

// P6 = 1 loops the range
// The end is exclusive
void CdRom::CommandCDDAPlay()
{
    u32 start = 0;
    u32 end = 0;

    // Chase HQ loops unless old responses are dropped when playback starts
    ClearStatus();
    InvalidateBuffer();

    if (m_cdrom_media->IsReady() && GetRange(start, end))
        m_cdrom_audio->Play((start >= 150) ? start - 150 : 0, (end >= 150) ? end - 150 : 0,
            m_state.active_params[6] == 0x01);

    if ((m_state.active_command & k_cdrom_flag_status) == 0)
        return;

    if (!PushErrorStatus())
        PushStatus(0x00, GetStatusSecondByte());

    RaiseSIRQ((m_state.active_command & k_cdrom_flag_irq) != 0);
}

// Shadow of the Beast 2 reads the TOC without STATUS and expects the first entry right away
void CdRom::CommandTOCRead()
{
    ClearStatus();

    if ((m_state.active_command & k_cdrom_flag_status) != 0)
    {
        if (PushErrorStatus())
            return;

        PushStatus(0x00, GetStatusSecondByte());
    }

    if (m_cdrom_media->IsReady())
    {
        const std::vector<CdRomImage::Track>& tracks = m_cdrom_media->GetTracks();
        u8 track_count = (u8)tracks.size();
        GT_CdRomMSF msf = m_cdrom_media->GetCdRomLength();

        PushStatus(0x16, 0x00, 0xA0, 0x00);
        PushStatus(0x17, 0x01, 0x00, 0x00);
        PushStatus(0x16, 0x00, 0xA1, 0x00);
        PushStatus(0x17, DecToBcd(track_count), 0x00, 0x00);
        PushStatus(0x16, 0x00, 0xA2, 0x00);
        PushStatus(0x17, DecToBcd(msf.minutes), DecToBcd(msf.seconds), DecToBcd(msf.frames));

        // Data tracks report 40h in the control byte
        for (u8 i = 0; i < track_count; i++)
        {
            u8 control = (tracks[i].type == GT_CDROM_AUDIO_TRACK) ? 0x00 : 0x40;
            LbaToMsf(tracks[i].start_lba + 150, &msf);
            PushStatus(0x16, control, DecToBcd(i + 1), 0x00);
            PushStatus(0x17, DecToBcd(msf.minutes), DecToBcd(msf.seconds), DecToBcd(msf.frames));
        }
    }

    if ((m_state.active_command & k_cdrom_flag_irq) != 0)
        RaiseSIRQ(true);
}

// One snapshot of the CD-DA cursor
// The cursor keeps its place while paused or stopped
void CdRom::CommandSubQRead()
{
    if ((m_state.active_command & k_cdrom_flag_status) == 0)
        return;

    if (!PushErrorStatus())
    {
        const std::vector<CdRomImage::Track>& tracks = m_cdrom_media->GetTracks();
        u32 lba = m_cdrom_audio->GetCurrentLBA();
        size_t track = 0;

        // Pregaps belong to the track that follows them
        for (size_t i = 1; i < tracks.size(); i++)
        {
            u32 first_lba = tracks[i].has_lead_in ? tracks[i].lead_in_lba : tracks[i].start_lba;

            if (lba >= first_lba)
                track = i;
        }

        u32 track_lba = tracks[track].start_lba;
        GT_CdRomMSF relative;
        GT_CdRomMSF absolute;
        LbaToMsf((lba >= track_lba) ? lba - track_lba : track_lba - lba, &relative);
        LbaToMsf(lba + 150, &absolute);

        PushStatus(0x00, 0x00);
        PushStatus(0x18, 0x00, DecToBcd((u8)(track + 1)), 0x00);
        PushStatus(0x19, DecToBcd(relative.minutes), DecToBcd(relative.seconds), DecToBcd(relative.frames));
        PushStatus(0x19, 0x00, DecToBcd(absolute.minutes), DecToBcd(absolute.seconds));
        PushStatus(0x20, DecToBcd(absolute.frames), 0x00, 0x00);
    }

    if ((m_state.active_command & k_cdrom_flag_irq) != 0)
        RaiseSIRQ(true);
}

// Partially identified query
// With P0 = 3 it answers with the disc type seen on an MX
void CdRom::CommandUnknown1F()
{
    if ((m_state.active_command & k_cdrom_flag_status) == 0)
        return;

    if (m_state.active_params[0] == 0x03)
    {
        if (!m_cdrom_media->IsReady())
        {
            PushStatus(0x00, 0x09);
            return;
        }

        if (m_state.disc_changed)
        {
            m_state.disc_changed = false;
            PushStatus(0x21, 0x08);
        }

        const std::vector<CdRomImage::Track>& tracks = m_cdrom_media->GetTracks();
        u8 disc_type = 0x21;

        for (size_t i = 0; i < tracks.size(); i++)
        {
            if (tracks[i].type != GT_CDROM_AUDIO_TRACK)
            {
                disc_type = 0x41;
                break;
            }
        }

        PushStatus(0x00, GetStatusSecondByte());
        PushStatus(0x18, disc_type);
        PushStatus(0x19, 0x00);
        PushStatus(0x19, 0x00);
        PushStatus(0x20, 0x00);
    }
    else if (PushErrorStatus())
        return;
    else
        PushStatus(0x00, GetStatusSecondByte());

    if ((m_state.active_command & k_cdrom_flag_irq) != 0)
        RaiseSIRQ(true);
}

void CdRom::CommandGetState()
{
    if ((m_state.active_command & k_cdrom_flag_status) == 0)
        return;

    if (!m_cdrom_media->IsReady())
        PushStatus(0x00, 0x09);
    else if (m_state.disc_changed)
    {
        m_state.disc_changed = false;
        PushStatus(0x21, 0x08);

        // The Linux 2.0.33 driver takes a media change alone as neither success nor error
        if (m_state.status_count == 4)
            PushStatus(0x00, 0x00);
    }
    else
        PushStatus(0x00, GetStatusSecondByte());

    if ((m_state.active_command & k_cdrom_flag_irq) != 0)
        RaiseSIRQ(true);
}

// The BIOS sends A1h with 07 FF of unknown meaning
// Chase HQ expects no answer to C1h
void CdRom::CommandCDDASet()
{
    if ((m_state.active_command & k_cdrom_flag_status) == 0)
        return;

    if (!PushErrorStatus())
        PushStatus(0x00, GetStatusSecondByte());

    if ((m_state.active_command & k_cdrom_flag_irq) != 0)
        RaiseSIRQ(true);
}

void CdRom::CommandCDDAStop(u64 clocks)
{
    if (m_cdrom_audio->IsPlaying())
        ScheduleEvent(CDROM_EVENT_CDDA_STOP, clocks + k_cdrom_cdda_stop_clocks);
    else
        StopCDDA();
}

// Chase HQ needs the pause to take effect without STATUS
void CdRom::CommandCDDAPause()
{
    m_cdrom_audio->Pause();

    if ((m_state.active_command & k_cdrom_flag_status) == 0)
        return;

    if (!PushErrorStatus())
    {
        PushStatus(0x00, 0x01);
        PushStatus(0x12, 0x00);
    }

    if ((m_state.active_command & k_cdrom_flag_irq) != 0)
        RaiseSIRQ(true);
}

void CdRom::CommandCDDAResume()
{
    m_cdrom_audio->Resume();

    if ((m_state.active_command & k_cdrom_flag_status) == 0)
        return;

    if (!PushErrorStatus())
    {
        PushStatus(0x00, 0x00);
        PushStatus(0x13, 0x00);
    }

    if ((m_state.active_command & k_cdrom_flag_irq) != 0)
        RaiseSIRQ(true);
}

// The Windows 95 driver sends 5F FC 5F FC and expects two packets
// Anything else gets an error
void CdRom::CommandUnknown9F()
{
    const u8* params = m_state.active_params;

    if ((params[1] == 0x5F) && (params[2] == 0xFC) && (params[3] == 0x5F) && (params[4] == 0xFC))
    {
        PushStatus(0x00, 0x00);
        PushStatus(0x1F, 0x5F, 0xFC, 0x01);
    }
    else
        PushStatus(0x21, 0x00);
}

void CdRom::SeekDone()
{
    if ((m_state.active_command & k_cdrom_flag_status) == 0)
        return;

    PushStatus(0x04, 0x00);

    if ((m_state.active_command & k_cdrom_flag_irq) != 0)
        RaiseSIRQ(true);
}

void CdRom::SectorReady(u64 clocks)
{
    if (m_state.read_lba > m_state.read_end_lba)
    {
        ReadDone();
        return;
    }

    // Shadow of the Beast 2 expects Data Ready and DMA end interrupts to alternate
    // The next sector waits while the previous DMA end interrupt is still asserted
    if (m_state.dei && m_state.enable_dei)
    {
        ScheduleEvent(CDROM_EVENT_SECTOR, clocks + k_cdrom_notify_clocks);
        return;
    }

    if (!LoadSector())
        return;

    m_state.dei = false;
    m_state.transfer = CDROM_TRANSFER_READY;
    PushStatus(0x22, 0x00);

    // Shadow of the Beast waits for Data Ready without asking for STATUS
    RaiseSIRQ(((m_state.active_command & k_cdrom_flag_status) != 0) &&
        ((m_state.active_command & k_cdrom_flag_irq) != 0));

    ScheduleEvent(CDROM_EVENT_LOST_DATA, clocks + k_cdrom_lost_data_clocks);
}

// Shadow of the Beast 2 abandons a read by leaving its sector undrained
void CdRom::LostData()
{
    Debug("CDROM: sector %u not transferred in time", m_state.read_lba);

    InvalidateBuffer();
    m_state.transfer = CDROM_TRANSFER_NONE;
    m_state.dry = true;
    m_state.dei = false;
    m_state.sirq = false;
    m_state.sirq_irq = false;
    ClearStatus();
    PushStatus(0x21, 0x0F);

    if (((m_state.active_command & k_cdrom_flag_status) != 0) && ((m_state.active_command & k_cdrom_flag_irq) != 0) &&
        m_state.enable_sirq)
        RaiseSIRQ(true);
}

void CdRom::ReadDone()
{
    m_state.dry = true;
    ClearStatus();
    PushStatus(0x06, 0x00);

    if ((m_state.active_command & k_cdrom_flag_status) != 0)
        RaiseSIRQ((m_state.active_command & k_cdrom_flag_irq) != 0);
    else
    {
        m_state.sirq = false;
        m_state.sirq_irq = false;
    }
}

// The stop answers without looking at STATUS
void CdRom::StopCDDA()
{
    ClearStatus();
    m_cdrom_audio->Stop();

    if (!PushErrorStatus())
    {
        PushStatus(0x00, GetStatusSecondByte());
        PushStatus(0x11, 0x00);
        PushStatus(0x00, 0x0D);
    }

    if ((m_state.active_command & k_cdrom_flag_irq) != 0)
        RaiseSIRQ(true);
}

// Mode 1 hands out the 2048 user bytes, Mode 2 the 2336 after the header and RAW the 2340 after the sync
bool CdRom::LoadSector()
{
    u8 command = m_state.active_command & k_cdrom_command_mask;
    u32 lba = m_state.read_lba;
    u8 error = 0x00;

    if (m_cdrom_media->IsAudioSector(lba))
        error = 0x05;
    else if (command == CDROM_COMMAND_MODE1_READ)
    {
        m_state.sector_position = 0;
        m_state.sector_end = 2048;

        if (!m_cdrom_media->ReadSector(lba, m_state.sector))
            error = 0x04;
    }
    else
    {
        m_state.sector_position = (command == CDROM_COMMAND_RAW_READ) ? 12 : 16;
        m_state.sector_end = CDROM_SECTOR_SIZE;

        if (!m_cdrom_media->ReadRawSector2352(lba, m_state.sector))
            error = 0x04;
    }

    if (error == 0x00)
        return true;

    Debug("CDROM: unable to read sector %u, error %02X", lba, error);

    InvalidateBuffer();
    m_state.dry = true;
    PushStatus(0x21, error);

    if ((m_state.active_command & k_cdrom_flag_status) != 0)
        RaiseSIRQ((m_state.active_command & k_cdrom_flag_irq) != 0);

    return false;
}

// A sector never comes before the pickup reads it
void CdRom::FinishSector()
{
    if (m_state.transfer == CDROM_TRANSFER_DMA)
        m_dma->SetRequest(k_cdrom_dma_channel, false);

    u64 clocks = m_scheduler->GetClocks();
    m_state.transfer = CDROM_TRANSFER_NONE;
    m_state.dei = true;
    m_state.read_lba++;
    ReleaseBuffer(m_state.read_lba, clocks);

    u64 next = clocks + k_cdrom_notify_clocks;

    if (m_state.read_lba <= m_state.read_end_lba)
        next = MAX(next, GetArrivalClocks(m_state.read_lba, clocks));

    // A command received meanwhile ends the read when it runs
    if (m_state.event != CDROM_EVENT_EXECUTE)
        ScheduleEvent(CDROM_EVENT_SECTOR, next);

    UpdateIRQ();
}

// The drive's 8 KiB buffer holds 4 sectors of 2048 bytes or 3 raw ones
// A read already buffered answers right away, one the pickup is about to reach comes when it arrives
// Any other read seeks and starts a new stream
u64 CdRom::StartBuffer(u64 clocks)
{
    bool cooked = (m_state.active_command & k_cdrom_command_mask) == CDROM_COMMAND_MODE1_READ;
    u8 capacity = (u8)(k_cdrom_buffer_size / (cooked ? 2048 : 2340));
    u32 lba = m_state.read_lba;
    UpdateBuffer(clocks);

    if (m_state.buffer_valid && (m_state.buffer_capacity == capacity) && (lba >= m_state.buffer_lba) &&
        (lba <= m_state.prefetch_lba))
    {
        ReleaseBuffer(lba, clocks);
        return MAX(clocks + k_cdrom_notify_clocks, GetArrivalClocks(lba, clocks));
    }

    // Seeks keep the old model, measured from the last sector handed out
    u32 head = m_cdrom_media->GetCurrentSector();
    u64 delay = m_sector_clocks;

    if (lba != head)
        delay = (u64)((((double)m_cdrom_media->SeekTime(head, lba) * GT_CPU_CLOCK_RATE) / 1000.0) * m_seek_scale);

    m_state.buffer_valid = true;
    m_state.buffer_capacity = capacity;
    m_state.buffer_lba = lba;
    m_state.prefetch_lba = lba;
    m_state.prefetch_clocks = clocks + delay;
    return m_state.prefetch_clocks;
}

// The pickup keeps filling the buffer one sector per period and stops when it is full
void CdRom::UpdateBuffer(u64 clocks)
{
    if (!m_state.buffer_valid)
        return;

    u32 count = m_cdrom_media->GetSectorCount();

    while ((m_state.prefetch_clocks <= clocks) && (m_state.prefetch_lba < count) &&
        ((m_state.prefetch_lba - m_state.buffer_lba) < m_state.buffer_capacity))
    {
        m_state.prefetch_lba++;
        m_state.prefetch_clocks += m_sector_clocks;
    }
}

// Sectors before lba leave the buffer
// A full buffer had stopped the pickup, which needs another period to read again
void CdRom::ReleaseBuffer(u32 lba, u64 clocks)
{
    UpdateBuffer(clocks);

    if (!m_state.buffer_valid || (lba <= m_state.buffer_lba))
        return;

    if ((m_state.prefetch_lba - m_state.buffer_lba) >= m_state.buffer_capacity)
        m_state.prefetch_clocks = MAX(m_state.prefetch_clocks, clocks + m_sector_clocks);

    m_state.buffer_lba = lba;
}

u64 CdRom::GetArrivalClocks(u32 lba, u64 clocks) const
{
    if (lba < m_state.prefetch_lba)
        return clocks;

    return m_state.prefetch_clocks + ((u64)(lba - m_state.prefetch_lba) * m_sector_clocks);
}

void CdRom::InvalidateBuffer()
{
    m_state.buffer_valid = false;
}

// Faster drives read at the full multiple but seek only by its square root, as drives of the time did roughly
void CdRom::SetReadSpeed(int speed)
{
    m_read_speed = CLAMP(speed, 1, 8);
    m_sector_clocks = k_cdrom_sector_clocks / (u64)m_read_speed;
    m_seek_scale = 1.0 / sqrt((double)m_read_speed);
    m_cdrom_audio->SetSeekScale(m_seek_scale);
}

int CdRom::GetReadSpeed() const
{
    return m_read_speed;
}

void CdRom::AbortTransfer()
{
    if (m_state.transfer == CDROM_TRANSFER_DMA)
        m_dma->SetRequest(k_cdrom_dma_channel, false);

    m_state.transfer = CDROM_TRANSFER_NONE;
}

// A missing disc is reported as a successful status saying not ready
// A pending media change is reported once
bool CdRom::PushErrorStatus()
{
    if (!m_cdrom_media->IsReady())
    {
        PushStatus(0x00, 0x09);
        return true;
    }

    if (m_state.disc_changed)
    {
        m_state.disc_changed = false;
        PushStatus(0x21, 0x08);
        return true;
    }

    return false;
}

// Packets never wrap because they are four bytes and the queue size is a multiple of four
void CdRom::PushStatus(u8 status0, u8 status1, u8 status2, u8 status3)
{
    if (m_state.status_count + 4 > CDROM_STATUS_QUEUE_SIZE)
    {
        Debug("CDROM: status queue full, dropping %02X %02X", status0, status1);
        return;
    }

    u16 tail = (m_state.status_head + m_state.status_count) & (CDROM_STATUS_QUEUE_SIZE - 1);
    m_state.status[tail + 0] = status0;
    m_state.status[tail + 1] = status1;
    m_state.status[tail + 2] = status2;
    m_state.status[tail + 3] = status3;
    m_state.status_count += 4;

    if (IsValidPointer(m_trace_logger) && m_trace_logger->IsEnabled(TRACE_CDROM))
    {
        GT_Trace_Entry* entry = m_trace_logger->Record(TRACE_CDROM, TRACE_CDROM_STATUS);
        entry->cdrom.command = m_state.active_command;
        memset(entry->cdrom.bytes, 0, sizeof(entry->cdrom.bytes));
        entry->cdrom.bytes[0] = status0;
        entry->cdrom.bytes[1] = status1;
        entry->cdrom.bytes[2] = status2;
        entry->cdrom.bytes[3] = status3;
    }
}

// A packet the CPU already started keeps its remaining bytes
void CdRom::ClearStatus()
{
    m_state.status_count = MIN(m_state.status_count, (u16)((4 - (m_state.status_head & 0x03)) & 0x03));
}

u8 CdRom::GetStatusSecondByte()
{
    if (!m_cdrom_media->IsReady())
        return 0x09;

    if (m_cdrom_audio->IsPaused())
        return 0x01;

    if (m_cdrom_audio->IsPlaying())
        return 0x03;

    return 0x00;
}

// Absolute frames of the BCD start in P0-P2 and the BCD end in P3-P5
bool CdRom::GetRange(u32& start, u32& end) const
{
    const u8* params = m_state.active_params;

    for (int i = 0; i < 6; i++)
    {
        if (!IsValidBcd(params[i]))
            return false;
    }

    GT_CdRomMSF start_msf = { BcdToDec(params[0]), BcdToDec(params[1]), BcdToDec(params[2]) };
    GT_CdRomMSF end_msf = { BcdToDec(params[3]), BcdToDec(params[4]), BcdToDec(params[5]) };

    if ((start_msf.seconds >= 60) || (start_msf.frames >= 75) || (end_msf.seconds >= 60) || (end_msf.frames >= 75))
        return false;

    start = MsfToLba(&start_msf);
    end = MsfToLba(&end_msf);
    return true;
}

// SIRQ only latches with a packet to read
// Once eligible for the IRQ line it stays so until acknowledged
void CdRom::RaiseSIRQ(bool irq)
{
    if (m_state.status_count == 0)
        return;

    m_state.sirq_irq = (m_state.sirq && m_state.sirq_irq) || irq;
    m_state.sirq = true;
    UpdateIRQ();
}

void CdRom::UpdateIRQ()
{
    bool status_irq = m_state.sirq && m_state.sirq_irq && m_state.enable_sirq;
    bool dma_irq = m_state.dei && m_state.enable_dei;
    m_pic->SetIRQLine(k_cdrom_irq, status_irq || dma_irq);
}

void CdRom::ScheduleEvent(CdRom_Event event, u64 clocks)
{
    m_state.event = event;
    m_state.event_clocks = clocks;
    UpdateNextEvent();
}

void CdRom::UpdateNextEvent()
{
    u64 next = (m_state.event != CDROM_EVENT_NONE) ? m_state.event_clocks : GT_NO_EVENT;
    m_scheduler->Schedule(SCHEDULER_EVENT_CDROM, next);
}

void CdRom::SaveState(std::ostream& stream)
{
    m_state.head_lba = m_cdrom_media->GetCurrentSector();
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void CdRom::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
    m_cdrom_media->SetCurrentSector(m_state.head_lba);
}

void CdRom::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_state.command);
    G_SERIALIZE(serializer, m_state.command_received);
    G_SERIALIZE_ARRAY(serializer, m_state.params, CDROM_PARAM_COUNT);
    G_SERIALIZE(serializer, m_state.param_count);
    G_SERIALIZE(serializer, m_state.active_command);
    G_SERIALIZE_ARRAY(serializer, m_state.active_params, CDROM_PARAM_COUNT);
    G_SERIALIZE_ARRAY(serializer, m_state.status, CDROM_STATUS_QUEUE_SIZE);
    G_SERIALIZE(serializer, m_state.status_head);
    G_SERIALIZE(serializer, m_state.status_count);
    G_SERIALIZE(serializer, m_state.sirq);
    G_SERIALIZE(serializer, m_state.sirq_irq);
    G_SERIALIZE(serializer, m_state.dei);
    G_SERIALIZE(serializer, m_state.enable_sirq);
    G_SERIALIZE(serializer, m_state.enable_dei);
    G_SERIALIZE(serializer, m_state.dry);
    G_SERIALIZE(serializer, m_state.disc_changed);
    G_SERIALIZE(serializer, m_state.event);
    G_SERIALIZE(serializer, m_state.event_clocks);
    G_SERIALIZE(serializer, m_state.transfer);
    G_SERIALIZE(serializer, m_state.read_lba);
    G_SERIALIZE(serializer, m_state.read_end_lba);
    G_SERIALIZE(serializer, m_state.head_lba);
    G_SERIALIZE(serializer, m_state.buffer_valid);
    G_SERIALIZE(serializer, m_state.buffer_capacity);
    G_SERIALIZE(serializer, m_state.buffer_lba);
    G_SERIALIZE(serializer, m_state.prefetch_lba);
    G_SERIALIZE(serializer, m_state.prefetch_clocks);
    G_SERIALIZE(serializer, m_state.sector_position);
    G_SERIALIZE(serializer, m_state.sector_end);
    G_SERIALIZE_ARRAY(serializer, m_state.sector, CDROM_SECTOR_SIZE);
}

void CdRom::SanitizeState()
{
    m_state.param_count = MIN(m_state.param_count, CDROM_PARAM_COUNT);
    m_state.status_head &= CDROM_STATUS_QUEUE_SIZE - 1;
    m_state.status_count = MIN(m_state.status_count, CDROM_STATUS_QUEUE_SIZE);

    if (((m_state.status_head + m_state.status_count) & 0x03) != 0)
    {
        m_state.status_head = 0;
        m_state.status_count = 0;
    }

    m_state.sector_end = MIN(m_state.sector_end, CDROM_SECTOR_SIZE);
    m_state.sector_position = MIN(m_state.sector_position, m_state.sector_end);

    if (m_state.event > CDROM_EVENT_CDDA_STOP)
        m_state.event = CDROM_EVENT_NONE;

    if (m_state.transfer > CDROM_TRANSFER_CPU)
        m_state.transfer = CDROM_TRANSFER_NONE;

    if ((m_state.buffer_capacity == 0) || (m_state.buffer_capacity > (k_cdrom_buffer_size / 2048)) ||
        (m_state.prefetch_lba < m_state.buffer_lba) ||
        ((m_state.prefetch_lba - m_state.buffer_lba) > m_state.buffer_capacity))
        m_state.buffer_valid = false;

    UpdateIRQ();
    UpdateNextEvent();
}
