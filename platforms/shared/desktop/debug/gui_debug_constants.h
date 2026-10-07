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

#ifndef GUI_DEBUG_CONSTANTS_H
#define GUI_DEBUG_CONSTANTS_H

#include "geartowns.h"

struct stDebugPortLabel
{
    u16 port;
    const char* label;
    const char* description;
};

static const stDebugPortLabel k_debug_port_labels[] =
{
    { 0x0000, "PIC_M_CMD", "PIC master ICW1/OCW2/OCW3, IRR/ISR read" },
    { 0x0002, "PIC_M_DATA", "PIC master ICW2-ICW4, IMR" },
    { 0x0010, "PIC_S_CMD", "PIC slave ICW1/OCW2/OCW3, IRR/ISR read" },
    { 0x0012, "PIC_S_DATA", "PIC slave ICW2-ICW4, IMR" },
    { 0x0020, "SYS_RESET", "Reset cause read, reset and write protect" },
    { 0x0022, "SYS_POWER", "Power off" },
    { 0x0030, "MACHINE_ID_LO", "Machine ID low" },
    { 0x0031, "MACHINE_ID_HI", "Machine ID high" },
    { 0x0032, "SERIAL_ROM", "Serial ID ROM" },
    { 0x0040, "PIT0_COUNT0", "PIT counter 0, interval timer" },
    { 0x0042, "PIT0_COUNT1", "PIT counter 1, I/O timeout" },
    { 0x0044, "PIT0_COUNT2", "PIT counter 2, buzzer" },
    { 0x0046, "PIT0_CONTROL", "PIT counters 0-2 control" },
    { 0x0050, "PIT1_COUNT3", "PIT counter 3" },
    { 0x0052, "PIT1_COUNT4", "PIT counter 4, RS-232C baud rate" },
    { 0x0054, "PIT1_COUNT5", "PIT counter 5" },
    { 0x0056, "PIT1_CONTROL", "PIT counters 3-5 control" },
    { 0x0060, "TIMER_CONTROL", "Timer interrupt status and control, buzzer" },
    { 0x0070, "RTC_DATA", "RTC data" },
    { 0x0080, "RTC_COMMAND", "RTC command" },
    { 0x00A0, "DMA_INIT", "DMA initialize" },
    { 0x00A1, "DMA_CHANNEL", "DMA channel select" },
    { 0x00A2, "DMA_COUNT_LO", "DMA count low" },
    { 0x00A3, "DMA_COUNT_HI", "DMA count high" },
    { 0x00A4, "DMA_ADDRESS_0", "DMA address bits 0-7" },
    { 0x00A5, "DMA_ADDRESS_1", "DMA address bits 8-15" },
    { 0x00A6, "DMA_ADDRESS_2", "DMA address bits 16-23" },
    { 0x00A7, "DMA_ADDRESS_3", "DMA address bits 24-31" },
    { 0x00A8, "DMA_DEVICE_LO", "DMA device control low" },
    { 0x00A9, "DMA_DEVICE_HI", "DMA device control high" },
    { 0x00AA, "DMA_MODE", "DMA mode control" },
    { 0x00AB, "DMA_STATUS", "DMA status" },
    { 0x00AC, "DMA_TEMP_LO", "DMA temporary low" },
    { 0x00AD, "DMA_TEMP_HI", "DMA temporary high" },
    { 0x00AE, "DMA_REQUEST", "DMA software request" },
    { 0x00AF, "DMA_MASK", "DMA mask" },
    { 0x0200, "FDC_STATUS", "FDC status read, command write" },
    { 0x0202, "FDC_TRACK", "FDC track" },
    { 0x0204, "FDC_SECTOR", "FDC sector" },
    { 0x0206, "FDC_DATA", "FDC data" },
    { 0x0208, "FDC_DRIVE_CONTROL", "FDC drive status read, drive control write" },
    { 0x020C, "FDC_DRIVE_SELECT", "FDC drive select" },
    { 0x020E, "FDC_DRIVE_SWITCH", "FDC drive switch" },
    { 0x0400, "VIDEO_RESOLUTION", "Resolution status" },
    { 0x0404, "FMR_VRAM_MAP", "FM-R VRAM mapping" },
    { 0x0440, "CRTC_ADDRESS", "CRTC register index" },
    { 0x0442, "CRTC_DATA_LO", "CRTC data low" },
    { 0x0443, "CRTC_DATA_HI", "CRTC data high" },
    { 0x0448, "VIDEO_OUT_ADDRESS", "Video output control register index" },
    { 0x044A, "VIDEO_OUT_DATA", "Video output control data" },
    { 0x044C, "VIDEO_STATUS", "Digital palette and sprite status" },
    { 0x0450, "SPRITE_ADDRESS", "Sprite controller register index" },
    { 0x0452, "SPRITE_DATA", "Sprite controller data" },
    { 0x0458, "VRAM_MASK_ADDRESS", "VRAM write mask register index" },
    { 0x045A, "VRAM_MASK_LO", "VRAM write mask low" },
    { 0x045B, "VRAM_MASK_HI", "VRAM write mask high" },
    { 0x0480, "SYS_ROM_MAP", "Boot ROM and dictionary windows" },
    { 0x0484, "DIC_ROM_BANK", "Dictionary ROM bank" },
    { 0x048A, "MEMCARD_STATUS", "Memory card status" },
    { 0x04C0, "CDROM_MASTER", "CD-ROM master status and control" },
    { 0x04C2, "CDROM_COMMAND", "CD-ROM status read, command write" },
    { 0x04C4, "CDROM_DATA", "CD-ROM data read, parameter write" },
    { 0x04C6, "CDROM_TRANSFER", "CD-ROM transfer control" },
    { 0x04CC, "CDROM_SUBCODE", "CD-ROM subcode status" },
    { 0x04CD, "CDROM_SUBCODE_DATA", "CD-ROM subcode data" },
    { 0x04D0, "PAD_A", "Game port A" },
    { 0x04D2, "PAD_B", "Game port B" },
    { 0x04D5, "SOUND_MUTE", "Sound mute" },
    { 0x04D6, "PAD_OUTPUT", "Game port output control" },
    { 0x04D8, "FM_ADDRESS_0", "YM3438 status read, part 0 address write" },
    { 0x04DA, "FM_DATA_0", "YM3438 part 0 data" },
    { 0x04DC, "FM_ADDRESS_1", "YM3438 part 1 address" },
    { 0x04DE, "FM_DATA_1", "YM3438 part 1 data" },
    { 0x04E0, "EVOL1_DATA", "Electronic volume 1 data" },
    { 0x04E1, "EVOL1_COMMAND", "Electronic volume 1 command" },
    { 0x04E2, "EVOL2_DATA", "Electronic volume 2 data" },
    { 0x04E3, "EVOL2_COMMAND", "Electronic volume 2 command" },
    { 0x04E7, "ADC_DATA", "ADC sample data" },
    { 0x04E8, "ADC_READY", "ADC sample ready" },
    { 0x04E9, "SOUND_IRQ_CAUSE", "Sound interrupt cause" },
    { 0x04EA, "PCM_IRQ_MASK", "PCM interrupt mask" },
    { 0x04EB, "PCM_IRQ_CAUSE", "PCM interrupt cause" },
    { 0x04EC, "SOUND_LED_MUTE", "LED and output mute" },
    { 0x04F0, "PCM_ENV", "RF5C68 envelope" },
    { 0x04F1, "PCM_PAN", "RF5C68 pan" },
    { 0x04F2, "PCM_FD_LO", "RF5C68 step low" },
    { 0x04F3, "PCM_FD_HI", "RF5C68 step high" },
    { 0x04F4, "PCM_LS_LO", "RF5C68 loop start low" },
    { 0x04F5, "PCM_LS_HI", "RF5C68 loop start high" },
    { 0x04F6, "PCM_ST", "RF5C68 start address" },
    { 0x04F7, "PCM_CONTROL", "RF5C68 control" },
    { 0x04F8, "PCM_CHANNEL_ON", "RF5C68 channel enables" },
    { 0x05C0, "EXP_NMI_MASK", "Expansion NMI mask" },
    { 0x05C2, "EXP_NMI_STATUS", "Expansion NMI status" },
    { 0x05C8, "TVRAM_WRITTEN", "Text VRAM written" },
    { 0x05CA, "VSYNC_IRQ_CLEAR", "VSYNC interrupt clear" },
    { 0x05E0, "UNDOCUMENTED", "Undocumented, BIOS writes 01h" },
    { 0x0600, "KB_DATA", "Keyboard data" },
    { 0x0602, "KB_STATUS", "Keyboard status read, command write" },
    { 0x0604, "KB_IRQ", "Keyboard interrupt" },
    { 0x0800, "PRN_DATA", "Printer data, status 1" },
    { 0x0802, "PRN_CONTROL", "Printer control, status 2" },
    { 0x0804, "PRN_IRQ", "Printer interrupt enable" },
    { 0x0A00, "SIO_DATA", "RS-232C data" },
    { 0x0A02, "SIO_STATUS", "RS-232C status read, command write" },
    { 0x0A04, "SIO_MODEM_STATUS", "RS-232C modem status" },
    { 0x0A06, "SIO_IRQ_CAUSE", "RS-232C interrupt cause" },
    { 0x0A08, "SIO_IRQ_CONTROL", "RS-232C interrupt control" },
    { 0x0A0A, "SIO_MODEM_CONTROL", "RS-232C modem control" },
    { 0x0C30, "SCSI_DATA", "SCSI data" },
    { 0x0C32, "SCSI_STATUS", "SCSI status read, control write" },
    { 0xFD90, "PAL_INDEX", "Palette index" },
    { 0xFD92, "PAL_BLUE", "Palette blue" },
    { 0xFD94, "PAL_RED", "Palette red" },
    { 0xFD96, "PAL_GREEN", "Palette green" },
    { 0xFD98, "DPAL_0", "FM-R digital palette 0" },
    { 0xFD99, "DPAL_1", "FM-R digital palette 1" },
    { 0xFD9A, "DPAL_2", "FM-R digital palette 2" },
    { 0xFD9B, "DPAL_3", "FM-R digital palette 3" },
    { 0xFD9C, "DPAL_4", "FM-R digital palette 4" },
    { 0xFD9D, "DPAL_5", "FM-R digital palette 5" },
    { 0xFD9E, "DPAL_6", "FM-R digital palette 6" },
    { 0xFD9F, "DPAL_7", "FM-R digital palette 7" },
    { 0xFDA0, "CRT_OUTPUT", "Sync status read, CRT output control write" },
    { 0xFF81, "FMR_PLANE_MASK", "FM-R plane access mask" },
    { 0xFF82, "FMR_DISPLAY", "FM-R display planes and page" },
    { 0xFF83, "FMR_PAGE", "FM-R access page" },
    { 0xFF84, "FMR_LIGHT_PEN", "FM-R light pen status" },
    { 0xFF86, "FMR_SYNC", "FM-R sync status" },
    { 0xFF94, "KANJI_HI", "Kanji ROM code high, status read" },
    { 0xFF95, "KANJI_LO", "Kanji ROM code low" },
    { 0xFF96, "KANJI_LEFT", "Kanji ROM pattern left" },
    { 0xFF97, "KANJI_RIGHT", "Kanji ROM pattern right, row advance" },
    { 0xFF98, "BUZZER", "Buzzer on read, off write" },
    { 0xFF99, "FMR_ANK", "ANK font window" },
    { 0xFFA0, "FMR_LOGIC", "FM-R logical operation status" }
};

