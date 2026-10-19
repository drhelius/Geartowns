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

#include <fstream>
#include <sstream>
#include <time.h>
#include "geartowns_core.h"
#include "audio/audio.h"
#include "cdrom/cdrom.h"
#include "cdrom/cdrom_audio.h"
#include "cdrom/cdrom_media.h"
#include "drive/fdc.h"
#include "media/firmware.h"
#include "input/input.h"
#include "input/keyboard.h"
#include "common/memory_stream.h"
#include "common/profiler.h"
#include "common/trace_logger.h"
#include "common/state_serializer.h"
#include "media/media.h"
#include "media/game_db.h"
#include "system/memory.h"
#include "i386/i386.h"
#include "system/io.h"
#include "system/pic.h"
#include "system/pit.h"
#include "system/msm58321.h"
#include "system/system_control.h"
#include "system/scheduler.h"
#include "system/machine_profiles.h"
#include "system/upd71071.h"
#include "video/video.h"

GeartownsCore::GeartownsCore()
{
    InitPointer(m_audio);
    InitPointer(m_firmware);
    InitPointer(m_input);
    InitPointer(m_media);
    InitPointer(m_memory);
    InitPointer(m_i386);
    InitPointer(m_io);
    InitPointer(m_pic);
    InitPointer(m_pit);
    InitPointer(m_system_control);
    InitPointer(m_scheduler);
    InitPointer(m_video);
    InitPointer(m_cdrom_media);
    InitPointer(m_cdrom_audio);
    InitPointer(m_cdrom);
    InitPointer(m_fdc);
    InitPointer(m_keyboard);
    InitPointer(m_trace_logger);
    InitPointer(m_profiler);
    InitPointer(m_rtc);
    InitPointer(m_dma);
    InitPointer(m_frame_buffer);

    m_machine_settings.model = GT_MACHINE_MODEL1_2;
    m_machine_settings.cpu_mhz = GT_MACHINE_AUTO;
    m_machine_settings.ram_mb = GT_MACHINE_AUTO;
    InitPointer(m_auto_entry);
    m_auto_three_mode = false;
    memset(m_firmware_sources, 0, sizeof(m_firmware_sources));

    for (int i = 0; i < GT_MACHINE_COUNT; i++)
        m_firmware_sources[i].set = -1;

    ResolveMachineConfig();
    m_machine_config = m_pending_machine_config;
    m_machine_report = m_pending_machine_report;
    m_powered = false;
    m_paused = false;
    m_debug_step = GT_DEBUG_STEP_NONE;
    m_debug_step_resume = false;
    m_debug_interrupt_deferred = false;
}

GeartownsCore::~GeartownsCore()
{
    SafeDelete(m_audio);
    SafeDelete(m_input);
    SafeDelete(m_media);
    SafeDelete(m_i386);
    SafeDelete(m_io);
    SafeDelete(m_pic);
    SafeDelete(m_pit);
    SafeDelete(m_system_control);
    SafeDelete(m_scheduler);
    SafeDelete(m_video);
    SafeDelete(m_cdrom);
    SafeDelete(m_cdrom_audio);
    SafeDelete(m_fdc);
    SafeDelete(m_keyboard);
    SafeDelete(m_trace_logger);
    SafeDelete(m_profiler);
    SafeDelete(m_rtc);
    SafeDelete(m_dma);
    SafeDelete(m_memory);
    SafeDelete(m_firmware);
    SafeDelete(m_cdrom_media);
}

void GeartownsCore::Init()
{
    if (!IsValidPointer(m_cdrom_media))
        m_cdrom_media = new CdRomMedia();

    if (!IsValidPointer(m_cdrom_audio))
        m_cdrom_audio = new CdRomAudio(m_cdrom_media);

    if (!IsValidPointer(m_audio))
        m_audio = new Audio();

    if (!IsValidPointer(m_firmware))
        m_firmware = new Firmware();

    if (!IsValidPointer(m_input))
        m_input = new Input();

    if (!IsValidPointer(m_media))
        m_media = new Media(m_cdrom_media);

    if (!IsValidPointer(m_memory))
        m_memory = new Memory();

    if (!IsValidPointer(m_i386))
        m_i386 = new I386();

    if (!IsValidPointer(m_io))
        m_io = new IO();

    if (!IsValidPointer(m_pic))
        m_pic = new PIC();

    if (!IsValidPointer(m_pit))
        m_pit = new PIT();

    if (!IsValidPointer(m_system_control))
        m_system_control = new SystemControl();

    if (!IsValidPointer(m_scheduler))
        m_scheduler = new Scheduler();

    if (!IsValidPointer(m_video))
        m_video = new Video();

    if (!IsValidPointer(m_cdrom))
        m_cdrom = new CdRom(m_cdrom_media, m_cdrom_audio);

    if (!IsValidPointer(m_fdc))
        m_fdc = new FDC();

    if (!IsValidPointer(m_keyboard))
        m_keyboard = new Keyboard();

    if (!IsValidPointer(m_trace_logger))
        m_trace_logger = new TraceLogger();

    if (!IsValidPointer(m_profiler))
        m_profiler = new Profiler();

    if (!IsValidPointer(m_rtc))
        m_rtc = new MSM58321();

    if (!IsValidPointer(m_dma))
        m_dma = new UPD71071();

    m_firmware->Init();
    m_scheduler->Init();
    m_memory->Init();
    m_cdrom_media->Init();
    m_cdrom_audio->Init();
    m_pic->Init();
    m_audio->Init(m_scheduler, m_cdrom_audio, m_pic, m_pit);
    m_pit->Init(m_pic, m_scheduler, m_audio);
    m_system_control->Init(m_memory);
    m_system_control->SetBusModeCallback(this, BusModeCallback);
    m_video->Init(m_pic, m_pit, m_scheduler, m_memory, m_firmware->GetFontRom());
    m_dma->Init(m_memory, m_scheduler);
    m_cdrom->Init(m_pic, m_scheduler, m_dma, m_audio);
    m_fdc->Init(m_pic, m_scheduler, m_dma);
    m_keyboard->Init(m_pic, m_scheduler);
    m_rtc->Init();
    m_io->Init(m_scheduler, m_audio, m_pic, m_pit, m_video, m_memory, m_system_control, m_cdrom, m_fdc, m_keyboard,
        m_input, m_rtc, m_dma);
    m_i386->Init(m_memory, m_io);
    m_input->Init();
    m_trace_logger->Init(&m_scheduler->GetState()->clocks);
    m_profiler->Init(&m_scheduler->GetState()->clocks);
    m_i386->SetTraceLogger(m_trace_logger);
    m_i386->SetProfiler(m_profiler);
    m_memory->SetTraceLogger(m_trace_logger);
    m_system_control->SetTraceLogger(m_trace_logger);
    m_pic->SetTraceLogger(m_trace_logger);
    m_pit->SetTraceLogger(m_trace_logger);
    m_dma->SetTraceLogger(m_trace_logger);
    m_rtc->SetTraceLogger(m_trace_logger);
    m_video->SetTraceLogger(m_trace_logger);
    m_audio->SetTraceLogger(m_trace_logger);
    m_cdrom->SetTraceLogger(m_trace_logger);
    m_fdc->SetTraceLogger(m_trace_logger);
    m_keyboard->SetTraceLogger(m_trace_logger);
    m_input->SetTraceLogger(m_trace_logger);
    m_media->Init();
    Reset();
}

GT_Run_Result GeartownsCore::RunToFrame(u8* frame_buffer, s16* sample_buffer, int* sample_count, bool render)
{
    return RunToFrameTemplate<false>(frame_buffer, sample_buffer, sample_count, NULL, render);
}

GT_Run_Result GeartownsCore::RunToFrame(u8* frame_buffer, s16* sample_buffer, int* sample_count, GT_Debug_Run* debug,
    bool render)
{
#if defined(GT_DISABLE_DISASSEMBLER)
    return RunToFrameTemplate<false>(frame_buffer, sample_buffer, sample_count, debug, render);
#else
    return RunToFrameTemplate<true>(frame_buffer, sample_buffer, sample_count, debug, render);
#endif
}

template<bool debugger>
GT_Run_Result GeartownsCore::RunToFrameTemplate(u8* frame_buffer, s16* sample_buffer, int* sample_count,
    GT_Debug_Run* debug, bool render)
{
    m_frame_buffer = frame_buffer;

    if (sample_count != NULL)
        *sample_count = 0;

    if (m_paused)
        return GT_RUN_PAUSED;

    if (!m_powered || !IsValidPointer(m_firmware) || !m_firmware->IsReady())
        return GT_RUN_NOT_READY;

    u64 frame_start = m_scheduler->GetClocks();
    m_video->BeginFrame(frame_buffer, render);

    bool debugging = debugger && IsValidPointer(debug);
    m_trace_logger->SetActive(debugging);
    m_profiler->SetActive(debugging);
    m_i386->EnableDebuggerChecks(debugging);
    m_dma->SetDebugCallback(debugging ? &GeartownsCore::DMADebugCallback : NULL, m_i386);

    if (debugging)
    {
        RunDebuggerFrame(frame_start, debug);

        if (m_video->IsFrameReady())
        {
            m_profiler->CountFrame();
            m_i386->UpdateVBlankWatch();
        }
    }
    else
        RunFrame(frame_start);

    EndFrame(frame_start, sample_buffer, sample_count);

    return GT_RUN_FRAME_READY;
}

