---
name: geartowns-debugging
description: >-
  Debug and trace FM Towns games and software using the Geartowns emulator MCP
  server. Provides workflows for Intel 80386 CPU debugging, breakpoint
  management, hardware inspection, disassembly analysis, and execution
  tracing. Use when the user wants to debug an FM Towns game, trace code
  execution, inspect i386 registers, segment descriptors, paging or hardware
  state, set breakpoints, analyze interrupts, step through 80386 instructions,
  reverse engineer game code, examine CRTC/VRAM/sprite/YM3438/RF5C68
  registers, view the call stack, or diagnose rendering, audio, or timing
  issues. Also use when the user mentions FM Towns development, Towns OS,
  FM Towns Marty, i386 or 80386 debugging, DOS-extender games, protected mode
  code, CD-ROM game debugging, floppy boot debugging, or FM Towns debugging
  with Geartowns.
compatibility: >-
  Requires the Geartowns MCP server. Direct tool mode is the default. Before
  installing or configuring, call debug_get_status to check if the server is
  already connected. If --mcp-router is enabled, use get_tool_info and
  execute_tool for routed tools.
metadata:
  author: drhelius
  version: "1.0"
---

# FM Towns Game Debugging with Geartowns

## Overview

Debug FM Towns games using the Geartowns emulator as an MCP server. Control execution (pause, step, breakpoints), inspect the Intel 80386 CPU and hardware (CRTC, VRAM layers, sprite engine, YM3438 FM, RF5C68 PCM, 8259A PIC, 8253 PIT, uPD71071 DMA, CD-ROM and floppy controllers), read/write memory, disassemble code, trace instructions, and capture screenshots — all through MCP tool calls. Hardware documentation is available in the [references/](references/) directory.

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

### Firmware

The FM Towns needs its firmware (FMT_SYS.ROM and companion files). Set the BIOS directory in the emulator before starting, or call `load_bios` with the directory path. `get_media_info` reports whether the firmware is ready.

### Hardware Documentation (References)

FM Towns hardware documentation is available in the [references/](references/) directory. Load them into your context when investigating specific hardware.

| Reference | File | Load when... |
|---|---|---|
| Intel 80386 CPU | [references/i386_cpu.md](references/i386_cpu.md) | Registers, real/protected/VM86 modes, segmentation, paging, exceptions, IDT, FM Towns IRQ vectors |
| Memory Map | [references/memory_map.md](references/memory_map.md) | Physical map, low memory banking, VRAM views, CMOS, debugger memory areas |
| I/O Ports | [references/io_ports.md](references/io_ports.md) | Every decoded I/O port, IRQ lines, DMA channels, timer counters |
| System Devices | [references/system.md](references/system.md) | PIC, PIT, DMA, clock, system control, keyboard, game ports |
| Video | [references/video.md](references/video.md) | CRTC, layers, video modes, palettes, FM-R compatible display |
| Sprites | [references/sprites.md](references/sprites.md) | Sprite engine, Sprite RAM entries, patterns, color tables |
| Sound | [references/sound.md](references/sound.md) | YM3438 FM, RF5C68 PCM, electronic volumes, sound interrupts |
| CD-ROM | [references/cdrom.md](references/cdrom.md) | CD-ROM controller ports, commands, status packets, CD-DA, DMA transfer |
| Floppy | [references/floppy.md](references/floppy.md) | MB8877 floppy controller, drive control, D77 disk images |

---

## Debugging Workflow

### 1. Load and Orient

```
load_bios → load_media → get_media_info → get_i386_status → get_screenshot
```

Start every session by making sure the firmware is loaded (`load_bios` with the firmware directory, unless it is already configured), then load the CD image, confirming it loaded correctly with `get_media_info`, then checking CPU state and taking a screenshot to understand the current game state. Games are CD images (`.cue`, `.chd`, `.iso`, `.zip`) loaded with `load_media`. Some titles also need a boot floppy: use `insert_floppy` with a `.d77`, `.d88`, `.hdm`, `.img`, `.xdf`, `.m3u` or `.zip` image in drive 0 or 1. `list_recent_media` shows recently opened images.

