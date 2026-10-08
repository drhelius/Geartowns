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

#ifndef I386_H
#define I386_H

#include <iostream>
#include <map>
#include <vector>
#include "../common/common.h"
#include "../common/debug_memory.h"

#define I386_TLB_SETS 8
#define I386_TLB_WAYS 4
#define I386_TLB_SIZE (I386_TLB_SETS * I386_TLB_WAYS)

class Memory;
class Profiler;
class TraceLogger;
class IO;

enum I386_Register
{
    I386_REG_EAX = 0,
    I386_REG_ECX,
    I386_REG_EDX,
    I386_REG_EBX,
    I386_REG_ESP,
    I386_REG_EBP,
    I386_REG_ESI,
    I386_REG_EDI,
    I386_REG_COUNT
};

enum I386_Segment_Register
{
    I386_SEGMENT_ES = 0,
    I386_SEGMENT_CS,
    I386_SEGMENT_SS,
    I386_SEGMENT_DS,
    I386_SEGMENT_FS,
    I386_SEGMENT_GS,
    I386_SEGMENT_COUNT
};

enum I386_Execution_Mode
{
    I386_MODE_REAL = 0,
    I386_MODE_PROTECTED,
    I386_MODE_VM86
};

enum I386_Exception_Class
{
    I386_EXCEPTION_FAULT = 0,
    I386_EXCEPTION_TRAP,
    I386_EXCEPTION_ABORT
};

enum I386_Interrupt_Shadow
{
    I386_SHADOW_NONE = 0,
    I386_SHADOW_STI,
    I386_SHADOW_MOV_SS
};

enum I386_Call_Type
{
    I386_CALL = 0,
    I386_CALL_SOFTWARE_INTERRUPT,
    I386_CALL_HARDWARE_INTERRUPT,
    I386_CALL_EXCEPTION
};

enum I386_Segment_Attributes
{
    I386_SEGMENT_PRESENT = 0x0001,
    I386_SEGMENT_READABLE = 0x0002,
    I386_SEGMENT_WRITABLE = 0x0004,
    I386_SEGMENT_EXECUTABLE = 0x0008,
    I386_SEGMENT_DEFAULT_32 = 0x0010,
    I386_SEGMENT_EXPAND_DOWN = 0x0020,
    I386_SEGMENT_GRANULAR = 0x0040,
    I386_SEGMENT_SYSTEM = 0x0080,
    I386_SEGMENT_CONFORMING = 0x0100,
    I386_SEGMENT_ACCESSED = 0x0200,
    I386_SEGMENT_TYPE_SHIFT = 12,
    I386_SEGMENT_TYPE_MASK = 0xF000
};

enum I386_Flags
{
    I386_FLAG_CF = 0x00000001,
    I386_FLAG_FIXED = 0x00000002,
    I386_FLAG_PF = 0x00000004,
    I386_FLAG_AF = 0x00000010,
    I386_FLAG_ZF = 0x00000040,
    I386_FLAG_SF = 0x00000080,
    I386_FLAG_TF = 0x00000100,
    I386_FLAG_IF = 0x00000200,
    I386_FLAG_DF = 0x00000400,
    I386_FLAG_OF = 0x00000800,
    I386_FLAG_IOPL = 0x00003000,
    I386_FLAG_NT = 0x00004000,
    I386_FLAG_RF = 0x00010000,
    I386_FLAG_VM = 0x00020000
};

struct I386_Segment
{
    u16 selector;
    u16 attributes;
    u32 base;
    u32 limit;
    u8 dpl;
};

struct I386_Descriptor_Table
{
    u32 base;
    u16 limit;
};

struct I386_Pending_Exception
{
    bool pending;
    bool has_error_code;
    bool has_return_eip;
    u8 vector;
    u8 exception_class;
    u32 error_code;
    u32 return_eip;
};

struct I386_Repeat_State
{
    bool active;
    u32 start_eip;
    u32 next_eip;
    u8 opcode;

    u8 operand_size;
    u8 address_size;
    u8 repeat;
    u8 segment_override;
};

struct I386_State
{
    u32_union registers[I386_REG_COUNT];
    u32 eip;
    u32 eflags;

    I386_Segment segments[I386_SEGMENT_COUNT];

    I386_Descriptor_Table gdtr;
    I386_Descriptor_Table idtr;
    I386_Segment ldtr;
    I386_Segment task_register;

    u32 cr0;
    u32 cr2;
    u32 cr3;

    u32 debug_registers[8];
    u32 test_registers[2];

    I386_Execution_Mode execution_mode;
    u8 current_privilege_level;
    I386_Interrupt_Shadow interrupt_shadow;
    u8 interrupt_shadow_steps;
    u8 last_exception_vector;

    bool halted;
    bool shutdown;
    bool nmi_blocked;

    I386_Repeat_State repeat;
};

struct I386_Run_Result
{
    u64 clocks;
    u32 steps;
    bool instruction_completed;
    bool end_batch;
    bool exception;
    u8 exception_vector;
    bool exception_after_instruction;
    u16 exception_return_cs;
    u32 exception_return_eip;
    u32 exception_return_base;
    u32 exception_source_eip;
};

struct I386_Debug_Segment_State
{
    u16 selector;
    u32 base;
    u32 limit;
    u32 access;
    u8 dpl;
    bool present;
};

struct I386_Debug_State
{
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;
    u32 esi;
    u32 edi;
    u32 ebp;
    u32 esp;
    u32 eip;
    u32 eflags;

    u32 cr0;
    u32 cr2;
    u32 cr3;

    I386_Debug_Segment_State segment[I386_SEGMENT_COUNT];
};

struct I386_Decode_State
{
    u32 start_eip;
    u32 next_eip;

    u32 effective_offset;
    s32 displacement;

    u32 immediate;
    u32 immediate2;

    u8 bytes[GT_I386_MAX_INSTRUCTION_LENGTH];
    u8 length;
    u8 opcode;
    u8 opcode2;

    u8 modrm;
    u8 sib;
    u8 mod;
    u8 reg;
    u8 rm;

    u8 operand_size;
    u8 address_size;
    u8 segment;
    u8 segment_override;
    u8 repeat;

    u8 sib_scale;
    u8 sib_index;
    u8 sib_base;

    bool two_byte;
    bool lock;
    bool has_modrm;
    bool has_sib;
    bool memory_operand;
    bool invalid_lock;
};

struct I386_Trace_Entry
{
    I386_Decode_State instruction;
    I386_State before;
    I386_State after;
};