void GeartownsCore::RunFrame(u64 frame_start)
{
    if (unlikely(m_debug_interrupt_deferred))
    {
        m_debug_interrupt_deferred = false;
        GT_Bus_Access_Context context = BeginSlice();
        EnterPendingInterrupt(context);
    }

    while (!IsFrameDone(frame_start))
    {
        u32 slice = m_scheduler->GetSliceClocks(GetFrameLimit(frame_start));
        u32 cycles = m_scheduler->GetSliceCycles(slice);
        GT_Bus_Access_Context context = BeginSlice();

        I386_Run_Result result = m_i386->RunFor(cycles, context, false, m_pic->IsInterruptPending());
        CompleteSlice(result, context, slice, false);
    }
}

void GeartownsCore::RunDebuggerFrame(u64 frame_start, GT_Debug_Run* debug)
{
    if (BeginDebuggerFrame(debug))
        return;

    if (unlikely(m_debug_step != GT_DEBUG_STEP_NONE))
    {
        RunDebuggerSteps(frame_start, debug);
        return;
    }

    bool regular = debug->stop_on_breakpoint;
    bool run_to = debug->stop_on_run_to_breakpoint;

    while (!IsFrameDone(frame_start))
    {
        if (m_i386->CheckDebuggerBreakpoints(regular, run_to))
        {
            StopDebugger(debug, true);
            break;
        }

        u32 slice = m_scheduler->GetSliceClocks(GetFrameLimit(frame_start));
        GT_Bus_Access_Context context = BeginSlice();
        m_i386->RunInstruction(context);
        CompleteSlice(m_i386->GetStepInfo(), context, slice, regular);

        if (unlikely(m_i386->IsDebuggerHitPending()) && AcceptDebuggerHit(debug))
            break;
    }
}

NO_INLINE void GeartownsCore::RunDebuggerSteps(u64 frame_start, GT_Debug_Run* debug)
{
    bool regular = debug->stop_on_breakpoint;
    bool run_to = debug->stop_on_run_to_breakpoint;

    while (!IsFrameDone(frame_start))
    {
        if (m_i386->CheckDebuggerBreakpoints(regular, run_to) && BreakpointStopsDebugger(debug))
            break;

        if (m_debug_step == GT_DEBUG_STEP_INSTRUCTION)
        {
            if (RunDebuggerStep(frame_start, debug))
                break;

            continue;
        }

        u32 slice = m_scheduler->GetSliceClocks(GetFrameLimit(frame_start));
        GT_Bus_Access_Context context = BeginSlice();
        m_i386->RunInstruction(context);
        CompleteSlice(m_i386->GetStepInfo(), context, slice, regular);

        if (unlikely(m_i386->IsDebuggerHitPending()) && AcceptDebuggerHit(debug))
            break;
    }
}

NO_INLINE bool GeartownsCore::BeginDebuggerFrame(GT_Debug_Run* debug)
{
    debug->stopped = false;
    debug->breakpoint_hit = false;

    if (!debug->step_debugger && !debug->step_over)
        m_debug_step = GT_DEBUG_STEP_NONE;
    else if (debug->step_start || (m_debug_step == GT_DEBUG_STEP_NONE))
    {
        m_debug_step = GT_DEBUG_STEP_INSTRUCTION;
        m_debug_step_resume = false;
        m_i386->ClearRunToBreakpoint();
    }

    if (!m_debug_interrupt_deferred)
        return false;

    m_debug_interrupt_deferred = false;
    return RunDeferredInterrupt(debug);
}

// An IRQ handler that ran in the middle of the stepped instruction returned to it, so the step goes on
NO_INLINE bool GeartownsCore::BreakpointStopsDebugger(GT_Debug_Run* debug)
{
    if ((m_debug_step == GT_DEBUG_STEP_RETURN) && m_debug_step_resume && m_i386->RunToBreakpointHit())
    {
        m_i386->ClearBreakpointHit();
        m_debug_step = GT_DEBUG_STEP_INSTRUCTION;
        m_debug_step_resume = false;
        return false;
    }

    StopDebugger(debug, true);
    return true;
}

// One instruction of a step, returns true when the debugger stops
NO_INLINE bool GeartownsCore::RunDebuggerStep(u64 frame_start, GT_Debug_Run* debug)
{
    u16 stack_selector = 0;
    u32 stack_pointer = 0;

    if (debug->step_over)
        m_i386->GetStackPointer(stack_selector, stack_pointer);

    u32 slice = m_scheduler->GetSliceClocks(GetFrameLimit(frame_start));
    GT_Bus_Access_Context context = BeginSlice();
    m_i386->RunInstruction(context);

    u32 call_return_linear = 0;

    // Step Over runs a call or a software interrupt until it returns
    if (debug->step_over && m_i386->GetStepCall(call_return_linear))
    {
        m_i386->AddRunToBreakpoint(call_return_linear, stack_selector, stack_pointer);
        m_debug_step = GT_DEBUG_STEP_RETURN;
        m_debug_step_resume = false;
        CompleteSlice(m_i386->GetStepInfo(), context, slice, debug->stop_on_breakpoint);
        return unlikely(m_i386->IsDebuggerHitPending()) && AcceptDebuggerHit(debug);
    }

    u32 return_linear = m_i386->GetCurrentLinearPC();
    bool repeat = m_i386->IsRepeatActive();

    m_i386->GetStackPointer(stack_selector, stack_pointer);

    bool interrupted = CompleteSlice(m_i386->GetStepInfo(), context, slice, debug->stop_on_breakpoint);

    if (unlikely(m_i386->IsDebuggerHitPending()) && AcceptDebuggerHit(debug))
        return true;

    if (interrupted)
    {
        if (!StepInterrupt(debug, return_linear, stack_selector, stack_pointer, repeat))
            return false;
    }
    else if (m_i386->IsRepeatActive() || m_i386->Halted())
        return false;

    StopDebugger(debug, false);
    return true;
}

bool GeartownsCore::RunDeferredInterrupt(GT_Debug_Run* debug)
{
    u32 return_linear = m_i386->GetCurrentLinearPC();
    u16 stack_selector = 0;
    u32 stack_pointer = 0;

    m_i386->GetStackPointer(stack_selector, stack_pointer);

    GT_Bus_Access_Context context = BeginSlice();

    if (!EnterPendingInterrupt(context))
        return false;

    if (unlikely(m_i386->IsDebuggerHitPending()) && AcceptDebuggerHit(debug))
        return true;

    if ((m_debug_step == GT_DEBUG_STEP_INSTRUCTION) &&
        StepInterrupt(debug, return_linear, stack_selector, stack_pointer, true))
    {
        StopDebugger(debug, false);
        return true;
    }

    return false;
}

bool GeartownsCore::StepInterrupt(GT_Debug_Run* debug, u32 return_linear, u16 stack_selector, u32 stack_pointer,
    bool resume)
{
    if (!debug->step_over && !debug->skip_interrupts_on_step)
        return true;

    m_i386->AddRunToBreakpoint(return_linear, stack_selector, stack_pointer);
    m_debug_step = GT_DEBUG_STEP_RETURN;
    m_debug_step_resume = resume;
    return false;
}

NO_INLINE bool GeartownsCore::AcceptDebuggerHit(GT_Debug_Run* debug)
{
    if (!debug->stop_on_breakpoint)
    {
        m_i386->DiscardDebuggerHit();
        return false;
    }

    if (!m_i386->AcceptDebuggerHit())
        return false;

    m_debug_interrupt_deferred = true;
    StopDebugger(debug, true);
    return true;
}

void GeartownsCore::StopDebugger(GT_Debug_Run* debug, bool breakpoint)
{
    debug->stopped = true;
    debug->breakpoint_hit = breakpoint;
    m_debug_step = GT_DEBUG_STEP_NONE;
    m_i386->ClearRunToBreakpoint();
}

void GeartownsCore::EndFrame(u64 frame_start, s16* sample_buffer, int* sample_count)
{
    m_video->EndFrame();

    // Debugger memory views refresh once per executed frame or debugger step
    if (m_scheduler->GetClocks() != frame_start)
        m_memory->InvalidateDebugSnapshot();

    m_audio->Synchronize(m_scheduler->GetClocks());
    m_audio->EndFrame(sample_buffer, sample_count);
}

INLINE bool GeartownsCore::IsFrameDone(u64 frame_start) const
{
    return m_video->IsFrameReady() || (m_scheduler->GetClocks() >= GetFrameLimit(frame_start)) || !m_powered;
}

INLINE u64 GeartownsCore::GetFrameLimit(u64 frame_start) const
{
    return frame_start + (m_video->IsRunning() ? GT_MACHINE_CLOCKS_PER_FRAME * 4 : GT_MACHINE_CLOCKS_PER_FRAME);
}

INLINE GT_Bus_Access_Context GeartownsCore::BeginSlice()
{
    GT_Bus_Access_Context context = {};
    context.origin = GT_BUS_ORIGIN_CPU;
    context.clocks = m_scheduler->GetClocks();
    context.synchronize = &GeartownsCore::SynchronizeIOCallback;
    context.synchronize_memory = &GeartownsCore::SynchronizeMemoryCallback;
    context.poll_port = &GeartownsCore::PollPortCallback;
    context.synchronize_context = this;
    return context;
}