static const int k_debug_port_label_count = sizeof(k_debug_port_labels) / sizeof(k_debug_port_labels[0]);

struct stDebugInterruptName
{
    u8 vector;
    const char* label;
    const char* description;
};

// Software interrupts of DOS and the real mode BIOS of the FM Towns
static const stDebugInterruptName k_debug_interrupt_names[] =
{
    { 0x20, "DOS_EXIT", "DOS terminate program" },
    { 0x21, "DOS", "DOS function call" },
    { 0x22, "DOS_EXIT_ADDR", "DOS terminate address" },
    { 0x23, "DOS_CTRL_C", "DOS Ctrl-C handler" },
    { 0x24, "DOS_CRIT_ERROR", "DOS critical error handler" },
    { 0x25, "DOS_DISK_READ", "DOS absolute disk read" },
    { 0x26, "DOS_DISK_WRITE", "DOS absolute disk write" },
    { 0x27, "DOS_TSR", "DOS terminate and stay resident" },
    { 0x28, "DOS_IDLE", "DOS idle" },
    { 0x29, "DOS_FAST_OUT", "DOS fast console output" },
    { 0x2F, "MULTIPLEX", "DOS multiplex, MSCDEX" },
    { 0x8E, "EXT_SERVICE", "Extended service routines" },
    { 0x90, "KEYBOARD_BIOS", "Keyboard BIOS" },
    { 0x93, "DISK_BIOS", "Disk and CD-ROM BIOS" },
    { 0x94, "PRINTER_BIOS", "Printer BIOS" },
    { 0x96, "CALENDAR_BIOS", "Calendar clock BIOS" },
    { 0x97, "TIMER_BIOS", "Timer manager BIOS" },
    { 0x98, "ALARM_BIOS", "Clock and alarm manager BIOS" },
    { 0x9B, "RS232C_BIOS", "RS-232C BIOS" },
    { 0x9E, "BUZZER_BIOS", "Buzzer BIOS" },
    { 0xAE, "INT_MANAGER", "General interrupt manager" },
    { 0xAF, "SERVICE_BIOS", "Service routines" }
};

