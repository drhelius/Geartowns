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

#define GT_MACHINE_RELEASE_ORDER_COUNT 8
#define GT_MACHINE_FEATURE_COUNT 13
#define GT_MACHINE_CPU_SPEED_COUNT 8
#define GT_MACHINE_RAM_SIZE_COUNT 11

const u8 k_machine_ram_model1_2[] = { 1, 2, 4, 6, 0 };
const u8 k_machine_ram_1f_2f[] = { 1, 2, 4, 6, 8, 0 };
const u8 k_machine_ram_10f_20f[] = { 2, 4, 6, 8, 10, 12, 16, 20, 24, 26, 0 };
const u8 k_machine_ram_cx[] = { 2, 4, 6, 8, 10, 12, 16, 0 };
const u8 k_machine_ram_ux[] = { 2, 4, 6, 10, 0 };
const u8 k_machine_ram_hr[] = { 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 28, 0 };
const u8 k_machine_ram_ur[] = { 2, 4, 6, 8, 10, 0 };
const u8 k_machine_ram_me[] = { 2, 4, 6, 8, 10, 12, 16, 20, 24, 32, 48, 64, 66, 0 };
const u8 k_machine_ram_ma[] = { 4, 6, 8, 10, 12, 16, 20, 24, 32, 48, 64, 96, 100, 0 };
const u8 k_machine_ram_mx[] = { 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 36, 38, 40, 42, 44, 46, 48, 52,
    53, 54, 56, 60, 68, 70, 72, 76, 84, 100, 0 };
const u8 k_machine_ram_mf_fresh[] = { 4, 6, 8, 10, 12, 14, 16, 20, 22, 24, 28, 36, 38, 40, 44, 52, 68, 0 };
const u8 k_machine_ram_hc[] = { 8, 10, 12, 16, 20, 24, 32, 48, 64, 96, 128, 136, 0 };
const u8 k_machine_ram_marty[] = { 2, 4, 0 };

const u32 k_machine_features_1f = GT_MACHINE_FEATURE_RAM_SIZE | GT_MACHINE_FEATURE_FDD_STATUS;
const u32 k_machine_features_10f = k_machine_features_1f | GT_MACHINE_FEATURE_WAIT_1US |
    GT_MACHINE_FEATURE_INTERVAL_TIMER2 | GT_MACHINE_FEATURE_MEMORY_WAIT;
const u32 k_machine_features_cx = k_machine_features_1f | GT_MACHINE_FEATURE_WAIT_1US |
    GT_MACHINE_FEATURE_INTERVAL_TIMER2 | GT_MACHINE_FEATURE_NMI_MASK | GT_MACHINE_FEATURE_JEIDA4 |
    GT_MACHINE_FEATURE_SPEED_CONTROL;
const u32 k_machine_features_ux = k_machine_features_10f | GT_MACHINE_FEATURE_NMI_MASK | GT_MACHINE_FEATURE_JEIDA4 |
    GT_MACHINE_FEATURE_LSPEED | GT_MACHINE_FEATURE_POWER_OFF_RESET;
const u32 k_machine_features_ug = k_machine_features_10f | GT_MACHINE_FEATURE_SPEED_CONTROL |
    GT_MACHINE_FEATURE_UG_REGISTERS | GT_MACHINE_FEATURE_FREE_RUN_TIMER;
const u32 k_machine_features_hg = k_machine_features_cx | GT_MACHINE_FEATURE_UG_REGISTERS |
    GT_MACHINE_FEATURE_FREE_RUN_TIMER | GT_MACHINE_FEATURE_CRT_POWER_OFF;

