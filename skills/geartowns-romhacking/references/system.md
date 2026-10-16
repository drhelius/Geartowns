# FM Towns — System Devices

## 1. Overview

The FM Towns board surrounds the 80386 with standard parts at FM Towns specific ports:

- Two 8259A interrupt controllers (master and slave)
- Two 8253 timers (six counters) and a timer control register, plus Interval Timer II on the 10F and later (the UX,
  the Marty) and a free-running microsecond timer on the UG and the Marty
- A uPD71071 DMA controller with four channels
- An MSM58321 real time clock
- System control registers (reset, power, machine ID, serial ID ROM, memory windows)
- A keyboard controller

All registers are 8-bit and sparsely decoded. See the io_ports resource for the full map.

---

## 2. Interrupt Controllers (8259A)

| Port | Chip | Write | Read |
|---|---|---|---|
| 0000h | Master | ICW1 (bit 4 set), OCW2, OCW3 | IRR or ISR (OCW3), or poll result |
| 0002h | Master | ICW2-ICW4 during init, then OCW1 (IMR) | IMR |
| 0010h | Slave | ICW1, OCW2, OCW3 | IRR or ISR, or poll result |
| 0012h | Slave | ICW2-ICW4 during init, then OCW1 (IMR) | IMR |

The slave's INT output drives master IR7. The Model 1/2 BIOS programs:

| | ICW1 | ICW2 | ICW3 | ICW4 | IMR |
|---|---|---|---|---|---|
| Master | 19h: level, cascade, ICW4 | 40h: vectors 40h-47h | 80h: slave on IR7 | 0Dh: 8086, buffered master | FEh |
| Slave | 19h: level, cascade, ICW4 | 48h: vectors 48h-4Fh | 87h: slave ID 7 (bits 2-0) | 09h: 8086, buffered slave | FFh |

ICW1: bit 0 IC4 (ICW4 follows), bit 1 SNGL (single), bit 3 LTIM (level triggered), bit 4 = 1. Writing ICW1 clears
the IMR and special mask mode, selects the IRR for status reads and starts the sequence ICW2, ICW3, ICW4.
ICW4: bit 0 uPM (8086), bit 1 AEOI, bit 2 M/S, bit 3 BUF, bit 4 SFNM.
OCW2 (bits 4-3 = 00): 20h non-specific EOI, 6xh specific EOI for line x, A0h rotate on non-specific EOI, Exh
rotate on specific EOI, Cxh set lowest priority to line x. OCW3 (bits 4-3 = 01): 0Ah read IRR, 0Bh read ISR, 0Ch
poll, 68h/48h set/clear special mask.

Interrupt lines:

| IRQ | Source | IRQ | Source |
|---:|---|---:|---|
| 0 | Timer | 8 | SCSI |
| 1 | Keyboard | 9 | CD-ROM |
| 2 | RS-232C | 10 | I/O expansion |
| 3 | Expansion RS-232C | 11 | VSYNC |
| 4 | I/O expansion | 12 | Printer |
| 5 | I/O expansion | 13 | Sound (YM3438, RF5C68) |
| 6 | Floppy disk | 14 | I/O expansion |
| 7 | Slave PIC (cascade) | 15 | Reserved |

A handler for IRQ8-15 sends an EOI to the slave and then to the master.

---

## 3. Timers (8253) and Register 0060h

| Port | Counter | Clock | Use |
|---|---:|---|---|
| 0040h | 0 | 307.2 kHz | Interval timer (IRQ0) |
| 0042h | 1 | 307.2 kHz | I/O timeout (IRQ0) |
| 0044h | 2 | 307.2 kHz | Buzzer tone |
| 0046h | 0-2 | | Control word |
| 0050h | 3 | — | Reserved |
| 0052h | 4 | 1.2288 MHz | RS-232C baud rate |
| 0054h | 5 | — | Reserved |
| 0056h | 3-5 | | Control word |

Control word: bits 7-6 counter (0-2 within the chip), bits 5-4 access (00 latch, 01 LSB, 10 MSB, 11 LSB then
MSB), bits 3-1 mode, bit 0 BCD. Modes: 0 interrupt on terminal count, 1 one-shot, 2 rate generator, 3 square wave,
4 software strobe, 5 hardware strobe. No counter has a gate input on this board, so modes 1 and 5 never trigger. A
count of 0 means 65536 (10000 in BCD). Allow at least 1.3 µs between consecutive accesses to the timers.

Example: counter 0 in mode 3 with 0C00h gives 307200 / 3072 = 100 Hz.

Register 0060h:

| Bit | Read | Write |
|---:|---|---|
| 0 | Counter 0 timeout latched | Counter 0 timeout enable |
| 1 | Counter 1 timeout latched | Counter 1 timeout enable |
| 2 | Counter 0 enable | SOUND: buzzer on |
| 3 | Counter 1 enable | |
| 4 | SOUND | |
| 7 | | 1 clears the counter 0 latch |

