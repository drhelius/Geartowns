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

#include <stdio.h>
#include <stdint.h>
#include "i386_dynarec_emit.h"

#if !defined(GT_DISABLE_DYNAREC)

extern "C"
{
#include <lightning.h>
}

// Guest registers and flags live in I386_State. Generated code keeps the CPU in V0 and uses five scratch registers,
// the floor of every lightning backend
#define DR_CPU JIT_V0
#define DR_T0 JIT_R0
#define DR_T1 JIT_R1
#define DR_T2 JIT_R2
#define DR_T3 JIT_V1
#define DR_T4 JIT_V2

static const int k_dynarec_max_instructions = 64;
static const u32 k_dynarec_max_bytes = 256;
static const u32 k_dynarec_max_clocks = 0xF000;
static const u32 k_dynarec_max_watch = 512;
static const int k_dynarec_frame = 64;

// Conditional branches reach only 4 KiB on RISC-V and 32 KiB on PowerPC, so each fallback follows its instruction
#if defined(__riscv) || defined(__powerpc__)
static const bool k_dynarec_near_fallbacks = true;
#else
static const bool k_dynarec_near_fallbacks = false;
#endif
static const u32 k_dynarec_compiled = 1;

static const u16 k_flags_all = I386_FLAG_CF | I386_FLAG_PF | I386_FLAG_AF | I386_FLAG_ZF | I386_FLAG_SF | I386_FLAG_OF;
static const u16 k_flags_logic = I386_FLAG_CF | I386_FLAG_PF | I386_FLAG_ZF | I386_FLAG_SF | I386_FLAG_OF;
static const u16 k_flags_inc_dec = I386_FLAG_PF | I386_FLAG_AF | I386_FLAG_ZF | I386_FLAG_SF | I386_FLAG_OF;
static const u16 k_flags_shift = I386_FLAG_CF | I386_FLAG_PF | I386_FLAG_ZF | I386_FLAG_SF | I386_FLAG_OF;

static const u8 k_alu_memory_clocks[8][4] =
{
    { 7, 7, 6, 6 }, { 6, 6, 7, 7 }, { 7, 7, 6, 6 }, { 6, 6, 7, 7 },
    { 6, 6, 7, 7 }, { 6, 6, 7, 7 }, { 6, 6, 7, 7 }, { 5, 5, 6, 6 }
};

enum Dynarec_Kind
{
    DYNAREC_NATIVE = 0,
    DYNAREC_HELPER,
    DYNAREC_HELPER_LAST
};

enum Dynarec_Access
{
    DYNAREC_READ = 0,
    DYNAREC_WRITE,
    DYNAREC_READ_WRITE
};

// Lightning keeps process-wide state, set up once for every core instance
void DynarecEmitter::InitLightning()
{
    static bool initialized = false;

    if (initialized)
        return;

    init_jit(NULL);
    initialized = true;
}

DynarecEmitter::DynarecEmitter(I386* cpu)
{
    m_cpu = cpu;
    InitPointer(_jit);
    InitPointer(m_block);
    InitPointer(m_block_start);
    m_count = 0;
    m_key = 0;
}

// Decodes the block at the code window and translates it into code
// Returns false when its first instruction must run in the interpreter
bool DynarecEmitter::Compile(I386::DynarecBlock& block, u32 available, u32 instruction_limit, u8* code, u32 capacity,
    u32& code_size)
{
    m_block = &block;
    m_key = (u8)block.key;

    if (!Decode(available, instruction_limit))
        return false;

    ComputeLiveness();

    _jit = jit_new_state();
    EmitBlock();
    block.eip_limit = MAX(block.eip_limit, block.watch_eip + block.watch_length - 1);
    jit_realize();
    jit_set_data(NULL, 0, JIT_DISABLE_DATA | JIT_DISABLE_NOTE);
    jit_set_code(code, capacity);

    bool emitted = IsValidPointer(jit_emit());
    jit_word_t size = 0;

    if (emitted)
        jit_get_code(&size);

    jit_clear_state();
    jit_destroy_state();
    InitPointer(_jit);

    code_size = (u32)size;
    return emitted;
}

int DynarecEmitter::GetInstructionCount() const
{
    return m_count;
}

u32 DynarecEmitter::GetInstructionEIP(int index) const
{
    return m_instructions[index].context.start_eip;
}

// The C callable entry of generated code. It saves the callee-saved registers once and jumps to a block, blocks jump
// to each other in its frame and come back to its exit, which adds the steps and clocks the last block passes in R1
// and R2 to the totals of the run and returns the exit flags in R0
bool DynarecEmitter::EmitEntry(u8* code, u32 capacity, u32& code_size, u32& exit_offset, u32& link_offset)
{
    _jit = jit_new_state();
    jit_prolog();
    jit_frame(k_dynarec_frame);
    jit_node_t* cpu = jit_arg();
    jit_node_t* target = jit_arg();
    jit_getarg(DR_CPU, cpu);
    jit_getarg(JIT_R0, target);
    jit_jmpr(JIT_R0);

    jit_node_t* exit = jit_indirect();
    jit_live(JIT_R0);
    jit_live(JIT_R1);
    jit_live(JIT_R2);
    LoadU32(DR_T3, DR_CPU, Offset(&m_cpu->m_dynarec_steps));
    jit_addr(DR_T3, DR_T3, JIT_R1);
    jit_stxi_i(Offset(&m_cpu->m_dynarec_steps), DR_CPU, DR_T3);
    LoadU32(DR_T3, DR_CPU, Offset(&m_cpu->m_dynarec_clocks));
    jit_addr(DR_T3, DR_T3, JIT_R2);
    jit_stxi_i(Offset(&m_cpu->m_dynarec_clocks), DR_CPU, DR_T3);
    jit_retr(JIT_R0);
    jit_epilog();

    jit_prolog();
    jit_tramp(k_dynarec_frame);
    jit_node_t* link = jit_indirect();
    EmitLink(exit);

    jit_realize();
    jit_set_data(NULL, 0, JIT_DISABLE_DATA | JIT_DISABLE_NOTE);
    jit_set_code(code, capacity);

    bool emitted = IsValidPointer(jit_emit());
    jit_word_t size = 0;

    if (emitted)
    {
        jit_get_code(&size);
        exit_offset = (u32)((u8*)jit_address(exit) - code);
        link_offset = (u32)((u8*)jit_address(link) - code);
    }

    jit_clear_state();
    jit_destroy_state();
    InitPointer(_jit);

    code_size = (u32)size;
    return emitted;
}

bool DynarecEmitter::Decode(u32 available, u32 instruction_limit)
{
    I386::DynarecBlock& block = *m_block;
    bool default32 = (m_key & k_dynarec_key_default32) != 0;
    u32 position = 0;
    u32 clocks = 0;
    int limit = MIN((int)instruction_limit, k_dynarec_max_instructions);

    m_count = 0;
    block.eip_limit = 0;
    block.call_eip = 0;
    block.call_next_eip = 0;

    while (m_count < limit && position < available && position < k_dynarec_max_bytes)
    {
        Instruction& instruction = m_instructions[m_count];
        u32 eip = block.eip + position;

        m_cpu->m_passive_bytes = block.host + position;
        m_cpu->m_passive_count = MIN(available - position, (u32)GT_I386_MAX_INSTRUCTION_LENGTH);
        m_cpu->m_passive_index = 0;

        if (!m_cpu->DecodeInstructionPassive(instruction.context, eip, default32))
            break;

        u32 length = instruction.context.next_eip - eip;

        if (position + length > k_dynarec_max_bytes)
            break;

        Classify(instruction);

        if (clocks + instruction.clocks > k_dynarec_max_clocks)
            break;

#if !defined(GT_DISABLE_DISASSEMBLER)
        m_cpu->DisassembleInstruction(eip);
#endif

        instruction.before = clocks;
        clocks += instruction.clocks;
        position += length;
        m_count++;

        if (instruction.ends_block)
            break;
    }

    m_block->watch_eip = block.eip;
    m_block->watch_length = (u16)position;
    return m_count != 0;
}

// Flags written by an instruction and overwritten before anything can read them are not computed
// Exits, helpers and instructions that can run in the interpreter see every flag
void DynarecEmitter::ComputeLiveness()
{
    u16 live = k_flags_all;

    for (int i = m_count - 1; i >= 0; i--)
    {
        Instruction& instruction = m_instructions[i];
        instruction.live = live;

        if (instruction.kind != DYNAREC_NATIVE || instruction.memory)
            live = k_flags_all;
        else
            live = (u16)((live & ~instruction.defs) | instruction.uses);
    }
}

