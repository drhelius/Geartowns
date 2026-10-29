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

#define GT_MACHINE_RELEASE_ORDER_COUNT 12
#define GT_MACHINE_FEATURE_COUNT 28
#define GT_MACHINE_CPU_SPEED_COUNT 8
#define GT_MACHINE_RAM_SIZE_COUNT 16

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

const u32 k_machine_features_1f = GT_MACHINE_FEATURE_RAM_SIZE | GT_MACHINE_FEATURE_FDD_STATUS |
    GT_MACHINE_FEATURE_SCSI;
const u32 k_machine_features_10f = k_machine_features_1f | GT_MACHINE_FEATURE_WAIT_1US |
    GT_MACHINE_FEATURE_INTERVAL_TIMER2 | GT_MACHINE_FEATURE_MEMORY_WAIT;
const u32 k_machine_features_cx = k_machine_features_1f | GT_MACHINE_FEATURE_WAIT_1US |
    GT_MACHINE_FEATURE_INTERVAL_TIMER2 | GT_MACHINE_FEATURE_NMI_MASK | GT_MACHINE_FEATURE_JEIDA4 |
    GT_MACHINE_FEATURE_SPEED_CONTROL | GT_MACHINE_FEATURE_SCSI_WORD_DMA;
const u32 k_machine_features_ux = k_machine_features_10f | GT_MACHINE_FEATURE_NMI_MASK | GT_MACHINE_FEATURE_JEIDA4 |
    GT_MACHINE_FEATURE_LSPEED | GT_MACHINE_FEATURE_POWER_OFF_RESET;
const u32 k_machine_features_ug = k_machine_features_10f | GT_MACHINE_FEATURE_SPEED_CONTROL |
    GT_MACHINE_FEATURE_UG_REGISTERS | GT_MACHINE_FEATURE_FREE_RUN_TIMER | GT_MACHINE_FEATURE_SCSI_WORD_DMA |
    GT_MACHINE_FEATURE_SCSI_WORD_PORT;
const u32 k_machine_features_marty = (k_machine_features_ux & ~(GT_MACHINE_FEATURE_RAM_SIZE |
    GT_MACHINE_FEATURE_MEMORY_WAIT | GT_MACHINE_FEATURE_SCSI)) | GT_MACHINE_FEATURE_UG_REGISTERS |
    GT_MACHINE_FEATURE_FREE_RUN_TIMER | GT_MACHINE_FEATURE_RAM_CARD | GT_MACHINE_FEATURE_MARTY_PORTS |
    GT_MACHINE_FEATURE_FDD_NO_TYPE | GT_MACHINE_FEATURE_TIMER1_LATCHED | GT_MACHINE_FEATURE_PCM_BYTE_LANE;
const u32 k_machine_features_hg = k_machine_features_cx | GT_MACHINE_FEATURE_UG_REGISTERS |
    GT_MACHINE_FEATURE_FREE_RUN_TIMER | GT_MACHINE_FEATURE_CRT_POWER_OFF | GT_MACHINE_FEATURE_SCSI_WORD_PORT;
const u32 k_machine_features_hr = k_machine_features_hg | GT_MACHINE_FEATURE_CACHE_CONTROL |
    GT_MACHINE_FEATURE_CD_CACHE;
const u32 k_machine_features_ur = k_machine_features_ug | GT_MACHINE_FEATURE_CACHE_CONTROL;
const u32 k_machine_features_me = k_machine_features_hr | GT_MACHINE_FEATURE_DMA_CARRY |
    GT_MACHINE_FEATURE_RAM_SIZE_127MB | GT_MACHINE_FEATURE_CD_DOUBLE_SPEED | GT_MACHINE_FEATURE_SERIAL_FIFO |
    GT_MACHINE_FEATURE_PCM16;

