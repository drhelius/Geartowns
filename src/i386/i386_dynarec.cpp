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
#include "i386_dynarec_emit.h"
#include "exec_memory.h"
#include "../common/profiler.h"
#include "../system/memory.h"

#if !defined(GT_DISABLE_DYNAREC)

static const u32 k_dynarec_none = 0xFFFFFFFFU;
static const u8 k_dynarec_counting = 0;
static const u8 k_dynarec_compiled = 1;
static const u8 k_dynarec_interpret = 2;
static const u8 k_dynarec_dead = 3;

// Where the dispatcher stands: inside a run of interpreted instructions, at a start that is counted, or where a block
// ran to its end, whose successor is hot already
static const u8 k_dynarec_entry_none = 0;
static const u8 k_dynarec_entry_count = 1;
static const u8 k_dynarec_entry_hot = 2;

static const u32 k_dynarec_block_bytes = 64;
static const u32 k_dynarec_max_watch_bytes = 512;
static const u32 k_dynarec_block_code = 64 * 1024;
static const u32 k_dynarec_bus_runs = GT_DYNAREC_MAX_BLOCKS * 4;
// Two per instruction of the longest block
static const u32 k_dynarec_max_bus_runs = 128;
static const u32 k_dynarec_default_threshold = 8;
static const u32 k_dynarec_default_block_limit = 64;

// A RAM page whose code keeps being rewritten, or whose data is written very often through its checked path, runs in
// the interpreter for k_dynarec_cool_periods cooling periods, which gives it its fast writes back
// Each eviction doubles the next one, up to k_dynarec_max_backoff times, and each quiet period halves it again
static const u8 k_dynarec_hot_rewrites = 16;
static const u16 k_dynarec_hot_writes = 4096;
static const u8 k_dynarec_cool_periods = 8;
static const u8 k_dynarec_max_backoff = 4;
static const u64 k_dynarec_cool_period_clocks = 1 << 20;

// A block watches its own bytes and the bytes of the transfer targets it timed when it was compiled
INLINE static const u8* GetDynarecWatchHost(const u8* host, u32 eip, u32 watch_eip)
{
    return host - (eip - watch_eip);
}

INLINE static u32 HashDynarecKey(const u8* host, u32 eip)
{
    u32 value = (u32)(uintptr_t)host ^ (eip * 0x9E3779B1U);
    return (value ^ (value >> 15)) & (GT_DYNAREC_MAX_BLOCKS - 1);
}

void I386::EnableDynarec(bool enable)
{
    if (!enable && m_dynarec_enabled)
        FlushDynarec();

    m_dynarec_enabled = enable && !m_dynarec_failed;

    if (!m_dynarec_enabled || IsValidPointer(m_dynarec_memory))
        return;

    m_dynarec_memory = new ExecMemory();

    // Without executable memory the CPU stays in the interpreter, IsDynarecEnabled tells the frontend
    if (!m_dynarec_memory->Init(GT_DYNAREC_CODE_SIZE))
    {
        SafeDelete(m_dynarec_memory);
        m_dynarec_failed = true;
        m_dynarec_enabled = false;
        return;
    }

    DynarecEmitter::InitLightning();

    m_dynarec_blocks = new DynarecBlock[GT_DYNAREC_MAX_BLOCKS];
    m_dynarec_hash = new u32[GT_DYNAREC_MAX_BLOCKS];
    m_dynarec_bytes = new u8[GT_DYNAREC_MAX_BLOCKS * k_dynarec_block_bytes];
    m_dynarec_bus_runs = new DynarecBusRun[k_dynarec_bus_runs];
    m_dynarec_region_generation = 0;

    // The entry function stays at the start of the code memory, flushes start after it
    DynarecEmitter emitter(this);
    u32 size = 0;
    u32 exit_offset = 0;
    u32 link_offset = 0;

    m_dynarec_memory->BeginWrite();
    bool emitted = emitter.EmitEntry(m_dynarec_memory->GetWriteView(), k_dynarec_block_code, size, exit_offset,
        link_offset);
    m_dynarec_memory->EndWrite(0, size);

    if (!emitted)
    {
        SafeDelete(m_dynarec_memory);
        m_dynarec_failed = true;
        m_dynarec_enabled = false;
        return;
    }

    m_dynarec_entry = (DynarecEntry)(void*)m_dynarec_memory->GetExecuteView();
    m_dynarec_exit = m_dynarec_memory->GetExecuteView() + exit_offset;
    m_dynarec_link = m_dynarec_memory->GetExecuteView() + link_offset;
    m_dynarec_code_start = (size + 15) & ~15U;
    FlushDynarec();
}