// Natives are translated. Helpers run in the interpreter and the block goes on, which needs the instruction to
// leave CS, SS, IF, the mode and the debug state alone. Everything else runs in the interpreter as the last
// instruction of its block: transfers it can't follow, interrupts, system instructions, OUT, INS, OUTS and REP strings
void DynarecEmitter::Classify(Instruction& instruction)
{
    const I386::InstructionContext& c = instruction.context;
    u8 opcode = c.opcode;
    int width = c.operand_size * 8;
    bool memory = c.memory_operand;

    instruction.kind = DYNAREC_HELPER_LAST;
    instruction.memory = false;
    instruction.ends_block = true;
    instruction.uses = 0;
    instruction.defs = 0;
    instruction.clocks = 0;
    InitPointer(instruction.fallback);
    InitPointer(instruction.resume);

    if (c.lock)
        return;

    instruction.ends_block = false;

    if (c.two_byte)
    {
        u8 opcode2 = c.opcode2;

        if (opcode2 >= 0x80 && opcode2 <= 0x8F)
        {
            // Jcc rel16/32
            SetNative(instruction, 3, false);
            instruction.ends_block = true;
            instruction.uses = GetConditionFlags(opcode2 & 15);
        }
        else if (opcode2 >= 0x90 && opcode2 <= 0x9F)
        {
            // SETcc r/m8
            SetNative(instruction, memory ? 5 : 4, memory);
            instruction.uses = GetConditionFlags(opcode2 & 15);
        }
        else if (opcode2 == 0xB6 || opcode2 == 0xB7 || opcode2 == 0xBE || opcode2 == 0xBF)
            SetNative(instruction, memory ? 6 : 3, memory);
        else if (opcode2 == 0x02 || opcode2 == 0x03 || opcode2 == 0xA0 || opcode2 == 0xA8 || opcode2 == 0xA3 ||
            opcode2 == 0xAB || opcode2 == 0xB3 || opcode2 == 0xBB || opcode2 == 0xBA || opcode2 == 0xA4 ||
            opcode2 == 0xA5 || opcode2 == 0xAC || opcode2 == 0xAD || opcode2 == 0xAF || opcode2 == 0xBC ||
            opcode2 == 0xBD || opcode2 == 0xA1 || opcode2 == 0xA9 || opcode2 == 0xB4 || opcode2 == 0xB5)
            instruction.kind = DYNAREC_HELPER;
        else
            instruction.ends_block = true;

        return;
    }

    if (opcode < 0x40 && (opcode & 7) <= 5)
    {
        int operation = opcode >> 3;
        int form = opcode & 7;

        // 32-bit ADC and SBB take their carry from the bit above the width, which 32-bit hosts don't have
        if ((operation == 2 || operation == 3) && !CanCarry((form & 1) == 0 ? 8 : width))
        {
            instruction.kind = DYNAREC_HELPER;
            return;
        }

        SetNative(instruction, form <= 3 && memory ? k_alu_memory_clocks[operation][form] : 2, form <= 3 && memory);
        instruction.defs = operation == 1 || operation == 4 || operation == 6 ? k_flags_logic : k_flags_all;
        instruction.uses = operation == 2 || operation == 3 ? I386_FLAG_CF : 0;
        return;
    }

    switch (opcode)
    {
        case 0x07: case 0x1F: case 0xC4: case 0xC5:
            // POP ES, POP DS, LES and LDS
        case 0x06: case 0x0E: case 0x16: case 0x1E:
            // PUSH segment
        case 0x27: case 0x2F: case 0x37: case 0x3F:
            // DAA, DAS, AAA, AAS
        case 0x60: case 0x61: case 0x62: case 0x69: case 0x6B:
            // PUSHA, POPA, BOUND, IMUL
        case 0x8C: case 0x8F:
            // MOV r/m,Sreg and POP r/m
        case 0x9B: case 0x9C: case 0x9E: case 0x9F:
            // WAIT, PUSHF, SAHF, LAHF
        case 0xC8: case 0xC9:
            // ENTER, LEAVE
        case 0xD4: case 0xD5: case 0xD6: case 0xD7:
            // AAM, AAD, SALC, XLAT
        case 0xD2: case 0xD3:
            // Shifts by CL
            instruction.kind = DYNAREC_HELPER;
            break;
        case 0xD8: case 0xD9: case 0xDA: case 0xDB: case 0xDC: case 0xDD: case 0xDE: case 0xDF:
            // ESC
            instruction.kind = DYNAREC_HELPER;
            break;
        case 0x8E:
            // MOV Sreg,r/m for the data segments. SS changes the stack size and CS is invalid
            if (c.reg != I386_SEGMENT_SS && c.reg != I386_SEGMENT_CS && c.reg < I386_SEGMENT_COUNT)
                instruction.kind = DYNAREC_HELPER;
            else
                instruction.ends_block = true;

            break;
        case 0xA4: case 0xA5: case 0xA6: case 0xA7: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAE: case 0xAF:
            // String instructions, repeated ones continue in RunRepeatBatch
            if (c.repeat == 0)
                instruction.kind = DYNAREC_HELPER;
            else
                instruction.ends_block = true;

            break;
        case 0xE0: case 0xE1: case 0xE2: case 0xE3:
            // LOOPNE, LOOPE, LOOP and JCXZ
            SetNative(instruction, opcode == 0xE3 ? 5 : 11, false);
            instruction.ends_block = true;
            instruction.uses = opcode <= 0xE1 ? I386_FLAG_ZF : 0;
            break;
        case 0xE4: case 0xE5: case 0xEC: case 0xED:
            // IN, DynarecStep leaves the block when the read ends the batch
            instruction.kind = DYNAREC_HELPER;
            break;
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
        case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
            // INC/DEC r16/32
            SetNative(instruction, 2, false);
            instruction.defs = k_flags_inc_dec;
            break;
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
            // PUSH r16/32
        case 0x68: case 0x6A:
            // PUSH imm
            SetNative(instruction, 2, true);
            break;
        case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
            // POP r16/32
            SetNative(instruction, 4, true);
            break;
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
            // Jcc rel8
            SetNative(instruction, 3, false);
            instruction.ends_block = true;
            instruction.uses = GetConditionFlags(opcode & 15);
            break;
        case 0x80: case 0x81: case 0x82: case 0x83:
            // Group 1 r/m,imm
            if ((c.reg == 2 || c.reg == 3) && !CanCarry(opcode == 0x80 || opcode == 0x82 ? 8 : width))
                instruction.kind = DYNAREC_HELPER;
            else
            {
                SetNative(instruction, memory ? (c.reg == 7 ? 5 : 7) : 2, memory);
                instruction.defs = c.reg == 1 || c.reg == 4 || c.reg == 6 ? k_flags_logic : k_flags_all;
                instruction.uses = c.reg == 2 || c.reg == 3 ? I386_FLAG_CF : 0;
            }

            break;
        case 0x84: case 0x85:
            // TEST r/m,r
            SetNative(instruction, memory ? 5 : 2, memory);
            instruction.defs = k_flags_logic;
            break;
        case 0x86: case 0x87:
            // XCHG r/m,r
            SetNative(instruction, memory ? 5 : 3, memory);
            break;
        case 0x88: case 0x89:
            // MOV r/m,r
            SetNative(instruction, 2, memory);
            break;
        case 0x8A: case 0x8B:
            // MOV r,r/m
            SetNative(instruction, memory ? 4 : 2, memory);
            break;
        case 0x8D:
            // LEA, the register form is invalid
            if (memory)
                SetNative(instruction, 2, false);
            else
                instruction.kind = DYNAREC_HELPER;

            break;
        case 0x90:
            // NOP
            SetNative(instruction, 3, false);
            break;
        case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
            // XCHG eAX,r
            SetNative(instruction, 3, false);
            break;
        case 0x98:
            // CBW/CWDE
            SetNative(instruction, 3, false);
            break;
        case 0x99:
            // CWD/CDQ
            SetNative(instruction, 2, false);
            break;
        case 0xA0: case 0xA1:
            // MOV accumulator,moffs
            SetNative(instruction, 4, true);
            break;
        case 0xA2: case 0xA3:
            // MOV moffs,accumulator
            SetNative(instruction, 2, true);
            break;
        case 0xA8: case 0xA9:
            // TEST accumulator,imm
            SetNative(instruction, 2, false);
            instruction.defs = k_flags_logic;
            break;
        case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7:
        case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF:
            // MOV r,imm
            SetNative(instruction, 2, false);
            break;
        case 0xC0: case 0xC1: case 0xD0: case 0xD1:
        {
            // Rotates and shifts by a constant, the rotates through carry run in the interpreter
            int shift_width = opcode == 0xC0 || opcode == 0xD0 ? 8 : width;
            u32 count = opcode >= 0xD0 ? 1 : (c.immediate & 0x1F);

            if (count != 0 && (c.reg <= 1 || (c.reg >= 4 && count <= (u32)shift_width)))
            {
                SetNative(instruction, memory ? 7 : 3, memory);
                instruction.defs = c.reg <= 1 ? (u16)(I386_FLAG_CF | I386_FLAG_OF) : k_flags_shift;
            }
            else
                instruction.kind = DYNAREC_HELPER;

            break;
        }
        case 0xC2: case 0xC3:
            // RET near
            SetNative(instruction, 10, true);
            instruction.ends_block = true;
            break;
        case 0xC6: case 0xC7:
            // MOV r/m,imm
            if (c.reg == 0)
                SetNative(instruction, 2, memory);
            else
                instruction.kind = DYNAREC_HELPER;

            break;
        case 0xE8:
            // CALL near
            SetNative(instruction, 7, true);
            instruction.ends_block = true;
            break;
        case 0xE9: case 0xEB:
            // JMP near and short
            SetNative(instruction, 7, false);
            instruction.ends_block = true;
            break;
        case 0xF5:
            // CMC
            SetNative(instruction, 2, false);
            instruction.uses = I386_FLAG_CF;
            instruction.defs = I386_FLAG_CF;
            break;
        case 0xF6: case 0xF7:
            // Group 3: TEST, NOT and NEG, the multiplies and divides run in the interpreter
            if (c.reg <= 3)
            {
                static const u8 k_memory_extra[4] = { 3, 3, 4, 4 };
                SetNative(instruction, 2 + (memory ? k_memory_extra[c.reg] : 0), memory);
                instruction.defs = c.reg <= 1 ? k_flags_logic : c.reg == 3 ? k_flags_all : 0;
            }
            else
                instruction.kind = DYNAREC_HELPER;

            break;
        case 0xF8: case 0xF9:
            // CLC, STC
            SetNative(instruction, 2, false);
            instruction.defs = I386_FLAG_CF;
            break;
        case 0xFC: case 0xFD:
            // CLD, STD
            SetNative(instruction, 2, false);
            break;
        case 0xFE: case 0xFF:
            // Group 4 and 5: INC and DEC natively, near indirect transfers and PUSH in the interpreter
            if (c.reg <= 1)
            {
                SetNative(instruction, memory ? 6 : 2, memory);
                instruction.defs = k_flags_inc_dec;
            }
            else if (opcode == 0xFF && (c.reg == 2 || c.reg == 4))
            {
                instruction.kind = DYNAREC_HELPER_LAST;
                instruction.ends_block = true;
            }
            else if (opcode == 0xFF && c.reg == 6)
                instruction.kind = DYNAREC_HELPER;
            else
                instruction.ends_block = true;

            break;
        default:
            instruction.ends_block = true;
            break;
    }
}

