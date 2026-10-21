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

#ifndef I386_BUS_INLINE_H
#define I386_BUS_INLINE_H

INLINE bool I386::IsBusTimed() const
{
    return m_bus_timed;
}

INLINE u8 I386::GetBusPattern(const InstructionContext& instruction)
{
    u8 entry = k_bus_patterns[instruction.two_byte ? 256 + instruction.opcode2 : instruction.opcode];
    u8 pattern = entry & k_i386_bus_pattern_mask;

    if (pattern >= I386_BUS_GROUP)
        return (u8)(k_bus_groups[pattern - I386_BUS_GROUP][instruction.reg] | (entry & ~k_i386_bus_pattern_mask));

    return entry;
}

INLINE u32 I386::GetBusCycles(u32 size) const
{
    return size > m_bus.width ? size >> (m_bus.width >> 1) : 1;
}

// Aligned far-row DRAM time, folded into compiled runs: their accesses add only the difference
INLINE u32 I386::GetBusDramFold(u32 size, bool read, bool write, u32 code_row) const
{
    u32 cycles = GetBusCycles(size);
    bool far_row = (code_row & k_i386_bus_code_row) != k_i386_bus_code_row;
    u32 shift = code_row >> 31;
    u32 fold = 0;

    if (read)
        fold += (far_row ? m_bus.dram_far[0] >> shift : m_bus.dram_near[0]) + cycles * m_bus.cycle[GT_BUS_DRAM][0];

    if (write)
        fold += (far_row ? m_bus.dram_far[1] >> shift : m_bus.dram_near[1]) + cycles * m_bus.cycle[GT_BUS_DRAM][1];

    return fold;
}

// Passive and host-cache independent: most recent way first, then any way of the set, then the page tables
INLINE bool I386::TranslateCurrentPassive(u32 linear, u32& physical) const
{
    physical = linear;

    if ((m_state.cr0 & 0x80000000U) == 0)
        return true;

    u32 set = (linear >> 12) & (I386_TLB_SETS - 1);
    const TLBEntry* ways = &m_tlb[set * I386_TLB_WAYS];
    const TLBEntry& recent = ways[m_tlb_mru[set]];
    u32 page = linear & 0xFFFFF000U;
    int way = 0;

    if (likely((recent.flags & k_i386_tlb_valid) != 0 && recent.linear_page == page))
    {
        physical = recent.physical_page | (linear & 0xFFF);
        return true;
    }

    while (way < I386_TLB_WAYS && ((ways[way].flags & k_i386_tlb_valid) == 0 || ways[way].linear_page != page))
        way++;

    if (way < I386_TLB_WAYS)
    {
        physical = ways[way].physical_page | (linear & 0xFFF);
        return true;
    }

    u32 walked = linear;
    bool translated = TranslatePagedPassive(linear, walked);
    physical = walked;
    return translated;
}

INLINE u8 I386::GetBusPhysical(u32 linear, u32& physical) const
{
    if (!TranslateCurrentPassive(linear, physical))
        return GT_BUS_NONE;

    return m_bus_page_class[physical >> 12];
}

// Returns (cycles as if aligned << 8) + 1
INLINE u32 I386::AddBusAccess(u32 linear, u32 size, int write, u32 code_row)
{
    u32 physical = 0;
    u8 bus_class = GetBusPhysical(linear, physical);
    u32 aligned = GetBusCycles(size);
    u32 cycles = aligned;
    s32 penalty = 0;

    if (unlikely((linear & (size - 1)) != 0))
    {
        u32 offset = linear & 3;
        cycles = m_bus.width == 2 ? aligned + (linear & 1) : (offset + size > 4 ? 2 : 1);
        penalty = (s32)((cycles - aligned) * ((2U << 16) + m_bus.cycle_refresh));

        if (offset + size > 4)
            penalty += (s32)m_bus.misalign;
    }

    if (bus_class == GT_BUS_DRAM)
    {
        u32 row = physical >> 10;
        u32 code = code_row & k_i386_bus_code_row;
        bool far_row = code != k_i386_bus_code_row && row != code;
        penalty += (s32)(far_row ? m_bus.dram_far[write] >> (code_row >> 31) : m_bus.dram_near[write]);

        // The stored bit marks a 2-cycle store, which a load right after it in its row overlaps
        if (row != (m_bus_row & ~k_i386_bus_row_stored))
            penalty += (s32)m_bus.dram_change[write];
        else if (write == 0 && m_bus_row != row && far_row)
            penalty -= (s32)m_bus.store_load;

        m_bus_row = row | (write != 0 && cycles > 1 && m_bus.store_load != 0 ? k_i386_bus_row_stored : 0);
    }

    m_bus_cycle_counts[bus_class][write] += cycles;
    m_bus_fraction += penalty + (s32)(cycles * m_bus.cycle[bus_class][write]);
    return (aligned << 8) | 1;
}

// k_i386_bus_code_row outside DRAM. Bit 31 halves the far penalty of one-byte instructions, which share fetches
INLINE u32 I386::GetBusCodeRow(u32 eip, u32 length) const
{
    u32 row = m_bus_code_dram ? ((eip + m_bus_code_base) >> 10) & k_i386_bus_code_row : k_i386_bus_code_row;
    return row | (length == 1 ? 0x80000000U : 0);
}

