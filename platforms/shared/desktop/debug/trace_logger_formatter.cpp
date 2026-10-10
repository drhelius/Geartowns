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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "trace_logger_formatter.h"
#include "gui_debug_constants.h"
#include "gui_debug_i386_tables.h"
#include "../emu.h"
#include "system/i8259.h"
#include "drive/fdc.h"
#include "video/sprite.h"

static const double k_trace_fm_sample_rate = (double)GT_SOUND_CLOCK_RATE / k_ym3438_native_sample_cycles;
static const char* const k_trace_pit_names[6] = { "CH0", "CH1", "CH2", "CH3", "CH4", "CH5" };
static const char* const k_trace_palette_components[3] = { "BLUE", "RED", "GREEN" };
static const char* const k_trace_pcm_registers[7] = { "ENV", "PAN", "FD LSB", "FD MSB", "LS LSB", "LS MSB", "ST" };
static const char* const k_trace_controller_names[5] = { "NONE", "PAD", "MARTY PAD", "6B PAD", "MOUSE" };

static void format_event(char* buffer, size_t size, const char* tag, const char* name, const char* format, ...);
static void format_cpu(const GT_Trace_Entry& entry, const GT_Trace_Format_Options& options, char* buffer, size_t size);
static void format_interrupt(const GT_Trace_Entry& entry, char* buffer, size_t size);
static void format_io(const GT_Trace_Entry& entry, char* buffer, size_t size);
static void format_pic(const GT_Trace_Entry& entry, char* buffer, size_t size);
static void format_timer(const GT_Trace_Entry& entry, char* buffer, size_t size);
static void format_dma(const GT_Trace_Entry& entry, char* buffer, size_t size);
static void format_video(const GT_Trace_Entry& entry, char* buffer, size_t size);
static void format_sprite(const GT_Trace_Entry& entry, char* buffer, size_t size);
static void format_fm(const GT_Trace_Entry& entry, char* buffer, size_t size);
static void format_pcm(const GT_Trace_Entry& entry, char* buffer, size_t size);
static void format_mixer(const GT_Trace_Entry& entry, char* buffer, size_t size);
static void format_cdrom(const GT_Trace_Entry& entry, char* buffer, size_t size);
static void format_fdc(const GT_Trace_Entry& entry, char* buffer, size_t size);
static void format_keyboard(const GT_Trace_Entry& entry, char* buffer, size_t size);
static void format_input(const GT_Trace_Entry& entry, char* buffer, size_t size);
static void format_system(const GT_Trace_Entry& entry, char* buffer, size_t size);
static void format_bit_list(u8 bits, int base, char* buffer, size_t size);
static void format_msf(u32 lba, char* buffer, size_t size);
static void format_buttons(u16 buttons, char* buffer, size_t size);

void trace_log_format_cpu_address(const GT_Trace_Entry& entry, char* buffer, size_t buffer_size)
{
    if (entry.cpu.mode == I386_MODE_PROTECTED)
        snprintf(buffer, buffer_size, "%04X:%08X", entry.cpu.cs, entry.cpu.eip);
    else
        snprintf(buffer, buffer_size, "%04X:%04X    ", entry.cpu.cs, entry.cpu.eip & 0xFFFF);
}

// VM RF NT, IOPL and then the status flags, upper case when set
void trace_log_format_cpu_flags(const GT_Trace_Entry& entry, char* buffer, size_t buffer_size)
{
    u32 f = entry.cpu.eflags;

    snprintf(buffer, buffer_size, "%c%c%c%u%c%c%c%c%c%c%c%c%c",
        (f & I386_FLAG_VM) ? 'V' : 'v', (f & I386_FLAG_RF) ? 'R' : 'r', (f & I386_FLAG_NT) ? 'N' : 'n',
        (unsigned)((f & I386_FLAG_IOPL) >> 12), (f & I386_FLAG_OF) ? 'O' : 'o', (f & I386_FLAG_DF) ? 'D' : 'd',
        (f & I386_FLAG_IF) ? 'I' : 'i', (f & I386_FLAG_TF) ? 'T' : 't', (f & I386_FLAG_SF) ? 'S' : 's',
        (f & I386_FLAG_ZF) ? 'Z' : 'z', (f & I386_FLAG_AF) ? 'A' : 'a', (f & I386_FLAG_PF) ? 'P' : 'p',
        (f & I386_FLAG_CF) ? 'C' : 'c');
}

void trace_log_format_cpu_bytes(const GT_Trace_Entry& entry, char* buffer, size_t buffer_size)
{
    static const char k_hex[] = "0123456789ABCDEF";
    size_t pos = 0;
    u8 size = MIN(entry.cpu.size, (u8)GT_I386_MAX_INSTRUCTION_LENGTH);

    for (u8 i = 0; i < size && pos + 3 < buffer_size; i++)
    {
        u8 value = entry.cpu.opcodes[i];
        buffer[pos++] = k_hex[value >> 4];
        buffer[pos++] = k_hex[value & 0x0F];
        buffer[pos++] = ' ';
    }

    buffer[pos] = 0;
}

void trace_log_format_cycle_prefix(const GT_Trace_Entry& entry, bool previous_cycle_valid,
    u64 previous_cycle, char* buffer, size_t buffer_size)
{
    if (previous_cycle_valid && entry.cycle >= previous_cycle)
        snprintf(buffer, buffer_size, "@%012llu +%-12llu ", (unsigned long long)entry.cycle,
            (unsigned long long)(entry.cycle - previous_cycle));
    else if (previous_cycle_valid)
        snprintf(buffer, buffer_size, "@%012llu RESET         ", (unsigned long long)entry.cycle);
    else
        snprintf(buffer, buffer_size, "@%012llu               ", (unsigned long long)entry.cycle);
}

void trace_logger_format_entry(const GT_Trace_Entry& entry,
    const GT_Trace_Format_Options& options, char* buffer, size_t buffer_size)
{
    if (options.cycles)
    {
        GT_Trace_Format_Options body_options = options;
        body_options.cycles = false;
        char body[GT_TRACE_FORMAT_BUFFER_SIZE];
        char prefix[64];
        trace_logger_format_entry(entry, body_options, body, sizeof(body));
        trace_log_format_cycle_prefix(entry, options.previous_cycle_valid, options.previous_cycle, prefix,
            sizeof(prefix));
        snprintf(buffer, buffer_size, "%s%s", prefix, body);
        return;
    }

    switch (entry.type)
    {
        case TRACE_CPU:
            format_cpu(entry, options, buffer, buffer_size);
            break;
        case TRACE_CPU_INTERRUPT:
            format_interrupt(entry, buffer, buffer_size);
            break;
        case TRACE_IO:
            format_io(entry, buffer, buffer_size);
            break;
        case TRACE_PIC:
            format_pic(entry, buffer, buffer_size);
            break;
        case TRACE_TIMER:
            format_timer(entry, buffer, buffer_size);
            break;
        case TRACE_DMA:
            format_dma(entry, buffer, buffer_size);
            break;
        case TRACE_VIDEO:
            format_video(entry, buffer, buffer_size);
            break;
        case TRACE_SPRITE:
            format_sprite(entry, buffer, buffer_size);
            break;
        case TRACE_FM:
            format_fm(entry, buffer, buffer_size);
            break;
        case TRACE_PCM:
            format_pcm(entry, buffer, buffer_size);
            break;
        case TRACE_MIXER:
            format_mixer(entry, buffer, buffer_size);
            break;
        case TRACE_CDROM:
            format_cdrom(entry, buffer, buffer_size);
            break;
        case TRACE_FDC:
            format_fdc(entry, buffer, buffer_size);
            break;
        case TRACE_KEYBOARD:
            format_keyboard(entry, buffer, buffer_size);
            break;
        case TRACE_INPUT:
            format_input(entry, buffer, buffer_size);
            break;
        case TRACE_SYSTEM:
            format_system(entry, buffer, buffer_size);
            break;
        default:
            snprintf(buffer, buffer_size, "  [TRACE]  UNKNOWN TYPE:$%02X", entry.type);
            break;
    }
}