bool DynarecEmitter::CanCarry(int width) const
{
#if __WORDSIZE == 64
    UNUSED(width);
    return true;
#else
    return width < 32;
#endif
}

void DynarecEmitter::SetNative(Instruction& instruction, u32 clocks, bool memory)
{
    instruction.kind = DYNAREC_NATIVE;
    instruction.memory = memory;
    instruction.clocks = clocks + (instruction.context.memory_operand ? GetAddressClocks(instruction.context) : 0);
}

// The base and index clock of the 386 effective address unit, as DecodeOperands counts it
u32 DynarecEmitter::GetAddressClocks(const I386::InstructionContext& c) const
{
    u8 mod = c.modrm >> 6;
    u8 rm = c.modrm & 7;

    if (c.address_size == 2)
        return rm <= 3 ? 1 : 0;

    if (rm == 4)
        return ((c.sib >> 3) & 7) != 4 && (mod != 0 || (c.sib & 7) != 5) ? 1 : 0;

    return 0;
}

// The m clocks of a transfer whose target instruction lies in the code window, counted from the same bytes
// GetNextInstructionComponents reads there. The block watches those bytes too and needs them inside the CS limit
bool DynarecEmitter::GetTargetComponents(u32 target, u32& components)
{
    u32 offset = target - m_cpu->m_code_window_eip;

    if (offset >= m_cpu->m_code_window_size)
        return false;

    u32 count = MIN(m_cpu->m_code_window_size - offset, (u32)GT_I386_MAX_INSTRUCTION_LENGTH);
    bool truncated = true;
    u32 counted = m_cpu->CountTargetComponents(m_cpu->m_code_window + offset, count, truncated);

    u32 start = MIN(m_block->watch_eip, target);
    u32 end = MAX(m_block->watch_eip + m_block->watch_length, target + count);

    if (truncated || end - start > k_dynarec_max_watch)
        return false;

    components = counted;
    m_block->watch_eip = start;
    m_block->watch_length = (u16)(end - start);
    m_block->eip_limit = MAX(m_block->eip_limit, target + count - 1);
    return true;
}

u16 DynarecEmitter::GetConditionFlags(int condition) const
{
    static const u16 k_condition_flags[8] =
    {
        I386_FLAG_OF, I386_FLAG_CF, I386_FLAG_ZF, I386_FLAG_CF | I386_FLAG_ZF,
        I386_FLAG_SF, I386_FLAG_PF, I386_FLAG_SF | I386_FLAG_OF, I386_FLAG_SF | I386_FLAG_OF | I386_FLAG_ZF
    };

    return k_condition_flags[(condition >> 1) & 7];
}

// Checks run before anything is written. A native instruction whose checks fail runs in the interpreter
// instead, from its start: those fallbacks and the helpers are where the slice budget is checked again
void DynarecEmitter::EmitBlock()
{
    int last = m_count - 1;
    int next_check = last;

    for (int i = last; i >= 0; i--)
    {
        Instruction& instruction = m_instructions[i];
        instruction.next_check = i == last ? 0 : m_instructions[next_check].before;

        if (instruction.kind != DYNAREC_NATIVE)
            next_check = i;
    }

    m_block->fit_clocks = m_instructions[next_check].before;
    m_block->link[0] = 0xFFFFFFFFU;
    m_block->link[1] = 0xFFFFFFFFU;

    // Blocks run in the frame of the entry function, with the CPU in V0
    jit_prolog();
    jit_tramp(k_dynarec_frame);
    m_block_start = jit_label();

    for (int i = 0; i < m_count; i++)
    {
        Instruction& instruction = m_instructions[i];

        if (instruction.kind == DYNAREC_NATIVE)
        {
            if (instruction.memory)
            {
                instruction.fallback = jit_forward();
                instruction.resume = jit_forward();
            }

            EmitNative(instruction, i);

            if (instruction.memory && k_dynarec_near_fallbacks)
            {
                if (!instruction.ends_block)
                    jit_patch_at(jit_jmpi(), instruction.resume);

                EmitFallback(instruction, i, last);
            }

            if (instruction.memory)
                jit_link(instruction.resume);
        }
        else
        {
            StoreEIP(instruction.context.start_eip);
            CallStep(instruction.next_check, 0, instruction.before);

            if (instruction.kind == DYNAREC_HELPER_LAST)
                Exit(i + 1, instruction.before, k_dynarec_exit_end);
            else
            {
                jit_node_t* next = jit_beqi(JIT_R0, 0);
                Exit(i + 1, instruction.before, 0);
                jit_patch(next);
            }
        }
    }

    const Instruction& end = m_instructions[last];

    if (!end.ends_block)
    {
        StoreEIP(end.context.next_eip);
        ExitLinked(m_count, end.before + end.clocks, k_dynarec_exit_end, 0);
    }

    for (int i = 0; i < m_count; i++)
    {
        Instruction& instruction = m_instructions[i];

        if (IsValidPointer(instruction.fallback) && !k_dynarec_near_fallbacks)
            EmitFallback(instruction, i, last);
    }
}

// The interpreter runs the instruction from its start when one of its checks fails, then the block goes on after it
// or leaves
void DynarecEmitter::EmitFallback(Instruction& instruction, int index, int last)
{
    jit_link(instruction.fallback);
    StoreEIP(instruction.context.start_eip);
    CallStep(instruction.next_check, instruction.clocks, instruction.before);

    if (instruction.ends_block)
    {
        Exit(index + 1, instruction.before + instruction.clocks, k_dynarec_exit_end);
        return;
    }

    jit_node_t* stop = jit_bnei(JIT_R0, 0);
    jit_patch_at(jit_jmpi(), instruction.resume);
    jit_patch(stop);
    Exit(index + 1, instruction.before + instruction.clocks, index == last ? k_dynarec_exit_end : 0);
}

// Leaves for the dispatcher through the exit of the entry function, with the steps and clocks of the block up to here
void DynarecEmitter::Exit(int steps, u32 clocks, u32 flags)
{
    jit_movi(JIT_R0, (jit_word_t)flags);
    jit_movi(JIT_R1, (jit_word_t)steps);
    jit_movi(JIT_R2, (jit_word_t)clocks);
    jit_movi(DR_T3, (jit_word_t)(uintptr_t)m_cpu->m_dynarec_exit);
    jit_jmpr(DR_T3);
}

// A static exit to the block at the EIP just stored, through the link code shared by every block
void DynarecEmitter::ExitLinked(int steps, u32 clocks, u32 flags, int slot)
{
    jit_movi(JIT_R0, (jit_word_t)(flags | k_dynarec_exit_link | (slot != 0 ? k_dynarec_exit_slot : 0)));
    jit_movi(JIT_R1, (jit_word_t)steps);
    jit_movi(JIT_R2, (jit_word_t)clocks);
    jit_movi(DR_T3, (jit_word_t)(uintptr_t)&m_block->link[slot]);
    jit_movi(DR_T4, (jit_word_t)(uintptr_t)m_cpu->m_dynarec_link);
    jit_jmpr(DR_T4);
}

// A jump to its own block start loops while the budget fits, writes to its code leave through a helper first
void DynarecEmitter::ExitTo(u32 target, int steps, u32 clocks, int slot)
{
    if (target != m_block->eip)
    {
        ExitLinked(steps, clocks, k_dynarec_exit_end, slot);
        return;
    }

    intptr_t step_total = Offset(&m_cpu->m_dynarec_steps);
    intptr_t clock_total = Offset(&m_cpu->m_dynarec_clocks);

    LoadU32(DR_T0, DR_CPU, step_total);
    jit_addi(DR_T0, DR_T0, (jit_word_t)steps);
    jit_stxi_i(step_total, DR_CPU, DR_T0);
    LoadU32(DR_T0, DR_CPU, clock_total);
    jit_addi(DR_T0, DR_T0, (jit_word_t)clocks);
    jit_stxi_i(clock_total, DR_CPU, DR_T0);
    jit_addi(DR_T0, DR_T0, (jit_word_t)m_block->fit_clocks);
    LoadU32(DR_T1, DR_CPU, Offset(&m_cpu->m_dynarec_remaining));
    jit_node_t* full = jit_bger_u(DR_T0, DR_T1);
    jit_patch_at(jit_jmpi(), m_block_start);
    jit_patch(full);
    Exit(0, 0, k_dynarec_exit_end | k_dynarec_exit_link | (slot != 0 ? k_dynarec_exit_slot : 0));
}

