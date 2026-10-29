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

#include "pcm16.h"
#include "../system/memory.h"
#include "../system/pic.h"
#include "../common/trace_logger.h"
#include "../common/state_serializer.h"

PCM16::PCM16()
{
    InitPointer(m_pic);
    InitPointer(m_memory);
    InitPointer(m_trace_logger);
    m_fitted = false;
    m_irq_line = -1;
    m_irq_asserted = false;
    ResetRegisters(0);
}

PCM16::~PCM16()
{
}

void PCM16::Init(PIC* pic, Memory* memory)
{
    m_pic = pic;
    m_memory = memory;
}

void PCM16::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

void PCM16::SetFitted(bool fitted)
{
    m_fitted = fitted;
    UpdateIRQ();
}

void PCM16::Reset(u64 clocks)
{
    ResetRegisters(clocks);
    m_state.reset_clocks = 0;
    UpdateIRQ();
}

void PCM16::ResetRegisters(u64 clocks)
{
    m_state.base_address = 0;
    m_state.current_address = 0;
    m_state.base_count = 0;
    m_state.current_count = 0;
    memset(m_state.buffer, 0, sizeof(m_state.buffer));
    m_state.buffer_head = 0;
    m_state.buffer_count = 0;
    m_state.bank = 0;
    m_state.dma_control = 0;
    m_state.terminal_count = false;
    m_state.clock = k_pcm16_reset_clock;
    m_state.mode = k_pcm16_reset_mode;
    m_state.system_control = k_pcm16_reset_system;
    m_state.buffer_control = 0;
    m_state.overrun = false;
    m_state.underrun = false;
    m_state.transfer_control = k_pcm16_reset_transfer;
    m_state.input_flag = false;
    m_state.peak = 0;
    m_state.threshold = 0;
    m_state.level_left = 0;
    m_state.level_right = 0;
    m_state.data_low = 0;
    m_state.data_high = 0;
    m_state.word = 0;
    m_state.high_byte = false;
    m_state.phase = 0;
    m_state.reset_clocks = clocks + k_pcm16_reset_clocks;
    m_state.ready_clocks = clocks + k_pcm16_ready_clocks;
    m_state.input_left = 0;
    m_state.input_right = 0;
    m_state.left_sample = 0;
    m_state.right_sample = 0;
    m_state.previous_left_sample = 0;
    m_state.previous_right_sample = 0;
}

u8 PCM16::Read(u16 port, u64 clocks)
{
    if (((port & 0x0F) != 0x0E) || !IsSoftwareTransfer() || !IsRecording())
        return Peek(port, clocks);

    u16 word = 0;

    if (m_state.buffer_count > 0)
    {
        word = m_state.buffer[m_state.buffer_head];
        m_state.buffer_head = (u8)((m_state.buffer_head + 1) % PCM16_BUFFER_SIZE);
        m_state.buffer_count--;
    }

    m_state.data_high = (u8)(word >> 8);
    TraceData(word, true);
    return (u8)word;
}

u8 PCM16::Peek(u16 port, u64 clocks) const
{
    bool base = m_state.bank != 0;
    u16 count = base ? m_state.base_count : m_state.current_count;
    u32 address = base ? m_state.base_address : m_state.current_address;

    switch (port & 0x0F)
    {
        // Undefined bits read 0 and the presence bit 7 is active low
        case 0x00:
            return (u8)(GetIRQCauses() | m_state.bank);
        case 0x01:
            return (u8)(m_state.dma_control | (m_state.terminal_count ? 0x01 : 0x00));
        case 0x02:
            return (u8)count;
        case 0x03:
            return (u8)(count >> 8);
        case 0x04:
            return (u8)address;
        case 0x05:
            return (u8)(address >> 8);
        case 0x06:
            return (u8)(address >> 16);
        case 0x07:
            return (u8)(address >> 24);
        case 0x08:
            return m_state.clock;
        case 0x09:
            return m_state.mode;
        case 0x0A:
            return (u8)((clocks < m_state.reset_clocks ? k_pcm16_system_reset : 0x00) | m_state.system_control);
        case 0x0B:
            return (u8)(m_state.buffer_control | (m_state.overrun ? 0x20 : 0x00) | (m_state.underrun ? 0x10 : 0x00) |
                m_state.buffer_count);
        case 0x0C:
            return (u8)((clocks < m_state.ready_clocks ? 0xC0 : 0x00) | m_state.transfer_control |
                (m_state.input_flag ? k_pcm16_transfer_input_flag : 0x00));
        case 0x0D:
            return m_state.peak;
        case 0x0E:
            if (!IsSoftwareTransfer())
                return m_state.level_left;

            return (IsRecording() && (m_state.buffer_count > 0)) ? (u8)m_state.buffer[m_state.buffer_head] : 0xFF;
        default:
            if (!IsSoftwareTransfer())
                return m_state.level_right;

            return IsRecording() ? m_state.data_high : 0xFF;
    }
}