// Event lines are indented under the instructions, with the tag and the event name in fixed columns
static void format_event(char* buffer, size_t size, const char* tag, const char* name, const char* format, ...)
{
    char tag_text[16];
    snprintf(tag_text, sizeof(tag_text), "[%s]", tag);
    int length = snprintf(buffer, size, "  %-9s%-11s", tag_text, name);

    if (length < 0 || (size_t)length >= size)
        return;

    va_list args;
    va_start(args, format);
    vsnprintf(buffer + length, size - length, format, args);
    va_end(args);
}

static void format_cpu(const GT_Trace_Entry& entry, const GT_Trace_Format_Options& options, char* buffer, size_t size)
{
    char linear[16] = "";
    char address[24];
    char registers[112] = "";
    char segments[48] = "";
    char flags[24] = "";
    char bytes[48] = "";

    if (options.linear)
        snprintf(linear, sizeof(linear), "%08X ", entry.cpu.linear);

    trace_log_format_cpu_address(entry, address, sizeof(address));

    if (options.registers)
    {
        const u32* r = entry.cpu.registers;
        snprintf(registers, sizeof(registers),
            "EAX:%08X EBX:%08X ECX:%08X EDX:%08X ESI:%08X EDI:%08X EBP:%08X ESP:%08X  ", r[I386_REG_EAX],
            r[I386_REG_EBX], r[I386_REG_ECX], r[I386_REG_EDX], r[I386_REG_ESI], r[I386_REG_EDI], r[I386_REG_EBP],
            r[I386_REG_ESP]);
    }

    if (options.segments)
        snprintf(segments, sizeof(segments), "DS:%04X ES:%04X SS:%04X FS:%04X GS:%04X  ", entry.cpu.ds, entry.cpu.es,
            entry.cpu.ss, entry.cpu.fs, entry.cpu.gs);

    if (options.flags)
    {
        trace_log_format_cpu_flags(entry, flags, sizeof(flags) - 2);
        strcat(flags, "  ");
    }

    if (options.bytes)
        trace_log_format_cpu_bytes(entry, bytes, sizeof(bytes));

    const char* name = entry.cpu.name[0] != 0 ? entry.cpu.name : "???";

    if (strlen(entry.cpu.name) == GT_TRACE_NAME_SIZE - 1)
    {
        const I386_Disassembler_Record* record = emu_get_core()->GetI386()->GetDisassemblerRecord(entry.cpu.linear);

        if (IsValidPointer(record) && record->size == entry.cpu.size &&
            memcmp(record->opcodes, entry.cpu.opcodes, entry.cpu.size) == 0)
            name = record->name;
    }

    snprintf(buffer, size, "%s%s  %s%s%s%-*s %s", linear, address, registers, segments, flags,
        GT_TRACE_INSTRUCTION_WIDTH, name, bytes);
}

static void format_interrupt(const GT_Trace_Entry& entry, char* buffer, size_t size)
{
    char name[16];
    char description[64];
    u8 vector = entry.interrupt.vector;
    u32 from = entry.interrupt.from;
    u32 to = entry.interrupt.to;

    gui_debug_i386_vector_name(vector, name, sizeof(name), description, sizeof(description), false);

    switch (entry.event)
    {
        case TRACE_CPU_INTERRUPT_HARDWARE:
        {
            u8 line = entry.interrupt.line;

            if (line < 16)
                format_event(buffer, size, "INT", "IRQ", "IRQ%u %s  Vector:$%02X  From:$%08X  To:$%08X", line,
                    k_debug_irq_sources[line], vector, from, to);
            else
                format_event(buffer, size, "INT", "IRQ", "%s  Vector:$%02X  From:$%08X  To:$%08X", description,
                    vector, from, to);
            break;
        }
        case TRACE_CPU_INTERRUPT_EXCEPTION:
        {
            char error[24] = "";

            if (entry.interrupt.has_error_code)
                snprintf(error, sizeof(error), "  Error:$%04X", entry.interrupt.error_code);

            format_event(buffer, size, "INT", "EXCEPTION", "%s %s  Vector:$%02X%s  From:$%08X  To:$%08X", name,
                description, vector, error, from, to);
            break;
        }
        default:
        {
            const char* function = gui_debug_i386_interrupt_function(vector, entry.interrupt.ax);
            bool named = IsValidPointer(function);

            format_event(buffer, size, "INT", "SOFTWARE", "INT $%02X %s%s%s  AX:$%04X  From:$%08X  To:$%08X", vector,
                description, named ? ": " : "", named ? function : "", entry.interrupt.ax, from, to);
            break;
        }
    }
}

static void format_io(const GT_Trace_Entry& entry, char* buffer, size_t size)
{
    const char* label = gui_debug_port_label(entry.io.port);
    int digits = MIN((int)entry.io.size, 4) * 2;
    u32 mask = digits >= 8 ? 0xFFFFFFFFU : (1U << (digits * 4)) - 1;
    bool write = entry.event == TRACE_IO_WRITE;

    format_event(buffer, size, "I/O", write ? "OUT" : "IN", "$%04X %-18s %s $%0*X  PC:$%08X", entry.io.port,
        IsValidPointer(label) ? label : "", write ? "<-" : "->", digits, entry.io.value & mask, entry.io.pc);
}

