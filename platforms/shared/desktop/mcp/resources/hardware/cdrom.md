# FM Towns — CD-ROM

## 1. Overview

The internal CD-ROM drive sits behind a controller with its own sub-MPU and an 8 KB sector buffer. Software writes
eight parameter bytes and a command, reads four-byte status packets, and takes sector data through DMA channel 3 or
CPU reads. CD audio plays into the mixer through electronic volume 2. The controller interrupts on IRQ9 (slave IR1,
vector 49h with the BIOS setup).

Addresses: MSF parameters and positions are BCD and absolute, 75 frames per second. LBA 0 = MSF 00:02:00, so
`LBA = (minute × 60 + second) × 75 + frame − 150`.

A raw sector is 2352 bytes: 12 sync bytes, a 4-byte header (minute, second, frame, mode), then 2048 user bytes plus
EDC/ECC in Mode 1, or 2336 bytes in Mode 2.

---

## 2. Ports

| Port | Read | Write |
|---|---|---|
| 04C0h | Master status | Master control |
| 04C2h | Next status packet byte | Command |
| 04C4h | Sector data byte (CPU transfer) | Next parameter |
| 04C6h | | Transfer control: bit 4 DMA (DTS), bit 3 CPU (STS) |
| 04CCh | Subcode status: bit 0 subcode byte ready, bit 1 overrun | |
| 04CDh | Subcode byte: bits 7-0 = P, Q, R, S, T, U, V, W | |

Master status (04C0h read):

| Bit | Name | Meaning |
|---:|---|---|
| 7 | SIRQ | Sub-MPU interrupt request |
| 6 | DEI | DMA transfer end interrupt request |
| 5 | STSF | CPU transfer in progress |
| 4 | DTSF | DMA transfer in progress |
| 1 | SRQ | A status packet is waiting at 04C2h |
| 0 | DRY | Ready to accept a command |

Master control (04C0h write): bit 7 clears SIRQ, bit 6 clears DEI, bit 2 resets the sub-MPU, bit 1 enables the
SIRQ interrupt, bit 0 enables the DEI interrupt. Bits 1 and 0 are written on every write: C3h clears both requests
and keeps both interrupts enabled, 80h clears SIRQ and leaves both disabled. IRQ9 is active while (SIRQ and enabled)
or (DEI and enabled).

---

## 3. Commands

Wait for DRY, write all eight parameters to 04C4h (P0 first, unused ones included), then the command to 04C2h.
Command byte: bit 6 requests an IRQ, bit 5 requests status packets, bits 7 and 4-0 are the operation.

| Operation | Name | Parameters |
|---|---|---|
| 00h | Seek | P0-P2 MSF |
| 01h | Mode 2 read | P0-P2 start MSF, P3-P5 end MSF (inclusive) |
| 02h | Mode 1 read | Same, 2048 bytes per sector |
| 03h | Raw read | Same |
| 04h | Play CD-DA | P0-P2 start MSF, P3-P5 end MSF |
| 05h | Read TOC | |
| 06h | Read SUBQ (current position) | |
| 80h | Status | |
| 84h | Stop CD-DA | |
| 85h | Pause CD-DA | |
| 87h | Resume CD-DA | |

The Model 1/2 boot ROM polls without IRQs: it uses 20h (seek with status), 22h (Mode 1 read with status) and A0h
(status query with status), waits for SRQ in 04C0h, reads the packet and writes 80h to 04C0h.

---

## 4. Status Packets

Read four bytes from 04C2h per packet; read all four even when only the first is needed.

| Packet | Meaning |
|---|---|
| 00 ss 00 00 | Command accepted or status reply; ss: 00 ready, 03 audio playing, 08 disc changed, 09 not ready (no disc) |
| 04 00 00 00 | Seek done |
| 06 00 00 00 | Read done |
| 11 / 12 / 13 | Stop / pause / resume done |
| 16 cc tt 00, 17 mm ss ff | TOC entry pair. tt A0h: mm is the first track; tt A1h: mm is the last track; tt A2h: mm:ss:ff is the lead-out start; otherwise tt is a track (BCD) starting at mm:ss:ff. cc bit 6 (40h) marks a data track |
| 21 ee 00 00 | Error, by the low nibble of ee: 1, 2, C parameter error; 5, 6, 7 media error; 3, 4, 9, D hardware error; 8 disc changed; F command ended abnormally |
| 22 00 00 00 | Data ready: a sector is waiting for transfer |

---

## 5. Reading Sectors

1. Wait for DRY, write P0-P7 (start and end MSF in BCD) and the read command (22h, or 62h with IRQ).
2. The acceptance packet 00h arrives, then 22h data ready for each sector.
3. For each sector, either program DMA channel 3 (I/O to memory, count 7FFh for 2048 bytes) and write 10h to
   04C6h, or write 08h to 04C6h and read 2048 bytes from 04C4h.
4. At the end of a DMA transfer DTSF clears and DEI is set; acknowledge DEI with 04C0h bit 6.
5. After the last sector the 06h read done packet arrives. A 21h packet ends the command with an error.

The Model 1/2 boot ROM reads with DMA mode 54h, writes 10h to 04C6h and polls 04C0h until DTSF clears.

---

## 6. CD Audio

Play (04h) streams the audio from the start MSF to the end MSF at 44.1 kHz, 16-bit stereo. Pause (85h) keeps the
position for resume (87h); stop (84h) ends playback. The status reply (80h) reports 03 while audio is playing. The
CD audio level is set by electronic volume 2 channels 0 (left) and 1 (right); see the sound resource.

---

## 7. Driving it from MCP

- `get_cdrom_status` shows the controller: `ports` (the 04C0h bits as `sirq`, `dei`, `cpu_transfer`, `dma_transfer`,
  `status_available`, `command_ready`; the last `command` with its name and flags, `parameters`, the queued status
  packets, `transfer`), `interrupts` (enables, pending, `irq9`), `drive` (`state`, `head_lba` and `head_msf`, the read
  range and `sectors_left`), `media` (`ready`, `disc_changed`, tracks, length), CD `audio` (`state`, start, stop and
  current MSF, `track`, volume in dB) and the `playlist`.
- `list_cdrom_tracks` gives the TOC: type, MSF and LBA ranges, length, pregap and the track under the head.
  `read_cdrom_sector` reads an image sector by LBA (MSF minus 00:02:00), `user` for 2048 bytes or `raw` for 2352, and
  does not move the head.
- `eject_cdrom`, `load_media` (a CD image) and `insert_playlist_disc` (an index from the `playlist`) change the disc
  while the machine runs. `debug_reset` with `boot: cdrom` holds C+D through a reset.
- `get_dma_status` shows channel 3. Break on the controller's IRQ with `set_breakpoint_on_interrupt` and `irq: 9`, or
  on the ports with `set_breakpoint` in the `io` space over 04C0h-04CDh. The disc is also the CD-ROM media image
  memory area.
- `set_audio_mute` with `source: cdda` silences the disc in the debugger only. `get_sound_status` shows electronic
  volume 2.
- Trace `cdrom.commands` (each command with its MSF or parameters), `cdrom.status` (every four-byte packet with its
  name), `cdrom.irqs`, `cdrom.control` (04C0h writes), `cdrom.data` (sector ready, 04C6h transfer, sector end, lost
  data) and `cdrom.cdda` (play, pause, resume, stop, end, loop).