Load symbols with `load_symbols` (a text file with `ADDRESS NAME` or `NAME = ADDRESS` per line) or add individual labels with `add_symbol`.

### 2. Pause and Inspect

Always call `debug_pause` before inspecting state. While paused:

- **CPU state**: `get_i386_status` — general registers, EIP, EFLAGS bits, CS:EIP with linear and physical PC, CR0/CR2/CR3, mode (real, protected, vm86), CPL, IOPL, segment descriptor caches, GDTR/IDTR/LDTR/TR, debug registers
- **Disassembly**: `get_disassembly` with `start_address` and `end_address` or `count` — decodes the current contents of memory; use `code_size` (`auto`, `16`, `32`) to force 16-bit or 32-bit decoding and `detailed` for control flow, targets, I/O port names and interrupt vector names
- **Call stack**: `get_call_stack` — calls, interrupts and exceptions with their vectors
- **Memory**: `read_memory` with a memory area ID (use `list_memory_areas` to discover available areas and their IDs)
- **Debugger state**: `debug_get_status` — paused state, what stopped execution, linear and logical PC, CPU mode, halted

### Addresses

Every tool that takes an Intel 80386 address accepts:

- `1234ABCD`, `0x1234ABCD` or `$1234ABCD` — a linear address
- `CS:1234` — a segment register (CS, DS, ES, FS, GS, SS) and an offset, using its cached base
- `0008:00001234` — a selector in protected mode (read from the GDT or LDT) or a segment in real and VM86 mode, and an offset

Use `translate_address` to walk an address through segmentation and paging (PDE, PTE, page flags, physical address, region) or to find out why it fails. `get_page_directory` lists the present page tables. `get_i386_descriptors` decodes GDT, LDT and IDT entries (in real mode the IDT is the interrupt vector table).

### 3. Set Breakpoints

Use breakpoints to stop execution at points of interest:

| Breakpoint Type | Tool | Use Case |
|---|---|---|
| Execution | `set_breakpoint` (type: execute) | Stop before the instruction at the address runs |
| Read | `set_breakpoint` (type: read) | Stop when memory is read |
| Write | `set_breakpoint` (type: write) | Stop when memory is written |
| Access | `set_breakpoint` (type: access) | Stop on any read or write |
| I/O port | `set_breakpoint` (space: io) | Stop on IN/OUT/INS/OUTS to a port |
| Range | `set_breakpoint_range` | Cover an address range (same type and space) |
| Interrupt | `set_breakpoint_on_interrupt` | Stop on entry to an interrupt vector (0-255) |
| IRQ | `set_breakpoint_on_irq` | Stop when the PIC delivers an IRQ line (0-15), whatever its vector |

Breakpoints support three address spaces (`space`): `linear` (default), `physical` and `io`. Execute breakpoints are linear only. Interrupt breakpoints take a `source` (`any`, `exception`, `hardware`, `software`) and stop before the handler's first instruction.

**Important**: Read, write and access breakpoints stop *after* the instruction that made the memory access, so the PC is at the following instruction. Execute breakpoints stop before the instruction runs. `debug_get_status` reports in `breakpoint` what stopped execution (kind, space, address, size, or the vector and source of an interrupt).

Manage breakpoints with `list_breakpoints`, `remove_breakpoint`, `list_breakpoints_on_interrupt`, `clear_breakpoint_on_interrupt`, `list_breakpoints_on_irq`, `clear_breakpoint_on_irq`.

### 4. Step Through Code

After hitting a breakpoint or pausing:

| Action | Tool | Behavior |
|---|---|---|
| Step Into | `debug_step_into` | Execute one instruction, enter calls and interrupts |
| Step Over | `debug_step_over` | Execute one instruction, run through CALL subroutines |
| Step Out | `debug_step_out` | Run until the current subroutine or interrupt handler returns |
| Step Frame | `debug_step_frame` | Run one frame with breakpoints active, then pause |
| Run To | `debug_run_to_cursor` | Continue until PC reaches target address |
| Continue | `debug_continue` | Resume normal execution |
| Reset | `debug_reset` | Reset the emulated FM Towns |