// Sprite times in half machine clocks. Other bus users delay the engine: CPU cycles to VRAM and sprite RAM in 1/256
// machine clocks, display fetch bytes in 1/65536 machine clocks at 15 kHz and at 24 and 31 kHz
// Entry times and fetch costs fitted on the Sprite Probe photos and disc 2: SX on a Marty, DX on a 20F
const GT_Machine_Video_Timing k_machine_video_dx = { 666, 2317, false, 169, 75, 210, 870 };
const GT_Machine_Video_Timing k_machine_video_mx = { 960, 1819, false, 169, 75, 210, 870 };
const GT_Machine_Video_Timing k_machine_video_sx = { 1888, 2193, false, 169, 75, 190, 1000 };
const GT_Machine_Video_Timing k_machine_video_marty = { 1888, 2193, true, 169, 75, 190, 1000 };

// 1/256 clocks. SX: fitted on a real Marty. DX: Data Book RAM 3 and VRAM 6 (0 and 3 high speed), the rest as SX
// dram_transfer: a transfer that waited for its code, then accessed another row, reopens the target's row
// dram_store_load: a load right after a 2-cycle store to its row overlaps the store's last clock
// Follows: the Marty's second cycle of a dword read from video memory or ROM overlaps the first. The DX and the fast SX
// have none and keep the ROM wait fitted without them
const GT_Machine_Bus_Waits k_machine_bus_waits_sx = { { 954, 338 }, { 155, 0 }, { 284, 305 }, 954, 256, { 0, 0 },
    { 1298, 868 }, { 1296, 840 }, 1568, 1250, { 1578, 1024 }, -458, -455, -447 };
const GT_Machine_Bus_Waits k_machine_bus_waits_sx_fast = { { 0, 0 }, { 0, 0 }, { 0, 0 }, 0, 0, { 0, 0 },
    { 768, 768 }, { 768, 768 }, 1384, 1250, { 1578, 1024 }, 0, 0, 0 };
const GT_Machine_Bus_Waits k_machine_bus_waits_dx = { { 0, 0 }, { 0, 0 }, { 0, 0 }, 0, 0, { 768, 768 },
    { 1536, 1536 }, { 1536, 1536 }, 1384, 1250, { 1578, 1024 }, 0, 0, 0 };
const GT_Machine_Bus_Waits k_machine_bus_waits_dx_fast = { { 0, 0 }, { 0, 0 }, { 0, 0 }, 0, 0, { 0, 0 },
    { 768, 768 }, { 768, 768 }, 1384, 1250, { 1578, 1024 }, 0, 0, 0 };

// Port blocks off the generic wait, extra 1/256 clocks to read and to write, measured on a Marty
// A write to a port the board doesn't decode ends sooner, as the io_free_write wait of the bus timing below
// Protected-mode segment loads read a descriptor: MOV and LxS, then POP, which overlaps it. DX takes half the cycles
const GT_Machine_Port_Wait k_machine_port_waits[GT_MACHINE_PORT_WAIT_BLOCKS] = { { 0x0060, 0x0060, { 748, 748 } },
    { 0x0200, 0x020E, { 0, -212 } }, { 0x0448, 0x044A, { 0, 1029 } }, { 0x04C0, 0x04CC, { 2282, 2282 } },
    { 0x04D0, 0x04D6, { 0, -212 } }, { 0x04D8, 0x04DE, { 748, 811 } }, { 0x0600, 0x0604, { 0, -212 } },
    { 0x3000, 0x3FFE, { -503, -444 } }, { 0xFD90, 0xFDA4, { 926, 994 } } };

const GT_Machine_Bus_Timing k_machine_bus_sx = { GT_MACHINE_BUS_FIXED, &k_machine_bus_waits_sx, NULL,
    17, 100, 830, 543, 289, { 1845, 1427 }, { 2872, 2470 }, { 14298, 14016 }, { 1536, 1024 }, { 1649, 1308 }, 1068,
    { 1664, 461 }, k_machine_port_waits };