static void format_pic(const GT_Trace_Entry& entry, char* buffer, size_t size)
{
    const char* chip = entry.pic.chip != 0 ? "SLAVE " : "MASTER";
    int base = entry.pic.chip != 0 ? 8 : 0;
    u8 value = entry.pic.value;
    char state[40];

    snprintf(state, sizeof(state), "IRR:$%02X ISR:$%02X IMR:$%02X", entry.pic.irr, entry.pic.isr, entry.pic.imr);

    switch (entry.event)
    {
        case TRACE_PIC_REQUEST:
        {
            u8 line = entry.pic.line & 0x0F;
            bool masked = ((entry.pic.imr >> (line & 7)) & 0x01) != 0;

            format_event(buffer, size, "PIC", "REQUEST", "IRQ%u %s  Vector:$%02X%s  %s", line,
                k_debug_irq_sources[line], entry.pic.vector, masked ? "  MASKED" : "", state);
            break;
        }
        case TRACE_PIC_MASK:
        {
            char unmasked[48];
            format_bit_list((u8)~entry.pic.imr, base, unmasked, sizeof(unmasked));
            format_event(buffer, size, "PIC", "MASK", "%s  IMR:$%02X->$%02X  Unmasked IRQs:%s  %s", chip,
                entry.pic.previous, entry.pic.imr, unmasked, state);
            break;
        }
        case TRACE_PIC_COMMAND:
        {
            if ((value & k_i8259_ocw3_select) != 0)
            {
                const char* read = (value & k_i8259_ocw3_rr) == 0 ? "" : (value & k_i8259_ocw3_ris) != 0 ?
                    " READ ISR" : " READ IRR";
                const char* special = (value & k_i8259_ocw3_esmm) == 0 ? "" : (value & k_i8259_ocw3_smm) != 0 ?
                    " SPECIAL MASK ON" : " SPECIAL MASK OFF";

                format_event(buffer, size, "PIC", "OCW3", "%s $%02X %s%s%s  %s", chip, value, read,
                    (value & k_i8259_ocw3_poll) != 0 ? " POLL" : "", special, state);
                break;
            }

            static const char* const k_ocw2_names[8] =
            {
                "CLEAR ROTATE ON AEOI", "NON-SPECIFIC EOI", "NOP", "SPECIFIC EOI", "SET ROTATE ON AEOI",
                "ROTATE ON NON-SPECIFIC EOI", "SET PRIORITY", "ROTATE ON SPECIFIC EOI"
            };
            int operation = value >> 5;
            bool eoi = (operation & 0x01) != 0;
            bool specific = operation == 3 || operation >= 6;

            if (specific)
                format_event(buffer, size, "PIC", eoi ? "EOI" : "OCW2", "%s $%02X %s IRQ%d  %s", chip, value,
                    k_ocw2_names[operation], base + (value & 0x07), state);
            else
                format_event(buffer, size, "PIC", eoi ? "EOI" : "OCW2", "%s $%02X %s  %s", chip, value,
                    k_ocw2_names[operation], state);
            break;
        }
        default:
        {
            switch (entry.pic.step)
            {
                case I8259::I8259_INIT_READY:
                    format_event(buffer, size, "PIC", "ICW1", "%s $%02X  %s %s ICW4:%s", chip, value,
                        (value & k_i8259_icw1_ltim) != 0 ? "LEVEL" : "EDGE",
                        (value & k_i8259_icw1_sngl) != 0 ? "SINGLE" : "CASCADE",
                        (value & k_i8259_icw1_ic4) != 0 ? "YES" : "NO");
                    break;
                case I8259::I8259_INIT_ICW2:
                    format_event(buffer, size, "PIC", "ICW2", "%s $%02X  Vectors:$%02X-$%02X", chip, value,
                        value & 0xF8, (value & 0xF8) + 7);
                    break;
                case I8259::I8259_INIT_ICW3:
                    if (entry.pic.chip == 0)
                        format_event(buffer, size, "PIC", "ICW3", "%s $%02X  Slave on IR:$%02X", chip, value, value);
                    else
                        format_event(buffer, size, "PIC", "ICW3", "%s $%02X  Slave ID:%u", chip, value, value & 0x07);
                    break;
                default:
                    format_event(buffer, size, "PIC", "ICW4", "%s $%02X  8086:%u AEOI:%u SFNM:%u", chip, value,
                        (value & k_i8259_icw4_upm) != 0 ? 1 : 0, (value & k_i8259_icw4_aeoi) != 0 ? 1 : 0,
                        (value & k_i8259_icw4_sfnm) != 0 ? 1 : 0);
                    break;
            }
            break;
        }
    }
}

static void format_timer(const GT_Trace_Entry& entry, char* buffer, size_t size)
{
    int channel = MIN(entry.timer.chip * 3 + entry.timer.counter, 5);
    u8 value = entry.timer.value;

    switch (entry.event)
    {
        case TRACE_TIMER_TIMEOUT:
        {
            bool irq = (value & entry.timer.enable) != 0;
            const char* source = value == 0x03 ? "CH0+CH1" : (value & 0x01) != 0 ? "CH0" : "CH1";

            format_event(buffer, size, "TIMER", "TIMEOUT", "%s %s  Latch:$%X  Enabled:$%X%s", source,
                (value & 0x01) != 0 ? k_debug_pit_uses[0] : k_debug_pit_uses[1], entry.timer.latch, entry.timer.enable,
                irq ? "  IRQ0" : "");
            break;
        }
        case TRACE_TIMER_CONTROL:
        {
            if (entry.timer.counter > 2)
            {
                format_event(buffer, size, "TIMER", "CONTROL", "PIT%u $%02X  Read-back is not on the 8253",
                    entry.timer.chip, value);
                break;
            }

            if (((value >> 4) & 0x03) == 0)
            {
                format_event(buffer, size, "TIMER", "CONTROL", "%s %s  $%02X  LATCH COUNT", k_trace_pit_names[channel],
                    k_debug_pit_uses[channel], value);
                break;
            }

            format_event(buffer, size, "TIMER", "CONTROL", "%s %s  $%02X  Mode:%u %s  Access:%s%s",
                k_trace_pit_names[channel], k_debug_pit_uses[channel], value, entry.timer.mode,
                k_debug_pit_mode_names[MIN(entry.timer.mode, (u8)5)], k_debug_pit_access_names[entry.timer.access & 3],
                entry.timer.bcd ? "  BCD" : "");
            break;
        }
        case TRACE_TIMER_COUNTER:
        {
            if (!entry.timer.complete)
            {
                format_event(buffer, size, "TIMER", "COUNTER", "%s %s  Write:$%02X  LSB, MSB pending",
                    k_trace_pit_names[channel], k_debug_pit_uses[channel], value);
                break;
            }

            u32 reload = entry.timer.reload;
            u32 count = reload;

            if (entry.timer.bcd)
            {
                count = ((reload >> 12) & 0x0F) * 1000 + ((reload >> 8) & 0x0F) * 100 + ((reload >> 4) & 0x0F) * 10 +
                    (reload & 0x0F);
                count = count == 0 ? 10000 : count;
            }
            else if (count == 0)
                count = 65536;

            double clock = channel == 4 ? 1228800.0 : 307200.0;

            format_event(buffer, size, "TIMER", "COUNTER", "%s %s  Write:$%02X  Reload:$%04X  %.2f Hz",
                k_trace_pit_names[channel], k_debug_pit_uses[channel], value, reload, clock / count);
            break;
        }
        default:
            format_event(buffer, size, "TIMER", "IRQ CTRL", "Write:$%02X  Enable CH0:%u CH1:%u%s  Buzzer:%u  Latch:$%X",
                value, value & 0x01, (value >> 1) & 0x01, (value & 0x80) != 0 ? "  ACK CH0" : "",
                (value >> 2) & 0x01, entry.timer.latch);
            break;
    }
}

