# GNU lightning 2.2.3 for Geartowns

GNU lightning 2.2.3 (https://www.gnu.org/software/lightning/, January 2024), the code generator of the i386
dynarec. LGPLv3 or later, see `COPYING.LESSER`.

Only the library sources and the backends Geartowns builds are kept: x86 (32 and 64-bit), aarch64, arm, mips, ppc,
riscv and loongarch. `include/lightning.h` is written by hand from `lightning.h.in`, and `include/jit_config.h`
takes the place of the `config.h` that `configure` generates. The build compiles `lib/lightning.c`,
`lib/jit_disasm.c`, `lib/jit_memory.c`, `lib/jit_note.c`, `lib/jit_print.c` and `lib/jit_size.c`; `lightning.c`
includes the backend of the host.

## Changes

`geartowns.patch` holds every change against the 2.2.3 tarball:

- `lightning.h`: no `unistd.h` or `pthread.h`, `stdint.h` included directly, `jit_config.h` included first.
- `jit_config.h`: no `mmap` (the code memory always comes from Geartowns), the MSVC architecture macros mapped to the
  GCC ones, little-endian byte order for Windows, and `__builtin_*` replacements for `cl.exe`.
- `jit_flush` is empty in every backend. The executable memory providers of `src/i386/exec_memory.cpp` flush the
  caches for both views, without `sysconf` or `__clear_cache`.
- `x18` is never allocated on aarch64: it is the platform register on Apple, Windows and Android.
- `jit_get_cpu` for arm, mips and ppc takes the CPU features from compiler macros instead of `/proc/cpuinfo`, so the
  code never uses more than the ISA the core is compiled for (an ARMv6 build gets ARMv6 code on any board).
- x86: `cpuid` through `__cpuidex` with MSVC, and 64-bit mask helpers that also hold where `long` has 32 bits.
- x86 Win64: the prolog and epilog save and restore all 128 bits of XMM6-XMM15 with `movupd` in 16-byte slots.
  Upstream restores them with `movsd`, which clears bits 127:64, so every return from generated code breaks the
  caller's vector registers.
- MSVC: no GNU case ranges in x86 `patch_at`, and the aarch64 `_lrotr` is `_lrotr_r`, since `_lrotr` is an intrinsic.
- The compare and swap fallback of `jit_fallback.c` (arm, mips, ppc, riscv, loongarch) takes no mutex, so the library
  needs no pthread. Geartowns never emits `casr` or `casi`.
- mips: the instruction held for a delay slot is dropped when a function is emitted again after its frame grows.
  Before, it was written at the start of the second pass, one word ahead of the code, so the function began with a
  stray store.

To update lightning, unpack the new release, copy the same files and apply `geartowns.patch`.