struct I386_Disassembler_Record
{
    u16 cs;
    u32 eip;
    u32 linear;
    char name[128];
    char bytes[GT_I386_MAX_INSTRUCTION_LENGTH * 3 + 1];
    char segment[16];
    u8 opcodes[GT_I386_MAX_INSTRUCTION_LENGTH];
    int size;
    I386_Execution_Mode mode;
    bool default32;

    bool jump;
    bool jump_far;
    bool jump_target_known;
    bool subroutine;
    bool returns;
    bool unconditional;

    u16 jump_cs;
    u32 jump_eip;
    u32 jump_linear;

    char auto_symbol[64];
};

enum I386_Breakpoint_Type
{
    I386_BREAKPOINT_EXECUTE = 0x01,
    I386_BREAKPOINT_READ = 0x02,
    I386_BREAKPOINT_WRITE = 0x04
};

enum I386_Breakpoint_Space
{
    I386_BREAKPOINT_LINEAR = 0,
    I386_BREAKPOINT_PHYSICAL,
    I386_BREAKPOINT_IO,
    I386_BREAKPOINT_SPACE_COUNT
};

enum I386_Interrupt_Source
{
    I386_INTERRUPT_ANY = 0,
    I386_INTERRUPT_EXCEPTION,
    I386_INTERRUPT_HARDWARE,
    I386_INTERRUPT_SOFTWARE,
    I386_INTERRUPT_SOURCE_COUNT
};

struct I386_Breakpoint
{
    bool enabled;
    u32 address1;
    u32 address2;
    bool range;
    u8 type;
    u8 space;
};

struct I386_Interrupt_Breakpoint
{
    bool enabled;
    u8 vector;
    u8 source;
};

struct I386_Breakpoint_Hit
{
    bool interrupt;
    u8 type;
    u8 space;
    u32 address;
    u32 size;
    u8 vector;
    u8 source;
    u8 line;
    u16 ax;
};

struct I386_CallStackEntry
{
    u16 src_cs;
    u16 dest_cs;
    u16 back_cs;
    u32 src;
    u32 dest;
    u32 back;
    u32 src_linear;
    u32 dest_linear;
    u32 back_linear;
    bool interrupt;
    u8 type;
    u8 vector;
};

class StateSerializer;

class I386
{
public:
    I386();
    ~I386();
    void Init(Memory* memory, IO* io = NULL);
    void Reset();
    u32 RunInstruction(GT_Bus_Access_Context& context);
    I386_Run_Result GetStepInfo() const;
    I386_Run_Result RunFor(u32 cycle_budget, GT_Bus_Access_Context& context, bool nmi_pending, bool intr_pending);

    bool Halted() const;
    bool Shutdown() const;

    bool CanAcceptMaskableInterrupt() const;
    bool CanAcceptNMI() const;
    u32 EnterExternalInterrupt(u8 vector, GT_Bus_Access_Context& context, int line = -1);
    u32 EnterNMI(GT_Bus_Access_Context& context);

    I386_State* GetState();
    void CopyState(I386_State& state) const;
    bool SetState(const I386_State& state);

    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream);

    u8 GetLastExceptionVector() const;
    bool CopyDecodeState(I386_Decode_State& state);

    void SetTraceEnabled(bool enabled);
    int CopyTraceEntries(I386_Trace_Entry* entries, int capacity) const;
    void SetTraceLogger(TraceLogger* trace_logger);
    void SetProfiler(Profiler* profiler);

    bool TryPeekLogical(I386_Segment_Register segment, u32 offset, u8& value) const;
    bool TryPeekLogical(u16 selector, u32 offset, u8& value) const;
    bool TryPeekLinear(u32 linear, u8& value) const;
    bool TryTranslateLinear(u32 linear, u32& physical) const;
    bool DebugTranslateLogical(I386_Segment_Register segment, u32 offset,
        GT_Debug_Memory_Translation& translation) const;
    bool DebugTranslateLogical(u16 selector, u32 offset, GT_Debug_Memory_Translation& translation) const;
    bool DebugTranslateLinear(u32 linear, GT_Debug_Memory_Translation& translation) const;

    bool CopyDebugState(I386_Debug_State& state) const;
    bool GetDebugRegisterValue(const char* name, u32& value) const;

    I386_Disassembler_Record* Disassemble(u32 eip);
    I386_Disassembler_Record* Disassemble(const I386_Segment& code_segment, u32 eip);
    void DisassembleAhead(int count);
    void DisassembleAhead(u32 start_eip, int count, int depth = 0);
    I386_Disassembler_Record* GetDisassemblerRecord(u32 linear);
    bool IsDisassemblerRecordCurrent(const I386_Disassembler_Record& record, const I386_Segment& code_segment) const;
    const std::map<u32, I386_Disassembler_Record>& GetDisassemblerRecords() const;
    void ResetDisassembler();
    void ResetDebuggerExecutionState();

    void ResetBreakpoints();
    void AddBreakpoint(u32 address);
    void AddBreakpoint(u32 start_address, u32 end_address);
    bool AddBreakpoint(u32 start_address, u32 end_address, u8 type, u8 space);
    void AddRunToBreakpoint(u32 address);
    void RemoveBreakpoint(u32 address, u32 end_address = 0);
    bool RemoveBreakpoint(u32 start_address, u32 end_address, u8 type, u8 space);
    bool IsBreakpoint(u32 address) const;
    std::vector<I386_Breakpoint>* GetBreakpoints();
    bool AddInterruptBreakpoint(u8 vector, u8 source);
    bool RemoveInterruptBreakpoint(u8 vector, u8 source);
    std::vector<I386_Interrupt_Breakpoint>* GetInterruptBreakpoints();
    void SetIRQBreakpoint(int line, bool set);
    void EnableIRQBreakpoint(int line, bool enabled);
    bool IsIRQBreakpoint(int line) const;
    bool IsIRQBreakpointEnabled(int line) const;
    u16 GetIRQBreakpoints() const;
    u16 GetDisabledIRQBreakpoints() const;
    void SetIRQBreakpoints(u16 lines, u16 disabled);
    void SetVBlankWatch(bool read, bool write, u32 address);
    void UpdateVBlankWatch();
    void EnableDebuggerChecks(bool enable);
    bool CheckDebuggerBreakpoints(bool regular, bool run_to);
    bool IsDebuggerHitPending() const;
    bool AcceptDebuggerHit();
    void DiscardDebuggerHit();
    bool GetBreakpointHitAddress(u32& address) const;
    bool GetBreakpointHit(I386_Breakpoint_Hit& hit) const;
    bool RunToBreakpointHit() const;

    const std::vector<I386_CallStackEntry>& GetDisassemblerCallStack() const;
    void SetDisassemblerCallStack(const std::vector<I386_CallStackEntry>& call_stack);
    bool GetStepCall(u32& return_linear) const;
    u32 GetCurrentLinearPC() const;

