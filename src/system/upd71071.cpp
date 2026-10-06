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

#include "upd71071.h"
#include "../common/trace_logger.h"
#include "memory.h"
#include "scheduler.h"
#include "../common/state_serializer.h"

UPD71071::UPD71071()
{
    InitPointer(m_memory);
    InitPointer(m_scheduler);
    InitPointer(m_trace_logger);
    memset(m_endpoints, 0, sizeof(m_endpoints));
    m_unsupported_logged = false;
    memset(&m_state, 0, sizeof(m_state));
    m_state.active_channel = -1;
}

UPD71071::~UPD71071()
{
}

void UPD71071::Init(Memory* memory, Scheduler* scheduler)
{
    m_memory = memory;
    m_scheduler = scheduler;
    Reset();
}

// A hardware reset selects the 8 bit bus, power on also clears what a software initialization keeps
void UPD71071::Reset()
{
    memset(&m_state, 0, sizeof(m_state));
    Initialize();
    UpdateNextEvent();
}

// Software initialization keeps the addresses, counts, bus width and the external high byte
void UPD71071::Initialize()
{
    for (int i = 0; i < UPD71071_CHANNELS; i++)
        m_state.channels[i].mode = 0;

    m_state.device_control = 0;
    m_state.temporary = 0;
    m_state.selected_channel = 0;
    m_state.base_access = false;
    m_state.mask = 0x0F;
    m_state.software_requests = 0;
    m_state.stalled = 0;
    m_state.status_tc = 0;
    m_state.active_channel = -1;
}

u8 UPD71071::Read(u16 port)
{
    u8 value = Peek(port);

    // Reading the status clears the terminal count bits, not the request bits
    if ((port & 0x0F) == 0x0B)
        m_state.status_tc = 0;

    return value;
}

u8 UPD71071::Peek(u16 port) const
{
    const UPD71071_Channel& channel = m_state.channels[m_state.selected_channel];
    u16 count = m_state.base_access ? channel.base_count : channel.current_count;
    u32 address = m_state.base_access ? channel.base_address : channel.current_address;

    switch (port & 0x0F)
    {
        case 0x01:
            return (u8)((1 << m_state.selected_channel) | (m_state.base_access ? 0x10 : 0x00));
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
            return m_state.high_address;
        case 0x08:
            return (u8)m_state.device_control;
        case 0x09:
            return (u8)(m_state.device_control >> 8);
        case 0x0A:
            return channel.mode;
        case 0x0B:
            return (u8)((m_state.request_levels << 4) | m_state.status_tc);
        case 0x0C:
            return (u8)m_state.temporary;
        case 0x0D:
            return (u8)(m_state.temporary >> 8);
        case 0x0E:
            return m_state.software_requests;
        case 0x0F:
            return m_state.mask;
        default:
            return 0xFF;
    }
}

void UPD71071::Write(u16 port, u8 value)
{
    switch (port & 0x0F)
    {
        case 0x00:
            // RES keeps the bus width written along with it
            m_state.bus_16bit = (value & 0x02) != 0;

            if ((value & 0x01) != 0)
                Initialize();
            break;
        case 0x01:
            m_state.selected_channel = value & 0x03;
            m_state.base_access = (value & 0x04) != 0;
            break;
        case 0x02:
        case 0x03:
            WriteCount((port & 0x0F) - 0x02, value);
            break;
        case 0x04:
        case 0x05:
        case 0x06:
            WriteAddress((port & 0x0F) - 0x04, value);
            break;
        case 0x07:
            m_state.high_address = value;
            break;
        case 0x08:
            m_state.device_control = (u16)((m_state.device_control & 0xFF00) | value);
            CheckUnsupported();
            break;
        case 0x09:
            m_state.device_control = (u16)((m_state.device_control & 0x00FF) | ((value & 0x03) << 8));
            CheckUnsupported();
            break;
        case 0x0A:
            m_state.channels[m_state.selected_channel].mode = value;
            CheckUnsupported();
            break;
        case 0x0E:
            m_state.software_requests = value & 0x0F;
            break;
        case 0x0F:
            m_state.mask = value & 0x0F;
            break;
        default:
            return;
    }

    m_state.stalled = 0;
    UpdateNextEvent();
}

void UPD71071::SetEndpoint(int channel, const GT_DMA_Endpoint& endpoint)
{
    m_endpoints[channel & 0x03] = endpoint;
}

// Dropping the request ends demand service without a terminal count
void UPD71071::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

