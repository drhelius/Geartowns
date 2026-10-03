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

#include "i386.h"
#include "memory.h"

void I386::SetSlowMemory(bool slow)
{
    m_slow_memory = slow;
    UpdateSegmentFastPaths();
}

void I386::SetUserMode(bool user)
{
    m_user_mode = user;
    CloseCodeWindow();

    for (int i = 0; i < I386_TLB_SIZE; i++)
    {
        InitPointer(m_tlb[i].read_page);
        InitPointer(m_tlb[i].write_page);
    }
}

void I386::UpdateSegmentFastPaths()
{
    // Code fetches only need the execute check and the limit
    // debug state never affects them
    const I386_Segment& code = m_segments[I386_SEGMENT_CS];

    m_default_size = (code.attributes & I386_SEGMENT_DEFAULT_32) != 0 ? 4 : 2;
    m_instruction_defaults.operand_size = m_default_size;
    m_instruction_defaults.address_size = m_default_size;
    m_stack32 = (m_segments[I386_SEGMENT_SS].attributes & I386_SEGMENT_DEFAULT_32) != 0;
    m_code_limit = -1;
    CloseCodeWindow();

    if ((code.attributes & I386_SEGMENT_PRESENT) != 0)
    {
        if (m_execution_mode != I386_MODE_PROTECTED)
            m_code_limit = code.limit;
        else if ((code.selector & 0xFFFC) != 0 && (code.attributes & I386_SEGMENT_EXECUTABLE) != 0 &&
            (code.attributes & (I386_SEGMENT_SYSTEM | I386_SEGMENT_EXPAND_DOWN)) == 0)
            m_code_limit = code.limit;
    }

    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
    {
        const I386_Segment& segment = m_segments[i];
        bool executable = (segment.attributes & I386_SEGMENT_EXECUTABLE) != 0;

        m_read_limits[i] = -1;
        m_write_limits[i] = -1;

        if (!m_slow_memory && (segment.attributes & I386_SEGMENT_PRESENT) != 0)
        {
            if (m_execution_mode != I386_MODE_PROTECTED)
                m_read_limits[i] = m_write_limits[i] = segment.limit;
            else if ((segment.selector & 0xFFFC) != 0 &&
                (segment.attributes & (I386_SEGMENT_SYSTEM | I386_SEGMENT_EXPAND_DOWN)) == 0)
            {
                if (!executable || (segment.attributes & I386_SEGMENT_READABLE) != 0)
                    m_read_limits[i] = segment.limit;

                if (!executable && (segment.attributes & I386_SEGMENT_WRITABLE) != 0)
                    m_write_limits[i] = segment.limit;
            }
        }

        u8 access_flags = 0;
        u16 flat_mask = I386_SEGMENT_PRESENT | I386_SEGMENT_SYSTEM | I386_SEGMENT_EXPAND_DOWN;

        if ((segment.selector & 0xFFFC) != 0 && (segment.attributes & flat_mask) == I386_SEGMENT_PRESENT &&
            segment.base == 0 && segment.limit == 0xFFFFFFFFU)
        {
            if (!executable || (segment.attributes & I386_SEGMENT_READABLE) != 0)
                access_flags |= k_i386_segment_fast_read;

            if (!executable && (segment.attributes & I386_SEGMENT_WRITABLE) != 0)
                access_flags |= k_i386_segment_fast_write;

            if (executable)
                access_flags |= k_i386_segment_fast_execute;
        }

        m_segment_access_flags[i] = access_flags;
    }
}

void I386::RefreshMemoryPointers()
{
    m_memory->PrepareCPUMap();
    m_read_pages = m_memory->GetCPUReadPages();
    m_write_pages = m_memory->GetCPUWritePages();
    m_memory_generation = m_memory->GetMapGeneration();
    CloseCodeWindow();

    for (int i = 0; i < I386_TLB_SIZE; i++)
    {
        InitPointer(m_tlb[i].read_page);
        InitPointer(m_tlb[i].write_page);
    }
}

