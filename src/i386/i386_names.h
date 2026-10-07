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

#ifndef I386_NAMES_H
#define I386_NAMES_H

static const char* k_i386_opcode_names[256] =
{
/*          0                      1                      2                      3 */
/* 0x00 */  "add {Eb},{Gb}",       "add {Ev},{Gv}",       "add {Gb},{Eb}",       "add {Gv},{Ev}",
/* 0x04 */  "add al,{Ib}",         "add {eAX},{Iv}",      "push es",             "pop es",
/* 0x08 */  "or {Eb},{Gb}",        "or {Ev},{Gv}",        "or {Gb},{Eb}",        "or {Gv},{Ev}",
/* 0x0C */  "or al,{Ib}",          "or {eAX},{Iv}",       "push cs",             "@escape",

/* 0x10 */  "adc {Eb},{Gb}",       "adc {Ev},{Gv}",       "adc {Gb},{Eb}",       "adc {Gv},{Ev}",
/* 0x14 */  "adc al,{Ib}",         "adc {eAX},{Iv}",      "push ss",             "pop ss",
/* 0x18 */  "sbb {Eb},{Gb}",       "sbb {Ev},{Gv}",       "sbb {Gb},{Eb}",       "sbb {Gv},{Ev}",
/* 0x1C */  "sbb al,{Ib}",         "sbb {eAX},{Iv}",      "push ds",             "pop ds",

/* 0x20 */  "and {Eb},{Gb}",       "and {Ev},{Gv}",       "and {Gb},{Eb}",       "and {Gv},{Ev}",
/* 0x24 */  "and al,{Ib}",         "and {eAX},{Iv}",      "@prefix",             "daa",
/* 0x28 */  "sub {Eb},{Gb}",       "sub {Ev},{Gv}",       "sub {Gb},{Eb}",       "sub {Gv},{Ev}",
/* 0x2C */  "sub al,{Ib}",         "sub {eAX},{Iv}",      "@prefix",             "das",

/* 0x30 */  "xor {Eb},{Gb}",       "xor {Ev},{Gv}",       "xor {Gb},{Eb}",       "xor {Gv},{Ev}",
/* 0x34 */  "xor al,{Ib}",         "xor {eAX},{Iv}",      "@prefix",             "aaa",
/* 0x38 */  "cmp {Eb},{Gb}",       "cmp {Ev},{Gv}",       "cmp {Gb},{Eb}",       "cmp {Gv},{Ev}",
/* 0x3C */  "cmp al,{Ib}",         "cmp {eAX},{Iv}",      "@prefix",             "aas",

/* 0x40 */  "inc {rV}",            "inc {rV}",            "inc {rV}",            "inc {rV}",
/* 0x44 */  "inc {rV}",            "inc {rV}",            "inc {rV}",            "inc {rV}",
/* 0x48 */  "dec {rV}",            "dec {rV}",            "dec {rV}",            "dec {rV}",
/* 0x4C */  "dec {rV}",            "dec {rV}",            "dec {rV}",            "dec {rV}",

/* 0x50 */  "push {rV}",           "push {rV}",           "push {rV}",           "push {rV}",
/* 0x54 */  "push {rV}",           "push {rV}",           "push {rV}",           "push {rV}",
/* 0x58 */  "pop {rV}",            "pop {rV}",            "pop {rV}",            "pop {rV}",
/* 0x5C */  "pop {rV}",            "pop {rV}",            "pop {rV}",            "pop {rV}",

/* 0x60 */  "{PUSHA}",             "{POPA}",              "bound {Gv},{Ma}",     "arpl {Ew},{Gw}",
/* 0x64 */  "@prefix",             "@prefix",             "@prefix",             "@prefix",
/* 0x68 */  "push {Iv}",           "imul {Gv},{Ev},{Iv}", "push {Is}",           "imul {Gv},{Ev},{Is}",
/* 0x6C */  "insb",                "{INSv}",              "outsb",               "{OUTSv}",

/* 0x70 */  "jo {Jb}",             "jno {Jb}",            "jb {Jb}",             "jae {Jb}",
/* 0x74 */  "je {Jb}",             "jne {Jb}",            "jbe {Jb}",            "ja {Jb}",
/* 0x78 */  "js {Jb}",             "jns {Jb}",            "jp {Jb}",             "jnp {Jb}",
/* 0x7C */  "jl {Jb}",             "jge {Jb}",            "jle {Jb}",            "jg {Jb}",

/* 0x80 */  "@g1b",                "@g1v",                "@g1b",                "@g1s",
/* 0x84 */  "test {Eb},{Gb}",      "test {Ev},{Gv}",      "xchg {Eb},{Gb}",      "xchg {Ev},{Gv}",
/* 0x88 */  "mov {Eb},{Gb}",       "mov {Ev},{Gv}",       "mov {Gb},{Eb}",       "mov {Gv},{Ev}",
/* 0x8C */  "mov {EwRv},{Sw}",     "lea {Gv},{M}",        "mov {Sw},{Ew}",       "pop {Ev}",

/* 0x90 */  "nop",                 "xchg {eAX},{rV}",     "xchg {eAX},{rV}",     "xchg {eAX},{rV}",
/* 0x94 */  "xchg {eAX},{rV}",     "xchg {eAX},{rV}",     "xchg {eAX},{rV}",     "xchg {eAX},{rV}",
/* 0x98 */  "{CBW}",               "{CWD}",               "call {FAR}{Ap}",      "wait",
/* 0x9C */  "{PUSHF}",             "{POPF}",              "sahf",                "lahf",

/* 0xA0 */  "mov al,{Ob}",         "mov {eAX},{Ov}",      "mov {Ob},al",         "mov {Ov},{eAX}",
/* 0xA4 */  "movsb",               "{MOVSv}",             "cmpsb",               "{CMPSv}",
/* 0xA8 */  "test al,{Ib}",        "test {eAX},{Iv}",     "stosb",               "{STOSv}",
/* 0xAC */  "lodsb",               "{LODSv}",             "scasb",               "{SCASv}",

/* 0xB0 */  "mov {r8},{Ib}",       "mov {r8},{Ib}",       "mov {r8},{Ib}",       "mov {r8},{Ib}",
/* 0xB4 */  "mov {r8},{Ib}",       "mov {r8},{Ib}",       "mov {r8},{Ib}",       "mov {r8},{Ib}",
/* 0xB8 */  "mov {rV},{Iv}",       "mov {rV},{Iv}",       "mov {rV},{Iv}",       "mov {rV},{Iv}",
/* 0xBC */  "mov {rV},{Iv}",       "mov {rV},{Iv}",       "mov {rV},{Iv}",       "mov {rV},{Iv}",

/* 0xC0 */  "@g2bi",               "@g2vi",               "ret {Iw}",            "ret",
/* 0xC4 */  "les {Gv},{Mp}",       "lds {Gv},{Mp}",       "mov {Eb},{Ib}",       "mov {Ev},{Iv}",
/* 0xC8 */  "enter {Iw},{Ib2}",    "leave",               "retf {Iw}",           "retf",
/* 0xCC */  "int3",                "int {Ib}",            "into",                "{IRET}",

/* 0xD0 */  "@g2b1",               "@g2v1",               "@g2bc",               "@g2vc",
/* 0xD4 */  "aam {Ib}",            "aad {Ib}",            "salc",                "xlat",
/* 0xD8 */  "@x87",                "@x87",                "@x87",                "@x87",
/* 0xDC */  "@x87",                "@x87",                "@x87",                "@x87",

/* 0xE0 */  "loopne {Jb}",         "loope {Jb}",          "loop {Jb}",           "{JCXZ} {Jb}",
/* 0xE4 */  "in al,{Ib}",          "in {eAX},{Ib}",       "out {Ib},al",         "out {Ib},{eAX}",
/* 0xE8 */  "call {Jv}",           "jmp {Jv}",            "jmp {FAR}{Ap}",       "jmp {Jb}",
/* 0xEC */  "in al,dx",            "in {eAX},dx",         "out dx,al",           "out dx,{eAX}",

/* 0xF0 */  "@prefix",             "icebp",               "@prefix",             "@prefix",
/* 0xF4 */  "hlt",                 "cmc",                 "@g3b",                "@g3v",
/* 0xF8 */  "clc",                 "stc",                 "cli",                 "sti",
/* 0xFC */  "cld",                 "std",                 "@g4",                 "@g5"
};

