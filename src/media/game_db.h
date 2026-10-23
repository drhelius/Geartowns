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

#include "../common/common.h"

struct GT_DB_Entry
{
    u32 crc;
    const char* title;
    u16 flags;
    u8 model;
    u8 ram_mb;
    u8 cpu_mhz;
    u8 port_1;
    u8 port_2;
};

#define GT_GAMEDB_FLAG_NONE                0x0000
#define GT_GAMEDB_HDD                      0x0001
#define GT_GAMEDB_CPU_486                  0x0002
#define GT_GAMEDB_FLOPPY_BOOT              0x0010
#define GT_GAMEDB_FLOPPY_REQUIRED          0x0020
#define GT_GAMEDB_FLOPPY_OPTIONAL          0x0040
#define GT_GAMEDB_FLOPPY_PROTECTED         0x0080
#define GT_GAMEDB_FLOPPY_MASK              0x00F0
#define GT_GAMEDB_FIRMWARE_EXTENSION       0x0200
#define GT_GAMEDB_FIRMWARE_SERIAL          0x0400
#define GT_GAMEDB_FIRMWARE_SYSTEM          0x0800
#define GT_GAMEDB_FIRMWARE_OS              0x1000
#define GT_GAMEDB_FIRMWARE_FONT            0x2000
#define GT_GAMEDB_FIRMWARE_DICTIONARY      0x4000
#define GT_GAMEDB_FIRMWARE_FONT20          0x8000
#define GT_GAMEDB_FIRMWARE_MASK            0xFE00

#define GT_GAMEDB_PORT_DEFAULT             0
#define GT_GAMEDB_PORT_NONE                1
#define GT_GAMEDB_PORT_PAD                 2
#define GT_GAMEDB_PORT_MARTY_PAD           3
#define GT_GAMEDB_PORT_6_BUTTON_PAD        4
#define GT_GAMEDB_PORT_MOUSE               5

