# FM Towns — Video

## 1. Overview

The Model 1/2 video hardware has one 512 KB VRAM read by a CRTC as two layers, an output controller that sets the
formats and the priority, three analog palettes, an FM-R compatible planar view and the sprite engine (see the
sprites resource).

| Physical address | Size | View |
|---|---:|---|
| 80000000h-8007FFFFh | 512 KB | VRAM, two-page order: page 0 (layer 0) at 00000h, page 1 (layer 1) at 40000h |
| 80100000h-8017FFFFh | 512 KB | The same VRAM, single-page (interleaved) order |
| 81000000h-8101FFFFh | 128 KB | Sprite RAM |
| C0000h-C7FFFh | 32 KB | FM-R planes window (while 0404h bit 7 is 0) |

Single-page offset `a` maps to the two-page offset `p = ((a & 4) << 16) | ((a & 7FFF8h) >> 1) | (a & 3)`: groups of
four bytes alternate between the two halves.

Standard formats and the line pitch the BIOS modes use:

| Virtual surface | Format | View | Bytes per line |
|---|---|---|---:|
| 640×400, two pages per layer | 16 colors | Two-page | 320 |
| 1024×512 | 16 colors | Two-page | 512 |
| 1024×512 | 256 colors | Single-page | 1024 |
| 512×512 | 32K colors | Single-page | 1024 |
| 512×256 | 32K colors | Two-page | 1024 |
| 256×512 | 32K colors | Two-page | 512 |

---

## 2. Ports

| Port | Function |
|---|---|
| 0440h | CRTC index (5 bits) |
| 0442h/0443h | CRTC data, low and high byte (a word write to 0442h writes both) |
| 0448h | Output controller index |
| 044Ah | Output controller data |
| 044Ch | Read: bit 7 DPMD, FM-R digital palette written (the read clears it), bit 1 sprite busy, bit 0 sprite page |
| 0450h/0452h | Sprite register index and data |
| 0458h | VRAM write mask register index (bits 1-0) |
| 045Ah/045Bh | VRAM write mask, low and high byte of the selected register |
| 05C8h | Read: bit 7 text VRAM written (the read clears it) |
| 05CAh | Write: clears the VSYNC interrupt (IRQ11) |
| FD90h | Palette index |
| FD92h/FD94h/FD96h | Palette blue, red, green |
| FD98h-FD9Fh | FM-R digital palette (low 4 bits each); a write sets DPMD and does not change the displayed colors |
| FDA0h | Read: bit 0 VSYNC, bit 1 HSYNC. Write: bits 3-2 layer 0 enable, bits 1-0 layer 1 enable |
| FF81h-FF83h, FF94h-FF99h | FM-R plane mask, display page, access page, kanji ROM, ANK (aliases of CFF8xh/CFF9xh) |

---

## 3. CRTC Registers

| Index | Name | Function |
|---:|---|---|
| 00h | HSW1 | Horizontal sync width |
| 01h | HSW2 | Horizontal sync width during vertical sync |
| 02h-03h | — | Reserved |
| 04h | HST | Horizontal period minus 1, in dots |
| 05h/06h | VST1/VST2 | Vertical sync timing |
| 07h | EET | Equalizing pulse interval |
| 08h | VST | Vertical period minus 1, in half-lines |
| 09h/0Ah | HDS0/HDE0 | Layer 0 horizontal display start and end, in dots |
| 0Bh/0Ch | HDS1/HDE1 | Layer 1 horizontal display start and end |
| 0Dh/0Eh | VDS0/VDE0 | Layer 0 vertical display start and end, in half-lines |
| 0Fh/10h | VDS1/VDE1 | Layer 1 vertical display start and end |
| 11h | FA0 | Layer 0 start address (scroll) |
| 12h | HAJ0 | Layer 0 horizontal address adjustment, in dots |
| 13h/14h | FO0/LO0 | Layer 0 field offset and line offset (stride) |
| 15h-18h | FA1, HAJ1, FO1, LO1 | Same for layer 1 |
| 19h/1Ah | EHAJ/EVAJ | External sync adjustment |
| 1Bh | ZOOM | Nibbles from low to high: layer 0 X, layer 0 Y, layer 1 X, layer 1 Y; each is the factor minus 1 |
| 1Ch | CR0 | Bit 15 START, bit 14 external sync, bits 7-6 superimpose (0) or digitize (1) per layer, bits 5-4 address carry enables (CEN1/CEN0), bits 3-2 layer 1 and bits 1-0 layer 0 color encoding |
| 1Dh | CR1 | Bits 1-0 dot clock: 0 28.6363 MHz, 1 24.5454 MHz, 2 25.175 MHz, 3 21.0525 MHz; bits 3-2 color subcarrier divider |
| 1Eh | FR | Read (0443h): bit 0 video input present, bit 1 HSYNC, bit 2 VSYNC, bit 3 field, bits 4-5 layer 0/1 horizontal display, bits 6-7 layer 0/1 vertical display |
| 1Fh | CR2 | External sync monostable |

