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

#ifndef I386_MEMORY_INLINE_H
#define I386_MEMORY_INLINE_H

#include "../system/memory.h"

INLINE u32 I386::LoadHost(const u8* data, int width) const
{
    if (width == 32)
        return read_u32_le(data);

    if (width == 16)
        return read_u16_le(data);

    return data[0];
}

INLINE void I386::StoreHost(u8* data, u32 value, int width)
{
    if (width == 32)
        write_u32_le(data, value);
    else if (width == 16)
        write_u16_le(data, (u16)value);
    else
        data[0] = (u8)value;
}

// Every entry point that can access guest memory refreshes the host page pointers first
// The memory map can only change between CPU calls or through I/O, which ends the batch
INLINE void I386::SetBusContext(GT_Bus_Access_Context& context)
{
    m_bus_context = &context;

    if (unlikely(m_memory_generation != m_memory->GetMapGeneration()))
        RefreshMemoryPointers();

    UpdateMemoryMode();
}

// Data breakpoints, debugger memory breakpoints and write observers disable the memory fast paths
INLINE void I386::UpdateMemoryMode()
{
    bool slow = (m_state.debug_registers[7] & 0xFF) != 0 || m_debugger_memory_checks ||
        (IsValidPointer(m_bus_context) && IsValidPointer(m_bus_context->observe_memory_write));

    if (unlikely(slow != m_slow_memory))
        SetSlowMemory(slow);
}

// Cached TLB host pages are only valid for the privilege level they were checked against
INLINE void I386::UpdateUserMode()
{
    bool user = m_state.execution_mode == I386_MODE_VM86 || m_state.current_privilege_level == 3;

    if (unlikely(user != m_user_mode))
        SetUserMode(user);
}

INLINE void I386::CloseCodeWindow()
{
    m_code_window_size = 0;
}

// Fast paths only check the most recently used way of the set. A repeated hit on that way leaves the
// pseudo-LRU bits unchanged, so skipping their update there is exact. Hits on other ways take the slow path
INLINE const I386::TLBEntry& I386::GetRecentTLBEntry(u32 linear) const
{
    u32 set = (linear >> 12) & (I386_TLB_SETS - 1);
    return m_tlb[set * I386_TLB_WAYS + m_tlb_mru[set]];
}

// A translating access to a way points the tree away from it: B0 selects the half, B1 and B2 the way in it
// The code window skips these updates, which is only exact while its page stays the most recent way
INLINE void I386::TouchTLBWay(u32 set, u32 way)
{
    static const u8 k_plru_keep_mask[I386_TLB_WAYS] = { 0x04, 0x04, 0x02, 0x02 };
    static const u8 k_plru_way_bits[I386_TLB_WAYS] = { 0x03, 0x01, 0x04, 0x00 };

    if (m_tlb_mru[set] != way)
        CloseCodeWindow();

    m_tlb_plru[set] = (m_tlb_plru[set] & k_plru_keep_mask[way]) | k_plru_way_bits[way];
    m_tlb_mru[set] = (u8)way;
}

// TLB host pages are only cached for valid entries the current privilege level may use
INLINE const u8* I386::GetReadHost(u32 linear, u32 size)
{
    if (unlikely((linear & 0xFFF) + size > 0x1000))
        return NULL;

    const u8* page;

    if ((m_state.cr0 & 0x80000000U) == 0)
        page = m_read_pages[linear >> 12];
    else
    {
        const TLBEntry& entry = GetRecentTLBEntry(linear);

        if (entry.linear_page != (linear & 0xFFFFF000U))
            return NULL;

        page = entry.read_page;
    }

    return IsValidPointer(page) ? page + (linear & 0xFFF) : NULL;
}

INLINE u8* I386::GetWriteHost(u32 linear, u32 size)
{
    if (unlikely((linear & 0xFFF) + size > 0x1000))
        return NULL;

    u8* page;

    if ((m_state.cr0 & 0x80000000U) == 0)
        page = m_write_pages[linear >> 12];
    else
    {
        const TLBEntry& entry = GetRecentTLBEntry(linear);

        if (entry.linear_page != (linear & 0xFFFFF000U))
            return NULL;

        page = entry.write_page;
    }

    return IsValidPointer(page) ? page + (linear & 0xFFF) : NULL;
}

