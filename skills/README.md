# Geartowns Agent Skills

[Agent Skills](https://agentskills.io/) for the Geartowns FM Towns emulator MCP server. These skills teach AI agents how to effectively use Geartowns's MCP tools for debugging and game hacking tasks.

## Prerequisites

All skills require the **Geartowns emulator** running as an MCP server. The emulator must be configured in your AI client (VS Code, Claude Desktop, Claude Code, etc.) so the agent can access the MCP tools.

See [MCP_README.md](../MCP_README.md) for complete setup instructions (STDIO, HTTP, VS Code, Claude Desktop, Claude Code).

## Installation

The recommended way to install the skills is using the [`skills`](https://skills.sh/docs) CLI, which requires no prior installation:

```bash
npx skills add drhelius/geartowns
```

Or install a specific skill:

```bash
npx skills add drhelius/geartowns --skill geartowns-debugging
npx skills add drhelius/geartowns --skill geartowns-romhacking
```

This downloads and configures the skills for use with your AI agent. See the [skills CLI reference](https://skills.sh/docs/cli) for more details.

## Available Skills

### geartowns-debugging

**Purpose**: Game development, debugging, and tracing of FM Towns games and software.

**What it covers**:
- Loading the firmware, CD images, boot floppies, and debug symbols
- Intel 80386 register, flag, mode, and segment descriptor inspection (real, protected, and VM86 modes)
- Setting execution, read, write, access, I/O port, range, and interrupt breakpoints
- Address translation through segmentation and paging
- Stepping through code (into, over, out, frame, run-to)
- Execution tracing with interleaved hardware events (interrupts, I/O, DMA, CD-ROM, floppy, VSYNC)
- Hardware inspection: CRTC and video output, VRAM layers, sprite engine, YM3438 FM, RF5C68 PCM, 8259A PIC, 8253 PIT, uPD71071 DMA, CD-ROM and floppy controllers
- Sprite viewer and VRAM layer capture with image output
- Screenshot capture
- Call stack analysis and CPU profiling
- Organizing debug sessions with symbols, bookmarks, and watches

**Key MCP tools used**: `debug_pause`, `debug_step_into`, `debug_step_over`, `debug_step_out`, `set_breakpoint`, `set_breakpoint_on_interrupt`, `get_i386_status`, `get_i386_descriptors`, `translate_address`, `get_disassembly`, `get_call_stack`, `set_trace_log`, `get_trace_log`, `get_crtc_registers`, `get_pic_status`, `get_ym3438_status`, `list_sprites`, `add_symbol`, `get_screenshot`

**Example prompts**:
- "Find the VSYNC interrupt handler and analyze what it does"
- "Set a breakpoint at CS:2140 and step through the code"
- "The game has corrupted graphics — diagnose the CRTC registers and VRAM layers"
- "The game switches to protected mode during boot — stop after it loads the GDT and decode every descriptor"
- "Debug the CD-ROM loading sequence and trace the DMA transfers"

### geartowns-romhacking

**Purpose**: Creating modifications, cheats, translations, and hacks for FM Towns games.

**What it covers**:
- Memory search workflows (capture → change → compare cycle)
- Finding game variables (lives, health, score, position)
- Creating cheats (infinite lives, score modification, etc.)
- Text and Shift-JIS string discovery for translations
- Sprite and graphics data location via sprite viewer and VRAM inspection
- Data table and structure reverse engineering
- Reading CD and floppy sectors to locate data on the media
- Save state management for safe experimentation
- Fast forwarding to reach specific game states

**Key MCP tools used**: `memory_search_capture`, `memory_search`, `memory_find`, `read_memory`, `write_memory`, `translate_address`, `set_breakpoint` (write type), `add_memory_watch`, `add_memory_bookmark`, `save_state`, `load_state`, `toggle_fast_forward`, `get_screenshot`, `list_sprites`, `get_sprite`, `get_frame_buffer`, `read_cdrom_sector`, `controller_button`, `keyboard_key`

**Example prompts**:
- "Find the lives counter and give me infinite lives"
- "Search for the score variable in memory"
- "Find all Shift-JIS text strings in this game for translation"
- "Locate the sprite data for the player character"
- "Create a cheat for maximum health in this CD-ROM game"
