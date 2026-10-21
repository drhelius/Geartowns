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

#ifndef SCSI_DISK_H
#define SCSI_DISK_H

#include <iostream>
#include "../common/common.h"
#include "hard_disk_image.h"

#define SCSI_DISK_BUFFER_SIZE 512

class StateSerializer;
class TraceLogger;

enum Scsi_Data
{
    SCSI_DATA_NONE = 0,
    SCSI_DATA_IN,
    SCSI_DATA_OUT
};

class ScsiDisk
{
public:
    struct ScsiDisk_State
    {
        u8 buffer[SCSI_DISK_BUFFER_SIZE];
        u16 buffer_size;
        u16 buffer_position;
        u8 command;
        u32 lba;
        u32 length;
        u32 position;
        u8 status;
        u8 sense_key;
        u8 sense_code;
        bool sense_valid;
        u32 sense_information;
    };

public:
    ScsiDisk();
    ~ScsiDisk();
    void Init(int id);
    void SetTraceLogger(TraceLogger* trace_logger);
    void Reset();
    bool Attach(const char* path, bool write_protected, const char* working_path = NULL);
    void Detach();
    bool IsAttached() const;
    HardDiskImage* GetImage();
    Scsi_Data Execute(const u8* command);
    void RejectLUN();
    bool ReadByte(u8& value);
    bool WriteByte(u8 value);
    bool IsDataDone() const;
    u8 GetStatus() const;
    ScsiDisk_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream, int version = GT_SAVESTATE_VERSION);

private:
    Scsi_Data StartTransfer(u32 lba, u32 blocks, bool write);
    Scsi_Data StartResponse(u32 size, u32 allocation);
    Scsi_Data RequestSense(const u8* command);
    Scsi_Data Inquiry(const u8* command);
    Scsi_Data ModeSense(const u8* command);
    Scsi_Data ReadCapacity();
    Scsi_Data Verify(const u8* command);
    bool LoadBlock();
    bool StoreBlock();
    bool InRange(u32 lba, u32 blocks) const;
    void SetSense(u8 key, u8 code, u32 information = 0, bool valid = false);
    void ClearSense();
    void Serialize(StateSerializer& serializer, int version);
    void SanitizeState();

private:
    HardDiskImage m_image;
    TraceLogger* m_trace_logger;
    ScsiDisk_State m_state;
    int m_id;
};

static const u8 k_scsi_good = 0x00;
static const u8 k_scsi_check_condition = 0x02;

static const u8 k_scsi_sense_none = 0x00;
static const u8 k_scsi_sense_medium_error = 0x03;
static const u8 k_scsi_sense_illegal_request = 0x05;
static const u8 k_scsi_sense_data_protect = 0x07;
static const u8 k_scsi_sense_miscompare = 0x0E;

static const u8 k_scsi_asc_read_error = 0x11;
static const u8 k_scsi_asc_miscompare = 0x1D;
static const u8 k_scsi_asc_invalid_command = 0x20;
static const u8 k_scsi_asc_lba_out_of_range = 0x21;
static const u8 k_scsi_asc_invalid_field = 0x24;
static const u8 k_scsi_asc_lun_not_supported = 0x25;
static const u8 k_scsi_asc_write_protected = 0x27;

#endif /* SCSI_DISK_H */