// Takes the exit flags in R0, the steps and clocks of the block in R1 and R2 and its link slot in T3. Once the
// dispatcher links the slot, the code goes straight into that block while it is valid and the slice budget covers it,
// as the dispatcher would check it
void DynarecEmitter::EmitLink(jit_node* exit)
{
    jit_live(JIT_R0);
    jit_live(JIT_R1);
    jit_live(JIT_R2);
    jit_live(DR_T3);
    intptr_t step_total = Offset(&m_cpu->m_dynarec_steps);
    intptr_t clock_total = Offset(&m_cpu->m_dynarec_clocks);

    LoadU32(DR_T4, DR_CPU, step_total);
    jit_addr(DR_T4, DR_T4, JIT_R1);
    jit_stxi_i(step_total, DR_CPU, DR_T4);
    LoadU32(JIT_R1, DR_CPU, clock_total);
    jit_addr(JIT_R1, JIT_R1, JIT_R2);
    jit_stxi_i(clock_total, DR_CPU, JIT_R1);

    LoadU32(DR_T4, DR_T3, 0);
    jit_node_t* unlinked = jit_beqi(DR_T4, (jit_word_t)0xFFFFFFFFU);
    jit_muli(JIT_R2, DR_T4, (jit_word_t)sizeof(I386::DynarecBlock));
    jit_movi(DR_T3, (jit_word_t)(uintptr_t)m_cpu->m_dynarec_blocks);
    jit_addr(JIT_R2, JIT_R2, DR_T3);
    jit_ldxi_uc(DR_T3, JIT_R2, (jit_word_t)offsetof(I386::DynarecBlock, state));
    jit_node_t* dead = jit_bnei(DR_T3, (jit_word_t)k_dynarec_compiled);
    LoadU32(DR_T3, JIT_R2, (jit_word_t)offsetof(I386::DynarecBlock, epoch));
    LoadU32(JIT_R1, DR_CPU, Offset(&m_cpu->m_dynarec_epoch));
    jit_node_t* stale = jit_bner(DR_T3, JIT_R1);
    LoadU32(DR_T3, JIT_R2, (jit_word_t)offsetof(I386::DynarecBlock, fit_clocks));
    LoadU32(JIT_R1, DR_CPU, clock_total);
    jit_addr(DR_T3, DR_T3, JIT_R1);
    LoadU32(JIT_R1, DR_CPU, Offset(&m_cpu->m_dynarec_remaining));
    jit_node_t* full = jit_bger_u(DR_T3, JIT_R1);
    jit_stxi_i(Offset(&m_cpu->m_dynarec_current), DR_CPU, DR_T4);
    jit_ldxi(JIT_R2, JIT_R2, (jit_word_t)offsetof(I386::DynarecBlock, code));
    jit_jmpr(JIT_R2);

    jit_patch(unlinked);
    jit_patch(dead);
    jit_patch(stale);
    jit_patch(full);
    jit_movi(JIT_R1, 0);
    jit_movi(JIT_R2, 0);
    jit_patch_at(jit_jmpi(), exit);
}

void DynarecEmitter::StoreEIP(u32 eip)
{
    jit_movi(DR_T0, (jit_word_t)eip);
    jit_stxi_i(Offset(&m_cpu->m_state.eip), DR_CPU, DR_T0);
}

// Helpers are reached through a register, so the code runs from any executable view
void DynarecEmitter::CallStep(u32 next_check, u32 native_clocks, u32 before)
{
    jit_movi(DR_T3, (jit_word_t)(uintptr_t)&I386::DynarecStep);
    jit_prepare();
    jit_pushargr(DR_CPU);
    jit_pushargi((jit_word_t)next_check);
    jit_pushargi((jit_word_t)native_clocks);
    jit_pushargi((jit_word_t)before);
    jit_finishr(DR_T3);
    jit_retval(JIT_R0);
}

intptr_t DynarecEmitter::Offset(const void* member) const
{
    return (intptr_t)((const u8*)member - (const u8*)m_cpu);
}

void DynarecEmitter::LoadU32(int r, int base, intptr_t offset)
{
#if __WORDSIZE == 64
    jit_ldxi_ui(r, base, offset);
#else
    jit_ldxi_i(r, base, offset);
#endif
}

void DynarecEmitter::Mask(int r, int width)
{
    if (width == 8)
        jit_andi(r, r, 0xFF);
    else if (width == 16)
        jit_andi(r, r, 0xFFFF);
#if __WORDSIZE == 64
    else
        jit_extr_ui(r, r);
#endif
}

intptr_t DynarecEmitter::RegisterOffset(int index, int width) const
{
    I386_State& state = m_cpu->m_state;

    if (width == 32)
        return Offset(&state.registers[index].value);

    if (width == 16)
        return Offset(&state.registers[index].low);

    return index < 4 ? Offset(&state.registers[index].byte0) : Offset(&state.registers[index & 3].byte1);
}

void DynarecEmitter::LoadRegister(int r, int index, int width)
{
    intptr_t offset = RegisterOffset(index, width);

    if (width == 32)
        LoadU32(r, DR_CPU, offset);
    else if (width == 16)
        jit_ldxi_us(r, DR_CPU, offset);
    else
        jit_ldxi_uc(r, DR_CPU, offset);
}

void DynarecEmitter::StoreRegister(int index, int width, int r)
{
    intptr_t offset = RegisterOffset(index, width);

    if (width == 32)
        jit_stxi_i(offset, DR_CPU, r);
    else if (width == 16)
        jit_stxi_s(offset, DR_CPU, r);
    else
        jit_stxi_c(offset, DR_CPU, r);
}

// Guest memory is little-endian on every host. Stores write the low bits of r, and scratch holds the swapped value
// on big-endian hosts
void DynarecEmitter::LoadHost(int r, int host, int width)
{
    if (width == 8)
    {
        jit_ldr_uc(r, host);
        return;
    }

#if defined(GT_DYNAREC_UNALIGNED)
    jit_unldr_u(r, host, width / 8);
#else
    if (width == 16)
        jit_ldr_us(r, host);
    else
#if __WORDSIZE == 64
        jit_ldr_ui(r, host);
#else
        jit_ldr_i(r, host);
#endif
#endif

#if defined(GT_BIG_ENDIAN)
    if (width == 16)
        jit_bswapr_us(r, r);
    else
        jit_bswapr_ui(r, r);
#endif
}

void DynarecEmitter::StoreHost(int host, int r, int width, int scratch)
{
    if (width == 8)
    {
        jit_str_c(host, r);
        return;
    }

    int value = r;

#if defined(GT_BIG_ENDIAN)
    if (width == 16)
        jit_bswapr_us(scratch, r);
    else
        jit_bswapr_ui(scratch, r);

    value = scratch;
#else
    UNUSED(scratch);
#endif

#if defined(GT_DYNAREC_UNALIGNED)
    jit_unstr(host, value, width / 8);
#else
    if (width == 16)
        jit_str_s(host, value);
    else
        jit_str_i(host, value);
#endif
}

// Effective offset from ModR/M, SIB and displacement, as CalculateEffectiveOffset computes it
u8 DynarecEmitter::EmitEffectiveOffset(int r, int t, const I386::InstructionContext& c)
{
    u8 mod = c.modrm >> 6;
    u8 rm = c.modrm & 7;
    bool stack = false;
    bool initialized = false;

    if (c.address_size == 2)
    {
        static const s8 k_base[8] = { I386_REG_EBX, I386_REG_EBX, I386_REG_EBP, I386_REG_EBP, -1, -1, I386_REG_EBP,
            I386_REG_EBX };
        static const s8 k_index[8] = { I386_REG_ESI, I386_REG_EDI, I386_REG_ESI, I386_REG_EDI, I386_REG_ESI,
            I386_REG_EDI, -1, -1 };

        int base = rm == 6 && mod == 0 ? -1 : k_base[rm];
        int index = k_index[rm];

        if (base >= 0)
        {
            LoadRegister(r, base, 16);
            initialized = true;
            stack = base == I386_REG_EBP;
        }

        if (index >= 0)
        {
            LoadRegister(initialized ? t : r, index, 16);

            if (initialized)
                jit_addr(r, r, t);

            initialized = true;
        }
    }
    else
    {
        if (rm == 4)
        {
            u8 base = c.sib & 7;
            u8 index = (c.sib >> 3) & 7;
            u8 scale = c.sib >> 6;
            bool has_base = mod != 0 || base != 5;

            stack = has_base && (base == I386_REG_EBP || base == I386_REG_ESP);

            if (has_base)
            {
                LoadRegister(r, base, 32);
                initialized = true;

                // Without an index register a non-zero scale applies to the base
                if (index == 4 && scale != 0)
                    jit_lshi(r, r, scale);
            }

            if (index != 4)
            {
                int index_register = initialized ? t : r;
                LoadRegister(index_register, index, 32);

                if (scale != 0)
                    jit_lshi(index_register, index_register, scale);

                if (initialized)
                    jit_addr(r, r, t);

                initialized = true;
            }
        }
        else if (mod != 0 || rm != 5)
        {
            LoadRegister(r, rm, 32);
            initialized = true;
            stack = rm == I386_REG_EBP;
        }
    }

    if (!initialized)
        jit_movi(r, (jit_word_t)(u32)c.displacement);
    else if (c.displacement != 0)
        jit_addi(r, r, (jit_word_t)c.displacement);

    Mask(r, c.address_size * 8);

    if (c.segment_override != 0xFF)
        return c.segment_override;

    return stack ? I386_SEGMENT_SS : I386_SEGMENT_DS;
}

