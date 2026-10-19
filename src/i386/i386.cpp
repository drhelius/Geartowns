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
#include "exec_memory.h"
#include "../common/profiler.h"
#include "../common/trace_logger.h"
#include "../system/memory.h"
#include "../system/io.h"
#include "../common/state_serializer.h"

const u8 I386::k_szp_flags[256] =
{
    0x44, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04,
    0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00,
    0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00,
    0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04,
    0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00,
    0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04,
    0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04,
    0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00,
    0x80, 0x84, 0x84, 0x80, 0x84, 0x80, 0x80, 0x84, 0x84, 0x80, 0x80, 0x84, 0x80, 0x84, 0x84, 0x80,
    0x84, 0x80, 0x80, 0x84, 0x80, 0x84, 0x84, 0x80, 0x80, 0x84, 0x84, 0x80, 0x84, 0x80, 0x80, 0x84,
    0x84, 0x80, 0x80, 0x84, 0x80, 0x84, 0x84, 0x80, 0x80, 0x84, 0x84, 0x80, 0x84, 0x80, 0x80, 0x84,
    0x80, 0x84, 0x84, 0x80, 0x84, 0x80, 0x80, 0x84, 0x84, 0x80, 0x80, 0x84, 0x80, 0x84, 0x84, 0x80,
    0x84, 0x80, 0x80, 0x84, 0x80, 0x84, 0x84, 0x80, 0x80, 0x84, 0x84, 0x80, 0x84, 0x80, 0x80, 0x84,
    0x80, 0x84, 0x84, 0x80, 0x84, 0x80, 0x80, 0x84, 0x84, 0x80, 0x80, 0x84, 0x80, 0x84, 0x84, 0x80,
    0x80, 0x84, 0x84, 0x80, 0x84, 0x80, 0x80, 0x84, 0x84, 0x80, 0x80, 0x84, 0x80, 0x84, 0x84, 0x80,
    0x84, 0x80, 0x80, 0x84, 0x80, 0x84, 0x84, 0x80, 0x80, 0x84, 0x84, 0x80, 0x84, 0x80, 0x80, 0x84
};

static const u32 k_i386_real_interrupt_entry_clocks = 33;

I386::I386()
{
    InitPointer(m_memory);
    InitPointer(m_io);
    InitPointer(m_trace);
    m_trace_enabled = false;
    m_trace_count = 0;

#if defined(GT_DISABLE_DISASSEMBLER)
    InitPointer(m_disassembler_pages);
#else
    m_disassembler_pages = new I386_Disassembler_Record**[k_i386_disassembler_pages]();
#endif
    m_disassembler_generation = 0;
    m_run_to_breakpoint = 0;
    m_run_to_stack_pointer = 0;
    m_run_to_stack_selector = 0;
    m_run_to_stack_check = false;
    m_breakpoint_hit_address = 0;
    m_step_call_return_linear = 0;
    m_step_call = false;
    m_task_call_entered = false;
    m_run_to_breakpoint_enabled = false;
    m_breakpoint_hit = false;
    m_run_to_hit = false;
    memset(&m_breakpoint_hit_info, 0, sizeof(m_breakpoint_hit_info));
    m_debugger_checks = false;
    m_debugger_memory_checks = false;
    m_debugger_io_checks = false;
    m_debugger_interrupt_checks = false;
    m_debugger_exec_checks = false;
    m_execute_filter_all = false;
    memset(m_execute_filter, 0, sizeof(m_execute_filter));
    memset(&m_poll, 0, sizeof(m_poll));
    m_poll_batch = 0;
    m_poll_budget = 0;
    m_poll_steps = 0;
    m_poll_active = false;
    m_poll_enabled = true;
    m_debugger_hit_pending = false;
    memset(&m_last_exception, 0, sizeof(m_last_exception));
    m_irq_breakpoints = 0;
    m_irq_breakpoints_disabled = 0;
    m_external_line = -1;
    InitPointer(m_trace_logger);
    InitPointer(m_profiler);
    m_reset_edx = 0x00000300;
    m_cr0_fixed = 0x7FFFFFE0;
    memset(&m_bus, 0, sizeof(m_bus));
    m_bus_timed = false;
    InitPointer(m_bus_page_class);
    m_trace_internal = false;
    m_trace_cpu = false;
    m_profiler_active = false;
    m_vblank_watch_read = false;
    m_vblank_watch_write = false;
    m_vblank_watch_address = 0;
    m_vblank_watch_hit = false;
    m_vblank_watch_armed = false;
    m_vblank_watch_misses = 0;

#if !defined(GT_DISABLE_DYNAREC)
    m_dynarec_enabled = false;
    m_dynarec_failed = false;
    InitPointer(m_dynarec_memory);
    InitPointer(m_dynarec_entry);
    InitPointer(m_dynarec_exit);
    InitPointer(m_dynarec_link);
    m_dynarec_code_start = 0;
    m_dynarec_code_used = 0;
    InitPointer(m_dynarec_blocks);
    m_dynarec_block_count = 0;
    InitPointer(m_dynarec_hash);
    InitPointer(m_dynarec_bytes);
    m_dynarec_bytes_used = 0;
    InitPointer(m_dynarec_bus_runs);
    m_dynarec_bus_runs_used = 0;
    InitPointer(m_dynarec_page_blocks);
    InitPointer(m_dynarec_page_granules);
    InitPointer(m_dynarec_page_heat);
    InitPointer(m_dynarec_page_writes);
    InitPointer(m_dynarec_page_cool);
    InitPointer(m_dynarec_page_backoff);
    m_dynarec_page_count = 0;
    InitPointer(m_dynarec_ram);
    m_dynarec_region_generation = 0;
    m_dynarec_epoch = 0;
    m_dynarec_threshold = 0;
    m_dynarec_block_limit = 0;
    m_dynarec_flushes = 0;
    m_dynarec_invalidations = 0;
    m_dynarec_cool_clocks = 0;
    m_dynarec_current = 0xFFFFFFFFU;
    m_dynarec_steps = 0;
    m_dynarec_link_block = 0xFFFFFFFFU;
    m_dynarec_link_slot = 0;
    m_dynarec_link_window = 0;
    m_dynarec_dirty = false;
    m_dynarec_keep_entry = false;
    m_dynarec_clocks = 0;
    m_dynarec_remaining = 0;
    m_dynarec_wait = 0;
    m_dynarec_start = 0;
    m_dynarec_spill = 0;
#endif

    SetDynarecTuning(0, 0);
    Reset();
}

I386::~I386()
{
    SafeDeleteArray(m_trace);
    ClearDisassemblerPages();
    SafeDeleteArray(m_disassembler_pages);
#if !defined(GT_DISABLE_DYNAREC)
    SafeDelete(m_dynarec_memory);
    SafeDeleteArray(m_dynarec_blocks);
    SafeDeleteArray(m_dynarec_hash);
    SafeDeleteArray(m_dynarec_bytes);
    SafeDeleteArray(m_dynarec_bus_runs);
    SafeDeleteArray(m_dynarec_page_blocks);
    SafeDeleteArray(m_dynarec_page_granules);
    SafeDeleteArray(m_dynarec_page_heat);
    SafeDeleteArray(m_dynarec_page_writes);
    SafeDeleteArray(m_dynarec_page_cool);
    SafeDeleteArray(m_dynarec_page_backoff);
#endif
}

void I386::Init(Memory* memory, IO* io)
{
    m_memory = memory;
    m_io = io;
#if !defined(GT_DISABLE_DYNAREC)
    m_memory->SetCodeWriteCallback(this, DynarecCodeWrite);
#endif

    m_disassembler_records.clear();
    m_disassembler_generation++;
    ClearDisassemblerPages();

    Reset();
}

