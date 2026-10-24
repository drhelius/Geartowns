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

#ifndef X87_H
#define X87_H

#include <iostream>
#include "../common/common.h"

#define X87_REGISTER_COUNT 8

enum X87_Flavor
{
    X87_FLAVOR_80387 = 0,
    X87_FLAVOR_80486
};

struct X87_Operation
{
    u8 opcode;
    u8 modrm;
    bool memory;
    bool operand32;
    bool protected_mode;
    u16 instruction_selector;
    u32 instruction_offset;
    u16 operand_selector;
    u32 operand_offset;
};

class StateSerializer;

class X87
{
public:
    struct X87_Register
    {
        u64 significand;
        u16 sign_exponent;
    };

    struct X87_State
    {
        X87_Register registers[X87_REGISTER_COUNT];
        u16 control;
        u16 status;
        u16 tag;
        u16 opcode;
        u32 instruction_offset;
        u16 instruction_selector;
        u32 operand_offset;
        u16 operand_selector;
    };

public:
    X87();
    ~X87();
    void Init(X87_Flavor flavor);
    void Reset();
    X87_State* GetState();
    X87_Flavor GetFlavor() const;
    bool IsErrorPending() const;
    u16 GetStatusWord() const;
    u16 GetTagWord() const;
    bool Execute(const X87_Operation& operation, u8* operand);
    u32 GetClocks(u8 opcode, u8 modrm, const u8* operand) const;
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

    static bool IsWaiting(u8 opcode, u8 modrm);
    static u32 GetMemoryOperandSize(u8 opcode, u8 modrm, bool operand32, bool& write);

private:
    void Serialize(StateSerializer& serializer);
    void SanitizeState();
    void Initialize();
    void UpdatePointers(const X87_Operation& operation);
    void ExecuteRegister(const X87_Operation& operation);
    void ExecuteD9(u8 modrm);
    bool ExecuteMemory(const X87_Operation& operation, u8* operand);

    int GetTop() const;
    void SetTop(int top);
    int GetTag(int index) const;
    void SetTag(int index, int tag);
    bool IsEmpty(int index) const;
    const X87_Register& Read(int index) const;
    void Write(const X87_Register& value, int index);
    void Push();
    void Pop();

    void SetConditionCodes(u16 codes);
    void ClearC1();
    u32 RaiseExceptions(u32 exceptions, bool store = false);
    void StackOverflow();
    void StackUnderflow(int index, bool pop);
    void UpdateErrorSummary();

    void Arithmetic(int operation, int destination, int source, bool pop);
    void ArithmeticReal(int operation, u32 size, const u8* operand);
    void ArithmeticInteger(int operation, s32 value);
    void Compare(int source, int pops, bool quiet);
    void CompareReal(u32 size, const u8* operand, bool pop);
    void CompareInteger(s32 value, bool pop);
    void Test();
    void Examine();
    void LoadRegister(int source);
    void LoadReal(u32 size, const u8* operand);
    void LoadExtended(const u8* operand);
    void LoadInteger(s64 value);
    void LoadBCD(const u8* operand);
    void LoadConstant(const X87_Register& value, int adjust);
    void StoreRegister(int destination, bool pop, bool alias);
    bool StoreReal(u32 size, u8* operand, bool pop);
    bool StoreExtended(u8* operand);
    bool StoreInteger(u32 size, u8* operand, bool pop);
    bool StoreBCD(u8* operand);
    void Exchange(int source);
    void Free(int index, bool pop);
    void ChangeSign(bool absolute);
    void Unary(u8 modrm);
    void Binary(u8 modrm);
    void PushPair(u8 modrm);
    void Remainder(bool ieee);
    u32 StoreEnvironment(u8* buffer, bool protected_mode, bool operand32);
    u32 LoadEnvironment(const u8* buffer, bool protected_mode, bool operand32);
    void Save(u8* buffer, bool protected_mode, bool operand32);
    void Restore(const u8* buffer, bool protected_mode, bool operand32);

private:
    X87_State m_state;
    X87_Flavor m_flavor;
};

#include "x87_inline.h"

#endif /* X87_H */