static void format_dma(const GT_Trace_Entry& entry, char* buffer, size_t size)
{
    u8 channel = entry.dma.channel & 0x03;
    const char* device = k_debug_dma_devices[channel];

    switch (entry.event)
    {
        case TRACE_DMA_REQUEST:
            format_event(buffer, size, "DMA", "REQUEST", "CH%u %s  Address:$%08X  Count:$%04X  Mode:$%02X%s", channel,
                device, entry.dma.address, entry.dma.count, entry.dma.mode,
                ((entry.dma.mask >> channel) & 0x01) != 0 ? "  MASKED" : "");
            break;
        case TRACE_DMA_END:
            format_event(buffer, size, "DMA", "END", "CH%u %s  %s  Address:$%08X  Count:$%04X", channel, device,
                entry.dma.terminal ? "TERMINAL COUNT" : "DEVICE END", entry.dma.address, entry.dma.count);
            break;
        default:
        {
            u8 value = entry.dma.value;

            switch (entry.dma.reg)
            {
                case 0x00:
                    format_event(buffer, size, "DMA", "INIT", "Write:$%02X  Bus:%s%s", value,
                        (value & 0x02) != 0 ? "16-BIT" : "8-BIT", (value & 0x01) != 0 ? "  RESET" : "");
                    break;
                case 0x01:
                    format_event(buffer, size, "DMA", "SELECT", "CH%u %s  %s", channel, device,
                        (value & 0x04) != 0 ? "BASE" : "BASE+CURRENT");
                    break;
                case 0x02:
                case 0x03:
                    format_event(buffer, size, "DMA", "COUNT", "CH%u %s  Write:$%02X  Count:$%04X", channel, device,
                        value, entry.dma.count);
                    break;
                case 0x04:
                case 0x05:
                case 0x06:
                case 0x07:
                    format_event(buffer, size, "DMA", "ADDRESS", "CH%u %s  Write:$%02X  Address:$%08X", channel,
                        device, value, entry.dma.address);
                    break;
                case 0x08:
                case 0x09:
                    format_event(buffer, size, "DMA", "DEVICE", "Write:$%02X  Control:$%03X%s", value, entry.dma.count,
                        (entry.dma.count & 0x0004) != 0 ? "  DISABLED" : "");
                    break;
                case 0x0A:
                    format_event(buffer, size, "DMA", "MODE", "CH%u %s  $%02X  %s %s %s%s%s", channel, device, value,
                        k_debug_dma_direction_names[(value >> 2) & 0x03], k_debug_dma_service_names[(value >> 6) & 0x03],
                        (value & 0x01) != 0 ? "WORD" : "BYTE", (value & 0x10) != 0 ? " AUTO-INIT" : "",
                        (value & 0x20) != 0 ? " DECREMENT" : "");
                    break;
                case 0x0E:
                    format_event(buffer, size, "DMA", "REQUEST", "Software requests:$%X", value & 0x0F);
                    break;
                default:
                {
                    char masked[24];
                    format_bit_list(value & 0x0F, 0, masked, sizeof(masked));
                    format_event(buffer, size, "DMA", "MASK", "$%X  Masked channels:%s", value & 0x0F, masked);
                    break;
                }
            }
            break;
        }
    }
}

static void format_video(const GT_Trace_Entry& entry, char* buffer, size_t size)
{
    char beam[32] = "CRTC stopped";
    u8 raw = entry.video.raw;

    if (entry.video.line != 0xFFFF)
        snprintf(beam, sizeof(beam), "Line:%u Dot:%u", entry.video.line, entry.video.dot);

    switch (entry.event)
    {
        case TRACE_VIDEO_CRTC:
            format_event(buffer, size, "VIDEO", "CRTC", "R%02X %-4s <- $%04X  %s:$%02X  %s", entry.video.reg,
                k_debug_crtc_register_names[entry.video.reg & 0x1F], entry.video.value, entry.video.bank ? "MSB" : "LSB",
                raw, beam);
            break;
        case TRACE_VIDEO_OUTPUT:
            if (entry.video.reg == 0)
                format_event(buffer, size, "VIDEO", "OUTPUT", "R0 <- $%02X  %s  %s", raw,
                    (raw & 0x10) != 0 ? "TWO PAGE" : "SINGLE PAGE", beam);
            else if (entry.video.reg == 1)
            {
                int palette = (raw >> 4) & 0x03;
                format_event(buffer, size, "VIDEO", "OUTPUT", "R1 <- $%02X  Front:LAYER %u  Palette:%s  %s", raw,
                    raw & 0x01, palette == 0 ? "LAYER 0" : palette == 2 ? "LAYER 1" : "256", beam);
            }
            else
                format_event(buffer, size, "VIDEO", "OUTPUT", "R%u <- $%02X  %s", entry.video.reg, raw, beam);
            break;
        case TRACE_VIDEO_DISPLAY:
            format_event(buffer, size, "VIDEO", "DISPLAY", "Write:$%02X  Layer 0:%s  Layer 1:%s  %s", raw,
                (raw & 0x0C) != 0 ? "ON " : "OFF", (raw & 0x03) != 0 ? "ON " : "OFF", beam);
            break;
        case TRACE_VIDEO_PALETTE:
        {
            u8 bank = entry.video.bank;
            const char* name = bank == 0 ? "LAYER 0" : bank == 2 ? "LAYER 1" : "256";
            u8 index = bank == 0 || bank == 2 ? (entry.video.reg & 0x0F) : entry.video.reg;

            format_event(buffer, size, "VIDEO", "PALETTE", "%s[$%02X] %s <- $%02X  RGB:$%06X  %s", name, index,
                k_trace_palette_components[MIN(entry.video.value, (u16)2)], raw, entry.video.param, beam);
            break;
        }
        case TRACE_VIDEO_DIGITAL_PALETTE:
            format_event(buffer, size, "VIDEO", "DPALETTE", "[%u] <- $%X  %s", entry.video.reg, raw & 0x0F, beam);
            break;
        case TRACE_VIDEO_MASK:
            format_event(buffer, size, "VIDEO", "VRAM MASK", "Write:$%02X  Mask:$%08X  %s", raw, entry.video.param,
                beam);
            break;
        case TRACE_VIDEO_VSYNC:
            format_event(buffer, size, "VIDEO", "VSYNC", "IRQ11  Frame:%u", entry.video.param);
            break;
        case TRACE_VIDEO_VSYNC_CLEAR:
            format_event(buffer, size, "VIDEO", "VSYNC ACK", "IRQ11 cleared  %s", beam);
            break;
        case TRACE_VIDEO_FMR:
            switch (entry.video.reg)
            {
                case 0x81:
                    format_event(buffer, size, "VIDEO", "FM-R", "PLANE MASK <- $%02X  %s", raw, beam);
                    break;
                case 0x82:
                    format_event(buffer, size, "VIDEO", "FM-R", "DISPLAY <- $%02X  Planes:$%X  Page:%u  %s", raw,
                        (raw & 0x07) | ((raw >> 2) & 0x08), (raw >> 4) & 0x01, beam);
                    break;
                case 0x83:
                    format_event(buffer, size, "VIDEO", "FM-R", "ACCESS PAGE <- $%02X  Page:%u  %s", raw,
                        (raw >> 4) & 0x01, beam);
                    break;
                default:
                    format_event(buffer, size, "VIDEO", "FM-R", "ANK FONT <- $%02X  %s  %s", raw,
                        (raw & 0x01) != 0 ? "ON " : "OFF", beam);
                    break;
            }
            break;
        default:
        {
            const char* access = raw == 0x03 ? "R/W" : raw == 0x02 ? "W" : "R";
            format_event(buffer, size, "VIDEO", "VBLANK", "MISSED  Watch:$%08X %s  Consecutive:%u", entry.video.param,
                access, entry.video.value);
            break;
        }
    }
}

