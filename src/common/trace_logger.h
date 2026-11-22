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

#ifndef TRACE_LOGGER_H
#define TRACE_LOGGER_H

#include "common.h"

#define GT_TRACE_HEADER_SIZE 16
#define GT_TRACE_ENTRY_SIZE 96
#define GT_TRACE_FILTER_RANGES 4
#define GT_TRACE_FILTER_FUNCTIONS 8
#define GT_TRACE_INTERRUPT_TEXT_SIZE 27

enum GT_Trace_Type : u8
{
    TRACE_CPU = 0,
    TRACE_CPU_INTERRUPT,
    TRACE_IO,
    TRACE_PIC,
    TRACE_TIMER,
    TRACE_DMA,
    TRACE_VIDEO,
    TRACE_SPRITE,
    TRACE_FM,
    TRACE_PCM,
    TRACE_PCM16,
    TRACE_MIXER,
    TRACE_CDROM,
    TRACE_FDC,
    TRACE_SCSI,
    TRACE_KEYBOARD,
    TRACE_INPUT,
    TRACE_SYSTEM,
    TRACE_MARK,
    TRACE_TYPE_COUNT
};

static_assert(TRACE_TYPE_COUNT < 32, "Trace category flags exceed 32 bits");

#define TRACE_FLAG_CPU              (1U << TRACE_CPU)
#define TRACE_FLAG_CPU_INTERRUPT    (1U << TRACE_CPU_INTERRUPT)
#define TRACE_FLAG_IO               (1U << TRACE_IO)
#define TRACE_FLAG_PIC              (1U << TRACE_PIC)
#define TRACE_FLAG_TIMER            (1U << TRACE_TIMER)
#define TRACE_FLAG_DMA              (1U << TRACE_DMA)
#define TRACE_FLAG_VIDEO            (1U << TRACE_VIDEO)
#define TRACE_FLAG_SPRITE           (1U << TRACE_SPRITE)
#define TRACE_FLAG_FM               (1U << TRACE_FM)
#define TRACE_FLAG_PCM              (1U << TRACE_PCM)
#define TRACE_FLAG_PCM16            (1U << TRACE_PCM16)
#define TRACE_FLAG_MIXER            (1U << TRACE_MIXER)
#define TRACE_FLAG_CDROM            (1U << TRACE_CDROM)
#define TRACE_FLAG_FDC              (1U << TRACE_FDC)
#define TRACE_FLAG_SCSI             (1U << TRACE_SCSI)
#define TRACE_FLAG_KEYBOARD         (1U << TRACE_KEYBOARD)
#define TRACE_FLAG_INPUT            (1U << TRACE_INPUT)
#define TRACE_FLAG_SYSTEM           (1U << TRACE_SYSTEM)
#define TRACE_FLAG_MARK             (1U << TRACE_MARK)

#define TRACE_ENTRY_NEXT_PC         0x0001
#define TRACE_ENTRY_IGNORED         0x0002
#define TRACE_ENTRY_DEBUGGER        0x0004
#define TRACE_ENTRY_UNMAPPED        0x0008
#define TRACE_ENTRY_TYPE_SHIFT      8

enum GT_Trace_Mark_Event : u8
{
    TRACE_MARK_START = 0,
    TRACE_MARK_FRAME,
    TRACE_MARK_STOP,
    TRACE_MARK_CONTINUE,
    TRACE_MARK_STATE_LOAD,
    TRACE_MARK_REWIND,
    TRACE_MARK_RESET,
    TRACE_MARK_POWER,
    TRACE_MARK_MEDIA,
    TRACE_MARK_DEBUGGER_OFF,
    TRACE_MARK_LOST
};

enum GT_Trace_CPU_Interrupt_Event : u8
{
    TRACE_CPU_INTERRUPT_HARDWARE = 0,
    TRACE_CPU_INTERRUPT_EXCEPTION,
    TRACE_CPU_INTERRUPT_SOFTWARE,
    TRACE_CPU_INTERRUPT_RAISE,
    TRACE_CPU_INTERRUPT_SHUTDOWN,
    TRACE_CPU_INTERRUPT_RETURN,
    TRACE_CPU_INTERRUPT_MODE,
    TRACE_CPU_INTERRUPT_PAGING,
    TRACE_CPU_INTERRUPT_TASK,
    TRACE_CPU_INTERRUPT_TABLES,
    TRACE_CPU_INTERRUPT_MSR
};

enum GT_Trace_CPU_Cause : u8
{
    TRACE_CPU_CAUSE_MOV_CR0 = 0,
    TRACE_CPU_CAUSE_LMSW,
    TRACE_CPU_CAUSE_MOV_CR3,
    TRACE_CPU_CAUSE_MOV_CR4,
    TRACE_CPU_CAUSE_INVLPG,
    TRACE_CPU_CAUSE_LGDT,
    TRACE_CPU_CAUSE_LIDT,
    TRACE_CPU_CAUSE_CPUID,
    TRACE_CPU_CAUSE_RDTSC,
    TRACE_CPU_CAUSE_RDMSR,
    TRACE_CPU_CAUSE_WRMSR,
    TRACE_CPU_CAUSE_TASK_JUMP,
    TRACE_CPU_CAUSE_TASK_CALL,
    TRACE_CPU_CAUSE_TASK_IRET,
    TRACE_CPU_CAUSE_TASK_INTERRUPT
};

