# FM Towns — Floppy Disk

## 1. Overview

A Fujitsu MB8877A floppy controller (WD1793 compatible) drives up to four drives: internal drives 0-1 (one on the
Model 1, two on the Model 2) and external drives 2-3. Transfers use DMA channel 0 and the controller interrupts on
IRQ6 (master IR6, vector 46h with the BIOS setup).

| Media | Towns OS format | Encoding | Controller clock | Spindle | Transfer rate |
|---|---|---|---|---|---|
| 2HD, 1232 KB | 77 cylinders × 2 sides × 8 sectors of 1024 bytes | MFM | 2 MHz | 360 rpm | 500 kbit/s |
| 2DD, 640 KB | 80 cylinders × 2 sides × 8 sectors of 512 bytes | MFM | 1 MHz | 300 rpm | 250 kbit/s |
| 2HD, 1440 KB | 80 cylinders × 2 sides × 18 sectors of 512 bytes | MFM | 2 MHz | 300 rpm | 500 kbit/s |

The 1440 KB format needs a three-mode drive (HG and later models). The Model 1/2 two-mode drives can hold such a
disk but never read it.

2D (FM-family) disks can be read for compatibility. The controller handles sector sizes of 128, 256, 512 and 1024
bytes, so other layouts are possible.

---

## 2. Ports

| Port | Read | Write |
|---|---|---|
| 0200h | MB8877A status | MB8877A command |
| 0202h | Track register | Track register |
| 0204h | Sector register | Sector register |
| 0206h | Data register | Data register |
| 0208h | Drive status | Drive control |
| 020Ch | | Drive select |
| 020Dh | Drive type extension | |
| 020Eh | Drive switch | Drive switch: bit 0 swaps drives 0-1 with 2-3 |

Drive status (0208h read): bit 0 always 1, bit 1 READY of the selected drive, bit 2 external drive type (0 5.25",
1 3.5"). Machines with three-mode drives read 011b in bits 4-2 instead, and 7Fh at 020Dh (FDDVEXT low).

Drive control (0208h write): bit 0 IRQ enable, bit 1 MFM (double density), bit 2 side 1, bit 4 motor on, bit 5
CLKSEL (0 2 MHz for 2HD and fast seeks, 1 1 MHz for 2D/2DD).

Drive select (020Ch): bits 3-0 select drives 0-3 (one bit each, never more than one), bit 4 in-use lamp, bit 6
HISPD (1 360 rpm for 2HD, 0 300 rpm). Three-mode drives add bit 7 MODE-B: with HISPD it selects 300 rpm for
1440 KB disks, without it the unsupported 180 rpm mode. The in-use and speed bits are latched when a drive select
bit is written as 1, so software writes them first and then selects the drive in a second write.

---

## 3. MB8877A Commands

| Command | Type | Bits |
|---|---|---|
| 0xh RESTORE | I | h head load, V verify, r1-r0 step rate |
| 1xh SEEK (to the data register) | I | Same |
| 2xh/3xh STEP | I | u (bit 4) updates the track register |
| 4xh/5xh STEP IN | I | Same |
| 6xh/7xh STEP OUT | I | Same |
| 8xh/9xh READ SECTOR | II | m (bit 4) multiple, S (bit 3) side to compare, E (bit 2) head settle delay, C (bit 1) compare side |
| Axh/Bxh WRITE SECTOR | II | Same, a0 (bit 0) deleted data mark |
| C0h/C4h READ ADDRESS | III | 6 ID bytes: C, H, R, N, CRC (2) |
| E0h/E4h READ TRACK | III | E (bit 2) |
| F0h/F4h WRITE TRACK | III | Formats a track |
| Dxh FORCE INTERRUPT | IV | Bit 0 not ready to ready, bit 1 ready to not ready, bit 2 index pulse, bit 3 immediate; D0h ends the command without an interrupt |

Step rates r1-r0 = 0-3 are 3, 6, 10 and 15 ms with the 2 MHz clock, twice as long with 1 MHz. The E delay is 15 ms
with the 2 MHz clock and 30 ms with 1 MHz.

Status (0200h read):

| Bit | Type I | Type II/III |
|---:|---|---|
| 7 | NOT READY | NOT READY |
| 6 | WRITE PROTECT | WRITE PROTECT |
| 5 | HEAD LOADED | RECORD TYPE (deleted data mark) on reads, WRITE FAULT on writes |
| 4 | SEEK ERROR | RECORD NOT FOUND |
| 3 | CRC ERROR | CRC ERROR |
| 2 | TRACK 00 | LOST DATA |
| 1 | INDEX | DRQ |
| 0 | BUSY | BUSY |

Reading the status or writing a command clears INTRQ.

---

## 4. A Sector Read

1. Write the control values to 020Ch, then select the drive; turn the motor on with MFM, the clock and the side in
   0208h, and wait for READY.
2. Seek: write the cylinder to 0206h, then 1xh to 0200h; wait for INTRQ.
3. Program DMA channel 0 (mode 44h, I/O to memory, the sector size minus one), write the sector number to 0204h
   and 80h to 0200h.
4. INTRQ at the end; the status reports RECORD NOT FOUND or CRC ERROR if the sector failed.

---

## 5. Disk Images

The debugger's Floppy 0 Image and Floppy 1 Image memory areas hold each inserted disk as a D77 image:

- Header, 2B0h bytes: name (17 bytes), write protect at 1Ah (10h protected), media type at 1Bh (00h 2D, 10h 2DD,
  20h 2HD), image size at 1Ch (4 bytes), then 164 track offsets of 4 bytes at 20h (0 for an absent track).
- Each sector: a 16-byte header (C, H, R, N, sectors in the track (2 bytes), density (00h MFM, 40h FM), deleted mark
  (10h deleted), status, 5 reserved bytes, data size (2 bytes)) followed by the data.

Sector status values: 00h OK, 10h deleted data, A0h ID CRC error, B0h data CRC error, E0h no address mark, F0h no
data mark.