bool I386::LogicalToLinearProtected(int segment, u32 offset, u32 size, bool write, bool stack, bool execute, u32& linear)
{
    if (segment < 0 || segment >= I386_SEGMENT_COUNT || size == 0)
        return RaiseException(stack ? 12 : 13, I386_EXCEPTION_FAULT, true, 0);

    const I386_Segment& state = m_segments[segment];
    u8 fault = stack || segment == I386_SEGMENT_SS ? 12 : 13;

    if ((state.selector & 0xFFFC) == 0 || (state.attributes & I386_SEGMENT_PRESENT) == 0)
        return RaiseException(fault, I386_EXCEPTION_FAULT, true, 0);

    bool executable = (state.attributes & I386_SEGMENT_EXECUTABLE) != 0;
    bool valid_type = true;

    if (execute)
        valid_type = executable;
    else if (write)
        valid_type = !executable && (state.attributes & I386_SEGMENT_WRITABLE) != 0;
    else
        valid_type = !executable || (state.attributes & I386_SEGMENT_READABLE) != 0;

    if ((state.attributes & I386_SEGMENT_SYSTEM) != 0 || !valid_type)
        return RaiseException(fault, I386_EXCEPTION_FAULT, true, 0);

    u64 end = (u64)offset + size - 1;
    bool in_limit = end <= 0xFFFFFFFFULL;

    if ((state.attributes & I386_SEGMENT_EXPAND_DOWN) != 0)
    {
        u32 upper_limit = (state.attributes & I386_SEGMENT_DEFAULT_32) != 0 ? 0xFFFFFFFFU : 0xFFFFU;
        in_limit = in_limit && offset > state.limit && end <= upper_limit;
    }
    else
        in_limit = in_limit && end <= state.limit;

    if (!in_limit)
        return RaiseException(fault, I386_EXCEPTION_FAULT, true, 0);

    linear = state.base + offset;
    return true;
}

// Way of the set holding a valid tag or -1
INLINE int I386::FindTLBWay(u32 set, u32 linear_page) const
{
    const TLBEntry* ways = &m_tlb[set * I386_TLB_WAYS];

    for (int way = 0; way < I386_TLB_WAYS; way++)
    {
        if ((ways[way].flags & k_i386_tlb_valid) != 0 && ways[way].linear_page == linear_page)
            return way;
    }

    return -1;
}

// A hit in any way of the set whose host page allows the access
// as the fast paths would use it from the most recent way
// It updates the replacement state like every translating hit
INLINE const I386::TLBEntry* I386::FindTLBHost(u32 linear, bool write)
{
    u32 linear_page = linear & 0xFFFFF000U;
    u32 set = (linear >> 12) & (I386_TLB_SETS - 1);
    const TLBEntry* ways = &m_tlb[set * I386_TLB_WAYS];

    for (u32 way = 0; way < I386_TLB_WAYS; way++)
    {
        const TLBEntry& entry = ways[way];
        const u8* page = write ? entry.write_page : entry.read_page;

        if (entry.linear_page == linear_page && IsValidPointer(page))
        {
            TouchTLBWay(set, way);
            return &entry;
        }
    }

    return NULL;
}

bool I386::ReadMemorySlow(int segment, u32 offset, int width, GT_Bus_Access_Context& context, u32& value, bool stack)
{
    u32 linear = 0;
    u32 bytes = (u32)width >> 3;

    if ((m_cr0 & 0x80000000U) != 0 && (s64)((u64)offset + bytes - 1) <= m_read_limits[segment])
    {
        linear = m_segments[segment].base + offset;

        if ((linear & 0xFFF) + bytes <= 0x1000)
        {
            u32 set = (linear >> 12) & (I386_TLB_SETS - 1);
            int way = FindTLBWay(set, linear & 0xFFFFF000U);
            const u8* page = way >= 0 ? m_tlb[set * I386_TLB_WAYS + way].read_page : NULL;

            if (IsValidPointer(page))
            {
                TouchTLBWay(set, (u32)way);
                value = LoadHost(page + (linear & 0xFFF), width);
                return true;
            }

            u32 physical = 0;

            if (!TranslatePagedWay(linear, false, context, physical, false, set, way))
                return false;

            value = ReadPhysical(physical, bytes, context);
            return true;
        }
    }

    if (!LogicalToLinear(segment, offset, bytes, false, stack, linear))
        return false;

    bool paging = (m_cr0 & 0x80000000U) != 0;
    bool single_page = (linear & 0xFFF) + bytes <= 0x1000;

    if (!paging || single_page)
    {
        u32 physical = linear;

        if (paging)
        {
            if (!TranslateLinear(linear, false, context, physical))
                return false;
        }

        value = ReadPhysical(physical, bytes, context);
    }
    else
    {
        value = 0;

        for (u32 i = 0; i < bytes; i++)
        {
            u32 physical = 0;

            if (!TranslateLinear(linear + i, false, context, physical))
                return false;

            value |= (u32)m_memory->Read8Physical(physical, context) << (i * 8);
        }
    }

    if (unlikely((m_debug_registers[7] & 0xFF) != 0))
        RecordDataBreakpoints(linear, bytes, false);

    return true;
}