Line rate = dot clock / (HST + 1). Field rate = 2 × dot clock / ((VST + 1) × (HST + 1)). With an odd VST + 1 the
fields are half a line apart (interlaced), and FO gives the start offset of the second field.

FA and LO count 4 bytes in two-page mode and 8 bytes in single-page mode. One step of FA moves 8 pixels at 16 or
256 colors, 2 pixels at 32K colors in two-page mode and 4 pixels at 32K colors in single-page mode. CEN1/CEN0
enable the carry from the low address counter into the high address bits; with the carry disabled the address
wraps, which gives the cylindrical and spherical scrolling of the virtual surfaces.

A layer pixel spans zoom_x dots and zoom_y lines. A layer is displayed between HDS and HDE and between VDS and VDE,
which gives (HDE − HDS) / zoom_x pixels by (VDE − VDS) / 2 / zoom_y lines per field. HAJ sets where the address
fetch starts in the line; the standard modes set HAJ equal to HDS.

Color encoding (CR0 bits 3-0): 01b two-page 32K colors, 10b single-page 32K colors, 11b single-page 256 colors or
two-page 16 colors.

Example, 320×240 32K colors on layer 0 in two-page mode (15 kHz): CR1 = 1, HST = 617h, VST = 20Bh, HDS0 = HAJ0 =
E7h, HDE0 = 5E7h (1280 dots), VDS0 = 2Ah, VDE0 = 20Ah (480 half-lines), ZOOM low byte 03h (×4 horizontally), LO0 =
100h (1024 bytes per line).

---

## 4. Output Controller

Register 0 (0448h = 0): bit 4 PMODE (0 single page, 1 two pages).

| Mode | Bits | Output |
|---|---|---|
| Single page | bits 3-0 = 1010b | Layer 0 256 colors |
| | bits 3-0 = 1111b | Layer 0 32K colors |
| | 0000b, 0101b | Off |
| Two pages | bits 1-0 layer 0, bits 3-2 layer 1 | 01 16 colors, 11 32K colors, 00/10 off |

Register 1 (0448h = 1): bit 0 front layer (0 layer 0, 1 layer 1), bit 2 video layer luminance, bit 3 YS switching,
bits 5-4 palette written through FD92h-FD96h (00 layer 0 16 colors, 10 layer 1 16 colors, 01 or 11 the 256 colors).

A layer shows when the output controller selects a format for it and its FDA0h field is non-zero. In single page
mode there is no layer 1.

---

## 5. Pixel Formats and Palettes

| Format | Storage | Color |
|---|---|---|
| 16 colors | 4 bits, low nibble first | Palette of the layer: 4-bit B, R, G in the high nibble of each component |
| 256 colors | 8 bits | 256 palette: 8-bit B, R, G |
| 32K colors | 16 bits little endian | GRB555: bits 14-10 green, 9-5 red, 4-0 blue; bit 15 transparent; no palette |

In the front layer, index 0 (16 colors) or bit 15 (32K colors) lets the back layer through. Each of the two
16-color palettes has 16 entries of 12-bit color; the 256-color palette has 24-bit color.

---

## 6. VSYNC and Status

The VSYNC interrupt is IRQ11 (vector 4Bh with the BIOS setup). It stays pending until a write to 05CAh, independent
of the PIC EOI. The VSYNC and HSYNC levels are readable at FDA0h, at CFF86h (bit 2 VSYNC, bit 7 HSYNC, bit 4 always
1) and in the FR status.

---

## 7. VRAM Write Mask

Four mask bytes, one per byte lane (address & 3). Register 0 (0458h = 0) holds lanes 0-1 and register 1 holds lanes
2-3. A mask bit of 1 lets the CPU write that VRAM bit; 0 keeps the VRAM bit. Reads are not masked.

---

## 8. FM-R Compatible View

At C0000h-C7FFFh, layer 0 VRAM appears as four 1-bit planes for FM-R software: one byte is 8 pixels of one plane.

| Register | Function |
|---|---|
| CFF81h | Bits 3-0 planes written (a write goes to every selected plane), bits 7-6 plane read |
| CFF82h | Bits 2-0 and 5 visible planes C0-C2 and C3, bit 4 display page (+20000h in VRAM) |
| CFF83h | Bit 4 page accessed through the window (+20000h) |
| CFF86h | Sync status |
| CFF94h-CFF97h | Kanji ROM: JIS code high and low, then two bytes per glyph row |
| CFF99h | Bit 0 ANK font window select |

The Model 1/2 BIOS also reaches CFF81h through the I/O alias FF81h. FM-R text output is software drawn into the
layers; the text RAM at C8000h is held in Sprite RAM. See the memory_map resource.