bool I386::IsDynarecEnabled() const
{
    return m_dynarec_enabled;
}

void I386::GetDynarecInfo(I386_Dynarec_Info& info) const
{
    info.available = !m_dynarec_failed;
    info.enabled = m_dynarec_enabled;
    info.blocks = m_dynarec_block_count;
    info.code_used = m_dynarec_code_used;
    info.code_size = IsValidPointer(m_dynarec_memory) ? m_dynarec_memory->GetSize() : 0;
    info.flushes = m_dynarec_flushes;
    info.invalidations = m_dynarec_invalidations;
}

// Test hooks: blocks compile after threshold lookups and hold at most block_limit instructions
void I386::SetDynarecTuning(u32 threshold, u32 block_limit)
{
    m_dynarec_threshold = threshold != 0 ? threshold : k_dynarec_default_threshold;
    m_dynarec_block_limit = block_limit != 0 ? block_limit : k_dynarec_default_block_limit;
    FlushDynarec();
}

// Drops every block and gives every RAM page its CPU write pointers back
void I386::FlushDynarec()
{
    if (!IsValidPointer(m_dynarec_blocks))
        return;

    for (u32 page = 0; page < m_dynarec_page_count; page++)
    {
        if (m_dynarec_page_blocks[page] != k_dynarec_none)
            m_memory->SetCodePage(page, false);

        m_dynarec_page_blocks[page] = k_dynarec_none;
        m_dynarec_page_granules[page * 2] = 0;
        m_dynarec_page_granules[page * 2 + 1] = 0;
    }

    memset(m_dynarec_hash, 0xFF, GT_DYNAREC_MAX_BLOCKS * sizeof(u32));
    m_dynarec_block_count = 0;
    m_dynarec_bytes_used = 0;
    m_dynarec_bus_runs_used = 0;
    m_dynarec_code_used = m_dynarec_code_start;
    m_dynarec_link_block = k_dynarec_none;
    m_dynarec_flushes++;
}

// Region changes can move RAM, so they drop every block and size the page tables for the new RAM
void I386::CheckDynarecMemory()
{
    if (likely(m_memory->GetRegionGeneration() == m_dynarec_region_generation))
        return;

    FlushDynarec();
    SafeDeleteArray(m_dynarec_page_blocks);
    SafeDeleteArray(m_dynarec_page_granules);
    SafeDeleteArray(m_dynarec_page_heat);
    SafeDeleteArray(m_dynarec_page_writes);
    SafeDeleteArray(m_dynarec_page_cool);
    SafeDeleteArray(m_dynarec_page_backoff);

    m_dynarec_ram = m_memory->GetWorkingRAM();
    m_dynarec_page_count = IsValidPointer(m_dynarec_ram) ? (u32)(m_memory->GetWorkingRAMSize() >> 12) : 0;
    m_dynarec_region_generation = m_memory->GetRegionGeneration();

    if (m_dynarec_page_count == 0)
        return;

    m_dynarec_page_blocks = new u32[m_dynarec_page_count];
    m_dynarec_page_granules = new u64[m_dynarec_page_count * 2];
    m_dynarec_page_heat = new u8[m_dynarec_page_count];
    m_dynarec_page_writes = new u16[m_dynarec_page_count];
    m_dynarec_page_cool = new u8[m_dynarec_page_count];
    m_dynarec_page_backoff = new u8[m_dynarec_page_count];
    memset(m_dynarec_page_blocks, 0xFF, m_dynarec_page_count * sizeof(u32));
    memset(m_dynarec_page_granules, 0, m_dynarec_page_count * 2 * sizeof(u64));
    memset(m_dynarec_page_heat, 0, m_dynarec_page_count);
    memset(m_dynarec_page_writes, 0, m_dynarec_page_count * sizeof(u16));
    memset(m_dynarec_page_cool, 0, m_dynarec_page_count);
    memset(m_dynarec_page_backoff, 0, m_dynarec_page_count);
}