void PCM16::Write(u16 port, u8 value, u64 clocks)
{
    int index = port & 0x0F;

    if (index < 0x0E)
        TraceRegister(port, value);

    switch (index)
    {
        case 0x00:
            m_state.bank = value & 0x01;
            break;
        case 0x01:
            if ((value & 0x01) != 0)
                m_state.terminal_count = false;

            m_state.dma_control = value & (k_pcm16_dma_autoinit | k_pcm16_dma_irq);
            break;
        case 0x02:
        case 0x03:
        {
            int shift = (index - 0x02) * 8;
            u16 mask = (u16)(0xFF << shift);
            m_state.base_count = (u16)((m_state.base_count & ~mask) | (value << shift));

            if (m_state.bank == 0)
                m_state.current_count = (u16)((m_state.current_count & ~mask) | (value << shift));
            break;
        }
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        {
            int shift = (index - 0x04) * 8;
            u32 mask = 0xFFU << shift;
            m_state.base_address = ((m_state.base_address & ~mask) | ((u32)value << shift)) & 0xFFFFFFFE;

            if (m_state.bank == 0)
                m_state.current_address = ((m_state.current_address & ~mask) | ((u32)value << shift)) & 0xFFFFFFFE;
            break;
        }
        case 0x08:
            if ((value & 0x0F) != m_state.clock)
            {
                m_state.clock = value & 0x0F;
                m_state.ready_clocks = clocks + k_pcm16_ready_clocks;
            }
            break;
        case 0x09:
            m_state.mode = value & 0x3F;
            break;
        case 0x0A:
            if ((value & k_pcm16_system_reset) != 0)
                ResetRegisters(clocks);
            else
                m_state.system_control = value & (k_pcm16_system_data_port | k_pcm16_system_record |
                    k_pcm16_system_irq);
            break;
        case 0x0B:
            m_state.buffer_control = value & k_pcm16_buffer_irq;

            if ((value & 0x20) != 0)
                m_state.overrun = false;

            if ((value & 0x10) != 0)
                m_state.underrun = false;

            if ((value & 0x0F) == 0x0F)
            {
                m_state.buffer_head = 0;
                m_state.buffer_count = 0;
                m_state.high_byte = false;
            }
            break;
        case 0x0C:
            m_state.transfer_control = value & (k_pcm16_transfer_input_irq | k_pcm16_transfer_dreq_mask |
                k_pcm16_transfer_buffer_mask);

            if ((value & k_pcm16_transfer_input_flag) != 0)
                m_state.input_flag = false;
            break;
        case 0x0D:
            if ((value & 0x80) != 0)
                m_state.peak = 0;
            else
                m_state.threshold = value & 0x7F;
            break;
        case 0x0E:
            m_state.data_low = value;
            break;
        default:
            // The high byte of a word write at 051Eh completes it
            if (IsSoftwareTransfer() && !IsRecording())
            {
                u16 word = (u16)(m_state.data_low | (value << 8));
                TraceData(word, false);
                PushWord(word, clocks);
            }
            break;
    }

    ServiceDMA(clocks);
    UpdateIRQ();
}