void I386::Reset()
{
    InitPointer(m_read_pages);
    InitPointer(m_write_pages);
    InitPointer(m_bus_context);
    m_memory_generation = 0;
    m_io_start_cycles = 0;
    m_poll.batch = 0;
    m_poll_steps = 0;
    m_poll_active = false;
    m_state.execution_mode = I386_MODE_REAL;

    memset(&m_step, 0, sizeof(m_step));
    memset(&m_step_exception, 0, sizeof(m_step_exception));
    memset(m_state.registers, 0, sizeof(m_state.registers));
    memset(m_state.segments, 0, sizeof(m_state.segments));
    memset(&m_state.gdtr, 0, sizeof(m_state.gdtr));
    memset(&m_state.idtr, 0, sizeof(m_state.idtr));
    memset(&m_state.ldtr, 0, sizeof(m_state.ldtr));
    memset(&m_state.task_register, 0, sizeof(m_state.task_register));

    memset(m_state.debug_registers, 0, sizeof(m_state.debug_registers));
    memset(m_state.test_registers, 0, sizeof(m_state.test_registers));

    memset(&m_exception, 0, sizeof(m_exception));

    memset(&m_state.repeat, 0, sizeof(m_state.repeat));
    memset(&m_string, 0, sizeof(m_string));
    memset(&m_instruction, 0, sizeof(m_instruction));
    memset(&m_instruction_defaults, 0, sizeof(m_instruction_defaults));
    m_instruction_defaults.segment_override = 0xFF;

    m_address_clocks = 0;
    m_instruction_restored = false;
    InitPointer(m_passive_bytes);
    m_passive_count = 0;
    m_passive_index = 0;
    m_debug_step = false;
    m_step_slow = false;
    m_slow_memory = false;
    m_user_mode = false;
    InitPointer(m_code_window);
    m_code_window_eip = 0;
    m_code_window_size = 0;
    InitPointer(m_fetch_pointer);
    m_fetch_remaining = 0;
    m_bus_queue = 0;
    m_bus_row = k_i386_bus_no_row;
    m_bus_fraction = 0;
    UpdateBusCode(GT_BUS_NONE, 0);

    m_checked_eip = 0;
    m_checked_count = 0;
    m_checked_valid = false;

    memset(m_tlb, 0, sizeof(m_tlb));
    m_tlb_used = 0;
    ResetTLBReplacement();
    m_trace_count = 0;

    // Decoded rows are debugger history and survive a machine reset
    m_disassembler_call_stack.clear();

    if (IsValidPointer(m_profiler))
        m_profiler->ResetStack();

    m_run_to_breakpoint_enabled = false;
    m_breakpoint_hit = false;
    m_run_to_hit = false;
    m_breakpoint_hit_address = 0;
    m_debugger_hit_pending = false;
    UpdateExecuteFilter();
    ResetVBlankWatch();

    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
        SetRealModeSegment((I386_Segment_Register)i, 0);

    SetRealModeSegment(I386_SEGMENT_CS, 0xF000);
    m_state.segments[I386_SEGMENT_CS].base = 0xFFFF0000;

    m_state.eip = 0x0000FFF0;
    m_state.eflags = I386_FLAG_FIXED;
    m_state.idtr.limit = 0x03FF;

    m_state.cr0 = m_cr0_fixed;
    m_state.cr2 = 0;
    m_state.cr3 = 0;

    m_state.registers[I386_REG_EDX].value = m_reset_edx;
    m_state.debug_registers[6] = 0xFFFF1FF0;
    m_state.debug_registers[7] = 0x00000400;

    m_state.execution_mode = I386_MODE_REAL;
    m_state.current_privilege_level = 0;

    m_state.interrupt_shadow = I386_SHADOW_NONE;
    m_state.interrupt_shadow_steps = 0;

    m_state.halted = false;
    m_state.shutdown = false;
    m_state.nmi_blocked = false;
    m_external_event = false;

    m_debug_data_breakpoints = 0;
    m_state.last_exception_vector = 0xFF;
    m_last_exception.valid = false;

#if !defined(GT_DISABLE_DYNAREC)
    // Memory may have been rewritten without the CPU seeing it, so blocks check their bytes before running again
    m_dynarec_epoch++;
#endif

    UpdateSegmentFastPaths();
    UpdateDebugState();
}

NO_INLINE bool I386::ExecuteOPCodeDebug(const I386_State& before)
{
    m_debug_step = true;
    u16 old_task = m_state.task_register.selector;

    m_debug_data_breakpoints = 0;

    bool completed = ExecuteOPCode();
    m_debug_step = false;

    if (!completed)
        return false;

    if (m_step.instruction_completed && (m_state.eflags & I386_FLAG_RF) != 0)
    {
        bool preserve = !m_instruction.two_byte && m_instruction.opcode == 0xCF;

        if (m_state.task_register.selector != old_task)
            preserve = true;

        if (!preserve)
            m_state.eflags &= ~I386_FLAG_RF;
    }

    if (!m_step.exception)
    {
        bool single_step = m_step.instruction_completed &&
            (before.eflags & I386_FLAG_TF) != 0 &&
            m_state.task_register.selector == old_task &&
            (m_state.interrupt_shadow != I386_SHADOW_MOV_SS || m_state.interrupt_shadow_steps == 1);

        if (m_debug_data_breakpoints != 0 || single_step)
        {
            m_state.debug_registers[6] |= m_debug_data_breakpoints;

            if (single_step)
                m_state.debug_registers[6] |= 0x00004000U;

            RaiseException(1, I386_EXCEPTION_TRAP);
            m_exception.has_return_eip = true;
            m_exception.return_eip = m_state.eip;
            return false;
        }
    }

    return true;
}

// Delivers the exception raised by the step
// A failed delivery has already shut the CPU down
void I386::CompleteFault(u16 old_task)
{
    StepState& result = m_step;

    m_step_exception = {};
    m_step_exception.cycles = result.clocks;
    m_step_exception.instruction_completed = result.instruction_completed;
    m_step_exception.end_batch = result.end_batch;
    m_step_exception.exception_after_instruction =
        m_exception.exception_class == I386_EXCEPTION_TRAP || m_state.task_register.selector != old_task;

    ResolveException(*m_bus_context, m_step_exception);
    AddBusInterrupt(m_step_exception.cycles, false);

    result.clocks = m_step_exception.cycles;
    result.exception = m_step_exception.exception;
    result.end_batch = m_step_exception.end_batch;
}

I386_Run_Result I386::GetStepInfo() const
{
    I386_Run_Result result = {};
    result.cycles = m_step.clocks;
    result.steps = m_step.steps;
    result.instruction_completed = m_step.instruction_completed;
    result.end_batch = m_step.end_batch;
    result.exception = m_step.exception;
    result.exception_vector = 0xFF;

    if (m_step.exception)
    {
        result.exception_vector = m_step_exception.exception_vector;
        result.exception_after_instruction = m_step_exception.exception_after_instruction;
        result.exception_return_cs = m_step_exception.exception_return_cs;
        result.exception_return_eip = m_step_exception.exception_return_eip;
        result.exception_return_base = m_step_exception.exception_return_base;
        result.exception_source_eip = m_step_exception.exception_source_eip;
    }

    return result;
}

I386_Run_Result I386::RunFor(u32 cycle_budget, GT_Bus_Access_Context& context, bool nmi_pending, bool intr_pending)
{
    m_poll_batch++;
    m_poll_budget = cycle_budget;
    m_poll_active = m_poll_enabled;

    if (m_bus_timed)
        return RunTimed<true>(cycle_budget, context, nmi_pending, intr_pending);

    return RunTimed<false>(cycle_budget, context, nmi_pending, intr_pending);
}