void GeartownsCore::DMADebugCallback(void* cpu, u32 address, u32 size, bool write)
{
    ((I386*)cpu)->RecordDebuggerDMA(address, size, write);
}

// A port access moves machine time up to the start of its instruction and then to the access, running the events
// due on the way. Events due by the start stop the instruction, so the CPU samples INTR before it
GT_Synchronize_Result GeartownsCore::SynchronizeIOCallback(void* core, GT_Bus_Access_Context& context, u32 start_cycles, u32 access_cycles)
{
    GeartownsCore* geartowns = (GeartownsCore*)core;
    Scheduler* scheduler = geartowns->m_scheduler;
    GT_Synchronize_Result result = GT_SYNC_NO_EVENTS;
    scheduler->AddCycles(start_cycles - context.synchronized_cycles);
    context.synchronized_cycles = start_cycles;

    if (scheduler->IsEventDue())
        result = GT_SYNC_EVENTS_AT_START;
    else
    {
        scheduler->AddCycles(access_cycles - start_cycles);
        context.synchronized_cycles = access_cycles;

        if (scheduler->IsEventDue())
            result = GT_SYNC_EVENTS_AT_ACCESS;
    }

    if (result != GT_SYNC_NO_EVENTS)
        geartowns->DispatchEvents();

    context.clocks = scheduler->GetClocks();
    return result;
}

// A memory access that a device must see in time moves machine time up to the start of its instruction
// Events due by then run before the access, and their IRQs wait for the end of the instruction
void GeartownsCore::SynchronizeMemoryCallback(void* core, GT_Bus_Access_Context& context)
{
    GeartownsCore* geartowns = (GeartownsCore*)core;
    Scheduler* scheduler = geartowns->m_scheduler;
    u32 start_cycles = geartowns->m_i386->GetStartCycles();

    if (start_cycles <= context.synchronized_cycles)
        return;

    scheduler->AddCycles(start_cycles - context.synchronized_cycles);
    context.synchronized_cycles = start_cycles;

    if (scheduler->IsEventDue())
    {
        context.end_batch = true;
        geartowns->DispatchEvents();
    }

    context.clocks = scheduler->GetClocks();
}

// CPU cycles from the read just synchronized while the port reads the same, without side effects or due events
u32 GeartownsCore::PollPortCallback(void* core, u16 port, int width)
{
    GeartownsCore* geartowns = (GeartownsCore*)core;
    Scheduler* scheduler = geartowns->m_scheduler;
    u64 stable = geartowns->m_io->GetReadStableUntil(port, width, scheduler->GetClocks());

    return scheduler->GetCyclesBefore(MIN(stable, scheduler->GetNextEventClocks()));
}

// A halted CPU idles through the slice
// Events due by the end of the slice run before the CPU samples INTR at its boundary
// The debugger holds the IRQ back when the slice hit a breakpoint, so it stops right after the instruction
// Returns whether an IRQ was entered
INLINE bool GeartownsCore::CompleteSlice(const I386_Run_Result& result, GT_Bus_Access_Context& context, u32 slice,
    bool hold_on_hit)
{
    bool idle = (result.steps == 0) && m_i386->Halted();

    if (idle)
        m_scheduler->AddClocks(slice);
    else
        m_scheduler->AddCycles((u32)(result.cycles + context.wait_cycles - context.synchronized_cycles));

    if (m_scheduler->IsEventDue())
        DispatchEvents();

    // The board answers a shutdown cycle by resetting the CPU
    if (unlikely(m_i386->Shutdown()))
        m_system_control->RequestCPUReset(k_system_control_reset_shutdown);

    if (unlikely(m_system_control->IsPowerOffPending()))
    {
        PowerOff();
        return false;
    }

    if (unlikely(m_system_control->IsCPUResetPending()))
    {
        ResetCPU();
        return false;
    }

    if (hold_on_hit && m_i386->IsDebuggerHitPending())
        return false;

    return EnterPendingInterrupt(context);
}

INLINE bool GeartownsCore::EnterPendingInterrupt(GT_Bus_Access_Context& context)
{
    if (!m_pic->IsInterruptPending() || !m_i386->CanAcceptMaskableInterrupt())
        return false;

    int line = 0;
    u8 vector = m_pic->AcknowledgeInterrupt(line);
    u32 interrupt_cycles = m_i386->EnterExternalInterrupt(vector, context, line);
    m_scheduler->AddCycles(interrupt_cycles);

    if (m_scheduler->IsEventDue())
        DispatchEvents();

    return true;
}

INLINE void GeartownsCore::DispatchEvents()
{
    while (m_scheduler->IsEventDue())
    {
        u64 clocks = m_scheduler->GetClocks();

        switch (m_scheduler->PopEvent())
        {
            case SCHEDULER_EVENT_PIT:
                m_pit->HandleEvent(clocks);
                break;
            case SCHEDULER_EVENT_VIDEO:
                m_video->HandleEvent(clocks);
                break;
            case SCHEDULER_EVENT_CDROM:
                m_cdrom->HandleEvent(clocks);
                break;
            case SCHEDULER_EVENT_FDC:
                m_fdc->HandleEvent(clocks);
                break;
            case SCHEDULER_EVENT_KEYBOARD:
                m_keyboard->HandleEvent(clocks);
                break;
            case SCHEDULER_EVENT_DMA:
                // The DMA owns the bus for each unit it moves, so the CPU loses that time
                m_scheduler->AddClocks(m_dma->HandleEvent(clocks));
                break;
            case SCHEDULER_EVENT_AUDIO:
                m_audio->HandleEvent(clocks);
                break;
            default:
                break;
        }
    }
}

bool GeartownsCore::PowerOn()
{
    SearchFirmware();

    if (!LoadMachineFirmware(false, true))
        return false;

    Reset();
    m_powered = true;
    return true;
}

void GeartownsCore::PowerOff()
{
    m_powered = false;

    if (IsValidPointer(m_i386))
        m_i386->ResetDisassembler();
}

void GeartownsCore::SetMachineSettings(const GT_Machine_Settings& settings)
{
    m_machine_settings = settings;
    ResolveMachineConfig();

    if (!IsMachineConfigPending())
        m_machine_report = m_pending_machine_report;
}

void GeartownsCore::LatchAutoMedia()
{
    const GT_DB_Entry* cdrom = IsValidPointer(m_media) ? m_media->GetGameDBEntry() : NULL;
    const GT_DB_Entry* floppy = NULL;
    m_auto_three_mode = false;

    for (int i = 0; IsValidPointer(m_fdc) && (i < FDC_DRIVES); i++)
    {
        FloppyDisk* disk = GetFloppy(i);

        if (!disk->IsInserted())
            continue;

        if (!IsValidPointer(floppy) && (disk->GetDataCRC() != 0))
            floppy = Media::FindFloppyEntry(disk->GetDataCRC());

        if ((disk->GetMedia() == FLOPPY_MEDIA_2HD) && (disk->GetRPM() == 300))
            m_auto_three_mode = true;
    }

    bool uses_floppy = !IsValidPointer(cdrom) || ((cdrom->flags & GT_GAMEDB_FLOPPY_MASK) != 0);
    m_auto_entry = (IsValidPointer(floppy) && uses_floppy) ? floppy : cdrom;
    ResolveMachineConfig();

    if (!IsMachineConfigPending())
        m_machine_report = m_pending_machine_report;
}

bool GeartownsCore::IsMachineConfigPending()
{
    return (m_pending_machine_config.model != m_machine_config.model) ||
        (m_pending_machine_config.cpu != m_machine_config.cpu) ||
        (m_pending_machine_config.ram_size != m_machine_config.ram_size) ||
        (m_pending_machine_config.floppy_drives != m_machine_config.floppy_drives) ||
        (m_pending_machine_config.cpu_clock_rate != m_machine_config.cpu_clock_rate);
}

bool GeartownsCore::GetFloppyNotice(GT_Floppy_Notice& notice)
{
    const GT_DB_Entry* entry = m_auto_entry;
    notice.warning = false;
    notice.line_count = 0;

    if (!IsValidPointer(entry) || ((entry->flags & GT_GAMEDB_FLOPPY_MASK) == 0))
        return false;

    for (int i = 0; i < m_machine_config.floppy_drives; i++)
    {
        if (GetFloppy(i)->IsInserted())
            return false;
    }

    notice.warning = (entry->flags & (GT_GAMEDB_FLOPPY_BOOT | GT_GAMEDB_FLOPPY_REQUIRED |
        GT_GAMEDB_FLOPPY_PROTECTED)) != 0;

    if ((entry->flags & GT_GAMEDB_FLOPPY_BOOT) != 0)
        snprintf(notice.lines[notice.line_count++], GT_MACHINE_WARNING_SIZE,
            "%s boots from its floppy disk, insert it in Floppy 1", entry->title);
    else if ((entry->flags & GT_GAMEDB_FLOPPY_REQUIRED) != 0)
        snprintf(notice.lines[notice.line_count++], GT_MACHINE_WARNING_SIZE,
            "%s needs its floppy disk, insert it in Floppy 1", entry->title);
    else if ((entry->flags & GT_GAMEDB_FLOPPY_OPTIONAL) != 0)
        snprintf(notice.lines[notice.line_count++], GT_MACHINE_WARNING_SIZE,
            "%s came with an optional floppy disk, it runs without it", entry->title);

    if ((entry->flags & GT_GAMEDB_FLOPPY_PROTECTED) != 0)
        snprintf(notice.lines[notice.line_count++], GT_MACHINE_WARNING_SIZE,
            "Its original floppy disk is copy protected");

    return true;
}