void UPD71071::SetRequest(int channel, bool active)
{
    u8 bit = (u8)(1 << (channel & 0x03));

    if (active && (m_state.request_levels & bit) == 0 && IsValidPointer(m_trace_logger) &&
        m_trace_logger->IsEnabled(TRACE_DMA))
    {
        const UPD71071_Channel& state = m_state.channels[channel & 0x03];
        GT_Trace_Entry* entry = m_trace_logger->Record(TRACE_DMA, TRACE_DMA_REQUEST);
        entry->dma.address = state.current_address;
        entry->dma.count = state.current_count;
        entry->dma.channel = (u8)(channel & 0x03);
        entry->dma.mode = state.mode;
        entry->dma.terminal = 0;
    }

    if (active)
    {
        m_state.request_levels |= bit;
        m_state.stalled &= ~bit;
    }
    else
    {
        m_state.request_levels &= ~bit;

        if (m_state.active_channel == channel)
            m_state.active_channel = -1;
    }

    UpdateNextEvent();
}

void UPD71071::ExternalEnd(int channel)
{
    channel &= 0x03;
    u8 bit = (u8)(1 << channel);

    if (m_state.active_channel != channel && (m_state.request_levels & ~m_state.mask & bit) == 0)
        return;

    FinishService(channel, false);
    UpdateNextEvent();
}

// One unit per bus acquisition, a demand channel keeps the bus while its request stays active
u32 UPD71071::HandleEvent(u64 clocks)
{
    int channel = GetServiceChannel();

    if (channel < 0)
    {
        m_state.active_channel = -1;
        UpdateNextEvent();
        return 0;
    }

    u8 bit = (u8)(1 << channel);
    bool software = (m_state.software_requests & bit) != 0;
    bool terminal = false;

    if (!TransferUnit(channel, clocks, terminal))
    {
        // The device holds its request without data, so wait until it asserts it again
        m_state.stalled |= bit;
        m_state.active_channel = -1;
        UpdateNextEvent();
        return 0;
    }

    // In bus release mode a software request gets one unit and then all request bits clear
    if (software)
        m_state.software_requests = 0;

    u8 service = (m_state.channels[channel].mode >> 6) & 0x03;

    if (terminal)
        FinishService(channel, true);
    else if (service == k_upd71071_demand && !software && (m_state.request_levels & ~m_state.mask & bit) != 0)
        m_state.active_channel = (s8)channel;
    else
        m_state.active_channel = -1;

    // The CPU gets the bus back between single transfers
    m_state.next_clocks = clocks + (m_state.active_channel >= 0 ? k_upd71071_unit_clocks : k_upd71071_unit_clocks * 2);
    UpdateNextEvent();
    return (u32)k_upd71071_unit_clocks;
}

void UPD71071::WriteCount(int index, u8 value)
{
    UPD71071_Channel& channel = m_state.channels[m_state.selected_channel];
    int shift = index * 8;
    u16 mask = (u16)(0xFF << shift);

    channel.base_count = (u16)((channel.base_count & ~mask) | (value << shift));

    if (!m_state.base_access)
        channel.current_count = (u16)((channel.current_count & ~mask) | (value << shift));
}

void UPD71071::WriteAddress(int index, u8 value)
{
    UPD71071_Channel& channel = m_state.channels[m_state.selected_channel];
    int shift = index * 8;
    u32 mask = 0xFFu << shift;

    channel.base_address = (channel.base_address & ~mask) | ((u32)value << shift);

    if (!m_state.base_access)
        channel.current_address = (channel.current_address & ~mask) | ((u32)value << shift);
}

// Fixed priority, channel 0 first, the mask only gates hardware requests
int UPD71071::GetServiceChannel() const
{
    if ((m_state.device_control & k_upd71071_ddma) != 0)
        return -1;

    u8 requests = (u8)(((m_state.request_levels & ~m_state.mask) | m_state.software_requests) & ~m_state.stalled);

    if (m_state.active_channel >= 0 && (requests & (1 << m_state.active_channel)) != 0)
        return m_state.active_channel;

    for (int i = 0; i < UPD71071_CHANNELS; i++)
    {
        if ((requests & (1 << i)) != 0 && IsSupportedMode(m_state.channels[i].mode))
            return i;
    }

    return -1;
}

// The count holds units minus one, so the unit that starts at zero produces the terminal count
bool UPD71071::TransferUnit(int channel, u64 clocks, bool& terminal)
{
    UPD71071_Channel& state = m_state.channels[channel];
    const GT_DMA_Endpoint& endpoint = m_endpoints[channel];
    bool word = (state.mode & k_upd71071_mode_word) != 0 && m_state.bus_16bit;
    u8 direction = (state.mode >> 2) & 0x03;

    // Word transfers move the odd address down to the even one first
    if (word && (state.current_address & 0x01) != 0)
        state.current_address = (state.current_address - 1) & 0x00FFFFFF;

    u32 address = ((u32)m_state.high_address << 24) | state.current_address;
    GT_Bus_Access_Context context = { };
    context.clocks = clocks;
    context.origin = GT_BUS_ORIGIN_DMA;
    u16 value = 0;

    if (direction == k_upd71071_io_to_memory)
    {
        if (!IsValidPointer(endpoint.read) || !endpoint.read(endpoint.device, value, word))
            return false;

        m_memory->Write8Physical(address, (u8)value, context);

        if (word)
            m_memory->Write8Physical(address + 1, (u8)(value >> 8), context);
    }
    else
    {
        if (!IsValidPointer(endpoint.write))
            return false;

        value = m_memory->Read8Physical(address, context);

        if (word)
            value |= (u16)(m_memory->Read8Physical(address + 1, context) << 8);

        if (!endpoint.write(endpoint.device, value, word))
            return false;
    }

    int step = word ? 2 : 1;

    if ((state.mode & k_upd71071_mode_decrement) != 0)
        state.current_address = (state.current_address - step) & 0x00FFFFFF;
    else
        state.current_address = (state.current_address + step) & 0x00FFFFFF;

    terminal = state.current_count == 0;
    state.current_count--;
    return true;
}

