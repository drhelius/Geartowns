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

#include "scsi_disk.h"
#include "../common/trace_logger.h"
#include "../common/state_serializer.h"

static u32 ReadBig32(const u8* bytes)
{
    return ((u32)bytes[0] << 24) | ((u32)bytes[1] << 16) | ((u32)bytes[2] << 8) | bytes[3];
}

static void WriteBig32(u8* bytes, u32 value)
{
    bytes[0] = (u8)(value >> 24);
    bytes[1] = (u8)(value >> 16);
    bytes[2] = (u8)(value >> 8);
    bytes[3] = (u8)value;
}

ScsiDisk::ScsiDisk()
{
    InitPointer(m_trace_logger);
    memset(&m_state, 0, sizeof(m_state));
    m_id = 0;
}

ScsiDisk::~ScsiDisk()
{
}

void ScsiDisk::Init(int id)
{
    m_id = id;
    Reset();
}

void ScsiDisk::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

void ScsiDisk::Reset()
{
    memset(&m_state, 0, sizeof(m_state));
}

bool ScsiDisk::Attach(const char* path, bool write_protected)
{
    Reset();
    return m_image.Open(path, write_protected);
}

void ScsiDisk::Detach()
{
    m_image.Close();
    Reset();
}

bool ScsiDisk::IsAttached() const
{
    return m_image.IsOpen();
}

HardDiskImage* ScsiDisk::GetImage()
{
    return &m_image;
}

Scsi_Data ScsiDisk::Execute(const u8* command)
{
    m_state.command = command[0];
    m_state.status = k_scsi_good;
    m_state.lba = 0;
    m_state.length = 0;
    m_state.position = 0;
    m_state.buffer_size = 0;
    m_state.buffer_position = 0;

    if (command[0] != 0x03)
        ClearSense();

    switch (command[0])
    {
        case 0x00:
            // TEST UNIT READY
        case 0x01:
            // REZERO UNIT
        case 0x04:
            // FORMAT UNIT
        case 0x16:
            // RESERVE
        case 0x17:
            // RELEASE
        case 0x1B:
            // START STOP UNIT
        case 0x1E:
            // PREVENT ALLOW MEDIUM REMOVAL
            return SCSI_DATA_NONE;
        case 0x03:
            return RequestSense(command);
        case 0x08:
            // READ(6)
        case 0x0A:
            // WRITE(6)
        {
            u32 lba = ((u32)(command[1] & 0x1F) << 16) | ((u32)command[2] << 8) | command[3];
            return StartTransfer(lba, command[4] == 0 ? 256 : command[4], command[0] == 0x0A);
        }
        case 0x0B:
            // SEEK(6)
            if (!InRange(((u32)(command[1] & 0x1F) << 16) | ((u32)command[2] << 8) | command[3], 1))
                SetSense(k_scsi_sense_illegal_request, k_scsi_asc_lba_out_of_range);
            return SCSI_DATA_NONE;
        case 0x12:
            return Inquiry(command);
        case 0x15:
            // MODE SELECT(6), the parameters are taken and dropped
            m_state.length = command[4];
            m_state.buffer_size = (u16)MIN(m_state.length, (u32)SCSI_DISK_BUFFER_SIZE);
            return m_state.length != 0 ? SCSI_DATA_OUT : SCSI_DATA_NONE;
        case 0x1A:
            return ModeSense(command);
        case 0x1D:
            // SEND DIAGNOSTIC, the parameters are taken and dropped
            m_state.length = ((u32)command[3] << 8) | command[4];
            m_state.buffer_size = (u16)MIN(m_state.length, (u32)SCSI_DISK_BUFFER_SIZE);
            return m_state.length != 0 ? SCSI_DATA_OUT : SCSI_DATA_NONE;
        case 0x25:
            return ReadCapacity();
        case 0x28:
            // READ(10)
        case 0x2A:
            // WRITE(10)
            return StartTransfer(ReadBig32(command + 2), ((u32)command[7] << 8) | command[8], command[0] == 0x2A);
        case 0x2B:
            // SEEK(10)
            if (!InRange(ReadBig32(command + 2), 1))
                SetSense(k_scsi_sense_illegal_request, k_scsi_asc_lba_out_of_range);
            return SCSI_DATA_NONE;
        case 0x2F:
            return Verify(command);
        default:
            SetSense(k_scsi_sense_illegal_request, k_scsi_asc_invalid_command);
            return SCSI_DATA_NONE;
    }
}

void ScsiDisk::RejectLUN()
{
    m_state.length = 0;
    m_state.position = 0;
    SetSense(k_scsi_sense_illegal_request, k_scsi_asc_lun_not_supported);
}

bool ScsiDisk::ReadByte(u8& value)
{
    if (m_state.position >= m_state.length)
        return false;

    if ((m_state.buffer_position >= m_state.buffer_size) && !LoadBlock())
        return false;

    value = m_state.buffer[m_state.buffer_position++];
    m_state.position++;
    return true;
}