INLINE u32 I386::GetBusCodeNeed(u32 next_eip, u32 length) const
{
    // Code comes in dwords: the instruction waits for the whole dword holding its last byte
    // Its other bytes in that dword then take a decode clock each, while the queue goes on filling
    u32 last = next_eip - 1 + m_bus_code_base;
    u32 decode = MIN(length, (last & 3) + 1) - 1;
    return ((length + (~last & 3)) << 8) + decode * m_bus_code_rate;
}

INLINE u32 I386::GetTransferComponents()
{
    m_bus_components = GetNextInstructionComponents();
    return m_bus_components;
}

INLINE u32 I386::CarryBusTime()
{
    u32 carry = (u32)(m_bus_fraction >> 16);
    m_bus_fraction &= 0xFFFF;
    return carry;
}

// Pending until a taken transfer carries it into clocks, where blocks end too: no bound in their budget checks
INLINE void I386::AddBusTime()
{
    const InstructionContext& c = m_instruction;
    u8 pattern = GetBusPattern(c);
    u32 kind = pattern & k_i386_bus_pattern_mask;

    if (kind == I386_BUS_NONE)
    {
        AddBusQueue(0, false);
        return;
    }

    u32 code_row = GetBusCodeRow(c.start_eip, c.next_eip - c.start_eip);
    u32 size = (pattern & k_i386_bus_byte) != 0 ? 1 : (pattern & k_i386_bus_word) != 0 ? 2 : c.operand_size;
    u32 stack_mask = m_stack32 ? 0xFFFFFFFFU : 0xFFFFU;
    u32 stack = m_state.registers[I386_REG_ESP].value;
    u32 stack_base = m_state.segments[I386_SEGMENT_SS].base;
    bool transfer = false;
    u32 data = 0;

    switch (kind)
    {
        case I386_BUS_READ:
            if (c.memory_operand)
                data = AddBusAccess(m_state.segments[c.segment].base + c.effective_offset, size, 0, code_row);
            break;
        case I386_BUS_WRITE:
            if (c.memory_operand)
                data = AddBusAccess(m_state.segments[c.segment].base + c.effective_offset, size, 1, code_row);
            break;
        case I386_BUS_RMW:
            if (c.memory_operand)
            {
                u32 linear = m_state.segments[c.segment].base + c.effective_offset;
                data = AddBusAccess(linear, size, 0, code_row);
                data += AddBusAccess(linear, size, 1, code_row);
            }
            break;
        case I386_BUS_PUSH:
            data = AddBusAccess(stack_base + (stack & stack_mask), size, 1, code_row);
            break;
        case I386_BUS_POP:
            data = AddBusAccess(stack_base + ((stack - size) & stack_mask), size, 0, code_row);
            break;
        case I386_BUS_JUMP:
            transfer = true;
            break;
        case I386_BUS_JCC:
            transfer = CheckCondition(c.two_byte ? c.opcode2 & 15 : c.opcode & 15);
            break;
        case I386_BUS_CALL:
            data = AddBusAccess(stack_base + (stack & stack_mask), size, 1, code_row);
            transfer = true;
            break;
        case I386_BUS_RET:
            data = AddBusAccess(stack_base + ((stack - size - (c.opcode == 0xC2 ? c.immediate : 0)) & stack_mask),
                size, 0, code_row);
            transfer = true;
            break;
        default:
            data = AddBusInstructionAccesses(pattern, code_row, transfer);
            break;
    }

    AddBusQueue(data, transfer);
}

INLINE void I386::AddBusQueue(u32 data, bool transfer)
{
    const InstructionContext& c = m_instruction;
    u32 clocks = (u32)m_step.clocks;
    u32 accesses = data & 0xFF;
    u32 cycles = data >> 8;
    u32 length = c.next_eip - c.start_eip;
    u32 need = GetBusCodeNeed(c.next_eip, length);
    u32 free = clocks > accesses * 2 ? clocks - accesses * 2 : 0;
    s32 add = (s32)(cycles * m_bus.cycle_refresh + ((cycles - accesses) << 17));

    u32 queue = m_bus_queue;
    u32 stall = need > queue ? need - queue : 0;
    add += (s32)(stall * m_bus_code_cost);
    queue += stall + free * m_bus_code_rate - (length << 8);

    m_bus_queue = MIN(queue, k_i386_bus_queue);
    m_bus_fraction += add;

    // Only a transfer that waited for its own code finds its access behind the refill
    if (transfer)
        AddBusTransfer(accesses != 0 && stall != 0);
}

INLINE u32 I386::GetBusPending() const
{
    return (u32)(m_bus_fraction >> 16);
}

// Whole clocks before the access, the fraction pending with the rest of the bus time
// Nothing answers a port the board doesn't decode, so a write there ends sooner
INLINE void I386::AddBusPortWait(u16 port, int write)
{
    u32 wait = m_bus.io[write];

    if (IsValidPointer(m_bus_decoded_ports) && (m_bus_decoded_ports[port >> 3] & (1 << (port & 7))) == 0)
    {
        if (write)
            wait = m_bus.io_free_write;
    }
    else
    {
        for (int i = 0; i < GT_MACHINE_PORT_WAIT_BLOCKS; i++)
        {
            const BusPortWait& block = m_bus.io_blocks[i];

            if ((u32)port - block.first <= block.span)
            {
                wait = block.wait[write];
                break;
            }
        }
    }

    m_step.clocks += wait >> 16;
    m_bus_fraction += (s32)(wait & 0xFFFF);
}

#endif /* I386_BUS_INLINE_H */