static_assert(sizeof(k_i386_opcode_names) / sizeof(k_i386_opcode_names[0]) == 256,
    "The 80386 primary disassembly table must contain 256 entries");

struct I386_Extended_Opcode_Name
{
    u8 opcode;
    const char* name;
};

static const I386_Extended_Opcode_Name k_i386_extended_opcode_names[] =
{
    { 0x00, "@g6" },                 // Group 6: SLDT/STR/LLDT/LTR/VERR/VERW
    { 0x01, "@g7" },                 // Group 7: SGDT/SIDT/LGDT/LIDT/SMSW/LMSW
    { 0x02, "lar {Gv},{Ew}" },       // LAR r16/32,r/m16
    { 0x03, "lsl {Gv},{Ew}" },       // LSL r16/32,r/m16
    { 0x06, "clts" },                // CLTS
    { 0x20, "mov {Rd},{Cd}" },       // MOV r32,CRn
    { 0x21, "mov {Rd},{Dd}" },       // MOV r32,DRn
    { 0x22, "mov {Cd},{Rd}" },       // MOV CRn,r32
    { 0x23, "mov {Dd},{Rd}" },       // MOV DRn,r32
    { 0x24, "mov {Rd},{Td}" },       // MOV r32,TRn
    { 0x26, "mov {Td},{Rd}" },       // MOV TRn,r32

    { 0x80, "jo {Jv}" },
    { 0x81, "jno {Jv}" },
    { 0x82, "jb {Jv}" },
    { 0x83, "jae {Jv}" },
    { 0x84, "je {Jv}" },
    { 0x85, "jne {Jv}" },
    { 0x86, "jbe {Jv}" },
    { 0x87, "ja {Jv}" },
    { 0x88, "js {Jv}" },
    { 0x89, "jns {Jv}" },
    { 0x8A, "jp {Jv}" },
    { 0x8B, "jnp {Jv}" },
    { 0x8C, "jl {Jv}" },
    { 0x8D, "jge {Jv}" },
    { 0x8E, "jle {Jv}" },
    { 0x8F, "jg {Jv}" },

    { 0x90, "seto {Eb}" },
    { 0x91, "setno {Eb}" },
    { 0x92, "setb {Eb}" },
    { 0x93, "setae {Eb}" },
    { 0x94, "sete {Eb}" },
    { 0x95, "setne {Eb}" },
    { 0x96, "setbe {Eb}" },
    { 0x97, "seta {Eb}" },
    { 0x98, "sets {Eb}" },
    { 0x99, "setns {Eb}" },
    { 0x9A, "setp {Eb}" },
    { 0x9B, "setnp {Eb}" },
    { 0x9C, "setl {Eb}" },
    { 0x9D, "setge {Eb}" },
    { 0x9E, "setle {Eb}" },
    { 0x9F, "setg {Eb}" },

    { 0xA0, "push fs" },             // PUSH FS
    { 0xA1, "pop fs" },              // POP FS
    { 0xA3, "bt {Ev},{Gv}" },        // BT r/m16/32,r16/32
    { 0xA4, "shld {Ev},{Gv},{Ib}" }, // SHLD r/m16/32,r16/32,imm8
    { 0xA5, "shld {Ev},{Gv},cl" },   // SHLD r/m16/32,r16/32,CL
    { 0xA8, "push gs" },             // PUSH GS
    { 0xA9, "pop gs" },              // POP GS
    { 0xAB, "bts {Ev},{Gv}" },       // BTS r/m16/32,r16/32
    { 0xAC, "shrd {Ev},{Gv},{Ib}" }, // SHRD r/m16/32,r16/32,imm8
    { 0xAD, "shrd {Ev},{Gv},cl" },   // SHRD r/m16/32,r16/32,CL
    { 0xAF, "imul {Gv},{Ev}" },      // IMUL r16/32,r/m16/32

    { 0xB2, "lss {Gv},{Mp}" },       // LSS r16/32,m16:16/32
    { 0xB3, "btr {Ev},{Gv}" },       // BTR r/m16/32,r16/32
    { 0xB4, "lfs {Gv},{Mp}" },       // LFS r16/32,m16:16/32
    { 0xB5, "lgs {Gv},{Mp}" },       // LGS r16/32,m16:16/32
    { 0xB6, "movzx {Gv},{Eb}" },     // MOVZX r16/32,r/m8
    { 0xB7, "movzx {Gv},{Ew}" },     // MOVZX r16/32,r/m16
    { 0xBA, "@g8" },                 // BT/BTS/BTR/BTC r/m16/32,imm8
    { 0xBB, "btc {Ev},{Gv}" },       // BTC r/m16/32,r16/32
    { 0xBC, "bsf {Gv},{Ev}" },       // BSF r16/32,r/m16/32
    { 0xBD, "bsr {Gv},{Ev}" },       // BSR r16/32,r/m16/32
    { 0xBE, "movsx {Gv},{Eb}" },     // MOVSX r16/32,r/m8
    { 0xBF, "movsx {Gv},{Ew}" }      // MOVSX r16/32,r/m16
};

#endif /* I386_NAMES_H */