// RunFor with translated blocks: the interpreter steps whatever no block covers, with the same loop condition,
// step epilogue and interrupt checks, so the slice ends at the same instruction as in RunFor
template<bool timed>
I386_Run_Result I386::RunDynarec(u32 cycle_budget, GT_Bus_Access_Context& context, bool nmi_pending,
    bool intr_pending)
{
    I386_Run_Result total = {};
    total.instruction_completed = true;
    total.exception_vector = 0xFF;

    m_checked_valid = false;
    SetBusContext(context);
    UpdateStepMode();
    CheckDynarecMemory();
    m_dynarec_link_block = k_dynarec_none;

    u64 clocks = 0;
    u32 steps = 0;
    bool completed = true;
    bool interrupt_pending = nmi_pending || intr_pending;
    u8 entry = k_dynarec_entry_count;

    while (clocks + context.wait_cycles < cycle_budget)
    {
        if (likely(!m_step_slow && !m_state.repeat.active && (m_state.eflags & (I386_FLAG_TF | I386_FLAG_RF)) == 0))
        {
            DynarecBlock* block = NULL;
            m_dynarec_keep_entry = false;

            // Blocks never change IF or the interrupt shadow, so an interrupt that isn't ready now stays so
            if (likely(m_state.interrupt_shadow_steps == 0) &&
                !(unlikely(interrupt_pending) && IsInterruptReady(nmi_pending, intr_pending)))
                block = FindDynarecBlock(entry);

            // A flush in the lookup drops the pending link
            u32 link = m_dynarec_link_block;
            m_dynarec_link_block = k_dynarec_none;

            // A static exit of the previous block that led here in the same code window becomes a direct jump
            if (link != k_dynarec_none && IsValidPointer(block) && m_code_window_eip == m_dynarec_link_window)
                LinkDynarecBlock(link, m_dynarec_link_slot, (u32)(block - m_dynarec_blocks));

            if (IsValidPointer(block) && clocks + context.wait_cycles + block->fit_clocks < cycle_budget)
            {
                m_dynarec_remaining = (u32)(cycle_budget - clocks - context.wait_cycles);
                m_dynarec_wait = context.wait_cycles;
                m_dynarec_start = (u32)clocks + context.wait_cycles;
                m_dynarec_clocks = 0;
                m_dynarec_steps = 0;
                m_dynarec_dirty = false;
                m_dynarec_current = (u32)(block - m_dynarec_blocks);
                m_step.instruction_completed = true;
                m_step.end_batch = false;
                m_step.exception = false;

                u32 exit = m_dynarec_entry(this, block->code);
                u32 last = m_dynarec_current;

                clocks += m_dynarec_clocks;
                steps += m_dynarec_steps;
                completed = m_step.instruction_completed;
                entry = (exit & k_dynarec_exit_end) != 0 ? k_dynarec_entry_hot : k_dynarec_entry_count;

                if ((exit & k_dynarec_exit_link) != 0)
                {
                    m_dynarec_link_block = last;
                    m_dynarec_link_slot = (exit & k_dynarec_exit_slot) != 0 ? 1 : 0;
                    m_dynarec_link_window = m_code_window_eip;
                }

                m_dynarec_current = k_dynarec_none;

                if ((exit & k_dynarec_exit_transfer) != 0)
                {
                    u32 components = GetNextInstructionComponents();
                    clocks += components;

                    if (timed && (exit & k_dynarec_exit_prefetch) != 0)
                        m_bus_queue = MIN(m_bus_queue + components * m_bus_code_rate, k_i386_bus_queue);
                    else if (timed)
                    {
                        // A taken transfer whose target the block couldn't see, as AddBusTransfer
                        u32 add = 0;
                        u32 linear = m_state.segments[I386_SEGMENT_CS].base + m_state.eip;
                        m_bus_queue = GetBusTransferLevel(linear, components, add);

                        if ((exit & k_dynarec_exit_access) != 0)
                            add += GetBusTransferRow(m_state.eip);

                        m_bus_fraction += (s32)add;
                        clocks += CarryBusTime();
                    }
                }

#if !defined(GT_DISABLE_DISASSEMBLER)
                if ((exit & k_dynarec_exit_return) != 0)
                    TrackReturn(true);
#endif

                if (unlikely(m_step.end_batch || context.end_batch || m_state.halted || m_state.shutdown))
                {
                    if (m_step.exception)
                        CopyStepException(total);

                    total.end_batch = true;
                    break;
                }

                if (unlikely(interrupt_pending) && IsInterruptReady(nmi_pending, intr_pending))
                    break;

                continue;
            }

            if (!RunFastStep<timed>(total, context, clocks, steps, completed, interrupt_pending, nmi_pending,
                intr_pending))
                break;

            // Instructions that follow an interpreted one in sequence belong to the block that starts before them.
            // An entry that couldn't start a block, out of the code window or at an instruction that must be
            // interpreted, moves on to the next instruction
            u8 next = m_state.eip != m_instruction.next_eip ? k_dynarec_entry_count : k_dynarec_entry_none;
            entry = m_dynarec_keep_entry ? MAX(entry, next) : next;
        }
        else
        {
            entry = k_dynarec_entry_count;
            m_dynarec_link_block = k_dynarec_none;
            total.cycles = clocks;
            total.steps = steps;
            total.instruction_completed = completed;

            bool more = RunForSlowStep(total, cycle_budget, nmi_pending, intr_pending);

            clocks = total.cycles;
            steps = total.steps;
            completed = total.instruction_completed;

            if (!more)
                break;
        }
    }

    m_dynarec_cool_clocks += clocks;

    if (unlikely(m_dynarec_cool_clocks >= k_dynarec_cool_period_clocks))
        CoolDynarecPages();

    total.cycles = clocks + (timed ? CarryBusTime() : 0);
    total.steps = steps + m_poll_steps;
    total.instruction_completed = completed;
    m_poll_steps = 0;
    m_poll_active = false;
    return total;
}