private:
    struct StepState
    {
        u64 clocks;
        u32 steps;
        bool instruction_completed;
        bool end_batch;
        bool exception;
    };

    struct InstructionContext
    {
        u32 start_eip;
        u32 next_eip;

        u32 effective_offset;
        s32 displacement;

        u32 immediate;
        u32 immediate2;

        u8 operand_size;
        u8 address_size;
        u8 segment_override;
        u8 repeat;
        bool two_byte;
        bool lock;
        bool memory_operand;
        u8 segment;

        u8 opcode;
        u8 opcode2;
        u8 modrm;
        u8 sib;
        u8 reg;
        u8 rm;
#if !defined(GT_DISABLE_DISASSEMBLER)
        u8 call_return_size;
#endif
    };

    struct StringContext
    {
        u8 width;
        u8 address_width;
        u8 source_segment;
        u8 index_mask;
        u8 iteration_clocks;
        bool repeated;
    };

    struct TimingCursor
    {
        u32 eip;
        u32 linear_page;
        u32 physical_page;
        const u8* data;
        u8 remaining;
        u8 length;
        bool translated;
    };

    struct Descriptor
    {
        u32 low;
        u32 high;
        u32 address;
        u32 base;
        u32 limit;
        u16 selector;
        u16 attributes;
        u8 access;
        u8 type;
        u8 dpl;
        bool system;
        bool present;
    };

    struct TaskState
    {
        u32 registers[I386_REG_COUNT];
        u32 eip;
        u32 eflags;
        u32 cr3;
        u16 segments[I386_SEGMENT_COUNT];
        u16 ldtr;
        bool tss32;
        bool debug_trap;
    };

    enum I386_Shift_Operation
    {
        I386_SHIFT_ROL = 0,
        I386_SHIFT_ROR,
        I386_SHIFT_RCL,
        I386_SHIFT_RCR,
        I386_SHIFT_SHL,
        I386_SHIFT_SHR,
        I386_SHIFT_SAL,
        I386_SHIFT_SAR
    };

    enum I386_Shift_Count
    {
        I386_SHIFT_COUNT_IMMEDIATE = 0,
        I386_SHIFT_COUNT_ONE,
        I386_SHIFT_COUNT_CL
    };

    enum I386_Task_Switch
    {
        I386_TASK_SWITCH_JMP = 0,
        I386_TASK_SWITCH_CALL,
        I386_TASK_SWITCH_IRET,
        I386_TASK_SWITCH_INTERRUPT
    };

    // TLB entries are indexed set * I386_TLB_WAYS + way
    // The tag and the D/U/W attributes are the hardware entry
    // The host pages are cached for the current privilege level and are NULL for invalid entries
    struct TLBEntry
    {
        u32 linear_page;
        u32 physical_page;
        u8 flags;
        const u8* read_page;
        u8* write_page;
    };

    typedef bool (I386::*opcode_member_ptr)();
    typedef bool (*opcodeptr)(I386*);

    template<opcode_member_ptr Opcode>
    static bool OPCodeThunk(I386* cpu)
    {
        return (cpu->*Opcode)();
    }

    static const opcodeptr k_opcodes[256];
    static const opcodeptr k_opcodes_0f[256];
    static const u8 k_opcode_encoding[512];
    static const u8 k_szp_flags[256];

