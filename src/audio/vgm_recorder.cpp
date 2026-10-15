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

#include "vgm_recorder.h"

static const int k_vgm_header_size = 0x100;
static const u32 k_vgm_version = 0x161;
static const u32 k_vgm_rate = 60;

VgmRecorder::VgmRecorder()
{
    m_recording = false;
    m_pending_wait = 0;
    m_total_samples = 0;
    m_clock_rate = 0;
    m_memory_block_address = 0;
    m_memory_block_size = 0;
}

VgmRecorder::~VgmRecorder()
{
    if (m_recording)
        Stop();
}

void VgmRecorder::Start(const char* file_path, int clock_rate, const GT_VGM_Metadata& metadata)
{
    if (m_recording)
        return;

    open_ofstream_utf8(m_file, file_path, std::ios::out | std::ios::binary | std::ios::trunc);

    if (!m_file.is_open())
    {
        Error("Unable to open VGM file %s", file_path);
        return;
    }

    m_metadata = metadata;
    m_clock_rate = clock_rate;
    m_recording = true;
    m_pending_wait = 0;
    m_total_samples = 0;
    m_memory_block_address = 0;
    m_memory_block_size = 0;
    m_command_buffer.clear();
}

void VgmRecorder::Stop()
{
    if (!m_recording)
        return;

    FlushPendingWait();
    WriteCommand(0x66);

    std::vector<u8> gd3_tag;
    BuildGD3Tag(gd3_tag, m_metadata);

    u8 header[k_vgm_header_size];
    memset(header, 0, sizeof(header));

    u32 gd3_offset = k_vgm_header_size + (u32)m_command_buffer.size();
    u32 eof_offset = gd3_offset + (u32)gd3_tag.size() - 4;

    header[0x00] = 'V';
    header[0x01] = 'g';
    header[0x02] = 'm';
    header[0x03] = ' ';
    write_u32_le(header + 0x04, eof_offset);
    write_u32_le(header + 0x08, k_vgm_version);
    write_u32_le(header + 0x14, gd3_offset - 0x14);
    write_u32_le(header + 0x18, (u32)m_total_samples);
    write_u32_le(header + 0x24, k_vgm_rate);
    write_u32_le(header + 0x2C, (u32)m_clock_rate);
    write_u32_le(header + 0x34, k_vgm_header_size - 0x34);
    write_u32_le(header + 0x40, (u32)m_clock_rate);

    m_file.write((const char*)header, sizeof(header));
    m_file.write((const char*)&m_command_buffer[0], m_command_buffer.size());
    m_file.write((const char*)&gd3_tag[0], gd3_tag.size());
    m_file.close();

    m_recording = false;
    m_command_buffer.clear();
}

void VgmRecorder::WriteYM3438(u16 address, u8 data)
{
    if (!m_recording)
        return;

    FlushPendingWait();
    WriteCommand((u8)(0x52 + ((address >> 8) & 0x01)), (u8)address, data);
}

void VgmRecorder::WriteRF5C68(u8 reg, u8 data)
{
    if (!m_recording)
        return;

    FlushPendingWait();
    WriteCommand(0xB0, reg, data);
}

// Wave RAM writes go in RAM data blocks at the address within the selected bank
// Writes that follow each other within the same sample extend the open block
void VgmRecorder::WriteRF5C68Memory(u16 address, const u8* data, u32 size)
{
    if (!m_recording)
        return;

    FlushPendingWait();

    if ((m_memory_block_size == 0) || (address != m_memory_block_address + m_memory_block_size))
    {
        WriteCommand(0x67);
        m_command_buffer.push_back(0x66);
        m_command_buffer.push_back(0xC0);
        AppendUint32(m_command_buffer, 0);
        m_command_buffer.push_back((u8)address);
        m_command_buffer.push_back((u8)(address >> 8));
        m_memory_block_address = address;
    }

    m_command_buffer.insert(m_command_buffer.end(), data, data + size);
    m_memory_block_size += size;
    write_u32_le(&m_command_buffer[m_command_buffer.size() - m_memory_block_size - 6], m_memory_block_size + 2);
}

void VgmRecorder::WriteCommand(u8 command)
{
    m_command_buffer.push_back(command);
    m_memory_block_size = 0;
}

void VgmRecorder::WriteCommand(u8 command, u8 data1, u8 data2)
{
    WriteCommand(command);
    m_command_buffer.push_back(data1);
    m_command_buffer.push_back(data2);
}

void VgmRecorder::WriteWait(int samples)
{
    while (samples > 0)
    {
        if (samples == 735)
        {
            WriteCommand(0x62);
            samples -= 735;
        }
        else if (samples == 882)
        {
            WriteCommand(0x63);
            samples -= 882;
        }
        else if (samples <= 16)
        {
            WriteCommand((u8)(0x70 + (samples - 1)));
            samples = 0;
        }
        else if (samples <= 65535)
        {
            WriteCommand(0x61);
            m_command_buffer.push_back((u8)samples);
            m_command_buffer.push_back((u8)(samples >> 8));
            samples = 0;
        }
        else
        {
            WriteCommand(0x61);
            m_command_buffer.push_back(0xFF);
            m_command_buffer.push_back(0xFF);
            samples -= 65535;
        }
    }
}

void VgmRecorder::FlushPendingWait()
{
    if (m_pending_wait > 0)
    {
        WriteWait(m_pending_wait);
        m_pending_wait = 0;
    }
}

void VgmRecorder::AppendUint32(std::vector<u8>& buffer, u32 value)
{
    u8 data[4];
    write_u32_le(data, value);
    buffer.insert(buffer.end(), data, data + 4);
}

void VgmRecorder::AppendGD3Codepoint(std::vector<u8>& buffer, u32 codepoint)
{
    u8 data[2];

    if (codepoint <= 0xFFFF)
    {
        write_u16_le(data, (u16)codepoint);
        buffer.insert(buffer.end(), data, data + 2);
    }
    else
    {
        codepoint -= 0x10000;
        write_u16_le(data, (u16)(0xD800 + (codepoint >> 10)));
        buffer.insert(buffer.end(), data, data + 2);
        write_u16_le(data, (u16)(0xDC00 + (codepoint & 0x3FF)));
        buffer.insert(buffer.end(), data, data + 2);
    }
}

void VgmRecorder::AppendGD3String(std::vector<u8>& buffer, const char* value)
{
    size_t length = strlen(value);
    size_t index = 0;

    while (index < length)
        AppendGD3Codepoint(buffer, utf8_decode_next(value, length, index));

    AppendGD3Codepoint(buffer, 0);
}

void VgmRecorder::BuildGD3Tag(std::vector<u8>& tag, const GT_VGM_Metadata& metadata)
{
    std::vector<u8> data;

    for (int i = 0; i < 11; i++)
    {
        const char* value = "";

        if (i == 2)
            value = metadata.game_name.c_str();
        else if (i == 4)
            value = metadata.system_name.c_str();
        else if (i == 10)
            value = metadata.comment.c_str();

        AppendGD3String(data, value);
    }

    tag.push_back('G');
    tag.push_back('d');
    tag.push_back('3');
    tag.push_back(' ');
    AppendUint32(tag, 0x00000100);
    AppendUint32(tag, (u32)data.size());
    tag.insert(tag.end(), data.begin(), data.end());
}