const GT_DB_Entry k_game_database[] =
{
    // FM Towns (Model 1/2) with more RAM
    { 0x83352AF9, "Abel - Shin Mokushiroku Taisen", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x913CEBC6, "Alone in the Dark 2", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x41A67A62, "Appare CD Vol. 1 - Hiryuu no Maki", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x9780336E, "Cubic Sketch V1.1 L10", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x336B02B3, "Cyberia", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x27504E28, "DAPS for Windows", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xDF2AB948, "FM Towns II Demonstration CD-ROM '93 Winter (Rev A)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x0B8E81BC, "FM Towns II Demonstration CD-ROM '94 Spring", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x8D07926B, "FM Towns II Demonstration CD-ROM '94 Summer", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x72C94A11, "Fouge Vol. 2 - Masuo per Masuo - Ikeda Masuo Hanga-shuu (Rerelease)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 6, 0, 0, 0 },
    { 0xA366C82C, "Fujitsu Journal Special Edition By CD-ROM", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xBBC61FAD, "Golf Links 386 Pro", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x87EF24AC, "Half Moon ni Kawaru made - Ramiyaryou no Nijiiro Tamatebako", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xC7C31F57, "High C Compiler Multimedia Kaihatsu Kit V1.7 L13", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x4FE457F9, "Kanade V1.1 L10 (Rev A)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x7E5B9BDC, "Kanade V1.1 L20", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 6, 0, 0, 0 },
    { 0x0338CFD1, "Kero Kero Keroppi to Origami no Tabibito", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x5104D6F4, "LiveAnimation V1.1 L10", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x55A9DB0A, "LiveMovie V1.1 L10", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x899BD732, "Lunatic Dawn II", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xE23DB546, "Mahou Daisakusen", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 33, 0, 0 },
    { 0x8F2CE450, "Menzoberranzan - Yami no Monshou", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xA4D8170D, "Microsoft Windows Version 3.1 L10 Operating System (Setup CD-ROM)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xC09847CD, "Microsoft Windows Version 3.1 L11 Operating System (Setup CD-ROM)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xC86AA9E3, "Microsoft Windows Version 3.1 L12 Operating System (Setup CD-ROM)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x21C79FBD, "Might and Magic - Dark Side of Xeen", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xC37E1530, "Mujintou Monogatari", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x43CE5719, "OASYS-Win V2.0", GT_GAMEDB_HDD | GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xF707A789, "Pegasus V1.1 L10 (Rev A)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x127A04E1, "Pegasus V1.1 L10 (Rev C)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x97697D9D, "Princess Danger", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xC2D1FF4A, "Psychic Detective Series Vol. 3 - Aya (DCCS)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x8ED44C13, "Psychic Detective Series Vol. 4 - Orgel (DCCS)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xA4447E85, "Psychic Detective Series Vol. 5 - Nightmare (DCCS)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x63849281, "Reijou Monogatari", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xB2752ED3, "Return to Zork", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 6, 0, 0, 0 },
    { 0xEFF11460, "Samurai Spirits", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xB7932028, "SimCity 2000", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x8F3A1365, "Strike Commander Plus", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 6, 0, 0, 0 },
    { 0x519B3DBB, "Super Street Fighter II - The New Challengers", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 66, GT_GAMEDB_PORT_6_BUTTON_PAD, GT_GAMEDB_PORT_6_BUTTON_PAD },
    { 0x86A50F19, "Super Street Fighter II - The New Challengers (Demo)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 66, GT_GAMEDB_PORT_6_BUTTON_PAD, GT_GAMEDB_PORT_6_BUTTON_PAD },
    { 0x9D19776D, "TEATA - Vision", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x11BC5F23, "To Your Health! Vol. 1 - Tsubo Relaxation", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x954EACD5, "To Your Health! Vol. 2 - Wellness Yoga", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x39CC9818, "Toshiyuki Yoshino - Lullaby of BirdLand", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x8C5112A5, "Towns Graph V2.1 L10", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xD244B8B1, "Towns Paint II V1.1 L10", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x29A895DA, "Towns System Software V2.1 L50 (Rev A)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xAEE2D1E2, "Towns System Software V2.1 L51", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xDCE386FE, "TownsFullcolor V2.1 L10", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xAB6F985E, "TownsMagazine for School Vol. 3", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x14532CDE, "TownsMagazine for School Vol. 5", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xD425136B, "TownsMagazine Vol. 3 (No. 1)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xA4B705B8, "TownsMagazine Vol. 3 (No. 2)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x1FFEBE51, "Uwaki na Anata - Switch o Irete", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x24A2C02B, "Welcome to FM Towns 1 (Green Disc)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x1E6ECD6D, "Welcome to FM Towns 1 (Red Disc)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x891088E3, "Wing Commander - Armada", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 6, 0, 0, 0 },
    { 0x9C9981D6, "Wing Commander II - Vengeance of the Kilrathi - Special Operations 1 & Special Operations 2", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 6, 0, 0, 0 },

    // FM Towns (Model 1/2) with a faster CPU
    { 0x88216CFA, "RAC Rally", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 33, GT_GAMEDB_PORT_MOUSE, 0 },

    // FM Towns 1F/2F
    { 0xCAE67272, "Kikai Jikake no Marian - Electric Device Marian", GT_GAMEDB_FLAG_NONE, GT_MACHINE_1F_2F, 8, 0, 0, 0 },
    { 0x59C88BEE, "Strike Commander", GT_GAMEDB_HDD, GT_MACHINE_1F_2F, 8, 0, 0, 0 },

    // Controller ports
    { 0x06E3D83D, "Dennou Ehon - Kyouryuu no Sekai", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 0, GT_GAMEDB_PORT_MOUSE, 0 },
    { 0xB5C2AD16, "Enkaiou Ver. 3 - Chikyuu Saidai no Kessen", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 0, 0, GT_GAMEDB_PORT_PAD },
    { 0xC8468211, "J. B. Harold Jikenbo 1 - Murder Club DX", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 0, GT_GAMEDB_PORT_MOUSE, 0 },
    { 0x9F9B811E, "Last Survivor", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 0, 0, GT_GAMEDB_PORT_PAD },
    { 0xD11E3504, "Mad Stalker - FullMetalForce", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 0, 0, GT_GAMEDB_PORT_PAD },
    { 0x478F99B0, "Manami no Doko made Iku no 2 - Return of the Kuro Pack", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 0, GT_GAMEDB_PORT_MOUSE, 0 },
    { 0x61A92F7C, "Muscle Bomber - The Body Explosion", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 0, 0, GT_GAMEDB_PORT_PAD },
    { 0x45065A9C, "Muscle Bomber - The Body Explosion (Demo)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 0, 0, GT_GAMEDB_PORT_PAD },
    { 0xB2B60C8C, "Palamedes", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 0, 0, GT_GAMEDB_PORT_PAD },
    { 0xDA977A6D, "Toudou Ryuunosuke Tantei Nikki - Ougon no Rashinban - Shouyoumaru San Francisco Kouro Satsujin Jiken", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 0, GT_GAMEDB_PORT_MOUSE, 0 },

    // FM Towns Marty and the 386SX-only discs
    { 0x42F71CB4, "4D Driving (FM Towns Marty Rerelease)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MARTY, 0, 0, 0, 0 },
    { 0x8B93A37A, "Auto Demo '93 Fuyu", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MARTY, 0, 0, 0, 0 },
    { 0xF77D4A4F, "FM Towns Marty Tentou Auto Demo '93 Natsu", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MARTY, 0, 0, 0, 0 },
    { 0xC19736AC, "Ms. Detective File 1 - Iwami Ginzan Satsujin Jiken (FM Towns Marty)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MARTY, 0, 0, 0, 0 },

    // 486 titles. The 486 models (HR, MA, MX) are not emulated yet, so these name the HG, the closest emulated model
    { 0xA3A41A2E, "Alltynex", GT_GAMEDB_CPU_486, GT_MACHINE_HG, 4, 66, 0, 0 }, // MX
    { 0xC21B99C5, "Alltynex (T-En v1.1)", GT_GAMEDB_CPU_486, GT_MACHINE_HG, 0, 66, 0, 0 }, // MX
    { 0xF6B8469F, "ElFish", GT_GAMEDB_HDD | GT_GAMEDB_CPU_486, GT_MACHINE_HG, 4, 20, 0, 0 }, // HR
    { 0x71570BC6, "Gadget - Invention, Travel, & Adventure", GT_GAMEDB_HDD | GT_GAMEDB_CPU_486, GT_MACHINE_HG, 8, 20, 0, 0 }, // HR, MA recommended
    { 0xC2A397F7, "Horde, The", GT_GAMEDB_HDD | GT_GAMEDB_CPU_486, GT_MACHINE_HG, 4, 20, 0, 0 }, // HR
    { 0xB63BB04F, "Para Para Paradise (Disc A) (Akane Kouda)", GT_GAMEDB_HDD | GT_GAMEDB_CPU_486, GT_MACHINE_HG, 8, 33, 0, 0 }, // MA
    { 0x6DAC7913, "Para Para Paradise (Disc B) (Kyouko Hisakawa)", GT_GAMEDB_HDD | GT_GAMEDB_CPU_486, GT_MACHINE_HG, 8, 33, 0, 0 }, // MA
    { 0xCD8B3F09, "Para Para Paradise (Disc C) (Makoto Siina)", GT_GAMEDB_HDD | GT_GAMEDB_CPU_486, GT_MACHINE_HG, 8, 33, 0, 0 }, // MA
    { 0x75236D99, "Ravenloft - Aku no Keshin", GT_GAMEDB_CPU_486, GT_MACHINE_HG, 4, 20, 0, 0 }, // HR
    { 0x6BEA56A2, "Teo - Mou Hitotsu no Chikyuu (Version 1.0)", GT_GAMEDB_HDD | GT_GAMEDB_CPU_486, GT_MACHINE_HG, 8, 66, 0, 0 }, // MX, 2x CD-ROM
    { 0xB253F5C1, "Teo - Mou Hitotsu no Chikyuu (Version 2.0)", GT_GAMEDB_HDD | GT_GAMEDB_FLOPPY_REQUIRED | GT_GAMEDB_CPU_486, GT_MACHINE_HG, 8, 66, 0, 0 }, // MX, 2x CD-ROM
    { 0xADC925E6, "Theme Park", GT_GAMEDB_HDD | GT_GAMEDB_CPU_486, GT_MACHINE_HG, 4, 20, 0, 0 }, // HR

    // Hard disk installs
    { 0x09706738, "Gakuen King - Hidehiko Gakkou o Tsukuru", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x89A5A3E0, "Mugen Houyou", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xF7DEA1BC, "Only You - Seikimatsu no Juliet-tachi", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x38E9D30F, "Pro Student G (Rerelease)", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x9B45195C, "Rance 4.1 - Okusuri Koujou o Sukue!", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x0EF5C8F2, "Rance 4.2 - Angel-gumi", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xAE23D90C, "Running Girls - Hashiri Onna II", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xDC97C63A, "Toushin Toshi II - Soshite, Sorekara...", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x63E9C5F3, "Ultima Underworld II - Labyrinth of Worlds", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x3BDD294D, "Wing Commander - The Secret Missions & The Secret Missions 2 - Crusade", GT_GAMEDB_HDD, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },

    // Floppy disks
    { 0x0FD1A285, "4th Unit Series, The - Wyatt", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x262A6AB1, "4th Unit Series, The - Wyatt (Demo)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xBACACD7F, "Air Management - Oozora ni Kakeru", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xDD36BAD4, "AIV - A Ressha de Ikou 4", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x4F757638, "Akiko Gold", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xAE5BA860, "Alice no Yakata CD II", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xEEAD2215, "Amaranth III", GT_GAMEDB_FLOPPY_OPTIONAL, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x9672A1D3, "AmbivalenZ - Niritsu Haihan", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x71020913, "Angel", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xD6C1AD51, "Aoki Ookami to Shiroki Mejika - Genchou Hishi", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x2D79A7B0, "Asuka 120% Excellent - Burning Fest. Excellent", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, GT_GAMEDB_PORT_PAD },
    { 0xACC666FC, "Atlas II, The", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x99F2C5E4, "Ayumi-chan Monogatari", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x3446545D, "Ballade for Maria - Maria ni Sasageru Ballade", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xE2C17C10, "Band-kun", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x20BC3F5B, "Bible Master - Crash of the BlleotRutz", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xA35641AD, "Bible Master 2 - The Chaos of Aglia", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xCF8EFBBA, "California X Party - Joshi Daisei Himitsu Club", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x41571BEB, "Chiemi + Naomi", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x71FDC282, "CRI StacCard", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x22B1265E, "Crystal Rinal - Ouma no Meikyuu", GT_GAMEDB_FLOPPY_OPTIONAL, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xB7E5A175, "Custom Mate + Denwa no Bell ga...", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x26029AE5, "Custom Mate 2 + Itsuka Dokoka de", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x77A6C316, "Cyber Motion", GT_GAMEDB_HDD | GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x1C0B3EA0, "CyberCity - The Golden Age of Cybernetics", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x9FD16507, "Daikoukai Jidai", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x0668CE1A, "Daikoukai Jidai II", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x4C3151A9, "Daisenryaku III '90 (Ninety)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x47741726, "Dalk", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x4364FF85, "Dengeki Nurse 2 - More Sexy", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x699325A4, "Diamond Players", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xDDAD6C39, "Doki Doki Vacation - Kirameku Kisetsu no Naka de", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x39884F28, "Dynamic English 2 - Basic (Disc 1)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xB749F946, "Dynamic English 3 - Upper Basic (Disc 1) (Version 1.0)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x3777354A, "Eight Lakes G.C. (Course Disc)", GT_GAMEDB_FLOPPY_REQUIRED | GT_GAMEDB_FLOPPY_PROTECTED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xF7768015, "Emerald Dragon", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x7EC747A2, "Emit Vol. 1 - Toki no Maigo (En,Ja)", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x7219CABD, "Emit Vol. 2 - Inochigake no Tabi (En,Ja)", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xD98AC342, "Emit Vol. 3 - Watashi ni Sayonara o (En,Ja)", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xB8B8B737, "Empereur, L'", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x0E074247, "Engage Errands II - Kouki o Ninau Mono", GT_GAMEDB_FLOPPY_OPTIONAL, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xF5554267, "Europa Sensen", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xDE5662A4, "F29 Retaliator", GT_GAMEDB_FLOPPY_OPTIONAL, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xAE6AD5B2, "FM Towns Demonstration CD-ROM '90. Fuyu - The Galaxy of FM Towns", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xFAAD75B8, "Fujitsu Air Warrior V1.1", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xDFA6FA9D, "Fujitsu Air Warrior V1.2", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xBC0DF8CC, "Fujitsu Air Warrior V1.2 L11 (En,Ja)", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xEAAE0E23, "Fujitsu Air Warrior V2 - Version 2.1 Level 10 (En,Ja)", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xAE0D3427, "Fujitsu Habitat V1.1 L10", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x0C2BBF4E, "Fujitsu Habitat V2.1 L10", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xF20B6346, "Fujitsu Habitat V2.1 L13A", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x53C901F8, "G5", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x9A13BAC0, "G5 (Rev A)", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xB74E5198, "Ginga Eiyuu Densetsu III SP", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xFA9ED047, "Gou II - Towns Special", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x24244E28, "Gulf War - Soukouden", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x282791FD, "Hanafuda de Pon!", GT_GAMEDB_FLOPPY_OPTIONAL, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x54AED1D4, "Harukanaru Augusta (Course Disc)", GT_GAMEDB_FLOPPY_REQUIRED | GT_GAMEDB_FLOPPY_PROTECTED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x314C3E3E, "Hiouden II", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x7618CF43, "Hyper Address", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x8FB4CC67, "Hyper Address (Rev A)", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x356B92FC, "Hyper Address Ver. 2.0", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x99F83608, "Hyper Land - Doubutsu no Sekai", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x6023B391, "Hyper Note", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x18F327CD, "Hyper Planet", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xE6080E12, "Hyper Planet for Marty", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xF741BCF5, "Idol Project", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xC5C0C7C2, "Igo Doujou", GT_GAMEDB_FLOPPY_REQUIRED | GT_GAMEDB_FLOPPY_PROTECTED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x242E99DF, "Inindou - Datou Nobunaga", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x277FB617, "Iris-tei Serenade", GT_GAMEDB_FLOPPY_OPTIONAL, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x710F582A, "Ishin no Arashi", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x4E8C3CF7, "Jinmon Yuugi", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xEE22964C, "Joshikou Seifuku Monogatari", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x3AB52B19, "Kid Pix Jr.", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x43DBEEBC, "Kikou Shidan - Panzer Division II", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x9E440D92, "Kouryuuki", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x520D5479, "Lands of Lore - The Throne of Chaos", GT_GAMEDB_FLOPPY_OPTIONAL, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x5290D3C6, "Leading Company - Keys of the City", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x0FC706F8, "Legend of Kyrandia, The", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x6B5B5F99, "Lettuce Cooking - Tanoshiku Tsukureru Obentou", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xF721ACB6, "Lip 3 - Lipstick Adventure 3", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xC1358103, "Manami - Ai to Koukan no Hibi", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xF97F8227, "Ms. Detective File 2 - Sugata-naki Irainin", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xC7EF0483, "Multimedia Communication Virtual Town - TC-Station (Computer Gakushuu Program)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x185DCD80, "Multimedia Communication Virtual Town - TC-Station (Computer Graphics & Music)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xB3C7951D, "Multimedia Communication Virtual Town - TC-Station (Game Collection)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x2EFA5B47, "Multimedia Communication Virtual Town - TC-Station (Marty WorPro & Marty Hyoukeisan)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x86E28111, "Multimedia Communication Virtual Town - TC-Station (Photo Collection)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x0377C7BD, "Multimedia Communication Virtual Town - TC-Station (Virtual Town Tsuushin Program)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x96A0C889, "NHK Zoku Kiso Eigo - Dai-1-kan", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xB032B5B0, "NHK Zoku Kiso Eigo - Dai-2-kan", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xCC5D1CAB, "NHK Zoku Kiso Eigo - Dai-3-kan", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xCE930EED, "Nihongo Microsoft Windows V3.0 with Multimedia Extensions V1.0 L10", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x87E71E25, "Nobunaga no Yabou - Bushou Fuuunroku", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x830E82C4, "Nobunaga no Yabou - Haouden", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x698C98C6, "Nobunaga no Yabou - Sengoku Gunyuuden", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x34F3286E, "Nobunaga no Yabou - Tenshouki", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x3A89130D, "Noushuku Angel 120%", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x10C94488, "Planet Harmony", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x6D4F9A0C, "Power DoLLS", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xF14ED32F, "Pro Student G", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x38B0E36F, "Psychic Detective Series Final - Solitude Gekan", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x3E5A0090, "Psychic Detective Series Final - Solitude Joukan", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x92F3C78B, "Psychic Detective Series Vol. 1 - Invitation (DCCS)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xC5985559, "Psychic Detective Series Vol. 2 - Memories (DCCS)", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xA4F03968, "Psychic Detective Series Vol. 4 - Orgel", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x86440501, "Psychic Detective Series Vol. 5 - Nightmare", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xBB250646, "Queen of Duelist, The", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, GT_GAMEDB_PORT_PAD },
    { 0xFF5E877C, "Queen of Duellist Gaiden, The + Gaiden Alpha", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x9D583659, "Queen of Duellist, The (Rev A)", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, GT_GAMEDB_PORT_PAD },
    { 0x833FE0CF, "Rance IV - Kyoudan no Isan", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xD32CC1FF, "Record of Lodoss War - Lodoss-tou Senki - Haiiro no Majo", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xB2E7ED5B, "Record of Lodoss War - Lodoss-tou Senki II - Goshiki no Maryuu", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xA6959A28, "Royal Blood", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x7CA8F353, "Sangokushi II", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xA8162187, "Sangokushi III", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x52EA565E, "Sangokushi IV", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x45645B7E, "Scholar Movie Magazine Soukan-gou", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x4C517E3F, "Schwarzschild", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xC050F185, "Sensual Angels", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x0B4269C1, "Shamhat - The Holy Circlet", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x35980369, "Shinjuku Labyrinth", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xB6283BF9, "Shougaku Ongaku (5-6-nensei-you) (Rev A)", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xAB5A1C42, "Sid Meier's Railroad Tycoon", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x1AA48F1A, "Silent Service, The - Chinmoku no Kantai", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xCFA11167, "Soft de Hard na Monogatari", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xF2C65091, "Soft de Hard na Monogatari 2", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xBD0D3113, "Sotsugyou - Graduation '93", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xF1BAEB66, "Speech Trainer V1.0 L10", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x769A4475, "StarCruiser II for CD-ROM", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x97E9A343, "Suikoden - Tenmei no Chikai", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x417347FD, "Suzaku", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x17DE40BA, "Taiken Shiyou! Marty Channel", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x46765E77, "Taikou Risshiden", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x98EBCB1E, "Tamashii no Mon - Dante Shinkyoku yori", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x6FD4735C, "Tanjou - Debut", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x6FA4785E, "Teitoku no Ketsudan", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xB358974D, "Teitoku no Ketsudan II", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x4E35791F, "Tenka Gomen", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xC632FFF8, "Tenshin Ranma", GT_GAMEDB_FLOPPY_OPTIONAL, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xC46D0993, "Tokio - Tokyo-to Dai-24-ku", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xAF9CC2F9, "Tokyo Sexy Ave. - Tokyo Genshi Machi", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x98CB3719, "Touch the Music by Casiopea", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xB5CC95EF, "Towns-Telop", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xFFEBB254, "True Heart", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xA354A792, "Uchuu Kaitou Funny Bee", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xAACBCB13, "Vain Dream", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xF729FEBC, "Vain Dream II", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xF16C3E19, "Video Koubou V1.3 L10", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x8B73B12F, "Video Koubou V1.4 L10", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x32790894, "Viper GTS", GT_GAMEDB_FLOPPY_OPTIONAL, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xEEDF85C0, "Virtuacall", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xEFEC4CE6, "Virtuacall 2", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xBD27C936, "Winning Post", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x23E0854F, "Wizardry - Bane of the Cosmic Forge", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x985E0BF2, "Wizardry-V - Heart of the Maelstrom", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xF98F7C44, "Wrestle Angels 3 - Mezasu wa Saikyou Dantai!", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x829910E5, "Wrestle Angels Special", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xBC35A875, "Yami no Ketsuzoku Special - The Predestined Homicides (Rev 2)", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x551606EA, "Yayoi Umino Presents Private Slave", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x54005FB3, "Yubiwa Monogatari Dainikan - Futatsu no Tou", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xEE93FC42, "Yuuwaku", GT_GAMEDB_FLOPPY_BOOT, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0x15EF2C0A, "Zan II - Towns Special", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },
    { 0xD0529EC1, "Zan III - Ten'un Ware ni Ari - Towns Special", GT_GAMEDB_FLOPPY_REQUIRED, GT_MACHINE_MODEL1_2, 0, 0, 0, 0 },

    // FM Towns Model 1/2 firmware
    { 0x53319E23, "FM Towns Model 1/2 System ROM", GT_GAMEDB_FIRMWARE_SYSTEM, 0, 0, 0, 0, 0 },
    { 0x112872EE, "FM Towns Model 1/2 OS ROM", GT_GAMEDB_FIRMWARE_OS, 0, 0, 0, 0, 0 },
    { 0x955C6B75, "FM Towns Model 1/2 Font ROM", GT_GAMEDB_FIRMWARE_FONT, 0, 0, 0, 0, 0 },
    { 0xB314C659, "FM Towns Model 1/2 Dictionary ROM", GT_GAMEDB_FIRMWARE_DICTIONARY, 0, 0, 0, 0, 0 },

    // FM Towns II UX firmware (V03)
    { 0x92F3FA67, "FM Towns II UX System ROM", GT_GAMEDB_FIRMWARE_SYSTEM, 0, 0, 0, 0, 0 },
    { 0x22270E9F, "FM Towns II UX OS ROM", GT_GAMEDB_FIRMWARE_OS, 0, 0, 0, 0, 0 },
    { 0xDD6FD544, "FM Towns II Font ROM", GT_GAMEDB_FIRMWARE_FONT, 0, 0, 0, 0, 0 },
    { 0x82D1DAA2, "FM Towns II Dictionary ROM", GT_GAMEDB_FIRMWARE_DICTIONARY, 0, 0, 0, 0, 0 },
    { 0x5CC7E6BC, "FM Towns II UX Serial ROM", GT_GAMEDB_FIRMWARE_SERIAL, 0, 0, 0, 0, 0 },

    // FM Towns Marty firmware (V09)
    { 0x937311F6, "FM Towns Marty System ROM", GT_GAMEDB_FIRMWARE_SYSTEM, 0, 0, 0, 0, 0 },
    { 0x2BC2AF96, "FM Towns Marty OS ROM", GT_GAMEDB_FIRMWARE_OS, 0, 0, 0, 0, 0 },
    { 0xE1A9FA72, "FM Towns Marty Extension ROM", GT_GAMEDB_FIRMWARE_EXTENSION, 0, 0, 0, 0, 0 },
    { 0x75660AAC, "FM Towns II 20-dot Font ROM", GT_GAMEDB_FIRMWARE_FONT20, 0, 0, 0, 0, 0 },

    // FM Towns II HR firmware (V06)
    { 0x8AEFF982, "FM Towns II HR System ROM", GT_GAMEDB_FIRMWARE_SYSTEM, 0, 0, 0, 0, 0 },
    { 0xC52F0E89, "FM Towns II HR Serial ROM", GT_GAMEDB_FIRMWARE_SERIAL, 0, 0, 0, 0, 0 },

    // FM Towns II Fresh TV firmware (V10)
    { 0xAFE4EBCF, "FM Towns II Fresh TV System ROM", GT_GAMEDB_FIRMWARE_SYSTEM, 0, 0, 0, 0, 0 },
    { 0x9F55A20C, "FM Towns II Fresh TV 20-dot Font ROM", GT_GAMEDB_FIRMWARE_FONT20, 0, 0, 0, 0, 0 },
    { 0xA961AAE7, "FM Towns II Fresh TV Serial ROM", GT_GAMEDB_FIRMWARE_SERIAL, 0, 0, 0, 0, 0 },

    // Free FM Towns Project firmware
    { 0x65DC3B1F, "Free FM Towns System ROM", GT_GAMEDB_FIRMWARE_SYSTEM, 0, 0, 0, 0, 0 },
    { 0x43C18E92, "Free FM Towns OS ROM", GT_GAMEDB_FIRMWARE_OS, 0, 0, 0, 0, 0 },
    { 0x736D9CB1, "Free FM Towns Font ROM", GT_GAMEDB_FIRMWARE_FONT, 0, 0, 0, 0, 0 },
    { 0x504BF849, "Blank ROM", GT_GAMEDB_FIRMWARE_DICTIONARY | GT_GAMEDB_FIRMWARE_FONT20, 0, 0, 0, 0, 0 },

    { 0, 0, GT_GAMEDB_FLAG_NONE, 0, 0, 0, 0, 0 }
};

const GT_DB_Entry k_floppy_database[] =
{
    { 0xAFC56CB2, "Abunai Tengu Densetsu (Disk A)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 0, 0, GT_GAMEDB_PORT_PAD },
    { 0xB38D9C01, "Battle Fighters 2 - Yume o Kaketa Tatakai", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x2C2E3F39, "D.P.S - Dream Program System (Disk 1)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 0, 0, GT_GAMEDB_PORT_PAD },
    { 0x59C6C98D, "Half Moon ni Kawaru made - Ramiyaryou no Nijiiro Tamatebako (Disk A)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x7449A724, "Haou Soudatsu Battle Fighters", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x3495B01A, "Microsoft Windows Version 3.1 L10 Operating System (Setup Disk)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xB468312B, "Microsoft Windows Version 3.1 L10 Operating System (Setup Disk) (Model MX-MA)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x2E59D71C, "Microsoft Windows Version 3.1 L11 Operating System (Setup Disk)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x0A212AE7, "Microsoft Windows Version 3.1 L12 Operating System (Setup Disk)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0xC72AC1E2, "Might and Magic - World of Xeen (Install Disk)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x37F2BAE8, "Ms. Detective File 1 - Iwami Ginzan Satsujin Jiken (FM Towns Marty) (Disk)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MARTY, 0, 0, 0, 0 },
    { 0x80519723, "Psychic Detective Series Vol. 3 - Aya (DCCS) (Key-Disk)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x26F0B12B, "Psychic Detective Series Vol. 4 - Orgel (DCCS) (Key-Disk)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x9767DD06, "Psychic Detective Series Vol. 5 - Nightmare (DCCS) (Key-Disk)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x4C4B61CE, "Rance - Hikari o Motomete (Disk 1)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 0, 0, GT_GAMEDB_PORT_PAD },
    { 0x086BD554, "Rance 2 - Hangyaku no Shoujo-tachi (Disk A)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 0, 0, GT_GAMEDB_PORT_PAD },
    { 0xB0F49347, "Rance 2 Hint Disk", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 0, 0, 0, GT_GAMEDB_PORT_PAD },
    { 0x38A2C229, "Rumstorm", GT_GAMEDB_CPU_486, GT_MACHINE_HG, 0, 33, 0, 0 }, // MA, a 486 at 33 MHz or faster
    { 0x7208C490, "Samurai Spirits (Boot Disk)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },
    { 0x3C43B7DA, "Shamhat - The Holy Circlet (User Disk) (FM Towns Marty)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MARTY, 0, 0, 0, 0 },
    { 0xD58C19E0, "Uwaki na Anata - Switch o Irete (Boot Disk)", GT_GAMEDB_FLAG_NONE, GT_MACHINE_MODEL1_2, 4, 0, 0, 0 },

    { 0, 0, GT_GAMEDB_FLAG_NONE, 0, 0, 0, 0, 0 }
};

#endif /* GAME_DB_H */