template I386_Run_Result I386::RunDynarec<false>(u32 cycle_budget, GT_Bus_Access_Context& context, bool nmi_pending,
    bool intr_pending);
template I386_Run_Result I386::RunDynarec<true>(u32 cycle_budget, GT_Bus_Access_Context& context, bool nmi_pending,
    bool intr_pending);

// The block at CS:EIP when it is compiled, valid and inside the code window, which opens as StartInstruction opens it
// Entry points are counted and compiled once they get hot, successors of blocks that ran to their end right away
INLINE I386::DynarecBlock* I386::FindDynarecBlock(u8 entry)
{
    u32 eip = m_state.eip;

    if (unlikely(eip - m_code_window_eip >= m_code_window_size))
        OpenCodeWindow(eip);

    u32 offset = eip - m_code_window_eip;

    if (unlikely(offset >= m_code_window_size))
    {
        m_dynarec_keep_entry = true;
        return NULL;
    }

    const u8* host = m_code_window + offset;

    // A cooling page has no blocks and compiles none
    if (host >= m_dynarec_ram && host < m_dynarec_ram + ((size_t)m_dynarec_page_count << 12) &&
        m_dynarec_page_cool[(host - m_dynarec_ram) >> 12] != 0)
        return NULL;

    u64 key = ((u64)m_state.segments[I386_SEGMENT_CS].limit << 8) |
        (m_default_size == 4 ? k_dynarec_key_default32 : 0) |
        ((m_state.cr0 & 0x80000000U) != 0 ? k_dynarec_key_paging : 0) | (m_stack32 ? k_dynarec_key_stack32 : 0) |
        (u8)(m_state.execution_mode << 3);
    u32 index = LookupDynarecBlock(host, eip, key);

    if (index == k_dynarec_none)
    {
        if (entry == k_dynarec_entry_none)
            return NULL;

        if (m_dynarec_block_count == GT_DYNAREC_MAX_BLOCKS)
            FlushDynarec();

        u32 hash = HashDynarecKey(host, eip);
        index = m_dynarec_block_count++;
        DynarecBlock& block = m_dynarec_blocks[index];
        block.host = host;
        block.eip = eip;
        block.key = key;
        block.state = k_dynarec_counting;
        block.hits = 0;
        block.page = k_dynarec_none;
        block.link[0] = k_dynarec_none;
        block.link[1] = k_dynarec_none;
        block.hash_next = m_dynarec_hash[hash];
        m_dynarec_hash[hash] = index;
    }

    DynarecBlock& block = m_dynarec_blocks[index];

    if (unlikely(block.state != k_dynarec_compiled))
    {
        if (block.state == k_dynarec_counting && entry != k_dynarec_entry_none &&
            (++block.hits >= m_dynarec_threshold || entry == k_dynarec_entry_hot))
            CompileDynarecBlock(block, m_code_window_size - offset);

        m_dynarec_keep_entry = block.state == k_dynarec_interpret;

        if (block.state != k_dynarec_compiled)
            return NULL;

        // The first block of a RAM page refreshes the CPU memory pointers, which closes the code window
        if (unlikely(eip - m_code_window_eip >= m_code_window_size))
        {
            OpenCodeWindow(eip);
            offset = eip - m_code_window_eip;

            if (offset >= m_code_window_size || m_code_window + offset != host)
                return NULL;
        }
    }

    if (unlikely(block.epoch != m_dynarec_epoch) && !VerifyDynarecBlock(block))
        return NULL;

    u32 watch_offset = block.watch_eip - m_code_window_eip;

    if (unlikely(watch_offset >= m_code_window_size || watch_offset + block.watch_length > m_code_window_size ||
        block.eip_limit > m_state.segments[I386_SEGMENT_CS].limit))
        return NULL;

    return &block;
}