template<bool timed>
I386_Run_Result I386::RunTimed(u32 cycle_budget, GT_Bus_Access_Context& context, bool nmi_pending, bool intr_pending)
{
#if !defined(GT_DISABLE_DYNAREC)
    if (m_dynarec_enabled)
        return RunDynarec<timed>(cycle_budget, context, nmi_pending, intr_pending);
#endif

    I386_Run_Result total = {};
    total.instruction_completed = true;
    total.exception_vector = 0xFF;

    m_checked_valid = false;
    SetBusContext(context);
    UpdateStepMode();

    // The fast path keeps the totals in locals
    // They are synchronized around slow steps
    u64 clocks = 0;
    u32 steps = 0;
    bool completed = true;
    bool interrupt_pending = nmi_pending || intr_pending;

    while (clocks + context.wait_cycles < cycle_budget)
    {
        if (likely(!m_step_slow && !m_state.repeat.active && (m_state.eflags & (I386_FLAG_TF | I386_FLAG_RF)) == 0))
        {
            if (!RunFastStep<timed>(total, context, clocks, steps, completed, interrupt_pending, nmi_pending,
                intr_pending))
                break;
        }
        else
        {
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

    total.cycles = clocks + (timed ? CarryBusTime() : 0);
    total.steps = steps + m_poll_steps;
    total.instruction_completed = completed;
    m_poll_steps = 0;
    m_poll_active = false;
    return total;
}

// Debug, trace, halt and REP continuation steps
// returns false when the batch must end
NO_INLINE bool I386::RunForSlowStep(I386_Run_Result& total, u32 cycle_budget, bool nmi_pending, bool intr_pending)
{
    GT_Bus_Access_Context& context = *m_bus_context;
    m_io_start_cycles = (u32)total.cycles + context.wait_cycles + (m_bus_timed ? GetBusPending() : 0);

    if (m_state.repeat.active && !IsInterruptReady(nmi_pending, intr_pending) && !m_state.halted && !m_state.shutdown)
    {
        u32 clocks = 0;
        u32 steps = RunRepeatBatch((u32)(cycle_budget - total.cycles - context.wait_cycles), clocks);

        if (steps != 0)
        {
            total.cycles += clocks;
            total.steps += steps;
            total.instruction_completed = m_step.instruction_completed;
            total.end_batch = total.end_batch || m_step.end_batch;

            // A shadow from the instruction before the REP ends with it, as on the slow path
            if (unlikely(m_state.interrupt_shadow_steps > 0) && m_step.instruction_completed)
            {
                CountInterruptShadow();
                return !m_step.end_batch && !IsInterruptReady(nmi_pending, intr_pending);
            }

            return !m_step.end_batch;
        }
    }

    RunCheckedStep();

    const StepState& step = m_step;
    total.cycles += step.clocks;
    total.steps += step.steps;
    total.instruction_completed = step.instruction_completed;
    total.end_batch = total.end_batch || step.end_batch;

    if (step.exception)
        CopyStepException(total);

    return !(step.steps == 0 || step.end_batch || m_state.halted || m_state.shutdown ||
        IsInterruptReady(nmi_pending, intr_pending));
}

void I386::CopyStepException(I386_Run_Result& total) const
{
    total.exception = true;
    total.exception_vector = m_step_exception.exception_vector;
    total.exception_after_instruction = m_step_exception.exception_after_instruction;
    total.exception_return_cs = m_step_exception.exception_return_cs;
    total.exception_return_eip = m_step_exception.exception_return_eip;
    total.exception_return_base = m_step_exception.exception_return_base;
    total.exception_source_eip = m_step_exception.exception_source_eip;
}

void I386::UpdateStepMode()
{
    m_step_slow = m_state.halted || m_state.shutdown || m_trace_enabled || (m_state.debug_registers[7] & 0xFF) != 0;
}

void I386::EnablePollSkip(bool enable)
{
    m_poll_enabled = enable;
}

// 8-bit encodings 4-7 are AH, CH, DH and BH
static u32 GetPollRegister(const u32* registers, int index, int width)
{
    if (width == 8)
        return index < 4 ? registers[index] & 0xFF : (registers[index & 3] >> 8) & 0xFF;

    return width == 16 ? registers[index] & 0xFFFF : registers[index];
}

static void SetPollRegister(u32* registers, int index, int width, u32 value)
{
    if (width == 8 && index < 4)
        registers[index] = (registers[index] & 0xFFFFFF00U) | (value & 0xFF);
    else if (width == 8)
        registers[index & 3] = (registers[index & 3] & 0xFFFF00FFU) | ((value & 0xFF) << 8);
    else if (width == 16)
        registers[index] = (registers[index] & 0xFFFF0000U) | (value & 0xFFFF);
    else
        registers[index] = value;
}

// Polling iterations that can't see a new value, an event or the batch end are counted instead of run
// Reads the loop's code passively when no code window is open, so skipping doesn't depend on host caches
void I386::SkipPolling(u16 port, int width)
{
    GT_Bus_Access_Context& context = *m_bus_context;
    PollState& poll = m_poll;
    u32 eip = m_instruction.start_eip;

    if ((s64)eip > m_code_limit)
    {
        poll.batch = 0;
        return;
    }

    u32 linear = m_state.segments[I386_SEGMENT_CS].base + eip;
    u32 page_offset = linear & 0xFFF;
    u32 before = MIN(eip, page_offset);
    u32 after = (u32)MIN((u64)m_code_limit - eip + 1, (u64)(0x1000 - page_offset));
    u32 start = m_io_start_cycles;
    bool repeated = poll.batch == m_poll_batch && poll.linear == linear && poll.port == port && poll.width == width &&
        poll.eflags == m_state.eflags && start > poll.start_cycles;

    for (int i = 0; repeated && i < I386_REG_COUNT; i++)
        repeated = poll.registers[i] == m_state.registers[i].value;

    u32 period = start - poll.start_cycles;
    poll.start_cycles = start;

    if (!repeated)
    {
        poll.batch = m_poll_batch;
        poll.linear = linear;
        poll.port = port;
        poll.width = (u8)width;
        poll.eflags = m_state.eflags;
        poll.code_size = 0;

        for (int i = 0; i < I386_REG_COUNT; i++)
            poll.registers[i] = m_state.registers[i].value;

        return;
    }

    if (context.end_batch || m_step.end_batch || m_step_slow || m_debugger_io_checks ||
        (m_state.eflags & (I386_FLAG_TF | I386_FLAG_RF)) != 0 || m_state.interrupt_shadow_steps != 0 ||
        !IsValidPointer(context.poll_port))
        return;

    u32 count = context.poll_port(context.synchronize_context, port, width) / period;
    count = start < m_poll_budget ? MIN(count, (m_poll_budget - 1 - start) / period) : 0;

    if (count == 0)
        return;

    // The previous read must have run this same code
    u32 back = MIN(before, (u32)(I386_POLL_CODE_SIZE / 3));
    u32 code_eip = eip - back;
    u32 code_size = MIN(back + after, (u32)I386_POLL_CODE_SIZE);
    u8 buffer[I386_POLL_CODE_SIZE];
    const u8* code = buffer;

    if (eip - m_code_window_eip < m_code_window_size)
        code = m_code_window + (eip - m_code_window_eip) - back;
    else
    {
        u32 physical = 0;

        if (!TranslateCurrentPassive(linear - back, physical))
            return;

        for (u32 i = 0; i < code_size; i++)
        {
            if (!m_memory->TryPeekPhysical(physical + i, buffer[i]))
                return;
        }
    }

    bool same_code = poll.code_eip == code_eip && poll.code_size == code_size && memcmp(poll.code, code, code_size) == 0;

    if (!same_code)
    {
        poll.code_eip = code_eip;
        poll.code_size = code_size;
        memcpy(poll.code, code, code_size);
        return;
    }

    u32 steps = 0;

    if (!IsPollingLoop(port, steps))
        return;

    m_step.clocks += (u64)count * period;
    m_poll_steps += count * steps;
    poll.start_cycles = start + count * period;
}

// Runs the loop on the current registers: register operands only, no exit taken, one backward branch to the IN
bool I386::IsPollingLoop(u32 port, u32& steps)
{
    const PollState& poll = m_poll;
    u32 registers[I386_REG_COUNT];
    u32 eflags = m_state.eflags;
    u32 in_eip = m_instruction.start_eip;
    u32 eip = m_instruction.next_eip;
    bool default32 = m_default_size == 4;
    bool returned = false;

    for (int i = 0; i < I386_REG_COUNT; i++)
        registers[i] = m_state.registers[i].value;

    for (u32 count = 1; count <= k_i386_poll_max_instructions; count++)
    {
        u32 offset = eip - poll.code_eip;

        if (offset >= poll.code_size)
            return false;

        InstructionContext c;
        m_passive_bytes = poll.code + offset;
        m_passive_count = MIN(poll.code_size - offset, (u32)GT_I386_MAX_INSTRUCTION_LENGTH);
        m_passive_index = 0;

        if (!DecodeInstructionPassive(c, eip, default32) || c.lock || c.repeat != 0 || c.memory_operand)
            return false;

        if (returned && eip == in_eip)
        {
            bool dx = c.opcode == 0xEC || c.opcode == 0xED;

            if (c.two_byte || c.opcode != m_instruction.opcode || c.operand_size != m_instruction.operand_size ||
                (dx && (registers[I386_REG_EDX] & 0xFFFF) != port))
                return false;

            steps = count;
            return true;
        }

        int width = c.operand_size * 8;
        u32 mask = width == 16 ? 0xFFFFU : 0xFFFFFFFFU;
        u8 opcode = c.opcode;
        u32 next = c.next_eip;
        u32 target = 0;
        int condition = -1;
        int operation = -1;
        int alu_width = width;
        int destination = -1;
        u32 source = 0;

        if (c.two_byte)
        {
            if (c.opcode2 < 0x80 || c.opcode2 > 0x8F)
                return false;

            condition = c.opcode2 & 15;
            target = (next + c.immediate) & mask;
        }
        else if (opcode >= 0x70 && opcode <= 0x7F)
        {
            condition = opcode & 15;
            target = (next + (u32)(s32)(s8)c.immediate) & mask;
        }
        else if (opcode == 0xEB || opcode == 0xE9)
        {
            condition = 16;
            target = (next + (opcode == 0xEB ? (u32)(s32)(s8)c.immediate : c.immediate)) & mask;
        }
        else if (opcode == 0x90)
        {
        }
        else if (opcode >= 0xB0 && opcode <= 0xBF)
        {
            // MOV r,imm
            if (opcode < 0xB8)
                SetPollRegister(registers, opcode & 7, 8, c.immediate);
            else
                SetPollRegister(registers, opcode & 7, width, c.immediate);
        }
        else if ((opcode == 0x88 || opcode == 0x89 || opcode == 0x8A || opcode == 0x8B) && (c.modrm >> 6) == 3)
        {
            // MOV between registers
            int move_width = (opcode & 1) == 0 ? 8 : width;
            int to = (opcode & 2) == 0 ? c.rm : c.reg;
            int from = (opcode & 2) == 0 ? c.reg : c.rm;
            SetPollRegister(registers, to, move_width, GetPollRegister(registers, from, move_width));
        }
        else if (opcode == 0xA8 || opcode == 0xA9)
        {
            operation = 8;
            alu_width = opcode == 0xA8 ? 8 : width;
            destination = I386_REG_EAX;
            source = c.immediate;
        }
        else if (opcode < 0x40 && (opcode & 7) >= 4 && (opcode & 7) <= 5)
        {
            // OR, AND, XOR and CMP of the accumulator with an immediate
            operation = opcode >> 3;
            alu_width = (opcode & 1) == 0 ? 8 : width;
            destination = I386_REG_EAX;
            source = c.immediate;
        }
        else if (opcode < 0x40 && (opcode & 7) <= 3 && (c.modrm >> 6) == 3)
        {
            // OR, AND, XOR and CMP between registers
            operation = opcode >> 3;
            alu_width = (opcode & 1) == 0 ? 8 : width;
            destination = (opcode & 2) == 0 ? c.rm : c.reg;
            source = GetPollRegister(registers, (opcode & 2) == 0 ? c.reg : c.rm, alu_width);
        }
        else if ((opcode == 0x84 || opcode == 0x85) && (c.modrm >> 6) == 3)
        {
            operation = 8;
            alu_width = opcode == 0x84 ? 8 : width;
            destination = c.rm;
            source = GetPollRegister(registers, c.reg, alu_width);
        }
        else if ((opcode >= 0x80 && opcode <= 0x83) && (c.modrm >> 6) == 3)
        {
            operation = c.reg;
            alu_width = opcode == 0x80 || opcode == 0x82 ? 8 : width;
            destination = c.rm;
            source = opcode == 0x83 ? (u32)(s32)(s8)c.immediate : c.immediate;
        }
        else if ((opcode == 0xF6 || opcode == 0xF7) && c.reg == 0 && (c.modrm >> 6) == 3)
        {
            operation = 8;
            alu_width = opcode == 0xF6 ? 8 : width;
            destination = c.rm;
            source = c.immediate;
        }
        else
            return false;

        if (operation >= 0)
        {
            // OR, AND, XOR, CMP and TEST, whose condition flags are all defined
            if (operation != 1 && operation != 4 && operation != 6 && operation != 7 && operation != 8)
                return false;

            u32 width_mask = alu_width == 8 ? 0xFFU : alu_width == 16 ? 0xFFFFU : 0xFFFFFFFFU;
            u32 sign = alu_width == 8 ? 0x80U : alu_width == 16 ? 0x8000U : 0x80000000U;
            u32 a = GetPollRegister(registers, destination, alu_width);
            u32 b = source & width_mask;
            u32 result = 0;

            eflags &= ~(I386_FLAG_CF | I386_FLAG_PF | I386_FLAG_ZF | I386_FLAG_SF | I386_FLAG_OF);

            if (operation == 7)
            {
                result = (a - b) & width_mask;

                if (a < b)
                    eflags |= I386_FLAG_CF;

                if (((a ^ b) & (a ^ result) & sign) != 0)
                    eflags |= I386_FLAG_OF;
            }
            else
            {
                result = operation == 1 ? (a | b) : operation == 6 ? (a ^ b) : (a & b);

                if (operation != 8)
                    SetPollRegister(registers, destination, alu_width, result);
            }

            eflags |= k_szp_flags[result & 0xFF] & I386_FLAG_PF;

            if (result == 0)
                eflags |= I386_FLAG_ZF;

            if ((result & sign) != 0)
                eflags |= I386_FLAG_SF;
        }

        if (condition >= 0)
        {
            bool taken = condition == 16 || EvaluateCondition(eflags, condition);
            bool backward = target <= c.start_eip;

            if (taken && backward && !returned && target <= in_eip)
            {
                returned = true;
                next = target;
            }
            else if (taken || backward)
                return false;
        }

        eip = next;
    }

    return false;
}

bool I386::Halted() const
{
    return m_state.halted;
}

bool I386::Shutdown() const
{
    return m_state.shutdown;
}

bool I386::CanAcceptMaskableInterrupt() const
{
    return !m_state.shutdown && (m_state.eflags & I386_FLAG_IF) != 0 && m_state.interrupt_shadow == I386_SHADOW_NONE;
}

bool I386::CanAcceptNMI() const
{
    return !m_state.shutdown && !m_state.nmi_blocked && m_state.interrupt_shadow != I386_SHADOW_MOV_SS;
}

u32 I386::EnterExternalInterrupt(u8 vector, GT_Bus_Access_Context& context, int line)
{
    SetBusContext(context);
    m_io_start_cycles = 0;
    m_state.halted = false;

    // An interrupted REP restarts with RF set, so its instruction breakpoint doesn't hit again
    if (m_state.repeat.active)
        m_state.eflags |= I386_FLAG_RF;

    m_state.repeat.active = false;
    m_state.last_exception_vector = vector;

    u64 clocks = k_i386_real_interrupt_entry_clocks;

    // The debugger sees which IRQ line supplied the vector
    m_external_line = line;
    bool entered = EnterInterrupt(vector, m_state.eip, context, false, false, 0, false, true, &clocks);
    m_external_line = -1;

    if (entered)
    {
        AddBusInterrupt(clocks, false);
        return (u32)clocks;
    }

    if (m_exception.pending)
    {
        I386_Pending_Exception exception = m_exception;
        memset(&m_exception, 0, sizeof(m_exception));

        I386_Run_Result result = {};
        result.cycles = clocks;
        result.instruction_completed = true;
        result.exception_vector = 0xFF;

        DeliverException(exception, m_state.eip, context, result);
        return (u32)result.cycles;
    }
    else
        m_state.shutdown = true;

    return (u32)clocks;
}

u32 I386::EnterNMI(GT_Bus_Access_Context& context)
{
    if (!CanAcceptNMI())
        return 0;

    m_state.nmi_blocked = true;
    return EnterExternalInterrupt(2, context);
}

void I386::CopyState(I386_State& state) const
{
    state = m_state;
}

// Moving CS:EIP away from a stop forgets it, so the old stop address hits normally when reached again
bool I386::SetState(const I386_State& state)
{
    bool moved = state.eip != m_state.eip ||
        state.segments[I386_SEGMENT_CS].selector != m_state.segments[I386_SEGMENT_CS].selector ||
        state.segments[I386_SEGMENT_CS].base != m_state.segments[I386_SEGMENT_CS].base;

    m_state = state;
    UpdateExecutionMode();
    SanitizeState();
    FlushTLB();

    if (moved)
        ClearBreakpointHit();

    return true;
}

void I386::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer, GT_SAVESTATE_VERSION);
}