bool GeartownsCore::GetDatabaseController(int port, GT_Controller_Type& type) const
{
    if (!IsValidPointer(m_auto_entry) || (port < 0) || (port >= GT_MAX_GAMEPADS))
        return false;

    switch ((port == GT_CONTROLLER_1) ? m_auto_entry->port_1 : m_auto_entry->port_2)
    {
        case GT_GAMEDB_PORT_NONE:
            type = GT_CONTROLLER_NONE;
            return true;
        case GT_GAMEDB_PORT_PAD:
            type = GT_CONTROLLER_ORIGINAL_GAMEPAD;
            return true;
        case GT_GAMEDB_PORT_MARTY_PAD:
            type = GT_CONTROLLER_MARTY_GAMEPAD;
            return true;
        case GT_GAMEDB_PORT_6_BUTTON_PAD:
            type = GT_CONTROLLER_6_BUTTON_GAMEPAD;
            return true;
        case GT_GAMEDB_PORT_MOUSE:
            type = GT_CONTROLLER_MOUSE;
            return true;
        default:
            return false;
    }
}

bool GeartownsCore::LoadBios(const char* directory_path)
{
    std::string previous = m_firmware_folder;
    m_firmware_folder = IsValidPointer(directory_path) ? directory_path : "";

    if (ReloadFirmware())
        return true;

    m_firmware_folder = previous;
    SearchFirmware();
    return false;
}

void GeartownsCore::UnloadBios()
{
    m_powered = false;
    m_firmware_folder.clear();

    for (int i = 0; i < GT_MACHINE_COUNT; i++)
        m_model_firmware_folders[i].clear();

    SearchFirmware();

    if (IsValidPointer(m_firmware))
        m_firmware->Unload();

    if (IsValidPointer(m_i386))
        m_i386->ResetDisassembler();
}

void GeartownsCore::SetModelFirmwareFolder(GT_Machine_Model model, const char* directory_path)
{
    m_model_firmware_folders[model] = IsValidPointer(directory_path) ? directory_path : "";
}

bool GeartownsCore::ReloadFirmware()
{
    SearchFirmware();

    if (!LoadMachineFirmware(true, true))
        return false;

    if (m_powered)
        Reset();

    return true;
}

bool GeartownsCore::LoadMedia(const char* file_path)
{
    if (!IsValidPointer(m_media))
        return false;

    bool ok = m_media->LoadMedia(file_path);
    m_cdrom->NotifyMediaChanged();
    return ok;
}

#if defined(GT_ENABLE_PHYSICAL_CDROM)
bool GeartownsCore::LoadPhysicalCdRom(const char* device_id)
{
    if (!IsValidPointer(m_media))
        return false;

    bool ok = m_media->LoadPhysicalCdRom(device_id);
    m_cdrom->NotifyMediaChanged();
    return ok;
}
#endif

void GeartownsCore::EjectMedia()
{
    if (!IsValidPointer(m_media))
        return;

    m_media->Reset();
    m_cdrom->NotifyMediaChanged();
}

void GeartownsCore::ResetMedia()
{
    Reset();
}

bool GeartownsCore::InsertFloppy(int drive, const u8* data, u32 size, bool write_protected, u32 base_crc, u32 data_crc, const u8* source, u32 source_size)
{
    return IsValidPointer(m_fdc) &&
        m_fdc->InsertDisk(drive, data, size, write_protected, base_crc, data_crc, source, source_size);
}

void GeartownsCore::EjectFloppy(int drive)
{
    if (IsValidPointer(m_fdc))
        m_fdc->EjectDisk(drive);
}

void GeartownsCore::SwapFloppies()
{
    if (IsValidPointer(m_fdc))
        m_fdc->SwapDisks();
}

FloppyDisk* GeartownsCore::GetFloppy(int drive)
{
    return IsValidPointer(m_fdc) ? m_fdc->GetDisk(drive) : NULL;
}

void GeartownsCore::KeyPressed(GT_Keys key)
{
    if (IsValidPointer(m_keyboard))
        m_keyboard->KeyPressed(key);
}

void GeartownsCore::KeyReleased(GT_Keys key)
{
    if (IsValidPointer(m_keyboard))
        m_keyboard->KeyReleased(key);
}

void GeartownsCore::ReleaseAllKeys()
{
    if (IsValidPointer(m_keyboard))
        m_keyboard->ReleaseAllKeys();
}

bool GeartownsCore::SaveState(const char* path, int index, bool screenshot)
{
    using namespace std;

    if (!m_powered)
    {
        Error("Machine is not powered on when trying to save state");
        return false;
    }

    string full_path = GetSaveStatePath(path, index);
    string temp_path = full_path + ".tmp";
    Debug("Saving state to %s...", full_path.c_str());

    ofstream stream;
    open_ofstream_utf8(stream, temp_path.c_str(), ios::out | ios::binary);

    if (!stream.is_open())
    {
        Error("Failed to open save state file for writing: %s", temp_path.c_str());
        return false;
    }

    size_t size = 0;

    if (!SaveState(stream, size, screenshot, true))
    {
        stream.close();
        remove_file_utf8(temp_path.c_str());
        Error("Failed to save state to file: %s", full_path.c_str());
        return false;
    }

    stream.close();

    if (!stream.good() || !rename_file_utf8(temp_path.c_str(), full_path.c_str()))
    {
        remove_file_utf8(temp_path.c_str());
        Error("Failed to write save state file: %s", full_path.c_str());
        return false;
    }

    Log("Saved state to %s", full_path.c_str());
    return true;
}

bool GeartownsCore::SaveState(u8* buffer, size_t& size, bool screenshot, bool media)
{
    using namespace std;

    Debug("Saving state to buffer [%zu bytes]...", size);

    if (!m_powered)
    {
        Error("Machine is not powered on when trying to save state");
        return false;
    }

    if (!IsValidPointer(buffer))
    {
        stringstream stream;

        if (!SaveState(stream, size, screenshot, media))
        {
            Error("Failed to save state to stream to calculate size");
            return false;
        }

        return true;
    }

    memory_stream direct_stream(reinterpret_cast<char*>(buffer), size);

    if (!SaveState(direct_stream, size, screenshot, media))
    {
        Error("Failed to save state to buffer");
        return false;
    }

    if (!direct_stream.good())
    {
        Error("Failed to save state to buffer: output buffer is too small");
        return false;
    }

    size = direct_stream.size();
    return true;
}

bool GeartownsCore::GetMaxSaveStateSize(size_t& size)
{
    return SaveState(NULL, size, false);
}

bool GeartownsCore::SaveState(std::ostream& stream, size_t& size, bool screenshot, bool media)
{
    if (!m_powered)
    {
        Error("Machine is not powered on when trying to save state");
        return false;
    }

    Debug("Serializing save state...");

    StateSerializer serializer(stream);
    Serialize(serializer, m_machine_config);

    GT_SaveState_Media state_media = {};

    for (int i = 0; i < FDC_DRIVES; i++)
    {
        state_media.floppy_inserted[i] = m_fdc->GetDisk(i)->IsInserted();
        state_media.floppy_crc[i] = m_fdc->GetDisk(i)->GetBaseCRC();
    }

    state_media.cdrom_inserted = m_media->IsReady();
    state_media.cdrom_crc = m_media->GetCRC();
    SerializeMedia(serializer, state_media);

    m_scheduler->SaveState(stream);
    m_memory->SaveState(stream);
    m_i386->SaveState(stream);
    m_audio->SaveState(stream);
    m_input->SaveState(stream);
    m_pic->SaveState(stream);
    m_pit->SaveState(stream);
    m_system_control->SaveState(stream);
    m_video->SaveState(stream);
    m_cdrom->SaveState(stream);
    m_cdrom_audio->SaveState(stream);
    m_fdc->SaveState(stream, media);
    m_keyboard->SaveState(stream);
    m_rtc->SaveState(stream);
    m_dma->SaveState(stream);

    if (stream.fail())
    {
        Error("Failed to serialize save state");
        return false;
    }

#if defined(__LIBRETRO__)
    GT_SaveState_Header_Libretro header;
    header.magic = GT_SAVESTATE_MAGIC;
    header.version = GT_SAVESTATE_VERSION;
    Debug("Save state header magic: 0x%08X", header.magic);
    Debug("Save state header version: %u", header.version);
#else
    GT_SaveState_Header header;
    header.magic = GT_SAVESTATE_MAGIC;
    header.version = GT_SAVESTATE_VERSION;
    header.timestamp = (s64)time(NULL);
    strncpy_fit(header.rom_name, m_media->GetFileName(), sizeof(header.rom_name));
    header.rom_crc = m_media->GetCRC();
    strncpy_fit(header.emu_build, GT_VERSION, sizeof(header.emu_build));

    Debug("Save state header magic: 0x%08X", header.magic);
    Debug("Save state header version: %u", header.version);
    Debug("Save state header timestamp: %lld", (long long)header.timestamp);
    Debug("Save state header rom name: %s", header.rom_name);
    Debug("Save state header rom crc: 0x%08X", header.rom_crc);
    Debug("Save state header emu build: %s", header.emu_build);

    if (screenshot && IsValidPointer(m_frame_buffer))
    {
        header.screenshot_width = (u16)m_video->GetFrameWidth();
        header.screenshot_height = (u16)m_video->GetFrameHeight();
        header.screenshot_size = header.screenshot_width * header.screenshot_height * 4;
        stream.write(reinterpret_cast<const char*>(m_frame_buffer), header.screenshot_size);
    }
    else
    {
        header.screenshot_size = 0;
        header.screenshot_width = 0;
        header.screenshot_height = 0;
    }

    Debug("Save state header screenshot size: %u", header.screenshot_size);
    Debug("Save state header screenshot width: %u", header.screenshot_width);
    Debug("Save state header screenshot height: %u", header.screenshot_height);
#endif

    std::streampos position = stream.tellp();

    if (position == std::streampos(-1))
    {
        Error("Failed to calculate save state size");
        return false;
    }

    size = static_cast<size_t>(position) + sizeof(header);

#if !defined(__LIBRETRO__)
    header.size = static_cast<u32>(size);
    Debug("Save state header size: %u", header.size);
#endif

    stream.write(reinterpret_cast<const char*>(&header), sizeof(header));

    if (stream.fail())
    {
        Error("Failed to write save state header");
        return false;
    }

    return true;
}

