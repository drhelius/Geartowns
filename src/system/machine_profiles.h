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

#ifndef MACHINE_PROFILES_H
#define MACHINE_PROFILES_H

#include "../common/common.h"

const GT_Machine_Profile k_machine_profiles[GT_MACHINE_COUNT] =
{
    { "FM Towns (Model 1/2)", "Model 1, Model 2", true, GT_MACHINE_CPU_80386DX, 16000000, 1, 2, 6, 1, 2, 2 },
    { "FM Towns 1F/2F", "1F, 2F, 1H, 2H", false, GT_MACHINE_CPU_80386DX, 16000000, 1, 2, 8, 1, 2, 2 },
    { "FM Towns 10F/20F", "10F, 20F, 40H, 80H", false, GT_MACHINE_CPU_80386DX, 16000000, 2, 2, 26, 1, 2, 2 },
    { "FM Towns II CX", "CX10, CX20, CX40, CX100", false, GT_MACHINE_CPU_80386DX, 16000000, 2, 2, 26, 1, 2, 2 },
    { "FM Towns II UX", "UX10, UX20, UX40", false, GT_MACHINE_CPU_80386SX, 16000000, 2, 2, 10, 1, 2, 2 },
    { "FM Towns II UG", "UG10, UG20, UG40, UG80", false, GT_MACHINE_CPU_80386SX, 20000000, 2, 2, 10, 1, 2, 2 },
    { "FM Towns II HG", "HG20, HG40, HG100", false, GT_MACHINE_CPU_80386DX, 20000000, 2, 2, 26, 2, 2, 2 },
    { "FM Towns II HR", "HR20, HR100, HR200", false, GT_MACHINE_CPU_80486SX, 20000000, 4, 4, 28, 2, 2, 2 },
    { "FM Towns II UR", "UR20, UR40, UR80", false, GT_MACHINE_CPU_80486SX, 20000000, 2, 2, 10, 2, 2, 2 },
    { "FM Towns II ME", "ME20, ME170", false, GT_MACHINE_CPU_80486SX, 25000000, 2, 2, 66, 2, 2, 2 },
    { "FM Towns II MA", "MA170, MA340, MA170W, MA340W", false, GT_MACHINE_CPU_80486SX, 33000000, 4, 4, 100, 2, 2, 2 },
    { "FM Towns II MX", "MX20, MX170, MX340, MX170W", false, GT_MACHINE_CPU_80486DX2, 66000000, 4, 4, 100, 2, 2, 2 },
    { "FM Towns II MF / Fresh", "MF20, MF170W, Fresh", false, GT_MACHINE_CPU_80486SX, 33000000, 4, 4, 68, 2, 2, 2 },
    { "FM Towns II HC", "HC53, HC53M", false, GT_MACHINE_CPU_PENTIUM, 90000000, 8, 8, 136, 1, 1, 1 },
    { "FM Towns Marty", "Marty, Marty 2, TC Marty", false, GT_MACHINE_CPU_80386SX, 16000000, 2, 2, 4, 1, 1, 1 },
    { "Custom", "Model 1/2 board", true, GT_MACHINE_CPU_80386DX, 16000000, 1, 2, 10, 0, 2, 2 }
};

const GT_Machine_CPU_Info k_machine_cpus[GT_MACHINE_CPU_COUNT] =
{
    { "80386DX", true },
    { "80386SX", false },
    { "80486SX", false },
    { "80486DX2", false },
    { "Pentium", false }
};

#endif /* MACHINE_PROFILES_H */