static void format_sprite(const GT_Trace_Entry& entry, char* buffer, size_t size)
{
    double ms = (double)entry.sprite.clocks * 1000.0 / GT_CPU_CLOCK_RATE;

    switch (entry.event)
    {
        case TRACE_SPRITE_REGISTER:
        {
            u8 reg = entry.sprite.reg;
            u8 value = entry.sprite.value;

            if (reg == k_sprite_control0 || reg == k_sprite_control1)
                format_event(buffer, size, "SPRITE", "REGISTER", "R%u <- $%02X  First:%u  Enabled:%u", reg,
                    entry.sprite.raw, entry.sprite.first, reg == k_sprite_control1 ? (value >> 7) & 0x01 : 0);
            else if (reg == k_sprite_display_page)
                format_event(buffer, size, "SPRITE", "REGISTER", "R6 <- $%02X  Display page:%u", entry.sprite.raw,
                    (value >> 7) & 0x01);
            else
                format_event(buffer, size, "SPRITE", "REGISTER", "R%u <- $%02X  %s", reg, entry.sprite.raw,
                    reg < k_sprite_offset_y ? "OFFSET X" : "OFFSET Y");
            break;
        }
        case TRACE_SPRITE_TRANSFER_START:
            format_event(buffer, size, "SPRITE", "START", "Drawing page:%u  Entries:%u-1023 (%u)  Time:%.2f ms",
                entry.sprite.page, entry.sprite.first, entry.sprite.count, ms);
            break;
        case TRACE_SPRITE_TRANSFER_END:
            format_event(buffer, size, "SPRITE", "END", "Drawn page:%u  Entries:%u  Time:%.2f ms", entry.sprite.page,
                entry.sprite.count, ms);
            break;
        default:
            format_event(buffer, size, "SPRITE", "BUSY", "At VSYNC, no transfer this frame  Entry:%u/%u  Page:%u",
                entry.sprite.entry, entry.sprite.count, entry.sprite.page);
            break;
    }
}

static void format_fm(const GT_Trace_Entry& entry, char* buffer, size_t size)
{
    u16 address = entry.fm.address;
    int part = (address >> 8) & 0x01;
    u8 reg = (u8)address;
    u8 value = entry.fm.value;
    u8 channel = entry.fm.channel + 1;

    switch (entry.event)
    {
        case TRACE_FM_KEY:
        {
            u8 slots = value >> 4;
            format_event(buffer, size, "FM", "KEY", "CH%u %s  Slots:%c%c%c%c  Write:$%02X", channel,
                slots != 0 ? "ON " : "OFF", (slots & 0x01) ? '1' : '-', (slots & 0x02) ? '2' : '-',
                (slots & 0x04) ? '3' : '-', (slots & 0x08) ? '4' : '-', value);
            break;
        }
        case TRACE_FM_FREQUENCY:
        {
            u16 frequency = entry.fm.frequency;
            const char* latched = (reg & 0x04) != 0 ? "  (latched until LSB)" : "";

            if (reg >= 0xA8)
            {
                static const int k_special_operator[3] = { 3, 1, 2 };
                format_event(buffer, size, "FM", "FREQ", "CH3 OP%d  $%02X <- $%02X  Block:%u FNum:$%03X%s",
                    k_special_operator[(reg & 0x03) % 3], reg, value, (frequency >> 11) & 0x07, frequency & 0x7FF,
                    latched);
            }
            else
                format_event(buffer, size, "FM", "FREQ", "CH%u  $%02X <- $%02X  Block:%u FNum:$%03X%s", channel, reg,
                    value, (frequency >> 11) & 0x07, frequency & 0x7FF, latched);
            break;
        }
        case TRACE_FM_OPERATOR:
        {
            char name[48];
            char fields[32];

            if (!gui_debug_ym3438_register_name(part, reg, name, sizeof(name)))
                snprintf(name, sizeof(name), "$%02X", reg);

            switch (reg & 0xF0)
            {
                case 0x30: snprintf(fields, sizeof(fields), "DT:%u MUL:%u", (value >> 4) & 0x07, value & 0x0F); break;
                case 0x40: snprintf(fields, sizeof(fields), "TL:$%02X", value & 0x7F); break;
                case 0x50: snprintf(fields, sizeof(fields), "KS:%u AR:%u", value >> 6, value & 0x1F); break;
                case 0x60: snprintf(fields, sizeof(fields), "AM:%u D1R:%u", value >> 7, value & 0x1F); break;
                case 0x70: snprintf(fields, sizeof(fields), "D2R:%u", value & 0x1F); break;
                case 0x80: snprintf(fields, sizeof(fields), "D1L:%u RR:%u", value >> 4, value & 0x0F); break;
                default: snprintf(fields, sizeof(fields), "SSG-EG:$%X", value & 0x0F); break;
            }

            format_event(buffer, size, "FM", "OPERATOR", "%s <- $%02X  %s", name, value, fields);
            break;
        }
        case TRACE_FM_CHANNEL:
            if (reg < 0xB4)
                format_event(buffer, size, "FM", "CHANNEL", "CH%u  $%02X <- $%02X  ALG:%u FB:%u", channel, reg, value,
                    value & 0x07, (value >> 3) & 0x07);
            else
                format_event(buffer, size, "FM", "CHANNEL", "CH%u  $%02X <- $%02X  L:%u R:%u AMS:%u PMS:%u", channel,
                    reg, value, value >> 7, (value >> 6) & 0x01, (value >> 4) & 0x03, value & 0x07);
            break;
        case TRACE_FM_DAC:
            format_event(buffer, size, "FM", "DAC", "Data:$%02X", value);
            break;
        case TRACE_FM_TIMER:
        {
            u16 timer = entry.fm.frequency;

            if (reg == 0x24 || reg == 0x25)
                format_event(buffer, size, "FM", "TIMER", "A $%02X <- $%02X  Timer A:%u  %.3f ms", reg, value, timer,
                    (1024 - timer) * 1000.0 / k_trace_fm_sample_rate);
            else if (reg == 0x26)
                format_event(buffer, size, "FM", "TIMER", "B $%02X <- $%02X  Timer B:%u  %.3f ms", reg, value, timer,
                    (256 - timer) * 16 * 1000.0 / k_trace_fm_sample_rate);
            else
                format_event(buffer, size, "FM", "TIMER",
                    "CONTROL <- $%02X  Load A:%u B:%u  Enable A:%u B:%u  Reset A:%u B:%u  CH3:%s", value, value & 0x01,
                    (value >> 1) & 0x01, (value >> 2) & 0x01, (value >> 3) & 0x01, (value >> 4) & 0x01,
                    (value >> 5) & 0x01, k_debug_ym3438_ch3_mode_names[value >> 6]);
            break;
        }
        case TRACE_FM_IRQ:
        {
            u8 flags = entry.fm.flags;
            const char* source = (flags & 0x03) == 0x03 ? "TIMER A+B" : (flags & 0x01) != 0 ? "TIMER A" : "TIMER B";

            format_event(buffer, size, "FM", "IRQ", "IRQ13 %s  Flags A:%u B:%u", source, flags & 0x01,
                (flags >> 1) & 0x01);
            break;
        }
        default:
        {
            char name[48];

            if (part != 0 && reg < 0x30)
                format_event(buffer, size, "FM", "GLOBAL", "Part 1 $%02X <- $%02X  ignored", reg, value);
            else if (reg == 0x22)
                format_event(buffer, size, "FM", "GLOBAL", "LFO <- $%02X  %s  Freq:%u", value,
                    (value & 0x08) != 0 ? "ON " : "OFF", value & 0x07);
            else if (reg == 0x2B)
                format_event(buffer, size, "FM", "GLOBAL", "DAC ENABLE <- $%02X  %s", value,
                    (value & 0x80) != 0 ? "ON " : "OFF");
            else if (gui_debug_ym3438_register_name(part, reg, name, sizeof(name)))
                format_event(buffer, size, "FM", "GLOBAL", "%s <- $%02X", name, value);
            else
                format_event(buffer, size, "FM", "GLOBAL", "Part %d $%02X <- $%02X", part, reg, value);
            break;
        }
    }
}