const GT_Machine_Profile k_machine_profiles[GT_MACHINE_COUNT] =
{
    { "FM Towns (Model 1/2)", "model1_2", "Model 1, Model 2", "February 1989", true, 0x0101, GT_MACHINE_CPU_80386DX, 16000000, GT_MACHINE_MAP_32BIT, 0, k_machine_ram_model1_2, 2, 2, false, "fmtowns" },
    { "FM Towns 1F/2F", "1f_2f", "1F, 2F, 1H, 2H", "November 1989", false, 0x0201, GT_MACHINE_CPU_80386DX, 16000000, GT_MACHINE_MAP_32BIT, k_machine_features_1f, k_machine_ram_1f_2f, 2, 2, false, "fmtownsux" },
    { "FM Towns 10F/20F", "10f_20f", "10F, 20F, 40H, 80H", "October 1990", false, 0x0401, GT_MACHINE_CPU_80386DX, 16000000, GT_MACHINE_MAP_32BIT, k_machine_features_10f, k_machine_ram_10f_20f, 2, 2, false, "fmtownsux" },
    { "FM Towns II CX", "cx", "CX10, CX20, CX40, CX100", "November 1991", false, 0x0501, GT_MACHINE_CPU_80386DX, 16000000, GT_MACHINE_MAP_32BIT, k_machine_features_cx, k_machine_ram_cx, 2, 2, false, "fmtownsux" },
    { "FM Towns II UX", "ux", "UX10, UX20, UX40", "November 1991", false, 0x0303, GT_MACHINE_CPU_80386SX, 16000000, GT_MACHINE_MAP_24BIT, k_machine_features_ux, k_machine_ram_ux, 2, 2, false, "fmtownsux" },
    { "FM Towns II UG", "ug", "UG10, UG20, UG40, UG80", "November 1992", false, 0x0603, GT_MACHINE_CPU_80386SX, 20000000, GT_MACHINE_MAP_24BIT, k_machine_features_ug, k_machine_ram_ux, 2, 2, false, "fmtmarty" },
    { "FM Towns II HG", "hg", "HG20, HG40, HG100", "November 1992", false, 0x0801, GT_MACHINE_CPU_80386DX, 20000000, GT_MACHINE_MAP_32BIT, k_machine_features_hg, k_machine_ram_10f_20f, 2, 2, true, "fmtownsftv" },
    { "FM Towns II HR", "hr", "HR20, HR100, HR200", "November 1992", false, 0x0702, GT_MACHINE_CPU_80486SX, 20000000, GT_MACHINE_MAP_32BIT, 0, k_machine_ram_hr, 4, 2, true, "fmtownshr" },
    { "FM Towns II UR", "ur", "UR20, UR40, UR80", "February 1993", false, 0x0902, GT_MACHINE_CPU_80486SX, 20000000, GT_MACHINE_MAP_32BIT, 0, k_machine_ram_ur, 2, 2, false, NULL },
    { "FM Towns II ME", "me", "ME20, ME170", "November 1993", false, 0x0D02, GT_MACHINE_CPU_80486SX, 25000000, GT_MACHINE_MAP_32BIT, 0, k_machine_ram_me, 2, 2, true, NULL },
    { "FM Towns II MA", "ma", "MA170, MA340, MA170W, MA340W", "November 1993", false, 0x0B02, GT_MACHINE_CPU_80486SX, 33000000, GT_MACHINE_MAP_32BIT, 0, k_machine_ram_ma, 4, 2, true, NULL },
    { "FM Towns II MX", "mx", "MX20, MX170, MX340, MX170W", "November 1993", false, 0x0C02, GT_MACHINE_CPU_80486DX2, 66000000, GT_MACHINE_MAP_32BIT, 0, k_machine_ram_mx, 4, 2, true, "fmtownsmx" },
    { "FM Towns II MF / Fresh", "mf_fresh", "MF20, MF170W, Fresh", "February 1994", false, 0x0F02, GT_MACHINE_CPU_80486SX, 33000000, GT_MACHINE_MAP_32BIT, 0, k_machine_ram_mf_fresh, 4, 2, true, "fmtownsftv" },
    { "FM Towns II HC", "hc", "HC53, HC53M", "February 1995", false, 0x1102, GT_MACHINE_CPU_PENTIUM, 90000000, GT_MACHINE_MAP_32BIT, 0, k_machine_ram_hc, 8, 1, true, NULL },
    { "FM Towns Marty", "marty", "Marty, Marty 2, TC Marty", "February 1993", false, 0x4A03, GT_MACHINE_CPU_80386SX, 16000000, GT_MACHINE_MAP_MARTY, k_machine_features_ux, k_machine_ram_marty, 2, 1, false, "fmtmarty" }
};