// The fast path of ReadMemory, WriteMemory or GetRMWHost for size bytes at segment:offset
// r holds the offset on entry and the host address on exit, t1 to t3 are scratch
void DynarecEmitter::EmitHostAddress(int r, int t1, int t2, int t3, u8 segment, u32 size, int access,
    jit_node* fallback)
{
    intptr_t read_limit = Offset(&m_cpu->m_read_limits[segment]);
    intptr_t write_limit = Offset(&m_cpu->m_write_limits[segment]);

#if __WORDSIZE == 64
    jit_addi(t1, r, (jit_word_t)(size - 1));

    if (access != DYNAREC_WRITE)
    {
        jit_ldxi_l(t2, DR_CPU, read_limit);
        jit_patch_at(jit_bgtr(t1, t2), fallback);
    }

    if (access != DYNAREC_READ)
    {
        jit_ldxi_l(t2, DR_CPU, write_limit);
        jit_patch_at(jit_bgtr(t1, t2), fallback);
    }
#else
#if defined(GT_BIG_ENDIAN)
    const intptr_t low = 4;
    const intptr_t high = 0;
#else
    const intptr_t low = 0;
    const intptr_t high = 4;
#endif
    // A limit above 32 bits is -1, the checked path. The access end must not carry out of 32 bits
    jit_movr(t1, r);
    jit_patch_at(jit_boaddi_u(t1, (jit_word_t)(size - 1)), fallback);

    if (access != DYNAREC_WRITE)
    {
        jit_ldxi_i(t2, DR_CPU, read_limit + high);
        jit_patch_at(jit_bnei(t2, 0), fallback);
        jit_ldxi_i(t2, DR_CPU, read_limit + low);
        jit_patch_at(jit_bgtr_u(t1, t2), fallback);
    }

    if (access != DYNAREC_READ)
    {
        jit_ldxi_i(t2, DR_CPU, write_limit + high);
        jit_patch_at(jit_bnei(t2, 0), fallback);
        jit_ldxi_i(t2, DR_CPU, write_limit + low);
        jit_patch_at(jit_bgtr_u(t1, t2), fallback);
    }
#endif

    LoadU32(t1, DR_CPU, Offset(&m_cpu->m_state.segments[segment].base));
    jit_addr(t1, t1, r);
    Mask(t1, 32);

    if (size > 1)
    {
        jit_andi(t2, t1, 0xFFF);
        jit_patch_at(jit_bgti(t2, (jit_word_t)(0x1000 - size)), fallback);
    }

    if ((m_key & k_dynarec_key_paging) == 0)
    {
        jit_rshi_u(t2, t1, 12);
        jit_lshi(t2, t2, sizeof(void*) == 8 ? 3 : 2);

        if (access == DYNAREC_READ_WRITE)
        {
            jit_ldxi(t3, DR_CPU, Offset(&m_cpu->m_read_pages));
            jit_ldxr(t3, t3, t2);
            jit_ldxi(r, DR_CPU, Offset(&m_cpu->m_write_pages));
            jit_ldxr(r, r, t2);
            jit_patch_at(jit_bner(r, t3), fallback);
        }
        else
        {
            jit_ldxi(t3, DR_CPU, Offset(access == DYNAREC_READ ? (const void*)&m_cpu->m_read_pages :
                (const void*)&m_cpu->m_write_pages));
            jit_ldxr(t3, t3, t2);
        }
    }
    else
    {
        // The most recently used way of the set, as GetRecentTLBEntry
        intptr_t tlb = Offset(&m_cpu->m_tlb[0]);

        jit_rshi_u(t2, t1, 12);
        jit_andi(t2, t2, I386_TLB_SETS - 1);
        jit_addr(t3, DR_CPU, t2);
        jit_ldxi_uc(t3, t3, Offset(&m_cpu->m_tlb_mru[0]));
        jit_lshi(t2, t2, 2);
        jit_addr(t2, t2, t3);
        jit_muli(t2, t2, (jit_word_t)sizeof(I386::TLBEntry));
        jit_addr(t2, t2, DR_CPU);
        LoadU32(t3, t2, tlb + (jit_word_t)offsetof(I386::TLBEntry, linear_page));
        jit_andi(r, t1, 0xFFFFF000);
        jit_patch_at(jit_bner(t3, r), fallback);

        if (access == DYNAREC_READ_WRITE)
        {
            jit_ldxi(t3, t2, tlb + (jit_word_t)offsetof(I386::TLBEntry, read_page));
            jit_ldxi(r, t2, tlb + (jit_word_t)offsetof(I386::TLBEntry, write_page));
            jit_patch_at(jit_bner(r, t3), fallback);
        }
        else
            jit_ldxi(t3, t2, tlb + (jit_word_t)(access == DYNAREC_READ ? offsetof(I386::TLBEntry, read_page) :
                offsetof(I386::TLBEntry, write_page)));
    }

    jit_patch_at(jit_beqi(t3, 0), fallback);
    jit_andi(r, t1, 0xFFF);
    jit_addr(r, r, t3);
}

// SZP of a masked result ORed into f, for the flags in needed. t and u are scratch
void DynarecEmitter::EmitSZP(int f, int t, int u, int result, int width, u16 needed)
{
    u16 szp = needed & (I386_FLAG_PF | I386_FLAG_ZF | I386_FLAG_SF);

    if ((szp & I386_FLAG_PF) != 0)
    {
        jit_movi(u, (jit_word_t)(uintptr_t)I386::k_szp_flags);

        if (width == 8)
        {
            jit_ldxr_uc(t, u, result);

            if (szp != (I386_FLAG_PF | I386_FLAG_ZF | I386_FLAG_SF))
                jit_andi(t, t, szp);

            jit_orr(f, f, t);
            return;
        }

        jit_andi(t, result, 0xFF);
        jit_ldxr_uc(t, u, t);
        jit_andi(t, t, I386_FLAG_PF);
        jit_orr(f, f, t);
    }

    if ((szp & I386_FLAG_ZF) != 0)
    {
        jit_eqi(t, result, 0);
        jit_lshi(t, t, 6);
        jit_orr(f, f, t);
    }

    if ((szp & I386_FLAG_SF) != 0)
    {
        jit_rshi_u(t, result, width - 1);
        jit_lshi(t, t, 7);
        jit_orr(f, f, t);
    }
}

// Add and Sub flags in needed, from masked operands and result, merged into EFLAGS. left is clobbered
// The carry comes from comparing the operands, or is already in f for ADC and SBB
void DynarecEmitter::EmitArithmeticFlags(bool subtract, int result, int left, int right, int f, int t, int width,
    u16 needed, bool carry_in_f)
{
    if ((needed & I386_FLAG_CF) == 0)
        jit_movi(f, 0);
    else if (!carry_in_f)
    {
        if (subtract)
            jit_ltr_u(f, left, right);
        else
            jit_ltr_u(f, result, left);
    }

    if ((needed & I386_FLAG_AF) != 0)
    {
        jit_xorr(t, left, right);
        jit_xorr(t, t, result);
        jit_andi(t, t, I386_FLAG_AF);
        jit_orr(f, f, t);
    }

    if ((needed & I386_FLAG_OF) != 0)
    {
        if (subtract)
        {
            jit_xorr(t, left, right);
            jit_xorr(left, left, result);
        }
        else
        {
            jit_xorr(t, left, result);
            jit_xorr(left, right, result);
        }

        jit_andr(t, t, left);
        jit_rshi_u(t, t, width - 1);
        jit_andi(t, t, 1);
        jit_lshi(t, t, 11);
        jit_orr(f, f, t);
    }

    EmitSZP(f, t, left, result, width, needed);
    MergeFlags(f, t, needed);
}

void DynarecEmitter::MergeFlags(int f, int t, u32 mask)
{
    intptr_t eflags = Offset(&m_cpu->m_state.eflags);

    LoadU32(t, DR_CPU, eflags);
    jit_andi(t, t, (jit_word_t)(u32)~mask);
    jit_orr(t, t, f);
    jit_stxi_i(eflags, DR_CPU, t);
}

// 0 or 1 in r for a Jcc or SETcc condition, as CheckCondition evaluates it
void DynarecEmitter::EmitCondition(int r, int t, int condition)
{
    LoadU32(r, DR_CPU, Offset(&m_cpu->m_state.eflags));

    switch (condition >> 1)
    {
        case 0:
            jit_rshi_u(r, r, 11);
            break;
        case 1:
            break;
        case 2:
            jit_rshi_u(r, r, 6);
            break;
        case 3:
            jit_andi(r, r, I386_FLAG_CF | I386_FLAG_ZF);
            jit_nei(r, r, 0);
            break;
        case 4:
            jit_rshi_u(r, r, 7);
            break;
        case 5:
            jit_rshi_u(r, r, 2);
            break;
        case 6:
            jit_rshi_u(t, r, 4);
            jit_xorr(r, r, t);
            jit_rshi_u(r, r, 7);
            break;
        default:
            jit_rshi_u(t, r, 4);
            jit_xorr(t, t, r);
            jit_andi(t, t, I386_FLAG_SF);
            jit_andi(r, r, I386_FLAG_ZF);
            jit_orr(r, r, t);
            jit_nei(r, r, 0);
            break;
    }

    jit_andi(r, r, 1);

    if ((condition & 1) != 0)
        jit_xori(r, r, 1);
}