void I386::LoadState(std::istream& stream, int version)
{
    StateSerializer serializer(stream);
    m_bus_queue = 0;
    m_bus_row = k_i386_bus_no_row;
    m_bus_fraction = 0;
    Serialize(serializer, version);
    m_state.cr0 = (m_state.cr0 & 0x8000001FU) | m_cr0_fixed;
    SanitizeState();
}

void I386::SerializeSegment(StateSerializer& serializer, I386_Segment& segment)
{
    G_SERIALIZE(serializer, segment.selector);
    G_SERIALIZE(serializer, segment.attributes);
    G_SERIALIZE(serializer, segment.base);
    G_SERIALIZE(serializer, segment.limit);
    G_SERIALIZE(serializer, segment.dpl);
}

void I386::SerializeDescriptorTable(StateSerializer& serializer, I386_Descriptor_Table& table)
{
    G_SERIALIZE(serializer, table.base);
    G_SERIALIZE(serializer, table.limit);
}

void I386::Serialize(StateSerializer& serializer, int version)
{
    for (int i = 0; i < I386_REG_COUNT; i++)
        G_SERIALIZE(serializer, m_state.registers[i].value);

    G_SERIALIZE(serializer, m_state.eip);
    G_SERIALIZE(serializer, m_state.eflags);

    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
        SerializeSegment(serializer, m_state.segments[i]);

    SerializeDescriptorTable(serializer, m_state.gdtr);
    SerializeDescriptorTable(serializer, m_state.idtr);
    SerializeSegment(serializer, m_state.ldtr);
    SerializeSegment(serializer, m_state.task_register);
    G_SERIALIZE(serializer, m_state.cr0);
    G_SERIALIZE(serializer, m_state.cr2);
    G_SERIALIZE(serializer, m_state.cr3);
    G_SERIALIZE_ARRAY(serializer, m_state.debug_registers, 8);
    G_SERIALIZE_ARRAY(serializer, m_state.test_registers, 2);
    G_SERIALIZE(serializer, m_state.current_privilege_level);
    G_SERIALIZE(serializer, m_state.interrupt_shadow);
    G_SERIALIZE(serializer, m_state.interrupt_shadow_steps);
    G_SERIALIZE(serializer, m_state.last_exception_vector);
    G_SERIALIZE(serializer, m_state.halted);
    G_SERIALIZE(serializer, m_state.shutdown);
    G_SERIALIZE(serializer, m_state.nmi_blocked);
    G_SERIALIZE(serializer, m_state.repeat.active);
    G_SERIALIZE(serializer, m_state.repeat.start_eip);
    G_SERIALIZE(serializer, m_state.repeat.next_eip);
    G_SERIALIZE(serializer, m_state.repeat.opcode);
    G_SERIALIZE(serializer, m_state.repeat.operand_size);
    G_SERIALIZE(serializer, m_state.repeat.address_size);
    G_SERIALIZE(serializer, m_state.repeat.repeat);
    G_SERIALIZE(serializer, m_state.repeat.segment_override);

    for (int i = 0; i < I386_TLB_SIZE; i++)
    {
        G_SERIALIZE(serializer, m_tlb[i].linear_page);
        G_SERIALIZE(serializer, m_tlb[i].physical_page);
        G_SERIALIZE(serializer, m_tlb[i].flags);
    }

    G_SERIALIZE_ARRAY(serializer, m_tlb_plru, I386_TLB_SETS);

    if (version >= 10)
    {
        G_SERIALIZE(serializer, m_bus_queue);
        G_SERIALIZE(serializer, m_bus_row);
        G_SERIALIZE(serializer, m_bus_fraction);
    }
}