#define TRACE_CPU_INTERRUPT_EVENT_IRQS       (1U << TRACE_CPU_INTERRUPT_HARDWARE)
#define TRACE_CPU_INTERRUPT_EVENT_EXCEPTIONS ((1U << TRACE_CPU_INTERRUPT_EXCEPTION) | (1U << TRACE_CPU_INTERRUPT_RAISE) | \
    (1U << TRACE_CPU_INTERRUPT_SHUTDOWN))
#define TRACE_CPU_INTERRUPT_EVENT_SOFTWARE   (1U << TRACE_CPU_INTERRUPT_SOFTWARE)
#define TRACE_CPU_INTERRUPT_EVENT_RETURNS    (1U << TRACE_CPU_INTERRUPT_RETURN)
#define TRACE_CPU_INTERRUPT_EVENT_MODES      ((1U << TRACE_CPU_INTERRUPT_MODE) | (1U << TRACE_CPU_INTERRUPT_PAGING) | \
    (1U << TRACE_CPU_INTERRUPT_TASK) | (1U << TRACE_CPU_INTERRUPT_TABLES))
#define TRACE_CPU_INTERRUPT_EVENT_MSRS       (1U << TRACE_CPU_INTERRUPT_MSR)
#define TRACE_CPU_INTERRUPT_EVENT_DEFAULT    (TRACE_CPU_INTERRUPT_EVENT_IRQS | TRACE_CPU_INTERRUPT_EVENT_EXCEPTIONS | \
    TRACE_CPU_INTERRUPT_EVENT_MODES)
#define TRACE_CPU_INTERRUPT_EVENT_ALL        (TRACE_CPU_INTERRUPT_EVENT_DEFAULT | TRACE_CPU_INTERRUPT_EVENT_SOFTWARE | \
    TRACE_CPU_INTERRUPT_EVENT_RETURNS | TRACE_CPU_INTERRUPT_EVENT_MSRS)

#define TRACE_CPU_FLAG_FAILED                (1U << TRACE_ENTRY_TYPE_SHIFT)

enum GT_Trace_IO_Event : u8
{
    TRACE_IO_READ = 0,
    TRACE_IO_WRITE,
    TRACE_IO_POLL,
    TRACE_IO_DENIED
};

#define TRACE_IO_EVENT_READS  (1U << TRACE_IO_READ)
#define TRACE_IO_EVENT_WRITES (1U << TRACE_IO_WRITE)
#define TRACE_IO_EVENT_POLLS  (1U << TRACE_IO_POLL)
#define TRACE_IO_EVENT_DENIED (1U << TRACE_IO_DENIED)
#define TRACE_IO_EVENT_ALL    (TRACE_IO_EVENT_READS | TRACE_IO_EVENT_WRITES | TRACE_IO_EVENT_POLLS | \
    TRACE_IO_EVENT_DENIED)

#define TRACE_IO_FLAG_INS     (1U << TRACE_ENTRY_TYPE_SHIFT)

enum GT_Trace_PIC_Event : u8
{
    TRACE_PIC_REQUEST = 0,
    TRACE_PIC_MASK,
    TRACE_PIC_COMMAND,
    TRACE_PIC_INIT,
    TRACE_PIC_SPURIOUS,
    TRACE_PIC_LINE_LOW
};

#define TRACE_PIC_EVENT_REQUESTS (1U << TRACE_PIC_REQUEST)
#define TRACE_PIC_EVENT_MASK     (1U << TRACE_PIC_MASK)
#define TRACE_PIC_EVENT_COMMANDS (1U << TRACE_PIC_COMMAND)
#define TRACE_PIC_EVENT_INIT     (1U << TRACE_PIC_INIT)
#define TRACE_PIC_EVENT_SPURIOUS (1U << TRACE_PIC_SPURIOUS)
#define TRACE_PIC_EVENT_LINE_LOW (1U << TRACE_PIC_LINE_LOW)
#define TRACE_PIC_EVENT_DEFAULT  (TRACE_PIC_EVENT_REQUESTS | TRACE_PIC_EVENT_MASK | TRACE_PIC_EVENT_COMMANDS | \
    TRACE_PIC_EVENT_INIT | TRACE_PIC_EVENT_SPURIOUS)
#define TRACE_PIC_EVENT_ALL      (TRACE_PIC_EVENT_DEFAULT | TRACE_PIC_EVENT_LINE_LOW)

#define TRACE_PIC_FLAG_COALESCED  (1U << TRACE_ENTRY_TYPE_SHIFT)
#define TRACE_PIC_FLAG_IN_SERVICE (2U << TRACE_ENTRY_TYPE_SHIFT)