IRQ0 is active while a latched timeout is enabled. Counter 0 is periodic and its latch is cleared through bit 7;
loading counter 1 clears its latch. The Model 1/2 BIOS writes 81h. Reading CFF98h also turns the buzzer on and
writing it turns the buzzer off.

### Interval Timer II (10F and later, the UX)

| Port | Read | Write |
|---|---|---|
| 0068h | Bit 7 interrupt disable, bit 6 timeout pending, bit 5 overrun; the read clears bits 6 and 5 | Bit 7 interrupt disable |
| 006Ah | Period low | Period low |
| 006Bh | Period high | Period high; starts an interval |

The period counts microseconds, 0 meaning 65536. Each expiry reloads it. The first timeout sets bit 6 and a later
one before the read sets bit 5. IRQ0 is also active while bit 6 is set and bit 7 clear, until 0068h is read; bit 7
masks the interrupt without stopping the count. The UX BIOS writes 80h to 0068h at reset.

### Free-running timer (UG, HG, the Marty)

0026h reads a 16-bit counter of microseconds, 0027h its high byte; read it as one word, so a carry between two byte
reads can't tear it. The Marty BIOS times its delays with it and doesn't start without it.

---

## 4. DMA Controller (uPD71071)

| Port | Register |
|---|---|
| 00A0h | Initialize (write): bit 0 resets the controller, bit 1 selects the 16-bit register interface |
| 00A1h | Channel select. Write: bits 1-0 channel, bit 2 base-only access. Read: bits 3-0 selected channel (one bit each), bit 4 base-only access |
| 00A2h-00A3h | Count, low and high: the number of transfers minus one |
| 00A4h-00A7h | Address, bits 0-7 to 24-31 |
| 00A8h-00A9h | Device control |
| 00AAh | Mode of the selected channel |
| 00ABh | Status: bits 3-0 terminal count (cleared by the read), bits 7-4 requests |
| 00ACh-00ADh | Temporary (memory-to-memory transfers, not used) |
| 00AEh | Software request bits (write) |
| 00AFh | Mask bits, 1 masks a channel |

Channels: 0 floppy, 1 SCSI, 2 printer, 3 CD-ROM. With base-only access clear, writing count and address loads both
the base and the current registers and reads return the current ones; the current registers count during a
transfer and reload from the base on auto-initialize. The controller counts address bits 0-23; bits 24-31 come
from one board register shared by base and current, with no carry from bit 23. Mask a channel before reprogramming
it.

Device control: bit 0 MTM, 1 AHLD, 2 DDMA (1 disables all DMA), 3 CMP, 4 ROT, 5 EXW, 6 RQL, 7 AKL, 8 BHLD, 9 WEV.
The Model 1/2 BIOS writes 20h to 00A8h and 00h to 00A9h (extended write, everything else off).

Mode: bit 0 word units, bits 3-2 direction (01 I/O to memory, 10 memory to I/O), bit 4 auto-initialize, bit 5
decrement, bits 7-6 service (00 demand, 01 single). The Model 1/2 BIOS uses 44h and 48h for floppy reads and writes
on channel 0, and 54h for CD-ROM reads on channel 3.

---

## 5. Real Time Clock (MSM58321)

| Port | Read | Write |
|---|---|---|
| 0070h | Bit 7 READY (low for about 427 µs once per second while the counters update), bits 3-0 data | Bits 3-0 register number or data |
| 0080h | | Bit 7 CHIP SELECT, bit 2 READ, bit 1 WRITE, bit 0 ADDRESS WRITE |

Access sequence:

1. Wait for READY (0070h bit 7) to be 1.
2. Write 80h to 0080h, the register number to 0070h, then 81h and 80h to 0080h to latch it.
3. To read, write 84h to 0080h, wait at least 2 µs and read 0070h. To write, put the value in 0070h, write 82h to
   0080h, wait at least 2 µs and write 80h.
4. Write 00h to 0080h to deselect the chip.

An access must end within 244 µs of READY going high.

| Register | Digit | Register | Digit |
|---:|---|---:|---|
| 0 | Seconds ones | 8 | Day tens; bits 3-2 leap year phase |
| 1 | Seconds tens | 9 | Month ones |
| 2 | Minutes ones | 10 | Month tens |
| 3 | Minutes tens | 11 | Year ones |
| 4 | Hours ones | 12 | Year tens |
| 5 | Hours tens; bit 3 24-hour, bit 2 PM | 13 | Divider reset (any write) |
| 6 | Weekday, 0-6 | 14 | Reference signals (read) |
| 7 | Day ones | | |

The year is two BCD digits with no century. February has 29 days when the year modulo 4 equals the leap phase;
there is no century rule. A write to register 13 resets the divider and READY stays low for about one second.

---

## 6. System Control

