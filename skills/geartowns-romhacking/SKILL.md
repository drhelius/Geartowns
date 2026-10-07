---
name: geartowns-romhacking
description: >-
  Hack, modify, and translate FM Towns games and software using the Geartowns
  emulator MCP server. Provides workflows for memory searching, value
  discovery, cheat creation, data modification, sprite/text finding, and
  translation patching. Use when the user wants to create cheats, find game
  values in memory, modify game data, translate an FM Towns game, patch game
  behavior, create game hacks, discover hidden content, change sprites or
  graphics, find Shift-JIS text strings, do infinite lives or health hacks,
  search for score or item counters, or reverse engineer data structures in FM
  Towns or FM Towns Marty CD-ROM and floppy games. Also use for any game
  hacking, memory poking, or game modification task involving Geartowns.
compatibility: >-
  Requires the Geartowns MCP server. Direct tool mode is the default. Before
  installing or configuring, call debug_get_status to check if the server is
  already connected. If --mcp-router is enabled, use get_tool_info and
  execute_tool for routed tools.
metadata:
  author: drhelius
  version: "1.0"
---

# FM Towns Game Hacking with Geartowns

## Overview

Hack, modify, and translate FM Towns games using the Geartowns emulator as an MCP server. Search memory for game variables, create cheats, find text strings for translation, locate sprite and graphics data, and reverse engineer data structures — all through MCP tool calls. Use save states as checkpoints and fast forward to reach specific game states.

## MCP Server Prerequisite

**IMPORTANT — Check before installing:** Before attempting any installation or configuration, you MUST first verify if the Geartowns MCP server is already connected in your current session. In the default mode, call `debug_get_status` directly. If Geartowns was intentionally started with `--mcp-router`, call `get_tool_info` with `{"name":"debug_get_status"}`, then call `execute_tool` with `{"name":"debug_get_status","arguments":{}}`. A valid response from either workflow means the server is active and ready.

Only if neither workflow is available or the call fails, you need to help install and configure the Geartowns MCP server:

### Installing Geartowns

Run the bundled install script (macOS/Linux):

```bash
bash scripts/install.sh
```

This installs Geartowns via Homebrew on macOS or downloads the latest release on Linux. It prints the binary path on completion. You can also set `INSTALL_DIR` to control where the binary goes (default: `~/.local/bin`).

