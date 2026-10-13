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

#include "exec_memory.h"

#if !defined(GT_DISABLE_DYNAREC)

#if defined(GT_DYNAREC_POSIX)
    #include <sys/mman.h>
    #include <unistd.h>
    #if defined(__linux__)
        #include <sys/syscall.h>
    #endif
    #if !defined(MAP_ANONYMOUS)
        #define MAP_ANONYMOUS MAP_ANON
    #endif
#elif defined(GT_DYNAREC_APPLE)
    #include <sys/mman.h>
    #include <pthread.h>
    #include <libkern/OSCacheControl.h>
#elif defined(GT_DYNAREC_WINDOWS)
    #include <windows.h>
#elif defined(GT_DYNAREC_LIBNX)
    #include <switch.h>
#elif defined(GT_DYNAREC_VITA)
    #include <psp2/kernel/sysmem.h>
#elif defined(GT_DYNAREC_GEKKO)
    #include <malloc.h>
    #include <ogc/cache.h>
#endif

ExecMemory::ExecMemory()
{
    InitPointer(m_write);
    InitPointer(m_execute);
    InitPointer(m_handle);
    m_size = 0;
}

ExecMemory::~ExecMemory()
{
    Release();
}

bool ExecMemory::IsReady() const
{
    return IsValidPointer(m_write);
}

u8* ExecMemory::GetWriteView() const
{
    return m_write;
}

u8* ExecMemory::GetExecuteView() const
{
    return m_execute;
}

u32 ExecMemory::GetSize() const
{
    return m_size;
}

#if defined(GT_DYNAREC_POSIX)

// A read, write and execute mapping, or two views of one shared memory file where that is refused
bool ExecMemory::Init(u32 size)
{
    Release();

    void* memory = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (memory != MAP_FAILED)
    {
        m_write = m_execute = (u8*)memory;
        m_size = size;
        return true;
    }

#if defined(__linux__) && defined(SYS_memfd_create)
    int fd = (int)syscall(SYS_memfd_create, "geartowns-dynarec", 0);

    if (fd < 0)
        return false;

    void* write = MAP_FAILED;
    void* execute = MAP_FAILED;

    if (ftruncate(fd, size) == 0)
    {
        write = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        execute = mmap(NULL, size, PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
    }

    close(fd);

    if (write == MAP_FAILED || execute == MAP_FAILED)
    {
        if (write != MAP_FAILED)
            munmap(write, size);

        if (execute != MAP_FAILED)
            munmap(execute, size);

        return false;
    }

    m_write = (u8*)write;
    m_execute = (u8*)execute;
    m_size = size;
    return true;
#else
    return false;
#endif
}

void ExecMemory::Release()
{
    if (IsValidPointer(m_write))
    {
        if (m_execute != m_write)
            munmap(m_execute, m_size);

        munmap(m_write, m_size);
    }

    InitPointer(m_write);
    InitPointer(m_execute);
    m_size = 0;
}

void ExecMemory::BeginWrite()
{
}

void ExecMemory::EndWrite(u32 offset, u32 size)
{
    __builtin___clear_cache((char*)m_execute + offset, (char*)m_execute + offset + size);
}

#elif defined(GT_DYNAREC_APPLE)

// One MAP_JIT region, writable or executable per thread on Apple silicon
bool ExecMemory::Init(u32 size)
{
    Release();

    void* memory = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);

    if (memory == MAP_FAILED)
        return false;

    m_write = m_execute = (u8*)memory;
    m_size = size;
    return true;
}

void ExecMemory::Release()
{
    if (IsValidPointer(m_write))
        munmap(m_write, m_size);

    InitPointer(m_write);
    InitPointer(m_execute);
    m_size = 0;
}

void ExecMemory::BeginWrite()
{
#if defined(__aarch64__)
    if (__builtin_available(macOS 11.0, *))
        pthread_jit_write_protect_np(0);
#endif
}

void ExecMemory::EndWrite(u32 offset, u32 size)
{
#if defined(__aarch64__)
    if (__builtin_available(macOS 11.0, *))
        pthread_jit_write_protect_np(1);
#endif
    sys_icache_invalidate(m_execute + offset, size);
}

