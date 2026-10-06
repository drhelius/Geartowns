# FM Towns — Memory Map (Model 1/2)

## 1. Overview

The FM Towns Model 1/2 is built around an Intel 80386DX with a 32-bit physical address bus.

General characteristics:

- Physical address space: 4 GB, sparsely decoded
- Main RAM at physical 0: 1 MB on the Model 1, 2 MB on the Model 2, up to 6 MB with expansion modules
- VRAM, sprite RAM, ROMs, CMOS RAM and the PCM wave RAM window at fixed high addresses
- A window at C0000h–EFFFFh switches between RAM and FM-R compatible devices, and F8000h–FFFFFh between RAM and
  the boot ROM
- The CPU starts in real mode at FFFFFFF0h, inside the System ROM

Address spaces:

| Space | Meaning |
|---|---|
| Logical | Segment:offset as the program uses it, through the segment descriptor cache |
| Linear | After segmentation, before paging |
| Physical | After paging (equal to linear when CR0.PG is clear); the map below is a physical map |
| I/O | The 64 KB I/O port space, separate from memory |

---

## 2. Physical Map

| Physical range | Size | Contents |
|---|---:|---|
| 00000000–000BFFFF | 768 KB | Main RAM |
| 000C0000–000EFFFF | 192 KB | Main RAM, or the FM-R compatible devices (§4) |
| 000F0000–000F7FFF | 32 KB | Main RAM |
| 000F8000–000FFFFF | 32 KB | Boot ROM (last 32 KB of the System ROM) or RAM (§4) |
| 00100000–005FFFFF | 5 MB | Expansion RAM; only installed RAM responds |
| 00600000–3FFFFFFF | — | Reserved |
| 40000000–7FFFFFFF | 1 GB | I/O expansion slot memory |
| 80000000–8007FFFF | 512 KB | VRAM, two-page (layered) view |
| 80080000–800FFFFF | — | Reserved |
| 80100000–8017FFFF | 512 KB | VRAM, single-page view of the same memory |
| 80180000–80FFFFFF | — | Reserved |
| 81000000–8101FFFF | 128 KB | Sprite RAM (entries, color tables, patterns) |
| 81020000–BFFFFFFF | — | Reserved |
| C0000000–C0FFFFFF | 16 MB | ROM card window |
| C1000000–C1FFFFFF | — | Reserved |
| C2000000–C207FFFF | 512 KB | OS ROM (MS-DOS and the BIOS) |
| C2080000–C20FFFFF | 512 KB | Dictionary ROM |
| C2100000–C213FFFF | 256 KB | Font (kanji) ROM |
| C2140000–C2141FFF | 8 KB | CMOS (learning) RAM, battery backed, byte access only |
| C2142000–C21FFFFF | — | Reserved |
| C2200000–C2200FFF | 4 KB | RF5C68 wave RAM window, byte access only |
| C2201000–FFFBFFFF | — | Reserved |
| FFFC0000–FFFFFFFF | 256 KB | System ROM |

Only one 512 KB VRAM exists; both VRAM ranges are views of it.

---

## 3. VRAM Views

The 512 KB VRAM is one memory with two windows:

- **Two-page view (80000000h)**: layer 0 occupies 00000h–3FFFFh and layer 1 40000h–7FFFFh. Used by the two-layer
  modes (16 or 32K colors per layer).
- **Single-page view (80100000h)**: the 512 KB as one surface for the single-layer 256-color and 32K-color modes.
  Groups of 4 bytes alternate between the two halves of the memory:

```text
two_page_offset = ((view_offset & 4) << 16) | ((view_offset & 7FFF8h) >> 1) | (view_offset & 3)
```

The write mask set through ports 0458h/045Ah/045Bh applies to CPU writes in both views: a 0 bit protects the
VRAM bit. The mask is four bytes, selected by the low two address bits. See the video resource.

---

## 4. Low Memory Banking (C0000h–FFFFFh)

Three I/O registers select what the CPU sees in the low windows:

| Port | Bit | Meaning |
|---|---|---|
| 0404h | 7 | 0: FM-R compatible devices at C0000h–CFFFFh; 1: RAM at C0000h–EFFFFh |
| 0480h | 1 | 0: Boot ROM at F8000h–FFFFFh; 1: RAM |
| 0480h | 0 | 1: Dictionary ROM bank and CMOS RAM at D0000h–D9FFFh (while 0404h bit 7 is 0) |
| 0484h | 3–0 | Dictionary ROM bank for the D0000h window (16 banks of 32 KB) |

With R = bit 7 of 0404h, S = bit 1 of 0480h and D = bit 0 of 0480h:

