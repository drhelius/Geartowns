# Geartowns MCP Server

A [Model Context Protocol](https://modelcontextprotocol.io/introduction) server for the Geartowns emulator, enabling AI-assisted debugging and development of FM Towns software.

This server provides tools for game development, ROM hacking, translation, reverse engineering, and debugging through standardized MCP protocols compatible with AI agents like GitHub Copilot, Claude, Codex and others.

## Features

- **Full Debugger Access**: Intel 80386 registers, descriptor tables, paging, memory inspection, execute, data, I/O and interrupt breakpoints, and execution control
- **i386 Addressing**: Every address takes linear, `SR:offset` or `selector:offset` forms, translated through segmentation and paging
- **Multiple Memory Areas**: Linear and physical spaces, side-effect free I/O port reads, Main RAM, VRAM, Sprite RAM, CMOS, ROMs, PCM wave RAM, media images and inserted floppy images
- **Hardware Inspection**: 8259A interrupt controllers, 8253 timers, uPD71071 DMA, MSM58321 clock, system control and keyboard
- **Audio Inspection**: YM3438 FM channels, operators and registers, RF5C68 PCM channels and wave RAM, electronic volumes and sound interrupts, debugger mutes per source and channel
- **Storage Inspection**: CD-ROM controller, TOC, CD audio and disc sectors; MB8877 floppy controller, drives, track sector lists and sector data
- **Video Inspection**: CRTC timing and layers, CRTC registers, output controller, palettes, VRAM layers and sprite pages as PNG, sprite entries and patterns
- **Disassembly**: Intel-syntax 80386 disassembly from current memory, with control flow, targets, I/O port and interrupt vector names
- **Symbol Support**: User symbols (add, remove, load from files) and automatic labels
- **Bookmarks and Watches**: Memory and disassembler bookmarks, memory watches
- **Memory Search**: Value-change search and byte or text find
- **Call Stack**: Calls, interrupts and exceptions with their vectors
- **Trace Logger**: Executed instructions with registers interleaved with IRQs, interrupts, I/O, DMA, CD-ROM, FDC and VSYNC events, in memory or streamed to disk
- **Profiler**: Calls, calls per frame and inclusive/exclusive CPU cycles per function and interrupt vector
- **Input**: Pad buttons, FM Towns keyboard keys and typed text
- **Save States**: Slots and explicit files
- **Screenshot Capture**: Get current frame as PNG image
- **Video Recording**: Record emulated video and audio to AVI files on disk
- **Rewind**: Time-travel debugging with snapshot status and seek tools
- **Documentation Resources**: Built-in hardware and programming documentation for AI context
- **GUI Integration**: MCP server runs alongside the emulator GUI, sharing the same state

## Transport Modes

The Geartowns MCP server supports two transport modes:

### STDIO Transport (Recommended)

The default mode uses standard input/output for communication. The emulator is launched by the AI client and communicates through stdin/stdout pipes.

### HTTP Transport

The HTTP transport mode runs the emulator with an embedded web server on `127.0.0.1:7777/mcp` by default. The emulator stays running independently while the AI client connects via HTTP. Each request's `Host` and browser `Origin` must match the address on which its connection reached the server. Loopback mode can run without authentication; wildcard and other non-loopback bind addresses require `GEARTOWNS_MCP_HTTP_TOKEN`, and the server refuses to start without it.

### Headless Mode

Add `--headless` to run without a GUI window. This is useful for servers, CLI agents, or any machine without a display. All MCP tools work identically in headless mode. Requires `--mcp-stdio` or `--mcp-http`.

### Concurrent Clients

The HTTP server accepts repeated valid MCP initialization requests. All connected clients control the same Geartowns instance. Individual HTTP requests are serialized, but multi-request debugging workflows are not atomic. Concurrent agents can interfere with each other through pauses, resets, breakpoints, memory writes, media loads, and save states.

For independent agent tasks, run one Geartowns instance per agent on a unique HTTP port. Use `--headless` and give each instance its own portable application directory so its configuration and runtime files are isolated:

```bash
./geartowns --mcp-http --headless --portable --mcp-http-port 7778
```

The `--portable` option stores configuration and user data beside the application. Alternatively, create an empty `portable.ini` beside the executable in each application directory. On macOS, place it next to each `.app` bundle.

## MCP Tool Router

By default, Geartowns exposes every MCP tool directly. This avoids nested tool discovery in clients that already defer MCP schemas, including Claude Code.

Add `--mcp-router` to expose a compact set of high-frequency tools directly and route advanced debugger tools through lightweight discovery tools. This reduces MCP context while preserving access to the full debugger surface.

Direct tools in routed mode: `load_media`, `load_bios`, `get_media_info`, `debug_pause`, `debug_continue`, `debug_step_into`, `debug_step_over`, `debug_step_out`, `get_i386_status`, `read_memory`, `write_memory`, `get_disassembly`, `set_breakpoint`, `get_screenshot`, and `controller_button`.

Router tools:

- `list_tool_categories` lists routed tool categories with descriptions and tool counts.
- `get_category_tools` lists routed tools in a category with compact descriptions.
- `search_tools` searches direct and routed tools and returns compact category/tool/description matches.
- `get_tool_info` returns one tool's real input schema and metadata.
- `execute_tool` executes a routed tool by name. First use `search_tools` or `get_category_tools` to discover the tool, then call `get_tool_info` to obtain its exact input schema.

Example routed call:

```json
{
  "name": "translate_address",
  "arguments": {
    "address": "CS:1234"
  }
}
```

Without `--mcp-router`, call every MCP tool directly.

## Quick Start

The FM Towns needs its firmware (FMT_SYS.ROM and companion files). Set the BIOS directory in the emulator before starting, or call `load_bios` with the directory path.

### STDIO Mode with VS Code

1. **Install [GitHub Copilot extension](https://code.visualstudio.com/docs/copilot/overview)** in VS Code

2. **Configure VS Code settings**:

   Add to your workspace folder a file named `.vscode/mcp.json` with:

   ```json
   {
     "servers": {
       "geartowns": {
         "command": "/path/to/geartowns",
         "args": ["--mcp-stdio"]
       }
     }
   }
   ```

   **Important:** Update the `command` path to match your build location:
   - **macOS:** `/path/to/geartowns`
   - **Linux:** `/path/to/geartowns`
   - **Windows:** `C:/path/to/geartowns.exe`

3. **Restart VS Code** may be necessary for settings to take effect

4. **Open GitHub Copilot Chat** and start debugging:
   - The emulator will auto-start with MCP server enabled
   - Load a CD image
   - Start chatting with Copilot about the program state
   - You can add context from "MCP Resources" if needed

### STDIO Mode with Claude Desktop

1. **Edit Claude Desktop config file**:

   Follow [these instructions](https://modelcontextprotocol.io/quickstart/user#for-claude-desktop-users) to access Claude's config file, then edit it to include:

   ```json
   {
     "mcpServers": {
       "geartowns": {
         "command": "/path/to/geartowns/platforms/macos/geartowns",
         "args": ["--mcp-stdio"]
       }
     }
   }
   ```

   **Config file locations:**
   - **macOS:** `~/Library/Application Support/Claude/claude_desktop_config.json`
   - **Windows:** `%APPDATA%\Claude\claude_desktop_config.json`
   - **Linux:** `~/.config/Claude/claude_desktop_config.json`

   **Important:** Update the `command` path to match your build location.

2. **Restart Claude Desktop**

### STDIO Mode with Claude Code

1. **Add the Geartowns MCP server** using the CLI:
   ```bash
   claude mcp add --transport stdio geartowns -- /path/to/geartowns --mcp-stdio
   ```

   **Important:** Update the path to match your build location.

2. **Verify the server was added**:
   ```bash
   claude mcp list
   ```

3. **Start debugging**: Open Claude Code and start chatting about the program state. The emulator will auto-start when tools are invoked.

### HTTP Mode

1. **Start the emulator manually** with HTTP transport:

   ```bash
   ./geartowns --mcp-http
   ```

   The default endpoint is `http://127.0.0.1:7777/mcp`.

   To use a custom port:

   ```bash
   ./geartowns --mcp-http --mcp-http-port 3000
   ```

   To bind to a custom address, set a bearer token first:

   ```bash
   GEARTOWNS_MCP_HTTP_TOKEN="change-this-token" ./geartowns --mcp-http --mcp-http-address 0.0.0.0 --mcp-http-port 3000
   ```

   Clients must connect using the server's actual interface address, such as `http://192.168.1.50:3000/mcp`, not `0.0.0.0` or a spoofed loopback address.

   You can also start the server using the "MCP Server" item of the "Debug" menu in the GUI.

2. **Configure bearer-token authentication**:

   Set `GEARTOWNS_MCP_HTTP_TOKEN` before starting HTTP mode. Authentication is optional for loopback binds and required for wildcard or other non-loopback binds.

   macOS and Linux:

   ```bash
   GEARTOWNS_MCP_HTTP_TOKEN="change-this-token" ./geartowns --mcp-http
   ```

   Windows PowerShell:

   ```powershell
   $env:GEARTOWNS_MCP_HTTP_TOKEN = "change-this-token"
   .\geartowns.exe --mcp-http
   ```

   Windows Command Prompt:

   ```cmd
   set GEARTOWNS_MCP_HTTP_TOKEN=change-this-token
   geartowns.exe --mcp-http
   ```

3. **Configure VS Code** `.vscode/mcp.json`:

   ```json
   {
     "servers": {
       "geartowns": {
         "type": "http",
         "url": "http://127.0.0.1:7777/mcp",
         "headers": {
           "Authorization": "Bearer change-this-token"
         }
       }
     }
   }
   ```

4. **Or configure Claude Desktop**:

   ```json
   {
     "mcpServers": {
       "geartowns": {
         "type": "http",
         "url": "http://127.0.0.1:7777/mcp",
         "headers": {
           "Authorization": "Bearer change-this-token"
         }
       }
     }
   }
   ```

5. **Or configure Claude Code**:

   ```bash
   claude mcp add --transport http geartowns http://127.0.0.1:7777/mcp
   ```

6. **Restart your AI client** and start debugging

> **Note:** The MCP HTTP Server must be running standalone before connecting the AI client.
> **Security:** Without `GEARTOWNS_MCP_HTTP_TOKEN`, HTTP mode starts only on a loopback address. Wildcard and other non-loopback binds are refused. `Host` and browser `Origin` values are matched to the connection's actual destination address to prevent DNS rebinding and address spoofing.

## Usage Examples

Once configured, you can ask your AI assistant:

### Basic Commands

- "What is loaded, and is the firmware ready?"
- "Load the CD image at /path/to/game.cue"
- "Show me the 80386 registers and the current mode"
- "Read 64 bytes at DS:0100"
- "Translate 0014:00402000 to a physical address"
- "Set a breakpoint at CS:2140"
- "Step through the next 5 instructions"
- "Capture a screenshot of the current frame"
- "Show me layer 0 of VRAM and tell me its format and visible window"
- "List the visible sprites and show me sprite 120"
- "Which FM channels are playing and what notes?"
- "Mute PCM channel 3 and FM so I can hear the CD audio alone"
- "Show me the disc TOC and read the volume descriptor at LBA 16"
- "Insert /path/to/disk.d77 in drive 0 and list the sectors of cylinder 0"
- "Record a video of the next 600 frames to /path/to/clip.avi"
- "Tap the A button on pad 1"
- "Type DIR and Return at the DOS prompt"

### Advanced Debugging Workflows

- "Find the IRQ11 VSYNC handler through the IDT, analyze what it does, and bookmark the routines it calls"
- "Break when the game writes sprite entry 0 in Sprite RAM and show me the code that writes it"
- "The game switches to protected mode during boot. Stop right after it loads the GDT and decode every descriptor"
- "Find where this game keeps the player's lives in RAM using value-change searches, then add a watch on it"
- "Follow a CD-ROM read: break on the READ command, inspect the DMA channel 3 address and the sectors that arrive"
- "Disassemble the routine at 000C:00012345 in 32-bit mode with control flow and port names, and explain it"
- "Trace 2 frames of instructions and I/O, then tell me what the VSYNC handler does"
- "Profile the game for 10 seconds and list the 10 most expensive functions"

## Available MCP Tools

This is the full tool catalog. All tools are exposed directly by default. With `--mcp-router`, discover advanced tools through `search_tools` or `get_category_tools`, inspect their schemas with `get_tool_info`, then invoke them with `execute_tool`.

### Addresses

Every tool that takes an Intel 80386 address accepts:

- `1234ABCD`, `0x1234ABCD` or `$1234ABCD`: a linear address
- `CS:1234`: a segment register (CS, DS, ES, FS, GS, SS) and an offset, using its cached base
- `0008:00001234`: a selector in protected mode (read from the GDT or LDT) or a segment in real and VM86 mode, and an offset

Results report the `linear` address, and `logical` and `physical` addresses where meaningful.

The server exposes tools organized in the following categories:

### Execution Control
- `debug_pause` - Pause emulation
- `debug_continue` - Resume emulation
- `debug_step_into` - Step one instruction, entering calls and interrupts
- `debug_step_over` - Step over subroutine calls
- `debug_step_out` - Run until the current subroutine or interrupt handler returns
- `debug_step_frame` - Run one frame with breakpoints active, then pause
- `debug_run_to_cursor` - Continue execution until reaching an address
- `debug_reset` - Reset emulation
- `debug_get_status` - Get debug status: `paused`, `at_breakpoint`, `pc` (linear), `logical_pc`, `mode`, `halted`, `breakpoint`, `media_loading`, `media_ready`, `powered_on`, `frame`
- `set_fast_forward_speed` - Set fast forward speed multiplier (0: 1.5x, 1: 2x, 2: 2.5x, 3: 3x, 4: Unlimited)
- `toggle_fast_forward` - Toggle fast forward mode on/off

### CPU & Registers
- `get_i386_status` - Get the complete Intel 80386 status: general registers, EIP, EFLAGS bits, CS:EIP with linear and physical PC, CR0/CR2/CR3, mode, CPL, IOPL, code and stack size, last exception or interrupt vector, segment descriptor caches, GDTR/IDTR/LDTR/TR, debug and test registers
- `write_i386_register` - Write a register: EAX-EDI, EIP, EFLAGS, CR0, CR2, CR3, DR0-DR7, CS-GS. Segment writes reload base and limit in real and VM86 mode and load the descriptor in protected mode
- `get_i386_descriptors` - Decode `table` (`gdt`, `ldt`, `idt`) entries from `start`, up to `count`: selector, base, limit, type, DPL, flags, or gate targets with symbols; in real and VM86 mode the IDT is the IVT
- `get_page_directory` - List the present page-directory entries, or with `index` the present pages of that page table, with linear and physical addresses and flags

### Memory Operations
- `list_memory_areas` - List memory areas: LINEAR, PHYSICAL, I/O PORTS, and every memory region, with `id`, `name`, `size`, `unit_size`, `flags` and `physical_base`
- `read_memory` - Read from a memory area; unmapped or unreadable bytes read as `??`. The LINEAR area takes logical addresses too
- `write_memory` - Write to a memory area. ROMs and I/O PORTS are read-only
- `translate_address` - Translate a logical or linear address: logical, linear, PDE and PTE with their indexes, page flags, physical, bus, region and offset, or the reason it fails
- `get_memory_selection` - Get current memory selection range
- `select_memory_range` - Select a range of memory addresses
- `set_memory_selection_value` - Set all bytes in selection to specified value
- `add_memory_bookmark` - Add bookmark in memory area
- `remove_memory_bookmark` - Remove memory bookmark
- `list_memory_bookmarks` - List all bookmarks in memory area
- `add_memory_watch` - Add watch (tracked memory location), 8, 16, 32 or 64 bits
- `remove_memory_watch` - Remove memory watch
- `list_memory_watches` - List all watches in memory area with their values
- `memory_search_capture` - Capture memory snapshot for search comparison; optional `start`, `size` (needed for areas over 64 MB) and `width` (8, 16, 32)
- `memory_search` - Search memory with operators (<, >, ==, !=, <=, >=), compare types (previous, value, address), and data types (hex, signed, unsigned)
- `memory_find` - Find hex byte sequences (`hex_bytes`) or text (`text`, optional `case_sensitive`) in memory; optional `start` and `size`

### Disassembly & Debugging
- `get_disassembly` - Decode Intel 80386 instructions from current memory: `start_address` and `end_address` or `count`; `code_size` (`auto`, `16`, `32`); `resolve_symbols`; `detailed` adds `flow` (call, jump, conditional, return, int, iret), `target`, I/O `port` and `port_name` for IN/OUT, `vector` and `vector_name` for INT
- `get_call_stack` - View the call stack: entries with `kind` (call, interrupt, exception), `vector`, `from`, `to` and `return` addresses (logical and linear) and symbols
- `add_disassembler_bookmark` - Add bookmark in disassembler
- `remove_disassembler_bookmark` - Remove disassembler bookmark
- `list_disassembler_bookmarks` - List all disassembler bookmarks

### Symbols
- `add_symbol` - Add or rename a user symbol at an address; user symbols take precedence over automatic labels
- `remove_symbol` - Remove the user symbol at an address
- `load_symbols` - Load user symbols from a file: `ADDRESS NAME` or `NAME = ADDRESS` (or `EQU`) per line, `;` or `#` comments, linear hex or `SSSS:OOOOOOOO` addresses
- `list_symbols` - List user symbols and automatic labels, optional `filter`
- `lookup_symbol_by_name` - Find all exact-name symbol matches
- `lookup_symbol_at_address` - Find the symbol at an address

### Breakpoints
- `set_breakpoint` - Set a breakpoint at an address. `type`: `execute` (default, stops before the instruction), `read`, `write` or `access` (stop after the instruction that made the CPU access); `space`: `linear` (default), `physical` or `io` (IN/OUT/INS/OUTS ports). Execute breakpoints are linear only
- `set_breakpoint_range` - Set a breakpoint over an inclusive address range, same `type` and `space`
- `remove_breakpoint` - Remove a breakpoint matching address (and `end_address` for ranges), `type` and `space`
- `list_breakpoints` - List all breakpoints with type, space, range and enabled state
- `set_breakpoint_on_interrupt` - Break on entry to an interrupt `vector` (0-255), before the handler's first instruction; `source`: `any` (default), `exception`, `hardware` or `software`
- `clear_breakpoint_on_interrupt` - Remove an interrupt breakpoint by `vector` and `source`
- `list_breakpoints_on_interrupt` - List interrupt breakpoints with vector names and sources

`debug_get_status` reports what stopped execution in `breakpoint`: `kind` (`execute`, `read`, `write`, `access`, `interrupt`, `run_to`), with the space, address and size of the access, or the vector and source of the interrupt.

### System Hardware
- `get_pic_status` - Get both 8259A PICs: per-IRQ source, input level, request, in-service, mask and vector; ICW1-ICW4 decoded, init state, read register, special mask, poll and priority
- `get_pit_status` - Get the timers: board register 0060 (enables, latches, SOUND, IRQ0) and the six 8253 counters with use, clock, mode, access, BCD, reload, live count, OUT and frequency or period
- `get_dma_status` - Get the uPD71071 DMA: device control bits, mask, requests, terminal count, selected channel, and each channel's device, mode, current and base address and count
- `get_rtc_status` - Get the MSM58321 clock: date (two-digit year), time, weekday, 12/24 hour, leap phase, the 0070/0080 interface and its 16 registers
- `get_system_status` - Get the machine (model, CPU, clock, RAM, floppy drives), reset cause and power-off request, memory windows (0404/0480/0484), CMOS write protect, RAM wait and the serial ID ROM
- `get_keyboard_status` - Get the keyboard interface: data, status, IRQ enable, KBINT, last command, queued bytes with key names, and held keys

### Video Hardware
- `get_crtc_status` - Get the CRTC: dot clock, line and frame timing, interlace, sync widths, beam line, dot, field and H/V state, the VSYNC IRQ, and per layer format, H/V windows, VRAM start, stride, HAJ, field offset, zoom and visible size
- `get_crtc_registers` - Get the 32 CRTC registers R00-R1F with names and the selected index
- `write_crtc_register` - Write a CRTC register (0-31) through the port path, restoring the index
- `get_video_output_status` - Get the output controller (mode, layer formats, front layer, palette select), FDA0 layer enables, 044C status, the VRAM write mask and the FM-R display state
- `get_palettes` - Get the palettes: `layer0` and `layer1` (16 colors, 4-bit B, R, G), `256` (8-bit B, R, G), `digital` (FM-R), or `all`
- `get_frame_buffer` - Decode a VRAM buffer as PNG: `layer0`, `layer1` (whole page with the layer's format and stride), `sprite_display`, `sprite_draw` (256x256, transparent pixels as a checkerboard), or `custom` with `offset`, `format`, `width`, `height` and `palette`
- `list_sprites` - Get the sprite engine state and list entries with screen position, pattern, colors and flags; `start`, `count`, `filter` (`all`, `drawn`, `visible`)
- `get_sprite` - Get one sprite entry as an 8x PNG (`format` `image`) or its details (`info`)

### Audio Hardware
- `get_ym3438_status` - Get the YM3438: LFO, channel 3 mode, DAC, timers, status, and per channel frequency and note, algorithm, feedback, pan, AMS, PMS and the four operators with envelope state and level; optional `channel` (1-6)
- `get_ym3438_registers` - Get one part's register file as last written with register names, and the address latch; `part` (0 or 1)
- `get_rf5c68_status` - Get the RF5C68: sound enable, channel and wave banks, IRQ mask and flags, and each channel's envelope, pan, step and playback rate, loop start, start and play address
- `get_sound_status` - Get both electronic volumes with gain, the 04D5 FM and PCM mutes and the 04EC output gate, the 04E9-04EB interrupt causes, mask and flags, and the debugger mutes
- `set_audio_mute` - Mute or unmute `source` (`fm`, `pcm`, `cdda`) or one `channel` of it in the debugger; emulated state is unchanged

### CD-ROM Hardware
- `get_cdrom_status` - Get the CD-ROM controller: 04C0 master status bits, last command with flags and parameters, status queue, transfer mode, SIRQ/DEI and IRQ9, drive state, head position and track, read range, and the disc
- `list_cdrom_tracks` - Get the disc TOC: type, absolute start and end MSF, LBA, length, sectors, pregap, and the track under the head
- `get_cdrom_audio_status` - Get CD audio playback: state, end action, track, start, stop and current position, track position and CD-DA volume
- `read_cdrom_sector` - Read one sector of a CD image as hex by `lba`; `mode` `user` (2048 bytes) or `raw` (2352). Does not move the drive head. Not available for physical CD drives

### Floppy Hardware
- `get_fdc_status` - Get the MB8877: status with bit names for the last command type, decoded command, track, sector and data registers, BUSY/DRQ/INTRQ and IRQ6, drive control, status, select and switch
- `list_floppy_drives` - Get both drives: image, D77 disk name, media, geometry, RPM, write protect, modified, head cylinder, motor, ready and selected
- `list_floppy_sectors` - List a track's sectors by `drive`, `cylinder`, `head`: C, H, R, N, size, density, deleted mark, status and image offset
- `read_floppy_sector` - Read the sector with ID R (`sector`) on a track: IDs, status and data as hex

### Trace Logger & Profiler
- `set_trace_log` - Start (`enabled` true) or stop the trace logger and configure it: `filters` (`cpu`, `interrupt`, `io`, `dma`, `cdrom`, `fdc`, `vsync`), `output` (`memory`, `disk`), `memory_size`, `disk_size`, `output_path`, `registers`. It records while the debugger runs the machine
- `get_trace_log` - Read trace lines from an absolute `start` sequence or the last N (`start` negative), up to `count`; each line starts with the CPU clock
- `set_profiler` - `start` (opens the Profiler window), `stop` or `reset` the profiler; it collects while its window is open and the debugger runs the machine
- `get_profiler_data` - Read per-function and per-vector results: calls, calls per frame, inclusive and exclusive cycles and percentages, average, min and max cycles; `sort`, `count`, `filter`

### Screen Capture
- `get_screenshot` - Capture current screen frame as base64 PNG
- `start_video_recording` - Start recording video and audio to an AVI file (MJPEG or uncompressed video, 16-bit PCM audio). Only the resulting `file_path` is returned; the video stays on disk. Optional `file_path` (absolute; if omitted, an automatic name in the configured video recordings directory), `scale` (1-20), `aspect_ratio` (`screen`, `square`, `4:3`, `16:9`), and `quality` (`low`, `medium`, `high`, `lossless`). Given options update the recording settings, same as the GUI menu. Frames are recorded only while the emulator runs, so continue or step execution before stopping
- `stop_video_recording` - Stop the active recording and finalize the AVI file. Returns `file_path` and the number of recorded `frames`

### Media & State Management
- `get_media_info` - Get loaded media info, firmware status and the floppy drives (image, disk name, media, write protect, modified)
- `list_recent_media` - List the most recent CD images and floppies opened by Geartowns
- `load_media` - Load a CD image (.cue, .chd, .iso, .zip)
- `load_bios` - Load the FM Towns firmware set from a directory
- `insert_floppy` - Insert a floppy image (.d77, .d88, .hdm, .img, .xdf, an .m3u list or a .zip) in `drive`; optional `write_protected`
- `eject_floppy` - Eject the disk in `drive`, keeping its changes in the working copy
- `swap_floppies` - Swap the disks in drives 0 and 1
- `set_floppy_write_protect` - Set the write protect tab of the disk in `drive`
- `list_save_state_slots` - List all 5 save state slots with information (media name, timestamp, validity)
- `select_save_state_slot` - Select active save state slot (1-5) for save/load operations
- `save_state` - Save emulator state to currently selected slot
- `load_state` - Load emulator state from currently selected slot
- `save_state_file` - Save emulator state to an explicit file path
- `load_state_file` - Load emulator state from an explicit file path
- `get_rewind_status` - Get rewind buffer status (enabled, snapshots, capacity, buffered seconds)
- `rewind_seek` - Seek to a specific rewind snapshot while paused

### Controller Input
- `controller_button` - Control a button on a pad (player 1-2). Use action 'press' to hold the button, 'release' to let it go, or 'press_and_release' to simulate a quick tap. Buttons: up, down, left, right, start, run, A, B, C, X, Y, Z
- `get_input_state` - Get effective pressed buttons, held keyboard keys and pending tap releases
- `keyboard_key` - Press, release or tap a key of the JIS keyboard by name (RETURN, SPACE, A, 1, PF1, SHIFT, CTRL, KP_ENTER, HIRAGANA...)
- `keyboard_type` - Type ASCII text through a frame macro on the JIS layout; optional `frames_per_key`

## Available MCP Resources

In addition to tools, the MCP server provides documentation resources that AI assistants can access to better understand the FM Towns hardware and programming.

MCP clients usually offer resources in the "Add context..." section of the chat interface. You may need to manually add them when you think they are relevant.

### Hardware Documentation Resources

Programmer references for the FM Towns hardware:

- **FM Towns Memory Map** (`geartowns://hardware/memory_map`)
- **FM Towns I/O Port Map** (`geartowns://hardware/io_ports`)
- **Intel 80386 — What FM Towns Software Relies On** (`geartowns://hardware/i386_cpu`)
- **FM Towns System Devices** (`geartowns://hardware/system`)
- **FM Towns Video** (`geartowns://hardware/video`)
- **FM Towns Sprites** (`geartowns://hardware/sprites`)
- **FM Towns Sound** (`geartowns://hardware/sound`)
- **FM Towns CD-ROM** (`geartowns://hardware/cdrom`)
- **FM Towns Floppy Disk** (`geartowns://hardware/floppy`)

## How MCP Works in Geartowns

- The MCP server runs **alongside** the GUI in a background thread
- The emulator GUI remains fully functional (you can play/debug normally while using MCP)
- Commands from the AI are queued and executed on the GUI thread
- Both GUI and MCP share the same emulator state
- Changes made through MCP are instantly reflected in the GUI and vice versa

## Architecture

### STDIO Transport
```
┌─────────────────┐                    ┌──────────────────┐
│   VS Code /     │       stdio        │    Geartowns     │
│ Claude Desktop  │◄──────────────────►│    MCP Server    │
│   (AI Client)   │       pipes        │   (background)   │
└─────────────────┘                    └──────────────────┘
        │                                       │
        └───► Launches ►────────────────────────┘
                                                │
                                                │ Shared State
                                                ▼
                                       ┌──────────────────┐
                                       │   Emulator Core  │
                                       │   + GUI Window   │
                                       └──────────────────┘
```

### HTTP Transport
```
┌─────────────────┐                    ┌──────────────────┐
│   VS Code /     │  HTTP (port 7777)  │    Geartowns     │
│ Claude Desktop  │◄──────────────────►│ MCP HTTP Server  │
│   (AI Client)   │                    │    (listener)    │
└─────────────────┘                    └──────────────────┘
                                                │
                                                │ Shared State
                                                ▼
                                       ┌──────────────────┐
                                       │   Emulator Core  │
                                       │   + GUI Window   │
                                       └──────────────────┘
```
