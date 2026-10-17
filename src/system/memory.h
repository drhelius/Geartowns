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

#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <iostream>
#include "../common/common.h"
#include "../common/debug_memory.h"
#include "../i386/dynarec_platform.h"

class StateSerializer;
class TraceLogger;

typedef u8 (*GT_Memory_Read8_Fn)(void* device, u32 offset);
typedef void (*GT_Memory_Write8_Fn)(void* device, u32 offset, u8 value);
typedef void (*GT_Memory_Write16_Fn)(void* device, u32 offset, u16 value);
typedef void (*GT_Memory_Write32_Fn)(void* device, u32 offset, u32 value);
typedef void (*GT_Memory_Code_Write_Fn)(void* context, u32 offset, u32 size);

static const u32 k_memory_dma_slots = 4096;

class Memory
{
public:
    struct Memory_State
    {
        u8* main_ram;
        u8 cmos[GT_CMOS_SIZE];
        bool main_memory;
        bool boot_ram;
        bool dictionary;
        u8 dictionary_bank;
        u8 card_bank;
        bool card_attribute;
    };

public:
    Memory();
    ~Memory();
    void Init();
    void SetTraceLogger(TraceLogger* trace_logger);
    void Reset();
    void ResetMapping();
    void SetFeatures(u32 features);
    bool Decodes(u16 port) const;
    u8 ReadMappingControl(u16 port) const;
    void WriteMappingControl(u16 port, u8 value);

    u8 Read8Physical(u32 physical, GT_Bus_Access_Context& context);
    u16 Read16Physical(u32 physical, GT_Bus_Access_Context& context);
    u32 Read32Physical(u32 physical, GT_Bus_Access_Context& context);
    void Write8Physical(u32 physical, u8 value, GT_Bus_Access_Context& context);
    void Write16Physical(u32 physical, u16 value, GT_Bus_Access_Context& context);
    void Write32Physical(u32 physical, u32 value, GT_Bus_Access_Context& context);

    void ClearDebugRegions();
    bool RegisterDebugRegion(int id, const char* name, const u8* read_data, u8* write_data, u32 size, u32 physical_base, u32 flags);
    bool RegisterHandlerRegion(int id, const char* name, u32 size, u32 physical_base, u32 flags, void* device,
        GT_Memory_Read8_Fn read8, GT_Memory_Write8_Fn write8, GT_Memory_Read8_Fn peek8);
    bool SetRegionWideHandlers(int id, GT_Memory_Write16_Fn write16, GT_Memory_Write32_Fn write32);
    bool SetRegionMapped(int id, bool mapped);
    bool SetRegionData(int id, const u8* read_data, u8* write_data);
    bool SetRegionHandlerRange(int id, u32 offset, u32 size, bool reads);
    void SynchronizeAccess();
    int GetDebugRegionCount() const;
    bool GetDebugRegion(int index, GT_Debug_Memory_Region& region) const;
    void DebugReadRegionBlock(int id, u32 offset, u8* data, GT_Debug_Memory_Status* status, u32 size) const;
    bool DebugWriteRegionBlock(int id, u32 offset, const u8* data, u32 size);

    bool TryPeekPhysical(u32 physical, u8& value) const;
    const u8* GetPhysicalReadSpan(u32 physical, u32 size) const;
    bool TryPeekPhysicalBlock(u32 physical, u8* data, u32 size) const;
    void DebugReadPhysicalBlock(u32 physical, u8* data, GT_Debug_Memory_Status* status, u32 size) const;
    void DebugReadBusBlock(u32 bus_address, u8* data, GT_Debug_Memory_Status* status, u32 size) const;
    bool DebugWritePhysicalBlock(u32 physical, const u8* data, u32 size);
    bool DebugWriteBusBlock(u32 bus_address, const u8* data, u32 size);
    bool DebugTranslatePhysical(u32 physical, GT_Debug_Memory_Translation& translation) const;
    bool DebugTranslateBus(u32 bus_address, GT_Debug_Memory_Translation& translation) const;

    void PrepareCPUMap();
    const u8* const* GetCPUReadPages() const;
    u8* const* GetCPUWritePages() const;

    void SetPhysicalAddressMask(u32 mask);
    void SetDMAWindows(const GT_Machine_DMA_Window* windows);
    u32 TranslateDMAAddress(u32 address) const;
    void SetNMIVectorProtect(bool protect);
    u32 GetMapGeneration() const;
    u32 GetRegionGeneration() const;
#if !defined(GT_DISABLE_DYNAREC)
    void SetCodeWriteCallback(void* context, GT_Memory_Code_Write_Fn callback);
    void SetCodePage(u32 page, bool code);
    bool IsReadOnlyHost(const u8* host) const;
#endif
    u64 GetDebugSnapshotId() const;
    void InvalidateDebugSnapshot();

