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
#include "x87.h"
#include "../system/memory.h"

// Pattern in the low bits, then k_i386_bus_byte or k_i386_bus_word. From I386_BUS_GROUP, k_bus_groups by reg
const u8 I386::k_bus_patterns[512] =
{
    /* 0x00   */ 0x43, 0x03, 0x41, 0x01, 0x00, 0x00, 0x04, 0x05, 0x43, 0x03, 0x41, 0x01, 0x00, 0x00, 0x04, 0x00,
    /* 0x10   */ 0x43, 0x03, 0x41, 0x01, 0x00, 0x00, 0x04, 0x05, 0x43, 0x03, 0x41, 0x01, 0x00, 0x00, 0x04, 0x05,
    /* 0x20   */ 0x43, 0x03, 0x41, 0x01, 0x00, 0x00, 0x00, 0x00, 0x43, 0x03, 0x41, 0x01, 0x00, 0x00, 0x00, 0x00,
    /* 0x30   */ 0x43, 0x03, 0x41, 0x01, 0x00, 0x00, 0x00, 0x00, 0x41, 0x01, 0x41, 0x01, 0x00, 0x00, 0x00, 0x00,
    /* 0x40   */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0x50   */ 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    /* 0x60   */ 0x13, 0x14, 0x19, 0x83, 0x00, 0x00, 0x00, 0x00, 0x04, 0x01, 0x04, 0x01, 0x0A, 0x0A, 0x0A, 0x0A,
    /* 0x70   */ 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E,
    /* 0x80   */ 0x60, 0x20, 0x60, 0x20, 0x41, 0x01, 0x43, 0x03, 0x42, 0x02, 0x41, 0x01, 0x82, 0x00, 0x81, 0x07,
    /* 0x90   */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x04, 0x05, 0x00, 0x00,
    /* 0xA0   */ 0x48, 0x08, 0x49, 0x09, 0x0A, 0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
    /* 0xB0   */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0xC0   */ 0x61, 0x21, 0x0C, 0x0C, 0x18, 0x18, 0x62, 0x22, 0x15, 0x16, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
    /* 0xD0   */ 0x61, 0x21, 0x61, 0x21, 0x00, 0x00, 0x00, 0x17, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C,
    /* 0xE0   */ 0x0F, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x0D, 0x12, 0x0D, 0x00, 0x00, 0x00, 0x00,
    /* 0xF0   */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x25,
    /* 0x0F00 */ 0xA6, 0xA7, 0x81, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0x0F10 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0x0F20 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0x0F30 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0x0F40 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0x0F50 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0x0F60 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0x0F70 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0x0F80 */ 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E,
    /* 0x0F90 */ 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    /* 0x0FA0 */ 0x04, 0x05, 0x00, 0x01, 0x03, 0x03, 0x00, 0x00, 0x04, 0x05, 0x00, 0x03, 0x03, 0x03, 0x00, 0x01,
    /* 0x0FB0 */ 0x43, 0x03, 0x18, 0x03, 0x18, 0x18, 0x41, 0x81, 0x00, 0x00, 0x28, 0x03, 0x01, 0x01, 0x41, 0x81,
    /* 0x0FC0 */ 0x43, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0x0FD0 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0x0FE0 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0x0FF0 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const u8 I386::k_bus_groups[9][8] =
{
    { 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x01 }, // 80h-83h
    { 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03 }, // C0h, C1h, D0h-D3h
    { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // C6h, C7h
    { 0x01, 0x01, 0x03, 0x03, 0x01, 0x01, 0x01, 0x01 }, // F6h, F7h
    { 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // FEh
    { 0x03, 0x03, 0x10, 0x12, 0x11, 0x12, 0x06, 0x00 }, // FFh
    { 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00 }, // 0F00h
    { 0x1B, 0x1B, 0x1A, 0x1A, 0x02, 0x00, 0x01, 0x00 }, // 0F01h
    { 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x03 }  // 0FBAh
};

// 1/256 model clocks to 1/65536 CPU clocks: waits keep their real time with a CPU speed override
static u32 ScaleBusWait(u32 value, u32 cpu_clock_rate, u32 model_clock_rate)
{
    return (u32)(((u64)value * 256 * cpu_clock_rate + model_clock_rate / 2) / model_clock_rate);
}

void I386::SetBusTiming(const GT_Machine_Bus_Timing* timing, const GT_Machine_Bus_Waits* waits, int bus_width,
    u32 cpu_clock_rate, u32 model_clock_rate)
{
    BusTiming bus;
    memset(&bus, 0, sizeof(bus));
    bool valid = IsValidPointer(timing) && IsValidPointer(waits) && model_clock_rate != 0;

    if (valid)
    {
        u32 cpu = cpu_clock_rate;
        u32 model = model_clock_rate;

        bus.width = bus_width == 16 ? 2 : 4;
        bus.cycle_refresh = (u32)timing->refresh * 512;
        bus.refill = ScaleBusWait(timing->refill, cpu, model);
        // The 486's table already counts the core's split of a misaligned operand
        bus.misalign = m_i486_timing ? 0 : (u32)timing->misalign * 256;
        bus.string_row = ScaleBusWait(timing->string_row, cpu, model);
        bus.string_read = ScaleBusWait(timing->string_read, cpu, model);
        bus.acknowledge = (u32)(((u64)k_i386_bus_acknowledge_ns * cpu * 65536 + 500000000) / 1000000000);

        for (int k = 0; k < 2; k++)
        {
            bus.dram_far[k] = ScaleBusWait(waits->dram_far[k], cpu, model);
            bus.dram_near[k] = ScaleBusWait(waits->dram_near[k], cpu, model);
            bus.dram_change[k] = ScaleBusWait(waits->dram_change[k], cpu, model);
            bus.cycle[GT_BUS_DRAM][k] = ScaleBusWait(waits->dram_cycle[k], cpu, model);
            bus.cycle[GT_BUS_VRAM][k] = ScaleBusWait(waits->vram_cycle[k], cpu, model);
            bus.cycle[GT_BUS_SPRITE][k] = ScaleBusWait(waits->sprite_cycle[k], cpu, model);
            bus.cycle[GT_BUS_CMOS][k] = ScaleBusWait(waits->cmos_cycle[k], cpu, model);
            bus.slow[GT_BUS_FMR][k] = ScaleBusWait(timing->fmr_byte[k], cpu, model);
            bus.slow[GT_BUS_PCM][k] = ScaleBusWait(timing->pcm[k], cpu, model);
            bus.slow[GT_BUS_MMIO][k] = ScaleBusWait(timing->mmio[k], cpu, model);
            bus.fmr_word[k] = ScaleBusWait(timing->fmr_word[k], cpu, model);
            bus.io[k] = ScaleBusWait(timing->io[k], cpu, model);
        }

        bus.transfer_row = ScaleBusWait(waits->dram_transfer, cpu, model);
        bus.store_load = ScaleBusWait(waits->dram_store_load, cpu, model);
        bus.io_free_write = ScaleBusWait(timing->io_free_write, cpu, model);
        bus.segment_load[0] = ScaleBusWait(timing->segment_load[0], cpu, model);
        bus.segment_load[1] = ScaleBusWait(timing->segment_load[1], cpu, model);

        for (int i = 0; i < GT_MACHINE_PORT_WAIT_BLOCKS; i++)
            bus.io_blocks[i].first = 0x10000;

        for (int i = 0; IsValidPointer(timing->port_waits) && i < GT_MACHINE_PORT_WAIT_BLOCKS; i++)
        {
            const GT_Machine_Port_Wait& block = timing->port_waits[i];
            bus.io_blocks[i].first = block.first;
            bus.io_blocks[i].span = (u32)(block.last - block.first);

            for (int k = 0; k < 2; k++)
                bus.io_blocks[i].wait[k] = ScaleBusWait((u32)(timing->io[k] + block.extra[k]), cpu, model);
        }

        bus.cycle[GT_BUS_ROM][0] = ScaleBusWait(waits->rom_cycle, cpu, model);
        bus.cycle[GT_BUS_ROM][1] = bus.cycle[GT_BUS_ROM][0];

        // Per code byte: 1/width of a fetch cycle plus refresh. A free clock prefetches the inverse
        for (int c = 0; c < GT_BUS_CLASS_COUNT; c++)
        {
            u32 wait = c == GT_BUS_ROM ? ScaleBusWait(waits->rom_code, cpu, model) : bus.cycle[c][0];
            u64 cycle = ((2ULL << 16) + wait) * (256 + timing->refresh);
            bus.code_cost[c] = (u32)((cycle + 32768 * bus.width) / (65536ULL * bus.width));
            bus.code_rate[c] = (u32)(((u64)bus.width << 32) / cycle);
        }
    }

    m_bus_table = bus;
    m_bus_table_valid = valid;
    UpdateBusTiming();
}

// HSPEED and CMEN on a board with cache controls, always on a board without them
void I386::SetBoardCache(bool enabled)
{
    m_board_cache = enabled;
}

// The 486 runs its cache-hit clocks while its cache serves, and the bus model while every access goes to the bus
void I386::UpdateBusTiming()
{
    bool cached = m_i486_timing && m_board_cache && (m_state.cr0 & 0x40000000U) == 0;
    bool timed = m_bus_table_valid && !cached;
    BusTiming bus;

    if (timed)
        bus = m_bus_table;
    else
        memset(&bus, 0, sizeof(bus));

    bool changed = timed != m_bus_timed || memcmp(&bus, &m_bus, sizeof(bus)) != 0;

    // The run loop picks its instance per slice, so a 486 cache change ends the slice after the instruction
    if (timed != m_bus_timed)
        m_step.end_batch = true;

    m_bus = bus;
    m_bus_timed = timed;

    if (!changed)
        return;

    CloseCodeWindow();
#if !defined(GT_DISABLE_DYNAREC)
    FlushDynarec();
#endif
}

// A bit per port the board decodes, all decoded without them
void I386::SetDecodedPorts(const u8* ports)
{
    m_bus_decoded_ports = ports;
}

// Whole clocks of the generic wait
u32 I386::GetBusPortWait(bool write) const
{
    return (m_bus.io[write ? 1 : 0] + 0x8000) >> 16;
}

// Bus cycles per class, reads then writes, counted from power on and wrapping
const u32* I386::GetBusCycleCounts() const
{
    return &m_bus_cycle_counts[0][0];
}

void I386::UpdateBusCode(u8 bus_class, u32 code_base)
{
    m_bus_code_dram = bus_class == GT_BUS_DRAM;
    m_bus_code_base = code_base;
    m_bus_code_cost = m_bus.code_cost[bus_class];
    m_bus_code_rate = m_bus.code_rate[bus_class];
}

u32 I386::GetBusTransferLevel(u32 linear, u32 components, u32& add) const
{
    // The fetch restarts at the target's fetch unit, so the bytes before the target cost their time
    // The m clocks of the transfer prefetch from there
    u32 level = components * m_bus_code_rate;
    u32 skipped = (linear & (m_bus.width - 1)) << 8;

    if (level >= skipped)
    {
        add = 0;
        return MIN(level - skipped, k_i386_bus_queue);
    }

    add = (skipped - level) * m_bus_code_cost;
    return 0;
}

u32 I386::GetBusTransferRow(u32 eip) const
{
    u32 physical = eip + m_bus_code_base;

    if (m_bus.transfer_row == 0 || m_bus_page_class[physical >> 12] != GT_BUS_DRAM ||
        (physical >> 10) == (m_bus_row & ~k_i386_bus_row_stored))
        return 0;

    return m_bus.transfer_row;
}

NO_INLINE void I386::AddBusTransfer(bool access)
{
    u32 add = 0;
    m_bus_queue = GetBusTransferLevel(m_state.segments[I386_SEGMENT_CS].base + m_state.eip, m_bus_components, add);
    m_bus_components = 0;

    if (access)
        add += GetBusTransferRow(m_state.eip);

    m_bus_fraction += (s32)(m_bus.refill + add);
    m_step.clocks += CarryBusTime();
}

template<bool i486>
NO_INLINE u32 I386::AddBusInstructionAccesses(u8 pattern, u32 code_row, bool& transfer)
{
    const InstructionContext& c = m_instruction;
    u32 size = (pattern & k_i386_bus_byte) != 0 ? 1 : (pattern & k_i386_bus_word) != 0 ? 2 : c.operand_size;
    u32 stack_mask = m_stack32 ? 0xFFFFFFFFU : 0xFFFFU;
    u32 stack = m_state.registers[I386_REG_ESP].value;
    u32 stack_base = m_state.segments[I386_SEGMENT_SS].base;
    u32 linear = m_state.segments[c.segment].base + c.effective_offset;
    bool real = m_state.execution_mode == I386_MODE_REAL;
    u32 data = 0;

    switch (pattern & k_i386_bus_pattern_mask)
    {
        case I386_BUS_PUSH_READ:
            if (c.memory_operand)
                data = AddBusAccess<i486>(linear, size, 0, code_row);
            data += AddBusAccess<i486>(stack_base + (stack & stack_mask), size, 1, code_row);
            break;
        case I386_BUS_POP_WRITE:
            data = AddBusAccess<i486>(stack_base + ((stack - size) & stack_mask), size, 0, code_row);
            if (c.memory_operand)
                data += AddBusAccess<i486>(linear, size, 1, code_row);
            break;
        case I386_BUS_MOFFS_READ:
        case I386_BUS_MOFFS_WRITE:
        {
            u8 segment = c.segment_override == 0xFF ? (u8)I386_SEGMENT_DS : c.segment_override;
            data = AddBusAccess<i486>(m_state.segments[segment].base + c.immediate, size,
                (pattern & k_i386_bus_pattern_mask) == I386_BUS_MOFFS_WRITE ? 1 : 0, code_row);
            break;
        }
        case I386_BUS_STRING:
        {
            if (!m_string.executed)
                break;

            u32 width = m_string.width >> 3;
            u32 source = m_state.segments[m_string.source_segment].base + m_string.source_offset;
            u32 destination = m_state.segments[I386_SEGMENT_ES].base + m_string.destination_offset;

            switch (c.opcode & 0xFE)
            {
                case 0xA4:
                    data = AddBusAccess<i486>(source, width, 0, code_row);
                    data += AddBusAccess<i486>(destination, width, 1, code_row);
                    break;
                case 0xA6:
                    data = AddBusAccess<i486>(source, width, 0, code_row);
                    data += AddBusAccess<i486>(destination, width, 0, code_row);
                    break;
                case 0x6C:
                case 0xAA:
                    data = AddBusAccess<i486>(destination, width, 1, code_row);
                    break;
                case 0xAE:
                    data = AddBusAccess<i486>(destination, width, 0, code_row);
                    break;
                default:
                    data = AddBusAccess<i486>(source, width, 0, code_row);
                    break;
            }
            break;
        }
        case I386_BUS_LOOP:
        {
            u32 count = GetRegister(I386_REG_ECX, c.address_size * 8);
            bool zf = (m_state.eflags & I386_FLAG_ZF) != 0;
            transfer = c.opcode == 0xE3 ? count == 0 :
                count != 0 && (c.opcode == 0xE2 || (c.opcode == 0xE1 ? zf : !zf));
            break;
        }
        case I386_BUS_CALL_RM:
            if (c.memory_operand)
                data = AddBusAccess<i486>(linear, size, 0, code_row);
            data += AddBusAccess<i486>(stack_base + (stack & stack_mask), size, 1, code_row);
            transfer = true;
            break;
        case I386_BUS_JUMP_RM:
            if (c.memory_operand)
                data = AddBusAccess<i486>(linear, size, 0, code_row);
            transfer = true;
            break;
        case I386_BUS_FAR:
        {
            u32 table = real ? 0 : 2;

            switch (c.opcode)
            {
                case 0x9A:
                    AddBusTransferAccesses(table, 2, size);
                    break;
                case 0xEA:
                    AddBusTransferAccesses(table, 0, size);
                    break;
                case 0xCA:
                case 0xCB:
                    AddBusTransferAccesses(table + 2, 0, size);
                    break;
                case 0xCF:
                    AddBusTransferAccesses(table + 3, 0, size);
                    break;
                case 0xFF:
                    data = AddBusAccess<i486>(linear, size, 0, code_row) +
                        AddBusAccess<i486>(linear + size, 2, 0, code_row);
                    AddBusTransferAccesses(table, c.reg == 3 ? 2 : 0, size);
                    break;
                default:
                    // INTO only transfers when OF is set
                    if (c.opcode == 0xCE && m_state.eip == c.next_eip)
                        return 0;

                    AddBusTransferAccesses(real ? 1 : 4, 3, real ? 2 : 4);
                    break;
            }

            transfer = true;
            break;
        }
        case I386_BUS_PUSHA:
            for (u32 i = 0; i < 8; i++)
                data += AddBusAccess<i486>(stack_base + ((stack + i * size) & stack_mask), size, 1, code_row);
            break;
        case I386_BUS_POPA:
            for (u32 i = 0; i < 8; i++)
                data += AddBusAccess<i486>(stack_base + ((stack - (8 - i) * size) & stack_mask), size, 0, code_row);
            break;
        case I386_BUS_ENTER:
        {
            u32 level = (c.immediate2 & 0x1F);
            data = AddBusAccess<i486>(stack_base + (m_state.registers[I386_REG_EBP].value & stack_mask), size, 1,
                code_row);

            if (level != 0)
                AddBusTransferAccesses(level - 1, level, size);
            break;
        }
        case I386_BUS_LEAVE:
            data = AddBusAccess<i486>(stack_base + ((stack - size) & stack_mask), size, 0, code_row);
            break;
        case I386_BUS_XLAT:
        {
            u8 segment = c.segment_override == 0xFF ? (u8)I386_SEGMENT_DS : c.segment_override;
            u32 table = m_state.registers[I386_REG_EBX].value & (c.address_size == 2 ? 0xFFFFU : 0xFFFFFFFFU);
            data = AddBusAccess<i486>(m_state.segments[segment].base + table, 1, 0, code_row);
            break;
        }
        case I386_BUS_READ_FAR:
            if (c.memory_operand)
                data = AddBusAccess<i486>(linear, size, 0, code_row) +
                    AddBusAccess<i486>(linear + size, 2, 0, code_row);
            break;
        case I386_BUS_BOUND:
            if (c.memory_operand)
                data = AddBusAccess<i486>(linear, size, 0, code_row) +
                    AddBusAccess<i486>(linear + size, size, 0, code_row);
            break;
        case I386_BUS_DESC_READ:
        case I386_BUS_DESC_WRITE:
        {
            int write = (pattern & k_i386_bus_pattern_mask) == I386_BUS_DESC_WRITE ? 1 : 0;

            if (c.memory_operand)
                data = AddBusAccess<i486>(linear, 2, write, code_row) +
                    AddBusAccess<i486>(linear + 2, 4, write, code_row);
            break;
        }
        case I386_BUS_ESC:
        {
            if (!c.memory_operand || !IsValidPointer(m_x87))
                break;

            bool write = false;
            u32 operand = X87::GetMemoryOperandSize(c.opcode, c.modrm, c.operand_size == 4, write);

            for (u32 done = 0; done < operand; done += 4)
                data += AddBusAccess<i486>(linear + done, MIN(operand - done, 4U), write ? 1 : 0, code_row);
            break;
        }
        default:
            break;
    }

    return data;
}

template u32 I386::AddBusInstructionAccesses<false>(u8 pattern, u32 code_row, bool& transfer);
template u32 I386::AddBusInstructionAccesses<true>(u8 pattern, u32 code_row, bool& transfer);

// iteration: the clocks of one element in the table
void I386::AddBusStringElement(u32 iteration)
{
    if (!m_string.executed)
        return;

    u32 width = m_string.width >> 3;
    u32 source = m_state.segments[m_string.source_segment].base + m_string.source_offset;
    u32 destination = m_state.segments[I386_SEGMENT_ES].base + m_string.destination_offset;
    u32 cycles = 0;
    u32 busy = 0;

    switch (m_instruction.opcode & 0xFE)
    {
        case 0xA4:
            cycles = AddBusStringAccess(source, width, 0, false);
            cycles += AddBusStringAccess(destination, width, 1, false);
            break;
        case 0xA6:
            cycles = AddBusStringAccess(source, width, 0, false);
            cycles += AddBusStringAccess(destination, width, 0, false);
            break;
        case 0x6C:
        case 0xAA:
            cycles = AddBusStringAccess(destination, width, 1, false);
            break;
        case 0xAE:
            cycles = AddBusStringAccess(destination, width, 0, false);
            break;
        case 0xAC:
            // LODS works 4 clocks on the data it waited for
            cycles = AddBusStringAccess(source, width, 0, true);
            busy = 4;
            break;
        default:
            cycles = AddBusStringAccess(source, width, 0, false);
            break;
    }

    busy += cycles << 1;
    s32 add = (s32)(cycles * m_bus.cycle_refresh);

    if (busy > iteration)
        add += (s32)((busy - iteration) << 16);

    m_bus_queue = k_i386_bus_queue;
    m_bus_fraction += add;
    m_step.clocks += CarryBusTime();
}

u32 I386::AddBusStringAccess(u32 linear, u32 size, int write, bool load)
{
    u32 physical = 0;
    u8 bus_class = GetBusPhysical(linear, physical);
    u32 aligned = GetBusCycles(size);
    u32 cycles = aligned;
    s32 penalty = 0;

    if ((linear & (size - 1)) != 0)
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

        if (row != (m_bus_row & ~k_i386_bus_row_stored))
            penalty += (s32)m_bus.string_row;

        if (load)
            penalty += (s32)m_bus.string_read;

        m_bus_row = row;
    }

    m_bus_cycle_counts[bus_class][write] += cycles;
    m_bus_fraction += penalty + (s32)(cycles * m_bus.cycle[bus_class][write]);
    return aligned;
}

// Addresses not tracked: each access counts as far-row DRAM and leaves the open row unknown
void I386::AddBusTransferAccesses(u32 reads, u32 writes, u32 size)
{
    u32 cycles = GetBusCycles(size);
    u32 read = m_bus.dram_far[0] + cycles * m_bus.cycle[GT_BUS_DRAM][0] + (m_i486_timing ? k_i486_bus_read : 0);
    s32 penalty = (s32)(reads * read + writes * (m_bus.dram_far[1] + cycles * m_bus.cycle[GT_BUS_DRAM][1]));

    m_bus_fraction += penalty;
    m_bus_row = k_i386_bus_no_row;
}

void I386::AddBusInterrupt(u64& clocks, bool error_code)
{
    if (!m_bus_timed)
        return;

    bool real = m_state.execution_mode == I386_MODE_REAL;
    AddBusTransferAccesses(real ? 1 : 4, error_code ? 4 : 3, real ? 2 : 4);

    // Nothing from the code window: the dynarec's block lookup may have moved it before the interrupt
    m_bus_queue = 0;
    m_bus_components = 0;
    m_bus_fraction += (s32)m_bus.refill;
    clocks += CarryBusTime();
}

// Odd-address word elements alternate crossing a dword, so even and odd elements get a time each
void I386::PrepareBusRepeat(BusRepeat& repeat, u32 source, u32 destination, u32 size, bool move, bool reverse,
    u32 iteration) const
{
    u32 cycles = GetBusCycles(size) * (move ? 2 : 1);
    u32 busy = cycles << 1;
    u64 base = (u64)cycles * m_bus.cycle_refresh + (busy > iteration ? (u64)(busy - iteration) << 16 : 0);

    repeat.source = 0;
    repeat.destination = 0;
    repeat.source_class = move ? GetBusPhysical(source, repeat.source) : (u8)GT_BUS_NONE;
    repeat.destination_class = GetBusPhysical(destination, repeat.destination);

    for (u32 i = 0; i < 2; i++)
    {
        u32 step = reverse ? 0U - i * size : i * size;
        u32 destination_cycles = 0;
        u32 source_cycles = 0;
        u64 element = base + GetBusRepeatSplit(destination + step, size, destination_cycles) +
            (u64)destination_cycles * m_bus.cycle[repeat.destination_class][1];

        if (move)
            element += GetBusRepeatSplit(source + step, size, source_cycles) +
                (u64)source_cycles * m_bus.cycle[repeat.source_class][0];

        repeat.element[i] = element;
    }
}

u64 I386::GetBusRepeatSplit(u32 linear, u32 size, u32& cycles) const
{
    u32 aligned = GetBusCycles(size);
    cycles = aligned;

    if ((linear & (size - 1)) == 0)
        return 0;

    u32 offset = linear & 3;
    cycles = m_bus.width == 2 ? aligned + (linear & 1) : (offset + size > 4 ? 2 : 1);
    return (cycles - aligned) * ((2ULL << 16) + m_bus.cycle_refresh) + (offset + size > 4 ? m_bus.misalign : 0);
}

u64 I386::GetBusRepeatTime(const BusRepeat& repeat, u32 elements) const
{
    return ((elements + 1) >> 1) * repeat.element[0] + (elements >> 1) * repeat.element[1];
}

// Row changes of the first elements, by runs between 1 KB boundaries. row is the open row in and out
u32 I386::CountBusRepeatChanges(const BusRepeat& repeat, u32 elements, u32 size, bool move, bool reverse,
    u32& row) const
{
    bool source_dram = move && repeat.source_class == GT_BUS_DRAM;
    bool destination_dram = repeat.destination_class == GT_BUS_DRAM;
    s64 step = reverse ? -(s64)size : (s64)size;
    u32 changes = 0;
    u32 done = 0;

    while (done < elements && (source_dram || destination_dram))
    {
        u32 source = (u32)(repeat.source + step * done);
        u32 destination = (u32)(repeat.destination + step * done);
        u32 run = elements - done;

        if (destination_dram)
        {
            u32 room = reverse ? (destination & 0x3FF) / size + 1 : (0x400 - (destination & 0x3FF)) / size;
            run = MIN(run, MAX(room, 1U));
        }

        if (source_dram)
        {
            u32 room = reverse ? (source & 0x3FF) / size + 1 : (0x400 - (source & 0x3FF)) / size;
            run = MIN(run, MAX(room, 1U));
        }

        if (source_dram && destination_dram)
        {
            u32 alternate = (source >> 10) != (destination >> 10) ? 1 : 0;
            changes += ((source >> 10) != row ? 1 : 0) + alternate + (run - 1) * 2 * alternate;
            row = destination >> 10;
        }
        else
        {
            u32 only = source_dram ? source >> 10 : destination >> 10;
            changes += only != row ? 1 : 0;
            row = only;
        }

        done += run;
    }

    return changes;
}

// Elements that start before the budget runs out, as the interpreter stops, however batches split
u32 I386::LimitBusRepeat(const BusRepeat& repeat, u32 elements, u32 size, bool move, bool reverse, u32 budget,
    u32 iteration) const
{
    u32 low = 1;
    u32 high = elements;

    while (low < high)
    {
        u32 middle = low + (high - low) / 2;
        u32 row = m_bus_row & ~k_i386_bus_row_stored;
        u64 fraction = (u32)m_bus_fraction + GetBusRepeatTime(repeat, middle) +
            (u64)CountBusRepeatChanges(repeat, middle, size, move, reverse, row) * m_bus.string_row;
        u64 clocks = (u64)middle * iteration + (fraction >> 16);

        if (clocks >= budget)
            high = middle;
        else
            low = middle + 1;
    }

    return low;
}

u32 I386::AddBusRepeat(const BusRepeat& repeat, u32 elements, u32 size, bool move, bool reverse)
{
    u32 row = m_bus_row & ~k_i386_bus_row_stored;
    u64 total = GetBusRepeatTime(repeat, elements) +
        (u64)CountBusRepeatChanges(repeat, elements, size, move, reverse, row) * m_bus.string_row +
        (u32)m_bus_fraction;

    m_bus_row = row;
    m_bus_queue = k_i386_bus_queue;
    m_bus_fraction = (s32)(total & 0xFFFF);
    m_bus_cycle_counts[repeat.destination_class][1] += elements * GetBusCycles(size);

    if (move)
        m_bus_cycle_counts[repeat.source_class][0] += elements * GetBusCycles(size);

    return (u32)(total >> 16);
}

// Once per CPU access, though Memory may split it into bytes
void I386::AddBusSlowAccess(u32 physical, u32 size, bool write)
{
    u8 bus_class = m_bus_page_class[physical >> 12];
    int k = write ? 1 : 0;

    if (bus_class == GT_BUS_FMR && size > 1)
        m_bus_fraction += (s32)(m_bus.fmr_word[k] * (size >> 1));
    else
        m_bus_fraction += (s32)(m_bus.slow[bus_class][k] * (bus_class == GT_BUS_PCM ? size : 1));
}

#if !defined(GT_DISABLE_DYNAREC)
// access: size, 08h write, 10h RMW, 20h code in DRAM with its row from bit 8, 40h one-byte instruction
template<bool i486>
void I386::DynarecBusAccess(I386* cpu, uintptr_t linear, uintptr_t access)
{
    u32 size = (u32)(access & 7);
    u32 code_row = ((access & 0x20) != 0 ? (u32)(access >> 8) & k_i386_bus_code_row : k_i386_bus_code_row) |
        ((access & 0x40) != 0 ? 0x80000000U : 0);
    bool both = (access & 0x10) != 0;
    bool write = (access & 0x08) != 0;

    cpu->m_bus_fraction -= (s32)cpu->GetBusDramFold(size, both || !write, both || write, code_row, i486);

    if (both)
    {
        cpu->AddBusAccess<i486>((u32)linear, size, 0, code_row);
        cpu->AddBusAccess<i486>((u32)linear, size, 1, code_row);
    }
    else
        cpu->AddBusAccess<i486>((u32)linear, size, write ? 1 : 0, code_row);
}

template void I386::DynarecBusAccess<false>(I386* cpu, uintptr_t linear, uintptr_t access);
template void I386::DynarecBusAccess<true>(I386* cpu, uintptr_t linear, uintptr_t access);
#endif