bool GeartownsCore::LoadState(const char* path, int index)
{
    using namespace std;

    bool ret = false;
    string full_path = GetSaveStatePath(path, index);
    Debug("Loading state from %s...", full_path.c_str());

    ifstream stream;
    open_ifstream_utf8(stream, full_path.c_str(), ios::in | ios::binary);

    if (!stream.fail())
    {
        ret = LoadState(stream);

        if (ret)
            Log("Loaded state from %s", full_path.c_str());
        else
            Error("Failed to load state from %s", full_path.c_str());
    }
    else
        Error("Load state file doesn't exist: %s", full_path.c_str());

    stream.close();
    return ret;
}

bool GeartownsCore::LoadState(const u8* buffer, size_t size, GT_SaveState_Media* media)
{
    Debug("Loading state from buffer [%zu bytes]...", size);

    if (!m_powered)
    {
        Error("Machine is not powered on when trying to load state");
        return false;
    }

    if (!IsValidPointer(buffer) || size == 0)
    {
        Error("Invalid load state buffer");
        return false;
    }

    memory_input_stream direct_stream(reinterpret_cast<const char*>(buffer), size);
    return LoadState(direct_stream, media);
}

bool GeartownsCore::LoadState(std::istream& stream, GT_SaveState_Media* media)
{
    using namespace std;

    if (!m_powered)
    {
        Error("Machine is not powered on when trying to load state");
        return false;
    }

    GT_SaveState_Header_Libretro header = {};

    stream.seekg(0, ios::end);
    size_t size = static_cast<size_t>(stream.tellg());

#if !defined(__LIBRETRO__)
    bool is_desktop_savestate = false;
    GT_SaveState_Header desktop_header = {};

    if (size >= sizeof(desktop_header))
    {
        stream.seekg(size - sizeof(desktop_header), ios::beg);
        stream.read(reinterpret_cast<char*>(&desktop_header), sizeof(desktop_header));

        if (desktop_header.magic == GT_SAVESTATE_MAGIC)
        {
            header.magic = desktop_header.magic;
            header.version = desktop_header.version;
            is_desktop_savestate = true;
            Debug("Loading desktop save state");
        }
    }
#endif

    if (header.magic != GT_SAVESTATE_MAGIC && size >= sizeof(header))
    {
        stream.seekg(size - sizeof(header), ios::beg);
        stream.read(reinterpret_cast<char*>(&header), sizeof(header));
    }

    stream.clear();
    stream.seekg(0, ios::beg);

    Debug("Load state header magic: 0x%08X", header.magic);
    Debug("Load state header version: %u", header.version);

    if (header.magic != GT_SAVESTATE_MAGIC)
    {
        Log("Invalid save state: 0x%08X", header.magic);
        return false;
    }

    if ((header.version < GT_SAVESTATE_MIN_VERSION) || (header.version > GT_SAVESTATE_VERSION))
    {
        Error("Invalid save state version: %u", header.version);
        return false;
    }

#if !defined(__LIBRETRO__)
    if (is_desktop_savestate)
    {
        Debug("Load state header size: %u", desktop_header.size);
        Debug("Load state header timestamp: %lld", (long long)desktop_header.timestamp);
        Debug("Load state header rom name: %s", desktop_header.rom_name);
        Debug("Load state header rom crc: 0x%08X", desktop_header.rom_crc);
        Debug("Load state header screenshot size: %u", desktop_header.screenshot_size);
        Debug("Load state header screenshot width: %u", desktop_header.screenshot_width);
        Debug("Load state header screenshot height: %u", desktop_header.screenshot_height);
        Debug("Load state header emu build: %s", desktop_header.emu_build);

        if (desktop_header.size != size)
        {
            Error("Invalid save state size: %u", desktop_header.size);
            return false;
        }

        if (desktop_header.rom_crc != m_media->GetCRC())
        {
            Error("Invalid save state media crc: 0x%08X", desktop_header.rom_crc);
            return false;
        }
    }
#endif

    Debug("Unserializing save state...");

    GT_Machine_Config machine_config = m_machine_config;
    StateSerializer serializer(stream);
    Serialize(serializer, machine_config);
    SanitizeMachineConfig(machine_config);

    // The CPU speed may differ, the hardware may not
    // The running machine keeps its own configuration either way
    bool same_machine = (m_machine_config.model == machine_config.model) &&
        (m_machine_config.cpu == machine_config.cpu) && (m_machine_config.ram_size == machine_config.ram_size) &&
        (m_machine_config.floppy_drives == machine_config.floppy_drives);

    if (!same_machine)
    {
        Error("Save state is for another machine configuration");
        return false;
    }

    GT_SaveState_Media state_media = {};
    SerializeMedia(serializer, state_media);

    if (stream.fail())
    {
        Error("Failed to unserialize save state");
        return false;
    }

    state_media.valid = true;

    bool mount = IsValidPointer(media);

    if (mount)
        *media = state_media;

    for (int i = 0; i < FDC_DRIVES; i++)
    {
        FloppyDisk* disk = m_fdc->GetDisk(i);
        bool other_disk = disk->IsInserted() ? state_media.floppy_crc[i] != disk->GetBaseCRC() : mount;

        if (state_media.floppy_inserted[i] && other_disk)
        {
            Error("Save state is for another disk in floppy drive %d", i + 1);
            return false;
        }
    }

    bool other_disc = m_media->IsReady() ? state_media.cdrom_crc != m_media->GetCRC() : mount;

    if (state_media.cdrom_inserted && other_disc)
    {
        Error("Save state is for another CD-ROM disc");
        return false;
    }

    m_scheduler->LoadState(stream);
    m_memory->LoadState(stream, header.version);
    m_i386->LoadState(stream, header.version);
    m_audio->LoadState(stream, header.version);
    m_input->LoadState(stream);
    m_pic->LoadState(stream);
    m_pit->LoadState(stream, header.version);
    m_system_control->LoadState(stream, header.version);
    m_video->LoadState(stream, header.version);
    m_cdrom->LoadState(stream, header.version);
    m_cdrom_audio->LoadState(stream);
    m_fdc->LoadState(stream, header.version);
    m_keyboard->LoadState(stream);
    m_rtc->LoadState(stream);
    m_dma->LoadState(stream);

    if (stream.fail())
    {
        Error("Failed to unserialize save state");
        return false;
    }

    ApplyBusTiming();
    return true;
}

bool GeartownsCore::GetSaveStateHeader(int index, const char* path, GT_SaveState_Header* header)
{
    using namespace std;

    if (!IsValidPointer(header))
        return false;

    string full_path = GetSaveStatePath(path, index);
    Debug("Loading state header from %s...", full_path.c_str());

    ifstream stream;
    open_ifstream_utf8(stream, full_path.c_str(), ios::in | ios::binary);

    if (stream.fail())
    {
        Debug("Save state file doesn't exist: %s", full_path.c_str());
        stream.close();
        return false;
    }

    stream.seekg(0, ios::end);
    size_t savestate_size = static_cast<size_t>(stream.tellg());

    if (savestate_size < sizeof(GT_SaveState_Header))
    {
        Error("Invalid save state file size: %zu", savestate_size);
        stream.close();
        return false;
    }

    stream.seekg(savestate_size - sizeof(GT_SaveState_Header), ios::beg);
    stream.read(reinterpret_cast<char*>(header), sizeof(GT_SaveState_Header));

    if (stream.fail())
    {
        Error("Failed to read save state header from %s", full_path.c_str());
        stream.close();
        return false;
    }

    stream.close();

    if (header->magic != GT_SAVESTATE_MAGIC)
    {
        Error("Invalid save state magic: 0x%08X", header->magic);
        return false;
    }

    if (header->size != savestate_size)
    {
        Error("Invalid save state size: %u", header->size);
        return false;
    }

    return true;
}

