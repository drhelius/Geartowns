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

#ifndef GAME_DB_H
#define GAME_DB_H

#include "common.h"

struct GT_DB_Entry
{
    u32 crc;
    const char* title;
    u16 flags;
};

#define GT_GAMEDB_FLAG_NONE                0x0000
#define GT_GAMEDB_FIRMWARE_SYSTEM          0x0800
#define GT_GAMEDB_FIRMWARE_OS              0x1000
#define GT_GAMEDB_FIRMWARE_FONT            0x2000
#define GT_GAMEDB_FIRMWARE_DICTIONARY      0x4000
#define GT_GAMEDB_FIRMWARE_FONT20          0x8000

const GT_DB_Entry k_game_database[] =
{
    // FM Towns Model 1/2 firmware
    { 0x53319E23, "FM Towns Model 1/2 System ROM", GT_GAMEDB_FIRMWARE_SYSTEM },
    { 0x112872EE, "FM Towns Model 1/2 OS ROM", GT_GAMEDB_FIRMWARE_OS },
    { 0x955C6B75, "FM Towns Model 1/2 Font ROM", GT_GAMEDB_FIRMWARE_FONT },
    { 0xB314C659, "FM Towns Model 1/2 Dictionary ROM", GT_GAMEDB_FIRMWARE_DICTIONARY },

    // Free FM Towns Project firmware
    { 0x65DC3B1F, "Free FM Towns System ROM", GT_GAMEDB_FIRMWARE_SYSTEM },
    { 0x43C18E92, "Free FM Towns OS ROM", GT_GAMEDB_FIRMWARE_OS },
    { 0x736D9CB1, "Free FM Towns Font ROM", GT_GAMEDB_FIRMWARE_FONT },
    { 0x504BF849, "Blank ROM", GT_GAMEDB_FIRMWARE_DICTIONARY | GT_GAMEDB_FIRMWARE_FONT20 },

    { 0, 0, GT_GAMEDB_FLAG_NONE }
};

#endif /* GAME_DB_H */
