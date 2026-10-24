#!/usr/bin/env python3
"""Merges the x87 arithmetic of Bochs into bochs_fpu.h and bochs_fpu.cpp.

amalgamate.py <bochs> [output]

<bochs> is a checkout of https://github.com/bochs-emu/Bochs (its root, its bochs directory or bochs/cpu). Every
quoted include is inlined once, where it first appears; config.h is replaced by CONFIG. The output directory
defaults to the directory of this script.
"""

import os
import re
import shutil
import subprocess
import sys

PUBLIC = [
    "softfloat3e/include/softfloat.h",
    "fpu/fpu_trans.h",
]

SOURCES = [
    "softfloat3e/extF80_addsub.cc",
    "softfloat3e/extF80_class.cc",
    "softfloat3e/extF80_compare.cc",
    "softfloat3e/extF80_div.cc",
    "softfloat3e/extF80_extract.cc",
    "softfloat3e/extF80_mul.cc",
    "softfloat3e/extF80_roundToInt.cc",
    "softfloat3e/extF80_scale.cc",
    "softfloat3e/extF80_sqrt.cc",
    "softfloat3e/extF80_to_f128.cc",
    "softfloat3e/extF80_to_f32.cc",
    "softfloat3e/extF80_to_f64.cc",
    "softfloat3e/extF80_to_i32.cc",
    "softfloat3e/extF80_to_i64.cc",
    "softfloat3e/f128_addsub.cc",
    "softfloat3e/f128_div.cc",
    "softfloat3e/f128_mul_by_extF80.cc",
    "softfloat3e/f128_mul.cc",
    "softfloat3e/f128_round.cc",
    "softfloat3e/f128_to_extF80.cc",
    "softfloat3e/f32_to_extF80.cc",
    "softfloat3e/f64_to_extF80.cc",
    "softfloat3e/i32_to_extF80.cc",
    "softfloat3e/i32_to_f128.cc",
    "softfloat3e/i64_to_extF80.cc",
    "softfloat3e/isNaN.cc",
    "softfloat3e/isSignalingNaN.cc",
    "softfloat3e/s_addMagsExtF80.cc",
    "softfloat3e/s_addMagsF128.cc",
    "softfloat3e/s_approxRecipSqrt_1Ks.cc",
    "softfloat3e/s_approxRecipSqrt32_1.cc",
    "softfloat3e/s_countLeadingZeros64.cc",
    "softfloat3e/s_countLeadingZeros8.cc",
    "softfloat3e/s_mul128To256M.cc",
    "softfloat3e/s_mul64To128.cc",
    "softfloat3e/s_normRoundPackToExtF80.cc",
    "softfloat3e/s_normRoundPackToF128.cc",
    "softfloat3e/s_normSubnormalExtF80Sig.cc",
    "softfloat3e/s_normSubnormalF128Sig.cc",
    "softfloat3e/s_normSubnormalF32Sig.cc",
    "softfloat3e/s_normSubnormalF64Sig.cc",
    "softfloat3e/s_packToExtF80.cc",
    "softfloat3e/s_roundPackToExtF80.cc",
    "softfloat3e/s_roundPackToF128.cc",
    "softfloat3e/s_roundPackToF32.cc",
    "softfloat3e/s_roundPackToF64.cc",
    "softfloat3e/s_roundToI32.cc",
    "softfloat3e/s_roundToI64.cc",
    "softfloat3e/s_shiftRightJam128.cc",
    "softfloat3e/s_shiftRightJam128Extra.cc",
    "softfloat3e/s_subMagsExtF80.cc",
    "softfloat3e/s_subMagsF128.cc",
    "softfloat3e/8086-SSE/s_commonNaNToExtF80UI.cc",
    "softfloat3e/8086-SSE/s_commonNaNToF128UI.cc",
    "softfloat3e/8086-SSE/s_commonNaNToF32UI.cc",
    "softfloat3e/8086-SSE/s_commonNaNToF64UI.cc",
    "softfloat3e/8086-SSE/s_propagateNaNExtF80UI.cc",
    "softfloat3e/8086-SSE/s_propagateNaNF128UI.cc",
    "fpu/f128_67.cc",
    "fpu/f2xm1.cc",
    "fpu/fpatan.cc",
    "fpu/fprem.cc",
    "fpu/fsincos.cc",
    "fpu/fyl2x.cc",
]

INCLUDE_DIRS = ["softfloat3e/include", "."]

LICENSES = [
    ("softfloat3e/COPYING.txt", "COPYING.txt"),
    ("../COPYING", "COPYING.LESSER"),
]