static void format_pcm(const GT_Trace_Entry& entry, char* buffer, size_t size)
{
    u8 value = entry.pcm.value;
    u8 channel = entry.pcm.channel + 1;

    switch (entry.event)
    {
        case TRACE_PCM_CHANNEL:
        {
            u8 reg = MIN(entry.pcm.reg, (u8)6);

            if (reg == 1)
                format_event(buffer, size, "PCM", "CHANNEL", "CH%u %s <- $%02X  L:%u R:%u", channel,
                    k_trace_pcm_registers[reg], value, value & 0x0F, value >> 4);
            else
                format_event(buffer, size, "PCM", "CHANNEL", "CH%u %s <- $%02X", channel, k_trace_pcm_registers[reg],
                    value);
            break;
        }
        case TRACE_PCM_KEY:
        {
            char on[32];
            format_bit_list((u8)~value, 1, on, sizeof(on));
            format_event(buffer, size, "PCM", "KEY", "Write:$%02X  On:%s", value, on);
            break;
        }
        case TRACE_PCM_CONTROL:
            if ((value & 0x40) != 0)
                format_event(buffer, size, "PCM", "CONTROL", "Write:$%02X  Sound:%s  Channel:%u", value,
                    (value & 0x80) != 0 ? "ON " : "OFF", (value & 0x07) + 1);
            else
                format_event(buffer, size, "PCM", "CONTROL", "Write:$%02X  Sound:%s  Wave bank:%u", value,
                    (value & 0x80) != 0 ? "ON " : "OFF", value & 0x0F);
            break;
        case TRACE_PCM_IRQ_MASK:
            format_event(buffer, size, "PCM", "IRQ MASK", "Write:$%02X", value);
            break;
        case TRACE_PCM_IRQ:
            format_event(buffer, size, "PCM", "IRQ", "IRQ13 Causes:$%02X  Mask:$%02X", value, entry.pcm.mask);
            break;
        default:
            format_event(buffer, size, "PCM", "IRQ READ", "Causes:$%02X read and cleared", value);
            break;
    }
}

static void format_mixer(const GT_Trace_Entry& entry, char* buffer, size_t size)
{
    u8 value = entry.mixer.value;

    if (entry.event == TRACE_MIXER_MUTE)
    {
        if (entry.mixer.port == 0x04D5)
            format_event(buffer, size, "MIXER", "MUTE", "$04D5 <- $%02X  FM:%s  PCM:%s", value,
                (value & 0x02) != 0 ? "ON " : "OFF", (value & 0x01) != 0 ? "ON " : "OFF");
        else
            format_event(buffer, size, "MIXER", "MUTE", "$04EC <- $%02X  Output:%s  LED:%s", value,
                (value & 0x40) != 0 ? "ON " : "OFF", (value & 0x80) != 0 ? "OFF" : "ON ");
        return;
    }

    u8 control = entry.mixer.control;
    const char* level = (control & 0x04) == 0 ? "MUTED" : (control & 0x10) != 0 ? "-32 dB" :
        (control & 0x08) != 0 ? "0 dB" : "DATA";

    format_event(buffer, size, "MIXER", "VOLUME", "$%04X <- $%02X  Volume %u CH%u%s  Data:$%02X  Level:%s",
        entry.mixer.port, value, entry.mixer.chip + 1, entry.mixer.channel,
        entry.mixer.chip == 1 && entry.mixer.channel < 2 ? (entry.mixer.channel == 0 ? " CD L" : " CD R") : "",
        entry.mixer.data, level);
}

