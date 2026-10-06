# Intel 80386 — What FM Towns Software Relies On

## 1. Overview

The FM Towns Model 1/2 uses an Intel 80386DX at 16 MHz; an 80387 coprocessor is optional. The 80386 has no
on-chip cache. The system ROM starts in real mode. Towns OS combines MS-DOS with a 386 DOS extender: DOS runs in
real mode and applications run as 32-bit protected-mode `.EXP` programs, one at a time, without using
virtual-8086 mode as their environment. This reference covers the system architecture a debugger user needs; it is
not an instruction set reference.

---

## 2. Registers

| Register | Width | Use |
|---|---:|---|
| EAX, EBX, ECX, EDX | 32 | General; AX/AH/AL etc. are the low parts |
| ESI, EDI | 32 | General; string source and destination |
| EBP | 32 | General; frame pointer, addresses SS by default |
| ESP | 32 | Stack pointer (SP when the stack segment is 16-bit) |
| EIP | 32 | Offset of the next instruction in CS (IP in 16-bit code) |
| EFLAGS | 32 | Status and control flags |
| CS, DS, ES, FS, GS, SS | 16 | Segment selectors, each with a hidden descriptor cache (base, limit, attributes) |
| CR0, CR2, CR3 | 32 | Control: mode bits, page-fault address, page directory base (CR1 is reserved) |
| GDTR, IDTR | 48 | Base and limit of the global descriptor and interrupt tables |
| LDTR, TR | 16 | Selectors of the local descriptor table and task state segment, with hidden caches |
| DR0–DR3, DR6, DR7 | 32 | Hardware breakpoints, status and control |
| TR6, TR7 | 32 | TLB test registers |

### 2.1 EFLAGS

| Bit | Flag | Meaning |
|---:|---|---|
| 0 | CF | Carry |
| 2 | PF | Parity of the low result byte |
| 4 | AF | Auxiliary carry (BCD) |
| 6 | ZF | Zero |
| 7 | SF | Sign |
| 8 | TF | Trap (single step) |
| 9 | IF | Maskable interrupts enabled |
| 10 | DF | String direction, 1 decrements |
| 11 | OF | Overflow |
| 13–12 | IOPL | I/O privilege level |
| 14 | NT | Nested task |
| 16 | RF | Resume (suppresses an instruction breakpoint for the next instruction) |
| 17 | VM | Virtual-8086 mode |

### 2.2 CR0

