# FluidSynth for Geartowns

FluidSynth (https://www.fluidsynth.org), the SoundFont synthesizer, release `v2.6.2` at commit
`c3b244ddfd094489a5490beee013c621a504adf2` (November 2026). LGPL-2.1 or later, see `LICENSE`
("Copyright (C) 2003 Peter Hanappe and others"). `fluid_hash` and `fluid_list` come from GLib under the same license,
`fluid_rev_freeverb.cpp` is public domain (Jezar) and a few small files carry no header and fall under the project's
LGPL.

Only the synthesizer is kept: it is created, loads a SoundFont from a file and renders audio into a buffer. There is no
audio driver, MIDI driver, shell, server, sequencer or file renderer. The SoundFont is not part of Geartowns: the user
provides it.

## Files

34 sources and the headers they need, in upstream's folders:

- `src/utils`: `fluid_hash.c`, `fluid_list.c`, `fluid_conv.c`, `fluid_ringbuffer.c`, `fluid_settings.c`, `fluid_sys.c`
- `src/synth`: `fluid_chan.c`, `fluid_gen.c`, `fluid_mod.c`, `fluid_synth.c`, `fluid_synth_monopoly.c`,
  `fluid_tuning.c`, `fluid_voice.c`, `fluid_synth_write_int.cpp`
- `src/rvoice`: `fluid_adsr_env.c`, `fluid_chorus.c`, `fluid_iir_filter.c`, `fluid_iir_filter_impl.cpp`, `fluid_lfo.c`,
  `fluid_rvoice.c`, `fluid_rvoice_dsp.cpp`, `fluid_rvoice_event.c`, `fluid_rvoice_mixer.c`, `fluid_rev.cpp`,
  `fluid_rev_fdn.cpp`, `fluid_rev_freeverb.cpp`, `fluid_rev_lexverb.cpp`, `fluid_rev_dattorro.cpp`
- `src/sfloader`: `fluid_defsfont.c`, `fluid_sffile.c`, `fluid_samplecache.c`, `fluid_sfont.c`
- `src/midi`: `fluid_midi.c` (mostly the standard MIDI file player, but `fluid_synth.c` and `fluid_settings.c` call
  into it)
- `src/drivers`: `fluid_audio_convert.cpp`, for the 16-bit output
- `include/fluidsynth/`: the 15 public headers as released, and `include/fluidsynth.h` and
  `include/fluidsynth/version.h`, written by hand from `fluidsynth.cmake` and `version.h.in` (static library, version
  2.6.2)

Left out: `fluid_event.c`, `fluid_midi_router.c`, `fluid_limiter.c`, `fluid_file.cpp`, the SoundFont 2 DLS loader, the
sequencer, the shell and the server, `bindings/`, `drivers/` (the audio and MIDI drivers), the table generators in
`gentables/` and the test and documentation folders. Some headers of the left out files stay because kept files
include them. The `sinc7` interpolation table is computed at run time.

Geartowns's own files:

- `src/utils/fluidsynth_config.h`: the configuration, written by hand (below).
- `fluid_geartowns.c`: the four empty functions `fluid_settings_init` calls (`fluid_audio_driver_settings`,
  `fluid_midi_driver_settings`, `fluid_shell_settings` and `fluid_file_renderer_settings`) from files that are left
  out, so `fluid_settings.c` stays as released.
- `fluid_tables.c`: the generated conversion tables, see `gen_tables.sh` below.

## Configuration

- The operating system layer is `embedded` (`OSAL_embedded`, `src/utils/fluid_sys_embedded.h`): no threads, no
  exceptions, mutexes and atomics are plain variables. The synth is driven from one thread. Upstream's `cpp11` layer
  needs `<thread>` and `<mutex>` and takes a global mutex on every atomic.
- `WITH_FLOAT`: `fluid_real_t` is `float`, as upstream builds it for small devices. The tables are generated in float.
- `LIBSNDFILE_SUPPORT` is defined so that the SoundFont 3 code paths are built. It does not use libsndfile: the patch
  replaces the Ogg Vorbis reader with minivorbis (below).
- No glib, libsndfile, libinstpatch, readline, OpenMP, LADSPA, network, threads for the mixer or signalsmith.
- `SUPPORTS_VLA` except with MSVC; `HAVE_WINDOWS_H` and `HAVE_IO_H` on Windows, `HAVE_UNISTD_H` elsewhere.

Geartowns has its own `config.h` in a directory that is also on the include path, so the configuration is not called
`config.h`: the two includes of it (`fluidsynth_priv.h` and `fluid_limiter.h`) name `fluidsynth_config.h`, which sits in
`src/utils/`.

Geartowns includes the library with `#include <fluidsynth.h>`. The include directories are `include` and `src` with its
`bindings`, `drivers`, `midi`, `rvoice`, `sfloader`, `synth` and `utils` folders: the sources include each other's
headers as upstream's CMake lays them out. Only `minivorbis` is needed besides them.

## Tables

Upstream builds the seven conversion tables (cents to Hertz, centibels to amplitude, concave and convex velocity,
pan, and the cubic and linear interpolation coefficients) at compile time as `constexpr` C++ with gcem, a git
submodule that is empty in the release archive. `fluid_tables.c` holds them as float arrays and is generated, not
written. `gen_tables.sh` fetches FluidSynth and gcem at the pinned commits into a temporary folder, builds upstream's
`gentables/*.cpp` there against `fluidsynth_config.h`, dumps the tables and writes `fluid_tables.c` next to the script
(a C++17 compiler, `git` and a network connection; `CXX` selects the compiler). It can also take the two source
folders as arguments. gcem is Apache-2.0 and is not part of Geartowns.

## Changes

`geartowns.patch` holds every change against the files of the release:

- `fluid_rev.cpp`, `fluid_rev_filters.h`, `fluid_rev_lexverb.cpp`, `fluid_rev_dattorro.cpp`: Geartowns builds without
  exceptions. `fluid_rev.cpp` wrapped the reverb calls in `try` and `catch`, and three `throw`s guarded a sample rate
  or a buffer length that is not positive. The `try` blocks are gone. `new_fluid_revmodel` and
  `fluid_revmodel_samplerate_change` check the sample rate before they build or change anything and fail the way the
  `catch` did (a `NULL` reverb, `FLUID_FAILED`), the two constructors lost their `throw`, and a delay buffer length
  that is not positive becomes 1. A sample rate is always positive in the synth.
- `fluid_audio_convert.h`: `std::clamp` is C++17. A three-line helper takes its place, so the file builds as C++11.
- `fluid_sffile.c`: SoundFont 3 samples are Ogg Vorbis. The reader that used libsndfile through virtual I/O is
  replaced by one that reads the compressed bytes of the sample through the SoundFont's file callbacks into memory and
  decodes them with minivorbis (`ov_open_callbacks` over the buffer and `ov_read` to 16-bit mono), as the Ogg Vorbis
  decoder of Geartowns does. Mono samples only, as upstream.
- `fluidsynth_priv.h`, `fluid_limiter.h`: `fluidsynth_config.h` instead of `config.h`, as described above.

## Build notes

- C files build as C99 (`gnu99`), C++ files as C++11 with `-fno-rtti`. All of them build with `-Wall -Wextra`. The only
  messages are unused parameters (the stubs of `fluid_sys_embedded.h` and a few functions) and, with clang, an implicit
  `int` to `float` conversion in `fluid_audio_convert.h`; they are upstream's.
- Upstream builds `fluid_iir_filter_impl.cpp` with `-ffast-math -fno-math-errno`. Geartowns builds it without. Measured
  on the benchmark player, 20 s with GeneralUser GS, the output is the same: on an Apple M-series it renders about 10%
  faster with the flags (46 voices 117x against 129x real time, 92 voices 60x against 67x, 184 voices 32x against
  35x), on a Raspberry Pi 4 (g++ 14, `-O3`) the difference is within noise.
- Upstream uses `#pragma omp simd` and `#pragma omp parallel` where OpenMP is found. They are ignored here (g++ says
  so with `-Wall`).
- Windows: `fluid_sys.h` includes `winsock2.h` and `windows.h` for `HAVE_WINDOWS_H`.

## Updating

Check out the new release, copy the same files and headers into `src/` and `include/fluidsynth/`, regenerate
`include/fluidsynth.h` and `version.h` from `fluidsynth.cmake` and `version.h.in`, apply `geartowns.patch` with
`patch -p1` from this folder, compare `fluidsynth_config.h` with `src/config.cmake`, update the commits in
`gen_tables.sh` (the gcem commit is the one the submodule points to) and run it.
