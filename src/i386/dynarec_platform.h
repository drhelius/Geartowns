/*
 * Geartowns - FM Towns Emulator
 * Copyright (C) 2026  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#ifndef DYNAREC_PLATFORM_H
#define DYNAREC_PLATFORM_H

// The dynarec needs a GNU lightning backend for the host CPU and a way to get executable memory from the platform.
// Anything else builds without it, whatever the makefile asks for

#if !defined(GT_DISABLE_DYNAREC)
    #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86) || \
        defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(__mips__) || \
        defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(__riscv) || defined(__loongarch__)
        #define GT_DYNAREC_HOST_CPU
    #endif

    #if defined(__SWITCH__)
        #define GT_DYNAREC_LIBNX
    #elif defined(VITA) || defined(__vita__)
        #define GT_DYNAREC_VITA
    #elif defined(WIIU) || defined(__WIIU__)
        // The codegen area needs the RetroArch package to enable it
    #elif defined(GEKKO)
        #define GT_DYNAREC_GEKKO
    #elif defined(_WIN32)
        #define GT_DYNAREC_WINDOWS
    #elif defined(__APPLE__)
        #include <TargetConditionals.h>
        // iOS and tvOS only allow generated code to some processes, and not in a way RetroArch can rely on
        #if TARGET_OS_OSX
            #define GT_DYNAREC_APPLE
        #endif
    #elif defined(__EMSCRIPTEN__) || defined(_3DS) || defined(PSP) || defined(_EE) || defined(__PS3__) || \
        defined(__PSL1GHT__) || defined(SF2000) || defined(__OpenBSD__)
        // No executable memory, or no provider yet
    #elif defined(__unix__) || defined(__unix) || defined(__linux__) || defined(__ANDROID__) || defined(__HAIKU__) || \
        defined(__QNX__)
        #define GT_DYNAREC_POSIX
    #endif

    #if !defined(GT_DYNAREC_HOST_CPU) || !(defined(GT_DYNAREC_LIBNX) || defined(GT_DYNAREC_VITA) || \
        defined(GT_DYNAREC_GEKKO) || defined(GT_DYNAREC_WINDOWS) || defined(GT_DYNAREC_APPLE) || \
        defined(GT_DYNAREC_POSIX))
        #define GT_DISABLE_DYNAREC
    #endif
#endif

#if !defined(GT_DISABLE_DYNAREC)
    // Hosts whose loads and stores fault or trap on unaligned addresses use the unaligned lightning operations
    #if !(defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86) || \
        defined(__aarch64__) || defined(_M_ARM64) || (defined(__ARM_ARCH) && __ARM_ARCH >= 7))
        #define GT_DYNAREC_UNALIGNED
    #endif

    // Code cache size and block records: consoles, handhelds and 32-bit hosts get a smaller cache
    #if defined(GT_DYNAREC_LIBNX) || defined(GT_DYNAREC_VITA) || defined(GT_DYNAREC_GEKKO) || \
        defined(__ANDROID__) || !(defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || \
        defined(_M_ARM64) || (defined(__riscv) && __riscv_xlen == 64) || defined(__loongarch64))
        #define GT_DYNAREC_CODE_SIZE (8 * 1024 * 1024)
        #define GT_DYNAREC_MAX_BLOCKS 16384
    #else
        #define GT_DYNAREC_CODE_SIZE (32 * 1024 * 1024)
        #define GT_DYNAREC_MAX_BLOCKS 65536
    #endif
#endif

#endif /* DYNAREC_PLATFORM_H */
