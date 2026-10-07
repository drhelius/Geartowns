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

#include "i386.h"

const I386::opcodeptr I386::k_opcodes[256] =
{
    &I386::OPCodeThunk<&I386::OPCode0x00>,
    &I386::OPCodeThunk<&I386::OPCode0x01>,
    &I386::OPCodeThunk<&I386::OPCode0x02>,
    &I386::OPCodeThunk<&I386::OPCode0x03>,
    &I386::OPCodeThunk<&I386::OPCode0x04>,
    &I386::OPCodeThunk<&I386::OPCode0x05>,
    &I386::OPCodeThunk<&I386::OPCode0x06>,
    &I386::OPCodeThunk<&I386::OPCode0x07>,
    &I386::OPCodeThunk<&I386::OPCode0x08>,
    &I386::OPCodeThunk<&I386::OPCode0x09>,
    &I386::OPCodeThunk<&I386::OPCode0x0A>,
    &I386::OPCodeThunk<&I386::OPCode0x0B>,
    &I386::OPCodeThunk<&I386::OPCode0x0C>,
    &I386::OPCodeThunk<&I386::OPCode0x0D>,
    &I386::OPCodeThunk<&I386::OPCode0x0E>,
    &I386::OPCodeThunk<&I386::OPCode0x0F>,

    &I386::OPCodeThunk<&I386::OPCode0x10>,
    &I386::OPCodeThunk<&I386::OPCode0x11>,
    &I386::OPCodeThunk<&I386::OPCode0x12>,
    &I386::OPCodeThunk<&I386::OPCode0x13>,
    &I386::OPCodeThunk<&I386::OPCode0x14>,
    &I386::OPCodeThunk<&I386::OPCode0x15>,
    &I386::OPCodeThunk<&I386::OPCode0x16>,
    &I386::OPCodeThunk<&I386::OPCode0x17>,
    &I386::OPCodeThunk<&I386::OPCode0x18>,
    &I386::OPCodeThunk<&I386::OPCode0x19>,
    &I386::OPCodeThunk<&I386::OPCode0x1A>,
    &I386::OPCodeThunk<&I386::OPCode0x1B>,
    &I386::OPCodeThunk<&I386::OPCode0x1C>,
    &I386::OPCodeThunk<&I386::OPCode0x1D>,
    &I386::OPCodeThunk<&I386::OPCode0x1E>,
    &I386::OPCodeThunk<&I386::OPCode0x1F>,

    &I386::OPCodeThunk<&I386::OPCode0x20>,
    &I386::OPCodeThunk<&I386::OPCode0x21>,
    &I386::OPCodeThunk<&I386::OPCode0x22>,
    &I386::OPCodeThunk<&I386::OPCode0x23>,
    &I386::OPCodeThunk<&I386::OPCode0x24>,
    &I386::OPCodeThunk<&I386::OPCode0x25>,
    &I386::OPCodeThunk<&I386::OPCode0x26>,
    &I386::OPCodeThunk<&I386::OPCode0x27>,
    &I386::OPCodeThunk<&I386::OPCode0x28>,
    &I386::OPCodeThunk<&I386::OPCode0x29>,
    &I386::OPCodeThunk<&I386::OPCode0x2A>,
    &I386::OPCodeThunk<&I386::OPCode0x2B>,
    &I386::OPCodeThunk<&I386::OPCode0x2C>,
    &I386::OPCodeThunk<&I386::OPCode0x2D>,
    &I386::OPCodeThunk<&I386::OPCode0x2E>,
    &I386::OPCodeThunk<&I386::OPCode0x2F>,

    &I386::OPCodeThunk<&I386::OPCode0x30>,
    &I386::OPCodeThunk<&I386::OPCode0x31>,
    &I386::OPCodeThunk<&I386::OPCode0x32>,
    &I386::OPCodeThunk<&I386::OPCode0x33>,
    &I386::OPCodeThunk<&I386::OPCode0x34>,
    &I386::OPCodeThunk<&I386::OPCode0x35>,
    &I386::OPCodeThunk<&I386::OPCode0x36>,
    &I386::OPCodeThunk<&I386::OPCode0x37>,
    &I386::OPCodeThunk<&I386::OPCode0x38>,
    &I386::OPCodeThunk<&I386::OPCode0x39>,
    &I386::OPCodeThunk<&I386::OPCode0x3A>,
    &I386::OPCodeThunk<&I386::OPCode0x3B>,
    &I386::OPCodeThunk<&I386::OPCode0x3C>,
    &I386::OPCodeThunk<&I386::OPCode0x3D>,
    &I386::OPCodeThunk<&I386::OPCode0x3E>,
    &I386::OPCodeThunk<&I386::OPCode0x3F>,

    &I386::OPCodeThunk<&I386::OPCode0x40>,
    &I386::OPCodeThunk<&I386::OPCode0x41>,
    &I386::OPCodeThunk<&I386::OPCode0x42>,
    &I386::OPCodeThunk<&I386::OPCode0x43>,
    &I386::OPCodeThunk<&I386::OPCode0x44>,
    &I386::OPCodeThunk<&I386::OPCode0x45>,
    &I386::OPCodeThunk<&I386::OPCode0x46>,
    &I386::OPCodeThunk<&I386::OPCode0x47>,
    &I386::OPCodeThunk<&I386::OPCode0x48>,
    &I386::OPCodeThunk<&I386::OPCode0x49>,
    &I386::OPCodeThunk<&I386::OPCode0x4A>,
    &I386::OPCodeThunk<&I386::OPCode0x4B>,
    &I386::OPCodeThunk<&I386::OPCode0x4C>,
    &I386::OPCodeThunk<&I386::OPCode0x4D>,
    &I386::OPCodeThunk<&I386::OPCode0x4E>,
    &I386::OPCodeThunk<&I386::OPCode0x4F>,

    &I386::OPCodeThunk<&I386::OPCode0x50>,
    &I386::OPCodeThunk<&I386::OPCode0x51>,
    &I386::OPCodeThunk<&I386::OPCode0x52>,
    &I386::OPCodeThunk<&I386::OPCode0x53>,
    &I386::OPCodeThunk<&I386::OPCode0x54>,
    &I386::OPCodeThunk<&I386::OPCode0x55>,
    &I386::OPCodeThunk<&I386::OPCode0x56>,
    &I386::OPCodeThunk<&I386::OPCode0x57>,
    &I386::OPCodeThunk<&I386::OPCode0x58>,
    &I386::OPCodeThunk<&I386::OPCode0x59>,
    &I386::OPCodeThunk<&I386::OPCode0x5A>,
    &I386::OPCodeThunk<&I386::OPCode0x5B>,
    &I386::OPCodeThunk<&I386::OPCode0x5C>,
    &I386::OPCodeThunk<&I386::OPCode0x5D>,
    &I386::OPCodeThunk<&I386::OPCode0x5E>,
    &I386::OPCodeThunk<&I386::OPCode0x5F>,

    &I386::OPCodeThunk<&I386::OPCode0x60>,
    &I386::OPCodeThunk<&I386::OPCode0x61>,
    &I386::OPCodeThunk<&I386::OPCode0x62>,
    &I386::OPCodeThunk<&I386::OPCode0x63>,
    &I386::OPCodeThunk<&I386::OPCode0x64>,
    &I386::OPCodeThunk<&I386::OPCode0x65>,
    &I386::OPCodeThunk<&I386::OPCode0x66>,
    &I386::OPCodeThunk<&I386::OPCode0x67>,
    &I386::OPCodeThunk<&I386::OPCode0x68>,
    &I386::OPCodeThunk<&I386::OPCode0x69>,
    &I386::OPCodeThunk<&I386::OPCode0x6A>,
    &I386::OPCodeThunk<&I386::OPCode0x6B>,
    &I386::OPCodeThunk<&I386::OPCode0x6C>,
    &I386::OPCodeThunk<&I386::OPCode0x6D>,
    &I386::OPCodeThunk<&I386::OPCode0x6E>,
    &I386::OPCodeThunk<&I386::OPCode0x6F>,

    &I386::OPCodeThunk<&I386::OPCode0x70>,
    &I386::OPCodeThunk<&I386::OPCode0x71>,
    &I386::OPCodeThunk<&I386::OPCode0x72>,
    &I386::OPCodeThunk<&I386::OPCode0x73>,
    &I386::OPCodeThunk<&I386::OPCode0x74>,
    &I386::OPCodeThunk<&I386::OPCode0x75>,
    &I386::OPCodeThunk<&I386::OPCode0x76>,
    &I386::OPCodeThunk<&I386::OPCode0x77>,
    &I386::OPCodeThunk<&I386::OPCode0x78>,
    &I386::OPCodeThunk<&I386::OPCode0x79>,
    &I386::OPCodeThunk<&I386::OPCode0x7A>,
    &I386::OPCodeThunk<&I386::OPCode0x7B>,
    &I386::OPCodeThunk<&I386::OPCode0x7C>,
    &I386::OPCodeThunk<&I386::OPCode0x7D>,
    &I386::OPCodeThunk<&I386::OPCode0x7E>,
    &I386::OPCodeThunk<&I386::OPCode0x7F>,

    &I386::OPCodeThunk<&I386::OPCode0x80>,
    &I386::OPCodeThunk<&I386::OPCode0x81>,
    &I386::OPCodeThunk<&I386::OPCode0x82>,
    &I386::OPCodeThunk<&I386::OPCode0x83>,
    &I386::OPCodeThunk<&I386::OPCode0x84>,
    &I386::OPCodeThunk<&I386::OPCode0x85>,
    &I386::OPCodeThunk<&I386::OPCode0x86>,
    &I386::OPCodeThunk<&I386::OPCode0x87>,
    &I386::OPCodeThunk<&I386::OPCode0x88>,
    &I386::OPCodeThunk<&I386::OPCode0x89>,
    &I386::OPCodeThunk<&I386::OPCode0x8A>,
    &I386::OPCodeThunk<&I386::OPCode0x8B>,
    &I386::OPCodeThunk<&I386::OPCode0x8C>,
    &I386::OPCodeThunk<&I386::OPCode0x8D>,
    &I386::OPCodeThunk<&I386::OPCode0x8E>,
    &I386::OPCodeThunk<&I386::OPCode0x8F>,

    &I386::OPCodeThunk<&I386::OPCode0x90>,
    &I386::OPCodeThunk<&I386::OPCode0x91>,
    &I386::OPCodeThunk<&I386::OPCode0x92>,
    &I386::OPCodeThunk<&I386::OPCode0x93>,
    &I386::OPCodeThunk<&I386::OPCode0x94>,
    &I386::OPCodeThunk<&I386::OPCode0x95>,
    &I386::OPCodeThunk<&I386::OPCode0x96>,
    &I386::OPCodeThunk<&I386::OPCode0x97>,
    &I386::OPCodeThunk<&I386::OPCode0x98>,
    &I386::OPCodeThunk<&I386::OPCode0x99>,
    &I386::OPCodeThunk<&I386::OPCode0x9A>,
    &I386::OPCodeThunk<&I386::OPCode0x9B>,
    &I386::OPCodeThunk<&I386::OPCode0x9C>,
    &I386::OPCodeThunk<&I386::OPCode0x9D>,
    &I386::OPCodeThunk<&I386::OPCode0x9E>,
    &I386::OPCodeThunk<&I386::OPCode0x9F>,

    &I386::OPCodeThunk<&I386::OPCode0xA0>,
    &I386::OPCodeThunk<&I386::OPCode0xA1>,
    &I386::OPCodeThunk<&I386::OPCode0xA2>,
    &I386::OPCodeThunk<&I386::OPCode0xA3>,
    &I386::OPCodeThunk<&I386::OPCode0xA4>,
    &I386::OPCodeThunk<&I386::OPCode0xA5>,
    &I386::OPCodeThunk<&I386::OPCode0xA6>,
    &I386::OPCodeThunk<&I386::OPCode0xA7>,
    &I386::OPCodeThunk<&I386::OPCode0xA8>,
    &I386::OPCodeThunk<&I386::OPCode0xA9>,
    &I386::OPCodeThunk<&I386::OPCode0xAA>,
    &I386::OPCodeThunk<&I386::OPCode0xAB>,
    &I386::OPCodeThunk<&I386::OPCode0xAC>,
    &I386::OPCodeThunk<&I386::OPCode0xAD>,
    &I386::OPCodeThunk<&I386::OPCode0xAE>,
    &I386::OPCodeThunk<&I386::OPCode0xAF>,

    &I386::OPCodeThunk<&I386::OPCode0xB0>,
    &I386::OPCodeThunk<&I386::OPCode0xB1>,
    &I386::OPCodeThunk<&I386::OPCode0xB2>,
    &I386::OPCodeThunk<&I386::OPCode0xB3>,
    &I386::OPCodeThunk<&I386::OPCode0xB4>,
    &I386::OPCodeThunk<&I386::OPCode0xB5>,
    &I386::OPCodeThunk<&I386::OPCode0xB6>,
    &I386::OPCodeThunk<&I386::OPCode0xB7>,
    &I386::OPCodeThunk<&I386::OPCode0xB8>,
    &I386::OPCodeThunk<&I386::OPCode0xB9>,
    &I386::OPCodeThunk<&I386::OPCode0xBA>,
    &I386::OPCodeThunk<&I386::OPCode0xBB>,
    &I386::OPCodeThunk<&I386::OPCode0xBC>,
    &I386::OPCodeThunk<&I386::OPCode0xBD>,
    &I386::OPCodeThunk<&I386::OPCode0xBE>,
    &I386::OPCodeThunk<&I386::OPCode0xBF>,

    &I386::OPCodeThunk<&I386::OPCode0xC0>,
    &I386::OPCodeThunk<&I386::OPCode0xC1>,
    &I386::OPCodeThunk<&I386::OPCode0xC2>,
    &I386::OPCodeThunk<&I386::OPCode0xC3>,
    &I386::OPCodeThunk<&I386::OPCode0xC4>,
    &I386::OPCodeThunk<&I386::OPCode0xC5>,
    &I386::OPCodeThunk<&I386::OPCode0xC6>,
    &I386::OPCodeThunk<&I386::OPCode0xC7>,
    &I386::OPCodeThunk<&I386::OPCode0xC8>,
    &I386::OPCodeThunk<&I386::OPCode0xC9>,
    &I386::OPCodeThunk<&I386::OPCode0xCA>,
    &I386::OPCodeThunk<&I386::OPCode0xCB>,
    &I386::OPCodeThunk<&I386::OPCode0xCC>,
    &I386::OPCodeThunk<&I386::OPCode0xCD>,
    &I386::OPCodeThunk<&I386::OPCode0xCE>,
    &I386::OPCodeThunk<&I386::OPCode0xCF>,

    &I386::OPCodeThunk<&I386::OPCode0xD0>,
    &I386::OPCodeThunk<&I386::OPCode0xD1>,
    &I386::OPCodeThunk<&I386::OPCode0xD2>,
    &I386::OPCodeThunk<&I386::OPCode0xD3>,
    &I386::OPCodeThunk<&I386::OPCode0xD4>,
    &I386::OPCodeThunk<&I386::OPCode0xD5>,
    &I386::OPCodeThunk<&I386::OPCode0xD6>,
    &I386::OPCodeThunk<&I386::OPCode0xD7>,
    &I386::OPCodeThunk<&I386::OPCode0xD8>,
    &I386::OPCodeThunk<&I386::OPCode0xD9>,
    &I386::OPCodeThunk<&I386::OPCode0xDA>,
    &I386::OPCodeThunk<&I386::OPCode0xDB>,
    &I386::OPCodeThunk<&I386::OPCode0xDC>,
    &I386::OPCodeThunk<&I386::OPCode0xDD>,
    &I386::OPCodeThunk<&I386::OPCode0xDE>,
    &I386::OPCodeThunk<&I386::OPCode0xDF>,

    &I386::OPCodeThunk<&I386::OPCode0xE0>,
    &I386::OPCodeThunk<&I386::OPCode0xE1>,
    &I386::OPCodeThunk<&I386::OPCode0xE2>,
    &I386::OPCodeThunk<&I386::OPCode0xE3>,
    &I386::OPCodeThunk<&I386::OPCode0xE4>,
    &I386::OPCodeThunk<&I386::OPCode0xE5>,
    &I386::OPCodeThunk<&I386::OPCode0xE6>,
    &I386::OPCodeThunk<&I386::OPCode0xE7>,
    &I386::OPCodeThunk<&I386::OPCode0xE8>,
    &I386::OPCodeThunk<&I386::OPCode0xE9>,
    &I386::OPCodeThunk<&I386::OPCode0xEA>,
    &I386::OPCodeThunk<&I386::OPCode0xEB>,
    &I386::OPCodeThunk<&I386::OPCode0xEC>,
    &I386::OPCodeThunk<&I386::OPCode0xED>,
    &I386::OPCodeThunk<&I386::OPCode0xEE>,
    &I386::OPCodeThunk<&I386::OPCode0xEF>,

    &I386::OPCodeThunk<&I386::OPCode0xF0>,
    &I386::OPCodeThunk<&I386::OPCode0xF1>,
    &I386::OPCodeThunk<&I386::OPCode0xF2>,
    &I386::OPCodeThunk<&I386::OPCode0xF3>,
    &I386::OPCodeThunk<&I386::OPCode0xF4>,
    &I386::OPCodeThunk<&I386::OPCode0xF5>,
    &I386::OPCodeThunk<&I386::OPCode0xF6>,
    &I386::OPCodeThunk<&I386::OPCode0xF7>,
    &I386::OPCodeThunk<&I386::OPCode0xF8>,
    &I386::OPCodeThunk<&I386::OPCode0xF9>,
    &I386::OPCodeThunk<&I386::OPCode0xFA>,
    &I386::OPCodeThunk<&I386::OPCode0xFB>,
    &I386::OPCodeThunk<&I386::OPCode0xFC>,
    &I386::OPCodeThunk<&I386::OPCode0xFD>,
    &I386::OPCodeThunk<&I386::OPCode0xFE>,
    &I386::OPCodeThunk<&I386::OPCode0xFF>
};