const GT_Machine_Bus_Timing k_machine_bus_ug = { GT_MACHINE_BUS_SPEED_PORT, &k_machine_bus_waits_sx,
    &k_machine_bus_waits_sx_fast, 17, 100, 830, 543, 289, { 1845, 1427 }, { 2872, 2470 }, { 14298, 14016 },
    { 1536, 1024 }, { 1649, 1308 }, 1068, { 1664, 461 }, k_machine_port_waits };
const GT_Machine_Bus_Timing k_machine_bus_dx = { GT_MACHINE_BUS_FIXED, &k_machine_bus_waits_dx, NULL,
    17, 100, 830, 543, 289, { 1845, 1427 }, { 2872, 2470 }, { 14298, 14016 }, { 1536, 1024 }, { 1649, 1308 }, 1068,
    { 832, 230 }, k_machine_port_waits };
const GT_Machine_Bus_Timing k_machine_bus_10f = { GT_MACHINE_BUS_WAIT_PORTS, &k_machine_bus_waits_dx, NULL,
    17, 100, 830, 543, 289, { 1845, 1427 }, { 2872, 2470 }, { 14298, 14016 }, { 1536, 1024 }, { 1649, 1308 }, 1068,
    { 832, 230 }, k_machine_port_waits };
const GT_Machine_Bus_Timing k_machine_bus_cx = { GT_MACHINE_BUS_SPEED_PORT, &k_machine_bus_waits_dx,
    &k_machine_bus_waits_dx_fast, 17, 100, 830, 543, 289, { 1845, 1427 }, { 2872, 2470 }, { 14298, 14016 },
    { 1536, 1024 }, { 1649, 1308 }, 1068, { 832, 230 }, k_machine_port_waits };