bool I386::WriteMemorySlow(int segment, u32 offset, int width, u32 value, GT_Bus_Access_Context& context, bool stack)
{
    u32 linear = 0;
    u32 bytes = (u32)width >> 3;

    if ((m_cr0 & 0x80000000U) != 0 && (s64)((u64)offset + bytes - 1) <= m_write_limits[segment])
    {
        linear = m_segments[segment].base + offset;

        if ((linear & 0xFFF) + bytes <= 0x1000)
        {
            u32 set = (linear >> 12) & (I386_TLB_SETS - 1);
            int way = FindTLBWay(set, linear & 0xFFFFF000U);
            u8* page = way >= 0 ? m_tlb[set * I386_TLB_WAYS + way].write_page : NULL;

            if (IsValidPointer(page))
            {
                TouchTLBWay(set, (u32)way);
                StoreHost(page + (linear & 0xFFF), value, width);
                return true;
            }

            u32 physical = 0;

            if (!TranslatePagedWay(linear, true, context, physical, false, set, way))
                return false;

            WritePhysical(physical, value, bytes, context);
            return true;
        }
    }

    if (!LogicalToLinear(segment, offset, bytes, true, stack, linear))
        return false;

    bool paging = (m_cr0 & 0x80000000U) != 0;
    bool single_page = (linear & 0xFFF) + bytes <= 0x1000;

    if (!paging || single_page)
    {
        u32 physical = linear;

        if (paging)
        {
            if (!TranslateLinear(linear, true, context, physical))
                return false;
        }

        WritePhysical(physical, value, bytes, context);
    }
    else
    {
        u32 physical[4] = {};

        for (u32 i = 0; i < bytes; i++)
        {
            if (!TranslateLinear(linear + i, true, context, physical[i]))
                return false;
        }

        for (u32 i = 0; i < bytes; i++)
            m_memory->Write8Physical(physical[i], (u8)(value >> (i * 8)), context);
    }

    if (unlikely((m_debug_registers[7] & 0xFF) != 0))
        RecordDataBreakpoints(linear, bytes, true);

    return true;
}

void I386::RecordDataBreakpoints(u32 linear, u32 size, bool write)
{
    for (int i = 0; i < 4; i++)
    {
        u32 enable = (m_debug_registers[7] >> (i * 2)) & 3;
        u32 operation = (m_debug_registers[7] >> (16 + i * 4)) & 3;

        if (enable == 0 || operation == 0 || operation == 2 || (operation == 1 && !write))
            continue;

        u32 length_code = (m_debug_registers[7] >> (18 + i * 4)) & 3;
        u32 length = length_code == 0 ? 1 : length_code == 1 ? 2 : length_code == 3 ? 4 : 0;

        for (u32 access_byte = 0; access_byte < size && length != 0; access_byte++)
        {
            for (u32 breakpoint_byte = 0; breakpoint_byte < length; breakpoint_byte++)
            {
                if (linear + access_byte == m_debug_registers[i] + breakpoint_byte)
                {
                    m_debug_data_breakpoints |= 1U << i;
                    length = 0;
                    break;
                }
            }
        }
    }
}

bool I386::CheckMemoryAccess(int segment, u32 offset, u32 size, bool write, GT_Bus_Access_Context& context)
{
    u32 linear = 0;

    if (!LogicalToLinear(segment, offset, size, write, false, linear))
        return false;

    return CheckLinearAccess(linear, size, write, context);
}

bool I386::CheckLinearAccess(u32 linear, u32 size, bool write, GT_Bus_Access_Context& context, bool supervisor)
{
    if ((m_cr0 & 0x80000000U) == 0)
        return true;

    while (size != 0)
    {
        u32 physical = 0;

        if (!TranslateLinear(linear, write, context, physical, supervisor))
            return false;

        u32 bytes = MIN(size, 0x1000U - (linear & 0xFFF));
        linear += bytes;
        size -= bytes;
    }

    return true;
}