void PCM16::Tick(u64 time)
{
    if (IsRecording())
        Record(time);
    else
        Play(time);
}

void PCM16::Play(u64 time)
{
    u16 word = 0;
    s16 left = m_state.left_sample;
    s16 right = m_state.right_sample;

    switch (GetFormat())
    {
        case PCM16_FORMAT_16BIT_STEREO:
            if (PopWord(word, time))
                left = (s16)word;

            if (PopWord(word, time))
                right = (s16)word;
            break;
        case PCM16_FORMAT_16BIT_MONO:
            if (PopWord(word, time))
                left = right = (s16)word;
            break;
        case PCM16_FORMAT_8BIT_STEREO:
            if (PopWord(word, time))
            {
                left = ExpandByte((u8)word);
                right = ExpandByte((u8)(word >> 8));
            }
            break;
        default:
            // Two mono samples per word, the low byte first as they are in memory
            if (m_state.high_byte)
            {
                left = right = ExpandByte((u8)(m_state.word >> 8));
                m_state.high_byte = false;
            }
            else if (PopWord(word, time))
            {
                m_state.word = word;
                m_state.high_byte = true;
                left = right = ExpandByte((u8)word);
            }
            break;
    }

    m_state.previous_left_sample = m_state.left_sample;
    m_state.previous_right_sample = m_state.right_sample;
    m_state.left_sample = left;
    m_state.right_sample = right;
}

void PCM16::Record(u64 time)
{
    s32 left = m_state.input_left;
    s32 right = m_state.input_right;

    switch (GetFormat())
    {
        case PCM16_FORMAT_16BIT_STEREO:
            PushWord((u16)left, time);
            PushWord((u16)right, time);
            break;
        case PCM16_FORMAT_16BIT_MONO:
            PushWord((u16)CLAMP(left + right, -32768, 32767), time);
            break;
        case PCM16_FORMAT_8BIT_STEREO:
            PushWord((u16)(ConvertSample(left) | (ConvertSample(right) << 8)), time);
            break;
        default:
            if (m_state.high_byte)
            {
                PushWord((u16)(m_state.word | (ConvertSample(left + right) << 8)), time);
                m_state.high_byte = false;
            }
            else
            {
                m_state.word = ConvertSample(left + right);
                m_state.high_byte = true;
            }
            break;
    }
}

bool PCM16::PopWord(u16& word, u64 time)
{
    if (m_state.buffer_count == 0)
    {
        SetUnderrun();
        return false;
    }

    word = m_state.buffer[m_state.buffer_head];
    m_state.buffer_head = (u8)((m_state.buffer_head + 1) % PCM16_BUFFER_SIZE);
    m_state.buffer_count--;
    ServiceDMA(time);
    return true;
}

void PCM16::PushWord(u16 word, u64 time)
{
    if (m_state.buffer_count == PCM16_BUFFER_SIZE)
    {
        SetOverrun();
        return;
    }

    m_state.buffer[(m_state.buffer_head + m_state.buffer_count) % PCM16_BUFFER_SIZE] = word;
    m_state.buffer_count++;
    ServiceDMA(time);
}

void PCM16::ServiceDMA(u64 time)
{
    if (!m_fitted || !IsDMAEnabled())
        return;

    GT_Bus_Access_Context context = { };
    context.clocks = time;
    context.origin = GT_BUS_ORIGIN_DMA;

    if (IsRecording())
    {
        while (IsDMAEnabled() && (m_state.buffer_count > 0))
        {
            m_memory->Write16Physical(m_state.current_address, m_state.buffer[m_state.buffer_head], context);
            m_state.buffer_head = (u8)((m_state.buffer_head + 1) % PCM16_BUFFER_SIZE);
            m_state.buffer_count--;
            AdvanceDMA();
        }
    }
    else
    {
        while (IsDMAEnabled() && (m_state.buffer_count < PCM16_BUFFER_SIZE))
        {
            u16 word = m_memory->Read16Physical(m_state.current_address, context);
            m_state.buffer[(m_state.buffer_head + m_state.buffer_count) % PCM16_BUFFER_SIZE] = word;
            m_state.buffer_count++;
            AdvanceDMA();
        }
    }
}