    u8* GetWorkingRAM();
    size_t GetWorkingRAMSize() const;
    u8* GetMainRAM();
    u32 GetMainRAMSize() const;
    void SetMainRAMSize(u32 size);
    u8* GetCMOS();
    void ResetCMOS(int floppy_drives);
    bool LoadCMOSFile(const u8* data, size_t size, int floppy_drives);
    void SaveCMOSFile(u8* data) const;
    u8 ReadCMOS(u32 index) const;
    void WriteCMOS(u32 index, u8 value);
    u8* GetVideoRAM();
    size_t GetVideoRAMSize() const;
    Memory_State* GetState();
    void SaveState(std::ostream& stream);
    void LoadState(std::istream& stream, int version = GT_SAVESTATE_VERSION);

private:
    struct DebugRegion
    {
        GT_Debug_Memory_Region info;
        const u8* read_data;
        u8* write_data;
        void* device;
        GT_Memory_Read8_Fn read8;
        GT_Memory_Write8_Fn write8;
        GT_Memory_Read8_Fn peek8;
        GT_Memory_Write16_Fn write16;
        GT_Memory_Write32_Fn write32;
        u32 handler_offset;
        u32 handler_size;
        bool handler_reads;
    };

    const DebugRegion* FindRegion(int id) const;
    DebugRegion* FindRegion(int id);
    const DebugRegion* FindMappedRegion(u32 bus_address) const;
    DebugRegion* FindMappedRegion(u32 bus_address);
    const DebugRegion* FindMappedSpan(u32 physical, u32 size) const;
    const DebugRegion* FindPageSpan(u32 physical, u32 size) const;
    bool IsHandlerSpan(const DebugRegion* region, u32 offset, u32 size) const;
    bool IsProtectedSpan(const DebugRegion* region, u32 offset, u32 size) const;
    GT_Debug_Memory_Status DebugReadRegion(int id, u32 offset, u8& value) const;
    GT_Debug_Memory_Status DebugReadBus(u32 bus_address, u8& value) const;
    GT_Debug_Memory_Status ReadRegion(const DebugRegion& region, u32 offset, u8& value) const;
    u8 ReadBus(u32 bus_address, GT_Bus_Access_Context& context);
    void WriteBus(u32 bus_address, u8 value, GT_Bus_Access_Context& context);
    void SynchronizeRegister(GT_Bus_Access_Context& context);
    u32 NormalizePhysicalAddress(u32 physical) const;
    bool IsCanonicalPage(u32 page) const;
    void UpdateRAMRegions();
    void UpdateCPUPage(u32 page);
    void UpdateCPUPage(u32 page, const DebugRegion* region);
    void RemapRange(u32 base, u32 size);
#if !defined(GT_DISABLE_DYNAREC)
    void NotifyCodeWrite(const DebugRegion* region, u32 offset, u32 size);
#endif
    void ApplyMapping();
    void ResetCMOSSetup();
    void Serialize(StateSerializer& serializer, int version);
    void SanitizeState();

private:
    const u8** m_cpu_read_pages;
    u8** m_cpu_write_pages;
    u8* m_cpu_page_regions;
    u32 m_cpu_map_generation;

    DebugRegion m_debug_regions[GT_DEBUG_MEMORY_MAX_REGIONS];
    int m_debug_region_count;
    u32 m_physical_address_mask;
    u32 m_features;
    u32 m_dma_map[k_memory_dma_slots];
    bool m_nmi_vector_protect;
    u32 m_map_generation;
    u32 m_region_generation;
    u64 m_debug_snapshot_id;

    u8* m_working_ram;
    size_t m_working_ram_size;
    u32 m_main_ram_size;
    Memory_State m_state;

    u8* m_video_ram;
    size_t m_video_ram_size;
    TraceLogger* m_trace_logger;
    GT_Bus_Access_Context* m_access_context;

#if !defined(GT_DISABLE_DYNAREC)
    u8* m_code_pages;
    u32 m_code_page_count;
    void* m_code_write_context;
    GT_Memory_Code_Write_Fn m_code_write_callback;
#endif
};

// The real mode NMI vector, interrupt 2
static const u32 k_memory_nmi_vector = 0x00008;
static const u32 k_memory_nmi_vector_size = 4;

#include "memory_inline.h"

#endif /* MEMORY_H */