| Port | Read | Write |
|---|---|---|
| 0020h | Reset cause: bit 0 software reset, bit 1 CPU shutdown; the read clears them | Bit 0 reset the CPU, bit 6 power off, bit 7 write-protect the memory holding the NMI vector |
| 0022h | | Bit 6 power off; HG: bit 7 turns the CRT off (the picture goes black) |
| 0024h | UG and HG: capability flags, active low: bit 7 power-off flag valid, bit 6 CRT readback, bit 5 CRT power off, bit 4 free-running timer, bit 3 software power off; bits 2-0 read 1 (UG 2Fh, HG 07h) | |
| 0025h | UX: 7Fh, bit 7 = 0 means the NMI mask exists | |
| 0028h | UX: bit 0 NMI masked | UX: bit 0 masks NMI; power-on reset clears it, a software reset keeps it |
| 0030h-0031h | Bits 15-3 machine ID, bits 2-0 CPU ID (001b = 80386DX, 011b = 80386SX); 0101h on Model 1/2, 0201h on the 1F/2F, 0401h on the 10F/20F, 0501h on the CX, 0303h on the UX, 0603h on the UG, 0801h on the HG, 4A03h on the Marty; UG and HG: bit 15 reads 1 for 300 µs after power-on | UX: 0030h bit 0 LSPEED, two-wait RAM |
| 006Ch | 10F and later: 00h, and a 1 µs wait | 10F and later: a 1 µs wait |
| 0032h | Bit 0 serial ID ROM data | Bit 7 ID RESET, bit 6 ID CLK, bit 5 /CS |
| 0400h | Bit 0 resolution: 0 on Model 1/2 | |
| 0404h | Bit 7 low window: 0 FM-R devices, 1 RAM | Same |
| 0480h | Bit 1 boot window (0 ROM, 1 RAM), bit 0 dictionary and CMOS window | Same |
| 0484h | | Bits 3-0 dictionary bank |
| 048Ah | Memory card: bit 7 changed (cleared by the read), bit 5 battery dead, bit 4 battery low, bits 2-1 card detect (00 present, 11 absent), bit 0 write protect | |
| 05C0h | | Bit 3 expansion NMI enable |
| 05C2h | Bit 3: the expansion bus caused the NMI | |
| 05C8h | Bit 7 text VRAM written (cleared by the read) | |
| 05CAh | | Any write clears the VSYNC interrupt |
| 05E0h | | The Model 1/2 BIOS writes 01h at boot |
| 05E2h, 05E6h | 10F and later: the value written | Memory wait settings |
| 05E8h | 1F and later: installed RAM in MB, bits 4-0 | |
| 05ECh | CX: bit 7 = 0 means speed control exists, bit 0 the mode | CX: bit 0 high speed (1) or compatible (0) |
| 05EDh | UG and HG: bit 7 = 0, bits 6-0 the maximum CPU clock in MHz | |
| 0490h | UX: memory card bank | Bits 5-0: the 1 MB bank of the card at D00000h |
| 0491h | UX: bit 0 attribute space, bit 7 = 0 means JEIDA-4 capable | Bit 0: 0 card memory, 1 attribute space |

Both reset cause bits read 0 after power-on. Software masks the PICs before a software reset, never resets during
a DMA transfer, and executes HLT right after the reset write. The ports marked with a model read FFh and ignore
writes on machines without them; 006Ch delays reads and writes by 1 µs of CPU time at any CPU speed.

The serial ID ROM holds 256 bits, read one bit per ID CLK pulse after an ID RESET: a 0 nibble, `FUJITSU` in ASCII,
reserved ones, the model number (0101h on Model 1/2, 0303h on the UX, 4A03h on the Marty), nine serial number digits
and a final 00000h. The UX set's `mytownsux.rom` supplies it; without a serial ROM file the emulator builds one with
the model number and a zero serial number. The ports marked UX exist on the Marty too.

---

## 7. Keyboard

| Port | Read | Write |
|---|---|---|
| 0600h | Next byte; the read clears bit 0 of 0602h | Command parameters |
| 0602h | Bit 0 data available, bit 1 controller busy (writes ignored) | Command |
| 0604h | Bit 0 keyboard interrupt request, bit 1 keyboard NMI | Bit 0 keyboard IRQ1 enable |

Each key event is two bytes, a flag byte (bit 7 set) and a key code (bit 7 clear):

| Flag bit | Meaning |
|---:|---|
| 7 | 1: first byte of an event |
| 6-5 | Keyboard type: 01 JIS |
| 4 | 0 make (press), 1 break (release) |
| 3 | CTRL held |
| 2 | SHIFT held |
| 1-0 | Left and right thumb shift (thumb shift keyboard) |

Examples on the JIS keyboard: `A0 1E` A down, `B0 1E` A up, `A4 1E` A down with SHIFT. Key codes follow the
keyboard layout, for example 1Dh RETURN, 35h SPACE, 52h CTRL, 53h SHIFT, 5Dh PF1.

Commands: A1h reset, A4h/A5h thumb shift chord monitoring on/off, A9h/AAh/ABh typematic delay 400/500/300 ms,
ACh/ADh/AEh typematic interval 50/30/20 ms, B0h/B1h diagonal cursor codes on/off, B2h NMI acknowledge. Parameters
go to 0600h first and the command to 0602h last.