void I386::SanitizeState()
{
    m_bus_queue = MIN(m_bus_queue, k_i386_bus_queue);
    m_bus_fraction &= 0xFFFF;
    m_state.eflags |= I386_FLAG_FIXED;
    m_state.current_privilege_level &= 3;

    if (m_state.interrupt_shadow > I386_SHADOW_MOV_SS)
        m_state.interrupt_shadow = I386_SHADOW_NONE;

    m_state.interrupt_shadow_steps = MIN(m_state.interrupt_shadow_steps, (u8)2);
    m_external_event = false;

    m_debug_data_breakpoints = 0;
    m_step = {};
    m_step_exception = {};
    ResetVBlankWatch();

#if !defined(GT_DISABLE_DYNAREC)
    m_dynarec_epoch++;
#endif

    memset(&m_exception, 0, sizeof(m_exception));
    memset(&m_string, 0, sizeof(m_string));
    memset(&m_instruction, 0, sizeof(m_instruction));

    if (m_state.repeat.active)
    {
        if (m_state.repeat.operand_size != 4)
            m_state.repeat.operand_size = 2;

        if (m_state.repeat.address_size != 4)
            m_state.repeat.address_size = 2;

        if (m_state.repeat.segment_override >= I386_SEGMENT_COUNT)
            m_state.repeat.segment_override = 0xFF;

        m_instruction.start_eip = m_state.repeat.start_eip;
        m_instruction.next_eip = m_state.repeat.next_eip;
        m_instruction.opcode = m_state.repeat.opcode;
        m_instruction.operand_size = m_state.repeat.operand_size;
        m_instruction.address_size = m_state.repeat.address_size;
        m_instruction.repeat = m_state.repeat.repeat;
        m_instruction.segment_override = m_state.repeat.segment_override;
    }

    // A restored REP continuation has no fetched bytes to report until the next decode
    m_instruction_restored = m_state.repeat.active;
    m_checked_valid = false;
    m_fetch_remaining = 0;
    m_address_clocks = 0;
    CloseCodeWindow();

    // Host pages stay NULL until first use
    // The most recent way follows from the tree bits
    for (int i = 0; i < I386_TLB_SIZE; i++)
    {
        m_tlb[i].linear_page &= 0xFFFFF000U;
        m_tlb[i].physical_page &= 0xFFFFF000U;
        m_tlb[i].flags &= k_i386_tlb_valid | k_i386_tlb_user | k_i386_tlb_writable | k_i386_tlb_dirty;
        InitPointer(m_tlb[i].read_page);
        InitPointer(m_tlb[i].write_page);
    }

    m_tlb_used = 0xFFFFFFFFU;

    for (int set = 0; set < I386_TLB_SETS; set++)
    {
        u8 plru = m_tlb_plru[set] & 7;
        m_tlb_plru[set] = plru;
        m_tlb_mru[set] = (plru & 1) != 0 ? ((plru & 2) != 0 ? 0 : 1) : ((plru & 4) != 0 ? 2 : 3);
    }

    u8 privilege = m_state.current_privilege_level;

    InitPointer(m_bus_context);
    UpdateMemoryMode();
    UpdateExecutionMode();

    // CPL differs from the CS RPL between setting PE and the far jump
    if (m_state.execution_mode == I386_MODE_PROTECTED)
    {
        m_state.current_privilege_level = privilege;
        UpdateUserMode();
    }

    if (m_state.repeat.active)
        PrepareString();

    UpdateDebugState();
}

u8 I386::GetLastExceptionVector() const
{
    return m_state.last_exception_vector;
}

// Only exceptions the CPU raised, unlike the last vector in the state, which interrupts also update
const I386_Exception_Info& I386::GetLastException() const
{
    return m_last_exception;
}

// Passive copy of the bytes at CS:EIP before a single step executes them
void I386::CaptureCheckedBytes()
{
    u32 linear = m_state.segments[I386_SEGMENT_CS].base + m_state.eip;

    m_checked_eip = m_state.eip;
    m_checked_count = 0;

    while (m_checked_count < GT_I386_MAX_INSTRUCTION_LENGTH &&
        TryPeekLinear(linear + m_checked_count, m_checked_bytes[m_checked_count]))
        m_checked_count++;
}

