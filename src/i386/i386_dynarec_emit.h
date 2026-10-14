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

#ifndef I386_DYNAREC_EMIT_H
#define I386_DYNAREC_EMIT_H

#include "i386.h"

#if !defined(GT_DISABLE_DYNAREC)

struct jit_node;
struct jit_state;

static const u8 k_dynarec_key_default32 = 0x01;
static const u8 k_dynarec_key_paging = 0x02;
static const u8 k_dynarec_key_stack32 = 0x04;

// Blocks leave to the dispatcher with these flags: the m clocks of a transfer to count, a return for the debugger
// call stack, an exit at the natural end of the block and a static exit the dispatcher can link
static const u32 k_dynarec_exit_transfer = 0x01;
static const u32 k_dynarec_exit_return = 0x04;
static const u32 k_dynarec_exit_end = 0x08;
static const u32 k_dynarec_exit_link = 0x10;
static const u32 k_dynarec_exit_slot = 0x20;

class DynarecEmitter
{
public:
    DynarecEmitter(I386* cpu);
    static void InitLightning();
    bool EmitEntry(u8* code, u32 capacity, u32& code_size, u32& exit_offset, u32& link_offset);
    bool Compile(I386::DynarecBlock& block, u32 available, u32 instruction_limit, u8* code, u32 capacity,
        u32& code_size);
    int GetInstructionCount() const;
    u32 GetInstructionEIP(int index) const;

private:
    struct Instruction
    {
        I386::InstructionContext context;
        u8 kind;
        bool memory;
        bool ends_block;
        u16 uses;
        u16 defs;
        u16 live;
        u32 clocks;
        u32 before;
        u32 next_check;
        jit_node* fallback;
        jit_node* resume;
    };

    bool Decode(u32 available, u32 instruction_limit);
    void Classify(Instruction& instruction);
    void SetNative(Instruction& instruction, u32 clocks, bool memory);
    bool CanCarry(int width) const;
    u32 GetAddressClocks(const I386::InstructionContext& c) const;
    u16 GetConditionFlags(int condition) const;
    bool GetTargetComponents(u32 target, u32& components);
    s32 SignExtend(u32 value, int width) const;
    void ComputeLiveness();

    void EmitBlock();
    void EmitLink(jit_node* exit);
    void EmitFallback(Instruction& instruction, int index, int last);
    void Exit(int steps, u32 clocks, u32 flags);
    void ExitLinked(int steps, u32 clocks, u32 flags, int slot);
    void ExitTo(u32 target, int steps, u32 clocks, int slot);
    void StoreEIP(u32 eip);
    void CallStep(u32 next_check, u32 native_clocks, u32 before);
    intptr_t Offset(const void* member) const;
    intptr_t RegisterOffset(int index, int width) const;
    void LoadU32(int r, int base, intptr_t offset);
    void Mask(int r, int width);
    void LoadRegister(int r, int index, int width);
    void StoreRegister(int index, int width, int r);
    void LoadHost(int r, int host, int width);
    void StoreHost(int host, int r, int width, int scratch);
    u8 EmitEffectiveOffset(int r, int t, const I386::InstructionContext& c);
    void EmitHostAddress(int r, int t1, int t2, int t3, u8 segment, u32 size, int access, jit_node* fallback);
    void EmitOperandAddress(Instruction& instruction, int width, int access);
    void EmitSZP(int f, int t, int u, int result, int width, u16 needed);
    void EmitArithmeticFlags(bool subtract, int result, int left, int right, int f, int t, int width, u16 needed,
        bool carry_in_f);
    void EmitLogicFlags(Instruction& instruction, int result, int width);
    void MergeFlags(int f, int t, u32 mask);
    void EmitCondition(int r, int t, int condition);

    void EmitNative(Instruction& instruction, int index);
    void EmitALU(Instruction& instruction, int operation, int form, int width, u32 immediate);
    void EmitTest(Instruction& instruction, int width);
    void EmitIncDecRegister(Instruction& instruction, int index, int width, bool decrement);
    void EmitIncDecRM(Instruction& instruction, int width, bool decrement);
    void EmitGroup3(Instruction& instruction, int width);
    void EmitMove(Instruction& instruction, int width, bool load);
    void EmitMoveImmediate(Instruction& instruction, int width);
    void EmitMoveOffset(Instruction& instruction, int width, bool load);
    void EmitLEA(Instruction& instruction, int width);
    void EmitMOVX(Instruction& instruction, int source_width, bool sign_extend);
    void EmitExchange(Instruction& instruction, int width);
    void EmitSETcc(Instruction& instruction);
    void EmitShift(Instruction& instruction, int width, u32 count);
    void EmitPush(Instruction& instruction, int width);
    void EmitPop(Instruction& instruction, int index, int width);
    void EmitJcc(Instruction& instruction, int index, int condition, s32 displacement);
    void EmitLoop(Instruction& instruction, int index, s32 displacement);
    void EmitJump(Instruction& instruction, int index, s32 displacement);
    void EmitCall(Instruction& instruction, int index, int width);
    void EmitReturn(Instruction& instruction, int index, int width, u16 release);

private:
    I386* m_cpu;
    jit_state* _jit;
    I386::DynarecBlock* m_block;
    jit_node* m_block_start;
    u8 m_key;
    int m_count;
    Instruction m_instructions[64];
};

#endif

#endif /* I386_DYNAREC_EMIT_H */