bool I386::ProbeMemory(int segment, u32 offset, u32 size, bool write, GT_Bus_Access_Context& context)
{
    u32 linear = 0;

    if (!LogicalToLinear(segment, offset, size, write, false, linear))
        return false;

    for (u32 i = 0; i < size; i++)
    {
        u32 physical = 0;

        if (!TranslateLinear(linear + i, write, context, physical))
            return false;

        if (!write)
            m_memory->Read8Physical(physical, context);
    }

    if (!write && unlikely((m_debug_registers[7] & 0xFF) != 0))
        RecordDataBreakpoints(linear, size, false);

    return true;
}

bool I386::ReadLinear(u32 linear, int width, GT_Bus_Access_Context& context, u32& value, bool supervisor)
{
    u32 bytes = (u32)width >> 3;
    value = 0;

    for (u32 i = 0; i < bytes; i++)
    {
        u32 physical = 0;

        if (!TranslateLinear(linear + i, false, context, physical, supervisor))
            return false;

        value |= (u32)m_memory->Read8Physical(physical, context) << (i * 8);
    }

    return true;
}

bool I386::WriteLinear(u32 linear, int width, u32 value, GT_Bus_Access_Context& context, bool supervisor)
{
    u32 physical[4] = {};
    u32 bytes = (u32)width >> 3;

    for (u32 i = 0; i < bytes; i++)
    {
        if (!TranslateLinear(linear + i, true, context, physical[i], supervisor))
            return false;
    }

    for (u32 i = 0; i < bytes; i++)
        m_memory->Write8Physical(physical[i], (u8)(value >> (i * 8)), context);

    return true;
}

bool I386::StackPushSized(u32 value, int width, GT_Bus_Access_Context& context, int write_width)
{
    if (write_width == 0)
        write_width = width;

    u32 bytes = (u32)width >> 3;
    u32 mask = GetStackAddressSize() == 32 ? 0xFFFFFFFFU : 0xFFFFU;
    u32 stack = (GetStackPointer() - bytes) & mask;

    if (!WriteMemory(I386_SEGMENT_SS, stack, write_width, value, context, true))
        return false;

    SetStackPointer(stack);
    return true;
}

bool I386::StackPopSized(u32& value, int width, GT_Bus_Access_Context& context, int read_width)
{
    if (read_width == 0)
        read_width = width;

    u32 stack = GetStackPointer();

    if (!ReadMemory(I386_SEGMENT_SS, stack, read_width, context, value, true))
        return false;

    u32 mask = GetStackAddressSize() == 32 ? 0xFFFFFFFFU : 0xFFFFU;
    SetStackPointer((stack + ((u32)width >> 3)) & mask);
    return true;
}

INLINE void I386::CacheTLBPages(TLBEntry& entry)
{
    UpdateUserMode();
    CloseCodeWindow();

    bool readable = !m_user_mode || (entry.flags & k_i386_tlb_user) != 0;
    bool writable = (entry.flags & k_i386_tlb_dirty) != 0 && (!m_user_mode ||
        (entry.flags & (k_i386_tlb_user | k_i386_tlb_writable)) == (k_i386_tlb_user | k_i386_tlb_writable));

    entry.read_page = readable ? m_read_pages[entry.physical_page >> 12] : NULL;
    entry.write_page = writable ? m_write_pages[entry.physical_page >> 12] : NULL;
}

// 32-entry, four-way set-associative TLB with tree pseudo-LRU replacement
// A write to a page whose entry is still clean walks the tables again and refills that way
bool I386::TranslateLinearPaged(u32 linear, bool write, GT_Bus_Access_Context& context, u32& physical, bool supervisor)
{
    u32 set = (linear >> 12) & (I386_TLB_SETS - 1);
    return TranslatePagedWay(linear, write, context, physical, supervisor, set, FindTLBWay(set, linear & 0xFFFFF000U));
}