const GT_Machine_Profile k_machine_profiles[GT_MACHINE_COUNT] =
{
    { "FM Towns (Model 1/2)", "model1_2", "Model 1, Model 2", "February 1989", true, 0x0101, GT_MACHINE_CPU_80386DX, GT_MACHINE_FPU_SOCKET, 16000000, GT_MACHINE_MAP_32BIT, 0, k_machine_ram_model1_2, 2, 2, false, "fmtowns", &k_machine_video_dx, &k_machine_bus_dx },
    { "FM Towns 1F/2F", "1f_2f", "1F, 2F, 1H, 2H", "November 1989", true, 0x0201, GT_MACHINE_CPU_80386DX, GT_MACHINE_FPU_SOCKET, 16000000, GT_MACHINE_MAP_32BIT, k_machine_features_1f, k_machine_ram_1f_2f, 2, 2, false, "fmtownsux", &k_machine_video_dx, &k_machine_bus_dx },
    { "FM Towns 10F/20F", "10f_20f", "10F, 20F, 40H, 80H", "October 1990", true, 0x0401, GT_MACHINE_CPU_80386DX, GT_MACHINE_FPU_SOCKET, 16000000, GT_MACHINE_MAP_32BIT, k_machine_features_10f, k_machine_ram_10f_20f, 2, 2, false, "fmtownsux", &k_machine_video_dx, &k_machine_bus_10f },
    { "FM Towns II CX", "cx", "CX10, CX20, CX40, CX100", "November 1991", true, 0x0501, GT_MACHINE_CPU_80386DX, GT_MACHINE_FPU_SOCKET, 16000000, GT_MACHINE_MAP_32BIT, k_machine_features_cx, k_machine_ram_cx, 2, 2, false, "fmtownsux", &k_machine_video_dx, &k_machine_bus_cx },
    { "FM Towns II UX", "ux", "UX10, UX20, UX40", "November 1991", true, 0x0303, GT_MACHINE_CPU_80386SX, GT_MACHINE_FPU_NONE, 16000000, GT_MACHINE_MAP_24BIT, k_machine_features_ux, k_machine_ram_ux, 2, 2, false, "fmtownsux", &k_machine_video_sx, &k_machine_bus_sx },
    { "FM Towns II UG", "ug", "UG10, UG20, UG40, UG80", "November 1992", true, 0x0603, GT_MACHINE_CPU_80386SX, GT_MACHINE_FPU_NONE, 20000000, GT_MACHINE_MAP_24BIT, k_machine_features_ug, k_machine_ram_ux, 2, 2, false, "fmtownsux", &k_machine_video_sx, &k_machine_bus_ug },
    { "FM Towns II HG", "hg", "HG20, HG40, HG100", "November 1992", true, 0x0801, GT_MACHINE_CPU_80386DX, GT_MACHINE_FPU_SOCKET, 20000000, GT_MACHINE_MAP_32BIT, k_machine_features_hg, k_machine_ram_10f_20f, 2, 2, true, "fmtownshr", &k_machine_video_mx, &k_machine_bus_cx },
    { "FM Towns II HR", "hr", "HR20, HR100, HR200", "November 1992", true, 0x0702, GT_MACHINE_CPU_80486SX, GT_MACHINE_FPU_SOCKET, 20000000, GT_MACHINE_MAP_32BIT, k_machine_features_hr, k_machine_ram_hr, 4, 2, true, "fmtownshr", &k_machine_video_mx, &k_machine_bus_cx },
    { "FM Towns II UR", "ur", "UR20, UR40, UR80", "February 1993", true, 0x0902, GT_MACHINE_CPU_80486SX, GT_MACHINE_FPU_NONE, 20000000, GT_MACHINE_MAP_32BIT, k_machine_features_ur, k_machine_ram_ur, 2, 2, false, "fmtownshr", &k_machine_video_mx, &k_machine_bus_cx },
    { "FM Towns II ME", "me", "ME20, ME170", "November 1993", true, 0x0D02, GT_MACHINE_CPU_80486SX, GT_MACHINE_FPU_SOCKET, 25000000, GT_MACHINE_MAP_32BIT, k_machine_features_me, k_machine_ram_me, 2, 2, true, "fmtownsftv", &k_machine_video_mx, &k_machine_bus_cx },
    { "FM Towns II MA", "ma", "MA170, MA340, MA170W, MA340W", "November 1993", false, 0x0B02, GT_MACHINE_CPU_80486SX, GT_MACHINE_FPU_SOCKET, 33000000, GT_MACHINE_MAP_32BIT, 0, k_machine_ram_ma, 4, 2, true, NULL, &k_machine_video_mx, NULL },
    { "FM Towns II MX", "mx", "MX20, MX170, MX340, MX170W", "November 1993", false, 0x0C02, GT_MACHINE_CPU_80486DX2, GT_MACHINE_FPU_INTEGRATED, 66000000, GT_MACHINE_MAP_32BIT, 0, k_machine_ram_mx, 4, 2, true, "fmtownsmx", &k_machine_video_mx, NULL },
    { "FM Towns II MF / Fresh", "mf_fresh", "MF20, MF170W, Fresh", "February 1994", true, 0x0F02, GT_MACHINE_CPU_80486SX, GT_MACHINE_FPU_SOCKET, 33000000, GT_MACHINE_MAP_32BIT, k_machine_features_me, k_machine_ram_mf_fresh, 4, 2, true, "fmtownsftv", &k_machine_video_mx, &k_machine_bus_cx },
    { "FM Towns II HC", "hc", "HC53, HC53M", "February 1995", false, 0x1102, GT_MACHINE_CPU_PENTIUM, GT_MACHINE_FPU_INTEGRATED, 90000000, GT_MACHINE_MAP_32BIT, 0, k_machine_ram_hc, 8, 1, true, NULL, &k_machine_video_mx, NULL },
    { "FM Towns Marty", "marty", "Marty, Marty 2, TC Marty", "February 1993", true, 0x4A03, GT_MACHINE_CPU_80386SX, GT_MACHINE_FPU_NONE, 16000000, GT_MACHINE_MAP_MARTY, k_machine_features_marty, k_machine_ram_marty, 2, 1, false, "fmtmarty", &k_machine_video_marty, &k_machine_bus_sx }
};