After each step, call `get_i386_status` and `get_disassembly` to see where you are.

### 5. Trace Execution

The trace logger records executed instructions (CS:EIP, linear address, mode, bytes, Intel syntax) interleaved with hardware events (IRQ requests, interrupt entries, I/O port accesses, DMA, CD-ROM and floppy controller commands, VSYNC).

1. `set_trace_log` with `enabled: true` to start recording (optionally set `filters`, `registers` and `output`)
2. Let the game run or step through code (the trace records while the debugger runs the machine)
3. `set_trace_log` with `enabled: false` to stop (entries are preserved)
4. `get_trace_log` to read recorded entries (`start` negative reads the last N lines, `count` up to 1000)

Available trace event filters: `cpu`, `interrupt`, `io`, `dma`, `cdrom`, `fdc`, `vsync`. Set `registers: true` to add the general registers and EFLAGS to instruction lines. `output: "disk"` also streams the trace to a text file (`output_path`, `disk_size`) for long captures; `memory_size` sets how many entries are kept in memory.

Tracing is essential for understanding timing-sensitive code, interrupt handlers, and hardware interaction sequences.

### 6. Profile

`set_profiler` (`start`, `stop`, `reset`) collects calls and CPU cycles per function and interrupt vector while the debugger runs the machine. `get_profiler_data` returns the results (`sort`: `inclusive`, `exclusive`, `calls`, `average`, `max`; `count`; `filter`). Use it to find the hot routines of a game before stepping through them.

---

## Hardware Inspection

### CPU (Intel 80386)

- `get_i386_status` — full CPU state: registers, EFLAGS, CR0/CR2/CR3, mode, CPL, IOPL, descriptor caches, GDTR/IDTR/LDTR/TR, debug and test registers
- `write_i386_register` — modify a register live (EAX-EDI, EIP, EFLAGS, CR0, CR2, CR3, DR0-DR7, CS-GS). Segment writes reload base and limit in real and VM86 mode and load the descriptor in protected mode
- `get_i386_descriptors` — decode `gdt`, `ldt` or `idt` entries (`table`, `start`, `count`)
- `get_page_directory` — present page-directory entries, or with `index` the present pages of that page table

### Interrupt Controllers, Timers, DMA and Clock

- `get_pic_status` — both 8259A PICs: per-IRQ source, request, in-service, mask and vector; ICW1-ICW4
- `get_pit_status` — the board timer register (0060h) and the six 8253 counters: mode, reload, live count, OUT, frequency
- `get_dma_status` — uPD71071 DMA: mask, requests, terminal count and each channel's device, mode, address and count
- `get_rtc_status` — MSM58321 clock: date, time and registers
- `get_system_status` — machine model, CPU, RAM, reset cause, memory windows (0404h/0480h/0484h), CMOS write protect, serial ID ROM
- `get_keyboard_status` — keyboard interface: data, status, IRQ enable, queued bytes and held keys

### Video (CRTC, VRAM, Sprites)

- `get_crtc_status` — dot clock, line and frame timing, beam position, the VSYNC IRQ, and per layer format, windows, VRAM start, stride, zoom and visible size
- `get_crtc_registers` — the 32 CRTC registers R00-R1F with names
- `write_crtc_register` — write a CRTC register (0-31)
- `get_video_output_status` — output controller: mode, layer formats, front layer, palette select, layer enables, VRAM write mask
- `get_palettes` — the 16-color layer palettes, the 256-color palette and the FM-R digital palette (`palette`: `all`, `layer0`, `layer1`, `256`, `digital`)
- `get_frame_buffer` — a VRAM buffer as PNG (`buffer`: `layer0`, `layer1`, `sprite_display`, `sprite_draw`, or `custom` with `offset`, `format`, `width`, `height`, `palette`)
- `list_sprites` — sprite engine state and entries with position, pattern, colors and flags (`start`, `count`, `filter`: `all`, `drawn`, `visible`)
- `get_sprite` — one sprite entry as an 8x PNG (`format`: `image`) or its details (`info`)