const I386::opcodeptr I386::k_opcodes_0f[256] =
{
    &I386::OPCodeThunk<&I386::OPCode0F_0x00>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x01>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x02>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x03>,
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x04 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x05 invalid
    &I386::OPCodeThunk<&I386::OPCode0F_0x06>,
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x07 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x08 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x09 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x0A invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x0B invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x0C invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x0D invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x0E invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x0F invalid

    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x10 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x11 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x12 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x13 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x14 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x15 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x16 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x17 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x18 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x19 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x1A invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x1B invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x1C invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x1D invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x1E invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x1F invalid

    &I386::OPCodeThunk<&I386::OPCode0F_0x20>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x21>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x22>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x23>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x24>,
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x25 invalid
    &I386::OPCodeThunk<&I386::OPCode0F_0x26>,
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x27 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x28 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x29 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x2A invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x2B invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x2C invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x2D invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x2E invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x2F invalid

    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x30 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x31 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x32 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x33 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x34 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x35 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x36 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x37 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x38 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x39 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x3A invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x3B invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x3C invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x3D invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x3E invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x3F invalid

    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x40 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x41 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x42 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x43 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x44 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x45 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x46 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x47 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x48 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x49 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x4A invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x4B invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x4C invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x4D invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x4E invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x4F invalid

    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x50 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x51 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x52 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x53 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x54 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x55 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x56 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x57 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x58 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x59 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x5A invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x5B invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x5C invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x5D invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x5E invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x5F invalid

    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x60 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x61 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x62 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x63 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x64 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x65 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x66 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x67 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x68 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x69 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x6A invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x6B invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x6C invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x6D invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x6E invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x6F invalid

    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x70 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x71 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x72 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x73 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x74 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x75 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x76 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x77 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x78 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x79 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x7A invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x7B invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x7C invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x7D invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x7E invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0x7F invalid

    &I386::OPCodeThunk<&I386::OPCode0F_0x80>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x81>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x82>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x83>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x84>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x85>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x86>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x87>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x88>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x89>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x8A>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x8B>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x8C>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x8D>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x8E>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x8F>,

    &I386::OPCodeThunk<&I386::OPCode0F_0x90>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x91>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x92>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x93>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x94>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x95>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x96>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x97>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x98>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x99>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x9A>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x9B>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x9C>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x9D>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x9E>,
    &I386::OPCodeThunk<&I386::OPCode0F_0x9F>,

    &I386::OPCodeThunk<&I386::OPCode0F_0xA0>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xA1>,
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xA2 invalid
    &I386::OPCodeThunk<&I386::OPCode0F_0xA3>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xA4>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xA5>,
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xA6 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xA7 invalid
    &I386::OPCodeThunk<&I386::OPCode0F_0xA8>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xA9>,
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xAA invalid
    &I386::OPCodeThunk<&I386::OPCode0F_0xAB>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xAC>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xAD>,
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xAE invalid
    &I386::OPCodeThunk<&I386::OPCode0F_0xAF>,

    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xB0 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xB1 invalid
    &I386::OPCodeThunk<&I386::OPCode0F_0xB2>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xB3>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xB4>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xB5>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xB6>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xB7>,
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xB8 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xB9 invalid
    &I386::OPCodeThunk<&I386::OPCode0F_0xBA>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xBB>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xBC>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xBD>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xBE>,
    &I386::OPCodeThunk<&I386::OPCode0F_0xBF>,

    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xC0 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xC1 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xC2 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xC3 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xC4 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xC5 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xC6 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xC7 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xC8 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xC9 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xCA invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xCB invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xCC invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xCD invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xCE invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xCF invalid

    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xD0 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xD1 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xD2 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xD3 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xD4 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xD5 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xD6 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xD7 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xD8 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xD9 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xDA invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xDB invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xDC invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xDD invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xDE invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xDF invalid

    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xE0 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xE1 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xE2 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xE3 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xE4 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xE5 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xE6 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xE7 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xE8 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xE9 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xEA invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xEB invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xEC invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xED invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xEE invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xEF invalid

    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xF0 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xF1 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xF2 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xF3 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xF4 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xF5 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xF6 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xF7 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xF8 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xF9 invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xFA invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xFB invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xFC invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xFD invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid>, // 0xFE invalid
    &I386::OPCodeThunk<&I386::OPCodes_Invalid> // 0xFF invalid
};