private:
    static void SerializeSegment(StateSerializer& serializer, I386_Segment& segment);
    static void SerializeDescriptorTable(StateSerializer& serializer, I386_Descriptor_Table& table);
    void Serialize(StateSerializer& serializer);
    void SanitizeState();

    u32 RunCheckedStep();
    bool RunForSlowStep(I386_Run_Result& total, u32 cycle_budget, bool nmi_pending, bool intr_pending);
    bool IsInterruptReady(bool nmi_pending, bool intr_pending) const;
    void DisassembleNextInstruction();
    void ClearDisassemblerCache();
    void PushCallStack(u16 src_cs, u32 src_base, u32 src, u32 back, I386_Call_Type type, u8 vector);
    bool TrackCall(bool completed, u16 cs, u32 base);
    bool TrackReturn(bool completed);
    bool DeferIO();
    u32 RunRepeatBatch(u32 budget, u32& clocks);
    void CompleteFault(u16 old_task);
    void CopyStepException(I386_Run_Result& total) const;
    void CountInterruptShadow();
    void UpdateStepMode();
    bool ExecuteOPCode();
    bool ExecuteOPCodeDebug(const I386_State& before);
    void CaptureCheckedBytes();
    void RecordTrace(const I386_State& before);

    void SetRealModeSegment(I386_Segment_Register segment, u16 selector);
    void SetVM86Segment(I386_Segment_Register segment, u16 selector);
    void LoadRealCodeSegment(u16 selector, bool direct_jump);
    void UpdateSegmentFastPaths();
    void UpdateExecutionMode();
    void UpdateDebugState() const;
    static bool EqualName(const char* left, const char* right);

    u32 GetRegister(int index, int width) const;
    void SetRegister(int index, int width, u32 value);
    u8 GetRegister8(int index) const;
    void SetRegister8(int index, u8 value);
    u32 GetMask(int width) const;
    u32 GetSignBit(int width) const;
    u32 Truncate(u64 value, int width) const;
    s32 SignExtend(u32 value, int width) const;

    bool StartInstruction();
    void OpenCodeWindow(u32 eip);
    void CloseCodeWindow();
    bool DispatchOPCode();
    bool DispatchOPCode0F();
    bool PrefixSegment(u8 segment);
    bool PrefixOperandSize();
    bool PrefixAddressSize();
    bool PrefixLock();
    bool PrefixRepeat(u8 repeat);
    template<bool passive>
    bool FetchCode8(InstructionContext& instruction, u8& value);
    template<bool passive>
    bool FetchCode16(InstructionContext& instruction, u16& value);
    template<bool passive>
    bool FetchCode32(InstructionContext& instruction, u32& value);
    bool FetchCodeSlow8(InstructionContext& instruction, u8& value);
    template<bool passive>
    bool DecodeModRM(InstructionContext& instruction);
    template<bool passive>
    bool DecodeImmediate(InstructionContext& instruction);
    void CalculateEffectiveOffset(InstructionContext& instruction);
    bool DecodeOperands(bool modrm, int immediate_size);
    bool DecodeOperandsSlow(bool modrm, int immediate_size);
    bool HasDirectOperands(bool modrm, int immediate_size) const;
    u32 GetOperandsLength(bool modrm, int immediate_size, const u8* window, u32 available) const;
    bool StartExecution(u32 clocks);
    bool DecodeAndStart(bool modrm, int immediate_size, u32 register_clocks, u32 memory_clocks);
    void CommitEIP(const InstructionContext& instruction);
    bool BranchTo(u32 target, int width);
    bool IsLockAllowed(const InstructionContext& instruction) const;
    bool OPCodeHasModRM(bool two_byte, u8 opcode) const;
    static int GetEncodedImmediateSize(u8 encoding, u8 opcode, u8 reg, int operand_size, int address_size);
    int GetImmediateSize(bool two_byte, u8 opcode, u8 reg, int operand_size, int address_size) const;
    bool DecodeInstructionPassive(InstructionContext& instruction, u32 eip, bool default32);
    void FillDecodeState(const InstructionContext& instruction, const u8* bytes, u32 length,
        I386_Decode_State& state) const;

    u32 GetMultiplyClocks(u32 multiplier, int width, bool signed_multiplier, bool memory_operand) const;
    u32 GetBitScanClocks(u32 value, int width, bool reverse) const;
    u32 GetStringClocks(bool continuation) const;
    u32 GetTaskSwitchClocks(const Descriptor& descriptor, const TaskState& state, int switch_type, bool via_gate) const;
    u32 GetNextInstructionComponents() const;
    u32 GetNextInstructionComponentsChecked() const;
    u32 CountInstructionComponents(const u8* bytes, u32 count, bool& truncated) const;
    bool TranslateCodePassive(u32 linear, u32& physical) const;
    bool FetchTimingByte(TimingCursor& cursor, u8& value) const;

    u32 Add(u32 left, u32 right, u32 carry, int width);
    u32 Sub(u32 left, u32 right, u32 borrow, int width);
    u32 Logic(u32 value, int width);
    u32 ALU(int operation, u32 left, u32 right, int width);
    void SetSZP(u32 value, int width);
    u32 GetSZP(u32 value, int width) const;
    bool CheckCondition(int condition) const;
    template<int width> u32 RotateShift(u32 value, int shift_operation, u32 count);

    void SetBusContext(GT_Bus_Access_Context& context);
    void UpdateMemoryMode();
    void UpdateUserMode();
    void SetSlowMemory(bool slow);
    void SetUserMode(bool user);
    void RefreshMemoryPointers();
    const u8* GetReadHost(u32 linear, u32 size);
    u8* GetWriteHost(u32 linear, u32 size);
    u8* GetRMWHost(int segment, u32 offset, int width);
    u32 LoadHost(const u8* data, int width) const;
    void StoreHost(u8* data, u32 value, int width);
    u32 ReadPhysical(u32 physical, u32 size, GT_Bus_Access_Context& context);
    void WritePhysical(u32 physical, u32 value, u32 size, GT_Bus_Access_Context& context);
    bool ReadRM(const InstructionContext& instruction, int width, GT_Bus_Access_Context& context, u32& value);
    bool WriteRM(const InstructionContext& instruction, int width, u32 value, GT_Bus_Access_Context& context);
    bool ReadMemory(int segment, u32 offset, int width, GT_Bus_Access_Context& context, u32& value, bool stack = false);
    bool WriteMemory(int segment, u32 offset, int width, u32 value, GT_Bus_Access_Context& context, bool stack = false);
    bool ReadMemorySlow(int segment, u32 offset, int width, GT_Bus_Access_Context& context, u32& value, bool stack);
    bool WriteMemorySlow(int segment, u32 offset, int width, u32 value, GT_Bus_Access_Context& context, bool stack);
    bool ReadLinear(u32 linear, int width, GT_Bus_Access_Context& context, u32& value, bool supervisor);
    bool WriteLinear(u32 linear, int width, u32 value, GT_Bus_Access_Context& context, bool supervisor);
    bool ProbeMemory(int segment, u32 offset, u32 size, bool write, GT_Bus_Access_Context& context);
    bool CheckMemoryAccess(int segment, u32 offset, u32 size, bool write, GT_Bus_Access_Context& context);
    bool CheckLinearAccess(u32 linear, u32 size, bool write, GT_Bus_Access_Context& context, bool supervisor = false);
    bool LogicalToLinear(int segment, u32 offset, u32 size, bool write, bool stack, u32& linear, bool execute = false);
    bool LogicalToLinearProtected(int segment, u32 offset, u32 size, bool write, bool stack, bool execute, u32& linear);
    bool TranslateLinear(u32 linear, bool write, GT_Bus_Access_Context& context, u32& physical,
        bool supervisor = false);
    bool TranslateLinearPaged(u32 linear, bool write, GT_Bus_Access_Context& context, u32& physical, bool supervisor);
    bool TranslatePagedWay(u32 linear, bool write, GT_Bus_Access_Context& context, u32& physical, bool supervisor,
        u32 set, int way);
    bool TranslatePagedPassive(u32 linear, u32& physical) const;
    bool TranslateLinearForDebugger(u32 linear, u32& physical, u32& pde_address, u32& pte_address, u32& page_flags,
        char* reason, size_t reason_size) const;
    bool ReadPhysical32Passive(u32 physical, u32& value) const;
    const I386_Segment* FindSegment(u16 selector) const;
    static bool IsValidSegmentOffset(const I386_Segment& segment, u32 offset, bool null_unusable);

    int FindTLBWay(u32 set, u32 linear_page) const;
    const TLBEntry* FindTLBHost(u32 linear, bool write);
    const TLBEntry& GetRecentTLBEntry(u32 linear) const;
    void CacheTLBPages(TLBEntry& entry);
    void TouchTLBWay(u32 set, u32 way);
    void ResetTLBReplacement();
    void FlushTLB();
    void TestTLB();

    template<int width> bool StackPush(u32 value);
    template<int width> bool StackPop(u32& value);
    bool StackPushSized(u32 value, int width, GT_Bus_Access_Context& context, int write_width = 0);
    bool StackPopSized(u32& value, int width, GT_Bus_Access_Context& context, int read_width = 0);
    u32 GetStackPointer() const;
    void SetStackPointer(u32 value);
    int GetStackAddressSize() const;
    u32 GetFarTransferLimit() const;
    bool StackHasRoom(u32 limit, u16 attributes, u32 stack, u32 bytes) const;
    bool CheckStackFrame(u32 base, u16 attributes, u32 stack, u32 items, int width, bool supervisor,
        GT_Bus_Access_Context& context);

    bool LoadSegment(int segment, u16 selector, GT_Bus_Access_Context& context);
    bool LoadRealSegment(int segment, u16 selector);
    bool LoadProtectedSegment(int segment, u16 selector, GT_Bus_Access_Context& context);
    bool ReadDescriptor(u16 selector, Descriptor& descriptor, GT_Bus_Access_Context& context, u8 fault_vector = 13);
    bool ReadDescriptorNoFault(u16 selector, Descriptor& descriptor, GT_Bus_Access_Context& context, bool& valid);
    bool ReadGDTDescriptor(u16 selector, Descriptor& descriptor, GT_Bus_Access_Context& context, u8 fault_vector = 13);
    void DecodeDescriptor(u16 selector, u32 address, u32 low, u32 high, Descriptor& descriptor) const;
    void LoadDescriptorCache(u16 selector, const Descriptor& descriptor, I386_Segment& segment);
    void ClearSegmentCache(u16 selector, I386_Segment& segment);
    bool SetDescriptorAccessed(const Descriptor& descriptor, GT_Bus_Access_Context& context);
    bool SetDescriptorType(const Descriptor& descriptor, u8 type, GT_Bus_Access_Context& context);
    void ValidateDataSegmentsForPrivilege(u8 privilege);

    bool FarTransfer(u16 selector, u32 offset, int width, bool direct_jump = false);
    bool ProtectedFarTransfer(u16 selector, u32 offset, int width, bool call, u32 return_eip,
        GT_Bus_Access_Context& context, u64& clocks, bool indirect);
    bool ProtectedFarReturn(int width, u16 adjustment, GT_Bus_Access_Context& context, u64& clocks);
    bool ProtectedInterruptReturn(int width, GT_Bus_Access_Context& context, u64& clocks);
    bool TaskSwitch(u16 selector, const Descriptor& descriptor, int switch_type, u32 return_eip,
        GT_Bus_Access_Context& context, u64& clocks, bool via_gate, bool has_error_code = false, u32 error_code = 0,
        bool fault = false);
    bool TaskReturn(u32 return_eip, GT_Bus_Access_Context& context, u64& clocks);
    bool ReadTaskState(const Descriptor& descriptor, TaskState& state, GT_Bus_Access_Context& context);
    bool SaveTaskState(const I386_Segment& task, u32 return_eip, u32 saved_eflags, GT_Bus_Access_Context& context);
    bool LoadTaskSegments(const TaskState& state, GT_Bus_Access_Context& context);

    bool RaiseException(u8 vector, u8 exception_class, bool has_error_code = false, u32 error_code = 0);
    bool ResolveException(GT_Bus_Access_Context& context, I386_Run_Result& result);
    bool DeliverException(const I386_Pending_Exception& exception, u32 return_eip, GT_Bus_Access_Context& context,
        I386_Run_Result& result);
    bool CausesDoubleFault(u8 first, u8 second) const;
    bool EnterInterrupt(u8 vector, u32 return_eip, GT_Bus_Access_Context& context, bool software = false,
        bool has_error_code = false, u32 error_code = 0, bool fault = false, bool external = false, u64* clocks = NULL,
        I386_Run_Result* run_result = NULL);
    bool EnterProtectedInterrupt(u8 vector, u32 return_eip, GT_Bus_Access_Context& context, bool software,
        bool has_error_code, u32 error_code, bool fault, u64* clocks, I386_Run_Result* run_result,
        u32& saved_return_eip);
    bool ReadInterruptDescriptor(u8 vector, Descriptor& descriptor, GT_Bus_Access_Context& context);
    bool ReadPrivilegeStack(u8 privilege, u32& stack, u16& selector, GT_Bus_Access_Context& context);
    bool CheckIOPermission(u16 port, int width, GT_Bus_Access_Context& context, bool& allowed);
    u8 GetIOPrivilegeLevel() const;
    bool CheckInstructionBreakpoint();
    void RecordDataBreakpoints(u32 linear, u32 size, bool write);
    void RecordDebuggerAccess(u32 linear, u32 size, bool write);
    void ResetVBlankWatch();
    void RecordDebuggerIO(u16 port, u32 value, u32 size, bool write);
    void RecordDebuggerInterrupt(u8 vector, bool software, bool external, u32 from, bool has_error_code, u32 error_code);
    void TraceInstruction();
    NO_INLINE void TraceStep(I386_State& before);
    void RecordDebuggerHit(bool interrupt, u8 type, u8 space, u32 address, u32 size, u8 vector, u8 source,
        u8 line = 0xFF);

    bool DecodeInstructionForDebugger(const I386_Segment& code_segment, u32 eip, I386_Decode_State& state);
    void DisassembleAhead(const I386_Segment& code_segment, u32 start_eip, int count, int depth, int& branch_budget);

    template<int operation, int form, int width> bool OPCodes_ALU();
    template<int width, bool sign_extend> bool OPCodes_ALU_Immediate();
    template<int width, bool load> bool OPCodes_MOV_RM();
    template<int width> bool OPCodes_MOV_Immediate();
    template<int width, bool decrement> bool OPCodes_INC_DEC_Register();
    template<int condition, bool near_jump> bool OPCodes_Jcc();
    template<int width> bool OPCodes_PUSH_Register();
    template<int width> bool OPCodes_POP_Register();
    template<int width> bool OPCodes_PUSH_Immediate();
    template<int width> bool OPCodes_CALL_Near();
    template<int width> bool OPCodes_RET_Near();
    template<int width> bool OPCodes_TEST_RM();
    template<int width> bool OPCodes_LEA();
    template<int width> bool OPCodes_MOV_RM_Immediate();
    template<int width, int source> bool OPCodes_Group2();
    template<int width> bool OPCodes_Group3();
    template<int width> bool OPCodes_Group5();

    bool OPCodes_Invalid();
    bool OPCodes_PUSH_Segment();
    bool OPCodes_POP_Segment();
    bool OPCodes_DAA();
    bool OPCodes_DAS();
    bool OPCodes_AAA();
    bool OPCodes_AAS();
    bool OPCodes_PUSHA();
    bool OPCodes_POPA();
    bool OPCodes_BOUND();
    bool OPCodes_ARPL();
    bool OPCodes_IMUL_Immediate();
    bool OPCodes_String();
    bool OPCodes_XCHG_RM();
    bool OPCodes_MOV_RM_Segment();
    bool OPCodes_MOV_Segment_RM();
    bool OPCodes_POP_RM();
    bool OPCodes_NOP();
    bool OPCodes_XCHG_Accumulator();
    bool OPCodes_CBW_CWDE();
    bool OPCodes_CWD_CDQ();
    bool OPCodes_CALL_Far();
    bool OPCodes_WAIT();
    bool OPCodes_PUSHF();
    bool OPCodes_POPF();
    bool OPCodes_SAHF();
    bool OPCodes_LAHF();
    bool OPCodes_MOV_Moffs();
    bool OPCodes_TEST_Accumulator();
    bool OPCodes_LES_LDS();
    bool OPCodes_ENTER();
    bool OPCodes_LEAVE();
    bool OPCodes_RET_Far();
    bool OPCodes_INT();
    bool OPCodes_IRET();
    bool OPCodes_AAM();
    bool OPCodes_AAD();
    bool OPCodes_SALC();
    bool OPCodes_XLAT();
    bool OPCodes_Escape();
    bool OPCodes_LOOP();
    bool OPCodes_JCXZ();
    bool OPCodes_IN();
    bool OPCodes_OUT();
    bool OPCodes_JMP_Near();
    bool OPCodes_JMP_Far();
    bool OPCodes_JMP_Short();
    bool OPCodes_HLT();
    bool OPCodes_CMC();
    bool OPCodes_CLC();
    bool OPCodes_STC();
    bool OPCodes_CLI();
    bool OPCodes_STI();
    bool OPCodes_CLD();
    bool OPCodes_STD();
    bool OPCodes_Group4();

    bool OPCodes_TEST_RM_Immediate(int width, u32 operand);
    bool OPCodes_NOT_RM(int width, u32 operand);
    bool OPCodes_NEG_RM(int width, u32 operand);
    bool OPCodes_MUL_RM(int width, u32 operand);
    bool OPCodes_IMUL_RM(int width, u32 operand);
    bool OPCodes_DIV_RM(int width, u32 operand);
    bool OPCodes_IDIV_RM(int width, u32 operand);
    bool OPCodes_INC_DEC_RM(int width, bool decrement);
    bool OPCodes_INC_RM(int width);
    bool OPCodes_DEC_RM(int width);
    bool OPCodes_Near_Transfer_RM(int width, bool call);
    bool OPCodes_CALL_Near_RM(int width);
    bool OPCodes_JMP_Near_RM(int width);
    bool OPCodes_Far_Transfer_Memory(int width, bool call);
    bool OPCodes_CALL_Far_Memory(int width);
    bool OPCodes_JMP_Far_Memory(int width);
    bool OPCodes_PUSH_RM(int width);

    bool ExecuteStringElement(bool& repeat);
    bool ContinueRepeat();
    void PrepareString();
    bool OPCodes_INS(int width, u32 destination_offset);
    bool OPCodes_OUTS(int width, int source_segment, u32 source_offset);
    bool OPCodes_MOVS(int width, int source_segment, u32 source_offset, u32 destination_offset);
    bool OPCodes_CMPS(int width, int source_segment, u32 source_offset, u32 destination_offset);
    bool OPCodes_STOS(int width, u32 destination_offset);
    bool OPCodes_LODS(int width, int source_segment, u32 source_offset);
    bool OPCodes_SCAS(int width, u32 destination_offset);

    bool OPCodes0F_Group6();
    bool OPCodes0F_Group7();
    bool OPCodes0F_LAR_LSL();
    bool OPCodes0F_CLTS();
    bool OPCodes0F_LOADALL();
    bool OPCodes0F_MOV_Special();
    bool OPCodes0F_SETcc();
    bool OPCodes0F_PUSH_FS_GS();
    bool OPCodes0F_POP_FS_GS();
    bool OPCodes0F_Bit_Register();
    bool OPCodes0F_BitOperation(int operation, u32 bit_index);
    bool OPCodes0F_DoubleShift();
    bool OPCodes0F_IMUL();
    bool OPCodes0F_LoadFarPointer();
    template<int width, int source_width, bool sign_extend> bool OPCodes0F_MOVX();
    bool OPCodes0F_Group8();
    bool OPCodes0F_BitScan();

    bool StoreSystemSelector(InstructionContext& instruction, GT_Bus_Access_Context& context, u16 selector);
    bool LoadSystemSelector(InstructionContext& instruction, GT_Bus_Access_Context& context, bool load_task_register);
    bool VerifySegmentPermission(InstructionContext& instruction, GT_Bus_Access_Context& context, bool write);
    bool StoreDescriptorTable(InstructionContext& instruction, GT_Bus_Access_Context& context, bool interrupt_table);
    bool LoadDescriptorTable(InstructionContext& instruction, GT_Bus_Access_Context& context, bool interrupt_table);
    bool MoveControlRegister(u8 special_index, u8 general_index, bool write_special, u32 value);
    bool MoveDebugRegister(u8 special_index, u8 general_index, bool write_special, u32 value);
    bool MoveTestRegister(u8 special_index, u8 general_index, bool write_special, u32 value);
    bool OPCodes_SLDT();
    bool OPCodes_STR();
    bool OPCodes_LLDT();
    bool OPCodes_LTR();
    bool OPCodes_VERR();
    bool OPCodes_VERW();
    bool OPCodes_SGDT();
    bool OPCodes_SIDT();
    bool OPCodes_LGDT();
    bool OPCodes_LIDT();
    bool OPCodes_SMSW();
    bool OPCodes_LMSW();

    bool OPCode0x00(); bool OPCode0x01(); bool OPCode0x02(); bool OPCode0x03();
    bool OPCode0x04(); bool OPCode0x05(); bool OPCode0x06(); bool OPCode0x07();
    bool OPCode0x08(); bool OPCode0x09(); bool OPCode0x0A(); bool OPCode0x0B();
    bool OPCode0x0C(); bool OPCode0x0D(); bool OPCode0x0E(); bool OPCode0x0F();
    bool OPCode0x10(); bool OPCode0x11(); bool OPCode0x12(); bool OPCode0x13();
    bool OPCode0x14(); bool OPCode0x15(); bool OPCode0x16(); bool OPCode0x17();
    bool OPCode0x18(); bool OPCode0x19(); bool OPCode0x1A(); bool OPCode0x1B();
    bool OPCode0x1C(); bool OPCode0x1D(); bool OPCode0x1E(); bool OPCode0x1F();
    bool OPCode0x20(); bool OPCode0x21(); bool OPCode0x22(); bool OPCode0x23();
    bool OPCode0x24(); bool OPCode0x25(); bool OPCode0x26(); bool OPCode0x27();
    bool OPCode0x28(); bool OPCode0x29(); bool OPCode0x2A(); bool OPCode0x2B();
    bool OPCode0x2C(); bool OPCode0x2D(); bool OPCode0x2E(); bool OPCode0x2F();
    bool OPCode0x30(); bool OPCode0x31(); bool OPCode0x32(); bool OPCode0x33();
    bool OPCode0x34(); bool OPCode0x35(); bool OPCode0x36(); bool OPCode0x37();
    bool OPCode0x38(); bool OPCode0x39(); bool OPCode0x3A(); bool OPCode0x3B();
    bool OPCode0x3C(); bool OPCode0x3D(); bool OPCode0x3E(); bool OPCode0x3F();
    bool OPCode0x40(); bool OPCode0x41(); bool OPCode0x42(); bool OPCode0x43();
    bool OPCode0x44(); bool OPCode0x45(); bool OPCode0x46(); bool OPCode0x47();
    bool OPCode0x48(); bool OPCode0x49(); bool OPCode0x4A(); bool OPCode0x4B();
    bool OPCode0x4C(); bool OPCode0x4D(); bool OPCode0x4E(); bool OPCode0x4F();
    bool OPCode0x50(); bool OPCode0x51(); bool OPCode0x52(); bool OPCode0x53();
    bool OPCode0x54(); bool OPCode0x55(); bool OPCode0x56(); bool OPCode0x57();
    bool OPCode0x58(); bool OPCode0x59(); bool OPCode0x5A(); bool OPCode0x5B();
    bool OPCode0x5C(); bool OPCode0x5D(); bool OPCode0x5E(); bool OPCode0x5F();
    bool OPCode0x60(); bool OPCode0x61(); bool OPCode0x62(); bool OPCode0x63();
    bool OPCode0x64(); bool OPCode0x65(); bool OPCode0x66(); bool OPCode0x67();
    bool OPCode0x68(); bool OPCode0x69(); bool OPCode0x6A(); bool OPCode0x6B();
    bool OPCode0x6C(); bool OPCode0x6D(); bool OPCode0x6E(); bool OPCode0x6F();
    bool OPCode0x70(); bool OPCode0x71(); bool OPCode0x72(); bool OPCode0x73();
    bool OPCode0x74(); bool OPCode0x75(); bool OPCode0x76(); bool OPCode0x77();
    bool OPCode0x78(); bool OPCode0x79(); bool OPCode0x7A(); bool OPCode0x7B();
    bool OPCode0x7C(); bool OPCode0x7D(); bool OPCode0x7E(); bool OPCode0x7F();
    bool OPCode0x80(); bool OPCode0x81(); bool OPCode0x82(); bool OPCode0x83();
    bool OPCode0x84(); bool OPCode0x85(); bool OPCode0x86(); bool OPCode0x87();
    bool OPCode0x88(); bool OPCode0x89(); bool OPCode0x8A(); bool OPCode0x8B();
    bool OPCode0x8C(); bool OPCode0x8D(); bool OPCode0x8E(); bool OPCode0x8F();
    bool OPCode0x90(); bool OPCode0x91(); bool OPCode0x92(); bool OPCode0x93();
    bool OPCode0x94(); bool OPCode0x95(); bool OPCode0x96(); bool OPCode0x97();
    bool OPCode0x98(); bool OPCode0x99(); bool OPCode0x9A(); bool OPCode0x9B();
    bool OPCode0x9C(); bool OPCode0x9D(); bool OPCode0x9E(); bool OPCode0x9F();
    bool OPCode0xA0(); bool OPCode0xA1(); bool OPCode0xA2(); bool OPCode0xA3();
    bool OPCode0xA4(); bool OPCode0xA5(); bool OPCode0xA6(); bool OPCode0xA7();
    bool OPCode0xA8(); bool OPCode0xA9(); bool OPCode0xAA(); bool OPCode0xAB();
    bool OPCode0xAC(); bool OPCode0xAD(); bool OPCode0xAE(); bool OPCode0xAF();
    bool OPCode0xB0(); bool OPCode0xB1(); bool OPCode0xB2(); bool OPCode0xB3();
    bool OPCode0xB4(); bool OPCode0xB5(); bool OPCode0xB6(); bool OPCode0xB7();
    bool OPCode0xB8(); bool OPCode0xB9(); bool OPCode0xBA(); bool OPCode0xBB();
    bool OPCode0xBC(); bool OPCode0xBD(); bool OPCode0xBE(); bool OPCode0xBF();
    bool OPCode0xC0(); bool OPCode0xC1(); bool OPCode0xC2(); bool OPCode0xC3();
    bool OPCode0xC4(); bool OPCode0xC5(); bool OPCode0xC6(); bool OPCode0xC7();
    bool OPCode0xC8(); bool OPCode0xC9(); bool OPCode0xCA(); bool OPCode0xCB();
    bool OPCode0xCC(); bool OPCode0xCD(); bool OPCode0xCE(); bool OPCode0xCF();
    bool OPCode0xD0(); bool OPCode0xD1(); bool OPCode0xD2(); bool OPCode0xD3();
    bool OPCode0xD4(); bool OPCode0xD5(); bool OPCode0xD6(); bool OPCode0xD7();
    bool OPCode0xD8(); bool OPCode0xD9(); bool OPCode0xDA(); bool OPCode0xDB();
    bool OPCode0xDC(); bool OPCode0xDD(); bool OPCode0xDE(); bool OPCode0xDF();
    bool OPCode0xE0(); bool OPCode0xE1(); bool OPCode0xE2(); bool OPCode0xE3();
    bool OPCode0xE4(); bool OPCode0xE5(); bool OPCode0xE6(); bool OPCode0xE7();
    bool OPCode0xE8(); bool OPCode0xE9(); bool OPCode0xEA(); bool OPCode0xEB();
    bool OPCode0xEC(); bool OPCode0xED(); bool OPCode0xEE(); bool OPCode0xEF();
    bool OPCode0xF0(); bool OPCode0xF1(); bool OPCode0xF2(); bool OPCode0xF3();
    bool OPCode0xF4(); bool OPCode0xF5(); bool OPCode0xF6(); bool OPCode0xF7();
    bool OPCode0xF8(); bool OPCode0xF9(); bool OPCode0xFA(); bool OPCode0xFB();
    bool OPCode0xFC(); bool OPCode0xFD(); bool OPCode0xFE(); bool OPCode0xFF();

    bool OPCode0F_0x00(); bool OPCode0F_0x01(); bool OPCode0F_0x02(); bool OPCode0F_0x03();
    bool OPCode0F_0x06(); bool OPCode0F_0x07(); bool OPCode0F_0x10(); bool OPCode0F_0x11(); bool OPCode0F_0x12();
    bool OPCode0F_0x13(); bool OPCode0F_0x20(); bool OPCode0F_0x21(); bool OPCode0F_0x22();
    bool OPCode0F_0x23(); bool OPCode0F_0x24(); bool OPCode0F_0x26(); bool OPCode0F_0x80();
    bool OPCode0F_0x81(); bool OPCode0F_0x82(); bool OPCode0F_0x83(); bool OPCode0F_0x84();
    bool OPCode0F_0x85(); bool OPCode0F_0x86(); bool OPCode0F_0x87(); bool OPCode0F_0x88();
    bool OPCode0F_0x89(); bool OPCode0F_0x8A(); bool OPCode0F_0x8B(); bool OPCode0F_0x8C();
    bool OPCode0F_0x8D(); bool OPCode0F_0x8E(); bool OPCode0F_0x8F(); bool OPCode0F_0x90();
    bool OPCode0F_0x91(); bool OPCode0F_0x92(); bool OPCode0F_0x93(); bool OPCode0F_0x94();
    bool OPCode0F_0x95(); bool OPCode0F_0x96(); bool OPCode0F_0x97(); bool OPCode0F_0x98();
    bool OPCode0F_0x99(); bool OPCode0F_0x9A(); bool OPCode0F_0x9B(); bool OPCode0F_0x9C();
    bool OPCode0F_0x9D(); bool OPCode0F_0x9E(); bool OPCode0F_0x9F(); bool OPCode0F_0xA0();
    bool OPCode0F_0xA1(); bool OPCode0F_0xA3(); bool OPCode0F_0xA4(); bool OPCode0F_0xA5();
    bool OPCode0F_0xA8(); bool OPCode0F_0xA9(); bool OPCode0F_0xAB(); bool OPCode0F_0xAC();
    bool OPCode0F_0xAD(); bool OPCode0F_0xAF(); bool OPCode0F_0xB2(); bool OPCode0F_0xB3();
    bool OPCode0F_0xB4(); bool OPCode0F_0xB5(); bool OPCode0F_0xB6(); bool OPCode0F_0xB7();
    bool OPCode0F_0xBA(); bool OPCode0F_0xBB(); bool OPCode0F_0xBC(); bool OPCode0F_0xBD();
    bool OPCode0F_0xBE(); bool OPCode0F_0xBF();

