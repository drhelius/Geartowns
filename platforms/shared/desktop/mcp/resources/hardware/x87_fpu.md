# Intel x87 FPU — What FM Towns Software Relies On

## 1. Overview

The x87 is the floating point unit behind the ESC opcodes D8h–DFh and WAIT (9Bh). Which models have one:

| Model | CPU | FPU |
|---|---|---|
| Model 1/2, 1F/2F, 10F/20F, CX, HG | 80386DX | 80387 card, optional |
| UX, UG, Marty | 80386SX | None, no socket |
| HR, ME, MA, MF/Fresh | 80486SX | 487SX (ODP) card, optional |
| UR | 80486SX | None, no socket |
| MX | 80486DX2 | Integrated, always present |
| HC | Pentium | Integrated, always present |

- **Setting:** `fpu` is `auto`, `none` or `80387` (a 487SX on the 486SX models), from `--fpu`, the machine menu or
  the settings, applied at the next reset (`reset_pending` in `get_system_status`). **Auto fits none**, except where the
  game database asks for one: Para Para Paradise (discs A, B and C), whose game program is linked against an x87-only
  library. A fitted FPU changes the code paths and timing of titles that probe for it, so it is not on by default.
  Models without a socket ignore the setting with the warning "This machine has no socket for an FPU".
- **Two flavors of one unit:** the 80387 flavor on the 386s, the 486 flavor on the 486SX and DX2. They share the
  arithmetic and differ in reset, #UD, timing and the environment pointers.
- **What software is told:** `INT AFh` AH=05h returns a 16-byte machine information block whose byte 7 is 1 when a
  coprocessor is present; Towns OS fills it by probing the FPU at boot (section 6).
- **See also:** CR0 bits, exception vectors and the IRQ vectors are in `i386_cpu`.

---

## 2. Register Stack and Tags

Eight 80-bit registers R0–R7: sign, 15-bit exponent (bias 3FFFh) and a 64-bit significand with an explicit integer
bit. ST(i) is R[(TOP+i) mod 8]. A push decrements TOP and writes ST(0), a pop marks ST(0) empty and increments TOP.
FINCSTP and FDECSTP change TOP only.

- **Tags** are two bits per physical register, R0 in bits 1–0: 00 valid, 01 zero, 10 special (NaN, infinity, denormal,
  unsupported), 11 empty. The emulator stores only empty or in use, and works out the rest from the contents when the
  tag word is read (FSTENV, FSAVE, MCP).
- **Stack faults:** a push onto a full stack or an operation on an empty register sets IE and SF. C1 = 1 means
  overflow, C1 = 0 underflow. With IM masked the result is the real indefinite QNaN (FFFFh, C000000000000000h) written
  where the result would go.

---

## 3. Control Word

FNINIT gives 037Fh: all exceptions masked, 64-bit precision, round to nearest.

| Bits | Field | Meaning |
|---:|---|---|
| 0–5 | IM DM ZM OM UM PM | Mask bits: invalid, denormal, zero divide, overflow, underflow, precision |
| 9–8 | PC | Precision: 00 = 24 bits, 10 = 53 bits, 11 = 64 bits. 01 is reserved and rounds as 64 bits |
| 11–10 | RC | Rounding: 00 nearest-even, 01 down, 10 up, 11 chop |
| 12 | IC | Infinity control: stored, ignored. Infinity is always affine (-inf < +inf) |
| 7–6, 15–13 | — | Reserved: bit 6 reads 1, the others read 0 |

- **Loading is filtered:** FLDCW, FLDENV and FRSTOR clear bits 15–13 and 7 and set bit 6, so `FLDCW 83FFh` followed by
  FNSTCW reads back with bit 15 clear. FLDCW that unmasks a pending flag sets ES at once.
- **PC applies to** FADD, FSUB, FMUL, FDIV, their reverse and integer forms, and FSQRT. Loads, stores and the
  transcendentals ignore it. A store to single or double rounds to that type regardless of PC.

---

## 4. Status Word

| Bit | Name | Meaning |
|---:|---|---|
| 0–5 | IE DE ZE OE UE PE | Sticky exception flags, set even when the exception is masked |
| 6 | SF | Stack fault, with IE |
| 7 | ES | Error summary: an unmasked exception flag is set |
| 8, 9, 10 | C0, C1, C2 | Condition codes |
| 13–11 | TOP | Physical register at ST(0) |
| 14 | C3 | Condition code |
| 15 | B | Busy: set with ES, cleared with it |