bool GeartownsCore::GetSaveStateScreenshot(int index, const char* path, GT_SaveState_Screenshot* screenshot)
{
    using namespace std;

    if (!IsValidPointer(screenshot) || !IsValidPointer(screenshot->data) || screenshot->size == 0)
    {
        Error("Invalid save state screenshot buffer");
        return false;
    }

    string full_path = GetSaveStatePath(path, index);
    Debug("Loading state screenshot from %s...", full_path.c_str());

    ifstream stream;
    open_ifstream_utf8(stream, full_path.c_str(), ios::in | ios::binary);

    if (stream.fail())
    {
        Error("Save state file doesn't exist: %s", full_path.c_str());
        stream.close();
        return false;
    }

    GT_SaveState_Header header;

    if (!GetSaveStateHeader(index, path, &header))
    {
        Error("Invalid save state header");
        stream.close();
        return false;
    }

    if (header.screenshot_size == 0)
    {
        Debug("No screenshot data");
        stream.close();
        return false;
    }

    if ((header.screenshot_width == 0) || (header.screenshot_height == 0) ||
        (header.screenshot_width > GT_MAX_FRAME_BUFFER_WIDTH) || (header.screenshot_height > GT_MAX_FRAME_BUFFER_HEIGHT) ||
        (header.screenshot_size != (u32)header.screenshot_width * header.screenshot_height * 4))
    {
        Error("Invalid screenshot dimensions %ux%u for %u bytes", header.screenshot_width, header.screenshot_height, header.screenshot_size);
        stream.close();
        return false;
    }

    if (screenshot->size < header.screenshot_size)
    {
        Error("Invalid screenshot buffer size %u < %u", screenshot->size, header.screenshot_size);
        stream.close();
        return false;
    }

    if (header.size < sizeof(header) + header.screenshot_size)
    {
        Error("Invalid screenshot offset");
        stream.close();
        return false;
    }

    screenshot->size = header.screenshot_size;
    screenshot->width = header.screenshot_width;
    screenshot->height = header.screenshot_height;

    stream.seekg(header.size - sizeof(header) - screenshot->size, ios::beg);
    stream.read(reinterpret_cast<char*>(screenshot->data), screenshot->size);
    bool success = !stream.fail();
    stream.close();
    return success;
}

void GeartownsCore::Serialize(StateSerializer& serializer, GT_Machine_Config& config)
{
    G_SERIALIZE(serializer, config.model);
    G_SERIALIZE(serializer, config.cpu);
    G_SERIALIZE(serializer, config.ram_size);
    G_SERIALIZE(serializer, config.floppy_drives);
    G_SERIALIZE(serializer, config.cpu_clock_rate);
}

void GeartownsCore::SerializeMedia(StateSerializer& serializer, GT_SaveState_Media& media)
{
    for (int i = 0; i < FDC_DRIVES; i++)
    {
        G_SERIALIZE(serializer, media.floppy_inserted[i]);
        G_SERIALIZE(serializer, media.floppy_crc[i]);
    }

    G_SERIALIZE(serializer, media.cdrom_inserted);
    G_SERIALIZE(serializer, media.cdrom_crc);
}

void GeartownsCore::SanitizeMachineConfig(GT_Machine_Config& config)
{
    bool known_model = ((int)config.model >= 0) && ((int)config.model < GT_MACHINE_COUNT);

    if (!known_model || !k_machine_profiles[config.model].emulated)
        config.model = GT_MACHINE_MODEL1_2;

    const GT_Machine_Profile& profile = k_machine_profiles[config.model];
    const u8* sizes = profile.ram_sizes_mb;
    u64 megabyte = 1024 * 1024;
    u64 ram_mb = config.ram_size == 0 ? (u64)profile.ram_default_mb : ((u64)config.ram_size + megabyte - 1) / megabyte;
    int size = 0;

    while ((sizes[size + 1] != 0) && (sizes[size] < ram_mb))
        size++;

    config.cpu = profile.cpu;
    config.ram_size = (u32)(sizes[size] * megabyte);
    config.floppy_drives = profile.floppy_drives;

    if (config.cpu_clock_rate == 0)
        config.cpu_clock_rate = profile.cpu_clock_rate;
}

std::string GeartownsCore::GetSaveStatePath(const char* path, int index)
{
    using namespace std;

    if (index < 0)
    {
        if (IsValidPointer(path))
            return path;

        string full_path = m_media->GetFilePath();
        string::size_type dot_index = full_path.rfind('.');

        if (dot_index != string::npos)
            full_path.replace(dot_index + 1, full_path.length() - dot_index - 1, "state");

        return full_path;
    }

    string full_path;

    if (IsValidPointer(path))
    {
        full_path = path;
        append_path_component(full_path, m_media->GetFileName());
    }
    else
        full_path = m_media->GetFilePath();

    string::size_type dot_index = full_path.rfind('.');

    if (dot_index != string::npos)
        full_path.replace(dot_index + 1, full_path.length() - dot_index - 1, "state");

    stringstream ss;
    ss << index;
    full_path += ss.str();
    return full_path;
}

void GeartownsCore::GetRuntimeInfo(GT_Runtime_Info& runtime_info)
{
    runtime_info.screen_width = IsValidPointer(m_video) ? m_video->GetFrameWidth() : GT_FRAME_BUFFER_WIDTH;
    runtime_info.screen_height = IsValidPointer(m_video) ? m_video->GetFrameHeight() : GT_FRAME_BUFFER_HEIGHT;
    runtime_info.display_width = IsValidPointer(m_video) ? m_video->GetDisplayWidth() : GT_FRAME_BUFFER_WIDTH;
    runtime_info.display_height = IsValidPointer(m_video) ? m_video->GetDisplayHeight() : GT_FRAME_BUFFER_HEIGHT;
    runtime_info.frame_time = (float)((GT_MACHINE_CLOCKS_PER_FRAME * 1000.0) / GT_MACHINE_CLOCK_RATE);

    // A running CRTC paces frames at its VSYNC rate
    if (IsValidPointer(m_video) && m_video->IsRunning())
        runtime_info.frame_time = m_video->GetFrameTime();

    runtime_info.sample_rate = GT_AUDIO_SAMPLE_RATE;
    runtime_info.media_ready = IsValidPointer(m_media) && m_media->IsReady();
    runtime_info.bios_ready = IsValidPointer(m_firmware) && m_firmware->IsReady();
    runtime_info.paused = m_paused;
}

void GeartownsCore::Reset()
{
    m_paused = false;
    m_debug_step = GT_DEBUG_STEP_NONE;
    m_debug_step_resume = false;
    m_debug_interrupt_deferred = false;
    LoadMachineFirmware(false, false);
    ApplyMachineConfig();

    if (IsValidPointer(m_scheduler))
        m_scheduler->Reset();

    if (IsValidPointer(m_profiler))
        m_profiler->Reset();

    if (IsValidPointer(m_memory))
        m_memory->Reset();

    if (IsValidPointer(m_io))
        m_io->Reset();

    if (IsValidPointer(m_pic))
        m_pic->Reset();

    if (IsValidPointer(m_pit))
        m_pit->Reset();

    if (IsValidPointer(m_system_control))
        m_system_control->Reset();

    if (IsValidPointer(m_video))
        m_video->Reset();

    if (IsValidPointer(m_cdrom))
        m_cdrom->Reset();

    if (IsValidPointer(m_cdrom_audio))
        m_cdrom_audio->Reset();

    if (IsValidPointer(m_fdc))
        m_fdc->Reset();

    if (IsValidPointer(m_keyboard))
        m_keyboard->Reset();

    if (IsValidPointer(m_rtc))
        m_rtc->Reset(m_scheduler->GetClocks());

    if (IsValidPointer(m_dma))
        m_dma->Reset();

    InitMemoryMap();

    if (IsValidPointer(m_i386))
        m_i386->Reset();

    if (IsValidPointer(m_audio))
        m_audio->Reset();

    if (IsValidPointer(m_input))
        m_input->Reset();

    ApplyBusTiming();
}

// Only the CPU restarts and the low memory view returns to its boot setup, memory and devices keep their state
void GeartownsCore::ResetCPU()
{
    m_system_control->AcknowledgeCPUReset();
    m_memory->ResetMapping();
    m_video->ResetFMRView();
    m_i386->Reset();
}