Alternatively, download from [GitHub Releases](https://github.com/drhelius/Geartowns/releases/latest) or install with `brew install --cask drhelius/geardome/geartowns` on macOS.

### Connecting as MCP Server

Configure your AI client to run Geartowns as an MCP server via STDIO transport. Example for Claude Desktop (`~/Library/Application Support/Claude/claude_desktop_config.json`):
```json
{
  "mcpServers": {
    "geartowns": {
      "command": "/path/to/geartowns",
      "args": ["--mcp-stdio"]
    }
  }
}
```
Replace `/path/to/geartowns` with the actual binary path from the install script. Add `--headless` before `--mcp-stdio` on headless machines.

### Firmware and Media

The FM Towns needs its firmware (FMT_SYS.ROM and companion files). Set the BIOS directory in the emulator before starting, or call `load_bios` with the directory path. Then load the game with `load_media` (CD image: `.cue`, `.chd`, `.iso`, `.zip`) and, if the game needs a boot floppy, `insert_floppy` (`.d77`, `.d88`, `.hdm`, `.img`, `.xdf`, `.m3u`, `.zip`). `get_media_info` confirms what is loaded.

### Hardware Documentation (References)

FM Towns hardware documentation is available in the [references/](references/) directory. Load them into your context when you need data formats, memory layout, or hardware details.

| Reference | File | Load when... |
|---|---|---|
| Memory Map | [references/memory_map.md](references/memory_map.md) | Physical map, VRAM views, Sprite RAM, CMOS, debugger memory areas |
| Intel 80386 CPU | [references/i386_cpu.md](references/i386_cpu.md) | Registers, modes, segmentation, paging, address forms |
| I/O Ports | [references/io_ports.md](references/io_ports.md) | I/O port map, kanji ROM ports, device registers |
| System Devices | [references/system.md](references/system.md) | PIC, PIT, DMA, clock, system control, keyboard, game ports |
| Video | [references/video.md](references/video.md) | CRTC, layers, video modes, palettes, pixel formats |
| Sprites | [references/sprites.md](references/sprites.md) | Sprite engine, entry format, patterns, color tables |
| Sound | [references/sound.md](references/sound.md) | YM3438 FM, RF5C68 PCM, wave RAM |
| CD-ROM | [references/cdrom.md](references/cdrom.md) | Sector addressing (LBA, MSF), data and audio tracks |
| Floppy | [references/floppy.md](references/floppy.md) | Floppy disk images, sector layout, D77 format |

---

## Core Technique: Memory Search

Memory search is the primary tool for game hacking. It uses a capture → change → compare cycle to isolate memory addresses holding game values. Search the **Main RAM** area (use `list_memory_areas` to get its ID); offsets in Main RAM are physical addresses starting at 0.

### The Search Loop

```
1. memory_search_capture    → snapshot current memory state
2. (change the value in-game using controller_button, keyboard_key, fast forward, etc.)
3. memory_search            → compare against snapshot to find changed addresses
4. Repeat 2-3 until only a few candidates remain
5. read_memory / write_memory → verify and modify the found addresses
```

### Search Operators and Types

`memory_search` supports these **operators**: `<`, `>`, `==`, `!=`, `<=`, `>=`

**Compare types** (`compare_type`):
- `previous` — compare current value to last captured snapshot (most common)
- `value` — compare current value to a specific number (`compare_value`)
- `address` — compare current value to value at another address (`compare_value` is the area offset)

**Data types** (`data_type`): `hex`, `signed`, `unsigned`

**Value width**: `memory_search_capture` takes `width` of 8 (default), 16 or 32 bits, matching aligned little-endian values. FM Towns games run on a 32-bit CPU, so counters and scores are often 16 or 32 bits wide: capture with the width you expect. Use `start` and `size` to limit the captured range.

### Example: Finding the Lives Counter

```
1. memory_search_capture                         → snapshot with 3 lives
2. Lose a life in-game (play or use controller_button)
3. memory_search (operator: <, compare_type: previous) → values that decreased
4. memory_search_capture                         → snapshot with 2 lives
5. Lose another life
6. memory_search (operator: <, compare_type: previous) → narrow further
7. Or use: memory_search (operator: ==, compare_type: value, compare_value: 1)
   → find addresses holding exactly 1
8. write_memory on the candidate address to set lives to 99
9. get_screenshot to verify the change took effect
```

### Example: Finding a Score Counter

Score values are often stored as multi-byte (16 or 32-bit little-endian on the 80386):

```
1. memory_search_capture (width: 32)                          → snapshot at score 0
2. Score some points in-game
3. memory_search (operator: >, compare_type: previous)        → values that increased
4. memory_search_capture (width: 32)
5. Score more points
6. memory_search (operator: >, compare_type: previous)        → narrow down
7. read_memory on candidates — look for values matching current score
8. write_memory to set a custom score
```

For multi-byte values: the low byte is at address N, high byte at N+1 (the 80386 is little-endian). Some games keep the score as BCD digits or as separate digit bytes instead of a binary number; if a binary search finds nothing, search for the digits individually.

### Memory Areas and Addresses

Games running in protected mode use linear addresses, which paging may map to different physical addresses. Main RAM offsets are physical. Use `translate_address` on a `selector:offset` or linear address from the disassembler to get the physical address and the Main RAM offset, and use the LINEAR area (which also accepts `DS:offset` and `selector:offset` addresses) for `read_memory` and `write_memory` when you only know the program's address.

---

## Fast Forward for Efficiency

Use fast forward to speed through gameplay when you need to trigger in-game changes:

```
set_fast_forward_speed (4 = unlimited)
toggle_fast_forward (enabled: true)
(play through the game section)
toggle_fast_forward (enabled: false)
```

This is essential when you need to reach specific game states without waiting in real-time, such as long CD loading and intro sequences.

---

## Save States as Checkpoints

Save states are critical for game hacking — they let you save your position and retry modifications:

```
select_save_state_slot (1-5)     → pick a slot
save_state                       → save current state
(try modifications)
load_state                       → revert if something breaks
```

Use different slots for different game states (e.g., slot 1 = start, slot 2 = boss fight, slot 3 = specific level).

`list_save_state_slots` shows all slots with media name, timestamp, and validity. `save_state_file` and `load_state_file` save and load states to explicit file paths, handy for keeping many checkpoints.

### Rewind as an Alternative

The emulator also records continuous snapshots into a rewind ring buffer. Use `get_rewind_status` to check availability, then `rewind_seek` to jump to any recorded point without manual save/load. This is especially useful for quickly reverting after a failed memory write — pause, seek back a few snapshots, and retry.

---

## Finding and Modifying Game Data

### Text and String Discovery

Most Japanese FM Towns software stores text as Shift-JIS: two bytes per kanji or full-width kana, single bytes for ASCII and half-width katakana. Text is often held in data files on the CD or floppy and loaded into RAM when needed. To find text strings for translation or modification:

1. `get_screenshot` of a screen with text and note the visible text
2. Encode the Japanese text as Shift-JIS and search for the bytes with `memory_find` and `hex_bytes` (`text` matches bytes literally, so it only fits ASCII). Search Main RAM, and the disc or floppy image areas
3. If nothing matches, the text may be compressed, use another encoding, or be drawn from a different table; trace the text routine instead (see the Translation Workflow)
4. Set read breakpoints on suspected text addresses with `set_breakpoint` (type: read) to confirm they're used for rendering
5. `read_memory` around a hit to see the surrounding strings, and `get_screenshot` to correlate displayed text with memory contents

To look at files on the media itself, `read_cdrom_sector` (by `lba`, `user` mode for the 2048-byte payload) and `read_floppy_sector` return raw sector data as hex; `list_cdrom_tracks` and `list_floppy_sectors` describe the layout.

### Sprite and Graphics Data

1. `list_sprites` to see the sprite engine state and entries with position, pattern, colors, and flags
2. `get_sprite` (format: `image`) to capture individual sprite entries as PNG, and `get_frame_buffer` (`sprite_display`, `sprite_draw`) to see whole sprite pages
3. `get_frame_buffer` with `layer0`, `layer1` or `custom` (`offset`, `format`, `width`, `height`, `palette`) to view background and image data in VRAM, including off-screen areas
4. `get_crtc_status` and `get_video_output_status` for layer formats, VRAM start and stride; `get_palettes` for colors
5. `read_memory` on VRAM and Sprite RAM areas to analyze pixel, pattern and color table data
6. Set write breakpoints on VRAM or Sprite RAM addresses to find the code that draws graphics, and read breakpoints on the source data to find where graphics are loaded from
7. `get_screenshot` before/after modifications to see visual changes

Reference the video and sprite docs ([references/video.md](references/video.md), [references/sprites.md](references/sprites.md)) for layer formats, pixel formats and sprite entry layout.

### Data Tables and Structures

1. `debug_pause` → `get_disassembly` around code that loads data
2. Look for MOV/LEA instructions with absolute or indexed addressing (`[ESI+EAX*4]`, `[table+EBX]`) — these point to data tables
3. `read_memory` at the target addresses to dump the table contents (use `translate_address` if the address is linear or segmented)
4. `add_memory_bookmark` to mark discovered data regions
5. `add_symbol` to label data table entry points for future reference

---

## Creating Cheats

### Infinite Lives / Health

```
1. Find the address using the search loop (above)
2. Set a write breakpoint: set_breakpoint (type: write) on the address
3. debug_continue → when it hits, get_disassembly to see the decrement code
4. Note the instruction (e.g., DEC DWORD PTR [00012345] or MOV [ESI+0C], EAX)
5. Option A: Periodically write_memory to reset the value (simple poke cheat)
6. Option B: Identify the decrement routine for a NOP patch (90 per byte)
```

Write breakpoints stop after the instruction that wrote, so look at the instructions just before the reported PC.

### Watching Values in Real-Time

Use `add_memory_watch` on discovered addresses (8, 16, 32 or 64 bits). Watches appear in the emulator's GUI memory editor, letting you monitor values as the game runs — useful for verifying cheats work across different game situations.

### Write Breakpoint Technique

The most powerful cheat-finding technique:

1. Find the variable address via memory search
2. `set_breakpoint` (type: write) on that address
3. `debug_continue` — the emulator stops when the game writes to that address
4. `get_i386_status` + `get_disassembly` reveals the exact code modifying the value
5. `get_call_stack` shows what triggered the write
6. You now know exactly where and how the game manages that variable

---

## Translation Workflow

### 1. Identify the Font System

1. `get_screenshot` of a screen with text
2. Find text rendering code by setting write breakpoints on VRAM (graphics modes) or Sprite RAM, or I/O breakpoints (`space: io`) on the kanji ROM ports (see [references/io_ports.md](references/io_ports.md)) when the game reads glyphs from the Font ROM
3. Trace back to find the character code and the routine that converts it to a glyph
4. `get_frame_buffer` and `read_memory` to inspect the glyph bitmaps the game draws or keeps in RAM
5. `add_symbol` to label the font routine and the text drawing routine

### 2. Find String Data

1. `memory_find` with `hex_bytes` for the Shift-JIS encoding of known text, or look for sequential Shift-JIS byte pairs with `read_memory` on large ranges
2. Check whether the game loads text from a file on the media: trace CD-ROM or floppy reads with `set_trace_log` (`filters: ["cpu", "cdrom", "fdc", "dma"]`) while the text appears
3. Use `read_cdrom_sector` or `read_floppy_sector` to inspect the source data
4. `add_memory_bookmark` to mark each string location

### 3. Measure Space Constraints

Translations must fit within existing space, and Japanese text is dense (two bytes per character):

1. `read_memory` to determine how much space each string occupies
2. Check for string terminators (commonly $00, $FF, or length-prefixed)
3. If the translation is longer, look for unused space, abbreviate, or switch to single-byte ASCII if the game's text routine supports it
4. Check whether the text uses variable-width or fixed-width glyphs and whether strings are referenced by pointer tables that also need updating

### 4. Apply and Test

1. `write_memory` to patch translated strings into memory
2. `get_screenshot` to verify rendering
3. `save_state` before each change so you can `load_state` if it breaks
4. Test all screens that display modified text

---

## Memory Map Quick Reference

Use `list_memory_areas` to get the full list with IDs and sizes. Common areas:

| Area | Description | Use |
|---|---|---|
| Main RAM | System RAM at physical 0 (1MB on Model 1, 2MB on Model 2, up to 6MB expanded and 10MB on the Custom machine) | Game variables, code, data, stack |
| LINEAR / PHYSICAL | 4GB address spaces | Read/write by program address; translate with `translate_address` |
| VRAM | 512KB video RAM (raw, two-page view, single-page view) | Layer 0 and layer 1 pixels, sprite destination pages |
| Sprite RAM | 128KB at physical 81000000h | Sprite entries, color tables, patterns |
| CMOS RAM | 8KB battery backed RAM | Save data, settings |
| PCM wave RAM | RF5C68 wave RAM (64KB) | Sampled sound data |
| System ROM / OS ROM | Firmware and MS-DOS (read-only) | BIOS and OS code |
| Font ROM | Kanji font (read-only) | Glyph bitmaps |
| Media images | Loaded CD image and inserted floppy images | Raw disc and disk data |
| I/O PORTS | Hardware registers (read-only) | Device state without side effects |

Linear addresses seen in protected-mode code do not always equal Main RAM offsets, so verify with `translate_address`.

---

## Bookmarks and Organization

Keep your hacking session organized:

- `add_memory_bookmark` — mark discovered data regions, variable locations, string tables
- `add_memory_watch` — track values that change during gameplay
- `add_symbol` — label addresses in disassembly for readability
- `add_disassembler_bookmark` — mark code routines you've identified

Use `list_memory_bookmarks`, `list_memory_watches`, `list_symbols`, `list_disassembler_bookmarks` to review.

---

## Persisting Changes

Changes made via `write_memory` are applied to the emulator's live memory only — they are **not** persisted to the CD image or floppy image on disk, and ROM and media image areas cannot be written. To create a permanent patch, locate the data inside the disc or disk files (`read_cdrom_sector`, `read_floppy_sector`, `list_cdrom_tracks`, `list_floppy_sectors` help map RAM contents back to sectors) and use command-line tools (e.g., a binary patch script) to apply the discovered modifications to a copy of the actual image file.