CONFIG = """#include <stdint.h>

typedef uint8_t Bit8u;
typedef int8_t Bit8s;
typedef uint16_t Bit16u;
typedef int16_t Bit16s;
typedef uint32_t Bit32u;
typedef int32_t Bit32s;
typedef uint64_t Bit64u;
typedef int64_t Bit64s;

#define BX_CONST64(x) (x##LL)
#define BX_CPP_INLINE inline

#if !defined(MSB_FIRST) && !defined(__BIG_ENDIAN__) && !(defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define BX_LITTLE_ENDIAN
#endif

#if defined(__GNUC__) || defined(__clang__)
#define BX_HAVE___BUILTIN_CLZ 1
#else
#define BX_HAVE___BUILTIN_CLZ 0
#endif

#if defined(__SIZEOF_INT128__)
#define BX_HAVE_INT128 1
#else
#define BX_HAVE_INT128 0
#endif

#define SOFTFLOAT_FAST_INT64
#define INLINE_LEVEL 5
#define SOFTFLOAT_FAST_DIV32TO16
#define SOFTFLOAT_FAST_DIV64TO32

#define FLOAT128
"""

INCLUDE = re.compile(r'^\s*#\s*include\s*([<"])([^">]+)[">]')


class Merger:
    def __init__(self, cpu):
        self.cpu = cpu
        self.done = set()

    def resolve(self, name, directory):
        for base in [directory] + [os.path.join(self.cpu, d) for d in INCLUDE_DIRS]:
            path = os.path.normpath(os.path.join(base, name))
            if os.path.isfile(path):
                return path
        sys.exit("can't find %s, included from %s" % (name, os.path.relpath(directory, self.cpu)))

    def expand(self, path, out):
        self.done.add(path)
        out.append("/* bochs/cpu/%s */\n" % os.path.relpath(path, self.cpu).replace(os.sep, "/"))
        with open(path, encoding="latin-1", newline="") as f:
            text = f.read().replace("\r\n", "\n")
        for line in text.splitlines(True):
            match = INCLUDE.match(line)
            name = match.group(2) if match else None
            if not match or (match.group(1) == "<" and name != "config.h"):
                out.append(line)
                continue
            if name == "config.h":
                if "config.h" not in self.done:
                    self.done.add("config.h")
                    out.append(CONFIG)
                continue
            target = self.resolve(name, os.path.dirname(path))
            if target not in self.done:
                self.expand(target, out)
        if out and not out[-1].endswith("\n"):
            out.append("\n")


def find_cpu(path):
    for candidate in [path, os.path.join(path, "cpu"), os.path.join(path, "bochs", "cpu")]:
        if os.path.isfile(os.path.join(candidate, "softfloat3e", "include", "softfloat.h")):
            return os.path.abspath(candidate)
    sys.exit("%s is not a Bochs checkout" % path)


def commit(cpu):
    try:
        result = subprocess.run(["git", "-C", cpu, "rev-parse", "HEAD"], capture_output=True, text=True)
    except OSError:
        return None
    return result.stdout.strip() if result.returncode == 0 else None


def write(path, parts):
    with open(path, "w", encoding="latin-1", newline="\n") as f:
        f.write("".join(parts))


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    cpu = find_cpu(sys.argv[1])
    output = sys.argv[2] if len(sys.argv) == 3 else os.path.dirname(os.path.abspath(__file__))
    revision = commit(cpu)
    banner = "/* Generated by amalgamate.py from Bochs%s */\n" % (" " + revision if revision else "")

    merger = Merger(cpu)
    header = [banner, "#ifndef BOCHS_FPU_H\n", "#define BOCHS_FPU_H\n"]
    for name in PUBLIC:
        path = os.path.normpath(os.path.join(cpu, name))
        if path not in merger.done:
            merger.expand(path, header)
    header.append("#endif\n")

    source = [banner, '#include "bochs_fpu.h"\n']
    for name in SOURCES:
        merger.expand(os.path.normpath(os.path.join(cpu, name)), source)

    write(os.path.join(output, "bochs_fpu.h"), header)
    write(os.path.join(output, "bochs_fpu.cpp"), source)
    for name, target in LICENSES:
        shutil.copyfile(os.path.normpath(os.path.join(cpu, name)), os.path.join(output, target))
    print("bochs_fpu.h: %d lines, bochs_fpu.cpp: %d lines, %d files merged" % (
        "".join(header).count("\n"), "".join(source).count("\n"), len(merger.done) - 1))


if __name__ == "__main__":
    main()