// Translation once the set has been searched: way holds the tag, or -1 for a miss
NO_INLINE bool I386::TranslatePagedWay(u32 linear, bool write, GT_Bus_Access_Context& context, u32& physical,
    bool supervisor, u32 set, int way)
{
    if (unlikely(m_memory_generation != m_memory->GetMapGeneration()))
        RefreshMemoryPointers();

    bool user = !supervisor && (m_execution_mode == I386_MODE_VM86 || m_current_privilege_level == 3);
    u32 linear_page = linear & 0xFFFFF000U;
    TLBEntry* ways = &m_tlb[set * I386_TLB_WAYS];

    if (way >= 0)
    {
        TLBEntry& entry = ways[way];

        if (user && (((entry.flags & k_i386_tlb_user) == 0) || (write && (entry.flags & k_i386_tlb_writable) == 0)))
        {
            m_cr2 = linear;
            return RaiseException(14, I386_EXCEPTION_FAULT, true, 1 | (write ? 2 : 0) | 4);
        }

        if (!write || (entry.flags & k_i386_tlb_dirty) != 0)
        {
            TouchTLBWay(set, (u32)way);

            if (!IsValidPointer(entry.read_page))
                CacheTLBPages(entry);

            physical = entry.physical_page | (linear & 0xFFF);
            return true;
        }
    }

    u32 pde_address = (m_cr3 & 0xFFFFF000U) + ((linear >> 20) & 0xFFCU);
    u32 pde = ReadPhysical(pde_address, 4, context);

    if ((pde & 1) == 0)
    {
        m_cr2 = linear;
        return RaiseException(14, I386_EXCEPTION_FAULT, true, (write ? 2 : 0) | (user ? 4 : 0));
    }

    u32 pte_address = (pde & 0xFFFFF000U) + ((linear >> 10) & 0xFFCU);
    u32 pte = ReadPhysical(pte_address, 4, context);

    if ((pte & 1) == 0)
    {
        m_cr2 = linear;
        return RaiseException(14, I386_EXCEPTION_FAULT, true, (write ? 2 : 0) | (user ? 4 : 0));
    }

    if (user && (((pde & pte) & 4) == 0 || (write && ((pde & pte) & 2) == 0)))
    {
        m_cr2 = linear;
        return RaiseException(14, I386_EXCEPTION_FAULT, true, 1 | (write ? 2 : 0) | 4);
    }

    if ((pde & 0x20) == 0)
        WritePhysical(pde_address, pde | 0x20, 4, context);

    u32 new_pte = pte | 0x20 | (write ? 0x40 : 0);

    if (new_pte != pte)
        WritePhysical(pte_address, new_pte, 4, context);

    // A present tag is refilled in place, otherwise the tree selects the victim
    if (way < 0)
    {
        u8 plru = m_tlb_plru[set];
        way = (plru & 1) != 0 ? ((plru & 4) != 0 ? 3 : 2) : ((plru & 2) != 0 ? 1 : 0);
    }

    TLBEntry& entry = ways[way];

    m_tlb_used |= 1U << (set * I386_TLB_WAYS + way);

    entry.linear_page = linear_page;
    entry.physical_page = pte & 0xFFFFF000U;
    entry.flags = k_i386_tlb_valid;

    if (((pde & pte) & 4) != 0)
        entry.flags |= k_i386_tlb_user;

    if (((pde & pte) & 2) != 0)
        entry.flags |= k_i386_tlb_writable;

    if ((new_pte & 0x40) != 0)
        entry.flags |= k_i386_tlb_dirty;

    CacheTLBPages(entry);
    TouchTLBWay(set, (u32)way);

    physical = entry.physical_page | (linear & 0xFFF);
    return true;
}

// Writing CR3 clears every valid bit and, the replacement state
void I386::FlushTLB()
{
    u32 index = 0;

    for (u32 used = m_tlb_used; used != 0; used >>= 1, index++)
    {
        if ((used & 1) != 0)
            memset(&m_tlb[index], 0, sizeof(TLBEntry));
    }

    m_tlb_used = 0;
    ResetTLBReplacement();
}

// Cleared tree bits make way 3 the most recent one
void I386::ResetTLBReplacement()
{
    memset(m_tlb_plru, 0, sizeof(m_tlb_plru));
    memset(m_tlb_mru, I386_TLB_WAYS - 1, sizeof(m_tlb_mru));
    CloseCodeWindow();
}