static const int k_debug_interrupt_name_count = sizeof(k_debug_interrupt_names) / sizeof(k_debug_interrupt_names[0]);

struct stDebugInterruptFunction
{
    u8 vector;
    u16 mask;
    u16 value;
    const char* name;
};

// Functions picked by AX, the first match wins: the CD-ROM device codes C0h-CFh come before the disk entries
static const stDebugInterruptFunction k_debug_interrupt_functions[] =
{
    { 0x21, 0xFF00, 0x0000, "Terminate program" },
    { 0x21, 0xFF00, 0x0100, "Read character with echo" },
    { 0x21, 0xFF00, 0x0200, "Write character" },
    { 0x21, 0xFF00, 0x0300, "Read auxiliary input" },
    { 0x21, 0xFF00, 0x0400, "Write auxiliary output" },
    { 0x21, 0xFF00, 0x0500, "Write to printer" },
    { 0x21, 0xFF00, 0x0600, "Direct console input or output" },
    { 0x21, 0xFF00, 0x0700, "Read character, no echo or filter" },
    { 0x21, 0xFF00, 0x0800, "Read character without echo" },
    { 0x21, 0xFF00, 0x0900, "Write string" },
    { 0x21, 0xFF00, 0x0A00, "Buffered keyboard input" },
    { 0x21, 0xFF00, 0x0B00, "Check input status" },
    { 0x21, 0xFF00, 0x0C00, "Flush input buffer and read" },
    { 0x21, 0xFF00, 0x0D00, "Disk reset" },
    { 0x21, 0xFF00, 0x0E00, "Select default drive" },
    { 0x21, 0xFF00, 0x0F00, "Open file (FCB)" },
    { 0x21, 0xFF00, 0x1000, "Close file (FCB)" },
    { 0x21, 0xFF00, 0x1100, "Find first file (FCB)" },
    { 0x21, 0xFF00, 0x1200, "Find next file (FCB)" },
    { 0x21, 0xFF00, 0x1300, "Delete file (FCB)" },
    { 0x21, 0xFF00, 0x1400, "Sequential read (FCB)" },
    { 0x21, 0xFF00, 0x1500, "Sequential write (FCB)" },
    { 0x21, 0xFF00, 0x1600, "Create file (FCB)" },
    { 0x21, 0xFF00, 0x1700, "Rename file (FCB)" },
    { 0x21, 0xFF00, 0x1900, "Get default drive" },
    { 0x21, 0xFF00, 0x1A00, "Set disk transfer address" },
    { 0x21, 0xFF00, 0x1B00, "Get default drive information" },
    { 0x21, 0xFF00, 0x1C00, "Get drive information" },
    { 0x21, 0xFF00, 0x2100, "Random read (FCB)" },
    { 0x21, 0xFF00, 0x2200, "Random write (FCB)" },
    { 0x21, 0xFF00, 0x2300, "Get file size (FCB)" },
    { 0x21, 0xFF00, 0x2400, "Set random record (FCB)" },
    { 0x21, 0xFF00, 0x2500, "Set interrupt vector, DOS extender call" },
    { 0x21, 0xFF00, 0x2600, "Create program segment prefix" },
    { 0x21, 0xFF00, 0x2700, "Random block read (FCB)" },
    { 0x21, 0xFF00, 0x2800, "Random block write (FCB)" },
    { 0x21, 0xFF00, 0x2900, "Parse file name" },
    { 0x21, 0xFF00, 0x2A00, "Get date" },
    { 0x21, 0xFF00, 0x2B00, "Set date" },
    { 0x21, 0xFF00, 0x2C00, "Get time" },
    { 0x21, 0xFF00, 0x2D00, "Set time" },
    { 0x21, 0xFF00, 0x2E00, "Set verify flag" },
    { 0x21, 0xFF00, 0x2F00, "Get disk transfer address" },
    { 0x21, 0xFF00, 0x3000, "Get DOS version" },
    { 0x21, 0xFF00, 0x3100, "Terminate and stay resident" },
    { 0x21, 0xFF00, 0x3300, "Get or set Ctrl-Break checking" },
    { 0x21, 0xFF00, 0x3400, "Get InDOS flag address" },
    { 0x21, 0xFF00, 0x3500, "Get interrupt vector" },
    { 0x21, 0xFF00, 0x3600, "Get free disk space" },
    { 0x21, 0xFF00, 0x3800, "Get or set country information" },
    { 0x21, 0xFF00, 0x3900, "Create directory" },
    { 0x21, 0xFF00, 0x3A00, "Remove directory" },
    { 0x21, 0xFF00, 0x3B00, "Change directory" },
    { 0x21, 0xFF00, 0x3C00, "Create file" },
    { 0x21, 0xFF00, 0x3D00, "Open file" },
    { 0x21, 0xFF00, 0x3E00, "Close file" },
    { 0x21, 0xFF00, 0x3F00, "Read from file or device" },
    { 0x21, 0xFF00, 0x4000, "Write to file or device" },
    { 0x21, 0xFF00, 0x4100, "Delete file" },
    { 0x21, 0xFF00, 0x4200, "Move file pointer" },
    { 0x21, 0xFF00, 0x4300, "Get or set file attributes" },
    { 0x21, 0xFF00, 0x4400, "Device I/O control" },
    { 0x21, 0xFF00, 0x4500, "Duplicate file handle" },
    { 0x21, 0xFF00, 0x4600, "Redirect file handle" },
    { 0x21, 0xFF00, 0x4700, "Get current directory" },
    { 0x21, 0xFF00, 0x4800, "Allocate memory" },
    { 0x21, 0xFF00, 0x4900, "Free memory" },
    { 0x21, 0xFF00, 0x4A00, "Resize memory block" },
    { 0x21, 0xFF00, 0x4B00, "Load or execute program" },
    { 0x21, 0xFF00, 0x4C00, "Terminate with return code" },
    { 0x21, 0xFF00, 0x4D00, "Get return code" },
    { 0x21, 0xFF00, 0x4E00, "Find first file" },
    { 0x21, 0xFF00, 0x4F00, "Find next file" },
    { 0x21, 0xFF00, 0x5000, "Set current process" },
    { 0x21, 0xFF00, 0x5100, "Get current process" },
    { 0x21, 0xFF00, 0x5200, "Get DOS internal tables" },
    { 0x21, 0xFF00, 0x5400, "Get verify flag" },
    { 0x21, 0xFF00, 0x5600, "Rename file" },
    { 0x21, 0xFF00, 0x5700, "Get or set file date and time" },
    { 0x21, 0xFF00, 0x5800, "Get or set memory strategy" },
    { 0x21, 0xFF00, 0x5900, "Get extended error" },
    { 0x21, 0xFF00, 0x5A00, "Create temporary file" },
    { 0x21, 0xFF00, 0x5B00, "Create new file" },
    { 0x21, 0xFF00, 0x5C00, "Lock or unlock file region" },
    { 0x21, 0xFF00, 0x5E00, "Network machine and printer" },
    { 0x21, 0xFF00, 0x5F00, "Network redirection" },
    { 0x21, 0xFF00, 0x6200, "Get current process" },
    { 0x21, 0xFF00, 0x6500, "Get extended country information" },
    { 0x21, 0xFF00, 0x6600, "Get or set code page" },
    { 0x21, 0xFF00, 0x6700, "Set file handle count" },
    { 0x21, 0xFF00, 0x6800, "Commit file" },
    { 0x21, 0xFF00, 0x6C00, "Extended open or create" },
    { 0x2F, 0xFFFF, 0x1500, "MSCDEX: get CD-ROM drive count" },
    { 0x2F, 0xFFFF, 0x1501, "MSCDEX: get drive device list" },
    { 0x2F, 0xFFFF, 0x1502, "MSCDEX: get copyright file name" },
    { 0x2F, 0xFFFF, 0x1503, "MSCDEX: get abstract file name" },
    { 0x2F, 0xFFFF, 0x1504, "MSCDEX: get bibliographic file name" },
    { 0x2F, 0xFFFF, 0x1505, "MSCDEX: read volume descriptor" },
    { 0x2F, 0xFFFF, 0x1506, "MSCDEX: debugging on" },
    { 0x2F, 0xFFFF, 0x1507, "MSCDEX: debugging off" },
    { 0x2F, 0xFFFF, 0x1508, "MSCDEX: absolute disk read" },
    { 0x2F, 0xFFFF, 0x1509, "MSCDEX: absolute disk write" },
    { 0x2F, 0xFFFF, 0x150B, "MSCDEX: check CD-ROM drive" },
    { 0x2F, 0xFFFF, 0x150C, "MSCDEX: get version" },
    { 0x2F, 0xFFFF, 0x150D, "MSCDEX: get drive letters" },
    { 0x2F, 0xFFFF, 0x150E, "MSCDEX: volume descriptor preference" },
    { 0x2F, 0xFFFF, 0x150F, "MSCDEX: get directory entry" },
    { 0x2F, 0xFFFF, 0x1510, "MSCDEX: send device request" },
    { 0x8E, 0xFF00, 0x0000, "Get system information area" },
    { 0x8E, 0xFF00, 0x0100, "Set cut-sheet feeder control" },
    { 0x90, 0xFF00, 0x0000, "Initialize" },
    { 0x90, 0xFF00, 0x0100, "Set input buffering" },
    { 0x90, 0xFF00, 0x0200, "Set mode, code system and masks" },
    { 0x90, 0xFF00, 0x0300, "Get mode, code system and masks" },
    { 0x90, 0xFF00, 0x0400, "Lock or unlock keyboard" },
    { 0x90, 0xFF00, 0x0500, "Key click control" },
    { 0x90, 0xFF00, 0x0600, "Clear input buffer" },
    { 0x90, 0xFF00, 0x0700, "Peek input" },
    { 0x90, 0xFF00, 0x0800, "Get shift state" },
    { 0x90, 0xFF00, 0x0900, "Read character" },
    { 0x90, 0xFF00, 0x0A00, "Read key matrix" },
    { 0x90, 0xFF00, 0x0B00, "Insert character string" },
    { 0x90, 0xFF00, 0x0C00, "Set PF key routine" },
    { 0x90, 0xFF00, 0x0D00, "Get PF key routine" },
    { 0x90, 0xFF00, 0x0E00, "Set key replacement string" },
    { 0x90, 0xFF00, 0x0F00, "Get key replacement string" },
    { 0x93, 0xFFF0, 0x00C0, "CD-ROM: set drive mode" },
    { 0x93, 0xFFF0, 0x01C0, "CD-ROM: get drive mode" },
    { 0x93, 0xFFF0, 0x02C0, "CD-ROM: get drive status" },
    { 0x93, 0xFFF0, 0x03C0, "CD-ROM: restore" },
    { 0x93, 0xFFF0, 0x04C0, "CD-ROM: seek by sector" },
    { 0x93, 0xFFF0, 0x05C0, "CD-ROM: read by sector" },
    { 0x93, 0xFFF0, 0x14C0, "CD-ROM: seek by time" },
    { 0x93, 0xFFF0, 0x15C0, "CD-ROM: read by time" },
    { 0x93, 0xFFF0, 0x50C0, "CD-ROM: play audio" },
    { 0x93, 0xFFF0, 0x51C0, "CD-ROM: get audio range" },
    { 0x93, 0xFFF0, 0x52C0, "CD-ROM: stop audio" },
    { 0x93, 0xFFF0, 0x53C0, "CD-ROM: get audio state" },
    { 0x93, 0xFFF0, 0x54C0, "CD-ROM: read table of contents" },
    { 0x93, 0xFFF0, 0x55C0, "CD-ROM: pause audio" },
    { 0x93, 0xFFF0, 0x56C0, "CD-ROM: resume audio" },
    { 0x93, 0xFF00, 0x0000, "Disk: set floppy mode" },
    { 0x93, 0xFF00, 0x0100, "Disk: get floppy mode" },
    { 0x93, 0xFF00, 0x0200, "Disk: get drive status" },
    { 0x93, 0xFF00, 0x0300, "Disk: seek cylinder 0" },
    { 0x93, 0xFF00, 0x0400, "Disk: seek cylinder" },
    { 0x93, 0xFF00, 0x0500, "Disk: read sectors" },
    { 0x93, 0xFF00, 0x0600, "Disk: write sectors" },
    { 0x93, 0xFF00, 0x0700, "Disk: verify sectors" },
    { 0x93, 0xFF00, 0x0800, "Disk: reset hard disk controller" },
    { 0x93, 0xFF00, 0x0900, "Disk: read sector ID" },
    { 0x93, 0xFF00, 0x0A00, "Disk: format track" },
    { 0x93, 0xFF00, 0x0D00, "Disk: get hard disk error details" },
    { 0x94, 0xFF00, 0x0000, "Get printer status" },
    { 0x94, 0xFF00, 0x0100, "Print byte" },
    { 0x94, 0xFF00, 0x0200, "Print string" },
    { 0x96, 0xFF00, 0x0000, "Set date and time" },
    { 0x96, 0xFF00, 0x0100, "Get date and time" },
    { 0x97, 0xFF00, 0x0000, "Register timer" },
    { 0x97, 0xFF00, 0x0100, "Cancel timer" },
    { 0x97, 0xFF00, 0x0200, "Get timer count" },
    { 0x98, 0xFF00, 0x0000, "Register alarm" },
    { 0x98, 0xFF00, 0x0100, "Cancel alarm" },
    { 0x9B, 0xFF00, 0x0000, "Detect serial ports" },
    { 0x9B, 0xFF00, 0x0100, "Open port" },
    { 0x9B, 0xFF00, 0x0200, "Close port" },
    { 0x9B, 0xFF00, 0x0300, "Set line parameters" },
    { 0x9B, 0xFF00, 0x0400, "Get line parameters" },
    { 0x9B, 0xFF00, 0x0500, "Get received byte count" },
    { 0x9B, 0xFF00, 0x0600, "Receive byte" },
    { 0x9B, 0xFF00, 0x0700, "Send byte" },
    { 0x9B, 0xFF00, 0x0800, "Set DTR and RTS" },
    { 0x9B, 0xFF00, 0x0900, "Get line state" },
    { 0x9B, 0xFF00, 0x0A00, "Clear receive buffer" },
    { 0x9B, 0xFF00, 0x0B00, "Send break" },
    { 0x9B, 0xFF00, 0x0C00, "Set CTS and CI routines" },
    { 0x9B, 0xFF00, 0x0D00, "Get CTS and CI routines" },
    { 0x9B, 0xFF00, 0x0E00, "Keep DTR on close" },
    { 0x9B, 0xFF00, 0x0F00, "Clear XOFF state" },
    { 0x9B, 0xFF00, 0x1000, "Get queued transmit count" },
    { 0x9E, 0xFF00, 0x0000, "Tone on" },
    { 0x9E, 0xFF00, 0x0100, "Tone off" },
    { 0x9E, 0xFF00, 0x0200, "Beep" },
    { 0x9E, 0xFF00, 0x0300, "Tone by divider" },
    { 0x9E, 0xFF00, 0x0400, "Get divider tone state" },
    { 0x9E, 0xFF00, 0x0500, "Tone by frequency" },
    { 0x9E, 0xFF00, 0x0600, "Get frequency tone state" },
    { 0xAE, 0xFF00, 0x0000, "Set cause data block" },
    { 0xAE, 0xFF00, 0x0100, "Get cause data block" },
    { 0xAE, 0xFF00, 0x0200, "Set enable data" },
    { 0xAE, 0xFF00, 0x0300, "Get enable data" },
    { 0xAE, 0xFF00, 0x0400, "Get data block table" },
    { 0xAF, 0xFF00, 0x0000, "JIS to Shift-JIS" },
    { 0xAF, 0xFF00, 0x0100, "Shift-JIS to JIS" },
    { 0xAF, 0xFF00, 0x0200, "Get CPU type" },
    { 0xAF, 0xFF00, 0x0300, "JIS to Shift-JIS, bytes swapped" },
    { 0xAF, 0xFF00, 0x0400, "Shift-JIS to JIS, bytes swapped" },
    { 0xAF, 0xFF00, 0x0500, "Get machine information" }
};

