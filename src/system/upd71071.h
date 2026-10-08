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

#ifndef UPD71071_H
#define UPD71071_H

#include <iostream>
#include "../common/common.h"

#define UPD71071_CHANNELS 4

class Memory;
class Scheduler;
class TraceLogger;
class StateSerializer;

typedef bool (*GT_DMA_Read_Fn)(void* device, u16& value, bool word);
typedef bool (*GT_DMA_Write_Fn)(void* device, u16 value, bool word);
typedef void (*GT_DMA_End_Fn)(void* device, bool terminal_count);

struct GT_DMA_Endpoint
{
    void* device;
    GT_DMA_Read_Fn read;
    GT_DMA_Write_Fn write;
    GT_DMA_End_Fn end;
};

class UPD71071
{
public:
    struct UPD71071_Channel
    {
        u32 current_address;
        u32 base_address;
        u16 current_count;
        u16 base_count;
        u8 mode;
    };

    struct UPD71071_State
    {
        UPD71071_Channel channels[UPD71071_CHANNELS];
        u16 device_control;
        u16 temporary;
        u8 high_address;
        u8 selected_channel;
        bool base_access;
        bool bus_16bit;
        u8 mask;
        u8 software_requests;
        u8 request_levels;
        u8 stalled;
        u8 status_tc;
        s8 active_channel;
        u64 next_clocks;
    };

public:
    UPD71071();
    ~UPD71071();
    void Init(Memory* memory, Scheduler* scheduler);
    void SetTraceLogger(TraceLogger* trace_logger);
    void Reset();
    u8 Read(u16 port);
    void Write(u16 port, u8 value);
    u8 Peek(u16 port) const;
    void SetEndpoint(int channel, const GT_DMA_Endpoint& endpoint);
    void SetRequest(int channel, bool active);
    void ExternalEnd(int channel);
    u32 HandleEvent(u64 clocks);
    UPD71071_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

private:
    void Initialize();
    void WriteCount(int index, u8 value);
    void WriteAddress(int index, u8 value);
    int GetServiceChannel() const;
    bool IsSupportedMode(u8 mode) const;
    bool TransferUnit(int channel, u64 clocks, bool& terminal);
    void FinishService(int channel, bool terminal_count);
    void CheckUnsupported();
    void UpdateNextEvent();
    void TraceEvent(u8 event, int channel, u8 reg, u8 value, bool terminal);
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

private:
    Memory* m_memory;
    Scheduler* m_scheduler;
    GT_DMA_Endpoint m_endpoints[UPD71071_CHANNELS];
    bool m_unsupported_logged;
    TraceLogger* m_trace_logger;
    UPD71071_State m_state;
};

static const u16 k_upd71071_ddma = 0x0004;
static const u16 k_upd71071_supported_control = 0x0024;
static const u8 k_upd71071_mode_word = 0x01;
static const u8 k_upd71071_mode_auto_init = 0x10;
static const u8 k_upd71071_mode_decrement = 0x20;
static const u8 k_upd71071_io_to_memory = 0x01;
static const u8 k_upd71071_memory_to_io = 0x02;
static const u8 k_upd71071_demand = 0x00;
static const u8 k_upd71071_single = 0x01;
static const u64 k_upd71071_unit_clocks = ((u64)GT_CPU_CLOCK_RATE * 2) / 1000000;

#include "upd71071_inline.h"

#endif /* UPD71071_H */