| Bit | Name | Meaning |
|---:|---|---|
| 0 | PE | Protected mode enable |
| 1 | MP | Monitor coprocessor (WAIT raises #NM when TS is also set) |
| 2 | EM | Emulate coprocessor (ESC instructions raise #NM) |
| 3 | TS | Task switched (the next ESC instruction raises #NM) |
| 4 | ET | Extension type (80387 vs 80287) |
| 31 | PG | Paging enable, valid only with PE set |

---

## 3. Operating Modes

| Mode | Entered by | Addressing |
|---|---|---|
| Real | Reset, or clearing CR0.PE | linear = segment × 16 + offset, 64 KB segments, IVT interrupts |
| Protected | Setting CR0.PE, then a far jump to load CS | Selectors index the GDT/LDT, privilege levels 0–3, IDT gates |
| Virtual-8086 | EFLAGS.VM through IRET or a task switch | Real-mode addressing at CPL 3 inside protected mode |

The debugger shows the mode as `real`, `protected` or `vm86`, with CPL (the privilege of CS) and IOPL.

---

## 4. Segmentation

### 4.1 Selectors

| Bits | Field |
|---|---|
| 15–3 | Descriptor index |
| 2 | TI: 0 = GDT, 1 = LDT |
| 1–0 | RPL, requested privilege level |

Selector 0000h–0003h is the null selector: DS, ES, FS and GS may hold it (an access through it raises #GP), CS and
SS may not.

### 4.2 Descriptors

Every descriptor is 8 bytes:

| Field | Meaning |
|---|---|
| Base (32 bits) | Linear address of offset 0 |
| Limit (20 bits) | Highest valid offset; with G=1 in 4 KB units: `(limit << 12) | FFFh` |
| P | Present; loading a non-present segment raises #NP (#SS for SS) |
| DPL | Descriptor privilege level |
| S | 1 = code/data, 0 = system |
| Type | Code: conforming, readable, accessed. Data: expand-down, writable, accessed |
| D/B | 1 = 32-bit default operand and address size (code), 32-bit stack pointer (stack) |
| G | Limit granularity |
| AVL | Available to software |

System descriptor types:

| Type | Descriptor |
|---:|---|
| 1 / 3 | 16-bit TSS, available / busy |
| 2 | LDT |
| 4 | 16-bit call gate |
| 5 | Task gate |
| 6 / 7 | 16-bit interrupt / trap gate |
| 9 / B | 32-bit TSS, available / busy |
| C | 32-bit call gate |
| E / F | 32-bit interrupt / trap gate |

0, 8, A and D are reserved. Loading a segment register copies the descriptor into the hidden cache; later changes
to the table do not affect the register until it is reloaded. `get_i386_status` shows the caches,
`get_i386_descriptors` the tables.

### 4.3 Logical Addresses in the Debugger

Tools accept `1234ABCD` (linear), `CS:1234` (segment register and offset, using its cached base) and
`0008:00001234` (a selector or real-mode segment; in protected mode the descriptor is read from the GDT or LDT).

---

## 5. Paging

With CR0.PG=1 every linear address goes through a two-level table:

```text
linear: bits 31-22 directory index, bits 21-12 table index, bits 11-0 offset
PDE = dword at (CR3 & FFFFF000h) + directory_index * 4
PTE = dword at (PDE & FFFFF000h) + table_index * 4
physical = (PTE & FFFFF000h) | offset
```

Entry bits: 0 P (present), 1 R/W, 2 U/S, 5 A (accessed), 6 D (dirty, PTE only), 11–9 available to software. User
code (CPL 3) can access a page only when both levels allow it; supervisor code (CPL 0–2) can read and write every
present page, including read-only ones, because the 80386 has no write-protect control. A fault raises #PF with the
linear address in CR2 and an error code (bit 0 protection vs not present, bit 1 write, bit 2 user). The 80386
caches translations in a 32-entry TLB that is flushed when CR3 is loaded.

`translate_address` and `get_page_directory` walk the tables without touching the TLB or the A/D bits.

---

## 6. Interrupts and Exceptions

### 6.1 Exceptions

| Vector | Name | Class | Error code |
|---:|---|---|---|
| 0 | #DE divide error | Fault | No |
| 1 | #DB debug | Fault or trap | No |
| 2 | NMI | Interrupt | No |
| 3 | #BP breakpoint (INT3) | Trap | No |
| 4 | #OF overflow (INTO) | Trap | No |
| 5 | #BR BOUND range | Fault | No |
| 6 | #UD invalid opcode | Fault | No |
| 7 | #NM coprocessor not available | Fault | No |
| 8 | #DF double fault | Abort | Yes (0) |
| 9 | Coprocessor segment overrun | Abort | No |
| 10 | #TS invalid TSS | Fault | Yes |
| 11 | #NP segment not present | Fault | Yes |
| 12 | #SS stack fault | Fault | Yes |
| 13 | #GP general protection | Fault | Yes |
| 14 | #PF page fault | Fault | Yes |
| 16 | #MF coprocessor error | Fault | No |

Vector 15 and 17–31 are reserved. Selector error codes: bits 15–3 index, bit 2 TI (LDT), bit 1 IDT, bit 0 EXT
(external event). Vectors 9 and 16 come from the 80387.

### 6.2 Real Mode Interrupts

In real mode the interrupt table holds 4-byte entries (offset, then segment) at the IDTR base, 0 after reset. An
interrupt pushes FLAGS, CS and IP and clears IF and TF.

### 6.3 Protected Mode IDT

8-byte gates: interrupt gates clear IF, trap gates do not, task gates switch tasks. Both clear TF and NT. A handler
in a non-conforming segment runs at the DPL of its code segment; a privilege change loads SS:ESP from the TSS and
pushes the old SS:ESP before EFLAGS, CS and EIP. Exceptions with an error code push it last. Software INT n requires
CPL <= gate DPL. Faults push the address of the faulting instruction, traps the next one. An interrupt or exception
in virtual-8086 mode goes through the IDT to a CPL 0 handler, which also receives the old GS, FS, DS and ES.

### 6.4 FM Towns Hardware Interrupts

Hardware IRQs reach the CPU through two 8259A PICs (see the system resource). The Model 1/2 BIOS programs the
master base to 40h and the slave to 48h:

| Vector | IRQ | Source |
|---:|---:|---|
| 40h | 0 | Timer |
| 41h | 1 | Keyboard |
| 42h | 2 | RS-232C |
| 43h | 3 | Expansion RS-232C |
| 44h–45h | 4–5 | I/O expansion |
| 46h | 6 | Floppy disk |
| 47h | 7 | Slave PIC (cascade, no device) |
| 48h | 8 | SCSI |
| 49h | 9 | CD-ROM |
| 4Ah | 10 | I/O expansion |
| 4Bh | 11 | VSYNC |
| 4Ch | 12 | Printer |
| 4Dh | 13 | Sound |
| 4Eh | 14 | I/O expansion |
| 4Fh | 15 | Reserved |

The PICs run without automatic EOI: a handler sends an EOI to the PIC (to the slave and to the master for
IRQ8–15) before that line, or one of lower priority, can interrupt again.

---

## 7. I/O Protection

In protected mode IN, OUT, INS, OUTS, CLI and STI require CPL <= IOPL. When CPL > IOPL, and always in
virtual-8086 mode, the TSS I/O permission bitmap decides each port access; a denied access raises #GP. In
virtual-8086 mode CLI, STI, PUSHF, POPF, INT n and IRET raise #GP unless IOPL is 3.

---

## 8. Tasks

A TSS holds a full register context, CR3 and the LDT selector. Task switches happen through JMP or CALL to a TSS
or task gate, interrupts through a task gate, and IRET with NT set. TR selects the current TSS; its descriptor is
marked busy.

---

## 9. Debug Registers

DR0–DR3 hold linear addresses; DR7 enables each one (L0–L3, G0–G3) and selects execute, write or read/write
access and a length of 1, 2 or 4 bytes. A hit raises #DB and sets the matching B0–B3 bit in DR6; DR6 also reports
single step (BS) and task switch (BT). The debugger's own breakpoints are separate and do not use these registers.
