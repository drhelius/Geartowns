# Geartowns

[![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/drhelius/Geartowns/geartowns.yml)](https://github.com/drhelius/Geartowns/actions/workflows/geartowns.yml)
[![GitHub Releases)](https://img.shields.io/github/v/tag/drhelius/Geartowns?label=version)](https://github.com/drhelius/Geartowns/releases)
[![commits)](https://img.shields.io/github/commit-activity/t/drhelius/Geartowns)](https://github.com/drhelius/Geartowns/commits/main)
[![GitHub contributors](https://img.shields.io/github/contributors/drhelius/Geartowns)](https://github.com/drhelius/Geartowns/graphs/contributors)
[![GitHub Sponsors](https://img.shields.io/github/sponsors/drhelius)](https://github.com/sponsors/drhelius)
[![License](https://img.shields.io/github/license/drhelius/Geartowns)](https://github.com/drhelius/Geartowns/blob/main/LICENSE)
[![Twitter Follow](https://img.shields.io/twitter/follow/drhelius)](https://x.com/drhelius)

Geartowns is a cross-platform Fujitsu FM Towns emulator written in C++ that runs on Windows, macOS, Linux, BSD and RetroArch, with an embedded MCP server for AI debugging and development.

This is an open source project with its ongoing development made possible thanks to the support by these awesome [backers](backers.md). If you find it useful, please consider [sponsoring](https://github.com/sponsors/drhelius).

Don't hesitate to report bugs or ask for new features by [opening an issue](https://github.com/drhelius/Geartowns/issues).

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
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/0.0.0/Geartowns-0.0.0-desktop-windows-x64.zip">Geartowns-0.0.0-desktop-windows-x64.zip</a></td>
    </tr>
    <tr>
      <td>Desktop ARM64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/0.0.0/Geartowns-0.0.0-desktop-windows-arm64.zip">Geartowns-0.0.0-desktop-windows-arm64.zip</a></td>
    </tr>
    <tr>
      <td rowspan="3"><strong>macOS</strong></td>
      <td>Homebrew</td>
      <td><code>brew install --cask drhelius/geardome/geartowns</code></td>
    </tr>
    <tr>
      <td>Desktop Apple Silicon</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/0.0.0/Geartowns-0.0.0-desktop-macos-arm64.zip">Geartowns-0.0.0-desktop-macos-arm64.zip</a></td>
    </tr>
    <tr>
      <td>Desktop Intel</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/0.0.0/Geartowns-0.0.0-desktop-macos-intel.zip">Geartowns-0.0.0-desktop-macos-intel.zip</a></td>
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
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/0.0.0/Geartowns-0.0.0-desktop-ubuntu26.04-x64.zip">Geartowns-0.0.0-desktop-ubuntu26.04-x64.zip</a></td>
    </tr>
    <tr>
      <td>Desktop Ubuntu 26.04 ARM64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/0.0.0/Geartowns-0.0.0-desktop-ubuntu26.04-arm64.zip">Geartowns-0.0.0-desktop-ubuntu26.04-arm64.zip</a></td>
    </tr>
    <tr>
      <td>Desktop Ubuntu 24.04 x64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/0.0.0/Geartowns-0.0.0-desktop-ubuntu24.04-x64.zip">Geartowns-0.0.0-desktop-ubuntu24.04-x64.zip</a></td>
    </tr>
    <tr>
      <td>Desktop Ubuntu 24.04 ARM64</td>
      <td><a href="https://github.com/drhelius/Geartowns/releases/download/0.0.0/Geartowns-0.0.0-desktop-ubuntu24.04-arm64.zip">Geartowns-0.0.0-desktop-ubuntu24.04-arm64.zip</a></td>
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

- CD-ROM drive emulation with `cue/bin`, `iso` and `chd` images, `m3u` disc playlists and disc swapping.
- Support for playing physical CD-ROM discs on Windows, macOS and Linux.
- Floppy disk emulation with two drives supporting `d77`, `d88`, `hdm`, `xdf` and `img` images, `m3u` playlists, write protection and optional persistence of disk changes.
- Compressed image support (`zip`).
- Save states with 5 slots and rewind support.
- Run-ahead support to reduce input latency.
- Video recording to AVI and screenshots.
- Supported platforms (standalone): Windows, Linux, BSD and macOS.
- Supported platforms (libretro): RetroArch.
- Full debugger with just-in-time disassembler, execute, data, I/O and interrupt breakpoints, code navigation, call stack, debug symbols, automatic labels, memory editor, trace logger, profiler, i386 registers, descriptor tables and paging inspectors, and viewers for CRTC, palettes, framebuffers, sprites, YM3438, RF5C68, CD-ROM, floppy controller and disks, interrupts, timers, DMA, RTC, keyboard and game ports.
- MCP server for AI-assisted debugging with GitHub Copilot, Claude, Codex and similar, exposing tools for execution control, memory inspection, hardware status, rewind and more.
- Windows, Linux and macOS *Portable Mode*.
- [Programmable Shader Chain](platforms/shared/desktop/shaders/README.md).
- Media loading from the command line by adding the CD-ROM or floppy image path as an argument.
- Media loading using drag & drop.
- Support for modern game controllers through [gamecontrollerdb.txt](https://github.com/mdqinc/SDL_GameControllerDB) file located in the same directory as the application binary.

## Tips

### Basic Usage
- **Firmware**: Geartowns requires the FM Towns system ROM files to run. Use **Machine → Firmware → Choose BIOS Directory...** to select a folder containing `FMT_SYS.ROM`, `FMT_DOS.ROM`, `FMT_FNT.ROM` and `FMT_DIC.ROM`. `FMT_F20.ROM` is optional and a blank ROM is used if it is not found. In libretro the files must be placed in the RetroArch system directory.
- **CD-ROM Images**: Geartowns supports `chd`, zipped and unzipped `cue/bin` and `iso` images, and `m3u` playlists for multi-disc games. CUE audio tracks can use raw BIN, compatible WAV (44100 Hz, 16-bit, stereo) or Ogg Vorbis files. Change discs from the `CD-ROM` menu.
- **Floppy Images**: Insert floppy disks from the `Floppy 1` and `Floppy 2` menus, or load them from the command line. Supported images are `d77`, `d88`, `hdm`, `xdf` and `img`, also inside `zip` files, and `m3u` playlists.
- **Machine**: Only the FM Towns Model 1/2 and Custom machine configurations are emulated for now. Change them from the `Machine` menu.
- **Portable Mode**: Run with `--portable`, or create an empty file named `portable.ini` in the same directory as the application binary. On macOS, place the file next to the `.app` bundle.

### Debugging Features
- **Docking Windows**: In debug mode, you can dock windows together by pressing SHIFT and dragging a window onto another.
- **Multi-viewport**: In Windows or macOS, you can enable "multi-viewport" in the debug menu. You must restart the emulator for the change to take effect. Once enabled, you can drag debugger windows outside the main window.
- **Single Instance**: You can enable "Single Instance" in the `Emulator` menu. When enabled, opening a file while another instance is running will send it to the running instance instead of starting a new one.
- **Debug Symbols**: The emulator automatically tries to load a symbol file when loading media. For example, for `path_to_image.cue` it tries to load `path_to_image.sym`. You can also load symbol files using the GUI or the CLI. Each line of a symbol file defines one symbol as `address name`, `name = address` or `name EQU address`, where the address can be linear or `selector:offset`. Comments start with `;` or `#`.

### Command Line Usage
```
geartowns [options] [media_file] [symbol_file]

Arguments:
  [media_file]                CD-ROM (.cue, .chd, .iso) or floppy (.d77, .d88, .hdm, .xdf, .img, .m3u), also in .zip
  [symbol_file]               Optional symbol file for debugging

Options:
    -f, --fullscreen          Start in fullscreen mode
    -w, --windowed            Start in windowed mode with menu visible
      --mcp-stdio             Auto-start MCP server with stdio transport
      --mcp-http              Auto-start MCP server with HTTP transport
      --mcp-router            Enable compact MCP tool routing
      --mcp-http-address A    HTTP bind address (default: 127.0.0.1)
      --mcp-http-port N       HTTP port for MCP server (default: 7777)
      --headless              Run without GUI (requires MCP)
      --portable              Store configuration and user data beside the application
    -v, --version             Display version information
    -h, --help                Display this help message
```

### MCP Server

Geartowns includes a [Model Context Protocol](https://modelcontextprotocol.io/introduction) (MCP) server that enables AI-assisted debugging through AI agents like GitHub Copilot, Claude, Codex and similar. The server provides tools for execution control, memory inspection, breakpoints, disassembly, hardware status, rewind and more. STDIO and HTTP transports are supported, with STDIO preferred.

For complete setup instructions and tool documentation, see [MCP_README.md](MCP_README.md).

### Agent Skills

Geartowns provides [Agent Skills](https://agentskills.io/) that teach AI assistants how to effectively use the emulator for specific tasks:

- **[geartowns-debugging](skills/geartowns-debugging/SKILL.md)** — Game debugging, code tracing, breakpoint management, hardware inspection, and reverse engineering.
- **[geartowns-romhacking](skills/geartowns-romhacking/SKILL.md)** — Cheat creation, memory searching, data modification, text translation, and game patching.

Install with `npx skills add drhelius/geartowns`. See the [skills README](skills/README.md) for details.

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