- **FNSTSW AX** copies the word to AX; SAHF maps C0 to CF, C2 to PF and C3 to ZF. After a compare (FCOM, FUCOM,
  FICOM, FTST) C3 C2 C0 = 000 ST(0) greater, 001 less, 100 equal, 111 unordered.
- **FXAM** gives class in C3 C2 C0 and sign in C1. **FPREM, FPREM1** give the quotient's low three bits in C1 (Q0), C3
  (Q1), C0 (Q2) when C2 = 0, and C2 = 1 when the reduction is incomplete (loop until C2 clears).
- **FPTAN, FSIN, FCOS, FSINCOS** with an operand of 2^63 or more set C2 = 1 and leave the stack alone.
- **Flags are cleared only by** FNCLEX, FNINIT, FLDENV, FRSTOR and FNSAVE. FNSTENV clears B and ES but keeps the flags.

---

## 5. Exceptions and Error Reporting

| Flag | Cause | Masked result |
|---|---|---|
| IE | Invalid operation: stack fault, signaling NaN operand, 0/0, inf - inf, unsupported format | Indefinite QNaN |
| DE | Denormal operand | Operation continues |
| ZE | Finite / 0 | Signed infinity |
| OE, UE | Result out of range | Infinity, denormal or zero by rounding mode |
| PE | Result was rounded | The rounded result; C1 tells round up |

An unmasked exception sets its flag, ES and B, and does not store the result (a precision exception still does). The
error is **reported late**, at the next waiting ESC or WAIT, not at the instruction that caused it. FNINIT, FNCLEX,
FNSTENV, FNSAVE, FNSTCW, FNSTSW (memory and AX) and FNENI, FNDISI, FNSETPM never wait; FSTSW and friends are WAIT
followed by the no-wait form.

### 5.1 CR0 and the CPU side