### Audio (YM3438, RF5C68)

- `get_ym3438_status` — YM3438 FM: LFO, channel 3 mode, DAC, timers, per channel frequency, algorithm, feedback, pan and the four operators (optional `channel` 1-6)
- `get_ym3438_registers` — the register file of one part (`part` 0 or 1)
- `get_rf5c68_status` — RF5C68 PCM: sound enable, banks, IRQ mask, and each channel's envelope, pan, step, loop and play address
- `get_sound_status` — electronic volumes, FM and PCM mutes, output gate, sound interrupt causes and mask
- `set_audio_mute` — mute `fm`, `pcm` or `cdda` (or one `channel`) in the debugger without changing emulated state; useful to isolate a sound source

### CD-ROM and Floppy

- `get_cdrom_status` — CD-ROM controller: master status, last command and parameters, status queue, transfer mode, drive state, head position
- `list_cdrom_tracks` — the disc TOC: track types, start and end MSF, LBA, length
- `get_cdrom_audio_status` — CD-DA playback state, position and volume
- `read_cdrom_sector` — read one sector of the image by `lba` (`mode`: `user` 2048 bytes or `raw` 2352 bytes). Does not move the drive head
- `get_fdc_status` — MB8877 floppy controller: decoded command, registers, BUSY/DRQ/INTRQ, drive control
- `list_floppy_drives` — both drives: image, disk name, media, geometry, write protect, head position, motor
- `list_floppy_sectors` — sector IDs of a track (`drive`, `cylinder`, `head`)
- `read_floppy_sector` — read a sector by ID (`drive`, `cylinder`, `head`, `sector`)

### Screenshots

- `get_screenshot` — current rendered frame as PNG

Use screenshots after stepping or continuing to see the visual impact of changes.

---

## Common Debugging Scenarios

### Finding the Boot Path

1. `debug_pause` right after `load_bios` — the CPU starts in real mode at the reset vector inside the System ROM
2. `get_disassembly` at `CS:EIP` to follow the firmware boot code
3. Set execution breakpoints along the way, then `debug_continue`
4. Once the game is loaded from CD or floppy, `get_call_stack` and `add_symbol` to label the entry points

### Finding an Interrupt Handler

1. `get_pic_status` to read the vectors programmed into the master and slave PICs (the FM Towns BIOS uses 40h-47h and 48h-4Fh; IRQ11 is VSYNC, IRQ9 is the CD-ROM, IRQ6 is the floppy)
2. `set_breakpoint_on_irq` with the IRQ line (it follows the line whatever vector the PIC uses), or `set_breakpoint_on_interrupt` with that `vector` (`source`: `hardware`), or read the handler address with `get_i386_descriptors` (`table: idt`) and set an execution breakpoint there
3. `debug_continue` to run until the IRQ fires
4. `get_i386_status` + `get_disassembly` to see the handler code
5. `get_call_stack` to see how deep you are
6. `add_symbol` to label the handler address and any subroutines it calls

### Protected Mode and DOS Extender Code

Towns OS runs MS-DOS in real mode and starts 32-bit protected-mode applications through a DOS extender, so game code is often 32-bit with selectors and paging.

1. `get_i386_status` — check `mode` (real, protected, vm86), CPL, CR0.PE and CR0.PG
2. `get_i386_descriptors` (`table: gdt`) — decode the selectors the game loaded; `get_page_directory` if paging is on
3. `translate_address` on an `0008:00001234` style address to get the linear and physical address
4. `get_disassembly` with `code_size: 32` (or `auto`) for 32-bit code
5. To catch the switch into protected mode, set a write breakpoint or step until CR0 bit 0 becomes set, then read the GDT

### Diagnosing Graphics Corruption