// TR6 command
// C=0 writes the TR6 tag, V and D/U/W with the TR7 physical page into way TR7.REP of the set
// C=1 looks the tag up: a hit sets TR7 HT, REP and physical page and the TR6 attribute pairs, a miss clears HT
// For each X/X# pair, 10 means 1 and 01 means 0
// The undefined 00 and 11 take X on writes and are ignored by lookups
// Test operations leave the replacement state unchanged
void I386::TestTLB()
{
    u32 command = m_test_registers[0];
    u32 data = m_test_registers[1];
    u32 linear_page = command & 0xFFFFF000U;
    u32 set = (command >> 12) & (I386_TLB_SETS - 1);
    bool valid = (command & 0x800) != 0;

    if ((command & 1) == 0)
    {
        u32 way = (data >> 2) & 3;
        TLBEntry& entry = m_tlb[set * I386_TLB_WAYS + way];

        entry.linear_page = linear_page;
        entry.physical_page = data & 0xFFFFF000U;
        entry.flags = (valid ? k_i386_tlb_valid : 0) | ((command & 0x400) != 0 ? k_i386_tlb_dirty : 0) |
            ((command & 0x100) != 0 ? k_i386_tlb_user : 0) | ((command & 0x040) != 0 ? k_i386_tlb_writable : 0);
        InitPointer(entry.read_page);
        InitPointer(entry.write_page);
        CloseCodeWindow();

        if (valid)
            CacheTLBPages(entry);

        m_tlb_used |= 1U << (set * I386_TLB_WAYS + way);
        return;
    }

    static const u32 k_attribute_bits[3] = { 0x400, 0x100, 0x040 };
    static const u8 k_attribute_flags[3] = { k_i386_tlb_dirty, k_i386_tlb_user, k_i386_tlb_writable };

    for (u32 way = 0; way < I386_TLB_WAYS; way++)
    {
        const TLBEntry& entry = m_tlb[set * I386_TLB_WAYS + way];
        bool match = ((entry.flags & k_i386_tlb_valid) != 0) == valid && entry.linear_page == linear_page;

        for (int i = 0; i < 3 && match; i++)
        {
            bool attribute = (command & k_attribute_bits[i]) != 0;
            bool attribute_complement = (command & (k_attribute_bits[i] >> 1)) != 0;

            if (attribute != attribute_complement && ((entry.flags & k_attribute_flags[i]) != 0) != attribute)
                match = false;
        }

        if (!match)
            continue;

        u32 attributes = 0;

        for (int i = 0; i < 3; i++)
            attributes |= (entry.flags & k_attribute_flags[i]) != 0 ? k_attribute_bits[i] : k_attribute_bits[i] >> 1;

        m_test_registers[0] = (command & ~0x7E0U) | attributes;
        m_test_registers[1] = entry.physical_page | 0x10 | (way << 2);
        return;
    }

    m_test_registers[1] = data & ~0x10U;
}

// Opens the code window over the code page of EIP when it has host memory
// and, with paging, its TLB entry is the most recent way of the set
// The window starts at the page start, or at EIP 0 when the segment
// begins inside the page, and ends at the page end or the CS limit
// Otherwise it stays closed and the bytes come from FetchCodeSlow8
NO_INLINE void I386::OpenCodeWindow(u32 eip)
{
    CloseCodeWindow();

    if ((s64)eip > m_code_limit)
        return;

    u32 linear = m_segments[I386_SEGMENT_CS].base + eip;
    const u8* page;

    if ((m_cr0 & 0x80000000U) == 0)
        page = m_read_pages[linear >> 12];
    else
    {
        const TLBEntry& entry = GetRecentTLBEntry(linear);

        if (entry.linear_page != (linear & 0xFFFFF000U))
            return;

        page = entry.read_page;
    }

    if (!IsValidPointer(page))
        return;

    u32 page_offset = linear & 0xFFF;
    u32 before = MIN(eip, page_offset);
    u64 after = MIN((u64)m_code_limit - eip + 1, (u64)(0x1000 - page_offset));

    m_code_window = page + page_offset - before;
    m_code_window_eip = eip - before;
    m_code_window_size = before + (u32)after;
}