- **EM or TS set:** every ESC raises #NM (vector 7) before it runs, with or without an FPU fitted.
- **MP and TS both set:** WAIT raises #NM. This is the lazy state-switch pattern.
- **ET:** on a 386 it is set at reset only when an 80387 is fitted (the 387 holds ERROR# active after reset), and
  software may write it. On a 486 it is wired to 1.
- **#MF, always:** a pending error is raised as exception 16 before the next waiting ESC or WAIT, on the 386 with a
  387 and on a 486 whatever NE says. The CPU takes the error itself.
- **No IRQ13 route:** on the Towns IRQ13 is the sound interrupt (vector 4Dh), so there is no AT-style path from the
  coprocessor's error line to a PIC input, no latch to clear and no FPU I/O port. How the 387 card's ERROR# and the
  486's FERR# are wired has not been verified on hardware, so the error takes the one route above. The CPU never
  stops waiting for an interrupt and IGNNE# is not modelled.
- **System ROM:** the Model 1/2 and HR ROMs run FNINIT at reset and nothing else on the FPU. The HR ROM (also run by
  the HG) clears CD, NW, AM, WP, NE, EM and MP in CR0 and then sets NE, so its error route is #MF.
- **Data book:** its CR0 text contradicts itself on what NE = 0 does, so it cannot settle the 486 route.
- **Exception 9** (coprocessor segment overrun) exists on the 386 only: an operand that wraps the address limit with
  an inaccessible byte in the middle. The 386 checks an operand's first and last byte, the 486 checks it whole and
  takes #GP, #SS, #PF or #AC (with AM and AC set) as for any access.
- **486 only:** an undefined ESC encoding raises #UD, for example any `DA` register form except `DA E9` and any `DB`
  register form except `DB E0`–`DB E4`. The 387 flavor ignores them.

### 5.2 No FPU fitted

- ESC raises #NM when EM or TS is set. Otherwise memory operands are only checked for faults. FNSTSW AX returns
  FFFFh, and FNSTCW and FNSTSW to memory store FFFFh. WAIT is a no-op unless MP and TS are set. On a 386 each such ESC
  costs 20 extra clocks, the bus cycle nobody answers.
- Recommended CR0 on a 486SX with no 487SX: EM = 1, MP = 0, NE = 1, so every ESC reaches an emulator through #NM.
  With a 487SX or a DX: EM = 0, MP = 1, NE = 0 for DOS.

---

## 6. Detection

- **Any FPU:** FNINIT; store a non-zero word; FNSTSW into it; a low byte of 0 means present. Then FNSTCW, AND AX,103Fh
  and CMP AX,003Fh. Use the no-wait forms. With an FPU the words read 0000h and 037Fh; without one, FFFFh each. That
  also tells a bare 486SX from a 487SX.
- **387 or 287:** FLD1, FLDZ, FDIV (1/0 = +inf), FLD ST, FCHS, FCOMPP, FSTSW AX, SAHF. ZF set means the infinities
  compared equal (projective, 8087 or 287), ZF clear means 387 (affine). The emulator is always affine.
- **State after reset** (what a program that skips FNINIT sees):

| Flavor | CW | SW | TW | Pointers |
|---|---|---|---|---|
| 387 (every CPU reset) | 037Eh | 8081h: B, ES and IE set | FFFFh | 0 |
| 486 | 037Fh | 0000h | FFFFh | 0; a CPU reset leaves the FPU as it was |

  The 387 asserts ERROR# after reset, which is why ET reads 1. FNINIT clears it: CW 037Fh, SW 0000h, TW FFFFh. Only
  the 486 flavor's FNINIT also zeroes the pointers; the 387 keeps them.
- **Probes in Towns software:**

| Software | Probe | Uses the answer for |
|---|---|---|
| IO.SYS (V1.1, V2.1) | FNINIT, WAIT, FNSTSW AX; present when AL = 0 | The `INT AFh` AH=05h byte 7 |
| RUN386 (Phar Lap) | FNINIT; FNSTCW masked with 1F3Fh must read 033Fh; FNSTSW masked with B8BFh must read 0; the infinity compare | With a 387, MP and ET set in protected mode and a WAIT at every real and protected switch; `-387fix` |
| High C runtime | As RUN386 with 0F3Fh, then FLDCW 83FFh and FNSTCW must read bit 15 clear | Its float library runs x87 code or software routines; an x87-only build prints an "80387 required" message and exits |
| F-BASIC386 runtime | FNINIT, FNSTSW over 5A5Ah | The same for BASIC programs |
| Watcom C/386 runtime | FNINIT; FNSTCW high byte 3; the infinity compare | Its `_8087` flag |
| CHKX87.EXE (ElFish) | `INT AFh` AH=05h, byte 7 as the errorlevel | AUTOEXEC.BAT picks a coprocessor build |

  Those two ROMs never read the FPU back, so detection lives in the OS and the extenders.

---

## 7. State Images

FNSTENV and FLDENV move 14 bytes (16-bit operand size) or 28 bytes (32-bit). FNSAVE and FRSTOR add the eight
registers as 10-byte images in stack order, ST(0) first: 94 or 108 bytes. FNSAVE then reinitializes the FPU as FNINIT.
FNSTENV also sets all six mask bits.

The fields, in order: control word, status word, tag word (exact tags from the register contents), instruction
offset, instruction CS selector, operand offset, operand selector. The 16-bit image packs them as words at 0, 2, 4, 6,
8, 10 and 12; the 32-bit image as dwords at 0, 4, 8, 12, 16, 20 and 24, with the opcode in bits 26–16 of the CS dword.

- **Real mode and VM86** store the 20-bit linear address of the instruction and operand instead of selectors: the low
  16 bits in the offset field, bits 19–16 in bits 15–12 of the next field, the opcode in bits 10–0. The mode at the ESC
  decides.
- **Pointers** are written by every ESC except the control group: address of the ESC instruction, 11-bit opcode and,
  for a memory operand, its selector and offset. `last_instruction`, `last_opcode` and `last_operand` show them.
- **486 only:** a FLDENV that faults on the limit leaves CW = 007Fh, and misaligned operands raise #AC.

---

## 8. Instruction Groups and Precision

| Group | Instructions |
|---|---|
| Load and store | FLD (m32, m64, m80, ST(i)), FILD, FBLD, FST, FSTP, FIST, FISTP, FBSTP, FXCH |
| Arithmetic | FADD, FSUB, FSUBR, FMUL, FDIV, FDIVR in D8, DC, DE (pop), DA and DE (integer memory) forms |
| Compare | FCOM, FCOMP, FCOMPP, FUCOM, FUCOMP, FUCOMPP (`DA E9`), FICOM, FICOMP, FTST, FXAM |
| Constants | FLD1, FLDL2T, FLDL2E, FLDPI, FLDLG2, FLDLN2, FLDZ (rounded by RC), FABS, FCHS |
| Math | F2XM1, FYL2X, FYL2XP1, FPATAN, FPTAN, FSIN, FCOS, FSINCOS, FSQRT, FSCALE, FRNDINT, FXTRACT, FPREM, FPREM1 |
| Control | FNINIT, FNCLEX, FLDCW, FNSTCW, FNSTSW, FLDENV, FNSTENV, FRSTOR, FNSAVE, FINCSTP, FDECSTP, FFREE, FNOP |

- **FENI, FDISI and FSETPM** (`DB E0`, `DB E1`, `DB E4`) are accepted and do nothing. No later x87 encodings exist.
- **Exact arithmetic:** values are 80-bit extended, rounded by PC and RC with correct IEEE results for add, subtract,
  multiply, divide, square root, conversions and compares, including denormals, NaN propagation and unsupported
  formats. F2XM1, FYL2X, FYL2XP1, FPATAN, FSIN, FCOS, FPTAN and FSINCOS always compute at 64 bits, with algorithms
  accurate to extended precision whose last bit may differ from a real chip. No FPU errata are modelled.
- **Fast path:** add, subtract and multiply of ordinary finite operands, compares, and loads and stores of single,
  double and 16 or 32-bit integers run on 64-bit integer arithmetic, with the same results and flags (PE, C1) as the
  exact path. NaN, infinity, denormals, range overflow, divides and 64-bit integer stores use the exact path.

---

## 9. Timing

The FPU is synchronous: the CPU is charged the whole instruction and nothing overlaps.

- **387 flavor (386DX, 386SX):** the midpoint of the 387 DX datasheet range for each form, plus the CPU's address
  clocks; loading a single or double zero costs 5 more.
- **486 flavor (486SX with 487SX, DX2):** Intel's cache-hit averages for each form. FST and FSTP of a zero take 27
  (single) and 28 (double). FDIV, FDIVR, FIDIV and their pops are 38 clocks shorter at 24-bit PC and 11 shorter at
  53-bit. FNINIT, FNCLEX, FNSTENV and FNSAVE cost 17 more while an error is pending, FNSTCW and FNSTSW 18 more. FLDENV,
  FNSTENV, FRSTOR and FNSAVE take their protected-mode counts there (34, 56, 120, 143).

---

## 10. Driving it from MCP

- **`get_i386_status`:** the `fpu` object, null when none is fitted. `control`, `status` and `tag` are 4-digit hex,
  `top` a number; `masks` (`im` `dm` `zm` `om` `um` `pm`, true = masked), `precision` and `rounding` as names,
  `conditions` (`c0`–`c3`), `exceptions` (`ie` `de` `ze` `oe` `ue` `pe` `sf` `es` `busy`). `stack` lists ST(0) to ST(7) with `st`, physical `register`, `tag` (`valid`, `zero`,
  `special`, `empty`), `raw` (4 hex digits of sign and exponent, then 16 of significand) and `value` (decimal, `+INF`,
  `QNAN`, `SNAN`, `UNSUPPORTED`; null when empty), then `last_instruction`, `last_opcode` and `last_operand`.
- **Same call, CPU side:** `cr0_flags` has `mp`, `em`, `ts`, `et` and, on a 486, `ne`; `last_exception` is the last
  vector the CPU raised (07 #NM, 10 #MF).
- **`get_system_status`:** `machine.fpu` (`80387`, `487SX`, `integrated` or null), `machine.settings.fpu` (`auto`,
  `none` or `80387`), `machine.source.fpu` (`model`, `user` or `database`), `machine.warnings` and `reset_pending`.
- **`write_i386_register`:** CR0 reaches EM, TS and MP (and NE on a 486), which is the way to force the #NM path. It
  does not write FPU registers; to change one, run FLD or FLDCW code, for example bytes placed with `write_memory` and
  stepped.
- **`write_io_port`:** an OUT as the CPU does it, with `port` (hex), `value` (hex) and `size` 8, 16 or 32. The FPU has
  no port on the Towns, so it is of no use for the x87 itself.
- **`set_breakpoint_on_interrupt`:** `vector` 7 with `source` exception stops on #NM and `vector` 16 on #MF, before
  the handler's first instruction. Do not use `irq` 13 for the FPU: it is the sound IRQ.
- **`get_pic_status`:** only to confirm that nothing FPU-related raises an IRQ; there is no coprocessor line.
- **`set_trace_log`:** filter `cpu.exceptions` logs 7, 16 and 9 as they are raised, with `cpu.instructions` for the
  ESC stream. `cpu.irqs` is not needed for the FPU.
