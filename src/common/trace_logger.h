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

#define TRACE_BUFFER_SIZE 100000
#define GT_TRACE_NAME_SIZE 56

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
    TRACE_MIXER,
    TRACE_CDROM,
    TRACE_FDC,
    TRACE_KEYBOARD,
    TRACE_INPUT,
    TRACE_SYSTEM,
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
#define TRACE_FLAG_MIXER            (1U << TRACE_MIXER)
#define TRACE_FLAG_CDROM            (1U << TRACE_CDROM)
#define TRACE_FLAG_FDC              (1U << TRACE_FDC)
#define TRACE_FLAG_KEYBOARD         (1U << TRACE_KEYBOARD)
#define TRACE_FLAG_INPUT            (1U << TRACE_INPUT)
#define TRACE_FLAG_SYSTEM           (1U << TRACE_SYSTEM)

enum GT_Trace_CPU_Interrupt_Event : u8
{
    TRACE_CPU_INTERRUPT_HARDWARE = 0,
    TRACE_CPU_INTERRUPT_EXCEPTION,
    TRACE_CPU_INTERRUPT_SOFTWARE
};

#define TRACE_CPU_INTERRUPT_EVENT_IRQS       (1U << TRACE_CPU_INTERRUPT_HARDWARE)
#define TRACE_CPU_INTERRUPT_EVENT_EXCEPTIONS (1U << TRACE_CPU_INTERRUPT_EXCEPTION)
#define TRACE_CPU_INTERRUPT_EVENT_SOFTWARE   (1U << TRACE_CPU_INTERRUPT_SOFTWARE)
#define TRACE_CPU_INTERRUPT_EVENT_DEFAULT    (TRACE_CPU_INTERRUPT_EVENT_IRQS | TRACE_CPU_INTERRUPT_EVENT_EXCEPTIONS)
#define TRACE_CPU_INTERRUPT_EVENT_ALL        (TRACE_CPU_INTERRUPT_EVENT_DEFAULT | TRACE_CPU_INTERRUPT_EVENT_SOFTWARE)

enum GT_Trace_IO_Event : u8
{
    TRACE_IO_READ = 0,
    TRACE_IO_WRITE
};

#define TRACE_IO_EVENT_READS  (1U << TRACE_IO_READ)
#define TRACE_IO_EVENT_WRITES (1U << TRACE_IO_WRITE)
#define TRACE_IO_EVENT_ALL    (TRACE_IO_EVENT_READS | TRACE_IO_EVENT_WRITES)

enum GT_Trace_PIC_Event : u8
{
    TRACE_PIC_REQUEST = 0,
    TRACE_PIC_MASK,
    TRACE_PIC_COMMAND,
    TRACE_PIC_INIT
};

#define TRACE_PIC_EVENT_REQUESTS (1U << TRACE_PIC_REQUEST)
#define TRACE_PIC_EVENT_MASK     (1U << TRACE_PIC_MASK)
#define TRACE_PIC_EVENT_COMMANDS (1U << TRACE_PIC_COMMAND)
#define TRACE_PIC_EVENT_INIT     (1U << TRACE_PIC_INIT)
#define TRACE_PIC_EVENT_ALL      (TRACE_PIC_EVENT_REQUESTS | TRACE_PIC_EVENT_MASK | TRACE_PIC_EVENT_COMMANDS | \
    TRACE_PIC_EVENT_INIT)

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
    TRACE_VIDEO_MISSED_VBLANK
};

#define TRACE_VIDEO_EVENT_CRTC          (1U << TRACE_VIDEO_CRTC)
#define TRACE_VIDEO_EVENT_OUTPUT        ((1U << TRACE_VIDEO_OUTPUT) | (1U << TRACE_VIDEO_DISPLAY))
#define TRACE_VIDEO_EVENT_PALETTE       ((1U << TRACE_VIDEO_PALETTE) | (1U << TRACE_VIDEO_DIGITAL_PALETTE))
#define TRACE_VIDEO_EVENT_MASK          (1U << TRACE_VIDEO_MASK)
#define TRACE_VIDEO_EVENT_VSYNC         ((1U << TRACE_VIDEO_VSYNC) | (1U << TRACE_VIDEO_VSYNC_CLEAR))
#define TRACE_VIDEO_EVENT_FMR           (1U << TRACE_VIDEO_FMR)
#define TRACE_VIDEO_EVENT_MISSED_VBLANK (1U << TRACE_VIDEO_MISSED_VBLANK)
#define TRACE_VIDEO_EVENT_DEFAULT       (TRACE_VIDEO_EVENT_CRTC | TRACE_VIDEO_EVENT_OUTPUT | \
    TRACE_VIDEO_EVENT_PALETTE | TRACE_VIDEO_EVENT_MASK | TRACE_VIDEO_EVENT_VSYNC | TRACE_VIDEO_EVENT_FMR)
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
    TRACE_CDROM_CDDA_LOOP
};