// Code bytes outside the fetch window: the instruction length limit, the CS checks and the translation of the next
// page, which then opens a new window. Pages without host memory are read through the bus one byte at a time
NO_INLINE bool I386::FetchCodeSlow8(InstructionContext& instruction, u8& value)
{
    u32 length = instruction.next_eip - instruction.start_eip;

    if (length >= GT_I386_MAX_INSTRUCTION_LENGTH)
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    u32 cursor = instruction.next_eip;
    u32 linear = 0;

    // The cached CS execute limit already proves the access when it covers the cursor
    if (likely((s64)cursor <= m_code_limit))
        linear = m_segments[I386_SEGMENT_CS].base + cursor;
    else if (!LogicalToLinear(I386_SEGMENT_CS, cursor, 1, false, false, linear, true))
        return false;

    GT_Bus_Access_Context& context = *m_bus_context;
    u32 physical = linear;
    const u8* page = NULL;

    // A TLB entry with a cached host page is valid for this privilege level, so no walk or fault is possible
    if ((m_cr0 & 0x80000000U) != 0)
    {
        const TLBEntry* entry = FindTLBHost(linear, false);

        if (IsValidPointer(entry))
            page = entry->read_page;
        else if (!TranslateLinear(linear, false, context, physical))
            return false;
    }

    if (!IsValidPointer(page))
        page = m_read_pages[physical >> 12];

    if (!IsValidPointer(page))
    {
        value = m_memory->Read8Physical(physical, context);
        instruction.next_eip++;
        return true;
    }

    const I386_Segment& code = m_segments[I386_SEGMENT_CS];
    u32 upper_limit = code.limit;

    if (m_execution_mode == I386_MODE_PROTECTED && (code.attributes & I386_SEGMENT_EXPAND_DOWN) != 0)
        upper_limit = (code.attributes & I386_SEGMENT_DEFAULT_32) != 0 ? 0xFFFFFFFFU : 0xFFFFU;

    u64 available = (u64)upper_limit - cursor + 1;
    available = MIN(available, (u64)(0x1000 - (linear & 0xFFF)));
    available = MIN(available, (u64)(GT_I386_MAX_INSTRUCTION_LENGTH - length));

    m_fetch_pointer = page + (linear & 0xFFF);
    m_fetch_remaining = (u32)available;

    value = *m_fetch_pointer++;
    m_fetch_remaining--;
    instruction.next_eip++;
    return true;
}

bool I386::TryPeekLogical(I386_Segment_Register segment, u32 offset, u8& value) const
{
    if (segment < 0 || segment >= I386_SEGMENT_COUNT)
        return false;

    const I386_Segment& state = m_segments[segment];

    if (!IsValidSegmentOffset(state, offset, m_execution_mode == I386_MODE_PROTECTED))
        return false;

    return TryPeekLinear(state.base + offset, value);
}

bool I386::TryPeekLogical(u16 selector, u32 offset, u8& value) const
{
    const I386_Segment* state = FindSegment(selector);

    if (!IsValidPointer(state) || !IsValidSegmentOffset(*state, offset, m_execution_mode == I386_MODE_PROTECTED))
        return false;

    return TryPeekLinear(state->base + offset, value);
}

bool I386::TryPeekCode(u32 eip, u8& value) const
{
    return TryPeekLogical(I386_SEGMENT_CS, eip, value);
}

bool I386::TryPeekLinear(u32 linear, u8& value) const
{
    u32 physical = 0;

    if (!TryTranslateLinear(linear, physical) || !IsValidPointer(m_memory))
        return false;

    return m_memory->TryPeekPhysical(physical, value);
}

NO_INLINE bool I386::TranslatePagedPassive(u32 linear, u32& physical) const
{
    u32 pde_address = 0;
    u32 pte_address = 0;
    u32 page_flags = 0;
    char reason[GT_DEBUG_MEMORY_REASON_SIZE];

    return TranslateLinearForDebugger(linear, physical, pde_address, pte_address, page_flags, reason, sizeof(reason));
}