// Physical accesses from the CPU slow paths: plainly mapped pages directly, everything else through Memory
// Writes stay in Memory while data breakpoints or write observers are active
INLINE u32 I386::ReadPhysical(u32 physical, u32 size, GT_Bus_Access_Context& context)
{
    const u8* page = m_read_pages[physical >> 12];

    if (likely(IsValidPointer(page) && (physical & 0xFFF) <= 0x1000 - size))
        return LoadHost(page + (physical & 0xFFF), (int)size * 8);

    if (size == 1)
        return m_memory->Read8Physical(physical, context);

    if (size == 2)
        return m_memory->Read16Physical(physical, context);

    return m_memory->Read32Physical(physical, context);
}

INLINE void I386::WritePhysical(u32 physical, u32 value, u32 size, GT_Bus_Access_Context& context)
{
    u8* page = m_write_pages[physical >> 12];

    if (likely(IsValidPointer(page) && !m_slow_memory && (physical & 0xFFF) <= 0x1000 - size))
    {
        StoreHost(page + (physical & 0xFFF), value, (int)size * 8);
        return;
    }

    if (size == 1)
        m_memory->Write8Physical(physical, (u8)value, context);
    else if (size == 2)
        m_memory->Write16Physical(physical, (u16)value, context);
    else
        m_memory->Write32Physical(physical, value, context);
}

// Segment limits are -1 while data breakpoints or write observers require the checked path
INLINE bool I386::ReadMemory(int segment, u32 offset, int width, GT_Bus_Access_Context& context, u32& value, bool stack)
{
    u32 size = (u32)width >> 3;

    if (likely((s64)((u64)offset + size - 1) <= m_read_limits[segment]))
    {
        const u8* data = GetReadHost(m_state.segments[segment].base + offset, size);

        if (likely(IsValidPointer(data)))
        {
            value = LoadHost(data, width);
            return true;
        }
    }

    return ReadMemorySlow(segment, offset, width, context, value, stack);
}

INLINE bool I386::WriteMemory(int segment, u32 offset, int width, u32 value, GT_Bus_Access_Context& context, bool stack)
{
    u32 size = (u32)width >> 3;

    if (likely((s64)((u64)offset + size - 1) <= m_write_limits[segment]))
    {
        u8* data = GetWriteHost(m_state.segments[segment].base + offset, size);

        if (likely(IsValidPointer(data)))
        {
            StoreHost(data, value, width);
            return true;
        }
    }

    return WriteMemorySlow(segment, offset, width, value, context, stack);
}

INLINE u8* I386::GetRMWHost(int segment, u32 offset, int width)
{
    u32 size = (u32)width >> 3;
    s64 end = (s64)((u64)offset + size - 1);

    if (end > m_read_limits[segment] || end > m_write_limits[segment])
        return NULL;

    u32 linear = m_state.segments[segment].base + offset;
    const u8* read = GetReadHost(linear, size);
    u8* write = GetWriteHost(linear, size);
    return read == write ? write : NULL;
}

// Stack accesses use the cached SS size. ESP only changes after the access succeeds
template<int width>
INLINE bool I386::StackPush(u32 value)
{
    u32 offset = m_state.registers[I386_REG_ESP].value - width / 8;

    if (!m_stack32)
        offset &= 0xFFFF;

    if (!WriteMemory(I386_SEGMENT_SS, offset, width, value, *m_bus_context, true))
        return false;

    if (m_stack32)
        m_state.registers[I386_REG_ESP].value = offset;
    else
        m_state.registers[I386_REG_ESP].low = (u16)offset;

    return true;
}

template<int width>
INLINE bool I386::StackPop(u32& value)
{
    u32 offset = m_stack32 ? m_state.registers[I386_REG_ESP].value : m_state.registers[I386_REG_ESP].low;

    if (!ReadMemory(I386_SEGMENT_SS, offset, width, *m_bus_context, value, true))
        return false;

    offset += width / 8;

    if (m_stack32)
        m_state.registers[I386_REG_ESP].value = offset;
    else
        m_state.registers[I386_REG_ESP].low = (u16)offset;

    return true;
}

#endif /* I386_MEMORY_INLINE_H */