void GeartownsCore::ResolveMachineConfig()
{
    GT_Machine_Config& config = m_pending_machine_config;
    GT_Machine_Report& report = m_pending_machine_report;
    const GT_DB_Entry* entry = m_auto_entry;
    bool auto_model = m_machine_settings.model < 0;
    bool auto_cpu = m_machine_settings.cpu_mhz <= 0;
    bool auto_ram = m_machine_settings.ram_mb <= 0;
    bool database = auto_model && IsValidPointer(entry);
    GT_Machine_Model named = database ? (GT_Machine_Model)entry->model : GT_MACHINE_MODEL1_2;
    bool three_mode = auto_model && m_auto_three_mode && !k_machine_profiles[named].three_mode_floppy;
    GT_Machine_Model fallback = GT_MACHINE_MODEL1_2;
    u32 megabyte = 1024 * 1024;

    for (int i = 0; three_mode && (i < GT_MACHINE_RELEASE_ORDER_COUNT); i++)
    {
        if (!k_machine_profiles[named].three_mode_floppy)
            named = k_machine_release_order[i];
    }

    for (int i = 0; (i < GT_MACHINE_RELEASE_ORDER_COUNT) && !CanAutoSelect(fallback); i++)
    {
        if (CanAutoSelect(k_machine_release_order[i]))
            fallback = k_machine_release_order[i];
    }

    config.model = !auto_model ? (GT_Machine_Model)m_machine_settings.model : CanAutoSelect(named) ? named : fallback;
    config.cpu_clock_rate = auto_cpu ? 0 : (u32)m_machine_settings.cpu_mhz * 1000000;
    config.ram_size = auto_ram ? 0 : (u32)m_machine_settings.ram_mb * megabyte;
    report.model = auto_model ? GT_MACHINE_SOURCE_DEFAULT : GT_MACHINE_SOURCE_USER;
    report.cpu_clock = auto_cpu ? GT_MACHINE_SOURCE_MODEL : GT_MACHINE_SOURCE_USER;
    report.ram = auto_ram ? GT_MACHINE_SOURCE_MODEL : GT_MACHINE_SOURCE_USER;
    report.fallback = auto_model && (config.model != named);
    report.title = IsValidPointer(entry) ? entry->title : NULL;
    report.warning_count = 0;

    if (database)
        report.model = GT_MACHINE_SOURCE_DATABASE;

    if (three_mode)
        report.model = GT_MACHINE_SOURCE_FLOPPY;

    if (database && auto_cpu && (entry->cpu_mhz > 0))
    {
        config.cpu_clock_rate = entry->cpu_mhz * 1000000;
        report.cpu_clock = GT_MACHINE_SOURCE_DATABASE;
    }

    if (database && auto_ram && (entry->ram_mb > 0))
    {
        config.ram_size = entry->ram_mb * megabyte;
        report.ram = GT_MACHINE_SOURCE_DATABASE;
    }

    SanitizeMachineConfig(config);

    if ((database || three_mode) && (config.model != named))
        snprintf(report.warnings[report.warning_count++], GT_MACHINE_WARNING_SIZE, k_machine_profiles[named].emulated ?
            "Needs an %s, no firmware found for it" : "Needs an %s, not emulated yet", k_machine_profiles[named].name);
    else if (m_auto_three_mode && !k_machine_profiles[config.model].three_mode_floppy)
        snprintf(report.warnings[report.warning_count++], GT_MACHINE_WARNING_SIZE,
            "This machine has no drive for 1.44 MB floppy disks");

    if (!IsValidPointer(entry))
        return;

    if (config.ram_size < entry->ram_mb * megabyte)
        snprintf(report.warnings[report.warning_count++], GT_MACHINE_WARNING_SIZE,
            "%s needs %d MB, this machine has %u MB", entry->title, entry->ram_mb, config.ram_size / megabyte);

    if (config.cpu_clock_rate < entry->cpu_mhz * 1000000u)
        snprintf(report.warnings[report.warning_count++], GT_MACHINE_WARNING_SIZE,
            "%s needs %d MHz, this machine runs at %u MHz", entry->title, entry->cpu_mhz,
            config.cpu_clock_rate / 1000000);

    if ((entry->flags & GT_GAMEDB_CPU_486) != 0)
        snprintf(report.warnings[report.warning_count++], GT_MACHINE_WARNING_SIZE,
            "%s needs a 486 CPU, not emulated yet", entry->title);

    if ((entry->flags & GT_GAMEDB_HDD) != 0)
        snprintf(report.warnings[report.warning_count++], GT_MACHINE_WARNING_SIZE,
            "%s installs to a hard disk, not emulated yet", entry->title);
}

bool GeartownsCore::CanAutoSelect(GT_Machine_Model model)
{
    GT_Firmware_Source_Type type = m_firmware_sources[model].type;
    return k_machine_profiles[model].emulated && (type != GT_FIRMWARE_SOURCE_NONE) &&
        (type != GT_FIRMWARE_SOURCE_OTHER_SET);
}

void GeartownsCore::SearchFirmware()
{
    Firmware::FindSources(m_firmware_folder.c_str(), m_model_firmware_folders, m_firmware_sources);
    ResolveMachineConfig();

    if (!IsMachineConfigPending())
        m_machine_report = m_pending_machine_report;
}

bool GeartownsCore::LoadMachineFirmware(bool force, bool report)
{
    if (!IsValidPointer(m_firmware))
        return false;

    GT_Machine_Model model = m_pending_machine_config.model;
    const GT_Firmware_Source& source = m_firmware_sources[model];
    const GT_Firmware_Source& loaded = m_firmware->GetSource();

    if (source.type == GT_FIRMWARE_SOURCE_NONE)
    {
        if (report)
            Error("No firmware found for %s", k_machine_profiles[model].name);

        return false;
    }

    if (!force && m_firmware->IsReady() && (loaded.zip == source.zip) && (loaded.crc == source.crc) &&
        (strcmp(loaded.path, source.path) == 0))
        return true;

    if (!m_firmware->LoadSource(source))
        return false;

    if (IsValidPointer(m_i386))
        m_i386->ResetDisassembler();

    return true;
}

void GeartownsCore::ApplyMachineConfig()
{
    m_machine_config = m_pending_machine_config;
    m_machine_report = m_pending_machine_report;
    const GT_Machine_Profile& profile = k_machine_profiles[m_machine_config.model];

    if (IsValidPointer(m_scheduler))
        m_scheduler->SetCPUClockRate(m_machine_config.cpu_clock_rate);

    if (IsValidPointer(m_profiler))
        m_profiler->SetCPUClockRate(m_machine_config.cpu_clock_rate);

    if (IsValidPointer(m_memory))
    {
        m_memory->SetMainRAMSize(m_machine_config.ram_size);
        m_memory->SetFeatures(profile.features);
    }

    if (IsValidPointer(m_pit))
        m_pit->SetFeatures(profile.features);

    if (IsValidPointer(m_video))
    {
        m_video->SetFeatures(profile.features);
        m_video->SetTiming(*profile.video_timing);
    }

    if (IsValidPointer(m_system_control))
    {
        bool serial_rom = IsValidPointer(m_firmware) && m_firmware->IsReady() &&
            !m_firmware->GetInfo(GT_FIRMWARE_SERIAL).synthetic;
        m_system_control->SetMachineID(profile.machine_id);
        m_system_control->SetSerialRom(serial_rom ? m_firmware->GetSerialRom() : NULL);
        m_system_control->SetFeatures(profile.features);
        m_system_control->SetCPUClockRate(m_machine_config.cpu_clock_rate);
    }

    if (IsValidPointer(m_i386))
    {
        m_i386->SetResetEDX(k_machine_cpus[m_machine_config.cpu].reset_edx);
        m_i386->SetCR0FixedBits(k_machine_cpus[m_machine_config.cpu].cr0_fixed);
    }

    if (IsValidPointer(m_fdc))
    {
        m_fdc->SetInternalDrives(m_machine_config.floppy_drives);
        m_fdc->SetThreeMode(profile.three_mode_floppy);
        m_fdc->SetFeatures(profile.features);
    }
}

void GeartownsCore::ApplyBusTiming()
{
    if (!IsValidPointer(m_i386) || !IsValidPointer(m_system_control))
        return;

    const GT_Machine_Profile& profile = k_machine_profiles[m_machine_config.model];
    const GT_Machine_Bus_Timing* timing = profile.bus_timing;
    const SystemControl::SystemControl_State* state = m_system_control->GetState();
    const GT_Machine_Bus_Waits* waits = IsValidPointer(timing) ? timing->compatible : NULL;
    GT_Machine_Bus_Waits ports;
    u32 cpu_clock_rate = m_machine_config.cpu_clock_rate;

    if (IsValidPointer(timing) && timing->mode == GT_MACHINE_BUS_SPEED_PORT && state->high_speed)
        waits = timing->fast;
    else if (IsValidPointer(timing) && timing->mode == GT_MACHINE_BUS_WAIT_PORTS)
    {
        ports = *timing->compatible;
        ports.dram_cycle[0] = ports.dram_cycle[1] = (u16)((state->port_05e2 & 0x0F) << 8);
        ports.vram_cycle[0] = ports.vram_cycle[1] = (u16)((state->port_05e6 & 0x0F) << 8);
        ports.sprite_cycle[0] = ports.sprite_cycle[1] = ports.vram_cycle[0];
        waits = &ports;
    }

    m_i386->SetBusTiming(timing, waits, k_machine_cpus[m_machine_config.cpu].bus_width, cpu_clock_rate,
        profile.cpu_clock_rate);

    u32 microsecond = cpu_clock_rate / 1000000;
    u32 cycle = 2 + m_i386->GetBusPortWait(true);
    m_system_control->SetWriteWait(!m_i386->IsBusTimed() ? microsecond : microsecond > cycle ? microsecond - cycle : 0);
}

void GeartownsCore::BusModeCallback(void* context)
{
    static_cast<GeartownsCore*>(context)->ApplyBusTiming();
}