bool I386::TranslateLinearForDebugger(u32 linear, u32& physical, u32& pde_address, u32& pte_address, u32& page_flags,
    char* reason, size_t reason_size) const
{
    if ((m_cr0 & 0x80000000U) == 0)
    {
        physical = linear;
        pde_address = 0;
        pte_address = 0;
        page_flags = 0;
        reason[0] = 0;
        return true;
    }

    u32 pde = 0;
    pde_address = (m_cr3 & 0xFFFFF000U) + ((linear >> 20) & 0xFFCU);

    if (!ReadPhysical32Passive(pde_address, pde) || (pde & 1) == 0)
    {
        strncpy_fit(reason, "Page-directory entry is unavailable", reason_size);
        return false;
    }

    u32 pte = 0;
    pte_address = (pde & 0xFFFFF000U) + ((linear >> 10) & 0xFFCU);

    if (!ReadPhysical32Passive(pte_address, pte) || (pte & 1) == 0)
    {
        strncpy_fit(reason, "Page-table entry is unavailable", reason_size);
        return false;
    }

    physical = (pte & 0xFFFFF000U) | (linear & 0xFFFU);
    page_flags = (pde & 0xFFFU) | ((pte & 0xFFFU) << 12);
    reason[0] = 0;
    return true;
}

bool I386::ReadPhysical32Passive(u32 physical, u32& value) const
{
    if (!IsValidPointer(m_memory))
        return false;

    const u8* span = m_memory->GetPhysicalReadSpan(physical, 4);

    if (IsValidPointer(span))
    {
        value = read_u32_le(span);
        return true;
    }

    u8 data[4];

    if (!m_memory->TryPeekPhysicalBlock(physical, data, sizeof(data)))
        return false;

    value = read_u32_le(data);
    return true;
}

bool I386::DebugTranslateLogical(I386_Segment_Register segment, u32 offset,
    GT_Debug_Memory_Translation& translation) const
{
    if (segment < 0 || segment >= I386_SEGMENT_COUNT)
    {
        strncpy_fit(translation.reason, "Invalid segment register", sizeof(translation.reason));
        return false;
    }

    const I386_Segment& state = m_segments[segment];

    translation.segment = state.selector;
    translation.offset = offset;
    translation.segment_base = state.base;
    translation.segment_limit = state.limit;
    translation.logical_valid = IsValidSegmentOffset(state, offset, m_execution_mode == I386_MODE_PROTECTED);

    if (!translation.logical_valid)
    {
        strncpy_fit(translation.reason, "Offset outside segment limits", sizeof(translation.reason));
        return false;
    }

    translation.linear = state.base + offset;
    translation.linear_valid = true;
    return DebugTranslateLinear(translation.linear, translation);
}

bool I386::DebugTranslateLogical(u16 selector, u32 offset, GT_Debug_Memory_Translation& translation) const
{
    const I386_Segment* state = FindSegment(selector);

    if (!IsValidPointer(state))
    {
        strncpy_fit(translation.reason, "Selector is not in a cached segment", sizeof(translation.reason));
        return false;
    }

    translation.segment = selector;
    translation.offset = offset;
    translation.segment_base = state->base;
    translation.segment_limit = state->limit;
    translation.logical_valid = IsValidSegmentOffset(*state, offset, m_execution_mode == I386_MODE_PROTECTED);

    if (!translation.logical_valid)
    {
        strncpy_fit(translation.reason, "Offset outside segment limits", sizeof(translation.reason));
        return false;
    }

    translation.linear = state->base + offset;
    translation.linear_valid = true;
    return DebugTranslateLinear(translation.linear, translation);
}

bool I386::IsValidSegmentOffset(const I386_Segment& segment, u32 offset, bool protected_mode)
{
    if ((segment.attributes & I386_SEGMENT_PRESENT) == 0)
        return false;

    if (protected_mode && (segment.attributes & I386_SEGMENT_EXPAND_DOWN) != 0)
    {
        u32 upper_limit = (segment.attributes & I386_SEGMENT_DEFAULT_32) != 0 ? 0xFFFFFFFFU : 0xFFFFU;
        return offset > segment.limit && offset <= upper_limit;
    }

    return offset <= segment.limit;
}

bool I386::DebugTranslateLinear(u32 linear, GT_Debug_Memory_Translation& translation) const
{
    translation.linear = linear;
    translation.linear_valid = true;

    if (!TranslateLinearForDebugger(linear, translation.physical, translation.page_directory_entry,
        translation.page_table_entry, translation.page_flags, translation.reason, sizeof(translation.reason)))
        return false;

    translation.physical_valid = true;
    return IsValidPointer(m_memory) && m_memory->DebugTranslatePhysical(translation.physical, translation);
}

const I386_Segment* I386::FindSegment(u16 selector) const
{
    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
    {
        if (m_segments[i].selector == selector)
            return &m_segments[i];
    }

    return NULL;
}