u32 I386::LookupDynarecBlock(const u8* host, u32 eip, u64 key) const
{
    u32 index = m_dynarec_hash[HashDynarecKey(host, eip)];

    while (index != k_dynarec_none)
    {
        const DynarecBlock& block = m_dynarec_blocks[index];

        if (block.host == host && block.eip == eip && block.key == key)
            return index;

        index = block.hash_next;
    }

    return index;
}

// Only main RAM and ROM pages are compiled, everything else runs in the interpreter
NO_INLINE void I386::CompileDynarecBlock(DynarecBlock& block, u32 available)
{
    u32 page = k_dynarec_none;

    if (block.host >= m_dynarec_ram && block.host < m_dynarec_ram + ((size_t)m_dynarec_page_count << 12))
        page = (u32)((block.host - m_dynarec_ram) >> 12);
    else if (!m_memory->IsReadOnlyHost(block.host))
    {
        block.state = k_dynarec_interpret;
        return;
    }

    if (m_dynarec_code_used + k_dynarec_block_code > m_dynarec_memory->GetSize() ||
        m_dynarec_bytes_used + k_dynarec_max_watch_bytes > GT_DYNAREC_MAX_BLOCKS * k_dynarec_block_bytes ||
        m_dynarec_bus_runs_used + k_dynarec_max_bus_runs > k_dynarec_bus_runs)
    {
        FlushDynarec();
        return;
    }

    DynarecEmitter emitter(this);
    u32 code_size = 0;
    u32 bus_runs = m_dynarec_bus_runs_used;

    m_dynarec_memory->BeginWrite();
    bool compiled = emitter.Compile(block, available, m_dynarec_block_limit,
        m_dynarec_memory->GetWriteView() + m_dynarec_code_used, k_dynarec_block_code, code_size);
    m_dynarec_memory->EndWrite(m_dynarec_code_used, code_size);

    if (!compiled)
    {
        m_dynarec_bus_runs_used = bus_runs;
        block.state = k_dynarec_interpret;
        return;
    }

    block.code = m_dynarec_memory->GetExecuteView() + m_dynarec_code_used;
    block.state = k_dynarec_compiled;
    block.epoch = m_dynarec_epoch;
    block.copy = m_dynarec_bytes_used;
    block.page = page;
    memcpy(m_dynarec_bytes + block.copy, GetDynarecWatchHost(block.host, block.eip, block.watch_eip),
        block.watch_length);
    m_dynarec_bytes_used += (block.watch_length + 3) & ~3U;
    m_dynarec_code_used += (code_size + 15) & ~15U;

    if (page != k_dynarec_none)
        AddDynarecPageBlock((u32)(&block - m_dynarec_blocks));

    // Instructions inside the block were counted while they ran in the interpreter. They only become blocks of
    // their own if something keeps jumping to them
    for (int i = 1; i < emitter.GetInstructionCount(); i++)
    {
        u32 eip = emitter.GetInstructionEIP(i);
        u32 inner = LookupDynarecBlock(block.host + (eip - block.eip), eip, block.key);

        if (inner != k_dynarec_none && m_dynarec_blocks[inner].state == k_dynarec_counting)
            m_dynarec_blocks[inner].hits = 0;
    }
}