void DynarecEmitter::EmitNative(Instruction& instruction, int index)
{
    const I386::InstructionContext& c = instruction.context;
    u8 opcode = c.opcode;
    int width = c.operand_size * 8;

    if (c.two_byte)
    {
        u8 opcode2 = c.opcode2;

        if (opcode2 >= 0x80 && opcode2 <= 0x8F)
            EmitJcc(instruction, index, opcode2 & 15, SignExtend(c.immediate, width));
        else if (opcode2 >= 0x90 && opcode2 <= 0x9F)
            EmitSETcc(instruction);
        else
            EmitMOVX(instruction, opcode2 == 0xB6 || opcode2 == 0xBE ? 8 : 16, opcode2 >= 0xBE);

        return;
    }

    if (opcode < 0x40)
    {
        int form = opcode & 7;
        int alu_width = (form & 1) == 0 ? 8 : width;

        if (form >= 4)
            EmitALU(instruction, opcode >> 3, form, alu_width, c.immediate);
        else
            EmitALU(instruction, opcode >> 3, form, alu_width, 0);

        return;
    }

    switch (opcode)
    {
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
        case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
            EmitIncDecRegister(instruction, opcode & 7, width, opcode >= 0x48);
            break;
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
            LoadRegister(DR_T4, opcode & 7, width);
            EmitPush(instruction, width);
            break;
        case 0x68:
            jit_movi(DR_T4, (jit_word_t)c.immediate);
            EmitPush(instruction, width);
            break;
        case 0x6A:
            jit_movi(DR_T4, (jit_word_t)((u32)(s32)(s8)c.immediate & (width == 16 ? 0xFFFFU : 0xFFFFFFFFU)));
            EmitPush(instruction, width);
            break;
        case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
            EmitPop(instruction, opcode & 7, width);
            break;
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
            EmitJcc(instruction, index, opcode & 15, SignExtend(c.immediate, 8));
            break;
        case 0x80: case 0x82:
            EmitALU(instruction, c.reg, 0, 8, c.immediate & 0xFF);
            break;
        case 0x81:
            EmitALU(instruction, c.reg, 1, width, c.immediate);
            break;
        case 0x83:
            EmitALU(instruction, c.reg, 1, width, (u32)SignExtend(c.immediate, 8));
            break;
        case 0x84: case 0x85:
            EmitTest(instruction, opcode == 0x84 ? 8 : width);
            break;
        case 0x86: case 0x87:
            EmitExchange(instruction, opcode == 0x86 ? 8 : width);
            break;
        case 0x88: case 0x89: case 0x8A: case 0x8B:
            EmitMove(instruction, (opcode & 1) == 0 ? 8 : width, opcode >= 0x8A);
            break;
        case 0x8D:
            EmitLEA(instruction, width);
            break;
        case 0x90:
            break;
        case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
            LoadRegister(DR_T0, opcode & 7, width);
            LoadRegister(DR_T1, I386_REG_EAX, width);
            StoreRegister(opcode & 7, width, DR_T1);
            StoreRegister(I386_REG_EAX, width, DR_T0);
            break;
        case 0x98:
            if (width == 16)
            {
                LoadRegister(DR_T0, 0, 8);
                jit_extr_c(DR_T0, DR_T0);
                StoreRegister(I386_REG_EAX, 16, DR_T0);
            }
            else
            {
                LoadRegister(DR_T0, I386_REG_EAX, 16);
                jit_extr_s(DR_T0, DR_T0);
                StoreRegister(I386_REG_EAX, 32, DR_T0);
            }

            break;
        case 0x99:
            LoadRegister(DR_T0, I386_REG_EAX, width);
            jit_rshi_u(DR_T0, DR_T0, width - 1);
            jit_negr(DR_T0, DR_T0);
            StoreRegister(I386_REG_EDX, width, DR_T0);
            break;
        case 0xA0: case 0xA1: case 0xA2: case 0xA3:
            EmitMoveOffset(instruction, (opcode & 1) == 0 ? 8 : width, opcode <= 0xA1);
            break;
        case 0xA8: case 0xA9:
        {
            int test_width = opcode == 0xA8 ? 8 : width;
            LoadRegister(DR_T1, I386_REG_EAX, test_width);
            jit_andi(DR_T1, DR_T1, (jit_word_t)c.immediate);
            EmitLogicFlags(instruction, DR_T1, test_width);
            break;
        }
        case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7:
            jit_movi(DR_T0, (jit_word_t)(c.immediate & 0xFF));
            StoreRegister(opcode & 7, 8, DR_T0);
            break;
        case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF:
            jit_movi(DR_T0, (jit_word_t)c.immediate);
            StoreRegister(opcode & 7, width, DR_T0);
            break;
        case 0xC0: case 0xD0:
            EmitShift(instruction, 8, opcode == 0xD0 ? 1 : c.immediate & 0x1F);
            break;
        case 0xC1: case 0xD1:
            EmitShift(instruction, width, opcode == 0xD1 ? 1 : c.immediate & 0x1F);
            break;
        case 0xC2: case 0xC3:
            EmitReturn(instruction, index, width, opcode == 0xC2 ? (u16)c.immediate : 0);
            break;
        case 0xC6: case 0xC7:
            EmitMoveImmediate(instruction, opcode == 0xC6 ? 8 : width);
            break;
        case 0xE0: case 0xE1: case 0xE2: case 0xE3:
            EmitLoop(instruction, index, SignExtend(c.immediate, 8));
            break;
        case 0xE8:
            EmitCall(instruction, index, width);
            break;
        case 0xE9:
            EmitJump(instruction, index, SignExtend(c.immediate, width));
            break;
        case 0xEB:
            EmitJump(instruction, index, SignExtend(c.immediate, 8));
            break;
        case 0xF5: case 0xF8: case 0xF9: case 0xFC: case 0xFD:
        {
            // CMC, CLC, STC, CLD and STD
            intptr_t eflags = Offset(&m_cpu->m_state.eflags);
            u32 bit = (opcode & 0x04) != 0 ? I386_FLAG_DF : I386_FLAG_CF;

            LoadU32(DR_T0, DR_CPU, eflags);

            if (opcode == 0xF5)
                jit_xori(DR_T0, DR_T0, I386_FLAG_CF);
            else if ((opcode & 1) != 0)
                jit_ori(DR_T0, DR_T0, (jit_word_t)bit);
            else
                jit_andi(DR_T0, DR_T0, (jit_word_t)(u32)~bit);

            jit_stxi_i(eflags, DR_CPU, DR_T0);
            break;
        }
        case 0xF6: case 0xF7:
            EmitGroup3(instruction, opcode == 0xF6 ? 8 : width);
            break;
        case 0xFE: case 0xFF:
            EmitIncDecRM(instruction, opcode == 0xFE ? 8 : width, c.reg == 1);
            break;
        default:
            break;
    }
}

s32 DynarecEmitter::SignExtend(u32 value, int width) const
{
    if (width == 8)
        return (s32)(s8)(u8)value;

    if (width == 16)
        return (s32)(s16)(u16)value;

    return (s32)value;
}

// Host address of the memory operand in T0, from the offset of the effective address
void DynarecEmitter::EmitOperandAddress(Instruction& instruction, int width, int access)
{
    u8 segment = EmitEffectiveOffset(DR_T0, DR_T1, instruction.context);
    EmitHostAddress(DR_T0, DR_T1, DR_T2, DR_T3, segment, (u32)width / 8, access, instruction.fallback);
}

// ADD, OR, AND, SUB, XOR and CMP. Forms 0 and 1 write r/m, 2 and 3 write reg, 4 and 5 are the accumulator with an
// immediate. Group 1 comes as form 1 with its immediate
void DynarecEmitter::EmitALU(Instruction& instruction, int operation, int form, int width, u32 immediate)
{
    const I386::InstructionContext& c = instruction.context;
    bool group1 = c.opcode >= 0x80 && c.opcode <= 0x83;
    bool memory = (form <= 3 || group1) && c.memory_operand;
    bool compare = operation == 7;
    u32 mask = width == 8 ? 0xFF : width == 16 ? 0xFFFF : 0xFFFFFFFFU;

    if (memory)
    {
        bool destination_rm = form <= 1 || group1;
        EmitOperandAddress(instruction, width, destination_rm && !compare ? DYNAREC_READ_WRITE : DYNAREC_READ);

        if (group1)
        {
            LoadHost(DR_T1, DR_T0, width);
            jit_movi(DR_T2, (jit_word_t)(immediate & mask));
        }
        else if (destination_rm)
        {
            LoadHost(DR_T1, DR_T0, width);
            LoadRegister(DR_T2, c.reg, width);
        }
        else
        {
            LoadHost(DR_T2, DR_T0, width);
            LoadRegister(DR_T1, c.reg, width);
        }
    }
    else if (group1)
    {
        LoadRegister(DR_T1, c.rm, width);
        jit_movi(DR_T2, (jit_word_t)(immediate & mask));
    }
    else if (form <= 1)
    {
        LoadRegister(DR_T1, c.rm, width);
        LoadRegister(DR_T2, c.reg, width);
    }
    else if (form <= 3)
    {
        LoadRegister(DR_T1, c.reg, width);
        LoadRegister(DR_T2, c.rm, width);
    }
    else
    {
        LoadRegister(DR_T1, I386_REG_EAX, width);
        jit_movi(DR_T2, (jit_word_t)(immediate & mask));
    }

    u16 needed = instruction.live & instruction.defs;
    bool carry = operation == 2 || operation == 3;

    // ADC and SBB take the carry in, and the carry out comes from the bit above the width
    if (carry)
    {
        LoadU32(DR_T4, DR_CPU, Offset(&m_cpu->m_state.eflags));
        jit_andi(DR_T4, DR_T4, I386_FLAG_CF);
    }

    switch (operation)
    {
        case 0:
            jit_addr(DR_T3, DR_T1, DR_T2);
            break;
        case 1:
            jit_orr(DR_T3, DR_T1, DR_T2);
            break;
        case 2:
            jit_addr(DR_T3, DR_T1, DR_T2);
            jit_addr(DR_T3, DR_T3, DR_T4);
            break;
        case 3:
            jit_subr(DR_T3, DR_T1, DR_T2);
            jit_subr(DR_T3, DR_T3, DR_T4);
            break;
        case 4:
            jit_andr(DR_T3, DR_T1, DR_T2);
            break;
        case 6:
            jit_xorr(DR_T3, DR_T1, DR_T2);
            break;
        default:
            jit_subr(DR_T3, DR_T1, DR_T2);
            break;
    }

    // Stores take the low bits, so the result keeps the bit above the width for the carry out until it is masked
    if (!compare)
    {
        if (memory && (form <= 1 || group1))
            StoreHost(DR_T0, DR_T3, width, DR_T4);
        else if (form <= 1 || group1)
            StoreRegister(c.rm, width, DR_T3);
        else if (form <= 3)
            StoreRegister(c.reg, width, DR_T3);
        else
            StoreRegister(I386_REG_EAX, width, DR_T3);
    }

    if (carry && (needed & I386_FLAG_CF) != 0)
    {
        jit_rshi_u(DR_T4, DR_T3, width);
        jit_andi(DR_T4, DR_T4, 1);
    }

    Mask(DR_T3, width);

    if (needed == 0)
        return;

    if (operation == 1 || operation == 4 || operation == 6)
        EmitLogicFlags(instruction, DR_T3, width);
    else
    {
        if (carry)
            jit_movr(DR_T0, DR_T4);

        EmitArithmeticFlags(operation == 3 || operation >= 5, DR_T3, DR_T1, DR_T2, DR_T0, DR_T4, width, needed, carry);
    }
}

// Logic flags of a masked result: CF and OF clear, AF kept
void DynarecEmitter::EmitLogicFlags(Instruction& instruction, int result, int width)
{
    u16 needed = instruction.live & k_flags_logic;

    if (needed == 0)
        return;

    int f = result == DR_T0 ? DR_T1 : DR_T0;
    int t = result == DR_T2 ? DR_T1 : DR_T2;
    int u = result == DR_T4 ? DR_T1 : DR_T4;

    jit_movi(f, 0);
    EmitSZP(f, t, u, result, width, needed);
    MergeFlags(f, t, needed);
}