1. `debug_pause` → `get_crtc_status` and `get_crtc_registers` — check layer formats, windows, VRAM start and stride, zoom
2. `get_video_output_status` — verify the video mode, layer enables and the front layer
3. `get_palettes` — verify palette contents for the active mode
4. `get_screenshot` and `get_frame_buffer` (`layer0`, `layer1`, `custom`) — compare what is on screen with what is in VRAM, including off-screen areas
5. `list_sprites` — check the sprite engine and entries (sprites are drawn into layer 1 VRAM)
6. Set write breakpoints (`space: physical`) on VRAM (physical 80000000h, or 80100000h for the single-page view) or Sprite RAM (81000000h) addresses to catch the corruption source

### Analyzing a Subroutine

1. `set_breakpoint` at the subroutine entry point
2. `debug_continue` → when hit, `get_i386_status`
3. Step through with `debug_step_into` / `debug_step_over`
4. After each step: check registers, read relevant memory
5. `add_symbol` for the routine and any called subroutines
6. `add_disassembler_bookmark` to mark interesting locations

### Tracking a Variable

1. `add_memory_watch` on the variable's address — watches are visible in the emulator GUI
2. Set a write breakpoint with `set_breakpoint` (type: write) on that address
3. When hit, `get_disassembly` around the previous instructions reveals what code is modifying it
4. `get_call_stack` shows the call chain leading to the write

### Timing Analysis

1. `set_trace_log` with `enabled: true` and `filters` such as `interrupt`, `vsync`, `io` to start recording
2. Let the game run through the section of interest
3. `get_trace_log` to see the interleaved CPU + hardware events
4. Check `get_pit_status` and `get_crtc_status` for timer and VSYNC timing
5. Correlate IRQs and I/O accesses with code execution in the trace; use `set_profiler` for per-function cycle costs

### Sound Debugging

1. `get_sound_status` — check the electronic volumes, mutes and the sound interrupt state
2. `get_ym3438_status` / `get_rf5c68_status` — see which FM and PCM channels are active and their notes, envelopes and addresses
3. `set_audio_mute` — mute individual sources or channels to find which one produces a sound
4. Set an I/O breakpoint (`space: io`) on the sound ports (see [references/sound.md](references/sound.md)) to find the code that programs them
5. `get_cdrom_audio_status` for CD-DA music

### CD-ROM Game Debugging

1. `load_media` with a `.cue`, `.chd`, `.iso` or `.zip` file to load a CD image
2. `list_cdrom_tracks` to see the TOC and `get_cdrom_status` to verify drive state
3. `set_trace_log` with `filters: ["cpu", "cdrom", "dma", "interrupt"]` to trace CD commands, DMA transfers and the CD-ROM IRQ
4. `get_dma_status` to inspect the DMA channel used for the transfer (channel 3 for CD-ROM data)
5. Set breakpoints on the destination RAM (write type) to catch data loading, and `read_cdrom_sector` to compare against the disc contents
6. `get_cdrom_audio_status` to inspect CD-DA playback

### Floppy Boot Debugging

1. `insert_floppy` with the disk image in `drive` 0 (use `set_floppy_write_protect` to keep the image untouched)
2. `list_floppy_drives` and `list_floppy_sectors` to confirm the image geometry and sector IDs
3. `set_trace_log` with `filters: ["cpu", "fdc", "dma", "interrupt"]` to trace controller commands and IRQ6
4. `get_fdc_status` to inspect the MB8877 command and registers while paused
5. `read_floppy_sector` to compare what the game read with the image contents
6. `eject_floppy` or `swap_floppies` to change disks, as the game would ask for

### Keyboard and Mouse Input

- `keyboard_key` presses, releases or taps a key of the JIS keyboard by name (RETURN, SPACE, A, 1, PF1, SHIFT, CTRL, HIRAGANA...)
- `keyboard_type` types ASCII text, useful at Towns OS and DOS prompts
- `controller_set_type` selects the device on a game port (`none`, `original_gamepad`, `marty_gamepad`, `six_button_gamepad`, `mouse`) and `controller_get_type` reads it
- `controller_button` presses, releases or taps pad buttons; on a mouse port the directions move the mouse and A/B are the left/right buttons
- `get_input_state` shows each port's device and the pressed buttons and keys