// Linked blocks share a code window and were compiled for the same CS limit
void I386::LinkDynarecBlock(u32 from, u8 slot, u32 to)
{
    DynarecBlock& block = m_dynarec_blocks[from];
    const DynarecBlock& next = m_dynarec_blocks[to];

    if (block.state != k_dynarec_compiled || block.key != next.key)
        return;

    block.link[slot] = to;
}

// After a state load or a reset the bytes of a block may have changed without a write the CPU saw
bool I386::VerifyDynarecBlock(DynarecBlock& block)
{
    if (memcmp(GetDynarecWatchHost(block.host, block.eip, block.watch_eip), m_dynarec_bytes + block.copy,
        block.watch_length) == 0)
    {
        block.epoch = m_dynarec_epoch;
        return true;
    }

    u32 index = (u32)(&block - m_dynarec_blocks);
    u32 page = block.page;
    RemoveDynarecBlock(index);

    if (page != k_dynarec_none && m_dynarec_page_blocks[page] == k_dynarec_none)
        m_memory->SetCodePage(page, false);

    return false;
}

// The first block of a RAM page takes the CPU write pointers away from it, so every write to it reaches Memory,
// which reports it to InvalidateDynarecCode
void I386::AddDynarecPageBlock(u32 index)
{
    DynarecBlock& block = m_dynarec_blocks[index];
    u32 page = block.page;
    u32 first = (u32)(GetDynarecWatchHost(block.host, block.eip, block.watch_eip) - m_dynarec_ram) & 0xFFF;
    u32 last = first + block.watch_length - 1;

    for (u32 granule = first >> 5; granule <= (last >> 5); granule++)
        m_dynarec_page_granules[page * 2 + (granule >> 6)] |= 1ULL << (granule & 63);

    bool first_block = m_dynarec_page_blocks[page] == k_dynarec_none;
    block.page_next = m_dynarec_page_blocks[page];
    m_dynarec_page_blocks[page] = index;

    if (first_block)
    {
        m_memory->SetCodePage(page, true);
        RefreshMemoryPointers();
    }
}

void I386::RemoveDynarecBlock(u32 index)
{
    DynarecBlock& block = m_dynarec_blocks[index];
    u32 hash = HashDynarecKey(block.host, block.eip);
    u32* link = &m_dynarec_hash[hash];

    while (*link != index)
        link = &m_dynarec_blocks[*link].hash_next;

    *link = block.hash_next;

    if (block.page != k_dynarec_none)
    {
        link = &m_dynarec_page_blocks[block.page];

        while (*link != index)
            link = &m_dynarec_blocks[*link].page_next;

        *link = block.page_next;
    }

    if (index == m_dynarec_current)
        m_dynarec_dirty = true;

    block.state = k_dynarec_dead;
    block.page = k_dynarec_none;
    m_dynarec_invalidations++;
}