void DynarecEmitter::EmitTest(Instruction& instruction, int width)
{
    const I386::InstructionContext& c = instruction.context;

    if (c.memory_operand)
    {
        EmitOperandAddress(instruction, width, DYNAREC_READ);
        LoadHost(DR_T3, DR_T0, width);
    }
    else
        LoadRegister(DR_T3, c.rm, width);

    LoadRegister(DR_T2, c.reg, width);
    jit_andr(DR_T3, DR_T3, DR_T2);
    EmitLogicFlags(instruction, DR_T3, width);
}

void DynarecEmitter::EmitIncDecRegister(Instruction& instruction, int index, int width, bool decrement)
{
    LoadRegister(DR_T1, index, width);
    jit_movi(DR_T2, 1);

    if (decrement)
        jit_subr(DR_T3, DR_T1, DR_T2);
    else
        jit_addr(DR_T3, DR_T1, DR_T2);

    Mask(DR_T3, width);
    StoreRegister(index, width, DR_T3);

    u16 needed = instruction.live & instruction.defs;

    if (needed != 0)
        EmitArithmeticFlags(decrement, DR_T3, DR_T1, DR_T2, DR_T0, DR_T4, width, needed, false);
}

void DynarecEmitter::EmitIncDecRM(Instruction& instruction, int width, bool decrement)
{
    const I386::InstructionContext& c = instruction.context;

    if (!c.memory_operand)
    {
        EmitIncDecRegister(instruction, c.rm, width, decrement);
        return;
    }

    EmitOperandAddress(instruction, width, DYNAREC_READ_WRITE);
    LoadHost(DR_T1, DR_T0, width);
    jit_movi(DR_T2, 1);

    if (decrement)
        jit_subr(DR_T3, DR_T1, DR_T2);
    else
        jit_addr(DR_T3, DR_T1, DR_T2);

    Mask(DR_T3, width);
    StoreHost(DR_T0, DR_T3, width, DR_T4);

    u16 needed = instruction.live & instruction.defs;

    if (needed != 0)
        EmitArithmeticFlags(decrement, DR_T3, DR_T1, DR_T2, DR_T0, DR_T4, width, needed, false);
}

// TEST, NOT and NEG
void DynarecEmitter::EmitGroup3(Instruction& instruction, int width)
{
    const I386::InstructionContext& c = instruction.context;
    u8 reg = c.reg;

    if (c.memory_operand)
    {
        EmitOperandAddress(instruction, width, reg <= 1 ? DYNAREC_READ : DYNAREC_READ_WRITE);
        LoadHost(DR_T2, DR_T0, width);
    }
    else
        LoadRegister(DR_T2, c.rm, width);

    if (reg <= 1)
    {
        jit_andi(DR_T3, DR_T2, (jit_word_t)c.immediate);
        EmitLogicFlags(instruction, DR_T3, width);
        return;
    }

    if (reg == 2)
        jit_comr(DR_T3, DR_T2);
    else
        jit_negr(DR_T3, DR_T2);

    Mask(DR_T3, width);

    if (c.memory_operand)
        StoreHost(DR_T0, DR_T3, width, DR_T4);
    else
        StoreRegister(c.rm, width, DR_T3);

    u16 needed = instruction.live & instruction.defs;

    if (reg == 3 && needed != 0)
    {
        jit_movi(DR_T1, 0);
        EmitArithmeticFlags(true, DR_T3, DR_T1, DR_T2, DR_T0, DR_T4, width, needed, false);
    }
}

void DynarecEmitter::EmitMove(Instruction& instruction, int width, bool load)
{
    const I386::InstructionContext& c = instruction.context;

    if (!c.memory_operand)
    {
        LoadRegister(DR_T0, load ? c.rm : c.reg, width);
        StoreRegister(load ? c.reg : c.rm, width, DR_T0);
        return;
    }

    EmitOperandAddress(instruction, width, load ? DYNAREC_READ : DYNAREC_WRITE);

    if (load)
    {
        LoadHost(DR_T1, DR_T0, width);
        StoreRegister(c.reg, width, DR_T1);
    }
    else
    {
        LoadRegister(DR_T1, c.reg, width);
        StoreHost(DR_T0, DR_T1, width, DR_T2);
    }
}

void DynarecEmitter::EmitMoveImmediate(Instruction& instruction, int width)
{
    const I386::InstructionContext& c = instruction.context;

    if (!c.memory_operand)
    {
        jit_movi(DR_T0, (jit_word_t)c.immediate);
        StoreRegister(c.rm, width, DR_T0);
        return;
    }

    EmitOperandAddress(instruction, width, DYNAREC_WRITE);
    jit_movi(DR_T1, (jit_word_t)c.immediate);
    StoreHost(DR_T0, DR_T1, width, DR_T2);
}

void DynarecEmitter::EmitMoveOffset(Instruction& instruction, int width, bool load)
{
    const I386::InstructionContext& c = instruction.context;
    u8 segment = c.segment_override == 0xFF ? (u8)I386_SEGMENT_DS : c.segment_override;

    jit_movi(DR_T0, (jit_word_t)c.immediate);
    EmitHostAddress(DR_T0, DR_T1, DR_T2, DR_T3, segment, (u32)width / 8, load ? DYNAREC_READ : DYNAREC_WRITE,
        instruction.fallback);

    if (load)
    {
        LoadHost(DR_T1, DR_T0, width);
        StoreRegister(I386_REG_EAX, width, DR_T1);
    }
    else
    {
        LoadRegister(DR_T1, I386_REG_EAX, width);
        StoreHost(DR_T0, DR_T1, width, DR_T2);
    }
}

void DynarecEmitter::EmitLEA(Instruction& instruction, int width)
{
    EmitEffectiveOffset(DR_T0, DR_T1, instruction.context);
    StoreRegister(instruction.context.reg, width, DR_T0);
}

void DynarecEmitter::EmitMOVX(Instruction& instruction, int source_width, bool sign_extend)
{
    const I386::InstructionContext& c = instruction.context;

    if (c.memory_operand)
    {
        EmitOperandAddress(instruction, source_width, DYNAREC_READ);
        LoadHost(DR_T1, DR_T0, source_width);
    }
    else
        LoadRegister(DR_T1, c.rm, source_width);

    if (sign_extend)
    {
        if (source_width == 8)
            jit_extr_c(DR_T1, DR_T1);
        else
            jit_extr_s(DR_T1, DR_T1);
    }

    StoreRegister(c.reg, c.operand_size * 8, DR_T1);
}

void DynarecEmitter::EmitExchange(Instruction& instruction, int width)
{
    const I386::InstructionContext& c = instruction.context;

    if (!c.memory_operand)
    {
        LoadRegister(DR_T1, c.rm, width);
        LoadRegister(DR_T2, c.reg, width);
        StoreRegister(c.rm, width, DR_T2);
        StoreRegister(c.reg, width, DR_T1);
        return;
    }

    EmitOperandAddress(instruction, width, DYNAREC_READ_WRITE);
    LoadHost(DR_T1, DR_T0, width);
    LoadRegister(DR_T2, c.reg, width);
    StoreHost(DR_T0, DR_T2, width, DR_T3);
    StoreRegister(c.reg, width, DR_T1);
}

void DynarecEmitter::EmitSETcc(Instruction& instruction)
{
    const I386::InstructionContext& c = instruction.context;

    if (c.memory_operand)
    {
        EmitOperandAddress(instruction, 8, DYNAREC_WRITE);
        EmitCondition(DR_T1, DR_T2, c.opcode2 & 15);
        jit_str_c(DR_T0, DR_T1);
    }
    else
    {
        EmitCondition(DR_T1, DR_T2, c.opcode2 & 15);
        StoreRegister(c.rm, 8, DR_T1);
    }
}

// ROL, ROR, SHL, SHR, SAL and SAR by a constant count, as RotateShift computes them. Shifts take counts from 1 to
// the width, rotates any count from 1 to 31
void DynarecEmitter::EmitShift(Instruction& instruction, int width, u32 count)
{
    const I386::InstructionContext& c = instruction.context;
    u8 operation = c.reg;
    u16 needed = instruction.live & instruction.defs;

    if (c.memory_operand)
    {
        EmitOperandAddress(instruction, width, DYNAREC_READ_WRITE);
        LoadHost(DR_T1, DR_T0, width);
    }
    else
        LoadRegister(DR_T1, c.rm, width);

    // T2 carry, T3 result
    if (operation <= 1)
    {
        u32 rotate = count & (width - 1);

        if (rotate == 0)
            jit_movr(DR_T3, DR_T1);
        else if (operation == 0)
        {
            jit_lshi(DR_T3, DR_T1, (jit_word_t)rotate);
            jit_rshi_u(DR_T2, DR_T1, (jit_word_t)(width - rotate));
            jit_orr(DR_T3, DR_T3, DR_T2);
            Mask(DR_T3, width);
        }
        else
        {
            jit_rshi_u(DR_T3, DR_T1, (jit_word_t)rotate);
            jit_lshi(DR_T2, DR_T1, (jit_word_t)(width - rotate));
            jit_orr(DR_T3, DR_T3, DR_T2);
            Mask(DR_T3, width);
        }

        if (operation == 0)
            jit_andi(DR_T2, DR_T3, 1);
        else
            jit_rshi_u(DR_T2, DR_T3, width - 1);
    }
    else if (operation == 5)
    {
        jit_rshi_u(DR_T2, DR_T1, (jit_word_t)(count - 1));
        jit_andi(DR_T2, DR_T2, 1);
        jit_rshi_u(DR_T3, DR_T1, (jit_word_t)count);
    }
    else if (operation == 7)
    {
        if (count < (u32)width)
        {
            jit_rshi_u(DR_T2, DR_T1, (jit_word_t)(count - 1));
            jit_andi(DR_T2, DR_T2, 1);
        }
        else
            jit_rshi_u(DR_T2, DR_T1, width - 1);

        if (width == 8)
            jit_extr_c(DR_T3, DR_T1);
        else if (width == 16)
            jit_extr_s(DR_T3, DR_T1);
#if __WORDSIZE == 64
        else
            jit_extr_i(DR_T3, DR_T1);
#else
        else
            jit_movr(DR_T3, DR_T1);
#endif

        jit_rshi(DR_T3, DR_T3, (jit_word_t)MIN(count, (u32)width - 1));
        Mask(DR_T3, width);
    }
    else
    {
        jit_rshi_u(DR_T2, DR_T1, (jit_word_t)(width - count));
        jit_andi(DR_T2, DR_T2, 1);
        jit_lshi(DR_T3, DR_T1, (jit_word_t)count);
        Mask(DR_T3, width);
    }

    if (c.memory_operand)
        StoreHost(DR_T0, DR_T3, width, DR_T4);
    else
        StoreRegister(c.rm, width, DR_T3);

    if (needed == 0)
        return;

    // T0 flags, T1 and T4 scratch. SAR clears OF, ROR and SHR take it from the two top bits of the result, ROL,
    // SHL and SAL from the top bit and the carry
    if ((needed & I386_FLAG_CF) != 0)
        jit_movr(DR_T0, DR_T2);
    else
        jit_movi(DR_T0, 0);

    if ((needed & I386_FLAG_OF) != 0 && operation != 7)
    {
        jit_rshi_u(DR_T1, DR_T3, width - 1);

        if (operation == 1 || operation == 5)
        {
            jit_rshi_u(DR_T4, DR_T3, width - 2);
            jit_xorr(DR_T1, DR_T1, DR_T4);
            jit_andi(DR_T1, DR_T1, 1);
        }
        else
            jit_xorr(DR_T1, DR_T1, DR_T2);

        jit_lshi(DR_T1, DR_T1, 11);
        jit_orr(DR_T0, DR_T0, DR_T1);
    }

    EmitSZP(DR_T0, DR_T1, DR_T4, DR_T3, width, needed);
    MergeFlags(DR_T0, DR_T1, needed);
}

