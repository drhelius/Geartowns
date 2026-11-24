# Munt (libmt32emu) for Geartowns

Munt (https://github.com/munt/munt), the Roland MT-32 and CM-32L emulation library, release `libmt32emu_2_8_6` at
commit `91d2205eb8f2591d177b257b92e2c75f830508de` (November 2026). LGPL-2.1 or later, see `COPYING.LESSER.txt`
("Copyright (C) 2003-2026 Dean Beeler, Jerome Fisher, Sergey V. Mikayev"). The SHA-1 code in `src/sha1/` is
Micael Hildenborg's 3-clause BSD and keeps its notice in its files.

The library is the MT-32 and CM-32L synth at the far end of the MIDI cable. It needs the machine's own control and PCM
ROMs, which are never part of Geartowns: the user provides them.

## Files

`src/` holds the 18 `.cpp` files Geartowns builds and 28 headers, plus `src/sha1/` (`sha1.cpp`, `sha1.h`):
`Analog`, `BReverbModel`, `Display`, `File`, `LA32FloatWaveGenerator`, `LA32Ramp`, `LA32WaveGenerator`, `Part`,
`Partial`, `PartialManager`, `Poly`, `ROMInfo`, `Synth`, `TVA`, `TVF`, `TVP`, `Tables` and `sha1/sha1`.
`Display`, `sha1`, `File` and `LA32FloatWaveGenerator` have to stay: `Synth` references them.

Left out:

- `c_interface/` and `mt32emu.pc.in`: the C API, not used.
- `FileStream`: the only user of `<fstream>`. The ROMs come from memory through `ArrayFile`.
- `SampleRateConverter` and `srchelper/`: Munt's own resampler. Its output rate is used as it comes.
- `MidiStreamParser`: it turns a byte stream into messages and Geartowns already has whole messages.
- `VersionTagging`: shared libraries only.
- `test/`, `config.h.in`, the CMake files, the documentation and the other COPYING files.

The library has no dependencies besides the C and C++ standard headers, no threads, no RTTI and no `throw`, `try` or
`catch`. All 18 files build with `-std=c++11 -fno-exceptions -fno-rtti -Wall -Wextra -Wshadow` without a warning.

## Configuration

Upstream generates `config.h` with CMake. Here `src/mt32emu_config.h` is written by hand: version 2.8.6, a static
library with the C++ API only (`MT32EMU_EXPORTS_TYPE 0`), no version tagging, and `MT32EMU_WITH_SNPRINTF` and
`MT32EMU_WITH_EXP2F`, which CMake detects from the C library. `MT32EMU_FASTER_MATH` stays at its default.

The include path for `mt32emu.h` is `src`, and Geartowns has its own `config.h` in a directory that is also on the
include path. So the file is not called `config.h`: the two lines that include it (`globals.h` and `mt32emu.h`) name
`mt32emu_config.h`, which sits in `src/` next to them.

Geartowns includes the library with `#include "mt32emu.h"`, from the include directory `mt32emu/src`.

On Windows `Display.cpp` has the same name as Geartowns's `display.cpp` and the project puts every object in one folder,
so the project names its object `mt32emu_Display.obj`.

## Changes

`geartowns.patch` holds every change against the files of the release:

- `TVP.cpp`, `Synth.cpp` and `Synth.h`: the pitch timer of each partial adds `rand() & 3` samples of jitter to its
  period, an imitation of the jitter of a real unit, from a C library generator that nobody seeds. The same input gave
  different output on different platforms and on every run. Now `Synth::nextPitchJitter()` takes the value from a
  32-bit xorshift generator that belongs to the synth (`Extensions::pitchJitterState`) and `Synth::open` seeds it with
  the same value (2463534242) every time. The jitter stays, only its source changes, and the 16-bit renderer gives the
  same output on every platform and in every run. The float renderer still depends on the platform's floating point.
- `Structures.h`: the MSVC packing declaration is used only when the compiler is not clang (`defined(_MSC_VER) &&
  !defined(__clang__)`), so clang-cl builds.
- `globals.h` and `mt32emu.h`: `mt32emu_config.h` instead of `config.h`, as described above.
- `mt32emu.h`: the three includes of files that are left out (`FileStream.h`, `MidiStreamParser.h` and
  `SampleRateConverter.h`) are removed, so the umbrella header works.

## Using it

`ROMImage::makeROMImage(&ArrayFile)` identifies a ROM by its size and SHA-1, `Synth::open(control, pcm, partialCount,
analogMode)` copies both ROMs, `Synth::playMsg(message, timestamp)` and `Synth::playSysex(data, size, timestamp)`
queue MIDI, and `Synth::render(Bit16s*, frames)` produces interleaved stereo. Timestamps count native 32 kHz samples
since the synth opened (`getInternalRenderedSampleCount()`), whatever the output rate.

## Updating

Check out the new release, copy the same files into `src/` and `src/sha1/`, apply `geartowns.patch` with `patch -p1`
from this folder, and check the patched lines and `mt32emu_config.h` against the new `config.h.in`.