private:
    const u8* const* m_read_pages;
    u8* const* m_write_pages;
    u32 m_memory_generation;
    bool m_batch_mode;
    u32 m_batch_start_pc;
    bool m_slow_memory;
    bool m_user_mode;
    u8 m_default_size;
    bool m_stack32;
    s64 m_code_limit;
    s64 m_read_limits[I386_SEGMENT_COUNT];
    s64 m_write_limits[I386_SEGMENT_COUNT];
    GT_Bus_Access_Context* m_bus_context;
    StepState m_step;

    I386_State m_state;
    bool m_external_event;

    u8 m_debug_data_breakpoints;

    I386_Pending_Exception m_exception;
    StringContext m_string;

    const u8* m_code_window;
    u32 m_code_window_eip;
    u32 m_code_window_size;
    const u8* m_fetch_pointer;
    u32 m_fetch_remaining;
    u8 m_address_clocks;
    bool m_debug_step;
    bool m_step_slow;
    InstructionContext m_instruction;
    InstructionContext m_instruction_defaults;

    Memory* m_memory;
    IO* m_io;

    bool m_trace_enabled;

    u32 m_checked_eip;
    u8 m_checked_bytes[GT_I386_MAX_INSTRUCTION_LENGTH];
    u8 m_checked_count;
    bool m_checked_valid;
    bool m_instruction_restored;

    const u8* m_passive_bytes;
    u32 m_passive_count;
    u32 m_passive_index;

    I386_Trace_Entry* m_trace;
    int m_trace_count;
    I386_Run_Result m_step_exception;

    mutable I386_Debug_State m_debug_state;

    u8 m_segment_access_flags[I386_SEGMENT_COUNT];
    TLBEntry m_tlb[I386_TLB_SIZE];
    u32 m_tlb_used;
    u8 m_tlb_plru[I386_TLB_SETS];
    u8 m_tlb_mru[I386_TLB_SETS];

    std::map<u32, I386_Disassembler_Record> m_disassembler_records;
    I386_Disassembler_Record** m_disassembler_cache;
    std::vector<I386_Breakpoint> m_breakpoints;
    std::vector<I386_Interrupt_Breakpoint> m_interrupt_breakpoints;
    u16 m_irq_breakpoints;
    u16 m_irq_breakpoints_disabled;
    int m_external_line;
    std::vector<I386_CallStackEntry> m_disassembler_call_stack;
    I386_Breakpoint_Hit m_breakpoint_hit_info;
    bool m_debugger_checks;
    bool m_debugger_memory_checks;
    bool m_debugger_io_checks;
    bool m_debugger_interrupt_checks;
    bool m_debugger_hit_pending;
    TraceLogger* m_trace_logger;
    Profiler* m_profiler;
    bool m_trace_internal;
    bool m_trace_cpu;
    bool m_profiler_active;
    bool m_vblank_watch_read;
    bool m_vblank_watch_write;
    u32 m_vblank_watch_address;
    bool m_vblank_watch_hit;
    bool m_vblank_watch_armed;
    u32 m_vblank_watch_misses;

    u32 m_run_to_breakpoint;
    u32 m_breakpoint_hit_address;
    u32 m_step_call_return_linear;
    bool m_step_call;
    bool m_task_call_entered;
    bool m_run_to_breakpoint_enabled;
    bool m_breakpoint_hit;
    bool m_run_to_hit;
};

static const u32 k_i386_disassembler_cache_size = 0x4000;

static const u8 k_i386_segment_fast_read = 0x01;
static const u8 k_i386_segment_fast_write = 0x02;
static const u8 k_i386_segment_fast_execute = 0x04;

static const u8 k_i386_tlb_valid = 0x01;
static const u8 k_i386_tlb_user = 0x02;
static const u8 k_i386_tlb_writable = 0x04;
static const u8 k_i386_tlb_dirty = 0x08;

static const u8 k_i386_immediate_byte = 1;
static const u8 k_i386_immediate_word = 2;
static const u8 k_i386_immediate_enter = 3;
static const u8 k_i386_immediate_operand = 4;
static const u8 k_i386_immediate_far = 5;
static const u8 k_i386_immediate_address = 6;
static const u8 k_i386_immediate_group3 = 7;
static const u8 k_i386_immediate_mask = 0x0F;
static const u8 k_i386_encoding_prefix = 0x40;
static const u8 k_i386_encoding_modrm = 0x80;

#include "i386_memory_inline.h"
#include "i386_inline.h"
#include "i386_decode_inline.h"
#include "i386_run_inline.h"

#endif /* I386_H */
