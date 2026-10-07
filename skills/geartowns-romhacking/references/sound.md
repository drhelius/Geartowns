# FM Towns — Sound

## 1. Overview

| Source | Chip | Ports |
|---|---|---|
| FM synthesis | YM3438 (OPN2C), 6 channels × 4 operators, 8 MHz master clock | 04D8h-04DEh |
| PCM | RF5C68, 8 channels, 64 KB wave RAM | 04F0h-04F8h, wave window C2200000h |
| CD audio | CD-ROM drive, 44.1 kHz stereo | through electronic volume 2 |
| Line in, microphone, modem | Analog inputs | through electronic volumes 1 and 2 |
| Volume | Two MB87078 electronic volumes | 04E0h-04E3h |
| Mutes | FM and PCM mutes, master output gate | 04D5h, 04ECh |

FM and PCM go to the mixer without electronic volume; their levels are set in the chips. The sound interrupt is
IRQ13 (slave IR5). 04E9h reads the cause: bit 0 FM (a YM3438 timer flag), bit 3 PCM.

---

## 2. YM3438 Ports

| Port | Chip address | Read | Write |
|---|---|---|---|
| 04D8h | A1=0, A0=0 | Status | Address of a register in 21h-B6h (timers, key on, channels 1-3) |
| 04DAh | A1=0, A0=1 | — | Data for that register |
| 04DCh | A1=1, A0=0 | — | Address of a register in 30h-B6h for channels 4-6 |
| 04DEh | A1=1, A0=1 | — | Data for that register |

Status: bit 7 busy (wait before the next write), bit 1 timer B flag, bit 0 timer A flag. The chip drives the data
bus only for a status read at 04D8h.

---

## 3. YM3438 Registers

Global registers (04D8h/04DAh only):

| Address | Contents |
|---|---|
| 21h, 2Ch | Test, keep at 0 |
| 22h | Bit 3 LFO enable, bits 2-0 LFO rate |
| 24h/25h | Timer A, 10 bits (24h bits 9-2, 25h bits 1-0) |
| 26h | Timer B, 8 bits |
| 27h | Bits 7-6 channel 3 mode (00 normal, 01 special, 10 CSM), bits 5/4 reset B/A flag, bits 3/2 enable B/A flag, bits 1/0 load (run) B/A |
| 28h | Key on/off: bits 2-0 channel (0-2 channels 1-3, 4-6 channels 4-6), bits 7-4 operators 4, 3, 2, 1 |
| 2Ah | DAC data (8 bits, offset 80h) |
| 2Bh | Bit 7 DAC select: channel 6 outputs the 2Ah data instead of FM |

Operator registers, 30h-9Eh, low 2 bits select the channel (3 is unused); in each block the order is OP1, OP3,
OP2, OP4 (+0, +4, +8, +Ch):

| Base | Contents |
|---|---|
| 30h | Bits 6-4 DT (detune), bits 3-0 MUL (0 = ×1/2, 1-15 = ×1-×15) |
| 40h | Bits 6-0 TL (total level, 0.75 dB steps) |
| 50h | Bits 7-6 KS, bits 4-0 AR |
| 60h | Bit 7 AM enable, bits 4-0 D1R |
| 70h | Bits 4-0 D2R |
| 80h | Bits 7-4 D1L (sustain level, 3 dB steps, 15 = 93 dB), bits 3-0 RR |
| 90h | Bits 3-0 SSG-EG |

Channel registers:

| Base | Contents |
|---|---|
| A4h then A0h | Bits 5-3 block, bits 2-0 F-number high; then F-number low. Write A4h first |
| ACh then A8h | Channel 3 special mode frequencies: A9h/ADh OP1, AAh/AEh OP2, A8h/ACh OP3; OP4 uses A2h/A6h |
| B0h | Bits 5-3 feedback (OP1), bits 2-0 algorithm |
| B4h | Bit 7 left, bit 6 right, bits 5-4 AMS, bits 2-0 PMS |

With the 8 MHz master clock:

- Native sample rate = 8 MHz / 144 = 55.56 kHz.
- Note frequency = F-number × 2^block × 55556 / 2^21 Hz.
- Timer A period = (1024 − TA) × 18 µs, timer B period = (256 − TB) × 288 µs. A running timer reloads when it
  overflows; its flag is set only when its enable bit is set.