void GeartownsCore::InitMemoryMap()
{
    if (!IsValidPointer(m_memory) || !IsValidPointer(m_firmware) || !m_firmware->IsReady())
        return;

    u32 rom_flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_EXECUTABLE | GT_DEBUG_REGION_ROM | GT_DEBUG_REGION_MAPPED;
    u32 ram_flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_WRITABLE | GT_DEBUG_REGION_EXECUTABLE |
        GT_DEBUG_REGION_MAPPED;
    const GT_Machine_Memory_Map& map = k_machine_maps[k_machine_profiles[m_machine_config.model].map];
    const u8* system_rom = m_firmware->GetSystemRom();
    const u8* boot_rom = system_rom + GT_FIRMWARE_SYSTEM_SIZE - GT_FIRMWARE_SYSTEM_BOOT_SIZE;
    u8* main_ram = m_memory->GetMainRAM();

    m_memory->SetPhysicalAddressMask(map.address_mask);
    m_memory->SetDMAWindows(map.dma_windows);

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_SYSTEM_ROM, "System ROM", system_rom, NULL,
        GT_FIRMWARE_SYSTEM_SIZE, map.system_rom, rom_flags))
        Error("Unable to map the system ROM");

    u32 boot_rom_flags = (rom_flags & ~GT_DEBUG_REGION_MAPPED) | GT_DEBUG_REGION_OVERLAY;

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_SYSTEM_ROM_LOW_ALIAS, "Boot ROM Window", boot_rom,
        NULL, GT_FIRMWARE_SYSTEM_BOOT_SIZE, 0x000F8000U, boot_rom_flags))
        Error("Unable to register the low system ROM alias");

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_MAIN_RAM, "Main RAM", main_ram, main_ram,
        m_memory->GetMainRAMSize(), 0, ram_flags))
        Error("Unable to map main RAM");

    u32 vram_flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_WRITABLE | GT_DEBUG_REGION_VIDEO;
    u32 sprite_ram_flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_WRITABLE | GT_DEBUG_REGION_MAPPED;
    u8* vram = m_video->GetVRAM();
    u8* sprite_ram = m_video->GetSpriteRAM();

    // Canonical VRAM for raw debugger access, the bus reaches it through the two views
    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_VRAM, "VRAM", vram, vram, VIDEO_VRAM_SIZE, 0, vram_flags))
        Error("Unable to register VRAM");

    if (!m_memory->RegisterHandlerRegion(GT_DEBUG_REGION_VRAM_TWO_PAGE, "VRAM Two-Page", VIDEO_VRAM_SIZE,
        map.vram_two_page, vram_flags | GT_DEBUG_REGION_MAPPED, m_video, Video::ReadVRAMTwoPageCallback,
        Video::WriteVRAMTwoPageCallback, Video::PeekVRAMTwoPageCallback) ||
        !m_memory->SetRegionWideHandlers(GT_DEBUG_REGION_VRAM_TWO_PAGE, Video::WriteVRAMTwoPage16Callback,
        Video::WriteVRAMTwoPage32Callback))
        Error("Unable to map the two-page VRAM view");

    if (!m_memory->RegisterHandlerRegion(GT_DEBUG_REGION_VRAM_SINGLE_PAGE, "VRAM Single-Page", VIDEO_VRAM_SIZE,
        map.vram_single_page, vram_flags | GT_DEBUG_REGION_MAPPED, m_video, Video::ReadVRAMSinglePageCallback,
        Video::WriteVRAMSinglePageCallback, Video::PeekVRAMSinglePageCallback) ||
        !m_memory->SetRegionWideHandlers(GT_DEBUG_REGION_VRAM_SINGLE_PAGE, Video::WriteVRAMSinglePage16Callback,
        Video::WriteVRAMSinglePage32Callback))
        Error("Unable to map the single-page VRAM view");

    if (!m_memory->RegisterHandlerRegion(GT_DEBUG_REGION_SPRITE_RAM, "Sprite RAM", VIDEO_SPRITE_RAM_SIZE, map.sprite_ram,
        sprite_ram_flags, m_video, Video::ReadSpriteRAMCallback, Video::WriteSpriteRAMCallback, NULL) ||
        !m_memory->SetRegionWideHandlers(GT_DEBUG_REGION_SPRITE_RAM, Video::WriteSpriteRAM16Callback,
        Video::WriteSpriteRAM32Callback) ||
        !m_memory->SetRegionData(GT_DEBUG_REGION_SPRITE_RAM, sprite_ram, sprite_ram))
        Error("Unable to map sprite RAM");

    u32 data_rom_flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_ROM | GT_DEBUG_REGION_MAPPED;

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_OS_ROM, "OS ROM", m_firmware->GetOsRom(), NULL,
        GT_FIRMWARE_OS_SIZE, map.os_rom, rom_flags))
        Error("Unable to map the OS ROM");

    if ((map.extension_rom != 0) && !m_memory->RegisterDebugRegion(GT_DEBUG_REGION_EXTENSION_ROM, "Extension ROM",
        m_firmware->GetExtensionRom(), NULL, GT_FIRMWARE_EXTENSION_MAP_SIZE, map.extension_rom, rom_flags))
        Error("Unable to map the extension ROM");

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_DICTIONARY_ROM, "Dictionary ROM", m_firmware->GetDictionaryRom(),
        NULL, GT_FIRMWARE_DICTIONARY_SIZE, map.dictionary_rom, data_rom_flags))
        Error("Unable to map the dictionary ROM");

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_FONT_ROM, "Font ROM", m_firmware->GetFontRom(), NULL,
        GT_FIRMWARE_FONT_SIZE, map.font_rom, data_rom_flags))
        Error("Unable to map the font ROM");

    u8* cmos = m_memory->GetCMOS();

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_CMOS, "CMOS RAM", cmos, cmos, GT_CMOS_SIZE, map.cmos,
        GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_WRITABLE | GT_DEBUG_REGION_MAPPED))
        Error("Unable to map CMOS RAM");

    if (!m_memory->RegisterHandlerRegion(GT_DEBUG_REGION_PCM_WINDOW, "PCM Bank Window", 0x1000, map.pcm_window,
        GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_WRITABLE | GT_DEBUG_REGION_MAPPED | GT_DEBUG_REGION_MMIO |
        GT_DEBUG_REGION_AUDIO, m_audio, Audio::ReadWaveWindowCallback, Audio::WriteWaveWindowCallback,
        Audio::PeekWaveWindowCallback))
        Error("Unable to map the PCM wave RAM window");

    u8* wave_ram = m_audio->GetRF5C68()->GetState()->wave_ram;

    // All 64 KiB of wave RAM for raw debugger access
    // The CPU only reaches it through the 4 KiB window
    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_PCM_RAM, "PCM Wave RAM", wave_ram, wave_ram,
        RF5C68_WAVE_RAM_SIZE, 0, GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_WRITABLE | GT_DEBUG_REGION_AUDIO))
        Error("Unable to register PCM wave RAM");

    // The low windows are overlays that the mapping latches switch on and off
    u32 overlay_flags = GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_WRITABLE | GT_DEBUG_REGION_OVERLAY;

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_DICTIONARY_ROM_LOW_WINDOW, "Dictionary Window",
        m_firmware->GetDictionaryRom(), NULL, 0x8000, 0x000D0000U,
        GT_DEBUG_REGION_READABLE | GT_DEBUG_REGION_ROM | GT_DEBUG_REGION_OVERLAY))
        Error("Unable to register the low dictionary ROM window");

    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_CMOS_LOW_WINDOW, "CMOS Window", cmos, cmos,
        GT_CMOS_SIZE, 0x000D8000U, overlay_flags))
        Error("Unable to register the low CMOS RAM window");

    if (!m_memory->RegisterHandlerRegion(GT_DEBUG_REGION_FMR_PLANES, "FM-R VRAM Planes", 0x8000, 0x000C0000U,
        overlay_flags | GT_DEBUG_REGION_VIDEO, m_video, Video::ReadFMRPlanesCallback, Video::WriteFMRPlanesCallback,
        Video::PeekFMRPlanesCallback))
        Error("Unable to register the FM-R VRAM planes");

    if (!m_memory->RegisterHandlerRegion(GT_DEBUG_REGION_FMR_TEXT, "FM-R Text RAM", 0x7000, 0x000C8000U,
        overlay_flags | GT_DEBUG_REGION_VIDEO, m_video, Video::ReadFMRTextCallback, Video::WriteFMRTextCallback,
        Video::PeekFMRTextCallback))
        Error("Unable to register the FM-R text window");

    if (!m_memory->RegisterHandlerRegion(GT_DEBUG_REGION_FMR_REGISTERS, "FM-R Registers", 0x1000, 0x000CF000U,
        overlay_flags | GT_DEBUG_REGION_MMIO, m_video, Video::ReadFMRRegisterCallback, Video::WriteFMRRegisterCallback,
        Video::PeekFMRRegisterCallback))
        Error("Unable to register the FM-R registers");

    // Registered after the dictionary and CMOS windows so they take priority over it
    if (!m_memory->RegisterDebugRegion(GT_DEBUG_REGION_FMR_VIEW, "FM-R Unmapped", NULL, NULL, 0x20000,
        0x000D0000U, GT_DEBUG_REGION_OVERLAY))
        Error("Unable to register the FM-R view");

    m_memory->ResetMapping();
    m_video->UpdateVRAMMapping();
}
