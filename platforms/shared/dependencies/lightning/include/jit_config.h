/*
 * Geartowns build configuration for GNU lightning, in place of the config.h that configure generates.
 * Lightning only generates code here: it never maps memory, it has no disassembler and no data or notes.
 */

#ifndef _jit_config_h
#define _jit_config_h

#define HAVE_MMAP		0
#define HAVE_MREMAP		0

#if defined(_MSC_VER)
#  include <intrin.h>
#  if defined(_M_X64) && !defined(__x86_64__)
#    define __x86_64__		1
#  elif defined(_M_IX86) && !defined(__i386__)
#    define __i386__		1
#  elif defined(_M_ARM64) && !defined(__aarch64__)
#    define __aarch64__		1
#  elif defined(_M_ARM) && !defined(__arm__)
#    define __arm__		1
#  endif

/* Every Windows target is little-endian and has a 32-bit long */
#  define __LITTLE_ENDIAN	1234
#  define __BIG_ENDIAN		4321
#  define __BYTE_ORDER		__LITTLE_ENDIAN
#  if !defined(__SIZEOF_LONG__)
#    define __SIZEOF_LONG__	4
#  endif

static __inline int __builtin_popcountll(unsigned long long value)
{
    int count = 0;

    while (value != 0)
    {
        value &= value - 1;
        count++;
    }

    return count;
}

static __inline int __builtin_popcountl(unsigned long value)
{
    return __builtin_popcountll(value);
}

static __inline int __builtin_popcount(unsigned int value)
{
    return __builtin_popcountll(value);
}

static __inline int __builtin_ctzll(unsigned long long value)
{
    unsigned long index;
    _BitScanForward64(&index, value);
    return (int)index;
}

static __inline int __builtin_ctzl(unsigned long value)
{
    unsigned long index;
    _BitScanForward(&index, value);
    return (int)index;
}

static __inline int __builtin_clzll(unsigned long long value)
{
    unsigned long index;
    _BitScanReverse64(&index, value);
    return 63 - (int)index;
}

static __inline int __builtin_clzl(unsigned long value)
{
    unsigned long index;
    _BitScanReverse(&index, value);
    return 31 - (int)index;
}

static __inline int __builtin_clz(unsigned int value)
{
    return __builtin_clzl(value);
}

static __inline int __builtin_ffs(int value)
{
    unsigned long index;
    return _BitScanForward(&index, (unsigned long)value) ? (int)index + 1 : 0;
}

static __inline int __builtin_ffsll(long long value)
{
    unsigned long index;
    return _BitScanForward64(&index, (unsigned long long)value) ? (int)index + 1 : 0;
}
#endif

#endif /* _jit_config_h */