bool I386::CopyDecodeState(I386_Decode_State& state)
{
    u8 bytes[GT_I386_MAX_INSTRUCTION_LENGTH];
    u32 length = m_instruction_restored ? 0 : m_instruction.next_eip - m_instruction.start_eip;
    length = MIN(length, (u32)GT_I386_MAX_INSTRUCTION_LENGTH);

    // A single step captured its bytes before executing, otherwise they are read back through the current CS
    if (m_checked_valid && m_instruction.start_eip == m_checked_eip)
    {
        length = MIN(length, (u32)m_checked_count);
        memcpy(bytes, m_checked_bytes, length);
    }
    else
    {
        u32 linear = m_state.segments[I386_SEGMENT_CS].base + m_instruction.start_eip;

        for (u32 i = 0; i < length; i++)
        {
            if (!TryPeekLinear(linear + i, bytes[i]))
            {
                length = i;
                break;
            }
        }
    }

    // The default size is recovered from the decoded size and the prefixes actually present
    bool operand_override = false;

    for (u32 i = 0; i < length && (bytes[i] == 0x26 || bytes[i] == 0x2E || bytes[i] == 0x36 || bytes[i] == 0x3E ||
        bytes[i] == 0x64 || bytes[i] == 0x65 || bytes[i] == 0x66 || bytes[i] == 0x67 || bytes[i] == 0xF0 ||
        bytes[i] == 0xF2 || bytes[i] == 0xF3); i++)
        operand_override = operand_override || bytes[i] == 0x66;

    bool default32 = (m_instruction.operand_size == 4) != operand_override;
    InstructionContext decoded;

    m_passive_bytes = bytes;
    m_passive_count = length;
    m_passive_index = 0;

    if (length == 0 || !DecodeInstructionPassive(decoded, m_instruction.start_eip, default32))
    {
        // Partially decoded instruction: report the prefix and opcode state of the execution context
        decoded = m_instruction;
        decoded.memory_operand = false;
        decoded.immediate = 0;
        decoded.immediate2 = 0;
    }
    else if (decoded.memory_operand)
    {
        decoded.effective_offset = m_instruction.effective_offset;
        decoded.segment = m_instruction.segment;
    }

    decoded.start_eip = m_instruction.start_eip;
    decoded.next_eip = m_instruction.next_eip;

    FillDecodeState(decoded, bytes, length, state);
    return state.length != 0;
}

void I386::SetTraceEnabled(bool enabled)
{
    if (enabled && !IsValidPointer(m_trace))
        m_trace = new I386_Trace_Entry[GT_I386_TRACE_SIZE]();

    m_trace_internal = enabled;
    m_trace_enabled = m_trace_internal || m_trace_cpu;
    m_trace_count = 0;
}

void I386::SetResetEDX(u32 edx)
{
    m_reset_edx = edx;
}

void I386::SetCR0FixedBits(u32 bits)
{
    m_cr0_fixed = bits;
}

void I386::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

void I386::SetProfiler(Profiler* profiler)
{
    m_profiler = profiler;
}

int I386::CopyTraceEntries(I386_Trace_Entry* entries, int capacity) const
{
    if (!IsValidPointer(entries) || capacity <= 0)
        return 0;

    int count = MIN(capacity, m_trace_count);
    int first = m_trace_count - count;

    for (int i = 0; i < count; i++)
        entries[i] = m_trace[first + i];

    return count;
}

void I386::SetRealModeSegment(I386_Segment_Register segment, u16 selector)
{
    I386_Segment& state = m_state.segments[segment];
    state.selector = selector;
    state.base = (u32)selector << 4;
    state.limit = 0xFFFF;
    state.dpl = 0;
    state.attributes = I386_SEGMENT_PRESENT | I386_SEGMENT_READABLE;

    if (segment == I386_SEGMENT_CS)
        state.attributes |= I386_SEGMENT_EXECUTABLE;
    else
        state.attributes |= I386_SEGMENT_WRITABLE;

    UpdateSegmentFastPaths();
}

void I386::SetVM86Segment(I386_Segment_Register segment, u16 selector)
{
    SetRealModeSegment(segment, selector);
    m_state.segments[segment].attributes &= ~(I386_SEGMENT_TYPE_MASK | I386_SEGMENT_ACCESSED);
    m_state.segments[segment].attributes |= I386_SEGMENT_SYSTEM | (2U << I386_SEGMENT_TYPE_SHIFT);
    m_state.segments[segment].dpl = 3;
    UpdateSegmentFastPaths();
}

// A real mode CS load only sets the selector and base, so the cached limit, access rights and D/B stay
// A direct far JMP also rewrites the access rights to present, DPL 0, system type 2, keeping G and D/B
// Accesses read type 2 as writable data, so code that sets PE without a far jump can still write through CS
void I386::LoadRealCodeSegment(u16 selector, bool direct_jump)
{
    I386_Segment& state = m_state.segments[I386_SEGMENT_CS];
    state.selector = selector;
    state.base = (u32)selector << 4;

    if (direct_jump)
    {
        state.attributes = (state.attributes & (I386_SEGMENT_GRANULAR | I386_SEGMENT_DEFAULT_32)) |
            I386_SEGMENT_PRESENT | I386_SEGMENT_SYSTEM | I386_SEGMENT_READABLE | I386_SEGMENT_WRITABLE |
            (2U << I386_SEGMENT_TYPE_SHIFT);
        state.dpl = 0;
    }

    UpdateSegmentFastPaths();
}

void I386::UpdateExecutionMode()
{
    if ((m_state.eflags & I386_FLAG_VM) != 0)
    {
        m_state.execution_mode = I386_MODE_VM86;
        m_state.current_privilege_level = 3;
    }
    else if ((m_state.cr0 & 1) != 0)
    {
        m_state.execution_mode = I386_MODE_PROTECTED;
        m_state.current_privilege_level = m_state.segments[I386_SEGMENT_CS].selector & 3;
    }
    else
    {
        m_state.execution_mode = I386_MODE_REAL;
        m_state.current_privilege_level = 0;
    }

    UpdateUserMode();
    UpdateSegmentFastPaths();
}

bool I386::CheckInstructionBreakpoint()
{
    u32 linear = m_state.segments[I386_SEGMENT_CS].base + m_instruction.start_eip;
    u8 matches = 0;

    for (int i = 0; i < 4; i++)
    {
        u32 enable = (m_state.debug_registers[7] >> (i * 2)) & 3;
        u32 operation = (m_state.debug_registers[7] >> (16 + i * 4)) & 3;

        if (enable != 0 && operation == 0 && m_state.debug_registers[i] == linear)
            matches |= 1U << i;
    }

    if (matches == 0)
        return true;

    m_state.debug_registers[6] |= matches;
    return RaiseException(1, I386_EXCEPTION_FAULT);
}

bool I386::LoadRealSegment(int segment, u16 selector)
{
    if (segment < 0 || segment >= I386_SEGMENT_COUNT)
        return RaiseException(6, I386_EXCEPTION_FAULT);

    if (m_state.execution_mode != I386_MODE_REAL)
    {
        SetRealModeSegment((I386_Segment_Register)segment, selector);
        return true;
    }

    // A real mode data segment load only sets the selector and base, so the cached limit and type stay
    // Unreal mode relies on it, and so does code that leaves protected mode with an expand-down stack
    I386_Segment& state = m_state.segments[segment];
    state.selector = selector;
    state.base = (u32)selector << 4;
    state.attributes |= I386_SEGMENT_PRESENT;
    UpdateSegmentFastPaths(state);
    return true;
}

bool I386::FarTransfer(u16 selector, u32 offset, int width, bool direct_jump)
{
    u32 target = width == 16 ? (u16)offset : offset;

    if (target > GetFarTransferLimit())
        return RaiseException(13, I386_EXCEPTION_FAULT, true, 0);

    if (m_state.execution_mode == I386_MODE_REAL)
        LoadRealCodeSegment(selector, direct_jump);
    else
        SetRealModeSegment(I386_SEGMENT_CS, selector);

    m_state.eip = target;
    m_state.repeat.active = false;
    return true;
}