| R | S | D | C0000–CFFFF | D0000–D7FFF | D8000–D9FFF | DA000–EFFFF | F8000–FFFFF |
|---:|---:|---:|---|---|---|---|---|
| 0 | 0 | 0 | FM-R devices | — | — | — | Boot ROM |
| 0 | 0 | 1 | FM-R devices | Dictionary bank | CMOS RAM | — | Boot ROM |
| 0 | 1 | 0 | FM-R devices | — | — | — | RAM |
| 0 | 1 | 1 | FM-R devices | Dictionary bank | CMOS RAM | — | RAM |
| 1 | 0 | x | RAM | RAM | RAM | RAM | Boot ROM |
| 1 | 1 | x | RAM | RAM | RAM | RAM | RAM |

After reset R = 0 and S = 0: the boot ROM is visible at F8000h and the FM-R devices at C0000h. The Model 1/2 BIOS
reads 0404h and 0480h back and changes them with read-modify-write sequences.

### 4.1 FM-R Compatible Devices (R = 0)

| Range | Contents |
|---|---|
| C0000–C7FFF | FM-R planar graphics: one byte is 8 pixels of one plane of layer 0 |
| C8000–C9FFF | FM-R text RAM: 4 KB of character text and 4 KB of kanji text, held in Sprite RAM |
| CA000–CBFFF | ANK font, while CFF99h bit 0 is set |
| CFF80–CFFFF | FM-R registers (§4.2) |

Planar graphics: CFF81h bits 7–6 select the plane read, bits 3–0 the planes written. CFF83h bit 4 selects the
second FM-R page (VRAM offset +20000h).

### 4.2 FM-R Registers

| Address | I/O alias | Function |
|---|---|---|
| CFF80 | — | MIX register: bit 5 cursor position LSB, bit 3 40/80 columns; no effect on the display |
| CFF81 | FF81h | Plane access: bits 7–6 plane read, bits 3–0 planes written |
| CFF82 | FF82h | Display: bits 5 and 2–0 visible planes C3 and C2–C0, bit 4 displayed page (write) |
| CFF83 | FF83h | Access page, bit 4 |
| CFF84 | FF84h | Light pen status, always 0 |
| CFF86 | FF86h | Sync status: bit 7 HSYNC, bit 4 always 1, bit 2 VSYNC |
| CFF94 | FF94h | Kanji ROM JIS code high (write); bit 7 = level 2 kanji present, always 1 (read) |
| CFF95 | FF95h | Kanji ROM JIS code low (write); resets the glyph row |
| CFF96 | FF96h | Glyph row, left byte (bits 15–8) |
| CFF97 | FF97h | Glyph row, right byte (bits 7–0); an access advances the row (0–15) |
| CFF98 | FF98h | Buzzer: a read turns it on, a write turns it off |
| CFF99 | FF99h | Bit 0: 0 kanji text RAM, 1 ANK font |
| CFFA0 | FFA0h | Logical operation status: bit 7 ESTART, always 0 |

Reads of CFF97h, CFF98h and port 05C8h have side effects. The debugger reads them without the side effect.

---

## 5. CMOS RAM

The 8 KB CMOS (learning) RAM is one memory seen three ways:

- Physical C2140000h–C2141FFFh (always)
- Physical D8000h–D9FFFh while the low dictionary window is enabled (§4)
- I/O ports 3000h–3FFEh, even ports only: `index = (port - 3000h) / 2` reaches the first 2 KB

The upper 6 KB can hold user-defined glyphs. It is not an IBM PC style 70h/71h CMOS: ports 0070h/0080h belong to
the MSM58321 clock.

---

## 6. RF5C68 Wave RAM

The RF5C68 has 64 KB of wave RAM. The CPU sees one 4 KB bank at C2200000h–C2200FFFh. The bank is selected with
the RF5C68 control register (port 04F7h): a write with bit 6 (MOD) clear selects the wave bank in bits 3–0, a write
with bit 6 set selects the channel for the channel registers instead. CPU reads of wave RAM are not available while
sound is on (04F7h bit 7). See the sound resource.

---

## 7. Reset

The 80386 starts at CS=F000h (base FFFF0000h), EIP=FFF0h, so the first instruction is fetched at FFFFFFF0h in the
System ROM. The boot ROM at F8000h–FFFFFh is the same last 32 KB of the System ROM, reachable from real mode.

---

## 8. Debugger Memory Areas

`list_memory_areas` returns, in this order:

1. LINEAR, PHYSICAL and I/O PORTS
2. The regions the machine maps: System ROM, System ROM (low boot window), Main RAM, VRAM (raw 512 KB, two-page
   order), VRAM (two-page view), VRAM (single-page view), Sprite RAM, OS ROM, Dictionary ROM, Font ROM, CMOS RAM,
   PCM wave RAM window, Dictionary ROM (low window), CMOS RAM (low window), FM-R VRAM planes, FM-R text RAM and
   ANK font, FM-R registers, FM-R view (unmapped)
3. Firmware images that are loaded but not mapped on this machine, the CD-ROM media image and the inserted floppy
   disk images

Region offsets are 0-based; `physical_base` gives the bus address of mapped regions. The LINEAR area also accepts
logical addresses (`CS:1234`, `0008:00001234`). The 64 KB wave RAM is reached through the 4 KB PCM wave RAM window
with the bank selected in 04F7h.