const GT_Machine_Model k_machine_release_order[GT_MACHINE_RELEASE_ORDER_COUNT] =
{
    GT_MACHINE_MODEL1_2, GT_MACHINE_1F_2F, GT_MACHINE_10F_20F, GT_MACHINE_CX, GT_MACHINE_UX, GT_MACHINE_UG, GT_MACHINE_HG,
    GT_MACHINE_MARTY
};

const int k_machine_cpu_speeds_mhz[GT_MACHINE_CPU_SPEED_COUNT] = { 16, 20, 25, 33, 40, 50, 66, 100 };
const int k_machine_ram_sizes_mb[GT_MACHINE_RAM_SIZE_COUNT] = { 1, 2, 4, 6, 8, 10, 12, 16, 20, 24, 26 };

const GT_Machine_CPU_Info k_machine_cpus[GT_MACHINE_CPU_COUNT] =
{
    { "80386DX", true, 0x00000300, 32 },
    { "80386SX", false, 0x00002308, 16 },
    { "80486SX", false, 0x00000420, 32 },
    { "80486DX2", false, 0x00000430, 32 },
    { "Pentium", false, 0x00000520, 64 }
};

const GT_Machine_DMA_Window k_machine_dma_windows_24bit[] =
{
    { 0x800, 1, 0x00A00000U },
    { 0x801, 1, 0x00B00000U },
    { 0x810, 1, 0x00C00000U },
    { 0xC20, 1, 0x00E00000U },
    { 0xC21, 1, 0x00F00000U },
    { 0xC22, 1, 0x00F80000U },
    { 0xFFF, 1, 0x00F00000U },
    { 0x400, 1, 0x00C80000U },
    { 0xC00, 16, 0x00D00000U },
    { 0, 0, 0 }
};

const GT_Machine_Memory_Map k_machine_maps[GT_MACHINE_MAP_COUNT] =
{
    { "32-bit", 0xFFFFFFFFU, 0x80000000U, 0x80100000U, 0x81000000U, 0xC2000000U, 0xC2080000U, 0xC2100000U, 0xC2140000U, 0xC2200000U, 0xFFFC0000U, NULL },
    { "24-bit", 0x00FFFFFFU, 0x00A00000U, 0x00B00000U, 0x00C00000U, 0x00E00000U, 0x00E80000U, 0x00F00000U, 0x00F40000U, 0x00F80000U, 0x00FC0000U, k_machine_dma_windows_24bit },
    { "Marty", 0x00FFFFFFU, 0x00A00000U, 0x00B00000U, 0x00C00000U, 0x00600000U, 0x00E80000U, 0x00F00000U, 0x00F40000U, 0x00F80000U, 0x00FC0000U, k_machine_dma_windows_24bit }
};

const char* const k_machine_feature_names[GT_MACHINE_FEATURE_COUNT] =
{
    "RAM size (05E8h)",
    "Floppy status (0208h)",
    "1 us wait (006Ch)",
    "Interval Timer II (0068h)",
    "Memory wait (05E2h)",
    "NMI mask (0028h)",
    "JEIDA-4 (0490h)",
    "LSPEED (0030h)",
    "Power-off reset (0020h)",
    "Speed control (05ECh)",
    "UG registers (0024h)",
    "Free-running timer (0026h)",
    "CRT power off (0022h)"
};

#endif /* MACHINE_PROFILES_H */