bool I386::RaiseException(u8 vector, u8 exception_class, bool has_error_code, u32 error_code)
{
    if (m_exception.pending)
        return false;

    m_exception.pending = true;
    m_exception.has_error_code = has_error_code;
    m_exception.has_return_eip = false;
    m_exception.has_double_fault_eip = false;
    m_exception.vector = vector;
    m_exception.exception_class = exception_class;
    m_exception.error_code = (m_external_event && has_error_code && vector >= 10 && vector <= 13) ?
        error_code | 1 : error_code;
    m_exception.return_eip = 0;
    m_state.last_exception_vector = vector;
    return false;
}

bool I386::ResolveException(GT_Bus_Access_Context& context, I386_Run_Result& result)
{
    if (!m_exception.pending)
        return true;

    I386_Pending_Exception exception = m_exception;
    memset(&m_exception, 0, sizeof(m_exception));

    u32 return_eip = exception.has_return_eip ? exception.return_eip :
        exception.exception_class == I386_EXCEPTION_FAULT ? m_instruction.start_eip : m_instruction.next_eip;

    m_state.repeat.active = false;
    return DeliverException(exception, return_eip, context, result);
}

bool I386::CausesDoubleFault(u8 first, u8 second) const
{
    bool first_contributory = first == 0 || first == 9 || (first >= 10 && first <= 13);
    bool second_contributory = second == 0 || second == 9 || (second >= 10 && second <= 13);

    if (first == 14)
        return second_contributory;

    // Unlike Intel's table, a #PF while delivering #TS is a double fault on the 386, other contributory faults are not
    return first_contributory && (second_contributory || (first == 10 && second == 14));
}

bool I386::DeliverException(const I386_Pending_Exception& exception, u32 return_eip,
    GT_Bus_Access_Context& context, I386_Run_Result& result)
{
    I386_Pending_Exception current = exception;
    u32 current_return_eip = return_eip;

    while (true)
    {
        // Every debug exception goes through the same microcode, which clears DR7.GD
        if (current.vector == 1)
            m_state.debug_registers[7] &= ~0x00002000U;

        u64 entry_clocks = k_i386_real_interrupt_entry_clocks;
        u16 return_cs = m_state.segments[I386_SEGMENT_CS].selector;
        bool ok = EnterInterrupt(current.vector, current_return_eip, context, false, current.has_error_code,
            current.error_code, current.exception_class == I386_EXCEPTION_FAULT, false, &entry_clocks, &result);

        result.cycles += entry_clocks;

        if (ok)
        {
            result.exception = true;
            result.exception_vector = current.vector;
            result.end_batch = true;
            m_state.last_exception_vector = current.vector;
            m_last_exception.valid = true;
            m_last_exception.vector = current.vector;
            m_last_exception.has_error_code = current.has_error_code;
            m_last_exception.error_code = current.error_code;
            m_last_exception.cs = return_cs;
            m_last_exception.eip = current_return_eip;
            return true;
        }

        if (!m_exception.pending)
        {
            m_state.shutdown = true;
            return false;
        }

        I386_Pending_Exception next = m_exception;
        memset(&m_exception, 0, sizeof(m_exception));

        // A #PF while delivering #PF shuts the 386 down instead of raising #DF
        if (current.vector == 8 || (current.vector == 14 && next.vector == 14))
        {
            m_state.shutdown = true;
            return false;
        }

        if (CausesDoubleFault(current.vector, next.vector))
        {
            if (current.has_double_fault_eip)
                current_return_eip = current.double_fault_eip;

            memset(&current, 0, sizeof(current));
            current.pending = true;
            current.has_error_code = true;
            current.vector = 8;
            current.exception_class = I386_EXCEPTION_ABORT;
            current.error_code = 0;
        }
        else
        {
            current = next;

            if (next.has_return_eip)
                current_return_eip = next.return_eip;
        }
    }
}

#if !defined(GT_DISABLE_DISASSEMBLER)
static INLINE I386_Call_Type get_call_type(bool software, bool external)
{
    if (software)
        return I386_CALL_SOFTWARE_INTERRUPT;

    return external ? I386_CALL_HARDWARE_INTERRUPT : I386_CALL_EXCEPTION;
}
#endif

bool I386::EnterInterrupt(u8 vector, u32 return_eip, GT_Bus_Access_Context& context, bool software,
    bool has_error_code, u32 error_code, bool fault, bool external, u64* clocks, I386_Run_Result* run_result)
{
    u16 return_cs = m_state.segments[I386_SEGMENT_CS].selector;
    u32 return_base = m_state.segments[I386_SEGMENT_CS].base;
    u32 source_eip = m_state.eip;

    if (m_state.execution_mode != I386_MODE_REAL)
    {
        bool previous_external = m_external_event;
        u32 saved_return_eip = return_eip;
        m_external_event = external;

        bool result = EnterProtectedInterrupt(vector, return_eip, context, software, has_error_code, error_code,
            fault, clocks, run_result, saved_return_eip);

        m_external_event = previous_external;

        if (result && IsValidPointer(run_result))
        {
            run_result->exception_return_cs = return_cs;
            run_result->exception_return_base = return_base;
            run_result->exception_source_eip = source_eip;
        }

#if !defined(GT_DISABLE_DISASSEMBLER)
        if (result)
            PushCallStack(return_cs, return_base, source_eip, saved_return_eip, get_call_type(software, external), vector);
#endif

        if (result && unlikely(m_debugger_interrupt_checks))
            RecordDebuggerInterrupt(vector, software, external, return_base + source_eip, has_error_code, error_code);

        return result;
    }

    UNUSED(fault);

    u32 table_offset = (u32)vector * 4;

    if ((u64)table_offset + 3 > m_state.idtr.limit)
        return RaiseException(8, I386_EXCEPTION_ABORT);

    u32 address = m_state.idtr.base + table_offset;
    u32 physical = 0;

    if (!TranslateLinear(address, false, context, physical))
        return false;

    u16 new_ip = m_memory->Read16Physical(physical, context);
    u16 new_cs = m_memory->Read16Physical(physical + 2, context);

    if (!StackPushSized(m_state.eflags, 16, context))
        return false;

    if (!StackPushSized(m_state.segments[I386_SEGMENT_CS].selector, 16, context))
        return false;

    if (!StackPushSized(return_eip, 16, context))
        return false;

    m_state.eflags &= ~(I386_FLAG_IF | I386_FLAG_TF);
    LoadRealCodeSegment(new_cs, false);
    m_state.eip = new_ip;
    m_state.halted = false;

    if (IsValidPointer(run_result))
    {
        run_result->exception_return_cs = return_cs;
        run_result->exception_return_eip = (u16)return_eip;
        run_result->exception_return_base = return_base;
        run_result->exception_source_eip = source_eip;
    }

#if !defined(GT_DISABLE_DISASSEMBLER)
    PushCallStack(return_cs, return_base, source_eip, (u16)return_eip, get_call_type(software, external), vector);
#endif

    if (unlikely(m_debugger_interrupt_checks))
        RecordDebuggerInterrupt(vector, software, external, return_base + source_eip, has_error_code, error_code);

    return true;
}

// The trace logger gets each instruction once, the internal capture keeps full states before and after
void I386::TraceStep(I386_State& before)
{
    if (m_trace_cpu && !m_state.repeat.active)
        TraceInstruction();

    if (m_trace_internal)
        CopyState(before);
    else
        before.eflags = m_state.eflags;
}