void PCM16::AdvanceDMA()
{
    bool terminal = m_state.current_count == 0;
    m_state.current_address += 2;
    m_state.current_count--;

    if (!terminal)
        return;

    m_state.terminal_count = true;
    TraceDMAEnd();

    if ((m_state.dma_control & k_pcm16_dma_autoinit) != 0)
    {
        m_state.current_address = m_state.base_address;
        m_state.current_count = m_state.base_count;
    }
    else
        m_state.transfer_control |= k_pcm16_transfer_dreq_mask;

    UpdateIRQ();
}

s16 PCM16::ExpandByte(u8 value) const
{
    if (GetFormat() != PCM16_FORMAT_SND)
        return (s16)((value - 0x80) * 256);

    s32 magnitude = (value & 0x7F) * 256;
    return (s16)((value & 0x80) != 0 ? magnitude : -magnitude);
}

u8 PCM16::ConvertSample(s32 sample) const
{
    s32 value = CLAMP(sample >> ((m_state.mode & k_pcm16_mode_level) + 1), -128, 127);

    if (GetFormat() != PCM16_FORMAT_SND)
        return (u8)(value + 0x80);

    return value >= 0 ? (u8)(0x80 | MIN(value, 0x7E)) : (u8)MIN(-value, 0x7F);
}

u8 PCM16::GetLevel(s32 sample) const
{
    PCM16_Format format = GetFormat();
    bool wide = (format == PCM16_FORMAT_16BIT_MONO) || (format == PCM16_FORMAT_16BIT_STEREO);
    s32 value = sample >> (wide ? 8 : (m_state.mode & k_pcm16_mode_level) + 1);
    s32 magnitude = value < 0 ? -value : value;
    return (u8)(((value > 127) || (value < -128) ? 0x80 : 0x00) | MIN(magnitude, 0x7F));
}

void PCM16::Input(s32 left, s32 right)
{
    m_state.input_left = (s16)CLAMP(left, -32768, 32767);
    m_state.input_right = (s16)CLAMP(right, -32768, 32767);
    m_state.level_left = GetLevel(left);
    m_state.level_right = GetLevel(right);

    u8 level = MAX(m_state.level_left & 0x7F, m_state.level_right & 0x7F);
    u8 overload = (m_state.level_left | m_state.level_right | m_state.peak) & 0x80;
    m_state.peak = (u8)(overload | MAX(m_state.peak & 0x7F, level));

    if (!m_state.input_flag && (level >= m_state.threshold))
    {
        m_state.input_flag = true;
        UpdateIRQ();
    }
}

u64 PCM16::GetClocksToEvent() const
{
    if (!IsRunning())
        return GT_NO_EVENT;

    bool record = IsRecording();
    u64 ticks = GT_NO_EVENT;

    // DMA keeps the buffer full for playback and empty for recording, so it moves a word with every word played or
    // recorded, whatever the interrupts. Without it, only the buffer error interrupt has a clock to keep
    if (IsDMAEnabled())
        ticks = GetTicksToWord(1, record);
    else if ((GetIRQLine() >= 0) && !m_state.overrun && !m_state.underrun &&
        ((m_state.buffer_control & k_pcm16_buffer_irq) != 0))
        ticks = GetTicksToWord(record ? PCM16_BUFFER_SIZE - m_state.buffer_count + 1 : m_state.buffer_count + 1, record);

    return ticks == GT_NO_EVENT ? GT_NO_EVENT : GetClocksToTick(ticks);
}

u64 PCM16::GetTicksToWord(u32 words, bool record) const
{
    switch (GetFormat())
    {
        case PCM16_FORMAT_16BIT_STEREO:
            return ((u64)words + 1) / 2;
        case PCM16_FORMAT_16BIT_MONO:
        case PCM16_FORMAT_8BIT_STEREO:
            return words;
        default:
            // An 8-bit mono word leaves the buffer on its low byte's tick and enters it on its high byte's tick
            return 2ULL * words - (m_state.high_byte == record ? 1 : 0);
    }
}