static void format_cdrom(const GT_Trace_Entry& entry, char* buffer, size_t size)
{
    char msf[16];
    char end_msf[16];
    const u8* b = entry.cdrom.bytes;
    u32 lba = entry.cdrom.lba;

    format_msf(lba, msf, sizeof(msf));
    format_msf(entry.cdrom.end_lba, end_msf, sizeof(end_msf));

    switch (entry.event)
    {
        case TRACE_CDROM_COMMAND:
        {
            u8 command = entry.cdrom.command;
            u8 code = command & k_cdrom_command_mask;
            char flags[24];

            snprintf(flags, sizeof(flags), "%s%s", (command & k_cdrom_flag_irq) != 0 ? " +IRQ" : "",
                (command & k_cdrom_flag_status) != 0 ? " +STATUS" : "");

            if (code <= 0x04)
                format_event(buffer, size, "CDROM", "COMMAND", "$%02X %s%s  MSF %02X:%02X:%02X-%02X:%02X:%02X%s",
                    command, gui_debug_cdrom_command_name(code), flags, b[0], b[1], b[2], b[3], b[4], b[5],
                    code == 0x04 && b[6] == 0x01 ? "  REPEAT" : "");
            else
                format_event(buffer, size, "CDROM", "COMMAND", "$%02X %s%s  Params:%02X %02X %02X %02X %02X %02X %02X %02X",
                    command, gui_debug_cdrom_command_name(code), flags, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
            break;
        }
        case TRACE_CDROM_STATUS:
        {
            const char* name = "";

            switch (b[0])
            {
                case 0x00: name = b[1] == 0x09 ? "READY, NO DISC" : "READY"; break;
                case 0x04: name = "SEEK DONE"; break;
                case 0x06: name = "READ DONE"; break;
                case 0x11: name = "CDDA STOPPED"; break;
                case 0x12: name = "CDDA PAUSED"; break;
                case 0x13: name = "CDDA RESUMED"; break;
                case 0x16:
                case 0x17: name = "TOC"; break;
                case 0x18:
                case 0x19:
                case 0x20: name = "SUBQ"; break;
                case 0x22: name = "DATA READY"; break;
                case 0x21:
                {
                    switch (b[1])
                    {
                        case 0x01: name = "ERROR, BAD PARAMETER"; break;
                        case 0x04: name = "ERROR, READ FAILED"; break;
                        case 0x05: name = "ERROR, AUDIO SECTOR"; break;
                        case 0x08: name = "ERROR, DISC CHANGED"; break;
                        case 0x09: name = "ERROR, NOT READY"; break;
                        case 0x0F: name = "ERROR, DATA LOST"; break;
                        default: name = "ERROR"; break;
                    }
                    break;
                }
                default: break;
            }

            format_event(buffer, size, "CDROM", "STATUS", "%02X %02X %02X %02X  %s", b[0], b[1], b[2], b[3], name);
            break;
        }
        case TRACE_CDROM_IRQ:
            format_event(buffer, size, "CDROM", "IRQ", "IRQ9%s%s", (entry.cdrom.value & 0x01) != 0 ? " STATUS" : "",
                (entry.cdrom.value & 0x02) != 0 ? " DMA END" : "");
            break;
        case TRACE_CDROM_CONTROL:
        {
            u8 value = entry.cdrom.value;
            format_event(buffer, size, "CDROM", "CONTROL", "$04C0 <- $%02X %s%s%s  Status IRQ:%s  DMA end IRQ:%s",
                value, (value & 0x80) != 0 ? " ACK STATUS" : "", (value & 0x40) != 0 ? " ACK DMA END" : "",
                (value & 0x04) != 0 ? " RESET" : "", (value & 0x02) != 0 ? "ON " : "OFF",
                (value & 0x01) != 0 ? "ON " : "OFF");
            break;
        }
        case TRACE_CDROM_SECTOR_READY:
            format_event(buffer, size, "CDROM", "DATA READY", "LBA:%u  MSF %s  %s  Last:%u", lba, msf,
                gui_debug_cdrom_command_name(entry.cdrom.command & k_cdrom_command_mask), entry.cdrom.end_lba);
            break;
        case TRACE_CDROM_TRANSFER:
            format_event(buffer, size, "CDROM", "TRANSFER", "$04C6 <- $%02X  %s  LBA:%u  Bytes:%u", entry.cdrom.value,
                (entry.cdrom.value & 0x10) != 0 ? "DMA" : "CPU", lba, entry.cdrom.size);
            break;
        case TRACE_CDROM_SECTOR_END:
            if (entry.cdrom.size != 0)
                format_event(buffer, size, "CDROM", "SECTOR END", "LBA:%u  %s  Dropped:%u bytes", lba,
                    (entry.cdrom.flags & 0x10) != 0 ? "DMA" : "CPU", entry.cdrom.size);
            else
                format_event(buffer, size, "CDROM", "SECTOR END", "LBA:%u  %s", lba,
                    (entry.cdrom.flags & 0x10) != 0 ? "DMA" : "CPU");
            break;
        case TRACE_CDROM_LOST_DATA:
            format_event(buffer, size, "CDROM", "LOST DATA", "LBA:%u  MSF %s  Not transferred in time", lba, msf);
            break;
        case TRACE_CDROM_CDDA_PLAY:
            format_event(buffer, size, "CDROM", "CDDA PLAY", "LBA:%u-%u  MSF %s-%s  Seek:%u ms%s", lba,
                entry.cdrom.end_lba, msf, end_msf, entry.cdrom.size, (entry.cdrom.flags & 0x01) != 0 ? "  REPEAT" : "");
            break;
        case TRACE_CDROM_CDDA_PAUSE:
            format_event(buffer, size, "CDROM", "CDDA PAUSE", "LBA:%u  MSF %s", lba, msf);
            break;
        case TRACE_CDROM_CDDA_RESUME:
            format_event(buffer, size, "CDROM", "CDDA RESUME", "LBA:%u  MSF %s", lba, msf);
            break;
        case TRACE_CDROM_CDDA_STOP:
            format_event(buffer, size, "CDROM", "CDDA STOP", "LBA:%u  MSF %s", lba, msf);
            break;
        case TRACE_CDROM_CDDA_END:
            format_event(buffer, size, "CDROM", "CDDA END", "LBA:%u  MSF %s", lba, msf);
            break;
        default:
            format_event(buffer, size, "CDROM", "CDDA LOOP", "Back to LBA:%u  MSF %s  End:%u", lba, msf,
                entry.cdrom.end_lba);
            break;
    }
}

static void format_fdc(const GT_Trace_Entry& entry, char* buffer, size_t size)
{
    char drive[8] = "-";

    if (entry.fdc.drive >= 0)
        snprintf(drive, sizeof(drive), "%d", entry.fdc.drive);

    switch (entry.event)
    {
        case TRACE_FDC_COMMAND:
        {
            char command[48];
            gui_debug_mb8877_command(entry.fdc.command, command, sizeof(command));
            format_event(buffer, size, "FDC", "COMMAND", "$%02X %s  Drive:%s Cylinder:%u  Track:%u Sector:%u Data:$%02X",
                entry.fdc.command, command, drive, entry.fdc.cylinder, entry.fdc.track, entry.fdc.sector,
                entry.fdc.data);
            break;
        }
        case TRACE_FDC_END:
        {
            char command[48];
            char status[96] = "";
            u8 value = entry.fdc.status;
            bool type1 = (entry.fdc.command & 0x80) == 0 || (entry.fdc.command & 0xF0) == 0xD0;
            const char* const* names = type1 ? k_debug_mb8877_type1_status : k_debug_mb8877_type2_status;

            gui_debug_mb8877_command(entry.fdc.command, command, sizeof(command));

            for (int i = 7; i >= 0; i--)
            {
                if ((value & (1 << i)) == 0 || i == 0)
                    continue;

                strncat(status, " ", sizeof(status) - strlen(status) - 1);
                strncat(status, names[i], sizeof(status) - strlen(status) - 1);
            }

            format_event(buffer, size, "FDC", "END", "%s  Status:$%02X%s  Drive:%s Track:%u Sector:%u", command, value,
                status[0] != 0 ? status : " OK", drive, entry.fdc.track, entry.fdc.sector);
            break;
        }
        case TRACE_FDC_DRIVE_CONTROL:
        {
            u8 value = entry.fdc.value;
            format_event(buffer, size, "FDC", "DRIVE CTRL", "$0208 <- $%02X  Motor:%s Side:%u %s %s IRQ:%s  Drive:%s",
                value, (value & k_fdc_motor) != 0 ? "ON " : "OFF", (value & k_fdc_side) != 0 ? 1 : 0,
                (value & k_fdc_double_density) != 0 ? "MFM" : "FM", (value & k_fdc_slow_clock) != 0 ? "SLOW" : "FAST",
                (value & k_fdc_irq_enable) != 0 ? "ON " : "OFF", drive);
            break;
        }
        default:
            format_event(buffer, size, "FDC", "DRIVE SEL", "Select:$%02X  Switch:%u  Drive:%s  Cylinder:%u",
                entry.fdc.data, entry.fdc.status & 0x01, drive, entry.fdc.cylinder);
            break;
    }
}

static void format_keyboard(const GT_Trace_Entry& entry, char* buffer, size_t size)
{
    u8 value = entry.keyboard.value;

    switch (entry.event)
    {
        case TRACE_KEYBOARD_KEY:
        {
            u8 flags = entry.keyboard.flags;
            const char* name = gui_debug_key_name(entry.keyboard.key);
            const char* kind = (flags & 0xF0) == 0xA0 ? "MAKE  " : (flags & 0xF0) == 0xB0 ? "BREAK " : "REPEAT";

            format_event(buffer, size, "KEYBRD", "KEY", "%s %s  Code:$%02X  Message:%02X %02X%s%s  Queued:%u", kind,
                IsValidPointer(name) ? name : "?", entry.keyboard.key, flags, entry.keyboard.key,
                (flags & 0x08) != 0 ? "  CTRL" : "", (flags & 0x04) != 0 ? "  SHIFT" : "", entry.keyboard.pending);
            break;
        }
        case TRACE_KEYBOARD_READ:
            format_event(buffer, size, "KEYBRD", "READ", "$0600 -> $%02X  Left:%u", value, entry.keyboard.pending);
            break;
        case TRACE_KEYBOARD_COMMAND:
        {
            const char* name = "";

            switch (value)
            {
                case 0xA0:
                case 0xA1:
                case 0xA2: name = "RESET"; break;
                case 0xA9: name = "REPEAT DELAY 400 MS"; break;
                case 0xAA: name = "REPEAT DELAY 500 MS"; break;
                case 0xAB: name = "REPEAT DELAY 300 MS"; break;
                case 0xAC: name = "REPEAT EVERY 50 MS"; break;
                case 0xAD: name = "REPEAT EVERY 30 MS"; break;
                case 0xAE: name = "REPEAT EVERY 20 MS"; break;
                default: break;
            }

            format_event(buffer, size, "KEYBRD", "COMMAND", "$%04X <- $%02X  %s", 0x0600 + entry.keyboard.flags, value,
                name);
            break;
        }
        default:
            format_event(buffer, size, "KEYBRD", "IRQ", "$0604 <- $%02X  IRQ1:%s", value,
                (value & 0x01) != 0 ? "ON " : "OFF");
            break;
    }
}

static void format_input(const GT_Trace_Entry& entry, char* buffer, size_t size)
{
    char port = entry.input.port == 0 ? 'A' : 'B';
    const char* device = k_trace_controller_names[MIN(entry.input.device, (u8)4)];
    u8 output = entry.input.output;

    switch (entry.event)
    {
        case TRACE_INPUT_READ:
        {
            u8 value = entry.input.value;
            format_event(buffer, size, "INPUT", "READ", "PORT %c %-9s -> $%02X  Lines:%c%c%c%c A:%u B:%u COM:%u", port,
                device, value, (value & 0x01) ? '1' : '0', (value & 0x02) ? '1' : '0', (value & 0x04) ? '1' : '0',
                (value & 0x08) ? '1' : '0', (value >> 4) & 0x01, (value >> 5) & 0x01, (value >> 6) & 0x01);
            break;
        }
        case TRACE_INPUT_WRITE:
            format_event(buffer, size, "INPUT", "WRITE", "$04D6 <- $%02X  TRIG A:%u%u B:%u%u  COM A:%u B:%u  Was:$%02X",
                output, output & 0x01, (output >> 1) & 0x01, (output >> 2) & 0x01, (output >> 3) & 0x01,
                (output >> 4) & 0x01, (output >> 5) & 0x01, entry.input.previous & 0xFF);
            break;
        default:
        {
            char pressed[96];
            char released[96];
            u16 buttons = entry.input.buttons;
            u16 previous = entry.input.previous;

            format_buttons(buttons & ~previous, pressed, sizeof(pressed));
            format_buttons(previous & ~buttons, released, sizeof(released));
            format_event(buffer, size, "INPUT", "CHANGE", "PORT %c %-9s Pressed:%s  Released:%s", port, device,
                pressed, released);
            break;
        }
    }
}

static void format_system(const GT_Trace_Entry& entry, char* buffer, size_t size)
{
    u8 value = entry.system.value;
    u8 flags = entry.system.flags;

    switch (entry.event)
    {
        case TRACE_SYSTEM_RESET:
            if (entry.system.port == 0x0020)
                format_event(buffer, size, "SYSTEM", (value & 0x01) != 0 ? "RESET" : "CONTROL",
                    "$0020 <- $%02X%s%s  Write protect:%s", value, (value & 0x01) != 0 ? "  CPU RESET" : "",
                    (value & 0x40) != 0 ? "  POWER OFF" : "", (flags & 0x02) != 0 ? "ON" : "OFF");
            else
                format_event(buffer, size, "SYSTEM", "POWER", "$0022 <- $%02X%s", value,
                    (value & 0x40) != 0 ? "  POWER OFF" : "");
            break;
        case TRACE_SYSTEM_MEMORY_MAP:
            if (entry.system.port == 0x0404)
                format_event(buffer, size, "SYSTEM", "MEMORY MAP", "$0404 <- $%02X  C0000-EFFFF:%s", value,
                    (flags & 0x01) != 0 ? "RAM" : "FM-R VRAM, ROM AND I/O");
            else if (entry.system.port == 0x0480)
                format_event(buffer, size, "SYSTEM", "MEMORY MAP", "$0480 <- $%02X  F8000-FFFFF:%s  Dictionary:%s",
                    value, (flags & 0x02) != 0 ? "RAM" : "BOOT ROM", (flags & 0x04) != 0 ? "ON " : "OFF");
            else
                format_event(buffer, size, "SYSTEM", "MEMORY MAP", "$0484 <- $%02X  Dictionary bank:%u", value,
                    entry.system.address);
            break;
        case TRACE_SYSTEM_RTC_READ:
            format_event(buffer, size, "SYSTEM", "RTC READ", "%-5s -> $%X",
                k_debug_rtc_register_names[entry.system.address & 0x0F], value);
            break;
        default:
            format_event(buffer, size, "SYSTEM", "RTC WRITE", "%-5s <- $%X",
                k_debug_rtc_register_names[entry.system.address & 0x0F], value);
            break;
    }
}

static void format_bit_list(u8 bits, int base, char* buffer, size_t size)
{
    size_t length = 0;
    buffer[0] = 0;

    for (int i = 0; i < 8 && length + 4 < size; i++)
    {
        if ((bits & (1 << i)) == 0)
            continue;

        int written = snprintf(buffer + length, size - length, length == 0 ? "%d" : ",%d", base + i);

        if (written > 0)
            length += (size_t)written;
    }

    if (length == 0)
        snprintf(buffer, size, "NONE");
}

static void format_msf(u32 lba, char* buffer, size_t size)
{
    u32 frames = lba + 150;
    snprintf(buffer, size, "%02u:%02u:%02u", frames / (75 * 60), (frames / 75) % 60, frames % 75);
}

static void format_buttons(u16 buttons, char* buffer, size_t size)
{
    static const char* const k_names[13] =
    {
        "UP", "DOWN", "LEFT", "RIGHT", "SELECT", "RUN", "A", "B", "C", "X", "Y", "Z", "ZOOM"
    };
    size_t length = 0;
    buffer[0] = 0;

    for (int i = 0; i < 13; i++)
    {
        if ((buttons & (1 << i)) == 0)
            continue;

        int written = snprintf(buffer + length, size - length, length == 0 ? "%s" : "+%s", k_names[i]);

        if (written < 0 || (size_t)written >= size - length)
            break;

        length += (size_t)written;
    }

    if (length == 0)
        snprintf(buffer, size, "-");
}