void I386::TraceInstruction()
{
    const I386_Segment& code = m_state.segments[I386_SEGMENT_CS];
    u32 linear = code.base + m_state.eip;
    GT_Trace_Entry entry = {};

    entry.type = TRACE_CPU;
    entry.cpu.linear = linear;
    entry.cpu.eip = m_state.eip;
    entry.cpu.eflags = m_state.eflags;
    entry.cpu.cs = code.selector;
    entry.cpu.ds = m_state.segments[I386_SEGMENT_DS].selector;
    entry.cpu.es = m_state.segments[I386_SEGMENT_ES].selector;
    entry.cpu.ss = m_state.segments[I386_SEGMENT_SS].selector;
    entry.cpu.fs = m_state.segments[I386_SEGMENT_FS].selector;
    entry.cpu.gs = m_state.segments[I386_SEGMENT_GS].selector;
    entry.cpu.mode = (u8)m_state.execution_mode;

    for (int i = 0; i < I386_REG_COUNT; i++)
        entry.cpu.registers[i] = m_state.registers[i].value;

#if !defined(GT_DISABLE_DISASSEMBLER)
    FormatDisassemblerRecordText(linear);

    const I386_Disassembler_Record* record = GetDisassemblerRecord(linear);

    if (IsValidPointer(record) && record->size > 0)
    {
        entry.cpu.size = (u8)MIN(record->size, GT_I386_MAX_INSTRUCTION_LENGTH);
        memcpy(entry.cpu.opcodes, record->opcodes, entry.cpu.size);
        strncpy_fit(entry.cpu.name, record->name, sizeof(entry.cpu.name));
    }
#endif

    m_trace_logger->TraceLog(entry);
}

void I386::RecordTrace(const I386_State& before)
{
    if (!m_trace_internal)
        return;

    if (m_trace_count == GT_I386_TRACE_SIZE)
    {
        memmove(&m_trace[0], &m_trace[1], sizeof(I386_Trace_Entry) * (GT_I386_TRACE_SIZE - 1));
        m_trace_count--;
    }

    I386_Trace_Entry& entry = m_trace[m_trace_count++];

    CopyDecodeState(entry.instruction);
    entry.before = before;
    CopyState(entry.after);
}

bool I386::CopyDebugState(I386_Debug_State& state) const
{
    UpdateDebugState();
    state = m_debug_state;
    return true;
}

bool I386::GetDebugRegisterValue(const char* name, u32& value) const
{
    UpdateDebugState();

    if (EqualName(name, "EAX"))
        value = m_debug_state.eax;
    else if (EqualName(name, "EBX"))
        value = m_debug_state.ebx;
    else if (EqualName(name, "ECX"))
        value = m_debug_state.ecx;
    else if (EqualName(name, "EDX"))
        value = m_debug_state.edx;
    else if (EqualName(name, "ESI"))
        value = m_debug_state.esi;
    else if (EqualName(name, "EDI"))
        value = m_debug_state.edi;
    else if (EqualName(name, "EBP"))
        value = m_debug_state.ebp;
    else if (EqualName(name, "ESP"))
        value = m_debug_state.esp;
    else if (EqualName(name, "EIP"))
        value = m_debug_state.eip;
    else if (EqualName(name, "EFLAGS"))
        value = m_debug_state.eflags;
    else if (EqualName(name, "CR0"))
        value = m_debug_state.cr0;
    else if (EqualName(name, "CR2"))
        value = m_debug_state.cr2;
    else if (EqualName(name, "CR3"))
        value = m_debug_state.cr3;
    else
        return GetDebugPartialRegisterValue(name, value);

    return true;
}

bool I386::GetDebugPartialRegisterValue(const char* name, u32& value) const
{
    static const char* k_names16[I386_REG_COUNT] = { "AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI" };
    static const char* k_names8[8] = { "AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH" };
    static const char* k_segments[I386_SEGMENT_COUNT] = { "ES", "CS", "SS", "DS", "FS", "GS" };

    for (int i = 0; i < I386_REG_COUNT; i++)
    {
        if (EqualName(name, k_names16[i]))
        {
            value = m_state.registers[i].value & 0xFFFF;
            return true;
        }

        if (EqualName(name, k_names8[i]))
        {
            value = (m_state.registers[i & 3].value >> (i < 4 ? 0 : 8)) & 0xFF;
            return true;
        }
    }

    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
    {
        if (EqualName(name, k_segments[i]))
        {
            value = m_state.segments[i].selector;
            return true;
        }
    }

    if (EqualName(name, "IP"))
        value = m_state.eip & 0xFFFF;
    else if (EqualName(name, "FLAGS"))
        value = m_state.eflags & 0xFFFF;
    else
        return false;

    return true;
}

bool I386::EqualName(const char* left, const char* right)
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

void I386::UpdateDebugState() const
{
    memset(&m_debug_state, 0, sizeof(m_debug_state));

    m_debug_state.eax = m_state.registers[I386_REG_EAX].value;
    m_debug_state.ebx = m_state.registers[I386_REG_EBX].value;
    m_debug_state.ecx = m_state.registers[I386_REG_ECX].value;
    m_debug_state.edx = m_state.registers[I386_REG_EDX].value;
    m_debug_state.esi = m_state.registers[I386_REG_ESI].value;
    m_debug_state.edi = m_state.registers[I386_REG_EDI].value;
    m_debug_state.ebp = m_state.registers[I386_REG_EBP].value;
    m_debug_state.esp = m_state.registers[I386_REG_ESP].value;
    m_debug_state.eip = m_state.eip;
    m_debug_state.eflags = m_state.eflags;
    m_debug_state.cr0 = m_state.cr0;
    m_debug_state.cr2 = m_state.cr2;
    m_debug_state.cr3 = m_state.cr3;

    for (int i = 0; i < I386_SEGMENT_COUNT; i++)
    {
        m_debug_state.segment[i].selector = m_state.segments[i].selector;
        m_debug_state.segment[i].base = m_state.segments[i].base;
        m_debug_state.segment[i].limit = m_state.segments[i].limit;
        m_debug_state.segment[i].access = m_state.segments[i].attributes;
        m_debug_state.segment[i].dpl = m_state.segments[i].dpl;
        m_debug_state.segment[i].present = (m_state.segments[i].attributes & I386_SEGMENT_PRESENT) != 0;
    }
}

void I386::FillDecodeState(const InstructionContext& instruction, const u8* bytes, u32 length,
    I386_Decode_State& state) const
{
    memset(&state, 0, sizeof(state));

    state.start_eip = instruction.start_eip;
    state.next_eip = instruction.next_eip;
    state.length = (u8)length;
    memcpy(state.bytes, bytes, length);
    state.opcode = instruction.opcode;
    state.opcode2 = instruction.two_byte ? instruction.opcode2 : 0;
    state.operand_size = instruction.operand_size;
    state.address_size = instruction.address_size;
    state.repeat = instruction.repeat;
    state.two_byte = instruction.two_byte;
    state.lock = instruction.lock;
    state.segment_override = instruction.segment_override;

    // ModR/M is only reported when its byte was fetched
    u32 position = 0;

    while (position < length && (bytes[position] == 0x26 || bytes[position] == 0x2E || bytes[position] == 0x36 ||
        bytes[position] == 0x3E || bytes[position] == 0x64 || bytes[position] == 0x65 || bytes[position] == 0x66 ||
        bytes[position] == 0x67 || bytes[position] == 0xF0 || bytes[position] == 0xF2 || bytes[position] == 0xF3))
        position++;

    position += instruction.two_byte ? 2 : 1;

    state.has_modrm = length > position &&
        OPCodeHasModRM(instruction.two_byte, instruction.two_byte ? instruction.opcode2 : instruction.opcode);

    if (state.has_modrm)
    {
        state.modrm = instruction.modrm;
        state.mod = instruction.modrm >> 6;
        state.reg = instruction.reg;
        state.rm = instruction.rm;
        state.memory_operand = instruction.memory_operand;
        state.segment = instruction.memory_operand ? instruction.segment : (u8)I386_SEGMENT_DS;

        if (instruction.memory_operand)
        {
            state.displacement = instruction.displacement;
            state.effective_offset = instruction.effective_offset;
            state.has_sib = instruction.address_size == 4 && instruction.rm == 4;

            if (state.has_sib)
            {
                state.sib = instruction.sib;
                state.sib_scale = instruction.sib >> 6;
                state.sib_index = (instruction.sib >> 3) & 7;
                state.sib_base = instruction.sib & 7;
            }
        }
    }

    if (GetImmediateSize(state.two_byte, state.two_byte ? state.opcode2 : state.opcode, state.reg,
        state.operand_size, state.address_size) != 0)
        state.immediate = instruction.immediate;

    if (!state.two_byte && (state.opcode == 0xC8 || state.opcode == 0x9A || state.opcode == 0xEA))
        state.immediate2 = instruction.immediate2;
}
