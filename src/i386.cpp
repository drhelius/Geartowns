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

static bool equal_name(const char* left, const char* right)
{
    if (!IsValidPointer(left) || !IsValidPointer(right))
        return false;

    while (*left != 0 && *right != 0)
    {
        if (toupper((unsigned char)*left) != toupper((unsigned char)*right))
            return false;
        left++;
        right++;
    }
    return *left == 0 && *right == 0;
}

I386::I386()
{
    InitPointer(m_memory);
    memset(&m_debug_state, 0, sizeof(m_debug_state));
}

I386::~I386()
{
}

void I386::Init(Memory* memory)
{
    m_memory = memory;
    Reset();
}

void I386::Reset()
{
    memset(&m_debug_state, 0, sizeof(m_debug_state));
    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
        m_debug_state.segment[i].limit = 0xFFFF;
    m_debug_state.segment[I386_SEGMENT_CS].selector = 0xF000;
    m_debug_state.segment[I386_SEGMENT_CS].base = 0xFFFF0000;
    m_debug_state.eip = 0xFFF0;
    m_debug_state.available = false;
}

bool I386::TryPeekLogical(I386_Segment_Register segment, u32 offset,
    u8& value) const
{
    if (!m_debug_state.available || segment < 0 || segment >= I386_SEGMENT_COUNT)
        return false;

    const I386_Debug_Segment_State& state = m_debug_state.segment[segment];
    if (!state.present || offset > state.limit)
        return false;
    return TryPeekLinear(state.base + offset, value);
}

bool I386::TryPeekLogical(u16 selector, u32 offset, u8& value) const
{
    const I386_Debug_Segment_State* state = FindSegment(selector);
    if (!IsValidPointer(state) || !state->present || offset > state->limit)
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
    return TryTranslateLinear(linear, physical) && IsValidPointer(m_memory) &&
        m_memory->TryPeekPhysical(physical, value);
}

bool I386::TryTranslateLinear(u32 linear, u32& physical) const
{
    u32 pde_address = 0;
    u32 pte_address = 0;
    u32 page_flags = 0;
    char reason[GT_DEBUG_MEMORY_REASON_SIZE];
    return TranslateLinearPassive(linear, physical, pde_address, pte_address,
        page_flags, reason, sizeof(reason));
}

bool I386::DebugTranslateLogical(I386_Segment_Register segment, u32 offset,
    GT_Debug_Memory_Translation& translation) const
{
    if (!m_debug_state.available)
    {
        strncpy_fit(translation.reason, "I386 execution state is not available",
            sizeof(translation.reason));
        return false;
    }
    if (segment < 0 || segment >= I386_SEGMENT_COUNT)
    {
        strncpy_fit(translation.reason, "Invalid segment register",
            sizeof(translation.reason));
        return false;
    }

    const I386_Debug_Segment_State& state = m_debug_state.segment[segment];
    translation.segment = state.selector;
    translation.offset = offset;
    translation.segment_base = state.base;
    translation.segment_limit = state.limit;
    translation.logical_valid = state.present && offset <= state.limit;
    if (!translation.logical_valid)
    {
        strncpy_fit(translation.reason, state.present ?
            "Offset exceeds segment limit" : "Segment is not present",
            sizeof(translation.reason));
        return false;
    }

    translation.linear = state.base + offset;
    translation.linear_valid = true;
    return DebugTranslateLinear(translation.linear, translation);
}

bool I386::DebugTranslateLogical(u16 selector, u32 offset,
    GT_Debug_Memory_Translation& translation) const
{
    const I386_Debug_Segment_State* state = FindSegment(selector);
    if (!IsValidPointer(state))
    {
        strncpy_fit(translation.reason, m_debug_state.available ?
            "Selector is not in a cached segment" :
            "I386 execution state is not available", sizeof(translation.reason));
        return false;
    }

    translation.segment = selector;
    translation.offset = offset;
    translation.segment_base = state->base;
    translation.segment_limit = state->limit;
    translation.logical_valid = state->present && offset <= state->limit;
    if (!translation.logical_valid)
    {
        strncpy_fit(translation.reason, state->present ?
            "Offset exceeds segment limit" : "Segment is not present",
            sizeof(translation.reason));
        return false;
    }

    translation.linear = state->base + offset;
    translation.linear_valid = true;
    return DebugTranslateLinear(translation.linear, translation);
}