enum GT_Trace_Timer_Event : u8
{
    TRACE_TIMER_TIMEOUT = 0,
    TRACE_TIMER_CONTROL,
    TRACE_TIMER_COUNTER,
    TRACE_TIMER_INTERRUPT_CONTROL
};

#define TRACE_TIMER_EVENT_TIMEOUTS  (1U << TRACE_TIMER_TIMEOUT)
#define TRACE_TIMER_EVENT_COUNTERS  ((1U << TRACE_TIMER_CONTROL) | (1U << TRACE_TIMER_COUNTER))
#define TRACE_TIMER_EVENT_INTERRUPT (1U << TRACE_TIMER_INTERRUPT_CONTROL)
#define TRACE_TIMER_EVENT_ALL       (TRACE_TIMER_EVENT_TIMEOUTS | TRACE_TIMER_EVENT_COUNTERS | \
    TRACE_TIMER_EVENT_INTERRUPT)

enum GT_Trace_DMA_Event : u8
{
    TRACE_DMA_REGISTER = 0,
    TRACE_DMA_REQUEST,
    TRACE_DMA_END
};

#define TRACE_DMA_EVENT_REGISTERS (1U << TRACE_DMA_REGISTER)
#define TRACE_DMA_EVENT_REQUESTS  (1U << TRACE_DMA_REQUEST)
#define TRACE_DMA_EVENT_ENDS      (1U << TRACE_DMA_END)
#define TRACE_DMA_EVENT_ALL       (TRACE_DMA_EVENT_REGISTERS | TRACE_DMA_EVENT_REQUESTS | TRACE_DMA_EVENT_ENDS)

enum GT_Trace_Video_Event : u8
{
    TRACE_VIDEO_CRTC = 0,
    TRACE_VIDEO_OUTPUT,
    TRACE_VIDEO_DISPLAY,
    TRACE_VIDEO_PALETTE,
    TRACE_VIDEO_DIGITAL_PALETTE,
    TRACE_VIDEO_MASK,
    TRACE_VIDEO_VSYNC,
    TRACE_VIDEO_VSYNC_CLEAR,
    TRACE_VIDEO_FMR,
    TRACE_VIDEO_MISSED_VBLANK,
    TRACE_VIDEO_HIRES
};

#define TRACE_VIDEO_EVENT_CRTC          (1U << TRACE_VIDEO_CRTC)
#define TRACE_VIDEO_EVENT_OUTPUT        ((1U << TRACE_VIDEO_OUTPUT) | (1U << TRACE_VIDEO_DISPLAY))
#define TRACE_VIDEO_EVENT_PALETTE       ((1U << TRACE_VIDEO_PALETTE) | (1U << TRACE_VIDEO_DIGITAL_PALETTE))
#define TRACE_VIDEO_EVENT_MASK          (1U << TRACE_VIDEO_MASK)
#define TRACE_VIDEO_EVENT_VSYNC         ((1U << TRACE_VIDEO_VSYNC) | (1U << TRACE_VIDEO_VSYNC_CLEAR))
#define TRACE_VIDEO_EVENT_FMR           (1U << TRACE_VIDEO_FMR)
#define TRACE_VIDEO_EVENT_MISSED_VBLANK (1U << TRACE_VIDEO_MISSED_VBLANK)
#define TRACE_VIDEO_EVENT_HIRES         (1U << TRACE_VIDEO_HIRES)
#define TRACE_VIDEO_EVENT_DEFAULT       (TRACE_VIDEO_EVENT_CRTC | TRACE_VIDEO_EVENT_OUTPUT | \
    TRACE_VIDEO_EVENT_PALETTE | TRACE_VIDEO_EVENT_MASK | TRACE_VIDEO_EVENT_VSYNC | TRACE_VIDEO_EVENT_FMR | \
    TRACE_VIDEO_EVENT_HIRES)
#define TRACE_VIDEO_EVENT_ALL           (TRACE_VIDEO_EVENT_DEFAULT | TRACE_VIDEO_EVENT_MISSED_VBLANK)

enum GT_Trace_Sprite_Event : u8
{
    TRACE_SPRITE_REGISTER = 0,
    TRACE_SPRITE_TRANSFER_START,
    TRACE_SPRITE_TRANSFER_END,
    TRACE_SPRITE_BUSY_AT_VSYNC
};

#define TRACE_SPRITE_EVENT_REGISTERS (1U << TRACE_SPRITE_REGISTER)
#define TRACE_SPRITE_EVENT_TRANSFERS ((1U << TRACE_SPRITE_TRANSFER_START) | (1U << TRACE_SPRITE_TRANSFER_END))
#define TRACE_SPRITE_EVENT_BUSY      (1U << TRACE_SPRITE_BUSY_AT_VSYNC)
#define TRACE_SPRITE_EVENT_ALL       (TRACE_SPRITE_EVENT_REGISTERS | TRACE_SPRITE_EVENT_TRANSFERS | \
    TRACE_SPRITE_EVENT_BUSY)