#define TRACE_CDROM_EVENT_COMMANDS (1U << TRACE_CDROM_COMMAND)
#define TRACE_CDROM_EVENT_STATUS   (1U << TRACE_CDROM_STATUS)
#define TRACE_CDROM_EVENT_IRQS     (1U << TRACE_CDROM_IRQ)
#define TRACE_CDROM_EVENT_CONTROL  (1U << TRACE_CDROM_CONTROL)
#define TRACE_CDROM_EVENT_DATA     ((1U << TRACE_CDROM_SECTOR_READY) | (1U << TRACE_CDROM_TRANSFER) | \
    (1U << TRACE_CDROM_SECTOR_END) | (1U << TRACE_CDROM_LOST_DATA))
#define TRACE_CDROM_EVENT_CDDA     ((1U << TRACE_CDROM_CDDA_PLAY) | (1U << TRACE_CDROM_CDDA_PAUSE) | \
    (1U << TRACE_CDROM_CDDA_RESUME) | (1U << TRACE_CDROM_CDDA_STOP) | (1U << TRACE_CDROM_CDDA_END) | \
    (1U << TRACE_CDROM_CDDA_LOOP))
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
    TRACE_SYSTEM_RTC_WRITE
};

#define TRACE_SYSTEM_EVENT_RESET  (1U << TRACE_SYSTEM_RESET)
#define TRACE_SYSTEM_EVENT_MEMORY (1U << TRACE_SYSTEM_MEMORY_MAP)
#define TRACE_SYSTEM_EVENT_RTC    ((1U << TRACE_SYSTEM_RTC_READ) | (1U << TRACE_SYSTEM_RTC_WRITE))
#define TRACE_SYSTEM_EVENT_DEFAULT (TRACE_SYSTEM_EVENT_RESET | TRACE_SYSTEM_EVENT_MEMORY)
#define TRACE_SYSTEM_EVENT_ALL    (TRACE_SYSTEM_EVENT_DEFAULT | TRACE_SYSTEM_EVENT_RTC)

static_assert(TRACE_VIDEO_MISSED_VBLANK < 32 && TRACE_CDROM_CDDA_LOOP < 32, "Trace event filters exceed 32 bits");

struct GT_Trace_Entry
{
    u64 cycle;
    GT_Trace_Type type;
    u8 event;
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
            u8 size;
            u8 opcodes[GT_I386_MAX_INSTRUCTION_LENGTH];
            char name[GT_TRACE_NAME_SIZE];
        } cpu;

        struct
        {
            u32 from;
            u32 to;
            u32 error_code;
            u16 ax;
            u8 vector;
            u8 line;
            u8 has_error_code;
        } interrupt;

        struct
        {
            u32 pc;
            u32 value;
            u16 port;
            u8 size;
        } io;

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
    };
};

static_assert(sizeof(GT_Trace_Entry) <= 144, "GT_Trace_Entry exceeds its memory budget");

class TraceLogger
{
public:
    TraceLogger();
    ~TraceLogger();
    void Init(const u64* clocks);
    void Reset();
    bool SetCapacity(u32 capacity);
    void SetActive(bool active);
    INLINE bool IsEnabled(GT_Trace_Type type) const;
    INLINE bool IsEventEnabled(GT_Trace_Type type, u8 event) const;
    INLINE void TraceLog(const GT_Trace_Entry& entry);
    void SetEnabledFlags(u32 flags);
    void SetEventFilter(GT_Trace_Type type, u32 filter);
    u32 GetEnabledFlags() const;
    u32 GetEventFilter(GT_Trace_Type type) const;
    u32 GetCount() const;
    u32 GetCapacity() const;
    u64 GetTotalLogged() const;
    u64 GetSequence() const;
    const GT_Trace_Entry& GetEntry(u32 index) const;

private:
    void UpdateEnabled();

private:
    GT_Trace_Entry* m_buffer;
    const u64* m_clocks;
    u32 m_position;
    u32 m_count;
    u32 m_capacity;
    u32 m_enabled_flags;
    u32 m_active_flags;
    bool m_active;
    u32 m_event_filters[TRACE_TYPE_COUNT];
    u64 m_total_logged;
    u64 m_sequence;
};

// Events are only recorded while the debugger runs the machine
INLINE bool TraceLogger::IsEnabled(GT_Trace_Type type) const
{
#if !defined(GT_DISABLE_DISASSEMBLER)
    return (m_active_flags & (1U << type)) != 0;
#else
    UNUSED(type);
    return false;
#endif
}

INLINE bool TraceLogger::IsEventEnabled(GT_Trace_Type type, u8 event) const
{
#if !defined(GT_DISABLE_DISASSEMBLER)
    return (m_active_flags & (1U << type)) != 0 && (m_event_filters[type] & (1U << event)) != 0;
#else
    UNUSED(type);
    UNUSED(event);
    return false;
#endif
}

INLINE void TraceLogger::TraceLog(const GT_Trace_Entry& entry)
{
#if !defined(GT_DISABLE_DISASSEMBLER)
    m_buffer[m_position] = entry;
    m_buffer[m_position].cycle = *m_clocks;
    m_position = m_position + 1 == m_capacity ? 0 : m_position + 1;

    if (m_count < m_capacity)
        m_count++;

    m_total_logged++;
    m_sequence++;
#else
    UNUSED(entry);
#endif
}

#endif /* TRACE_LOGGER_H */
