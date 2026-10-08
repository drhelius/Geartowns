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

#ifndef EXEC_MEMORY_H
#define EXEC_MEMORY_H

#include "../common/common.h"
#include "dynarec_platform.h"

#if !defined(GT_DISABLE_DYNAREC)

// One region of executable memory for generated code, from the platform provider
// Code is written through the write view and runs from the execute view, which may be a second mapping
class ExecMemory
{
public:
    ExecMemory();
    ~ExecMemory();
    bool Init(u32 size);
    void Release();
    bool IsReady() const;
    u8* GetWriteView() const;
    u8* GetExecuteView() const;
    u32 GetSize() const;
    void BeginWrite();
    void EndWrite(u32 offset, u32 size);

private:
    u8* m_write;
    u8* m_execute;
    u32 m_size;
    void* m_handle;
};

#endif

#endif /* EXEC_MEMORY_H */
