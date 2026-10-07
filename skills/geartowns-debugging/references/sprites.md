# FM Towns — Sprites

## 1. Overview

The sprite engine draws up to 1024 16×16 sprites from Sprite RAM into one half of layer 1 VRAM while the CRTC shows
the other half. Sprites are pixels written into VRAM, not an overlay. Layer 1 must be in two-page 32K color mode
with a 512 byte stride (the 256×512 surface) and in front of layer 0.

| Sprite RAM offset | Contents |
|---|---|
| 0000h-1FFFh | 1024 entries of 8 bytes |
| 2000h-3FFFh | Color tables 256-511, 32 bytes each (16 colors × 16 bits) |
| 4000h-1FFFFh | Patterns 128-1023 |

Sprite RAM is at physical 81000000h. Pattern and color table numbers are addresses in units of 128 and 32 bytes, so
the valid ranges are the ones above.

---

## 2. Registers (index 0450h, data 0452h)

| Index | Contents |
|---:|---|
| 0 | First entry, bits 7-0 |
| 1 | Bit 7 SPEN (engine on), bits 1-0 first entry bits 9-8 |
| 2/3 | X offset, 9 bits (bit 8 in register 3 bit 0) |
| 4/5 | Y offset, 9 bits (bit 8 in register 5 bit 0) |
| 6 | Write bit 7 DP1 (displayed page while SPEN is clear); read DP1 in bit 4 |

The engine processes entries from the first entry to 1023 in ascending order, so later entries draw on top. First
entry 0 draws 1024 entries; 1023 draws one. Reset leaves SPEN clear.

044Ch reads bit 1 busy and bit 0 PAGE, the half being drawn.

---

## 3. Entry (8 bytes)

| Offset | Word | Fields |
|---:|---|---|
| +0 | X | Bits 8-0 |
| +2 | Y | Bits 8-0 |
| +4 | Attributes | Bit 15 add the X/Y offsets, bits 14-12 rotation, bit 11 half height, bit 10 half width, bits 9-0 pattern |
| +6 | Color | Bit 15 CTEN: 16-color pattern with color table; bit 14 SPYS; bit 13 hide; bits 11-0 color table |

Rotation codes (bits 14-12):

| Code | Result | Code | Result |
|---:|---|---:|---|
| 000 | Normal | 100 | 270°, mirrored |
| 001 | 180°, mirrored (vertical flip) | 101 | 270° |
| 010 | Mirrored (horizontal flip) | 110 | 90° |
| 011 | 180° | 111 | 90°, mirrored |

Bit 14 swaps X and Y, bit 13 flips X and bit 12 flips Y, the flips applied after the swap. Halving applies to the
screen axes.

---

## 4. Patterns and Colors

| Kind | Address in Sprite RAM | Size | Pixel |
|---|---|---|---|
| 16 colors (CTEN = 1) | pattern × 128 | 128 bytes, 8 per row, low nibble first | Index 0 transparent, else the color table word |
| 32K colors (CTEN = 0) | pattern × 128, pattern a multiple of 4 | 512 bytes, 32 per row | GRB555, bit 15 transparent |

Color table address = table × 32; each of its 16 entries is a GRB555 word and entry 0 is transparent. Both kinds of
pattern share the pattern area: 896 16-color patterns, 224 32K-color patterns or a mix.

SPYS (bit 14) is the superimpose control: it is written as bit 15 of every pixel the entry draws, and bit 15 makes a
layer 1 pixel transparent, so a sprite with SPYS set punches a hole in layer 1 that shows layer 0 behind it.

---

## 5. Position and Clipping

Screen X = (X + X offset if bit 15) & 1FFh, and the same for Y. Only the 256×256 area at 0-255 is drawn, and
coordinates wrap at 512, so a sprite near 511 appears partially at the opposite edge and a sprite placed outside
the area is hidden. Lines 0 and 1 of each page hold the clear pattern and never receive sprite pixels. With a
256×240 screen the visible bottom line is Y = 239.

---

## 6. Pages and Timing

| | SPEN set | SPEN clear |
|---|---|---|
| Page drawn | PAGE (044Ch bit 0) | None |
| Page shown by layer 1 | The other page | DP1 |

The halves are at VRAM 40000h (page 0) and 60000h (page 1), two-page order. A transfer starts when VSYNC ends
while SPEN is set and the previous transfer has finished; the halves swap, the engine clears the new drawing half by copying its first two lines over the
rest (32 µs) and then processes each entry (75 µs per entry, visible or not). A full list of 1024 entries takes
about 77 ms and continues over several frames; busy stays set until the transfer ends.