u64 PCM16::GetClocksToTick(u64 ticks) const
{
    u64 rate = k_pcm16_rates[m_state.clock];
    return (ticks * k_pcm16_period - m_state.phase + rate - 1) / rate;
}

void PCM16::SetUnderrun()
{
    if (m_state.underrun)
        return;

    m_state.underrun = true;
    TraceBufferError();
    UpdateIRQ();
}

void PCM16::SetOverrun()
{
    if (m_state.overrun)
        return;

    m_state.overrun = true;
    TraceBufferError();
    UpdateIRQ();
}

void PCM16::UpdateIRQ()
{
    if (!IsValidPointer(m_pic))
        return;

    int line = m_fitted ? GetIRQLine() : -1;
    u8 causes = GetIRQCauses();
    bool asserted = (line >= 0) && (causes != 0);

    // No other emulated device drives IRQ 4, 5, 10, 14 or 15

    if ((m_irq_line >= 0) && (line != m_irq_line) && m_irq_asserted)
        m_pic->SetIRQLine(m_irq_line, false);

    if ((line >= 0) && ((line != m_irq_line) || (asserted != m_irq_asserted)))
    {
        if (asserted)
            TraceIRQ(causes);

        m_pic->SetIRQLine(line, asserted);
    }

    m_irq_line = line;
    m_irq_asserted = asserted;
}

void PCM16::TraceRegister(u16 port, u8 value)
{
    if (!IsValidPointer(m_trace_logger) || !m_trace_logger->IsEventEnabled(TRACE_PCM16, TRACE_PCM16_REGISTER))
        return;

    GT_Trace_Entry entry = {};
    entry.type = TRACE_PCM16;
    entry.event = TRACE_PCM16_REGISTER;
    entry.pcm16.port = port;
    entry.pcm16.value = value;
    entry.pcm16.fifo = m_state.buffer_count;
    m_trace_logger->TraceLog(entry);
}

void PCM16::TraceData(u16 word, bool read)
{
    if (!IsValidPointer(m_trace_logger) || !m_trace_logger->IsEventEnabled(TRACE_PCM16, TRACE_PCM16_DATA))
        return;

    GT_Trace_Entry entry = {};
    entry.type = TRACE_PCM16;
    entry.event = TRACE_PCM16_DATA;
    entry.pcm16.port = 0x051E;
    entry.pcm16.data = word;
    entry.pcm16.read = read ? 1 : 0;
    entry.pcm16.fifo = m_state.buffer_count;
    m_trace_logger->TraceLog(entry);
}

void PCM16::TraceDMAEnd()
{
    if (!IsValidPointer(m_trace_logger) || !m_trace_logger->IsEventEnabled(TRACE_PCM16, TRACE_PCM16_DMA_END))
        return;

    GT_Trace_Entry entry = {};
    entry.type = TRACE_PCM16;
    entry.event = TRACE_PCM16_DMA_END;
    entry.pcm16.address = m_state.current_address;
    entry.pcm16.count = m_state.base_count;
    entry.pcm16.read = IsRecording() ? 0 : 1;
    entry.pcm16.autoinit = (m_state.dma_control & k_pcm16_dma_autoinit) != 0 ? 1 : 0;
    m_trace_logger->TraceLog(entry);
}

void PCM16::TraceBufferError()
{
    if (!IsValidPointer(m_trace_logger) || !m_trace_logger->IsEventEnabled(TRACE_PCM16, TRACE_PCM16_BUFFER_ERROR))
        return;

    GT_Trace_Entry entry = {};
    entry.type = TRACE_PCM16;
    entry.event = TRACE_PCM16_BUFFER_ERROR;
    entry.pcm16.read = IsRecording() ? 0 : 1;
    entry.pcm16.fifo = m_state.buffer_count;
    m_trace_logger->TraceLog(entry);
}