enum GT_Trace_FM_Event : u8
{
    TRACE_FM_KEY = 0,
    TRACE_FM_FREQUENCY,
    TRACE_FM_OPERATOR,
    TRACE_FM_CHANNEL,
    TRACE_FM_GLOBAL,
    TRACE_FM_DAC,
    TRACE_FM_TIMER,
    TRACE_FM_IRQ
};

#define TRACE_FM_EVENT_KEY       (1U << TRACE_FM_KEY)
#define TRACE_FM_EVENT_FREQUENCY (1U << TRACE_FM_FREQUENCY)
#define TRACE_FM_EVENT_OPERATORS (1U << TRACE_FM_OPERATOR)
#define TRACE_FM_EVENT_CHANNELS  (1U << TRACE_FM_CHANNEL)
#define TRACE_FM_EVENT_GLOBAL    (1U << TRACE_FM_GLOBAL)
#define TRACE_FM_EVENT_DAC       (1U << TRACE_FM_DAC)
#define TRACE_FM_EVENT_TIMERS    (1U << TRACE_FM_TIMER)
#define TRACE_FM_EVENT_IRQS      (1U << TRACE_FM_IRQ)
#define TRACE_FM_EVENT_DEFAULT   (TRACE_FM_EVENT_KEY | TRACE_FM_EVENT_FREQUENCY | TRACE_FM_EVENT_OPERATORS | \
    TRACE_FM_EVENT_CHANNELS | TRACE_FM_EVENT_GLOBAL | TRACE_FM_EVENT_TIMERS | TRACE_FM_EVENT_IRQS)
#define TRACE_FM_EVENT_ALL       (TRACE_FM_EVENT_DEFAULT | TRACE_FM_EVENT_DAC)

enum GT_Trace_PCM_Event : u8
{
    TRACE_PCM_CHANNEL = 0,
    TRACE_PCM_KEY,
    TRACE_PCM_CONTROL,
    TRACE_PCM_IRQ_MASK,
    TRACE_PCM_IRQ,
    TRACE_PCM_IRQ_READ
};

#define TRACE_PCM_EVENT_CHANNELS (1U << TRACE_PCM_CHANNEL)
#define TRACE_PCM_EVENT_KEY      (1U << TRACE_PCM_KEY)
#define TRACE_PCM_EVENT_CONTROL  (1U << TRACE_PCM_CONTROL)
#define TRACE_PCM_EVENT_IRQS     ((1U << TRACE_PCM_IRQ_MASK) | (1U << TRACE_PCM_IRQ) | (1U << TRACE_PCM_IRQ_READ))
#define TRACE_PCM_EVENT_ALL      (TRACE_PCM_EVENT_CHANNELS | TRACE_PCM_EVENT_KEY | TRACE_PCM_EVENT_CONTROL | \
    TRACE_PCM_EVENT_IRQS)

enum GT_Trace_PCM16_Event : u8
{
    TRACE_PCM16_REGISTER = 0,
    TRACE_PCM16_DATA,
    TRACE_PCM16_DMA_END,
    TRACE_PCM16_BUFFER_ERROR,
    TRACE_PCM16_IRQ
};

#define TRACE_PCM16_EVENT_REGISTERS (1U << TRACE_PCM16_REGISTER)
#define TRACE_PCM16_EVENT_DATA      (1U << TRACE_PCM16_DATA)
#define TRACE_PCM16_EVENT_DMA       (1U << TRACE_PCM16_DMA_END)
#define TRACE_PCM16_EVENT_BUFFER    (1U << TRACE_PCM16_BUFFER_ERROR)
#define TRACE_PCM16_EVENT_IRQS      (1U << TRACE_PCM16_IRQ)
#define TRACE_PCM16_EVENT_DEFAULT   (TRACE_PCM16_EVENT_REGISTERS | TRACE_PCM16_EVENT_DMA | TRACE_PCM16_EVENT_BUFFER | \
    TRACE_PCM16_EVENT_IRQS)
#define TRACE_PCM16_EVENT_ALL       (TRACE_PCM16_EVENT_DEFAULT | TRACE_PCM16_EVENT_DATA)

enum GT_Trace_Mixer_Event : u8
{
    TRACE_MIXER_VOLUME = 0,
    TRACE_MIXER_MUTE
};

#define TRACE_MIXER_EVENT_VOLUME (1U << TRACE_MIXER_VOLUME)
#define TRACE_MIXER_EVENT_MUTE   (1U << TRACE_MIXER_MUTE)
#define TRACE_MIXER_EVENT_ALL    (TRACE_MIXER_EVENT_VOLUME | TRACE_MIXER_EVENT_MUTE)

enum GT_Trace_CDROM_Event : u8
{
    TRACE_CDROM_COMMAND = 0,
    TRACE_CDROM_STATUS,
    TRACE_CDROM_IRQ,
    TRACE_CDROM_CONTROL,
    TRACE_CDROM_SECTOR_READY,
    TRACE_CDROM_TRANSFER,
    TRACE_CDROM_SECTOR_END,
    TRACE_CDROM_LOST_DATA,
    TRACE_CDROM_CDDA_PLAY,
    TRACE_CDROM_CDDA_PAUSE,
    TRACE_CDROM_CDDA_RESUME,
    TRACE_CDROM_CDDA_STOP,
    TRACE_CDROM_CDDA_END,
    TRACE_CDROM_CDDA_LOOP,
    TRACE_CDROM_CDDA_REJECT
};