bool ScsiDisk::WriteByte(u8 value)
{
    if (m_state.position >= m_state.length)
        return false;

    m_state.buffer[m_state.buffer_position++] = value;
    m_state.position++;

    if (m_state.buffer_position < m_state.buffer_size)
        return true;

    bool ok = StoreBlock();
    m_state.buffer_position = 0;
    m_state.buffer_size = (u16)MIN((u32)SCSI_DISK_BUFFER_SIZE, m_state.length - m_state.position);
    return ok;
}

bool ScsiDisk::IsDataDone() const
{
    return m_state.position >= m_state.length;
}

u8 ScsiDisk::GetStatus() const
{
    return m_state.status;
}

ScsiDisk::ScsiDisk_State* ScsiDisk::GetState()
{
    return &m_state;
}

Scsi_Data ScsiDisk::StartTransfer(u32 lba, u32 blocks, bool write)
{
    if (blocks == 0)
        return SCSI_DATA_NONE;

    if (!InRange(lba, blocks))
    {
        SetSense(k_scsi_sense_illegal_request, k_scsi_asc_lba_out_of_range, lba, true);
        return SCSI_DATA_NONE;
    }

    if (write && m_image.IsWriteProtected())
    {
        SetSense(k_scsi_sense_data_protect, k_scsi_asc_write_protected);
        return SCSI_DATA_NONE;
    }

    m_state.lba = lba;
    m_state.length = blocks * k_hard_disk_block_size;
    m_state.buffer_size = write ? (u16)SCSI_DISK_BUFFER_SIZE : 0;
    return write ? SCSI_DATA_OUT : SCSI_DATA_IN;
}

Scsi_Data ScsiDisk::StartResponse(u32 size, u32 allocation)
{
    m_state.length = MIN(size, allocation);
    m_state.buffer_size = (u16)m_state.length;
    return m_state.length != 0 ? SCSI_DATA_IN : SCSI_DATA_NONE;
}

Scsi_Data ScsiDisk::RequestSense(const u8* command)
{
    u8* data = m_state.buffer;
    memset(data, 0, 18);
    data[0] = (u8)(0x70 | (m_state.sense_valid ? 0x80 : 0x00));
    data[2] = m_state.sense_key;
    WriteBig32(data + 3, m_state.sense_information);
    data[7] = 10;
    data[12] = m_state.sense_code;
    ClearSense();

    // In CCS an allocation length of 0 asks for the first 4 bytes
    return StartResponse(18, command[4] == 0 ? 4 : command[4]);
}

Scsi_Data ScsiDisk::Inquiry(const u8* command)
{
    if ((command[1] & 0x01) != 0)
    {
        SetSense(k_scsi_sense_illegal_request, k_scsi_asc_invalid_field);
        return SCSI_DATA_NONE;
    }

    u8* data = m_state.buffer;
    memset(data, 0, 36);
    data[2] = 0x01;
    data[3] = 0x01;
    data[4] = 31;
    memcpy(data + 8, "GEARTOWN", 8);
    memcpy(data + 16, "HARD DISK       ", 16);
    memcpy(data + 32, "1.00", 4);
    return StartResponse(36, command[4]);
}

Scsi_Data ScsiDisk::ModeSense(const u8* command)
{
    u8 page = command[2] & 0x3F;

    if ((page != 0x00) && (page != 0x3F))
    {
        SetSense(k_scsi_sense_illegal_request, k_scsi_asc_invalid_field);
        return SCSI_DATA_NONE;
    }

    bool descriptor = (command[1] & 0x08) == 0;
    u32 size = descriptor ? 12 : 4;
    u8* data = m_state.buffer;
    memset(data, 0, size);
    data[0] = (u8)(size - 1);
    data[2] = m_image.IsWriteProtected() ? 0x80 : 0x00;

    if (descriptor)
    {
        u32 blocks = MIN(m_image.GetBlocks(), (u32)0xFFFFFF);
        data[3] = 8;
        data[5] = (u8)(blocks >> 16);
        data[6] = (u8)(blocks >> 8);
        data[7] = (u8)blocks;
        data[10] = (u8)(k_hard_disk_block_size >> 8);
    }

    return StartResponse(size, command[4]);
}

Scsi_Data ScsiDisk::ReadCapacity()
{
    WriteBig32(m_state.buffer, m_image.GetBlocks() - 1);
    WriteBig32(m_state.buffer + 4, k_hard_disk_block_size);
    return StartResponse(8, 8);
}