void I386::DynarecCodeWrite(void* context, u32 offset, u32 size)
{
    ((I386*)context)->InvalidateDynarecCode(offset, size);
}

// A write to a RAM page with blocks removes the blocks whose bytes it overlaps
void I386::InvalidateDynarecCode(u32 offset, u32 size)
{
    u32 end = offset + size;

    for (u32 page = offset >> 12; page <= ((end - 1) >> 12) && page < m_dynarec_page_count; page++)
    {
        if (m_dynarec_page_blocks[page] == k_dynarec_none)
            continue;

        if (++m_dynarec_page_writes[page] >= k_dynarec_hot_writes)
        {
            EvictDynarecPage(page);
            continue;
        }

        u32 first = MAX(offset, page << 12) & 0xFFF;
        u32 last = (MIN(end, (page + 1) << 12) - 1) & 0xFFF;
        bool hit = false;

        for (u32 granule = first >> 5; granule <= (last >> 5); granule++)
            hit = hit || (m_dynarec_page_granules[page * 2 + (granule >> 6)] & (1ULL << (granule & 63))) != 0;

        if (!hit)
            continue;

        if (++m_dynarec_page_heat[page] >= k_dynarec_hot_rewrites)
        {
            EvictDynarecPage(page);
            continue;
        }

        u32 index = m_dynarec_page_blocks[page];
        m_dynarec_page_granules[page * 2] = 0;
        m_dynarec_page_granules[page * 2 + 1] = 0;

        while (index != k_dynarec_none)
        {
            DynarecBlock& block = m_dynarec_blocks[index];
            u32 next = block.page_next;
            u32 block_first = (u32)(GetDynarecWatchHost(block.host, block.eip, block.watch_eip) - m_dynarec_ram) &
                0xFFF;
            u32 block_last = block_first + block.watch_length - 1;

            if (block_first <= last && first <= block_last)
                RemoveDynarecBlock(index);
            else
            {
                for (u32 granule = block_first >> 5; granule <= (block_last >> 5); granule++)
                    m_dynarec_page_granules[page * 2 + (granule >> 6)] |= 1ULL << (granule & 63);
            }

            index = next;
        }

        if (m_dynarec_page_blocks[page] == k_dynarec_none)
            m_memory->SetCodePage(page, false);
    }
}

void I386::EvictDynarecPage(u32 page)
{
    while (m_dynarec_page_blocks[page] != k_dynarec_none)
        RemoveDynarecBlock(m_dynarec_page_blocks[page]);

    m_dynarec_page_granules[page * 2] = 0;
    m_dynarec_page_granules[page * 2 + 1] = 0;
    m_dynarec_page_cool[page] = (u8)(k_dynarec_cool_periods << m_dynarec_page_backoff[page]);
    m_memory->SetCodePage(page, false);

    if (m_dynarec_page_backoff[page] < k_dynarec_max_backoff)
        m_dynarec_page_backoff[page]++;
}

void I386::CoolDynarecPages()
{
    m_dynarec_cool_clocks = 0;

    for (u32 page = 0; page < m_dynarec_page_count; page++)
    {
        if (m_dynarec_page_cool[page] != 0)
            m_dynarec_page_cool[page]--;
        else if (m_dynarec_page_backoff[page] != 0 && m_dynarec_page_heat[page] == 0 && m_dynarec_page_writes[page] == 0)
            m_dynarec_page_backoff[page]--;

        m_dynarec_page_heat[page] = 0;
        m_dynarec_page_writes[page] = 0;
    }
}