bool I386::DebugTranslateLinear(u32 linear,
    GT_Debug_Memory_Translation& translation) const
{
    translation.linear = linear;
    translation.linear_valid = m_debug_state.available;
    if (!m_debug_state.available)
    {
        strncpy_fit(translation.reason, "I386 execution state is not available",
            sizeof(translation.reason));
        return false;
    }

    if (!TranslateLinearPassive(linear, translation.physical,
        translation.page_directory_entry, translation.page_table_entry,
        translation.page_flags, translation.reason, sizeof(translation.reason)))
        return false;

    translation.physical_valid = true;
    return IsValidPointer(m_memory) &&
        m_memory->DebugTranslatePhysical(translation.physical, translation);
}

bool I386::CopyDebugState(I386_Debug_State& state) const
{
    state = m_debug_state;
    return state.available;
}

bool I386::GetDebugRegisterValue(const char* name, u32& value) const
{
    if (!m_debug_state.available)
        return false;

    if (equal_name(name, "EAX")) value = m_debug_state.eax;
    else if (equal_name(name, "EBX")) value = m_debug_state.ebx;
    else if (equal_name(name, "ECX")) value = m_debug_state.ecx;
    else if (equal_name(name, "EDX")) value = m_debug_state.edx;
    else if (equal_name(name, "ESI")) value = m_debug_state.esi;
    else if (equal_name(name, "EDI")) value = m_debug_state.edi;
    else if (equal_name(name, "EBP")) value = m_debug_state.ebp;
    else if (equal_name(name, "ESP")) value = m_debug_state.esp;
    else if (equal_name(name, "EIP")) value = m_debug_state.eip;
    else if (equal_name(name, "EFLAGS")) value = m_debug_state.eflags;
    else if (equal_name(name, "CR0")) value = m_debug_state.cr0;
    else if (equal_name(name, "CR2")) value = m_debug_state.cr2;
    else if (equal_name(name, "CR3")) value = m_debug_state.cr3;
    else return false;
    return true;
}

void I386::SetDebugState(const I386_Debug_State& state)
{
    m_debug_state = state;
}

bool I386::TranslateLinearPassive(u32 linear, u32& physical,
    u32& pde_address, u32& pte_address, u32& page_flags,
    char* reason, size_t reason_size) const
{
    if (!m_debug_state.available)
    {
        strncpy_fit(reason, "I386 execution state is not available", reason_size);
        return false;
    }

    if ((m_debug_state.cr0 & 0x80000000U) == 0)
    {
        physical = linear;
        pde_address = 0;
        pte_address = 0;
        page_flags = 0;
        reason[0] = 0;
        return true;
    }

    pde_address = (m_debug_state.cr3 & 0xFFFFF000U) +
        (((linear >> 22) & 0x3FFU) * 4);
    u32 pde = 0;
    if (!ReadPhysical32(pde_address, pde) || (pde & 0x01) == 0)
    {
        strncpy_fit(reason, "Page-directory entry is unavailable",
            reason_size);
        return false;
    }

    pte_address = (pde & 0xFFFFF000U) + (((linear >> 12) & 0x3FFU) * 4);
    u32 pte = 0;
    if (!ReadPhysical32(pte_address, pte) || (pte & 0x01) == 0)
    {
        strncpy_fit(reason, "Page-table entry is unavailable", reason_size);
        return false;
    }

    physical = (pte & 0xFFFFF000U) | (linear & 0xFFFU);
    page_flags = (pde & 0xFFFU) | ((pte & 0xFFFU) << 12);
    reason[0] = 0;
    return true;
}

bool I386::ReadPhysical32(u32 physical, u32& value) const
{
    u8 data[4];
    if (!IsValidPointer(m_memory) ||
        !m_memory->TryPeekPhysicalBlock(physical, data, sizeof(data)))
        return false;
    value = read_u32_le(data);
    return true;
}

const I386_Debug_Segment_State* I386::FindSegment(u16 selector) const
{
    if (!m_debug_state.available)
        return NULL;
    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
    {
        if (m_debug_state.segment[i].selector == selector)
            return &m_debug_state.segment[i];
    }
    return NULL;
}