void PCM16::TraceIRQ(u8 causes)
{
    if (!IsValidPointer(m_trace_logger) || !m_trace_logger->IsEventEnabled(TRACE_PCM16, TRACE_PCM16_IRQ))
        return;

    GT_Trace_Entry entry = {};
    entry.type = TRACE_PCM16;
    entry.event = TRACE_PCM16_IRQ;
    entry.pcm16.causes = causes;
    entry.pcm16.line = (u8)GetIRQLine();
    m_trace_logger->TraceLog(entry);
}

void PCM16::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void PCM16::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void PCM16::Serialize(StateSerializer& serializer)
{
    G_SERIALIZE(serializer, m_state.base_address);
    G_SERIALIZE(serializer, m_state.current_address);
    G_SERIALIZE(serializer, m_state.base_count);
    G_SERIALIZE(serializer, m_state.current_count);
    G_SERIALIZE_ARRAY(serializer, m_state.buffer, PCM16_BUFFER_SIZE);
    G_SERIALIZE(serializer, m_state.buffer_head);
    G_SERIALIZE(serializer, m_state.buffer_count);
    G_SERIALIZE(serializer, m_state.bank);
    G_SERIALIZE(serializer, m_state.dma_control);
    G_SERIALIZE(serializer, m_state.terminal_count);
    G_SERIALIZE(serializer, m_state.clock);
    G_SERIALIZE(serializer, m_state.mode);
    G_SERIALIZE(serializer, m_state.system_control);
    G_SERIALIZE(serializer, m_state.buffer_control);
    G_SERIALIZE(serializer, m_state.overrun);
    G_SERIALIZE(serializer, m_state.underrun);
    G_SERIALIZE(serializer, m_state.transfer_control);
    G_SERIALIZE(serializer, m_state.input_flag);
    G_SERIALIZE(serializer, m_state.peak);
    G_SERIALIZE(serializer, m_state.threshold);
    G_SERIALIZE(serializer, m_state.level_left);
    G_SERIALIZE(serializer, m_state.level_right);
    G_SERIALIZE(serializer, m_state.data_low);
    G_SERIALIZE(serializer, m_state.data_high);
    G_SERIALIZE(serializer, m_state.word);
    G_SERIALIZE(serializer, m_state.high_byte);
    G_SERIALIZE(serializer, m_state.phase);
    G_SERIALIZE(serializer, m_state.reset_clocks);
    G_SERIALIZE(serializer, m_state.ready_clocks);
    G_SERIALIZE(serializer, m_state.input_left);
    G_SERIALIZE(serializer, m_state.input_right);
    G_SERIALIZE(serializer, m_state.left_sample);
    G_SERIALIZE(serializer, m_state.right_sample);
    G_SERIALIZE(serializer, m_state.previous_left_sample);
    G_SERIALIZE(serializer, m_state.previous_right_sample);
}

void PCM16::SanitizeState()
{
    m_state.base_address &= 0xFFFFFFFE;
    m_state.current_address &= 0xFFFFFFFE;
    m_state.buffer_head %= PCM16_BUFFER_SIZE;
    m_state.buffer_count = MIN(m_state.buffer_count, (u8)PCM16_BUFFER_SIZE);
    m_state.bank &= 0x01;
    m_state.dma_control &= k_pcm16_dma_autoinit | k_pcm16_dma_irq;
    m_state.clock &= 0x0F;
    m_state.mode &= 0x3F;
    m_state.system_control &= k_pcm16_system_data_port | k_pcm16_system_record | k_pcm16_system_irq;
    m_state.buffer_control &= k_pcm16_buffer_irq;
    m_state.transfer_control &= k_pcm16_transfer_input_irq | k_pcm16_transfer_dreq_mask | k_pcm16_transfer_buffer_mask;
    m_state.threshold &= 0x7F;
    m_state.phase %= k_pcm16_period;
    m_irq_line = -1;
    m_irq_asserted = false;
    UpdateIRQ();
}