Scsi_Data ScsiDisk::Verify(const u8* command)
{
    u32 lba = ReadBig32(command + 2);
    u32 blocks = ((u32)command[7] << 8) | command[8];

    if (blocks == 0)
        return SCSI_DATA_NONE;

    if (!InRange(lba, blocks))
    {
        SetSense(k_scsi_sense_illegal_request, k_scsi_asc_lba_out_of_range, lba, true);
        return SCSI_DATA_NONE;
    }

    if ((command[1] & 0x02) == 0)
        return SCSI_DATA_NONE;

    m_state.lba = lba;
    m_state.length = blocks * k_hard_disk_block_size;
    m_state.buffer_size = (u16)SCSI_DISK_BUFFER_SIZE;
    return SCSI_DATA_OUT;
}

bool ScsiDisk::LoadBlock()
{
    if (!m_image.ReadBlock(m_state.lba, m_state.buffer))
    {
        SetSense(k_scsi_sense_medium_error, k_scsi_asc_read_error, m_state.lba, true);
        m_state.length = m_state.position;
        return false;
    }

    m_state.lba++;
    m_state.buffer_size = (u16)k_hard_disk_block_size;
    m_state.buffer_position = 0;
    return true;
}

bool ScsiDisk::StoreBlock()
{
    switch (m_state.command)
    {
        case 0x0A:
        case 0x2A:
            if (m_image.WriteBlock(m_state.lba, m_state.buffer))
            {
                m_state.lba++;
                return true;
            }

            SetSense(k_scsi_sense_medium_error, k_scsi_asc_read_error, m_state.lba, true);
            break;
        case 0x2F:
        {
            u8 block[SCSI_DISK_BUFFER_SIZE];

            if (!m_image.ReadBlock(m_state.lba, block))
                SetSense(k_scsi_sense_medium_error, k_scsi_asc_read_error, m_state.lba, true);
            else if (memcmp(block, m_state.buffer, k_hard_disk_block_size) != 0)
                SetSense(k_scsi_sense_miscompare, k_scsi_asc_miscompare, m_state.lba, true);
            else
            {
                m_state.lba++;
                return true;
            }
            break;
        }
        default:
            return true;
    }

    m_state.length = m_state.position;
    return false;
}

bool ScsiDisk::InRange(u32 lba, u32 blocks) const
{
    u32 capacity = m_image.GetBlocks();
    return (lba < capacity) && (blocks <= capacity - lba);
}

void ScsiDisk::SetSense(u8 key, u8 code, u32 information, bool valid)
{
    m_state.status = k_scsi_check_condition;
    m_state.sense_key = key;
    m_state.sense_code = code;
    m_state.sense_information = information;
    m_state.sense_valid = valid;

    if (!IsValidPointer(m_trace_logger) || !m_trace_logger->IsEventEnabled(TRACE_SCSI, TRACE_SCSI_ERROR))
        return;

    GT_Trace_Entry entry = {};
    entry.type = TRACE_SCSI;
    entry.event = TRACE_SCSI_ERROR;
    entry.scsi.target = (s8)m_id;
    entry.scsi.command[0] = m_state.command;
    entry.scsi.key = key;
    entry.scsi.code = code;
    entry.scsi.lba = information;
    entry.scsi.value = valid ? 1 : 0;
    m_trace_logger->TraceLog(entry);
}

void ScsiDisk::ClearSense()
{
    m_state.sense_key = k_scsi_sense_none;
    m_state.sense_code = 0;
    m_state.sense_information = 0;
    m_state.sense_valid = false;
}

void ScsiDisk::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer, GT_SAVESTATE_VERSION);
}

void ScsiDisk::LoadState(std::istream& stream, int version)
{
    StateSerializer serializer(stream);
    Serialize(serializer, version);
    SanitizeState();
}

void ScsiDisk::Serialize(StateSerializer& serializer, int version)
{
    if (version < 15)
    {
        memset(&m_state, 0, sizeof(m_state));
        return;
    }

    G_SERIALIZE_ARRAY(serializer, m_state.buffer, SCSI_DISK_BUFFER_SIZE);
    G_SERIALIZE(serializer, m_state.buffer_size);
    G_SERIALIZE(serializer, m_state.buffer_position);
    G_SERIALIZE(serializer, m_state.command);
    G_SERIALIZE(serializer, m_state.lba);
    G_SERIALIZE(serializer, m_state.length);
    G_SERIALIZE(serializer, m_state.position);
    G_SERIALIZE(serializer, m_state.status);
    G_SERIALIZE(serializer, m_state.sense_key);
    G_SERIALIZE(serializer, m_state.sense_code);
    G_SERIALIZE(serializer, m_state.sense_valid);
    G_SERIALIZE(serializer, m_state.sense_information);
}

void ScsiDisk::SanitizeState()
{
    m_state.buffer_size = (u16)MIN((u32)m_state.buffer_size, (u32)SCSI_DISK_BUFFER_SIZE);
    m_state.buffer_position = (u16)MIN(m_state.buffer_position, m_state.buffer_size);
    m_state.position = MIN(m_state.position, m_state.length);
}
