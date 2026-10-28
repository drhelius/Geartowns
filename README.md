# Geartowns

[![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/drhelius/Geartowns/geartowns.yml)](https://github.com/drhelius/Geartowns/actions/workflows/geartowns.yml)
[![GitHub Releases)](https://img.shields.io/github/v/tag/drhelius/Geartowns?label=version)](https://github.com/drhelius/Geartowns/releases)
[![commits)](https://img.shields.io/github/commit-activity/t/drhelius/Geartowns)](https://github.com/drhelius/Geartowns/commits/main)
[![GitHub contributors](https://img.shields.io/github/contributors/drhelius/Geartowns)](https://github.com/drhelius/Geartowns/graphs/contributors)
[![GitHub Sponsors](https://img.shields.io/github/sponsors/drhelius)](https://github.com/sponsors/drhelius)
[![License](https://img.shields.io/github/license/drhelius/Geartowns)](https://github.com/drhelius/Geartowns/blob/main/LICENSE)
[![Twitter Follow](https://img.shields.io/twitter/follow/drhelius)](https://x.com/drhelius)

Geartowns is a very accurate, cross-platform Fujitsu FM Towns emulator written in C++ that runs on Windows, macOS, Linux, BSD and RetroArch, with an embedded MCP server for AI debugging and development.

This is an open source project with its ongoing development made possible thanks to the support by these awesome [backers](backers.md). If you find it useful, please consider [sponsoring](https://github.com/sponsors/drhelius).

Don't hesitate to report bugs or ask for new features by [opening an issue](https://github.com/drhelius/Geartowns/issues).

<img src="https://github.com/user-attachments/assets/f785ec53-4d6b-4135-890b-4c9abbd8e61d" />

## Downloads

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
      <td>Desktop x64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/1.0.2/Geartowns-1.0.2-desktop-windows-x64.zip">Geartowns-1.0.2-desktop-windows-x64.zip</a></td>
    </tr>
    <tr>
      <td>Desktop ARM64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/1.0.2/Geartowns-1.0.2-desktop-windows-arm64.zip">Geartowns-1.0.2-desktop-windows-arm64.zip</a></td>
    </tr>
    <tr>
      <td rowspan="3"><strong>macOS</strong></td>
      <td>Homebrew</td>
      <td><code>brew install --cask drhelius/geardome/geartowns</code></td>
    </tr>
    <tr>
      <td>Desktop Apple Silicon</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/1.0.2/Geartowns-1.0.2-desktop-macos-arm64.zip">Geartowns-1.0.2-desktop-macos-arm64.zip</a></td>
    </tr>
    <tr>
      <td>Desktop Intel</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/1.0.2/Geartowns-1.0.2-desktop-macos-intel.zip">Geartowns-1.0.2-desktop-macos-intel.zip</a></td>
    </tr>
    <tr>
      <td rowspan="6"><strong>Linux</strong></td>
      <td>Ubuntu PPA</td>
      <td><a href="https://github.com/drhelius/ppa-geardome">drhelius/ppa-geardome</a></td>
    </tr>
    <tr>
      <td>Fedora RPM</td>
      <td><a href="https://github.com/drhelius/rpm-geardome">drhelius/rpm-geardome</a></td>
    </tr>
    <tr>
      <td>Desktop Ubuntu 26.04 x64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/1.0.2/Geartowns-1.0.2-desktop-ubuntu26.04-x64.zip">Geartowns-1.0.2-desktop-ubuntu26.04-x64.zip</a></td>
    </tr>
    <tr>
      <td>Desktop Ubuntu 26.04 ARM64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/1.0.2/Geartowns-1.0.2-desktop-ubuntu26.04-arm64.zip">Geartowns-1.0.2-desktop-ubuntu26.04-arm64.zip</a></td>
    </tr>
    <tr>
      <td>Desktop Ubuntu 24.04 x64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/1.0.2/Geartowns-1.0.2-desktop-ubuntu24.04-x64.zip">Geartowns-1.0.2-desktop-ubuntu24.04-x64.zip</a></td>
    </tr>
    <tr>
      <td>Desktop Ubuntu 24.04 ARM64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/1.0.2/Geartowns-1.0.2-desktop-ubuntu24.04-arm64.zip">Geartowns-1.0.2-desktop-ubuntu24.04-arm64.zip</a></td>
    </tr>
    <tr>
      <td><strong>MCPB</strong></td>
      <td>All platforms</td>
      <td><a href="MCP_README.md">MCP Readme</a></td>
    </tr>
    <tr>
      <td><strong>RetroArch</strong></td>
      <td>All platforms</td>
      <td><a href="https://docs.libretro.com/library/geartowns/">Libretro core documentation</a></td>
    </tr>
    <tr>
      <td><strong>Dev Builds</strong></td>
      <td>All platforms</td>
      <td><a href="https://github.com/drhelius/Geartowns/actions/workflows/geartowns.yml">GitHub Actions</a></td>
    </tr>
  </tbody>
</table>

**Notes:**
- **Windows**: May need [Visual C++ Redistributable](https://go.microsoft.com/fwlink/?LinkId=746572) and [OpenGL Compatibility Pack](https://apps.microsoft.com/detail/9nqpsl29bfff)
- **Homebrew**: If Homebrew asks you to trust the third-party tap, run `brew trust --tap drhelius/geardome`
- **Linux**: May need `libsdl3`

## Features

- Very accurate 80386 core that passes [test386](https://github.com/barotto/test386.asm) and [SingleStepTests/80386](https://github.com/singlesteptests/80386).
- FM Towns (Model 1/2), 1F/2F, 10F/20F, FM Towns II CX, UX, UG, HG and HR, and FM Towns Marty emulation.
- CD-ROM drive emulation with `cue/bin`, `iso` and `chd` images, `m3u` disc playlists and disc swapping, with optional compressed images (`zip`).
- Physical CD-ROM discs on Windows, macOS and Linux.
- Floppy disk emulation with two drives supporting `d77`, `d88`, `hdm`, `xdf`, `img` and HxC `mfm` images, `m3u` playlists, write protection and optional persistence of disk changes.
- SCSI hard disks on IDs 0-6 with raw and `chd` images, blank disk creation and Towns OS installation.
- IC memory card slot with raw card images, the JEIDA 4 banked windows, blank card creation and booting from the card.
- Save states with 5 slots and rewind support.
- Run-ahead support to reduce input latency.
- Video recording to AVI and screenshots.
- VGM recorder for the YM3438 and RF5C68 sound chips.
- Internal database for automatic media detection and hardware selection if `Auto` options are selected.
- Supported platforms (standalone): Windows, Linux, BSD and macOS.
- Supported platforms (libretro): Windows, Linux, macOS, Raspberry Pi, Android, iOS, tvOS, webOS, PlayStation 3, Nintendo GameCube, Nintendo WiiU, Nintendo Switch, Emscripten, Classic Mini systems (NES, SNES, C64, ...), OpenDingux, RetroFW and QNX.
- Full debugger:
  - Just-in-time disassembler, code navigation, call stack, debug symbols and automatic labels.
  - Execute, data, I/O and interrupt breakpoints.
  - Memory editor, trace logger and profiler.
  - i386 registers, descriptor tables and paging inspectors.
  - Viewers for CRTC, palettes, framebuffers, sprites, YM3438, RF5C68, CD-ROM, floppy controller and disks, SCSI controller and disks, interrupts, timers, DMA, RTC, keyboard and game ports.
- MCP server for AI-assisted debugging with Claude Code, Codex and similar, exposing tools for execution control, memory inspection, hardware status, rewind and more.
- Windows, Linux and macOS *Portable Mode*.
- [Programmable Shader Chain](platforms/shared/desktop/shaders/README.md).
- Media loading from the command line by adding the CD-ROM, floppy, hard disk or IC memory card image path as an argument.
- Media loading using drag & drop.
- Support for modern game controllers through [gamecontrollerdb.txt](https://github.com/mdqinc/SDL_GameControllerDB) file located in the same directory as the application binary.

## Tips

### Basic Usage
- **Firmware**: Geartowns requires the FM Towns system ROMs to run. Use **Emulator → Firmware...** to choose the folder that holds them, with one subfolder or MAME zip per set. See [BIOS_README.md](BIOS_README.md) for the sets each model needs, where to put them and their checksums.
- **Machine**: **Machine → Model**, **CPU Speed** and **Main RAM** default to **Auto**, which picks:
  - The model, CPU speed and RAM that the game database names for the game.
  - The FM Towns II HG for 1.44 MB floppy disks.
  - The FM Towns (Model 1/2) for other content.
- **CD-ROM Images**: Geartowns supports `chd`, zipped and unzipped `cue/bin` and `iso` images, and `m3u` playlists for multi-disc games. CUE audio tracks can use raw BIN, compatible WAV (44100 Hz, 16-bit, stereo) or Ogg Vorbis files. Change discs from **Drives → CD-ROM**.
- **Floppy Images**: Insert floppy disks from **Drives → Floppies**, or load them from the command line. Supported images are `d77`, `d88`, `hdm`, `xdf`, `img` and `mfm`, also inside `zip` files, and `m3u` playlists.
- **Portable Mode**: Run with `--portable`, or create an empty file named `portable.ini` in the same directory as the application binary. On macOS, place the file next to the `.app` bundle.

### Hard Disks

Up to seven SCSI hard disks can be attached, on IDs 0-6:

- **1F/2F and later**: built-in SCSI interface.
- **FM Towns (Model 1/2)**: a SCSI card is fitted while a disk is attached.
- **FM Towns Marty**: no SCSI.

#### Image Formats
- **Raw images** (`hdd`, `h0`-`h6`, and `hdm` over 8 MB): 512-byte sectors. Smaller `hdm` images are floppies.
- **MAME hard disk `chd`**: read-only. At the first write, Geartowns makes a raw working copy in the `Save Files` folder and uses it from then on.

#### Attaching Disks
- **Desktop**: use **Drives → Hard Disks**, drag & drop or the command line. With drag & drop and the command line, the extension picks the ID:
  - `h0`-`h6`: that SCSI ID.
  - `hdd` and `hdm` over 8 MB: the first free ID.
- **Libretro**: load the image as content, alone or in an `m3u` playlist with the CD. Changes go to a working copy in the save directory and the original is never written.
- **Running machine**: attaching or detaching a disk takes effect at the next reset.

#### Disk Writes and Save States
- Writes are saved a few seconds after the machine stops writing.
- Save states, rewind and run-ahead hold no disk data. Loading one undoes the disk writes made since then (the last 32 MB of them per disk), and warns when it can't.

#### Installing Games
With **Formatted** checked, **New Blank Disk...** makes a disk partitioned and formatted like Towns OS does, with one empty drive per 127 MB (up to ten). Games booted from their CD can install to it. It holds no system and doesn't boot.

#### Installing Towns OS
1. Create a disk with **New Blank Disk...** and **Formatted** unchecked.
2. Boot the Towns OS V2.1 CD and run **HD Install** from the **Tools** folder of TownsMENU V2.1.
3. Eject the CD and reset.

Towns OS V2.1 needs 4 MB of RAM to start from a hard disk. **Main RAM** on Auto gives at least that while a disk is attached.

#### Booting From the Hard Disk
- A bootable CD boots before the disk.
- To boot the disk with a CD inserted, pick it in **Geartowns → Reset and Boot From**. This holds the system ROM's boot keys (H and the SCSI ID) through the reset.
- The Model 1/2 and Marty ROMs don't read these keys.

#### Drive Letters
Towns OS keeps hard disk drive letters in the model's CMOS, not on the disk, and only uses the partitions that have one.

- **Assign Drive Letters** on (**Drives → Hard Disks**, the default): each reset gives the free letters from D: to the partitions of an attached disk that have none. Letters already set are kept. This lets a disk installed on another model or emulator boot.
- **Assign Drive Letters** off: assign the letters yourself in Towns OS:
  1. Boot the Towns OS V2.1 CD.
  2. Open **Settings → Drive Configuration**.
  3. Click the disk's SCSI ID, then **Add Drive** and **Execute**.
  4. Eject the CD and reset.

### IC Memory Card

Every model has one slot for a battery-backed SRAM card. Towns OS uses the card as a drive.

#### Images
- **Raw images** (`icm`): the card's bytes, a whole number of KB up to 64 MB. The file is the card: writes are saved a few seconds after the machine stops writing, and when the card is ejected.
- **Desktop**: use **Drives → IC Memory Card** to insert a card, create a blank one, eject it or write protect it. Drag & drop and `--card` insert one too.
- **Libretro**: set **IC Memory Card** to **Per Game** (a card named after the content) or **Shared** (one card for all content). The card is kept in the save directory and created blank with **IC Memory Card Size** when missing.

#### Using the Card in Towns OS
1. Add `DEVICE=\SYS\TICM.SYS /U0` to `CONFIG.SYS`. At boot the driver shows the card's drive letter, for example E:.
2. Format the card with `TICMFMT E: /S1024`, the size in KB.

Towns OS V2.1's `TICM.SYS` uses at most 8 MB of a card. On the UX and the Marty it also misplaces transfers that cross from the second 1 MB bank of a card into the third, so keep cards there to 2 MB. The UG reaches 1 MB.

#### Booting From the Card
- A card that starts with an `IPL4` boot sector boots before the CD, on every model.
- **Geartowns → Reset and Boot From → IC Memory Card** holds I, C and M through the reset, which the UX and HR system ROMs read to boot the card before any device they remember.

#### Save States
Save states, rewind and run-ahead hold no card data. Loading one undoes the card writes made since then, and warns when it can't. A state loads only with a card of the same size in the slot, or with the slot empty if it was empty.

### Debugging Features
- **Docking Windows**: In debug mode, you can dock windows together by pressing SHIFT and dragging a window onto another.
- **Multi-viewport**: In Windows or macOS, you can enable "multi-viewport" in the debug menu. You must restart the emulator for the change to take effect. Once enabled, you can drag debugger windows outside the main window.
- **Single Instance**: You can enable "Single Instance" in the `Emulator` menu. When enabled, opening a file while another instance is running will send it to the running instance instead of starting a new one.
- **Debug Symbols**: The emulator automatically tries to load a symbol file when loading a CD-ROM or floppy image. For example, for `path_to_image.cue` it tries to load `path_to_image.sym`. You can also load symbol files using the GUI or the CLI.
  - Each line defines one symbol as `address name`, `name = address`, `name EQU address` or `address type name`. The last one is GNU `nm` output, for example `nm -n program.elf > program.sym`.
  - Addresses can be linear or `selector:offset`.
  - Comments start with `;` or `#`.
  - GNU ld and NASM map files can be loaded too.

### Command Line Usage
```
geartowns [options] [media_file] [symbol_file]

Arguments:
  media_file                CD-ROM (.cue .chd .iso), floppy (.d77 .d88 .hdm
                            .xdf .img .mfm), .m3u playlist, .zip, hard disk
                            (.hdd .h0-.h6, hard disk .chd, .hdm over 8 MB), or
                            IC memory card (.icm)
  symbol_file               Symbol file for debugging

Media:
      --cd FILE             CD-ROM image or .m3u playlist
      --fd1 FILE            Floppy image in drive 1
      --fd2 FILE            Floppy image in drive 2
      --card FILE           IC memory card image
      --load-state SLOT     Load save state slot 1-5, or a state file, once the
                            media is loaded

Machine (for this run only, the saved settings stay):
      --model MODEL         auto, model1_2, 1f_2f, 10f_20f, cx, ux, ug, hg, hr,
                            marty
      --cpu-mhz MHZ         auto, 16, 20, 25, 33, 40, 50, 66, 100
      --ram MB              auto, 1, 2, 4, 6, 8, 10, 12, 16, 20, 24, 26, 28
      --fpu FPU             auto, none, 80387 (a 487SX on the HR)

Options:
  -f, --fullscreen          Start in fullscreen mode
  -w, --windowed            Start in windowed mode with menu visible
      --mcp-stdio           Auto-start MCP server with stdio transport
      --mcp-http            Auto-start MCP server with HTTP transport
      --mcp-router          Enable compact MCP tool routing
      --mcp-http-address A  HTTP bind address (default: 127.0.0.1)
      --mcp-http-port N     HTTP port for MCP server (default: 7777)
      --headless            Run without GUI (requires MCP)
      --portable            Store configuration and user data beside the
                            application
  -v, --version             Display version information
  -h, --help                Display this help message
```

### MCP Server

Geartowns includes a [Model Context Protocol](https://modelcontextprotocol.io/introduction) (MCP) server that enables AI-assisted debugging through AI agents like Claude Code, Codex and similar. The server provides tools for execution control, memory inspection, breakpoints, disassembly, hardware status, rewind and more. STDIO and HTTP transports are supported, with STDIO preferred.

For complete setup instructions and tool documentation, see [MCP_README.md](MCP_README.md).

### Agent Skills

Geartowns provides [Agent Skills](https://agentskills.io/) that teach AI assistants how to effectively use the emulator for specific tasks:

- **[geartowns-debugging](skills/geartowns-debugging/SKILL.md)** — Game debugging, code tracing, breakpoint management, hardware inspection, and reverse engineering.
- **[geartowns-romhacking](skills/geartowns-romhacking/SKILL.md)** — Cheat creation, memory searching, data modification, text translation, and game patching.

Install with `npx skills add drhelius/geartowns`. See the [skills README](skills/README.md) for details.

## Hardware Accuracy

Geartowns is being developed based on the results of the [FM Towns Hardware Tests](https://github.com/drhelius/fmtowns-tests).

## Build Instructions

### Windows

- Install Microsoft Visual Studio Community 2026 or later.
- Download the latest SDL3 VC development libraries from [SDL3 Releases](https://github.com/libsdl-org/SDL/releases) (the file named `SDL3-devel-x.y.z-VC.zip`).
- Extract the archive and rename the resulting folder (e.g. `SDL3-x.y.z`) to `SDL3`.
- Place the `SDL3` folder inside `platforms/windows/dependencies/` so that the include path is `platforms/windows/dependencies/SDL3/include/SDL3/`.
- Open the Geartowns Visual Studio solution `platforms/windows/Geartowns.sln` and build.

### macOS

- Install Xcode and run `xcode-select --install` in the terminal for the compiler to be available on the command line.
- Run these commands to generate a Mac *app* bundle:

``` shell
brew install sdl3
cd platforms/macos
make dist
```

### Linux

- Ubuntu / Debian / Raspberry Pi (Raspbian):

If you are using Ubuntu 26.04, you can install SDL3 directly. Use the following commands to build:

``` shell
sudo apt install build-essential pkg-config libsdl3-dev libgl-dev
cd platforms/linux
make
```

For Ubuntu 24.04, you need to build SDL3 from source first. Use the following commands to build both SDL3 and Geartowns:

``` shell
sudo apt install build-essential cmake git curl jq pkg-config \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev \
  libxi-dev libxss-dev libxkbcommon-dev libwayland-dev libdecor-0-dev \
  libdrm-dev libgbm-dev libgl1-mesa-dev libegl1-mesa-dev libdbus-1-dev libudev-dev libxtst-dev
SDL3_TAG=$(curl -s https://api.github.com/repos/libsdl-org/SDL/releases/latest | jq -r '.tag_name')
git clone --depth 1 --branch "$SDL3_TAG" https://github.com/libsdl-org/SDL.git /tmp/SDL3
cmake -S /tmp/SDL3 -B /tmp/SDL3/build -DCMAKE_INSTALL_PREFIX=/usr -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
cmake --build /tmp/SDL3/build -j$(nproc)
sudo cmake --install /tmp/SDL3/build
cd platforms/linux
make
```

- Fedora:

``` shell
sudo dnf install @development-tools gcc-c++ SDL3-devel
cd platforms/linux
make
```

- Arch Linux:

``` shell
sudo pacman -S base-devel sdl3
cd platforms/linux
make
```

### BSD

- FreeBSD:

``` shell
su root -c "pkg install -y git gmake pkgconf sdl3"
cd platforms/bsd
USE_CLANG=1 gmake
```

- NetBSD:

``` shell
su root -c "pkgin install gmake pkgconf SDL3"
cd platforms/bsd
gmake
```

- OpenBSD

``` shell
doas pkg_add gmake sdl3
cd platforms/bsd
LDFLAGS=-L/usr/X11R6/lib/ USE_CLANG=1 gmake
```

### Libretro

- Ubuntu / Debian / Raspberry Pi (Raspbian):

``` shell
sudo apt install build-essential
cd platforms/libretro
make
```

- Fedora:

``` shell
sudo dnf install @development-tools gcc-c++
cd platforms/libretro
make
```

## Contributors

Thank you to all the people who have already contributed to Geartowns!

[![Contributors](https://contrib.rocks/image?repo=drhelius/geartowns)](https://github.com/drhelius/geartowns/graphs/contributors)

## License

Geartowns is licensed under the GNU General Public License v3.0 License, see [LICENSE](LICENSE) for more information.
