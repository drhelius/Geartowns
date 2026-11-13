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
#include "../system/memory.h"

void I386::SetSlowMemory(bool slow)
{
    m_slow_memory = slow;
    UpdateSegmentFastPaths();
}

// Host pages user code may use the supervisor may too, so only those the supervisor holds beyond them go, on the way
// to user mode
void I386::SetUserMode(bool user)
{
    m_user_mode = user;
    UpdateTLBMasks();
    CloseCodeWindow();

    if (user)
        DropTLBKernelPages();

    UpdateAlignmentCheck();
}

void I386::UpdateTLBMasks()
{
    m_tlb_read_mask = m_user_mode ? k_i386_tlb_user : 0;
    m_tlb_write_mask = k_i386_tlb_dirty | (m_user_mode ? k_i386_tlb_user | k_i386_tlb_writable :
        m_write_protect ? k_i386_tlb_writable : 0);
}

void I386::UpdateWriteProtect()
{
    bool write_protect = m_i486 && (m_state.cr0 & 0x00010000U) != 0;

    if (write_protect != m_write_protect)
    {
        m_write_protect = write_protect;
        UpdateTLBMasks();
        CloseCodeWindow();
        DropTLBHostPages();
    }
}

void I386::LoadCR0(u32 value)
{
    u32 changed = m_state.cr0 ^ ((value & m_cr0_writable) | m_cr0_fixed);
    m_state.cr0 ^= changed;
    UpdateWriteProtect();
    UpdateAlignmentCheck();

    // CD turns the 486's cache off and on
    if (unlikely(m_i486_timing) && (changed & 0x40000000U) != 0)
        UpdateBusTiming();
}

void I386::UpdateSegmentFastPaths()
{
    // Code fetches only check the CS limit, its type was checked when it was loaded in protected mode, so code runs on
    // with the real mode CS after setting PE. Debug state never affects them
    const I386_Segment& code = m_state.segments[I386_SEGMENT_CS];

    m_default_size = (code.attributes & I386_SEGMENT_DEFAULT_32) != 0 ? 4 : 2;
    m_instruction_defaults.operand_size = m_default_size;
    m_instruction_defaults.address_size = m_default_size;
    m_stack32 = GetStackAddressSize() == 32;
    m_code_limit = -1;
    CloseCodeWindow();

    if ((code.attributes & (I386_SEGMENT_PRESENT | I386_SEGMENT_EXPAND_DOWN)) == I386_SEGMENT_PRESENT)
        m_code_limit = code.limit;

    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
        UpdateSegmentFastPath(i);
}

// CS and SS also set the operand and stack sizes, the code window and the dynarec block key
void I386::UpdateSegmentFastPaths(const I386_Segment& segment)
{
    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
    {
        if (&segment == &m_state.segments[i] && i != I386_SEGMENT_CS && i != I386_SEGMENT_SS)
        {
            UpdateSegmentFastPath(i);
            return;
        }
    }

    UpdateSegmentFastPaths();
}

void I386::UpdateSegmentFastPath(int index)
{
    const I386_Segment& segment = m_state.segments[index];
    bool executable = (segment.attributes & I386_SEGMENT_EXECUTABLE) != 0;

    m_read_limits[index] = -1;
    m_write_limits[index] = -1;

    if (!m_slow_memory && (segment.attributes & I386_SEGMENT_PRESENT) != 0)
    {
        if (m_state.execution_mode != I386_MODE_PROTECTED)
        {
            if ((segment.attributes & I386_SEGMENT_EXPAND_DOWN) == 0)
                m_read_limits[index] = m_write_limits[index] = segment.limit;
        }
        else if ((segment.attributes & (I386_SEGMENT_SYSTEM | I386_SEGMENT_EXPAND_DOWN)) == 0)
        {
            if (!executable || (segment.attributes & I386_SEGMENT_READABLE) != 0)
                m_read_limits[index] = segment.limit;

            if (!executable && (segment.attributes & I386_SEGMENT_WRITABLE) != 0)
                m_write_limits[index] = segment.limit;
        }
    }

    u8 access_flags = 0;
    u16 flat_mask = I386_SEGMENT_PRESENT | I386_SEGMENT_SYSTEM | I386_SEGMENT_EXPAND_DOWN;

    if ((segment.attributes & flat_mask) == I386_SEGMENT_PRESENT && segment.base == 0 && segment.limit == 0xFFFFFFFFU)
    {
        if (!executable || (segment.attributes & I386_SEGMENT_READABLE) != 0)
            access_flags |= k_i386_segment_fast_read;

        if (!executable && (segment.attributes & I386_SEGMENT_WRITABLE) != 0)
            access_flags |= k_i386_segment_fast_write;

        if (executable)
            access_flags |= k_i386_segment_fast_execute;
    }

    m_segment_access_flags[index] = access_flags;
}