---

## Memory Areas

Use `list_memory_areas` to get the full list with IDs, sizes and the `physical_base` of mapped regions. Common areas:

| Area | Description | Typical Size |
|---|---|---|
| LINEAR | 4 GB linear space; also accepts `SR:offset` and `selector:offset` addresses | 4GB |
| PHYSICAL | 4 GB physical space (after paging) | 4GB |
| I/O PORTS | I/O port space, read without side effects (read-only) | 64KB |
| Main RAM | System RAM at physical 0 | 1-10MB |
| VRAM | Video RAM (raw, two-page view and single-page view) | 512KB |
| Sprite RAM | Sprite entries, color tables, patterns (physical 81000000h) | 128KB |
| CMOS RAM | Battery backed learning RAM | 8KB |
| PCM wave RAM | RF5C68 wave RAM (raw 64KB, and the 4KB CPU window) | 64KB |
| System ROM | Boot code, reset vector at FFFFFFF0h | 256KB |
| OS ROM | MS-DOS and the BIOS | 512KB |
| Dictionary ROM | Dictionary data | 512KB |
| Font ROM | Kanji font | 256KB |
| Media images | The loaded CD image and inserted floppy images | Varies |

Offsets in region areas are 0-based. ROMs, media images and I/O PORTS cannot be written.

---

## Organizing Your Debug Session

- **Symbols**: Use `add_symbol` liberally to label addresses you've identified — makes disassembly readable. Use `list_symbols`, `lookup_symbol_by_name` and `lookup_symbol_at_address` to review
- **Bookmarks**: Use `add_disassembler_bookmark` for code locations and `add_memory_bookmark` for data regions
- **Watches**: Use `add_memory_watch` for variables you're tracking across steps (8, 16, 32 or 64 bits)
- **Save states**: Use `save_state` / `load_state` to snapshot and restore emulator state at interesting points
- **Rewind**: Use `get_rewind_status` + `rewind_seek` to scrub back through recent execution history without manual save states
- **Screenshots**: Capture visual state with `get_screenshot` after significant changes

---

## Rewind (Time Travel Debugging)

The emulator continuously records snapshots into a ring buffer during gameplay. You can seek to any recorded snapshot to restore full emulator state at that point in time — like time travel debugging.

### Workflow

1. **Check availability**: `get_rewind_status` — returns snapshot count, capacity, buffered seconds
2. **Pause**: `debug_pause` — the emulator must be paused before seeking
3. **Seek**: `rewind_seek` with a snapshot number (1 = oldest, snapshot_count = newest)
4. **Inspect**: `get_i386_status`, `get_disassembly`, `get_screenshot`, `read_memory`, etc.
5. **Iterate**: Seek to different snapshots to narrow down when a bug first appeared
6. **Resume or continue debugging**: `debug_continue` to resume from the seeked state

### Tools

| Tool | Description |
|---|---|
| `get_rewind_status` | Snapshot count, capacity, buffered seconds, configuration |
| `rewind_seek` | Jump to snapshot N (1=oldest, count=newest). Non-destructive — can seek repeatedly |

### Key Details

- **Non-destructive seeking**: `rewind_seek` loads a snapshot without removing it. You can seek to the same snapshot multiple times, or jump between different snapshots freely.
- **Snapshot numbering**: Snapshot 1 is the oldest available, snapshot_count is the newest (most recent).
- **Buffer size**: Configured by the user. When full, oldest snapshots are overwritten.
- **Granularity**: Snapshots are taken every N frames (configurable).

### Bug Reproduction with Rewind

1. Let the game run past the bug occurrence
2. `debug_pause` → `get_rewind_status` to see how far back you can go
3. Binary search with `rewind_seek`: try the midpoint, check if the bug is visible (`get_screenshot`), then narrow the range
4. Once you find the exact snapshot where the bug appears, inspect CPU/memory state
5. Set breakpoints at the relevant code, then `rewind_seek` to a snapshot just before the bug and `debug_continue`