enum GT_Trace_CDDA_Stop : u8
{
    TRACE_CDDA_STOP_COMMAND = 0,
    TRACE_CDDA_STOP_INSERT,
    TRACE_CDDA_STOP_EJECT
};

#define TRACE_CDROM_EVENT_COMMANDS (1U << TRACE_CDROM_COMMAND)
#define TRACE_CDROM_EVENT_STATUS   (1U << TRACE_CDROM_STATUS)
#define TRACE_CDROM_EVENT_IRQS     (1U << TRACE_CDROM_IRQ)
#define TRACE_CDROM_EVENT_CONTROL  (1U << TRACE_CDROM_CONTROL)
#define TRACE_CDROM_EVENT_DATA     ((1U << TRACE_CDROM_SECTOR_READY) | (1U << TRACE_CDROM_TRANSFER) | \
    (1U << TRACE_CDROM_SECTOR_END) | (1U << TRACE_CDROM_LOST_DATA))
#define TRACE_CDROM_EVENT_CDDA     ((1U << TRACE_CDROM_CDDA_PLAY) | (1U << TRACE_CDROM_CDDA_PAUSE) | \
    (1U << TRACE_CDROM_CDDA_RESUME) | (1U << TRACE_CDROM_CDDA_STOP) | (1U << TRACE_CDROM_CDDA_END) | \
    (1U << TRACE_CDROM_CDDA_LOOP) | (1U << TRACE_CDROM_CDDA_REJECT))
#define TRACE_CDROM_EVENT_ALL      (TRACE_CDROM_EVENT_COMMANDS | TRACE_CDROM_EVENT_STATUS | TRACE_CDROM_EVENT_IRQS | \
    TRACE_CDROM_EVENT_CONTROL | TRACE_CDROM_EVENT_DATA | TRACE_CDROM_EVENT_CDDA)

enum GT_Trace_FDC_Event : u8
{
    TRACE_FDC_COMMAND = 0,
    TRACE_FDC_END,
    TRACE_FDC_DRIVE_CONTROL,
    TRACE_FDC_DRIVE_SELECT
};

#define TRACE_FDC_EVENT_COMMANDS (1U << TRACE_FDC_COMMAND)
#define TRACE_FDC_EVENT_RESULTS  (1U << TRACE_FDC_END)
#define TRACE_FDC_EVENT_DRIVES   ((1U << TRACE_FDC_DRIVE_CONTROL) | (1U << TRACE_FDC_DRIVE_SELECT))
#define TRACE_FDC_EVENT_ALL      (TRACE_FDC_EVENT_COMMANDS | TRACE_FDC_EVENT_RESULTS | TRACE_FDC_EVENT_DRIVES)

enum GT_Trace_SCSI_Event : u8
{
    TRACE_SCSI_CONTROL = 0,
    TRACE_SCSI_RESET,
    TRACE_SCSI_SELECTION,
    TRACE_SCSI_COMMAND,
    TRACE_SCSI_PHASE,
    TRACE_SCSI_STATUS,
    TRACE_SCSI_MESSAGE,
    TRACE_SCSI_DATA_START,
    TRACE_SCSI_DATA_END,
    TRACE_SCSI_ERROR
};

#define TRACE_SCSI_EVENT_CONTROL   (1U << TRACE_SCSI_CONTROL)
#define TRACE_SCSI_EVENT_RESET     (1U << TRACE_SCSI_RESET)
#define TRACE_SCSI_EVENT_SELECTION (1U << TRACE_SCSI_SELECTION)
#define TRACE_SCSI_EVENT_COMMANDS  (1U << TRACE_SCSI_COMMAND)
#define TRACE_SCSI_EVENT_PHASES    (1U << TRACE_SCSI_PHASE)
#define TRACE_SCSI_EVENT_STATUS    ((1U << TRACE_SCSI_STATUS) | (1U << TRACE_SCSI_MESSAGE))
#define TRACE_SCSI_EVENT_DATA      ((1U << TRACE_SCSI_DATA_START) | (1U << TRACE_SCSI_DATA_END))
#define TRACE_SCSI_EVENT_ERRORS    (1U << TRACE_SCSI_ERROR)
#define TRACE_SCSI_EVENT_DEFAULT   (TRACE_SCSI_EVENT_RESET | TRACE_SCSI_EVENT_SELECTION | TRACE_SCSI_EVENT_COMMANDS | \
    TRACE_SCSI_EVENT_STATUS | TRACE_SCSI_EVENT_DATA | TRACE_SCSI_EVENT_ERRORS)
#define TRACE_SCSI_EVENT_ALL       (TRACE_SCSI_EVENT_DEFAULT | TRACE_SCSI_EVENT_CONTROL | TRACE_SCSI_EVENT_PHASES)