void I386::RefreshMemoryPointers()
{
    m_memory->PrepareCPUMap();
    m_read_pages = m_memory->GetCPUReadPages();
    m_write_pages = m_memory->GetCPUWritePages();
    m_bus_page_class = m_memory->GetCPUPageBus();
    m_memory_generation = m_memory->GetMapGeneration();
    CloseCodeWindow();

    DropTLBHostPages();
}

bool I386::LogicalToLinearProtected(int segment, u32 offset, u32 size, bool write, bool stack, bool execute, u32& linear)
{
    if (segment < 0 || segment >= I386_SEGMENT_COUNT || size == 0)
        return RaiseException(stack ? 12 : 13, I386_EXCEPTION_FAULT, true, 0);

    const I386_Segment& state = m_state.segments[segment];
    u8 fault = stack || segment == I386_SEGMENT_SS ? 12 : 13;

    // Only a protected-mode null load leaves the cache unusable: a null selector loaded in real mode works after PE
    if ((state.attributes & I386_SEGMENT_PRESENT) == 0)
        return RaiseException(fault, I386_EXCEPTION_FAULT, true, 0);

    if (!execute)
    {
        bool executable = (state.attributes & I386_SEGMENT_EXECUTABLE) != 0;
        bool valid_type = write ? !executable && (state.attributes & I386_SEGMENT_WRITABLE) != 0 :
            !executable || (state.attributes & I386_SEGMENT_READABLE) != 0;

        if (!valid_type)
            return RaiseException(fault, I386_EXCEPTION_FAULT, true, 0);
    }

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

#if !defined(GT_DISABLE_DYNAREC)
// The block leaves after the site's instruction with what its Exit would count: written registers, code lines, bus run
void I386::LeaveDynarecSite(const DynarecSite& site)
{
    m_state.eip = site.eip;

    if (site.written >= 0)
        m_486_written = (u8)site.written;

    for (u32 i = 0; i < site.line_count; i++)
    {
        u32 line = site.line_first + i;
        u32& tag = m_bus_lines[line & m_bus_line_mask];

        if (tag != line)
        {
            tag = line;
            m_bus_fraction += (s32)site.line_fill;
        }
    }

    if (IsValidPointer(site.run))
    {
        const DynarecBusRun& run = *site.run;
        u32 level = m_bus_queue;

        // Code the 486's cache serves keeps no queue level
        if (run.cost == 0)
            m_bus_fraction += (s32)run.add;
        else
        {
            m_bus_fraction += GetDynarecBusTime(run, level);
            m_bus_queue = (u32)MIN(MAX((s32)level + run.offset, (s32)run.low), (s32)run.high);
        }
    }

    m_dynarec_leave_steps = site.steps;
    m_dynarec_leave_clocks = site.clocks;
}
#endif

// A hit whose host page allows the access, as the fast paths would use it
INLINE const I386::TLBEntry* I386::FindTLBHost(u32 linear, bool write)
{
    const TLBEntry& entry = GetTLBEntry(linear);
    const u8* page = write ? entry.write_page : entry.read_page;

    return entry.linear_page == (linear & 0xFFFFF000U) && IsValidPointer(page) ? &entry : NULL;
}

bool I386::ReadMemorySlow(int segment, u32 offset, int width, GT_Bus_Access_Context& context, u32& value, bool stack)
{
    u32 linear = 0;
    u32 bytes = (u32)width >> 3;

    if ((m_state.cr0 & 0x80000000U) != 0 && (s64)((u64)offset + bytes - 1) <= m_read_limits[segment])
    {
        linear = m_state.segments[segment].base + offset;

        if ((linear & 0xFFF) + bytes <= 0x1000)
        {
            const TLBEntry& entry = GetTLBEntry(linear);
            const u8* page = IsTLBHit(entry, linear) ? entry.read_page : NULL;

            if (IsValidPointer(page))
            {
                value = LoadHost(page + (linear & 0xFFF), width);
                return true;
            }

            u32 physical = 0;

            if (!TranslateLinearPaged(linear, false, context, physical, false))
                return false;

            value = ReadPhysical(physical, bytes, context);
            return true;
        }
    }

    if (!LogicalToLinear(segment, offset, bytes, false, stack, linear))
        return false;

    if (unlikely(m_alignment_check) && (linear & (bytes - 1)) != 0)
        return RaiseException(17, I386_EXCEPTION_FAULT, true, 0);

    bool paging = (m_state.cr0 & 0x80000000U) != 0;
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

    if (unlikely((m_state.debug_registers[7] & 0xFF) != 0))
        RecordDataBreakpoints(linear, bytes, false);

    if (unlikely(m_debugger_memory_checks))
        RecordDebuggerAccess(linear, bytes, false);

    return true;
}

bool I386::WriteMemorySlow(int segment, u32 offset, int width, u32 value, GT_Bus_Access_Context& context, bool stack)
{
    u32 linear = 0;
    u32 bytes = (u32)width >> 3;

    if ((m_state.cr0 & 0x80000000U) != 0 && (s64)((u64)offset + bytes - 1) <= m_write_limits[segment])
    {
        linear = m_state.segments[segment].base + offset;

        if ((linear & 0xFFF) + bytes <= 0x1000)
        {
            const TLBEntry& entry = GetTLBEntry(linear);
            u8* page = IsTLBHit(entry, linear) ? entry.write_page : NULL;

            if (IsValidPointer(page))
            {
                StoreHost(page + (linear & 0xFFF), value, width);
                return true;
            }

            u32 physical = 0;

            if (!TranslateLinearPaged(linear, true, context, physical, false))
                return false;

            WritePhysical(physical, value, bytes, context);
            return true;
        }
    }

    if (!LogicalToLinear(segment, offset, bytes, true, stack, linear))
        return false;

    if (unlikely(m_alignment_check) && (linear & (bytes - 1)) != 0)
        return RaiseException(17, I386_EXCEPTION_FAULT, true, 0);

    bool paging = (m_state.cr0 & 0x80000000U) != 0;
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

    if (unlikely((m_state.debug_registers[7] & 0xFF) != 0))
        RecordDataBreakpoints(linear, bytes, true);

    if (unlikely(m_debugger_memory_checks))
        RecordDebuggerAccess(linear, bytes, true);

    return true;
}

void I386::RecordDataBreakpoints(u32 linear, u32 size, bool write)
{
    for (int i = 0; i < 4; i++)
    {
        u32 enable = (m_state.debug_registers[7] >> (i * 2)) & 3;
        u32 operation = (m_state.debug_registers[7] >> (16 + i * 4)) & 3;

        if (enable == 0 || operation == 0 || operation == 2 || (operation == 1 && !write))
            continue;

        u32 length_code = (m_state.debug_registers[7] >> (18 + i * 4)) & 3;
        u32 length = length_code == 0 ? 1 : length_code == 1 ? 2 : length_code == 3 ? 4 : 0;
        u32 address = m_state.debug_registers[i] & ~(length - 1);

        for (u32 access_byte = 0; access_byte < size && length != 0; access_byte++)
        {
            for (u32 breakpoint_byte = 0; breakpoint_byte < length; breakpoint_byte++)
            {
                if (linear + access_byte == address + breakpoint_byte)
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
    if ((m_state.cr0 & 0x80000000U) == 0)
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

    if (!write && unlikely((m_state.debug_registers[7] & 0xFF) != 0))
        RecordDataBreakpoints(linear, size, false);

    return true;
}

bool I386::ReadLinear(u32 linear, int width, GT_Bus_Access_Context& context, u32& value, bool supervisor)
{
    u32 bytes = (u32)width >> 3;
    value = 0;

    if ((linear & 0xFFF) + bytes <= 0x1000)
    {
        u32 physical = 0;

        if (!TranslateLinear(linear, false, context, physical, supervisor))
            return false;

        value = ReadPhysical(physical, bytes, context);
        return true;
    }

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

    bool readable = (entry.flags & m_tlb_read_mask) == m_tlb_read_mask;
    bool writable = (entry.flags & m_tlb_write_mask) == m_tlb_write_mask;

    entry.read_page = readable ? m_read_pages[entry.physical_page >> 12] : NULL;
    entry.write_page = writable ? m_write_pages[entry.physical_page >> 12] : NULL;

    // What the supervisor holds that a user may not read or write goes on the way to user mode (SetUserMode)
    u8 user_write = k_i386_tlb_user | k_i386_tlb_writable;

    if (m_user_mode || ((!IsValidPointer(entry.read_page) || (entry.flags & k_i386_tlb_user) != 0) &&
        (!IsValidPointer(entry.write_page) || (entry.flags & user_write) == user_write)))
        return;

    if (m_tlb_kernel_count < I386_TLB_KERNEL_SIZE)
        m_tlb_kernel[m_tlb_kernel_count++] = (u16)(&entry - m_tlb);
    else
        m_tlb_kernel_count = I386_TLB_KERNEL_SIZE + 1;
}

// On a hit, the host pages again when the read page is missing, or the write page of a page that has one: an entry
// cached in user mode lacks it where only the supervisor may write (386, 486 with WP clear)
INLINE void I386::RefreshTLBPages(TLBEntry& entry, bool write)
{
    if (!IsValidPointer(entry.read_page) || (write && !IsValidPointer(entry.write_page) &&
        IsValidPointer(m_write_pages[entry.physical_page >> 12])))
        CacheTLBPages(entry);
}

// Walks cost no clocks and hits change nothing, so neither the 386's 32 entries and random replacement (386 DX PRM
// 1990 5.3.5) nor the 486's 4 ways and pseudo-LRU (PRM486 10.6.2) would show but in what a bigger table keeps longer:
// 512 entries, one per linear page's low bits. A write to a page whose entry is still clean walks the tables again
// and refills the entry
NO_INLINE bool I386::TranslateLinearPaged(u32 linear, bool write, GT_Bus_Access_Context& context, u32& physical,
    bool supervisor)
{
    if (unlikely(m_memory_generation != m_memory->GetMapGeneration()))
        RefreshMemoryPointers();

    bool user = !supervisor && (m_state.execution_mode == I386_MODE_VM86 || m_state.current_privilege_level == 3);
    u32 linear_page = linear & 0xFFFFF000U;
    TLBEntry& entry = GetTLBEntry(linear);

    if (IsTLBHit(entry, linear))
    {
        if ((user && (entry.flags & k_i386_tlb_user) == 0) ||
            (write && (entry.flags & k_i386_tlb_writable) == 0 && (user || m_write_protect)))
        {
            m_state.cr2 = linear;
            return RaiseException(14, I386_EXCEPTION_FAULT, true, 1 | (write ? 2 : 0) | (user ? 4 : 0));
        }

        if (!write || (entry.flags & k_i386_tlb_dirty) != 0)
        {
            RefreshTLBPages(entry, write);
            physical = entry.physical_page | (linear & 0xFFF);
            return true;
        }
    }

    u32 pde_address = (m_state.cr3 & 0xFFFFF000U) + ((linear >> 20) & 0xFFCU);
    u32 pde = ReadPhysical(pde_address, 4, context);

    if ((pde & 1) == 0)
    {
        m_state.cr2 = linear;
        return RaiseException(14, I386_EXCEPTION_FAULT, true, (write ? 2 : 0) | (user ? 4 : 0));
    }

    u32 pte_address = (pde & 0xFFFFF000U) + ((linear >> 10) & 0xFFCU);
    u32 pte = ReadPhysical(pte_address, 4, context);

    if ((pte & 1) == 0)
    {
        m_state.cr2 = linear;
        return RaiseException(14, I386_EXCEPTION_FAULT, true, (write ? 2 : 0) | (user ? 4 : 0));
    }

    u32 rights = pde & pte;

    if ((user && (rights & 4) == 0) || (write && (rights & 2) == 0 && (user || m_write_protect)))
    {
        m_state.cr2 = linear;
        return RaiseException(14, I386_EXCEPTION_FAULT, true, 1 | (write ? 2 : 0) | (user ? 4 : 0));
    }

    if ((pde & 0x20) == 0)
        WritePhysical(pde_address, pde | 0x20, 4, context);

    u32 new_pte = pte | 0x20 | (write ? 0x40 : 0);

    if (new_pte != pte)
        WritePhysical(pte_address, new_pte, 4, context);

    if ((entry.flags & k_i386_tlb_valid) == 0 && m_tlb_fill_count < I386_TLB_SIZE)
        m_tlb_fills[m_tlb_fill_count++] = (u16)(&entry - m_tlb);

    entry.linear_page = linear_page;
    entry.physical_page = pte & 0xFFFFF000U;
    entry.flags = k_i386_tlb_valid;

    if ((rights & 4) != 0)
        entry.flags |= k_i386_tlb_user;

    if ((rights & 2) != 0)
        entry.flags |= k_i386_tlb_writable;

    if ((new_pte & 0x40) != 0)
        entry.flags |= k_i386_tlb_dirty;

    if (m_i486)
        entry.flags |= ((pte & 0x08) != 0 ? k_i386_tlb_pwt : 0) | ((pte & 0x10) != 0 ? k_i386_tlb_pcd : 0);

    CacheTLBPages(entry);
    FillTestTLB(entry.linear_page, entry.physical_page, entry.flags);

    physical = entry.physical_page | (linear & 0xFFF);
    return true;
}

// A REP batch reaches pages without a host pointer only through hits that need no table walk and raise no fault, so it
// leaves the TLB as the slow path's TranslateLinearPaged would
bool I386::TranslateRepeatPage(u32 linear, u32 size, bool write, u32& physical)
{
    if ((linear & 0xFFF) + size > 0x1000)
        return false;

    if ((m_state.cr0 & 0x80000000U) == 0)
    {
        physical = linear;
        return true;
    }

    if (m_memory_generation != m_memory->GetMapGeneration())
        return false;

    TLBEntry& entry = GetTLBEntry(linear);
    bool user = m_state.execution_mode == I386_MODE_VM86 || m_state.current_privilege_level == 3;

    if ((entry.flags & k_i386_tlb_valid) == 0 || entry.linear_page != (linear & 0xFFFFF000U) ||
        (write && (entry.flags & k_i386_tlb_dirty) == 0))
        return false;

    if ((user && (entry.flags & k_i386_tlb_user) == 0) ||
        (write && (entry.flags & k_i386_tlb_writable) == 0 && (user || m_write_protect)))
        return false;

    RefreshTLBPages(entry, write);
    physical = entry.physical_page | (linear & 0xFFF);
    return true;
}

// Writing CR3 clears every valid bit, and the test array's with its replacement state
void I386::FlushTLB()
{
    if (m_tlb_fill_count < I386_TLB_SIZE)
    {
        for (u32 i = 0; i < m_tlb_fill_count; i++)
            memset(&m_tlb[m_tlb_fills[i]], 0, sizeof(TLBEntry));
    }
    else
        memset(m_tlb, 0, sizeof(m_tlb));

    m_tlb_fill_count = 0;
    m_tlb_kernel_count = 0;
    memset(m_tlb_test, 0, sizeof(m_tlb_test));
    memset(m_tlb_test_plru, 0, sizeof(m_tlb_test_plru));
    CloseCodeWindow();
}

// The host pages of the entries filled since the last flush, all of them when the list overflowed
void I386::DropTLBHostPages()
{
    bool listed = m_tlb_fill_count < I386_TLB_SIZE;
    u32 count = listed ? m_tlb_fill_count : I386_TLB_SIZE;

    for (u32 i = 0; i < count; i++)
    {
        TLBEntry& entry = m_tlb[listed ? m_tlb_fills[i] : i];
        InitPointer(entry.read_page);
        InitPointer(entry.write_page);
    }

    m_tlb_kernel_count = 0;
}

// The host pages the supervisor cached beyond a user's rights, all of them when they overflowed the list
void I386::DropTLBKernelPages()
{
    if (m_tlb_kernel_count > I386_TLB_KERNEL_SIZE)
    {
        DropTLBHostPages();
        return;
    }

    for (u32 i = 0; i < m_tlb_kernel_count; i++)
    {
        TLBEntry& entry = m_tlb[m_tlb_kernel[i]];
        InitPointer(entry.read_page);
        InitPointer(entry.write_page);
    }

    m_tlb_kernel_count = 0;
}

// Whether the test array, which keeps the hardware's 32 entries, held the page: INVLPG takes a clock more for a hit
bool I386::InvalidateTLBPage(u32 linear)
{
    TLBEntry& entry = GetTLBEntry(linear);
    u32 set = (linear >> 12) & (I386_TLB_TEST_SETS - 1);
    int way = FindTestTLBWay(set, linear & 0xFFFFF000U);

    if (way >= 0)
        memset(&m_tlb_test[set * I386_TLB_TEST_WAYS + way], 0, sizeof(TLBEntry));

    if (IsTLBHit(entry, linear))
    {
        memset(&entry, 0, sizeof(TLBEntry));
        CloseCodeWindow();
    }

    return way >= 0;
}

// The test registers see the hardware's organization of what the table holds: 8 sets of 4 ways that every fill and
// test write goes into, under a tree pseudo-LRU that fills and test accesses move. Hits don't, as they change nothing
void I386::FillTestTLB(u32 linear_page, u32 physical_page, u8 flags)
{
    u32 set = (linear_page >> 12) & (I386_TLB_TEST_SETS - 1);
    int way = FindTestTLBWay(set, linear_page);

    if (way < 0)
        way = GetTestTLBVictim(set);

    TLBEntry& entry = m_tlb_test[set * I386_TLB_TEST_WAYS + way];
    entry.linear_page = linear_page;
    entry.physical_page = physical_page;
    entry.flags = flags;
    TouchTestTLBWay(set, (u32)way);
}

// A TR6 write: the way it names in the test array, and the table's entry for the page, which then translates
void I386::WriteTestTLB(u32 set, u32 way, u32 linear_page, u32 physical_page, u8 flags)
{
    TLBEntry& test = m_tlb_test[set * I386_TLB_TEST_WAYS + way];
    test.linear_page = linear_page;
    test.physical_page = physical_page;
    test.flags = flags;

    TLBEntry& entry = GetTLBEntry(linear_page);

    if ((flags & k_i386_tlb_valid) == 0 && !IsTLBHit(entry, linear_page))
        return;

    if ((entry.flags & k_i386_tlb_valid) == 0 && m_tlb_fill_count < I386_TLB_SIZE)
        m_tlb_fills[m_tlb_fill_count++] = (u16)(&entry - m_tlb);

    entry.linear_page = linear_page;
    entry.physical_page = physical_page;
    entry.flags = flags;
    InitPointer(entry.read_page);
    InitPointer(entry.write_page);
    CloseCodeWindow();

    if ((flags & k_i386_tlb_valid) != 0)
        CacheTLBPages(entry);
}

// A valid test entry with the tag, or -1
int I386::FindTestTLBWay(u32 set, u32 linear_page) const
{
    for (int way = 0; way < I386_TLB_TEST_WAYS; way++)
    {
        const TLBEntry& entry = m_tlb_test[set * I386_TLB_TEST_WAYS + way];

        if ((entry.flags & k_i386_tlb_valid) != 0 && entry.linear_page == linear_page)
            return way;
    }

    return -1;
}

// The 486 fills an invalid way first, then the one the tree points at
int I386::GetTestTLBVictim(u32 set) const
{
    if (m_i486)
    {
        for (int way = 0; way < I386_TLB_TEST_WAYS; way++)
        {
            if ((m_tlb_test[set * I386_TLB_TEST_WAYS + way].flags & k_i386_tlb_valid) == 0)
                return way;
        }
    }

    u8 plru = m_tlb_test_plru[set];
    return (plru & 1) != 0 ? ((plru & 4) != 0 ? 3 : 2) : ((plru & 2) != 0 ? 1 : 0);
}

// A way filled, or written or found by the 486's test registers, points the tree away from it: B0 selects the half,
// B1 and B2 the way in it
void I386::TouchTestTLBWay(u32 set, u32 way)
{
    static const u8 k_plru_keep_mask[I386_TLB_TEST_WAYS] = { 0x04, 0x04, 0x02, 0x02 };
    static const u8 k_plru_way_bits[I386_TLB_TEST_WAYS] = { 0x03, 0x01, 0x04, 0x00 };

    m_tlb_test_plru[set] = (m_tlb_test_plru[set] & k_plru_keep_mask[way]) | k_plru_way_bits[way];
}

// TR6 command, on the test array (FillTestTLB)
// C=0 writes the TR6 tag, V and D/U/W with the TR7 physical page into way TR7.REP of the set
// C=1 looks the tag up: a hit sets TR7 HT, REP and physical page and the TR6 attribute pairs, a miss clears HT
// For each X/X# pair, 10 means 1 and 01 means 0
// The undefined 00 and 11 take X on writes and are ignored by lookups
// Test operations leave the replacement state unchanged
void I386::TestTLB()
{
    u32 command = m_state.test_registers[0];
    u32 data = m_state.test_registers[1];
    u32 linear_page = command & 0xFFFFF000U;
    u32 set = (command >> 12) & (I386_TLB_TEST_SETS - 1);
    bool valid = (command & 0x800) != 0;

    if ((command & 1) == 0)
    {
        u8 flags = (valid ? k_i386_tlb_valid : 0) | ((command & 0x400) != 0 ? k_i386_tlb_dirty : 0) |
            ((command & 0x100) != 0 ? k_i386_tlb_user : 0) | ((command & 0x040) != 0 ? k_i386_tlb_writable : 0);
        WriteTestTLB(set, (data >> 2) & 3, linear_page, data & 0xFFFFF000U, flags);
        return;
    }

    static const u32 k_attribute_bits[3] = { 0x400, 0x100, 0x040 };
    static const u8 k_attribute_flags[3] = { k_i386_tlb_dirty, k_i386_tlb_user, k_i386_tlb_writable };

    for (u32 way = 0; way < I386_TLB_TEST_WAYS; way++)
    {
        const TLBEntry& entry = m_tlb_test[set * I386_TLB_TEST_WAYS + way];
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

        m_state.test_registers[0] = (command & ~0x7E0U) | attributes;
        m_state.test_registers[1] = entry.physical_page | 0x10 | (way << 2);
        return;
    }

    m_state.test_registers[1] = data & ~0x10U;
}

void I386::TestTLB486()
{
    static const u32 k_attribute_bits[3] = { 0x400, 0x100, 0x040 };
    static const u8 k_attribute_flags[3] = { k_i386_tlb_dirty, k_i386_tlb_user, k_i386_tlb_writable };

    u32 command = m_state.test_registers[0];
    u32 data = m_state.test_registers[1];
    u32 linear_page = command & 0xFFFFF000U;
    u32 set = (command >> 12) & (I386_TLB_TEST_SETS - 1);
    bool valid = (command & 0x800) != 0;
    u32 lru = (u32)m_tlb_test_plru[set] << 7;

    // TR7 PL picks the way from REP
    if ((command & 1) == 0)
    {
        u32 way = (data & 0x10) != 0 ? (data >> 2) & 3 : (u32)GetTestTLBVictim(set);
        u8 flags = (valid ? k_i386_tlb_valid : 0) | ((command & 0x400) != 0 ? k_i386_tlb_dirty : 0) |
            ((command & 0x100) != 0 ? k_i386_tlb_user : 0) | ((command & 0x040) != 0 ? k_i386_tlb_writable : 0) |
            ((data & 0x400) != 0 ? k_i386_tlb_pwt : 0) | ((data & 0x800) != 0 ? k_i386_tlb_pcd : 0);
        WriteTestTLB(set, way, linear_page, data & 0xFFFFF000U, flags);
        TouchTestTLBWay(set, way);
        return;
    }

    for (u32 way = 0; way < I386_TLB_TEST_WAYS; way++)
    {
        const TLBEntry& entry = m_tlb_test[set * I386_TLB_TEST_WAYS + way];
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

        m_state.test_registers[0] = (command & ~0x7E0U) | attributes;
        m_state.test_registers[1] = entry.physical_page | ((entry.flags & k_i386_tlb_pcd) != 0 ? 0x800 : 0) |
            ((entry.flags & k_i386_tlb_pwt) != 0 ? 0x400 : 0) | lru | 0x10 | (way << 2);
        TouchTestTLBWay(set, way);
        return;
    }

    m_state.test_registers[1] = (data & ~0x390U) | lru;
}

// Opens the code window over the code page of EIP when it has host memory
// and, with paging, the TLB holds it with its host page
// The window starts at the page start, or at EIP 0 when the segment
// begins inside the page, and ends at the page end or the CS limit
// Otherwise it stays closed and the bytes come from FetchCodeSlow8
// Also sets the code fetch cost, from the page, even when the window doesn't open
// fetch: the instruction at EIP runs next, so what its first fetch does to a TLB hit can be done here: caching the host
// page a valid entry lost to a privilege change
NO_INLINE void I386::OpenCodeWindow(u32 eip, bool fetch)
{
    CloseCodeWindow();

    u32 base = m_state.segments[I386_SEGMENT_CS].base;
    u32 linear = base + eip;
    TLBEntry& tlb = GetTLBEntry(linear);
    const TLBEntry* entry = NULL;

    if ((m_state.cr0 & 0x80000000U) != 0 && tlb.linear_page == (linear & 0xFFFFF000U) && IsValidPointer(tlb.read_page))
        entry = &tlb;

    if (m_bus_timed)
    {
        u32 physical = IsValidPointer(entry) ? entry->physical_page | (linear & 0xFFF) : 0;
        u8 bus_class = IsValidPointer(entry) ? m_bus_page_class[physical >> 12] : GetBusPhysical(linear, physical);
        UpdateBusCode(bus_class, base + (physical & 0xFFFFF000U) - (linear & 0xFFFFF000U));
    }

    if ((s64)eip > m_code_limit)
        return;

    const u8* page;

    if ((m_state.cr0 & 0x80000000U) == 0)
        page = m_read_pages[linear >> 12];
    else if (IsValidPointer(entry))
        page = entry->read_page;
    else if (fetch && m_memory_generation == m_memory->GetMapGeneration())
    {
        // As TranslateLinearPaged's hit: no walk, and a page the privilege level can't read faults in the interpreter
        bool user = m_state.execution_mode == I386_MODE_VM86 || m_state.current_privilege_level == 3;

        if (!IsTLBHit(tlb, linear) || (user && (tlb.flags & k_i386_tlb_user) == 0))
            return;

        CacheTLBPages(tlb);
        page = tlb.read_page;
    }
    else
        return;

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
        linear = m_state.segments[I386_SEGMENT_CS].base + cursor;
    else if (!LogicalToLinear(I386_SEGMENT_CS, cursor, 1, false, false, linear, true))
        return false;

    GT_Bus_Access_Context& context = *m_bus_context;
    u32 physical = linear;
    const u8* page = NULL;

    // A TLB entry with a cached host page is valid for this privilege level, so no walk or fault is possible
    if ((m_state.cr0 & 0x80000000U) != 0)
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

    const I386_Segment& code = m_state.segments[I386_SEGMENT_CS];
    u32 upper_limit = code.limit;

    if (m_state.execution_mode == I386_MODE_PROTECTED && (code.attributes & I386_SEGMENT_EXPAND_DOWN) != 0)
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

    const I386_Segment& state = m_state.segments[segment];

    if (!IsValidSegmentOffset(state, offset))
        return false;

    return TryPeekLinear(state.base + offset, value);
}

bool I386::TryPeekLogical(u16 selector, u32 offset, u8& value) const
{
    const I386_Segment* state = FindSegment(selector);

    if (!IsValidPointer(state) || !IsValidSegmentOffset(*state, offset))
        return false;

    return TryPeekLinear(state->base + offset, value);
}

bool I386::TryPeekLinear(u32 linear, u8& value) const
{
    u32 physical = 0;

    if (!TryTranslateLinear(linear, physical) || !IsValidPointer(m_memory))
        return false;

    return m_memory->TryPeekPhysical(physical, value);
}

const u8* I386::PeekReadHost(u32 linear, u32 size) const
{
    if ((linear & 0xFFF) + size > 0x1000)
        return NULL;

    const u8* page;

    if ((m_state.cr0 & 0x80000000U) == 0)
        page = m_read_pages[linear >> 12];
    else
    {
        const TLBEntry& entry = GetTLBEntry(linear);

        if (!IsTLBHit(entry, linear))
            return NULL;

        page = entry.read_page;
    }

    return IsValidPointer(page) ? page + (linear & 0xFFF) : NULL;
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
    if ((m_state.cr0 & 0x80000000U) == 0)
    {
        physical = linear;
        pde_address = 0;
        pte_address = 0;
        page_flags = 0;
        reason[0] = 0;
        return true;
    }

    u32 pde = 0;
    pde_address = (m_state.cr3 & 0xFFFFF000U) + ((linear >> 20) & 0xFFCU);

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

    const I386_Segment& state = m_state.segments[segment];

    translation.segment = state.selector;
    translation.offset = offset;
    translation.segment_base = state.base;
    translation.segment_limit = state.limit;
    translation.logical_valid = IsValidSegmentOffset(state, offset);

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
    translation.logical_valid = IsValidSegmentOffset(*state, offset);

    if (!translation.logical_valid)
    {
        strncpy_fit(translation.reason, "Offset outside segment limits", sizeof(translation.reason));
        return false;
    }

    translation.linear = state->base + offset;
    translation.linear_valid = true;
    return DebugTranslateLinear(translation.linear, translation);
}

bool I386::IsValidSegmentOffset(const I386_Segment& segment, u32 offset)
{
    if ((segment.attributes & I386_SEGMENT_PRESENT) == 0)
        return false;

    if ((segment.attributes & I386_SEGMENT_EXPAND_DOWN) != 0)
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
        if (m_state.segments[i].selector == selector)
            return &m_state.segments[i];
    }

    return NULL;
}