// Runs one instruction of a block in the interpreter, as the fast loop of RunFor does
// Returns zero while the block can go on: nothing ended the batch, the code window and the block are still valid
// and the slice budget covers every instruction up to the next check
uintptr_t I386::DynarecStep(I386* cpu, uintptr_t next_check, uintptr_t native_clocks, uintptr_t before,
    uintptr_t settle, uintptr_t revert)
{
    StepState& step = cpu->m_step;
    u16 old_task = cpu->m_state.task_register.selector;
    u32 level = cpu->m_bus_queue;

    if (settle != 0)
    {
        const DynarecBusRun& run = *(const DynarecBusRun*)settle;
        s32 value = MIN(MAX((s32)level + run.offset, (s32)run.low), (s32)run.high);
        cpu->m_bus_fraction += cpu->GetDynarecBusTime(run, level);
        cpu->m_bus_queue = (u32)value;
    }

    cpu->m_io_start_cycles = cpu->m_dynarec_start + cpu->m_dynarec_clocks + (u32)before +
        (cpu->m_bus_timed ? cpu->GetBusPending() : 0);

    step.clocks = 0;
    step.instruction_completed = true;
    step.end_batch = false;
    step.exception = false;

    if (unlikely(!cpu->ExecuteOPCode()))
        cpu->CompleteFault(old_task);
    else if (cpu->m_bus_timed && (step.instruction_completed || cpu->m_state.repeat.active))
        cpu->AddBusTime();

    if (unlikely(cpu->m_state.interrupt_shadow_steps > 0) && step.instruction_completed)
        cpu->CountInterruptShadow();

    cpu->m_dynarec_clocks += (u32)step.clocks - (u32)native_clocks;

    const GT_Bus_Access_Context& context = *cpu->m_bus_context;

    // A real mode load of a data segment register starts an interrupt shadow, which the dispatcher counts down
    if (step.end_batch || context.end_batch || cpu->m_state.halted || cpu->m_state.shutdown ||
        !step.instruction_completed || cpu->m_state.repeat.active || cpu->m_state.interrupt_shadow_steps != 0 ||
        cpu->m_dynarec_dirty || context.wait_cycles != cpu->m_dynarec_wait ||
        (s64)next_check + (s32)cpu->m_dynarec_clocks >= (s64)cpu->m_dynarec_remaining)
        return 1;

    // Any TLB way change closes the code window. The next instruction opens it again as StartInstruction would, and
    // the block goes on while the window still covers its bytes
    if (unlikely(cpu->m_code_window_size == 0))
    {
        const DynarecBlock& block = cpu->m_dynarec_blocks[cpu->m_dynarec_current];
        cpu->OpenCodeWindow(cpu->m_state.eip);
        u32 watch_offset = block.watch_eip - cpu->m_code_window_eip;

        if (watch_offset >= cpu->m_code_window_size || watch_offset + block.watch_length > cpu->m_code_window_size ||
            cpu->m_code_window + (block.eip - cpu->m_code_window_eip) != block.host)
            return 1;
    }

    if (revert != 0)
    {
        cpu->m_bus_fraction -= cpu->GetDynarecBusTime(*(const DynarecBusRun*)revert, level);
        cpu->m_bus_queue = level;
    }

    return 0;
}

s32 I386::GetDynarecBusTime(const DynarecBusRun& run, u32 level) const
{
    s32 stall = run.knee - (s32)level;
    return (s32)((u32)MAX(stall, 0) * run.cost + run.add);
}

#if !defined(GT_DISABLE_DISASSEMBLER)
// The debugger call stack entry of a translated CALL
void I386::DynarecCall(I386* cpu)
{
    const I386_Segment& code = cpu->m_state.segments[I386_SEGMENT_CS];
    const DynarecBlock& caller = cpu->m_dynarec_blocks[cpu->m_dynarec_current];
    cpu->PushCallStack(code.selector, code.base, caller.call_eip, caller.call_next_eip, I386_CALL, 0);
}
#endif

#else

void I386::EnableDynarec(bool enable)
{
    UNUSED(enable);
}

bool I386::IsDynarecEnabled() const
{
    return false;
}

void I386::GetDynarecInfo(I386_Dynarec_Info& info) const
{
    memset(&info, 0, sizeof(info));
}

void I386::SetDynarecTuning(u32 threshold, u32 block_limit)
{
    UNUSED(threshold);
    UNUSED(block_limit);
}

#endif