- LFO rates 0-7: 3.98, 5.56, 6.02, 6.37, 6.88, 9.63, 48.1, 72.2 Hz.

Algorithms (→ modulates, + sums to the output):

| Algorithm | Connection |
|---:|---|
| 0 | 1 → 2 → 3 → 4 |
| 1 | (1 + 2) → 3 → 4 |
| 2 | (1 + (2 → 3)) → 4 |
| 3 | ((1 → 2) + 3) → 4 |
| 4 | (1 → 2) + (3 → 4) |
| 5 | 1 → each of 2, 3, 4, summed |
| 6 | (1 → 2) + 3 + 4 |
| 7 | 1 + 2 + 3 + 4 |

Operator 1 can feed back on itself (feedback 0 off, 1-7) in every algorithm.

---

## 4. RF5C68 PCM

| Port | Register (for the channel selected in 04F7h) |
|---|---|
| 04F0h | ENV: envelope (volume), 8 bits |
| 04F1h | PAN: bits 3-0 left, bits 7-4 right |
| 04F2h/04F3h | FD: step, 16 bits, 5.11 fixed point; 0800h plays one byte per output sample |
| 04F4h/04F5h | LS: loop start address in wave RAM |
| 04F6h | ST: start address high byte (start = ST × 256) |
| 04F7h | Control: bit 7 sound on; bit 6 set: bits 2-0 select the channel; bit 6 clear: bits 3-0 select the 4 KB wave bank |
| 04F8h | Channel on/off: one bit per channel, 0 plays |
| 04EAh | IRQ mask, one bit per 8 KB region of wave RAM (bit 0 = 0000h-1FFFh ... bit 7 = E000h-FFFFh) |
| 04EBh | IRQ cause, one bit per 8 KB region; the read clears all of them |

The RF5C68 outputs one sample every 384 clocks of its 8 MHz clock, 20.83 kHz; a channel plays 20833 × FD / 2048
bytes per second. Switching a channel on starts it at ST × 256; it then advances FD/2048 bytes per output sample
and keeps the fraction.

Samples are sign and magnitude: bit 7 set is positive, bits 6-0 the magnitude (80h is zero, 81h-FEh +1 to +126,
01h-7Fh −1 to −127). FFh is a loop marker: playback jumps to LS. Each channel is scaled by ENV and then by its left
and right PAN values, and the channel sums are clipped. The IRQ cause bits are set as playback moves through the
regions enabled in 04EAh, which software uses to refill a buffer while it plays.

The CPU sees the 4 KB wave bank selected in 04F7h at C2200000h-C2200FFFh; reads are available only while sound is
off.

---

## 5. Electronic Volume (MB87078)

| Port | Function |
|---|---|
| 04E0h | Volume 1 data: bits 5-0 attenuation of the selected channel |
| 04E1h | Volume 1 command: bits 1-0 channel, bit 2 EN, bit 3 C0 (0 dB), bit 4 C32 (−32 dB) |
| 04E2h | Volume 2 data |
| 04E3h | Volume 2 command, same layout |

| Volume | Channel 0 | Channel 1 | Channel 2 | Channel 3 |
|---|---|---|---|---|
| 1 | Line in left | Line in right | — | — |
| 2 | CD audio left | CD audio right | Microphone | Modem |

Attenuation = −31.5 dB + 0.5 dB × data, so data 3Fh is 0 dB and 00h is −31.5 dB. EN = 0 mutes the channel; with
EN set, C32 selects −32 dB, otherwise C0 selects 0 dB, otherwise the data applies.

---

## 6. Mutes and ADC

| Port | Function |
|---|---|
| 04D5h | Bit 1 FM enable, bit 0 PCM enable (0 mutes) |
| 04ECh | Bit 7 LED level display off, bit 6 master output enable (0 mutes everything) |
| 04E7h | ADC sample data (sign and magnitude, up to 19.2 kHz); the read takes it from the FIFO |
| 04E8h | Read: bit 0 sample ready. Write: clears the sampling FIFO |

Reset leaves the master output disabled. The mutes do not stop the FM timers or PCM playback and its interrupts.