enum GT_Trace_Keyboard_Event : u8
{
    TRACE_KEYBOARD_KEY = 0,
    TRACE_KEYBOARD_READ,
    TRACE_KEYBOARD_COMMAND,
    TRACE_KEYBOARD_IRQ_ENABLE
};

#define TRACE_KEYBOARD_EVENT_KEYS     (1U << TRACE_KEYBOARD_KEY)
#define TRACE_KEYBOARD_EVENT_READS    (1U << TRACE_KEYBOARD_READ)
#define TRACE_KEYBOARD_EVENT_COMMANDS ((1U << TRACE_KEYBOARD_COMMAND) | (1U << TRACE_KEYBOARD_IRQ_ENABLE))
#define TRACE_KEYBOARD_EVENT_ALL      (TRACE_KEYBOARD_EVENT_KEYS | TRACE_KEYBOARD_EVENT_READS | \
    TRACE_KEYBOARD_EVENT_COMMANDS)

enum GT_Trace_Input_Event : u8
{
    TRACE_INPUT_READ = 0,
    TRACE_INPUT_WRITE,
    TRACE_INPUT_CHANGE
};

#define TRACE_INPUT_EVENT_READS   (1U << TRACE_INPUT_READ)
#define TRACE_INPUT_EVENT_WRITES  (1U << TRACE_INPUT_WRITE)
#define TRACE_INPUT_EVENT_CHANGES (1U << TRACE_INPUT_CHANGE)
#define TRACE_INPUT_EVENT_DEFAULT (TRACE_INPUT_EVENT_WRITES | TRACE_INPUT_EVENT_CHANGES)
#define TRACE_INPUT_EVENT_ALL     (TRACE_INPUT_EVENT_DEFAULT | TRACE_INPUT_EVENT_READS)

enum GT_Trace_System_Event : u8
{
    TRACE_SYSTEM_RESET = 0,
    TRACE_SYSTEM_MEMORY_MAP,
    TRACE_SYSTEM_RTC_READ,
    TRACE_SYSTEM_RTC_WRITE,
    TRACE_SYSTEM_MEMORY_CARD
};

#define TRACE_SYSTEM_EVENT_RESET       (1U << TRACE_SYSTEM_RESET)
#define TRACE_SYSTEM_EVENT_MEMORY      (1U << TRACE_SYSTEM_MEMORY_MAP)
#define TRACE_SYSTEM_EVENT_RTC         ((1U << TRACE_SYSTEM_RTC_READ) | (1U << TRACE_SYSTEM_RTC_WRITE))
#define TRACE_SYSTEM_EVENT_MEMORY_CARD (1U << TRACE_SYSTEM_MEMORY_CARD)
#define TRACE_SYSTEM_EVENT_DEFAULT     (TRACE_SYSTEM_EVENT_RESET | TRACE_SYSTEM_EVENT_MEMORY | \
    TRACE_SYSTEM_EVENT_MEMORY_CARD)
#define TRACE_SYSTEM_EVENT_ALL         (TRACE_SYSTEM_EVENT_DEFAULT | TRACE_SYSTEM_EVENT_RTC)

static_assert(TRACE_VIDEO_HIRES < 32 && TRACE_CDROM_CDDA_REJECT < 32, "Trace event filters exceed 32 bits");

struct GT_Trace_Entry
{
    u64 cycle;
    u32 pc;
    GT_Trace_Type type;
    u8 event;
    u16 flags;
    union
    {
        struct
        {
            u32 linear;
            u32 eip;
            u32 registers[8];
            u32 eflags;
            u16 cs;
            u16 ds;
            u16 es;
            u16 ss;
            u16 fs;
            u16 gs;
            u8 mode;
            u8 cpl;
            u8 default32;
            u8 size;
            u8 opcodes[GT_I386_MAX_INSTRUCTION_LENGTH];
        } cpu;

        struct
        {
            u32 from;
            u32 to;
            u32 error_code;
            u32 eflags;
            u32 from_eip;
            u32 to_eip;
            u16 ax;
            u16 bx;
            u16 cx;
            u16 dx;
            u16 si;
            u16 di;
            u16 ds;
            u16 es;
            u16 from_cs;
            u16 to_cs;
            u8 vector;
            u8 line;
            u8 has_error_code;
            u8 modes;
            u8 cpls;
            char text[GT_TRACE_INTERRUPT_TEXT_SIZE];
        } interrupt;

        struct
        {
            u32 value;
            u32 count;
            u32 cycles;
            u16 port;
            u8 size;
            u8 reason;
        } io;

        struct
        {
            u32 old_value;
            u32 new_value;
            u32 extra;
            u32 extra2;
            u32 eip;
            u32 to_eip;
            u16 cs;
            u16 to_cs;
            u16 selector;
            u8 cause;
            u8 vector;
            u8 vector2;
            u8 modes;
            u8 has_error_code;
        } cpu_event;

        struct
        {
            u64 value;
            u32 detail;
            u8 cause;
        } mark;

        struct
        {
            u8 chip;
            u8 line;
            u8 vector;
            u8 value;
            u8 previous;
            u8 irr;
            u8 isr;
            u8 imr;
            u8 step;
            u8 previous_isr;
        } pic;

