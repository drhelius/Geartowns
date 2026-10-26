# Geartowns MCP Server

A [Model Context Protocol](https://modelcontextprotocol.io/introduction) server for the Geartowns emulator, enabling AI-assisted debugging and development of FM Towns software.

This server provides tools for game development, rom hacking, translation, reverse engineering, and debugging through standardized MCP protocols compatible with AI agents like Claude Code, Codex and others.

## Downloads

These MCP Bundles (`.mcpb`) package Geartowns with its MCP server configuration for MCP clients that install bundles, and are the packages published to the [MCP Registry](https://registry.modelcontextprotocol.io/). Claude Code and Codex use a regular Geartowns build instead, see [Quick Start](#quick-start).

<table>
  <thead>
    <tr>
      <th>Platform</th>
      <th>Architecture</th>
      <th>Download Link</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td rowspan="2"><strong>Windows</strong></td>
      <td>x64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/1.0.1/Geartowns-1.0.1-mcpb-windows-x64.mcpb">Geartowns-1.0.1-mcpb-windows-x64.mcpb</a></td>
    </tr>
    <tr>
      <td>ARM64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/1.0.1/Geartowns-1.0.1-mcpb-windows-arm64.mcpb">Geartowns-1.0.1-mcpb-windows-arm64.mcpb</a></td>
    </tr>
    <tr>
      <td rowspan="2"><strong>macOS</strong></td>
      <td>x64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/1.0.1/Geartowns-1.0.1-mcpb-macos-x64.mcpb">Geartowns-1.0.1-mcpb-macos-x64.mcpb</a></td>
    </tr>
    <tr>
      <td>ARM64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/1.0.1/Geartowns-1.0.1-mcpb-macos-arm64.mcpb">Geartowns-1.0.1-mcpb-macos-arm64.mcpb</a></td>
    </tr>
    <tr>
      <td rowspan="2"><strong>Linux</strong></td>
      <td>x64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/1.0.1/Geartowns-1.0.1-mcpb-linux-x64.mcpb">Geartowns-1.0.1-mcpb-linux-x64.mcpb</a></td>
    </tr>
    <tr>
      <td>ARM64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/1.0.1/Geartowns-1.0.1-mcpb-linux-arm64.mcpb">Geartowns-1.0.1-mcpb-linux-arm64.mcpb</a></td>
    </tr>
  </tbody>
</table>

## Features

- **Full Debugger Access**: Intel 80386 registers, descriptor tables, paging, memory inspection, execute, data, I/O and interrupt breakpoints, and execution control
- **i386 Addressing**: Every address takes linear, `SR:offset` or `selector:offset` forms, translated through segmentation and paging
- **Multiple Memory Areas**: Linear and physical spaces, side-effect free I/O port reads, Main RAM, VRAM, Sprite RAM, CMOS, ROMs, PCM wave RAM, media images and inserted floppy images
- **Disassembly**: Intel-syntax 80386 disassembly from current memory, with control flow, targets, I/O port and interrupt vector names
- **Hardware Inspection**: 8259A interrupt controllers, 8253 timers, uPD71071 DMA, MSM58321 clock, system control, keyboard, CRTC, output controller, YM3438, RF5C68, CD-ROM controller, MB8877 floppy controller and SCSI host interface
- **Sprite Viewer**: Inspect the sprite engine, list sprite entries and get sprite patterns and VRAM layers as images
- **Audio Inspection**: YM3438 FM channels, operators and registers, RF5C68 PCM channels, electronic volumes and sound interrupts, debugger mutes per source and channel
- **Storage Inspection**: CD-ROM TOC, CD audio and disc sectors; floppy drives, track sector lists and sector data; SCSI hard disks and their sectors
- **Symbol Support**: Add, remove, load, list, and look up user symbols, plus automatic labels
- **Input State**: Inspect effective pressed buttons, held keyboard keys and pending tap releases
- **Bookmarks**: Memory and disassembler bookmarks for navigation
- **Call Stack**: View calls, interrupts and exceptions with their vectors
- **Trace Logger**: CPU instruction trace with interleaved hardware events (interrupts, I/O ports, PIC, timers, DMA, video, sprites, FM, PCM, mixer, CD-ROM, FDC, SCSI, keyboard, game ports, system), in memory or streamed to disk
- **Profiler**: Per-function and per-interrupt-vector call counts, calls per frame, and inclusive/exclusive cycle statistics
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

Media and machine can be set at launch too, for example `--cd game.cue --fd1 disk.d77 --model hg --load-state 1`. The machine options apply to that run only; run `geartowns --help` for the full list.

### Pause When Inactive

While the MCP server is running, the emulator keeps running when its window loses focus, even with **Pause When Inactive** enabled in the **Emulator** menu. The setting keeps its value and applies again once the server stops. Tools that wait for frames to run (`debug_step_frame`, `mouse_move`, `mouse_click`, `keyboard_type`) end with an error if emulation is paused from the GUI while they wait.

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

Get a desktop build of Geartowns from the [downloads](README.md#downloads) or [build it from source](README.md#build-instructions). In the examples below, replace `/path/to/geartowns` with the full path to the executable:

- **macOS:** `/Applications/Geartowns.app/Contents/MacOS/geartowns`
- **Linux:** `/path/to/geartowns`
- **Windows:** `C:/path/to/geartowns.exe`

### STDIO Mode with Claude Code

1. **Add the Geartowns MCP server** using the CLI:

   ```bash
   claude mcp add --transport stdio --scope user geartowns -- /path/to/geartowns --mcp-stdio
   ```

   Everything after `--` is passed to Geartowns untouched. `--scope user` makes the server available in all your projects. Use `--scope local` (the default) to add it only to the current project, or `--scope project` to share it through a `.mcp.json` file in the project root:

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

   Claude Code asks for approval the first time it uses a server from `.mcp.json`.

2. **Verify the server was added**:

   ```bash
   claude mcp list
   ```

3. **Start debugging**: Start Claude Code and ask about the program state. Geartowns launches when the session starts. Run `/mcp` inside Claude Code to check the server status or reconnect it.

### STDIO Mode with Codex

The Codex CLI, IDE extension, and desktop app share the same MCP configuration, so you only need to add the server once.

1. **Add the Geartowns MCP server** using the CLI:

   ```bash
   codex mcp add geartowns -- /path/to/geartowns --mcp-stdio
   ```

   Or add it to `~/.codex/config.toml`:

   ```toml
   [mcp_servers.geartowns]
   command = "/path/to/geartowns"
   args = ["--mcp-stdio"]
   ```

   To use the server only in one project, add the same table to `.codex/config.toml` in that project. Codex loads project configuration only for trusted projects. In TOML, write Windows paths with forward slashes or in single quotes.

2. **Verify the server was added**:

   ```bash
   codex mcp list
   ```

3. **Start debugging**: Start Codex and ask about the program state. Geartowns launches when the session starts. Type `/mcp` in the Codex CLI to see the active servers.

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

3. **Configure Claude Code**:

   ```bash
   claude mcp add --transport http --scope user geartowns http://127.0.0.1:7777/mcp
   ```

   If the server uses a bearer token, add the header:

   ```bash
   claude mcp add --transport http --scope user geartowns http://127.0.0.1:7777/mcp \
     --header "Authorization: Bearer change-this-token"
   ```

   In a shared `.mcp.json`, read the token from the environment instead of committing it:

   ```json
   {
     "mcpServers": {
       "geartowns": {
         "type": "http",
         "url": "http://127.0.0.1:7777/mcp",
         "headers": {
           "Authorization": "Bearer ${GEARTOWNS_MCP_HTTP_TOKEN}"
         }
       }
     }
   }
   ```

4. **Or configure Codex**:

   ```bash
   codex mcp add geartowns --url http://127.0.0.1:7777/mcp
   ```

   If the server uses a bearer token, Codex reads it from an environment variable, so set `GEARTOWNS_MCP_HTTP_TOKEN` in the environment where Codex runs too:

   ```bash
   codex mcp add geartowns --url http://127.0.0.1:7777/mcp --bearer-token-env-var GEARTOWNS_MCP_HTTP_TOKEN
   ```

   The equivalent `~/.codex/config.toml` entry:

   ```toml
   [mcp_servers.geartowns]
   url = "http://127.0.0.1:7777/mcp"
   bearer_token_env_var = "GEARTOWNS_MCP_HTTP_TOKEN"
   ```

5. **Start a new Claude Code or Codex session** and start debugging

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

The server exposes tools organized in the following categories:

### Addresses

Every tool that takes an Intel 80386 address accepts:

- `1234ABCD`, `0x1234ABCD` or `$1234ABCD`: a linear address
- `CS:1234`: a segment register (CS, DS, ES, FS, GS, SS) and an offset, using its cached base
- `0008:00001234`: a selector in protected mode (read from the GDT or LDT) or a segment in real and VM86 mode, and an offset

Results report the `linear` address, and `logical` and `physical` addresses where meaningful.

### Execution Control
- `debug_pause` - Pause emulation
- `debug_continue` - Resume emulation
- `debug_step_into` - Step one instruction, entering calls and interrupts. A `REP` string instruction runs all its elements and a `HLT` waits until an IRQ wakes the CPU
- `debug_step_over` - Step over subroutine calls, `INT n` handlers and IRQs that come in during the step
- `debug_step_out` - Run until the current subroutine or interrupt handler returns; with an empty call stack it steps one instruction and adds a `note`
- `debug_step_frame` - Run complete frames with breakpoints active, then pause; `frames` sets how many (default 1). By default it answers when they finish or a breakpoint stops them, with the `debug_get_status` fields plus `frames` run, `requested_frames` and `breakpoint_hit`, for up to 1800 frames. `wait: false` answers at once and takes up to 216000 frames; poll `debug_get_status` or stop them with `debug_pause`. A reset, state load or rewind during a wait ends it with an error
- `debug_run_to_cursor` - Continue execution until reaching specified address
- `debug_reset` - Reset emulation; optional `boot` (`cdrom`, `floppy1`, `floppy2`, `hard_disk0`-`hard_disk4`) holds the system ROM's boot device keys through this reset, released at the first CD, floppy or SCSI read (1F/2F and later; the Model 1/2 and Marty ROMs ignore them)
- `debug_get_status` - Get debug status: `paused`, `at_breakpoint`, `pc` (linear), `logical_pc`, `mode`, `halted`, `breakpoint`, `media_loading`, `media_ready`, `powered_on`, `frame`
- `toggle_fast_forward` - Enable or disable fast forward mode with `enabled`; optional `speed` (0: 1.5x, 1: 2x, 2: 2.5x, 3: 3x, 4: Unlimited)

### CPU & Registers
- `get_i386_status` - Get the complete Intel 80386 status: general registers, EIP, EFLAGS bits, CS:EIP with linear and physical PC, CR0/CR2/CR3, mode, CPL, IOPL, code and stack size, the last exception the CPU raised with its error code and return address (interrupts and `INT n` don't replace it), segment descriptor caches, GDTR/IDTR/LDTR/TR, debug and test registers, and on a 486 the AC flag, the CR0 bits CD, NW, AM, WP and NE, and TR3-TR5
- `write_i386_register` - Write a register: EAX-EDI, EIP, EFLAGS, CR0, CR2, CR3, DR0-DR7, TR3-TR5 (486), TR6, TR7, CS-GS, LDTR, TR, GDTR_BASE, GDTR_LIMIT, IDTR_BASE, IDTR_LIMIT. Segment writes reload base and limit in real and VM86 mode and load the descriptor in protected mode; LDTR and TR load their LDT or TSS descriptor from the GDT
- `get_i386_descriptors` - Decode `table` (`gdt`, `ldt`, `idt`) entries from `start`, up to `count`: selector, base, limit, type, DPL, flags, or gate targets with symbols; in real mode the IDT is the IVT, VM86 mode still uses the protected-mode gates
- `get_page_directory` - List the present page-directory entries, or with `index` the present pages of that page table, with linear and physical addresses and flags

### Memory Operations
- `list_memory_areas` - List memory areas: LINEAR, PHYSICAL, I/O PORTS, and every memory region, with `id`, `name`, `size`, `unit_size`, `flags`, `physical_base`, `group` (`address_space`, `memory`, `rom`, `media`, `cpu_window`) and `description`. An area keeps its `id` when disks are inserted or ejected
- `read_memory` - Read from a memory area; unmapped or unreadable bytes read as `??`. The LINEAR area takes logical addresses too
- `write_memory` - Write hex `bytes` to a memory area; optional `repeat` writes them back to back that many times to fill a range, up to 1 MB in total. ROMs and I/O PORTS are read-only
- `translate_address` - Translate a logical or linear address: logical, linear, PDE and PTE with their indexes, page flags, physical, bus, region and offset, or the reason it fails
- `get_memory_selection` - Get current memory selection range
- `select_memory_range` - Select a range of memory addresses
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
- `get_disassembly` - Decode Intel 80386 instructions from current memory, without changing the debugger's disassembly or labels: `start_address` and `end_address` or `count`; `code_size` (`auto`, `16`, `32`); `resolve_symbols`; `detailed` adds `flow` (call, jump, conditional, return, int, iret), `target`, I/O `port` and `port_name` for IN/OUT, `vector`, `vector_name` and `vector_description` for INT, and `function` (the BIOS or DOS service picked by AX) when the INT is the current instruction
- `add_symbol` - Add or rename a user symbol at an address; user symbols take precedence over automatic labels
- `remove_symbol` - Remove the user symbol at an address
- `load_symbols` - Load user symbols from a file: `ADDRESS NAME`, `NAME = ADDRESS` (or `EQU`) or GNU `nm` output (`ADDRESS TYPE NAME`) per line, `;` or `#` comments, linear hex or `SSSS:OOOOOOOO` addresses; GNU ld and NASM map files load too. `SSSS:OOOOOOOO` symbols resolve through the current mode each time execution stops; `skipped` counts the lines that didn't parse
- `list_symbols` - List user symbols and automatic labels a page at a time: optional `filter` (case-insensitive part of the name), `start` and `count` (default 200, max 1000), with `total` and `has_more`. With `name` it returns every symbol with that exact name instead, and with `address` the symbol at that address
- `add_disassembler_bookmark` - Add bookmark in disassembler
- `remove_disassembler_bookmark` - Remove disassembler bookmark
- `list_disassembler_bookmarks` - List all disassembler bookmarks
- `get_call_stack` - View the call stack: entries with `kind` (call, interrupt, exception), `vector`, `vector_name`, `vector_description`, `from`, `to` and `return` addresses (logical and linear) and symbols
- `get_trace_log` - Read trace logger entries (CPU + hardware events) using absolute sequence pagination. Use `set_trace_log` to start or stop the logger
- `set_trace_log` - Start or stop trace logging and configure exact event filters and memory or disk storage

#### Trace pagination

`get_trace_log` returns:

- `total_entries`: entries currently retained in the memory ring.
- `total_logged`: the next absolute sequence number. It is monotonic for the process lifetime and is not reset by clear, resize, media changes, or resets.
- `oldest_sequence`: absolute sequence of the oldest retained entry (`total_logged - total_entries`).
- `start`: actual absolute sequence used for this page.
- `next_sequence`: absolute sequence to pass as the next `start`.
- `count`: number of returned entries.
- `overrun`: `true` when a requested `start` had expired and was clamped to `oldest_sequence`.
- `lines`: formatted trace entries in sequence order, each starting with the CPU clock and the clocks since the previous entry. Instruction lines carry the linear address, CS:EIP, general and segment registers, EFLAGS, Intel syntax and bytes.

When `start` is omitted, the latest 100 retained entries are returned. A negative `start` requests that many entries from the retained tail. Positive starts are absolute sequences. `count` defaults to 100 and is capped at 1000. A requested sequence older than `oldest_sequence` starts at the oldest retained entry and sets `overrun` to `true`. A sequence at or beyond `total_logged` returns an empty page with `start` and `next_sequence` equal to the requested value and `overrun` set to `false`.

Memory-mode counters shown in the GUI and written by manual export use the same absolute sequence values as MCP. Disk trace files use a separate zero-based entry counter local to each file.

#### Trace filters

Omitting `filters` enables the default of CPU instructions, IRQs and exceptions. When supplied, `filters` must be non-empty and unique. Each entry is one of these streams, or a group name, the part before the dot (`cpu`, `io`, `fdc`...), which enables all of its streams:

```text
cpu.instructions
cpu.irqs
cpu.exceptions
cpu.software_ints
io.reads
io.writes
pic.requests
pic.mask
pic.commands
pic.init
timer.timeouts
timer.counters
timer.interrupt_control
dma.registers
dma.requests
dma.ends
video.crtc
video.output
video.palette
video.vram_mask
video.vsync
video.fmr
video.missed_vblank
sprite.registers
sprite.transfers
sprite.busy
fm.key
fm.frequency
fm.operators
fm.channels
fm.global
fm.dac
fm.timers
fm.irqs
pcm.channels
pcm.key
pcm.control
pcm.irqs
mixer.volume
mixer.mute
cdrom.commands
cdrom.status
cdrom.irqs
cdrom.control
cdrom.data
cdrom.cdda
fdc.commands
fdc.results
fdc.drives
scsi.control
scsi.reset
scsi.selection
scsi.commands
scsi.phases
scsi.status
scsi.data
scsi.errors
keyboard.keys
keyboard.reads
keyboard.commands
input.reads
input.writes
input.changes
system.reset
system.memory_map
system.rtc
```

Hardware events are decoded as the guest sees them: register names and fields, IRQ lines and vectors, DMA channels with their device, CD-ROM commands with their MSF ranges, status packets and sector transfers, FDC commands and results, SCSI commands with their block ranges, bus phases, status, sense and data transfers, keyboard messages with the key name, and pad buttons. Video register writes carry the beam line and dot. I/O port writes are logged before the device acts on them, so the events they cause follow them.

`video.missed_vblank` detects frames where the game missed VBlank. It watches one linear address, set with `vblank_watch_address` (hex, e.g. `1234ABCD`, `0x1234ABCD`, or `$1234ABCD`), for the access set with `vblank_watch_operation` (`read`, `write`, or `read_write`). CPU data accesses count, instruction fetches do not. At each VSYNC the frame that just ended is checked, and if the watched access did not happen during it, a `VBLANK MISSED` entry is logged with the watched address, operation, and the number of consecutive missed frames. The first VSYNC after starting the logger, changing the watch, resetting, or loading a state only arms the check. Both parameters persist in the configuration and keep their current values when omitted; the response echoes them when `video.missed_vblank` is active.

`sprite.busy` logs the VSYNCs where the sprite controller is still drawing the previous list, so no new transfer starts and the shown page stays the same.

The trace logger records while the debugger runs the machine, so `set_trace_log` with `enabled` true turns the debugger on, opens the Trace Logger window and starts recording. Memory output allocates its buffer when recording starts. Entry times are machine clocks (the 16 MHz timebase, not CPU cycles), and an event inside an instruction carries the instruction's start time.

#### Trace storage

Starting a stopped logger without `output` selects `memory`. Memory capacities are `100K`, `500K`, `1M`, `2M`, and `5M` entries; the persisted default is `100K`. Disk limits are `10MB`, `50MB`, `100MB`, `250MB`, `500MB`, `1GB`, and `unbounded`; the persisted default is `100MB`.

Storage changes while tracing is active cleanly stop and restart the logger. Repeating the active storage configuration is idempotent and only updates filters. Stopping preserves retained memory entries; changing memory capacity or starting disk output resets the ring used by that recording. Resets, power cycles and media changes stop the logger.

`output_path` is a directory only, not a filename. Geartowns creates a unique timestamped trace filename in that directory. When omitted, the configured default, media, or custom directory policy remains in effect.

### Profiler
- `set_profiler` - Start, stop, or reset the function profiler with `action` (`start`, `stop`, `reset`). `start` opens the Profiler debugger window and `stop` closes it. Statistics are only collected while the window is visible (in headless mode, while started) and the debugger runs the machine, starting on the next frame
- `get_profiler_data` - Read profiler results: `collecting`, `window_open`, `total_cycles`, `frames`, `function_count`, and per-function `name`, `symbol`, `address`, `vector`, `vector_name` and `vector_description` (interrupt handlers), `type` (`call`, `interrupt`, or `root` for code running outside any call), `calls`, `calls_per_frame`, `inclusive_cycles`, `inclusive_percent`, `exclusive_cycles`, `exclusive_percent`, `average_cycles`, `min_cycles`, and `max_cycles`. Cycles are CPU cycles at the configured CPU speed, including time spent halted or stalled by DMA. Average, min and max count completed outermost calls, so a recursive function's inner returns don't skew them. Calls are listed per linear target and interrupt handlers per vector. Optional `sort` (`inclusive`, `exclusive`, `calls`, `average`, `max`; highest first), `count` (default 50, max 1000), and `filter` (name or hex address substring)

### Breakpoints
- `set_breakpoint` - Add or update a breakpoint at an address, or over an inclusive range up to `end_address`; `enabled` false keeps it in the list disabled. `type`: `execute` (stops before the instruction; default for linear), `read` (default for physical and io), `write` or `access` (stop after the instruction that made the access); `space`: `linear` (default), `physical` or `io` (IN/OUT/INS/OUTS ports). Execute breakpoints are linear or physical, a physical one follows the code through the current page mapping. Physical read and write breakpoints also see DMA transfers, and linear ones while paging is off. CPU accesses to descriptor tables, the TSS and page tables don't trigger breakpoints
- `remove_breakpoint` - Remove a breakpoint matching address (and `end_address` for ranges), `type` and `space`
- `set_breakpoints_active` - Turn every breakpoint on or off at once with `active`, like Disable All in the debugger; each keeps its own enabled state
- `list_breakpoints` - List all breakpoints with type, space, range and enabled state, and `all_disabled`, plus `interrupt_breakpoints` (with `vector_name`, `vector_description`, source and enabled state) and `irq_breakpoints`
- `set_breakpoint_on_interrupt` - Add or update a break on entry to an interrupt `vector` (0-255), with `source` `any` (default), `exception`, `hardware` or `software`, or on IRQ line `irq` (0-15) when the PIC delivers it, before the handler's first instruction. An IRQ breakpoint follows the line, not the vector, so it keeps working if the program moves the PIC vector bases. `enabled` false keeps it disabled
- `clear_breakpoint_on_interrupt` - Remove an interrupt breakpoint by `vector` and `source`, or an IRQ breakpoint by `irq`

`debug_get_status` reports what stopped execution in `breakpoint`: `kind` (`execute`, `read`, `write`, `access`, `interrupt`, `run_to`), with the space, address and size of the access (`dma` when a DMA transfer made it), or the vector and source of the interrupt, plus `irq` and `irq_name` for a hardware interrupt and `function` when a software interrupt calls a known BIOS or DOS service.

### System Hardware
- `get_pic_status` - Get both 8259A PICs: per-IRQ source, input level, request, in-service, mask and vector; ICW1-ICW4 decoded, init state, read register, special mask, poll and priority
- `get_pit_status` - Get the timers: board register 0060 (enables, latches, SOUND, IRQ0), the six 8253 counters with use, clock, mode, access, BCD, reload, live count, OUT and frequency or period, Interval Timer II and the free-running microsecond timer on models that have them
- `get_dma_status` - Get the uPD71071 DMA: device control bits, mask, requests, terminal count, selected channel, and each channel's device, mode, current and base address and count
- `get_rtc_status` - Get the MSM58321 clock: date (two-digit year), time, weekday, 12/24 hour, leap phase, the 0070/0080 interface and its 16 registers
- `get_system_status` - Get the machine (model, CPU, clock, RAM, floppy drives), reset cause and power-off request, memory windows (0404/0480/0484), NMI vector write protect, expansion NMI mask, RAM wait, the later-model registers this model has (RAM size, NMI mask, LSPEED, memory wait, speed control, CPU misc 3, maximum clock, CRT power and readback, memory card bank, the HR's cache controls) and the serial ID ROM
- `get_keyboard_status` - Get the keyboard interface: data, status, IRQ enable, KBINT, last command, queued bytes with key names, and held keys

### Video Hardware
- `get_crtc_status` - Get the CRTC: dot clock, line and frame timing (rates are null while the CRTC is stopped), interlace, sync widths, beam line, dot, field and H/V state, the VSYNC IRQ, and per layer format, H/V windows, VRAM start (with the FA register), stride, HAJ, field offset, zoom and visible size
- `get_crtc_registers` - Get the 32 CRTC registers R00-R1F with names and the selected index
- `write_crtc_register` - Write a CRTC register (0-31) through the port path like a CPU write, so timing updates and the trace logger records it, restoring the index
- `get_video_output_status` - Get the output controller (mode, layer formats, front layer, palette select), FDA0 layer enables, 044C status, the VRAM write mask and the FM-R display state
- `get_palettes` - Get the palettes: `layer0` and `layer1` (16 colors, 4-bit B, R, G), `256` (8-bit B, R, G), `digital` (FM-R, with the DPMD flag), or `all`, plus which palettes the display layers use
- `get_frame_buffer` - Decode a VRAM buffer as PNG: `layer0`, `layer1` (whole page with the layer's format and stride), `sprite_display`, `sprite_draw` (256x256, transparent pixels as a checkerboard), or `custom` with `offset`, `format`, `width`, `height` and `palette`

### Sprites
- `list_sprites` - Get the sprite engine state and list entries with screen position, pattern, colors and flags; `start`, `count`, `filter` (`all`, `drawn`, `visible`)
- `get_sprite` - Get one sprite entry as an 8x PNG (`format` `image`) or its details (`info`)

### Audio Hardware
- `get_ym3438_status` - Get the YM3438: LFO, channel 3 mode, DAC, timers, status, and per channel frequency and note, algorithm, feedback, pan, AMS, PMS and the four operators with envelope state and level; optional `channel` (1-6)
- `get_ym3438_registers` - Get one part's register file as last written with register names, and the address latch; `part` (0 or 1)
- `get_rf5c68_status` - Get the RF5C68: sound enable, channel and wave banks, IRQ mask and flags, and each channel's envelope, pan, step and playback rate, loop start, start and play address
- `get_sound_status` - Get both electronic volumes with gain, the 04D5 FM and PCM mutes and the 04EC output gate, the 04E9-04EB interrupt causes, mask and flags, and the debugger mutes
- `set_audio_mute` - Mute or unmute `source` (`fm`, `pcm`, `cdda`, `buzzer`) or one `channel` of it in the debugger; emulated state is unchanged

### CD-ROM Hardware
- `get_cdrom_status` - Get the CD-ROM controller: 04C0 master status bits, last command with flags and parameters, status queue, transfer mode, SIRQ/DEI and IRQ9, drive state, head position and track, read range, and the disc; `audio` with CD audio playback (state, end action, track, start, stop and current position, track position and CD-DA volume); and `playlist` with the discs of the loaded `.m3u` (`index`, current disc) and whether the drive is empty
- `list_cdrom_tracks` - Get the disc TOC: type, absolute start and end MSF, LBA, length, sectors, pregap, and the track under the head
- `read_cdrom_sector` - Read one sector of a CD image as hex by `lba`; `mode` `user` (2048 bytes) or `raw` (2352). Does not move the drive head. Not available for physical CD drives

### Floppy Hardware
- `get_fdc_status` - Get the MB8877: status with bit names for the last command type, decoded command, track, sector and data registers, BUSY/DRQ/INTRQ and IRQ6, drive control, status, select and switch
- `list_floppy_drives` - Get both drives: image, D77 disk name (Shift-JIS converted to UTF-8), media, geometry (with `total_size` and `mixed` when tracks differ from track 0), RPM, write protect, modified, head cylinder, motor, ready and selected
- `read_floppy_sector` - Read the sector with ID R (`sector`) on a track by `drive`, `cylinder`, `head`: IDs, status and data as hex. Without `sector` it lists the track's sectors: C, H, R, N, size, density, deleted mark, status and image offset

### Hard Disks
- `get_scsi_status` - Get the SCSI host interface (`built_in`, `card` or `none`): control and status bits, data, the 0C34h word DMA flag on the UG and HG, bus phase, target and LUN, last message, DRQ, IRQ8 and whether word DMA is on, the command in progress (CDB, decoded command, status, bytes moved) and each attached disk's sense key and code
- `list_hard_disks` - Get SCSI IDs 0-6: image, blocks and size, write protect, blocks the machine wrote that are not in the image file yet, and `at_next_reset` when a change waits for a reset; plus the machine's `interface` (`built_in`, `card` for Model 1/2 or `none`) and whether it is fitted now
- `attach_hard_disk` - Attach a raw image of 512-byte sectors (`.hdd`, `.h0`-`.h6`, `.hdm`) or a MAME hard disk `.chd` (writes go to a raw working copy in the Save Files folder) to SCSI `id`; optional `write_protected`, by default the ID's Write Protected setting. A running machine takes it at the next reset, one that is off at once. `create_size_mb` (1-2047) first creates an image at `file_path`, never replacing an existing file: partitioned and formatted like Towns OS, an empty drive per 127 MB, unless `formatted` is false
- `detach_hard_disk` - Detach the disk on SCSI `id` after writing its changes; on a running machine at the next reset
- `flush_hard_disks` - Write the changes to the image files now; otherwise they are written about 2 seconds after the machine stops writing, and on detach, power off and quit
- `read_hard_disk_sector` - Read sector `lba` of the disk on SCSI `id` as hex, as the machine sees it, unwritten changes included, without bus traffic

### Screen Capture
- `get_screenshot` - Capture current screen frame as base64 PNG. Right after a state load it is the frame saved in the state; when the state has none or the screen size changed, it returns an error until a frame runs
- `start_video_recording` - Start recording video and audio to an AVI file (MJPEG, uncompressed or PNG compressed video, 16-bit PCM audio). Only the resulting `file_path` is returned; the video stays on disk. Optional `file_path` (absolute; if omitted, an automatic name in the configured video recordings directory), `scale` (1-20), `aspect_ratio` (`screen`, `square`, `4:3`, `16:9`), and `quality` (`low`, `medium`, `high`, `lossless`, `lossless_compressed`). `lossless` is uncompressed; `lossless_compressed` stores PNG frames, much smaller but playable only with FFmpeg based players. Given options update the recording settings, same as the GUI menu. `screen` follows the display aspect ratio, which uses the debug output screen aspect ratio while debugging. When a shader preset is active outside debug mode, the shader output is recorded at the output window size and `scale` and `aspect_ratio` are ignored. Frames are recorded only while the emulator runs, so continue or step execution before stopping
- `stop_video_recording` - Stop the active recording and finalize the AVI file. Returns `file_path` and the number of recorded `frames`

### Media & State Management
- `get_media_info` - Get loaded media info, firmware status, where each model takes its firmware from, the floppy drives (image, disk name, media, write protect, modified) and the hard disks (as `list_hard_disks`)
- `list_recent_media` - List the most recent CD images, floppies and hard disks opened by Geartowns
- `load_media` - Load a CD image, a floppy image or a hard disk image (CD: .cue, .chd, .iso; floppy: .d77, .d88, .hdm, .xdf, .img, .mfm, .m3u; also inside .zip; hard disk: .h0-.h6 to that SCSI ID, .hdd, .hdm over 8 MB and hard disk .chd to the first free one). A hard disk powers on an empty machine, or waits for the next reset on a running one
- `eject_cdrom` - Eject the CD as the CD-ROM menu does; the machine keeps running with an empty drive
- `insert_playlist_disc` - Insert playlist disc `index` (from the `get_cdrom_status` playlist) into the empty drive, as CD-ROM > Disc in Playlist does: a disc swap that keeps the machine
- `load_bios` - Set the default firmware folder (searched with its subfolders and MAME zips), load the next machine's firmware, reset, and stop at the reset vector
- `insert_floppy` - Insert a floppy image (.d77, .d88, .hdm, .img, .xdf, .mfm, an .m3u list or a .zip) in `drive`; optional `write_protected`
- `eject_floppy` - Eject the disk in `drive`, keeping its changes in the working copy
- `swap_floppies` - Swap the disks in drives 0 and 1
- `set_floppy_write_protect` - Set the write protect tab of the disk in `drive`
- `list_save_state_slots` - List all 5 save state slots with information (media name, timestamp, validity)
- `save_state` - Save emulator state, with the current frame, to the selected slot, to `slot` (1-5, which becomes the selected one) or to an absolute `file_path`
- `load_state` - Load emulator state from the selected slot, from `slot` (1-5, which becomes the selected one) or from an absolute `file_path`. It puts the saved frame back on screen without running the machine; `frame_restored` is false when the state has no frame or the screen size changed. Hard disk writes made since the state are undone; `hard_disks_changed` lists the SCSI IDs where that wasn't possible
- `get_rewind_status` - Get rewind buffer status (enabled, snapshots, capacity, buffered seconds)
- `rewind_seek` - Seek to a specific rewind snapshot while paused

### Controller Input
- `controller_button` - Control a button on a game port (player 1-2). Use action 'press' to hold the button, 'release' to let it go, or 'press_and_release' to simulate a quick tap. Buttons: up, down, left, right, select, run, A, B, C, X, Y, Z, zoom (Marty pad). On a mouse port the directions move the mouse (4 counts per frame while held, 4 counts per tap) and A/B are the left/right buttons
- `controller_set_type` - Set the device on a game port: auto, none, original_gamepad, marty_gamepad, six_button_gamepad or mouse. Auto takes the device the game database names for the loaded game, or the default (original_gamepad on port 1, mouse on port 2)
- `get_input_state` - Get each port's device `type`, its `setting` (auto or a device), its `source` (explicit, database, default or fallback), `database_type` when an explicit choice contradicts the game database, effective pressed buttons, held keyboard keys and pending tap releases
- `keyboard_key` - Press, release or tap a key of the JIS keyboard by name (RETURN, SPACE, A, 1, PF1, SHIFT, CTRL, KP_ENTER, HIRAGANA...)
- `keyboard_type` - Type ASCII text (up to 256 characters) through a frame macro on the JIS layout; optional `frames_per_key`
- `mouse_move` - Move the mouse pointer to screen pixel `x`, `y` (the coordinates of `get_screenshot`; the top-left of the pointer image lands there), or by `dx`, `dy` mouse counts. A screen move finds the pointer by moving it 32 counts out and back and comparing frames, moves 4 counts per frame (Towns OS moves bigger steps faster) and checks again until it is there. Both axes share the steps, so diagonal moves are straight lines. Measurements that don't look like a pointer (too small, stopped at the screen edge, an out-of-range pixels per count) are retried, after 30 frames when the pointer wasn't seen moving at all (software that hides it while busy), and the error says why the last one failed. With a button held by `mouse_click` `press`, a screen move goes straight from the last known position instead, since moving out and back would drag; the result has `estimated: true`. It runs emulated frames, kept in rewind like any input; a breakpoint, reset, state load or rewind stops it. Optional `player` picks the port, default the first one set to mouse
- `mouse_click` - Click the `left` or `right` button (`click`, `double_click`, `press`, `release` or `drag`), at screen pixel `x`, `y` when given. `press_frames` and `release_frames` set how long the button stays down and up (1-120, default 4). `drag` moves to `x`, `y` with the button up, presses, moves straight to `to_x`, `to_y` and releases, also when something fails, returning `from` (measured) and `to` (estimated); `press`, `mouse_move` and `release` make the same drag over several calls. A pointer that snaps to a grid and can't land on the exact pixel still clicks when it covers the target
- `mouse_get_cursor` - Read where the last `mouse_move` or `mouse_click` measured the pointer: position, box, pixels per mouse count, and whether the current frame still draws it there. `estimated: true` marks a position moved by counts or with a button held since the last measurement. A reset, state load, rewind, failed mouse tool or `controller_button` mouse move forgets it. Read-only, it never runs frames

## Available MCP Resources

In addition to tools, the MCP server provides documentation resources that AI assistants can access to better understand the FM Towns hardware and programming.

In Claude Code, type `@` in your prompt to pick a resource, or reference one directly, for example `@geartowns:geartowns://hardware/memory_map`. Codex reads resources through its built-in MCP resource tools, so ask it to read the ones that are relevant.

### Hardware Documentation Resources

Complete technical reference documentation for all FM Towns hardware components:

- **FM Towns Memory Map** (`geartowns://hardware/memory_map`)
- **FM Towns I/O Port Map** (`geartowns://hardware/io_ports`)
- **Intel 80386 — What FM Towns Software Relies On** (`geartowns://hardware/i386_cpu`)
- **FM Towns System Devices** (`geartowns://hardware/system`)
- **FM Towns Video** (`geartowns://hardware/video`)
- **FM Towns Sprites** (`geartowns://hardware/sprites`)
- **FM Towns Sound** (`geartowns://hardware/sound`)
- **FM Towns CD-ROM** (`geartowns://hardware/cdrom`)
- **FM Towns Floppy Disk** (`geartowns://hardware/floppy`)
- **FM Towns SCSI Hard Disks** (`geartowns://hardware/scsi`)

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
│  Claude Code /  │       stdio        │    Geartowns     │
│      Codex      │◄──────────────────►│    MCP Server    │
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
│  Claude Code /  │  HTTP (port 7777)  │    Geartowns     │
│      Codex      │◄──────────────────►│ MCP HTTP Server  │
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