#elif defined(GT_DYNAREC_WINDOWS)

bool ExecMemory::Init(u32 size)
{
    Release();

    void* memory = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    if (!IsValidPointer(memory))
        return false;

    m_write = m_execute = (u8*)memory;
    m_size = size;
    return true;
}

void ExecMemory::Release()
{
    if (IsValidPointer(m_write))
        VirtualFree(m_write, 0, MEM_RELEASE);

    InitPointer(m_write);
    InitPointer(m_execute);
    m_size = 0;
}

void ExecMemory::BeginWrite()
{
}

void ExecMemory::EndWrite(u32 offset, u32 size)
{
    FlushInstructionCache(GetCurrentProcess(), m_execute + offset, size);
}

#elif defined(GT_DYNAREC_LIBNX)

// Code memory with separate writable and executable views, the transitions keep the caches coherent
bool ExecMemory::Init(u32 size)
{
    Release();

    Jit* jit = new Jit;

    if (R_FAILED(jitCreate(jit, size)))
    {
        delete jit;
        return false;
    }

    m_handle = jit;
    m_write = (u8*)jitGetRwAddr(jit);
    m_execute = (u8*)jitGetRxAddr(jit);
    m_size = size;
    return true;
}

void ExecMemory::Release()
{
    if (IsValidPointer(m_handle))
    {
        Jit* jit = (Jit*)m_handle;
        jitClose(jit);
        delete jit;
    }

    InitPointer(m_handle);
    InitPointer(m_write);
    InitPointer(m_execute);
    m_size = 0;
}

void ExecMemory::BeginWrite()
{
    jitTransitionToWritable((Jit*)m_handle);
}

void ExecMemory::EndWrite(u32 offset, u32 size)
{
    UNUSED(offset);
    UNUSED(size);
    jitTransitionToExecutable((Jit*)m_handle);
}

#elif defined(GT_DYNAREC_VITA)

// A VM memory block, writable while its domain is open
bool ExecMemory::Init(u32 size)
{
    Release();

    size = (size + 0xFFFFF) & ~0xFFFFFU;
    SceUID block = sceKernelAllocMemBlockForVM("geartowns_dynarec", size);

    if (block < 0)
        return false;

    void* base = NULL;

    if (sceKernelGetMemBlockBase(block, &base) < 0)
    {
        sceKernelFreeMemBlock(block);
        return false;
    }

    m_handle = (void*)(intptr_t)block;
    m_write = m_execute = (u8*)base;
    m_size = size;
    return true;
}

void ExecMemory::Release()
{
    if (IsValidPointer(m_write))
        sceKernelFreeMemBlock((SceUID)(intptr_t)m_handle);

    InitPointer(m_handle);
    InitPointer(m_write);
    InitPointer(m_execute);
    m_size = 0;
}

void ExecMemory::BeginWrite()
{
    sceKernelOpenVMDomain();
}

void ExecMemory::EndWrite(u32 offset, u32 size)
{
    sceKernelSyncVMDomain((SceUID)(intptr_t)m_handle, m_execute + offset, size);
    sceKernelCloseVMDomain();
}

#elif defined(GT_DYNAREC_GEKKO)

// GameCube and Wii memory has no execute protection, only the caches need maintenance
bool ExecMemory::Init(u32 size)
{
    Release();

    void* memory = memalign(32, size);

    if (!IsValidPointer(memory))
        return false;

    m_write = m_execute = (u8*)memory;
    m_size = size;
    return true;
}

void ExecMemory::Release()
{
    if (IsValidPointer(m_write))
        free(m_write);

    InitPointer(m_write);
    InitPointer(m_execute);
    m_size = 0;
}

void ExecMemory::BeginWrite()
{
}

void ExecMemory::EndWrite(u32 offset, u32 size)
{
    DCFlushRange(m_execute + offset, size);
    ICInvalidateRange(m_execute + offset, size);
}

#endif

#endif