        struct
        {
            u16 reload;
            u8 chip;
            u8 counter;
            u8 value;
            u8 access;
            u8 mode;
            u8 bcd;
            u8 complete;
            u8 latch;
            u8 enable;
            u8 sound;
        } timer;

        struct
        {
            u32 address;
            u16 count;
            u8 channel;
            u8 mode;
            u8 reg;
            u8 value;
            u8 terminal;
            u8 mask;
        } dma;

        struct
        {
            u32 param;
            u16 value;
            u16 line;
            u16 dot;
            u8 reg;
            u8 raw;
            u8 bank;
        } video;

        struct
        {
            u32 clocks;
            u16 first;
            u16 count;
            u16 entry;
            u8 reg;
            u8 value;
            u8 raw;
            u8 page;
        } sprite;

        struct
        {
            u16 address;
            u16 frequency;
            u8 value;
            u8 channel;
            u8 flags;
        } fm;

        struct
        {
            u8 reg;
            u8 value;
            u8 channel;
            u8 enabled;
            u8 flags;
            u8 mask;
        } pcm;

        struct
        {
            u32 address;
            u16 count;
            u16 port;
            u16 data;
            u8 value;
            u8 causes;
            u8 line;
            u8 fifo;
            u8 read;
            u8 autoinit;
        } pcm16;

        struct
        {
            u16 port;
            u8 value;
            u8 chip;
            u8 channel;
            u8 data;
            u8 control;
        } mixer;

        struct
        {
            u32 lba;
            u32 end_lba;
            u16 size;
            u8 command;
            u8 bytes[8];
            u8 value;
            u8 flags;
        } cdrom;

        struct
        {
            u8 command;
            u8 track;
            u8 sector;
            u8 data;
            u8 status;
            u8 value;
            s8 drive;
            u8 cylinder;
        } fdc;

        struct
        {
            u32 lba;
            u32 count;
            u8 command[12];
            u8 control;
            u8 status;
            u8 value;
            u8 phase;
            s8 target;
            u8 key;
            u8 code;
        } scsi;

        struct
        {
            u8 value;
            u8 flags;
            u8 key;
            u8 pending;
        } keyboard;

        struct
        {
            u16 buttons;
            u16 previous;
            u8 port;
            u8 value;
            u8 output;
            u8 device;
        } input;

        struct
        {
            u16 port;
            u8 value;
            u8 address;
            u8 flags;
        } system;

        u8 raw[GT_TRACE_ENTRY_SIZE - GT_TRACE_HEADER_SIZE];
    };
};

static_assert(sizeof(GT_Trace_Entry) == GT_TRACE_ENTRY_SIZE, "GT_Trace_Entry must fill one slot");

struct GT_Trace_Range
{
    u32 first;
    u32 last;
    u32 events;
};

struct GT_Trace_Function
{
    u8 vector;
    u8 match;
    u16 value;
};

enum GT_Trace_Function_Match : u8
{
    TRACE_FUNCTION_ANY = 0,
    TRACE_FUNCTION_AH,
    TRACE_FUNCTION_AX
};

struct GT_Trace_Filters
{
    GT_Trace_Range code_include[GT_TRACE_FILTER_RANGES];
    GT_Trace_Range code_exclude[GT_TRACE_FILTER_RANGES];
    GT_Trace_Range port_include[GT_TRACE_FILTER_RANGES];
    GT_Trace_Range port_exclude[GT_TRACE_FILTER_RANGES];
    GT_Trace_Function functions[GT_TRACE_FILTER_FUNCTIONS];
    u32 vectors[8];
    u8 code_include_count;
    u8 code_exclude_count;
    u8 port_include_count;
    u8 port_exclude_count;
    u8 function_count;
};