const GT_Machine_Model k_machine_release_order[GT_MACHINE_RELEASE_ORDER_COUNT] =
{
    GT_MACHINE_MODEL1_2, GT_MACHINE_1F_2F, GT_MACHINE_10F_20F, GT_MACHINE_CX, GT_MACHINE_UX, GT_MACHINE_UG, GT_MACHINE_HG,
    GT_MACHINE_HR, GT_MACHINE_UR, GT_MACHINE_ME, GT_MACHINE_MF_FRESH, GT_MACHINE_MARTY
};

const int k_machine_cpu_speeds_mhz[GT_MACHINE_CPU_SPEED_COUNT] = { 16, 20, 25, 33, 40, 50, 66, 100 };
const int k_machine_ram_sizes_mb[GT_MACHINE_RAM_SIZE_COUNT] = { 1, 2, 4, 6, 8, 10, 12, 16, 20, 24, 26, 28, 32, 48, 64,
    68 };

const GT_Machine_CPU_Info k_machine_cpus[GT_MACHINE_CPU_COUNT] =
{
    { "80386DX", true, 3, false, 0x00000300, 0x7FFFFFE0, 32 },
    { "80386SX", true, 3, false, 0x00002308, 0x7FFFFFF0, 16 },
    { "80486SX", true, 4, false, 0x00000420, 0x00000010, 32 },
    { "80486DX2", true, 4, true, 0x00000430, 0x00000010, 32 },
    { "Pentium", false, 5, true, 0x00000520, 0x00000010, 64 }
};

static inline const char* get_machine_fpu_name(GT_Machine_CPU cpu)
{
    if (k_machine_cpus[cpu].fpu)
        return "integrated";

    return k_machine_cpus[cpu].family >= 4 ? "487SX" : "80387";
}

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
    { "32-bit", 0xFFFFFFFFU, 0x80000000U, 0x80100000U, 0x81000000U, 0xC2000000U, 0xC2080000U, 0xC2100000U, 0xC2140000U, 0xC2200000U, 0xFFFC0000U, 0, 0, 0xC0000000U, 0x01000000U, 0xC1000000U, NULL },
    { "24-bit", 0x00FFFFFFU, 0x00A00000U, 0x00B00000U, 0x00C00000U, 0x00E00000U, 0x00E80000U, 0x00F00000U, 0x00F40000U, 0x00F80000U, 0x00FC0000U, 0, 0, 0x00D00000U, 0x00100000U, 0, k_machine_dma_windows_24bit },
    { "Marty", 0x00FFFFFFU, 0x00A00000U, 0x00B00000U, 0x00C00000U, 0x00600000U, 0x00E80000U, 0x00F00000U, 0x00F40000U, 0x00F80000U, 0x00FC0000U, 0x00680000U, 0x00F42000U, 0x00D00000U, 0x00100000U, 0, k_machine_dma_windows_24bit }
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
    "CRT power off (0022h)",
    "RAM card size (05E8h)",
    "Marty ports (0486h-0496h)",
    "Floppy status without drive type (0208h)",
    "Timer 1 latched at power-on (0060h)",
    "SCSI interface (0C30h)",
    "SCSI word DMA (0C32h W/B)",
    "SCSI word DMA flag (0C34h)",
    "Cache control (00C0h)",
    "CD-ROM cache (04C8h)",
    "Wave RAM byte lane",
    "DMA address carry (00A7h)",
    "RAM size up to 127 MB (05E8h)",
    "Double-speed CD-ROM (04B0h)",
    "Serial FIFO (0A0Ch)",
    "16-bit PCM (0510h)"
};

#endif /* MACHINE_PROFILES_H */