// StackPush of the value in T4: ESP only changes after the write
void DynarecEmitter::EmitPush(Instruction& instruction, int width)
{
    bool stack32 = (m_key & k_dynarec_key_stack32) != 0;
    intptr_t esp = RegisterOffset(I386_REG_ESP, 32);

    LoadU32(DR_T0, DR_CPU, esp);
    jit_subi(DR_T0, DR_T0, width / 8);
    Mask(DR_T0, stack32 ? 32 : 16);
    EmitHostAddress(DR_T0, DR_T1, DR_T2, DR_T3, I386_SEGMENT_SS, (u32)width / 8, DYNAREC_WRITE, instruction.fallback);
    StoreHost(DR_T0, DR_T4, width, DR_T1);
    LoadU32(DR_T0, DR_CPU, esp);
    jit_subi(DR_T0, DR_T0, width / 8);
    StoreRegister(I386_REG_ESP, stack32 ? 32 : 16, DR_T0);
}

void DynarecEmitter::EmitPop(Instruction& instruction, int index, int width)
{
    bool stack32 = (m_key & k_dynarec_key_stack32) != 0;

    LoadRegister(DR_T0, I386_REG_ESP, stack32 ? 32 : 16);
    jit_movr(DR_T4, DR_T0);
    EmitHostAddress(DR_T0, DR_T1, DR_T2, DR_T3, I386_SEGMENT_SS, (u32)width / 8, DYNAREC_READ, instruction.fallback);
    LoadHost(DR_T1, DR_T0, width);
    jit_addi(DR_T4, DR_T4, width / 8);
    StoreRegister(I386_REG_ESP, stack32 ? 32 : 16, DR_T4);
    StoreRegister(index, width, DR_T1);
}

void DynarecEmitter::EmitJcc(Instruction& instruction, int index, int condition, s32 displacement)
{
    const I386::InstructionContext& c = instruction.context;
    u32 target = c.next_eip + (u32)displacement;

    if (c.operand_size == 2)
        target &= 0xFFFF;

    m_block->eip_limit = MAX(m_block->eip_limit, target);

    u32 components = 0;
    bool known = GetTargetComponents(target, components);

    EmitCondition(DR_T0, DR_T1, condition);
    jit_node_t* taken = jit_bnei(DR_T0, 0);
    StoreEIP(c.next_eip);
    ExitLinked(index + 1, instruction.before + 3, k_dynarec_exit_end, 0);
    jit_patch(taken);
    StoreEIP(target);

    if (known)
        ExitTo(target, index + 1, instruction.before + 7 + components, 1);
    else
        Exit(index + 1, instruction.before + 7, k_dynarec_exit_transfer | k_dynarec_exit_end);
}

// LOOPcc leaves the flags alone and adds the next instruction's m clocks either way, JCXZ only when it jumps
void DynarecEmitter::EmitLoop(Instruction& instruction, int index, s32 displacement)
{
    const I386::InstructionContext& c = instruction.context;
    u8 opcode = c.opcode;
    int count_width = c.address_size * 8;
    u32 target = c.next_eip + (u32)displacement;

    if (c.operand_size == 2)
        target &= 0xFFFF;

    m_block->eip_limit = MAX(m_block->eip_limit, target);

    u32 target_components = 0;
    bool target_known = GetTargetComponents(target, target_components);
    u32 next_components = 0;
    bool next_known = opcode == 0xE3 || GetTargetComponents(c.next_eip, next_components);
    u32 next_clocks = opcode == 0xE3 ? 5 : 11;
    u32 target_clocks = opcode == 0xE3 ? 9 : 11;
    jit_node_t* taken = NULL;

    LoadRegister(DR_T0, I386_REG_ECX, count_width);

    if (opcode == 0xE3)
        taken = jit_beqi(DR_T0, 0);
    else
    {
        jit_subi(DR_T0, DR_T0, 1);
        Mask(DR_T0, count_width);
        StoreRegister(I386_REG_ECX, count_width, DR_T0);

        if (opcode == 0xE2)
            taken = jit_bnei(DR_T0, 0);
        else
        {
            jit_node_t* zero = jit_beqi(DR_T0, 0);
            EmitCondition(DR_T0, DR_T1, opcode == 0xE1 ? 4 : 5);
            taken = jit_bnei(DR_T0, 0);
            jit_patch(zero);
        }
    }

    StoreEIP(c.next_eip);

    if (next_known)
        ExitLinked(index + 1, instruction.before + next_clocks + next_components, k_dynarec_exit_end, 0);
    else
        Exit(index + 1, instruction.before + next_clocks, k_dynarec_exit_transfer | k_dynarec_exit_end);

    jit_patch(taken);
    StoreEIP(target);

    if (target_known)
        ExitTo(target, index + 1, instruction.before + target_clocks + target_components, 1);
    else
        Exit(index + 1, instruction.before + target_clocks, k_dynarec_exit_transfer | k_dynarec_exit_end);
}

void DynarecEmitter::EmitJump(Instruction& instruction, int index, s32 displacement)
{
    const I386::InstructionContext& c = instruction.context;
    u32 target = c.next_eip + (u32)displacement;

    if (c.operand_size == 2)
        target &= 0xFFFF;

    m_block->eip_limit = MAX(m_block->eip_limit, target);

    u32 components = 0;
    bool known = GetTargetComponents(target, components);

    StoreEIP(target);

    if (known)
        ExitTo(target, index + 1, instruction.before + 7 + components, 1);
    else
        Exit(index + 1, instruction.before + 7, k_dynarec_exit_transfer | k_dynarec_exit_end);
}

void DynarecEmitter::EmitCall(Instruction& instruction, int index, int width)
{
    const I386::InstructionContext& c = instruction.context;
    u32 target = c.next_eip + c.immediate;

    if (width == 16)
        target &= 0xFFFF;

    m_block->eip_limit = MAX(m_block->eip_limit, target);
    m_block->call_eip = c.start_eip;

    u32 components = 0;
    bool known = GetTargetComponents(target, components);
    m_block->call_next_eip = width == 16 ? c.next_eip & 0xFFFF : c.next_eip;

    jit_movi(DR_T4, (jit_word_t)(width == 16 ? c.next_eip & 0xFFFF : c.next_eip));
    EmitPush(instruction, width);
    StoreEIP(target);

#if !defined(GT_DISABLE_DISASSEMBLER)
    // The debugger call stack takes the call before the link
    jit_movi(DR_T3, (jit_word_t)(uintptr_t)&I386::DynarecCall);
    jit_prepare();
    jit_pushargr(DR_CPU);
    jit_finishr(DR_T3);
#endif

    if (known)
        ExitLinked(index + 1, instruction.before + instruction.clocks + components, k_dynarec_exit_end, 1);
    else
        Exit(index + 1, instruction.before + instruction.clocks, k_dynarec_exit_transfer | k_dynarec_exit_end);
}

void DynarecEmitter::EmitReturn(Instruction& instruction, int index, int width, u16 release)
{
    bool stack32 = (m_key & k_dynarec_key_stack32) != 0;

    LoadRegister(DR_T0, I386_REG_ESP, stack32 ? 32 : 16);
    jit_movr(DR_T4, DR_T0);
    EmitHostAddress(DR_T0, DR_T1, DR_T2, DR_T3, I386_SEGMENT_SS, (u32)width / 8, DYNAREC_READ, instruction.fallback);
    LoadHost(DR_T1, DR_T0, width);
    LoadU32(DR_T2, DR_CPU, Offset(&m_cpu->m_state.segments[I386_SEGMENT_CS].limit));
    jit_patch_at(jit_bgtr_u(DR_T1, DR_T2), instruction.fallback);
    jit_addi(DR_T4, DR_T4, (jit_word_t)(width / 8 + release));
    StoreRegister(I386_REG_ESP, stack32 ? 32 : 16, DR_T4);
    jit_stxi_i(Offset(&m_cpu->m_state.eip), DR_CPU, DR_T1);
    Exit(index + 1, instruction.before + instruction.clocks,
        k_dynarec_exit_transfer | k_dynarec_exit_return | k_dynarec_exit_end);
}

#endif