static const int k_debug_interrupt_function_count =
    sizeof(k_debug_interrupt_functions) / sizeof(k_debug_interrupt_functions[0]);

static const char* const k_debug_irq_sources[16] =
{
    "TIMER", "KEYBOARD", "RS-232C", "EXT RS-232C", "I/O EXPANSION", "I/O EXPANSION", "FDC", "SLAVE PIC",
    "SCSI", "CD-ROM", "EXPANSION", "VSYNC", "PRINTER", "SOUND", "EXPANSION", "RESERVED"
};

static const char* const k_debug_dma_devices[4] =
{
    "FDC", "SCSI", "PRINTER", "CD-ROM"
};

static const char* const k_debug_pit_uses[6] =
{
    "INTERVAL TIMER", "I/O TIMEOUT", "BUZZER", "RESERVED", "RS-232C BAUD", "RESERVED"
};

static const char* const k_debug_exception_names[32] =
{
    "#DE", "#DB", "NMI", "#BP", "#OF", "#BR", "#UD", "#NM",
    "#DF", "CSO", "#TS", "#NP", "#SS", "#GP", "#PF", "--",
    "#MF", "--", "--", "--", "--", "--", "--", "--",
    "--", "--", "--", "--", "--", "--", "--", "--"
};

static const char* const k_debug_exception_descriptions[32] =
{
    "Divide error", "Debug", "Non-maskable interrupt", "Breakpoint", "Overflow", "Bound range exceeded",
    "Invalid opcode", "Coprocessor not available", "Double fault", "Coprocessor segment overrun", "Invalid TSS",
    "Segment not present", "Stack fault", "General protection", "Page fault", "Reserved", "Coprocessor error",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved"
};