class TraceLogger
{
public:
    TraceLogger();
    ~TraceLogger();
    static TraceLogger* GetDisabled();
    static TraceLogger* Resolve(TraceLogger* trace_logger);
    void Init(const u64* clocks);
    void SetPCSource(const u32* base, const u32* offset);
    void Reset();
    bool SetCapacity(u32 capacity);
    void SetActive(bool active);
    INLINE bool IsEnabled(GT_Trace_Type type) const;
    INLINE bool IsEventEnabled(GT_Trace_Type type, u8 event) const;
    INLINE bool IsRecording() const;
    INLINE bool IsCodeTraced(u32 linear) const;
    INLINE bool IsPortTraced(u16 port, u8 event) const;
    INLINE bool IsSoftwareInterruptTraced(u8 vector, u16 ax) const;
    void SetFilters(const GT_Trace_Filters& filters);
    const GT_Trace_Filters& GetFilters() const;
    static void ClearFilters(GT_Trace_Filters& filters);
    INLINE GT_Trace_Entry* Begin(GT_Trace_Type type, u8 event);
    INLINE GT_Trace_Entry* Begin(GT_Trace_Type type, u8 event, u64 clocks);
    INLINE GT_Trace_Entry* BeginRaw(GT_Trace_Type type, u8 event, u64 clocks);
    INLINE void TraceLog(const GT_Trace_Entry& entry);
    INLINE void SetDispatching(bool dispatching);
    INLINE u64 GetClocks() const;
    void Mark(u8 event, u64 value, u32 detail = 0, u8 cause = 0);
    void SetEnabledFlags(u32 flags);
    void SetEventFilter(GT_Trace_Type type, u32 filter);
    u32 GetEnabledFlags() const;
    u32 GetEventFilter(GT_Trace_Type type) const;
    u32 GetCount() const;
    u32 GetCapacity() const;
    u64 GetTotalLogged() const;
    u64 GetSequence() const;
    u64 GetOverwritten() const;
    const GT_Trace_Entry& GetEntry(u32 index) const;

private:
    void UpdateEnabled();
    bool MatchesCode(u32 linear) const;
    bool MatchesPort(u16 port, u8 event) const;
    bool MatchesFunction(u8 vector, u16 ax) const;

private:
    GT_Trace_Entry* m_buffer;
    const u64* m_clocks;
    const u32* m_pc_base;
    const u32* m_pc_offset;
    u32 m_position;
    u32 m_count;
    u32 m_capacity;
    u32 m_enabled_flags;
    u32 m_active_flags;
    bool m_active;
    bool m_dispatching;
    u32 m_event_filters[TRACE_TYPE_COUNT];
    u64 m_total_logged;
    u64 m_sequence;
    GT_Trace_Filters m_filters;
    bool m_code_filtered;
    bool m_port_filtered;
    bool m_vector_filtered;
};

// Events are only recorded while the debugger runs the machine
INLINE bool TraceLogger::IsEnabled(GT_Trace_Type type) const
{
#if !defined(GT_DISABLE_DISASSEMBLER)
    return IS_SET_BIT(m_active_flags, type);
#else
    UNUSED(type);
    return false;
#endif
}

INLINE bool TraceLogger::IsEventEnabled(GT_Trace_Type type, u8 event) const
{
#if !defined(GT_DISABLE_DISASSEMBLER)
    return IS_SET_BIT(m_active_flags, type) && IS_SET_BIT(m_event_filters[type], event);
#else
    UNUSED(type);
    UNUSED(event);
    return false;
#endif
}

INLINE bool TraceLogger::IsRecording() const
{
#if !defined(GT_DISABLE_DISASSEMBLER)
    return m_active_flags != 0;
#else
    return false;
#endif
}

INLINE bool TraceLogger::IsCodeTraced(u32 linear) const
{
    return !m_code_filtered || MatchesCode(linear);
}

INLINE bool TraceLogger::IsPortTraced(u16 port, u8 event) const
{
    return !m_port_filtered || MatchesPort(port, event);
}

INLINE bool TraceLogger::IsSoftwareInterruptTraced(u8 vector, u16 ax) const
{
    return !m_vector_filtered || MatchesFunction(vector, ax);
}

INLINE GT_Trace_Entry* TraceLogger::Begin(GT_Trace_Type type, u8 event)
{
    return Begin(type, event, *m_clocks);
}

INLINE GT_Trace_Entry* TraceLogger::Begin(GT_Trace_Type type, u8 event, u64 clocks)
{
    GT_Trace_Entry* entry = BeginRaw(type, event, clocks);
    memset((u8*)entry + GT_TRACE_HEADER_SIZE, 0, GT_TRACE_ENTRY_SIZE - GT_TRACE_HEADER_SIZE);
    return entry;
}

INLINE GT_Trace_Entry* TraceLogger::BeginRaw(GT_Trace_Type type, u8 event, u64 clocks)
{
#if !defined(GT_DISABLE_DISASSEMBLER)
    GT_Trace_Entry* entry = &m_buffer[m_position];
    m_position = m_position + 1 == m_capacity ? 0 : m_position + 1;

    if (m_count < m_capacity)
        m_count++;

    m_total_logged++;
    m_sequence++;

    entry->cycle = clocks;
    entry->pc = *m_pc_base + *m_pc_offset;
    entry->type = type;
    entry->event = event;
    entry->flags = m_dispatching ? TRACE_ENTRY_NEXT_PC : 0;
    return entry;
#else
    static GT_Trace_Entry k_scratch;
    UNUSED(type);
    UNUSED(event);
    UNUSED(clocks);
    return &k_scratch;
#endif
}

INLINE void TraceLogger::TraceLog(const GT_Trace_Entry& entry)
{
    GT_Trace_Entry* slot = BeginRaw(entry.type, entry.event, *m_clocks);
    slot->flags |= entry.flags;
    memcpy((u8*)slot + GT_TRACE_HEADER_SIZE, (const u8*)&entry + GT_TRACE_HEADER_SIZE,
        GT_TRACE_ENTRY_SIZE - GT_TRACE_HEADER_SIZE);
}

INLINE void TraceLogger::SetDispatching(bool dispatching)
{
    m_dispatching = dispatching;
}

INLINE u64 TraceLogger::GetClocks() const
{
    return *m_clocks;
}

#endif /* TRACE_LOGGER_H */