// TC and END both latch the status bit, then the channel reloads from base or masks itself
void UPD71071::FinishService(int channel, bool terminal_count)
{
    UPD71071_Channel& state = m_state.channels[channel];
    u8 bit = (u8)(1 << channel);

    if (IsValidPointer(m_trace_logger) && m_trace_logger->IsEnabled(TRACE_DMA))
    {
        GT_Trace_Entry* entry = m_trace_logger->Record(TRACE_DMA, TRACE_DMA_END);
        entry->dma.address = state.current_address;
        entry->dma.count = state.current_count;
        entry->dma.channel = (u8)channel;
        entry->dma.mode = state.mode;
        entry->dma.terminal = terminal_count ? 1 : 0;
    }

    m_state.status_tc |= bit;

    if ((state.mode & k_upd71071_mode_auto_init) != 0)
    {
        state.current_address = state.base_address;
        state.current_count = state.base_count;
    }
    else
        m_state.mask |= bit;

    if (m_state.active_channel == channel)
        m_state.active_channel = -1;

    if (IsValidPointer(m_endpoints[channel].end))
        m_endpoints[channel].end(m_endpoints[channel].device, terminal_count);
}

void UPD71071::CheckUnsupported()
{
    if (m_unsupported_logged)
        return;

    if ((m_state.device_control & ~k_upd71071_supported_control) != 0)
    {
        Debug("DMA: unsupported device control %04X", m_state.device_control);
        m_unsupported_logged = true;
    }
    else if (!IsSupportedMode(m_state.channels[m_state.selected_channel].mode))
    {
        Debug("DMA: unsupported mode %02X on channel %d", m_state.channels[m_state.selected_channel].mode,
            m_state.selected_channel);
        m_unsupported_logged = true;
    }
}

void UPD71071::UpdateNextEvent()
{
    u64 next = GT_NO_EVENT;

    if (GetServiceChannel() >= 0)
        next = MAX(m_state.next_clocks, m_scheduler->GetClocks());

    m_scheduler->Schedule(SCHEDULER_EVENT_DMA, next);
}

void UPD71071::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
}

void UPD71071::LoadState(std::istream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer);
    SanitizeState();
}

void UPD71071::Serialize(StateSerializer& serializer)
{
    for (int i = 0; i < UPD71071_CHANNELS; i++)
    {
        G_SERIALIZE(serializer, m_state.channels[i].current_address);
        G_SERIALIZE(serializer, m_state.channels[i].base_address);
        G_SERIALIZE(serializer, m_state.channels[i].current_count);
        G_SERIALIZE(serializer, m_state.channels[i].base_count);
        G_SERIALIZE(serializer, m_state.channels[i].mode);
    }

    G_SERIALIZE(serializer, m_state.device_control);
    G_SERIALIZE(serializer, m_state.temporary);
    G_SERIALIZE(serializer, m_state.high_address);
    G_SERIALIZE(serializer, m_state.selected_channel);
    G_SERIALIZE(serializer, m_state.base_access);
    G_SERIALIZE(serializer, m_state.bus_16bit);
    G_SERIALIZE(serializer, m_state.mask);
    G_SERIALIZE(serializer, m_state.software_requests);
    G_SERIALIZE(serializer, m_state.request_levels);
    G_SERIALIZE(serializer, m_state.stalled);
    G_SERIALIZE(serializer, m_state.status_tc);
    G_SERIALIZE(serializer, m_state.active_channel);
    G_SERIALIZE(serializer, m_state.next_clocks);
}

void UPD71071::SanitizeState()
{
    for (int i = 0; i < UPD71071_CHANNELS; i++)
    {
        m_state.channels[i].current_address &= 0x00FFFFFF;
        m_state.channels[i].base_address &= 0x00FFFFFF;
    }

    m_state.selected_channel &= 0x03;

    if (m_state.active_channel < -1 || m_state.active_channel >= UPD71071_CHANNELS)
        m_state.active_channel = -1;

    UpdateNextEvent();
}