static const char* const k_debug_pit_mode_names[6] =
{
    "INTERRUPT ON TC", "ONE-SHOT", "RATE GENERATOR", "SQUARE WAVE", "SOFTWARE STROBE", "HARDWARE STROBE"
};

static const char* const k_debug_pit_access_names[4] =
{
    "LATCH", "LSB", "MSB", "LSB+MSB"
};

static const char* const k_debug_pic_init_names[4] =
{
    "READY", "ICW2", "ICW3", "ICW4"
};

static const char* const k_debug_dma_direction_names[4] =
{
    "VERIFY", "I/O>MEM", "MEM>I/O", "INVALID"
};

static const char* const k_debug_dma_service_names[4] =
{
    "DEMAND", "SINGLE", "BLOCK", "CASCADE"
};

static const char* const k_debug_dma_control_names[10] =
{
    "MTM", "AHLD", "DDMA", "CMP", "ROT", "EXW", "RQL", "AKL", "BHLD", "WEV"
};

static const char* const k_debug_rtc_register_names[16] =
{
    "S1", "S10", "MI1", "MI10", "H1", "H10", "W", "D1", "D10", "MO1", "MO10", "Y1", "Y10", "RESET", "REF", "REF"
};

static const char* const k_debug_weekday_names[7] =
{
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

static const char* const k_debug_crtc_register_names[32] =
{
    "HSW1", "HSW2", "--", "--", "HST", "VST1", "VST2", "EET", "VST", "HDS0", "HDE0", "HDS1", "HDE1", "VDS0", "VDE0", "VDS1",
    "VDE1", "FA0", "HAJ0", "FO0", "LO0", "FA1", "HAJ1", "FO1", "LO1", "EHAJ", "EVAJ", "ZOOM", "CR0", "CR1", "FR", "CR2"
};

static const char* const k_debug_layer_format_names[4] =
{
    "OFF", "16 COLORS", "256 COLORS", "32K COLORS"
};

static const double k_debug_crtc_clocks[4] = { 28.636364, 24.545455, 25.175, 21.0525 };

struct stDebugKeyName
{
    GT_Keys key;
    const char* name;
};

static const stDebugKeyName k_debug_key_names[] =
{
    { GT_KEY_ESCAPE, "ESCAPE" },
    { GT_KEY_1, "1" },
    { GT_KEY_2, "2" },
    { GT_KEY_3, "3" },
    { GT_KEY_4, "4" },
    { GT_KEY_5, "5" },
    { GT_KEY_6, "6" },
    { GT_KEY_7, "7" },
    { GT_KEY_8, "8" },
    { GT_KEY_9, "9" },
    { GT_KEY_0, "0" },
    { GT_KEY_MINUS, "MINUS" },
    { GT_KEY_CARET, "CARET" },
    { GT_KEY_YEN, "YEN" },
    { GT_KEY_BACKSPACE, "BACKSPACE" },
    { GT_KEY_TAB, "TAB" },
    { GT_KEY_Q, "Q" },
    { GT_KEY_W, "W" },
    { GT_KEY_E, "E" },
    { GT_KEY_R, "R" },
    { GT_KEY_T, "T" },
    { GT_KEY_Y, "Y" },
    { GT_KEY_U, "U" },
    { GT_KEY_I, "I" },
    { GT_KEY_O, "O" },
    { GT_KEY_P, "P" },
    { GT_KEY_AT, "AT" },
    { GT_KEY_LEFT_BRACKET, "LEFT_BRACKET" },
    { GT_KEY_RETURN, "RETURN" },
    { GT_KEY_A, "A" },
    { GT_KEY_S, "S" },
    { GT_KEY_D, "D" },
    { GT_KEY_F, "F" },
    { GT_KEY_G, "G" },
    { GT_KEY_H, "H" },
    { GT_KEY_J, "J" },
    { GT_KEY_K, "K" },
    { GT_KEY_L, "L" },
    { GT_KEY_SEMICOLON, "SEMICOLON" },
    { GT_KEY_COLON, "COLON" },
    { GT_KEY_RIGHT_BRACKET, "RIGHT_BRACKET" },
    { GT_KEY_Z, "Z" },
    { GT_KEY_X, "X" },
    { GT_KEY_C, "C" },
    { GT_KEY_V, "V" },
    { GT_KEY_B, "B" },
    { GT_KEY_N, "N" },
    { GT_KEY_M, "M" },
    { GT_KEY_COMMA, "COMMA" },
    { GT_KEY_PERIOD, "PERIOD" },
    { GT_KEY_SLASH, "SLASH" },
    { GT_KEY_UNDERSCORE, "UNDERSCORE" },
    { GT_KEY_SPACE, "SPACE" },
    { GT_KEY_KP_MULTIPLY, "KP_MULTIPLY" },
    { GT_KEY_KP_DIVIDE, "KP_DIVIDE" },
    { GT_KEY_KP_PLUS, "KP_PLUS" },
    { GT_KEY_KP_MINUS, "KP_MINUS" },
    { GT_KEY_KP_7, "KP_7" },
    { GT_KEY_KP_8, "KP_8" },
    { GT_KEY_KP_9, "KP_9" },
    { GT_KEY_KP_EQUALS, "KP_EQUALS" },
    { GT_KEY_KP_4, "KP_4" },
    { GT_KEY_KP_5, "KP_5" },
    { GT_KEY_KP_6, "KP_6" },
    { GT_KEY_KP_1, "KP_1" },
    { GT_KEY_KP_2, "KP_2" },
    { GT_KEY_KP_3, "KP_3" },
    { GT_KEY_KP_ENTER, "KP_ENTER" },
    { GT_KEY_KP_0, "KP_0" },
    { GT_KEY_KP_PERIOD, "KP_PERIOD" },
    { GT_KEY_INSERT, "INSERT" },
    { GT_KEY_KP_000, "KP_000" },
    { GT_KEY_DELETE, "DELETE" },
    { GT_KEY_UP, "UP" },
    { GT_KEY_HOME, "HOME" },
    { GT_KEY_LEFT, "LEFT" },
    { GT_KEY_DOWN, "DOWN" },
    { GT_KEY_RIGHT, "RIGHT" },
    { GT_KEY_CTRL, "CTRL" },
    { GT_KEY_SHIFT, "SHIFT" },
    { GT_KEY_CAPS, "CAPS" },
    { GT_KEY_HIRAGANA, "HIRAGANA" },
    { GT_KEY_NO_CONVERT, "NO_CONVERT" },
    { GT_KEY_CONVERT, "CONVERT" },
    { GT_KEY_KANA_KANJI, "KANA_KANJI" },
    { GT_KEY_KATAKANA, "KATAKANA" },
    { GT_KEY_PF12, "PF12" },
    { GT_KEY_ALT, "ALT" },
    { GT_KEY_PF1, "PF1" },
    { GT_KEY_PF2, "PF2" },
    { GT_KEY_PF3, "PF3" },
    { GT_KEY_PF4, "PF4" },
    { GT_KEY_PF5, "PF5" },
    { GT_KEY_PF6, "PF6" },
    { GT_KEY_PF7, "PF7" },
    { GT_KEY_PF8, "PF8" },
    { GT_KEY_PF9, "PF9" },
    { GT_KEY_PF10, "PF10" },
    { GT_KEY_PF11, "PF11" },
    { GT_KEY_KANJI_DICTIONARY, "KANJI_DICTIONARY" },
    { GT_KEY_DELETE_WORD, "DELETE_WORD" },
    { GT_KEY_ADD_WORD, "ADD_WORD" },
    { GT_KEY_PREVIOUS, "PREVIOUS" },
    { GT_KEY_NEXT, "NEXT" },
    { GT_KEY_HALF_FULL, "HALF_FULL" },
    { GT_KEY_CANCEL, "CANCEL" },
    { GT_KEY_EXECUTE, "EXECUTE" },
    { GT_KEY_PF13, "PF13" },
    { GT_KEY_PF14, "PF14" },
    { GT_KEY_PF15, "PF15" },
    { GT_KEY_PF16, "PF16" },
    { GT_KEY_PF17, "PF17" },
    { GT_KEY_PF18, "PF18" },
    { GT_KEY_PF19, "PF19" },
    { GT_KEY_PF20, "PF20" },
    { GT_KEY_BREAK, "BREAK" },
    { GT_KEY_COPY, "COPY" }
};

static const int k_debug_key_name_count = sizeof(k_debug_key_names) / sizeof(k_debug_key_names[0]);

static inline const stDebugPortLabel* gui_debug_find_port_label(u16 port)
{
    for (int i = 0; i < k_debug_port_label_count; i++)
    {
        if (k_debug_port_labels[i].port == port)
            return &k_debug_port_labels[i];
    }

    return NULL;
}

static inline const char* gui_debug_port_label(u16 port)
{
    const stDebugPortLabel* label = gui_debug_find_port_label(port);

    if (IsValidPointer(label))
        return label->label;

    // CMOS RAM sits on the even ports of 3000h-3FFEh
    if ((port & 0xF001) == 0x3000)
        return "CMOS";

    return NULL;
}

static inline const char* gui_debug_key_name(int key)
{
    for (int i = 0; i < k_debug_key_name_count; i++)
    {
        if (k_debug_key_names[i].key == key)
            return k_debug_key_names[i].name;
    }

    return NULL;
}

static inline const char* gui_debug_cdrom_command_name(u8 command)
{
    switch (command)
    {
        case 0x00: return "SEEK";
        case 0x01: return "MODE2 READ";
        case 0x02: return "MODE1 READ";
        case 0x03: return "RAW READ";
        case 0x04: return "CDDA PLAY";
        case 0x05: return "TOC READ";
        case 0x06: return "SUBQ READ";
        case 0x80: return "GET STATE";
        case 0x81: return "CDDA SET";
        case 0x84: return "CDDA STOP";
        case 0x85: return "CDDA PAUSE";
        case 0x87: return "CDDA RESUME";
        default: return "UNKNOWN";
    }
}

static const char* const k_debug_mb8877_type1_status[8] =
{
    "BUSY", "INDEX", "TRACK 00", "CRC ERROR", "SEEK ERROR", "HEAD LOADED", "WRITE PROTECT", "NOT READY"
};

static const char* const k_debug_mb8877_type2_status[8] =
{
    "BUSY", "DRQ", "LOST DATA", "CRC ERROR", "RECORD NOT FOUND", "RECORD TYPE", "WRITE PROTECT", "NOT READY"
};

static const char* const k_debug_floppy_media_names[3] = { "2D", "2DD", "2HD" };

static inline void gui_debug_mb8877_command(u8 command, char* text, size_t size)
{
    static const char* const k_rates[4] = { "6MS", "12MS", "20MS", "30MS" };

    switch (command >> 4)
    {
        case 0x0:
        case 0x1:
            snprintf(text, size, "%s h=%d V=%d r=%s", (command >> 4) == 0 ? "RESTORE" : "SEEK", (command >> 3) & 1,
                (command >> 2) & 1, k_rates[command & 3]);
            break;
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
        {
            const char* name = (command >> 5) == 1 ? "STEP" : (command >> 5) == 2 ? "STEP IN" : "STEP OUT";
            snprintf(text, size, "%s u=%d h=%d V=%d r=%s", name, (command >> 4) & 1, (command >> 3) & 1,
                (command >> 2) & 1, k_rates[command & 3]);
            break;
        }
        case 0x8:
        case 0x9:
            snprintf(text, size, "READ SECTOR m=%d S=%d E=%d C=%d", (command >> 4) & 1, (command >> 3) & 1,
                (command >> 2) & 1, (command >> 1) & 1);
            break;
        case 0xA:
        case 0xB:
            snprintf(text, size, "WRITE SECTOR m=%d S=%d E=%d C=%d a0=%d", (command >> 4) & 1, (command >> 3) & 1,
                (command >> 2) & 1, (command >> 1) & 1, command & 1);
            break;
        case 0xC:
            snprintf(text, size, "READ ADDRESS E=%d", (command >> 2) & 1);
            break;
        case 0xD:
            snprintf(text, size, "FORCE INTERRUPT I=%X", command & 0x0F);
            break;
        case 0xE:
            snprintf(text, size, "READ TRACK E=%d", (command >> 2) & 1);
            break;
        default:
            snprintf(text, size, "WRITE TRACK E=%d", (command >> 2) & 1);
            break;
    }
}

static inline const char* gui_debug_floppy_status_name(u8 status)
{
    switch (status)
    {
        case 0x00: return "OK";
        case 0x10: return "OK";
        case 0xA0: return "ID CRC";
        case 0xB0: return "DATA CRC";
        case 0xE0: return "NO ID";
        case 0xF0: return "NO DATA";
        default: return "ERROR";
    }
}

static const u8 k_debug_ym3438_lfo_cycles[8] = { 108, 77, 71, 67, 62, 44, 8, 5 };
static const char* const k_debug_ym3438_ch3_mode_names[4] = { "NORMAL ", "SPECIAL", "CSM    ", "SPECIAL" };
static const char* const k_debug_ym3438_envelope_names[4] = { "ATTACK ", "DECAY  ", "SUSTAIN", "RELEASE" };
static const char* const k_debug_note_names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// Register order within a channel block is OP1, OP3, OP2, OP4
static const int k_debug_ym3438_register_slot[4] = { 1, 3, 2, 4 };

static inline bool gui_debug_ym3438_register_name(int part, u8 address, char* name, size_t size)
{
    static const char* const k_mode_names[16] =
    {
        NULL, "LSI TEST", "LFO", NULL, "TIMER A MSB", "TIMER A LSB", "TIMER B", "TIMER CONTROL / CH3 MODE",
        "KEY ON/OFF", NULL, "DAC DATA", "DAC ENABLE", "LSI TEST 2", NULL, NULL, NULL
    };
    static const char* const k_operator_names[7] = { "DT / MUL", "TL", "KS / AR", "AM / D1R", "D2R", "D1L / RR", "SSG-EG" };
    static const char* const k_channel_names[6] =
    {
        "F-NUMBER LSB", "BLOCK / F-NUMBER MSB", "CH3 SPECIAL F-NUMBER LSB", "CH3 SPECIAL BLOCK / F-NUMBER MSB",
        "FEEDBACK / ALGORITHM", "PAN / AMS / PMS"
    };

    if (address >= 0x20 && address < 0x30)
    {
        if (part != 0 || !IsValidPointer(k_mode_names[address & 0x0F]))
            return false;

        snprintf(name, size, "%s", k_mode_names[address & 0x0F]);
        return true;
    }

    int channel = address & 0x03;

    if (channel == 3)
        return false;

    channel += part * 3 + 1;

    if (address >= 0x30 && address < 0xA0)
    {
        snprintf(name, size, "%s  CH%d OP%d", k_operator_names[(address >> 4) - 3], channel,
            k_debug_ym3438_register_slot[(address >> 2) & 0x03]);
        return true;
    }

    if (address >= 0xA0 && address < 0xB8)
    {
        int index = (address - 0xA0) >> 2;

        if (index == 2 || index == 3)
        {
            static const int k_special_operator[3] = { 3, 1, 2 };

            if (part != 0)
                return false;

            snprintf(name, size, "%s  OP%d", k_channel_names[index], k_special_operator[address & 0x03]);
            return true;
        }

        snprintf(name, size, "%s  CH%d", k_channel_names[index], channel);
        return true;
    }

    return false;
}

#endif /* GUI_DEBUG_CONSTANTS_H */
