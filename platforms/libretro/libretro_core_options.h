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

#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include "libretro.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
 */

struct retro_core_option_v2_category option_cats_us[] = {
    {
        "video",
        "Video",
        "Configure video output settings."
    },
    {
        "audio",
        "Audio",
        "Configure the volume of each sound source."
    },
    {
        "input",
        "Input",
        "Configure controller and mouse behavior."
    },
    {
        "cdrom",
        "CD-ROM",
        "Configure CD-ROM settings."
    },
    {
        "floppy",
        "Floppy",
        "Configure floppy drives and save disks."
    },
    {
        "system",
        "System",
        "Configure the emulated machine."
    },
    { NULL, NULL, NULL },
};

struct retro_core_option_v2_definition option_defs_us[] = {

    /* Video */
    {
        "geartowns_aspect_ratio",
        "Aspect Ratio",
        NULL,
        "Select which aspect ratio will be presented by the core. '1:1 PAR' produces square pixels at monitor resolution, also when Native Resolution outputs fewer pixels.",
        NULL,
        "video",
        {
            { "1:1 PAR",   NULL },
            { "4:3 DAR",   NULL },
            { "16:9 DAR",  NULL },
            { "16:10 DAR", NULL },
            { NULL, NULL },
        },
        "4:3 DAR"
    },
    {
        "geartowns_native_resolution",
        "Native Resolution",
        NULL,
        "Output one pixel per game pixel, as shaders expect. When disabled, zoomed modes are output at monitor resolution.",
        NULL,
        "video",
        {
            { "Enabled",  NULL },
            { "Disabled", NULL },
            { NULL, NULL },
        },
        "Enabled"
    },
    {
        "geartowns_frameskip",
        "Frameskip",
        NULL,
        "Skip frames to avoid audio crackling when the device is too slow. 'Auto' skips when the frontend reports that the audio buffer is about to run out, 'Manual' uses the threshold below.",
        NULL,
        "video",
        {
            { "Disabled", NULL },
            { "Auto",     NULL },
            { "Manual",   NULL },
            { NULL, NULL },
        },
        "Disabled"
    },
    {
        "geartowns_frameskip_threshold",
        "Frameskip Threshold (%)",
        NULL,
        "With 'Manual' frameskip, skip frames while the audio buffer is below this percentage. Higher values reduce crackling but skip more frames.",
        NULL,
        "video",
        {
            { "15", NULL }, { "18", NULL }, { "21", NULL }, { "24", NULL }, { "27", NULL }, { "30", NULL },
            { "33", NULL }, { "36", NULL }, { "39", NULL }, { "42", NULL }, { "45", NULL }, { "48", NULL },
            { "51", NULL }, { "54", NULL }, { "57", NULL }, { "60", NULL }, { NULL, NULL }
        },
        "33"
    },

    /* Audio */

    {
        "geartowns_fm_volume",
        "FM Volume",
        NULL,
        "Volume of the FM synthesizer, as a percentage where 100 is the default.",
        NULL,
        "audio",
        {
            { "0",   NULL }, { "10",  NULL }, { "20",  NULL }, { "30",  NULL }, { "40",  NULL },
            { "50",  NULL }, { "60",  NULL }, { "70",  NULL }, { "80",  NULL }, { "90",  NULL },
            { "100", NULL }, { "110", NULL }, { "120", NULL }, { "130", NULL }, { "140", NULL },
            { "150", NULL }, { "160", NULL }, { "170", NULL }, { "180", NULL }, { "190", NULL },
            { "200", NULL }, { NULL, NULL }
        },
        "100"
    },
    {
        "geartowns_pcm_volume",
        "PCM Volume",
        NULL,
        "Volume of the PCM sound source, as a percentage where 100 is the default.",
        NULL,
        "audio",
        {
            { "0",   NULL }, { "10",  NULL }, { "20",  NULL }, { "30",  NULL }, { "40",  NULL },
            { "50",  NULL }, { "60",  NULL }, { "70",  NULL }, { "80",  NULL }, { "90",  NULL },
            { "100", NULL }, { "110", NULL }, { "120", NULL }, { "130", NULL }, { "140", NULL },
            { "150", NULL }, { "160", NULL }, { "170", NULL }, { "180", NULL }, { "190", NULL },
            { "200", NULL }, { NULL, NULL }
        },
        "100"
    },
    {
        "geartowns_cdda_volume",
        "CD-DA Volume",
        NULL,
        "Volume of the CD-ROM audio tracks, as a percentage where 100 is the default.",
        NULL,
        "audio",
        {
            { "0",   NULL }, { "10",  NULL }, { "20",  NULL }, { "30",  NULL }, { "40",  NULL },
            { "50",  NULL }, { "60",  NULL }, { "70",  NULL }, { "80",  NULL }, { "90",  NULL },
            { "100", NULL }, { "110", NULL }, { "120", NULL }, { "130", NULL }, { "140", NULL },
            { "150", NULL }, { "160", NULL }, { "170", NULL }, { "180", NULL }, { "190", NULL },
            { "200", NULL }, { NULL, NULL }
        },
        "100"
    },
    {
        "geartowns_buzzer_volume",
        "Buzzer Volume",
        NULL,
        "Volume of the buzzer that plays system beeps, as a percentage where 100 is the default.",
        NULL,
        "audio",
        {
            { "0",   NULL }, { "10",  NULL }, { "20",  NULL }, { "30",  NULL }, { "40",  NULL },
            { "50",  NULL }, { "60",  NULL }, { "70",  NULL }, { "80",  NULL }, { "90",  NULL },
            { "100", NULL }, { "110", NULL }, { "120", NULL }, { "130", NULL }, { "140", NULL },
            { "150", NULL }, { "160", NULL }, { "170", NULL }, { "180", NULL }, { "190", NULL },
            { "200", NULL }, { NULL, NULL }
        },
        "100"
    },
    {
        "geartowns_pcm16_volume",
        "16-bit PCM Volume",
        NULL,
        "Volume of the 16-bit PCM of the FM Towns II ME and MF/Fresh, as a percentage where 100 is the default.",
        NULL,
        "audio",
        {
            { "0",   NULL }, { "10",  NULL }, { "20",  NULL }, { "30",  NULL }, { "40",  NULL },
            { "50",  NULL }, { "60",  NULL }, { "70",  NULL }, { "80",  NULL }, { "90",  NULL },
            { "100", NULL }, { "110", NULL }, { "120", NULL }, { "130", NULL }, { "140", NULL },
            { "150", NULL }, { "160", NULL }, { "170", NULL }, { "180", NULL }, { "190", NULL },
            { "200", NULL }, { NULL, NULL }
        },
        "100"
    },

    /* Input */

    {
        "geartowns_up_down_allowed",
        "Allow Opposing Directions",
        NULL,
        "Allow pressing opposite directions simultaneously. Disabling this may prevent movement glitches in some software.",
        NULL,
        "input",
        {
            { "Disabled", NULL },
            { "Enabled",  NULL },
            { NULL, NULL },
        },
        "Disabled"
    },
    {
        "geartowns_mouse_sensitivity",
        "Mouse Sensitivity",
        NULL,
        "Adjust the sensitivity of the FM Towns mouse. Higher values result in faster cursor movement.",
        NULL,
        "input",
        {
            { "1",  NULL }, { "2",  NULL }, { "3",  NULL }, { "4",  NULL },
            { "5",  NULL }, { "6",  NULL }, { "7",  NULL }, { "8",  NULL },
            { "9",  NULL }, { "10", NULL }, { "11", NULL }, { "12", NULL },
            { "13", NULL }, { "14", NULL }, { "15", NULL }, { NULL, NULL }
        },
        "5"
    },
    {
        "geartowns_analog_mouse",
        "Analog Stick Mouse",
        NULL,
        "Move the FM Towns mouse with an analog stick of the first controller, L2 and R2 are the mouse buttons. A controller port must be set to Mouse.",
        NULL,
        "input",
        {
            { "Disabled",    NULL },
            { "Left stick",  NULL },
            { "Right stick", NULL },
            { NULL, NULL },
        },
        "Disabled"
    },
    {
        "geartowns_analog_mouse_speed",
        "Analog Mouse Speed",
        NULL,
        "Adjust how fast the analog stick moves the FM Towns mouse.",
        NULL,
        "input",
        {
            { "1", NULL }, { "2", NULL }, { "3", NULL }, { "4", NULL }, { "5", NULL },
            { "6", NULL }, { "7", NULL }, { "8", NULL }, { "9", NULL }, { "10", NULL },
            { NULL, NULL }
        },
        "5"
    },

    /* CD-ROM */

    {
        "geartowns_cdrom_preload",
        "Preload CD-ROM (restart)",
        NULL,
        "Preload all CD-ROM tracks into RAM. This increases memory usage but may improve performance.",
        NULL,
        "cdrom",
        {
            { "Disabled", NULL },
            { "Enabled",  NULL },
            { NULL, NULL },
        },
        "Disabled"
    },
    {
        "geartowns_cdrom_speed",
        "CD-ROM Read Speed",
        NULL,
        "How fast the drive reads sectors. Auto reads at the speed of the model's drive: 2x on the FM Towns II ME and MF/Fresh in high-speed mode, 1x on the rest. Some software may not expect a faster drive.",
        NULL,
        "cdrom",
        {
            { "auto", "Auto" },
            { "1x", NULL },
            { "2x", NULL },
            { "4x", NULL },
            { "8x", NULL },
            { NULL, NULL },
        },
        "auto"
    },

    /* Floppy */

    {
        "geartowns_floppy_drive",
        "Disk Control Floppy Drive",
        NULL,
        "Select the floppy drive that the frontend's disk control ejects and inserts floppy disks into.",
        NULL,
        "floppy",
        {
            { "Floppy 1", NULL },
            { "Floppy 2", NULL },
            { NULL, NULL },
        },
        "Floppy 1"
    },
    {
        "geartowns_floppy_multidrive",
        "Floppy MultiDrive (restart)",
        NULL,
        "Also insert the floppy that follows the boot floppy into drive 2, for software that reads from both drives.",
        NULL,
        "floppy",
        {
            { "Disabled", NULL },
            { "Enabled",  NULL },
            { NULL, NULL },
        },
        "Disabled"
    },
    {
        "geartowns_save_disk",
        "Save Disk (restart)",
        NULL,
        "Add a blank 2HD floppy disk to the disk control list for software that asks for a user or save disk. It is stored in the save directory.",
        NULL,
        "floppy",
        {
            { "Disabled", NULL },
            { "Enabled",  NULL },
            { NULL, NULL },
        },
        "Disabled"
    },

    /* System */

    {
        "geartowns_model",
        "Model (restart)",
        NULL,
        "The FM Towns model to emulate. Auto picks the machine, CPU speed and RAM the game database names for the game, the FM Towns II HG for 1.44 MB floppy disks, and the FM Towns (Model 1/2) for other content. It is recommended to leave this option on Auto. RetroArch's 'Save Game Options' keeps a choice for one game.",
        NULL,
        "system",
        {
            { "auto",     "Auto" },
            { "model1_2", "FM Towns (Model 1/2) (80386DX 16 MHz)" },
            { "1f_2f",    "FM Towns 1F/2F (80386DX 16 MHz)" },
            { "10f_20f",  "FM Towns 10F/20F (80386DX 16 MHz)" },
            { "cx",       "FM Towns II CX (80386DX 16 MHz)" },
            { "ux",       "FM Towns II UX (80386SX 16 MHz)" },
            { "ug",       "FM Towns II UG (80386SX 20 MHz)" },
            { "hg",       "FM Towns II HG (80386DX 20 MHz)" },
            { "hr",       "FM Towns II HR (80486SX 20 MHz)" },
            { "ur",       "FM Towns II UR (80486SX 20 MHz)" },
            { "me",       "FM Towns II ME (80486SX 25 MHz)" },
            { "mf_fresh", "FM Towns II MF / Fresh (80486SX 33 MHz)" },
            { "marty",    "FM Towns Marty (80386SX 16 MHz)" },
            { NULL, NULL },
        },
        "auto"
    },
    {
        "geartowns_cpu_clock",
        "CPU Speed (restart)",
        NULL,
        "Auto runs the CPU at the model's original clock, or at the speed the game database names when Model is on Auto. Devices keep their real timing at any speed. Some software may not expect a faster CPU.",
        NULL,
        "system",
        {
            { "auto", "Auto" },
            { "16",   "16 MHz" },
            { "20",   "20 MHz" },
            { "25",   "25 MHz" },
            { "33",   "33 MHz" },
            { "40",   "40 MHz" },
            { "50",   "50 MHz" },
            { "66",   "66 MHz" },
            { "100",  "100 MHz" },
            { NULL, NULL },
        },
        "auto"
    },
    {
        "geartowns_main_ram",
        "Main RAM (restart)",
        NULL,
        "Auto uses the model's original RAM, or the RAM the game database names when Model is on Auto. A size the model doesn't offer uses its next size up, or its largest. Some software needs more than 2 MB. Save states grow with it.",
        NULL,
        "system",
        {
            { "auto", "Auto" },
            { "1",    "1 MB" },
            { "2",    "2 MB" },
            { "4",    "4 MB" },
            { "6",    "6 MB" },
            { "8",    "8 MB" },
            { "10",   "10 MB" },
            { "12",   "12 MB" },
            { "16",   "16 MB" },
            { "20",   "20 MB" },
            { "24",   "24 MB" },
            { "26",   "26 MB" },
            { "28",   "28 MB" },
            { "32",   "32 MB" },
            { "48",   "48 MB" },
            { "64",   "64 MB" },
            { "68",   "68 MB" },
            { NULL, NULL },
        },
        "auto"
    },
    {
        "geartowns_fpu",
        "FPU (restart)",
        NULL,
        "The math coprocessor: an 80387 card on the 386 models, a 487SX card on the HR, ME and MF/Fresh. Auto fits one when the game database names one for the game and Model is on Auto, otherwise it keeps the model's original, none on every emulated model. The UX, UG, UR and Marty have no socket for it. A few games need one, and some run different code with one fitted.",
        NULL,
        "system",
        {
            { "auto",  "Auto" },
            { "none",  "None" },
            { "80387", "80387 or 487SX" },
            { NULL, NULL },
        },
        "auto"
    },
    {
        "geartowns_speed_mode",
        "Speed Mode (restart)",
        NULL,
        "The FM Towns II models, except the UX, run in compatible or high-speed mode. High speed cuts memory waits and turns on the 486 cache and the 2x CD-ROM where fitted. The system ROM sets the mode at reset from the CMOS, where TownsMENU saves it. Auto uses high speed when Model is on Auto and the game database picks a 486 model, otherwise it keeps the CMOS setting, compatible on a new CMOS. Some software misbehaves in high-speed mode.",
        NULL,
        "system",
        {
            { "auto",       "Auto" },
            { "compatible", "Compatible" },
            { "high",       "High Speed" },
            { NULL, NULL },
        },
        "auto"
    },
    {
        "geartowns_boot_device",
        "Boot Device (restart)",
        NULL,
        "Holds the system ROM's boot keys at power-on and on every restart: C+D for the CD-ROM, F+0 or F+1 for a floppy drive, H+0 to H+4 for a SCSI hard disk, I+C+M for the IC memory card, released at the first read from a disk. The 1F/2F and later models read them; the Model 1/2 and Marty ROMs don't. Auto boots as usual, the CD first, so a hard disk boots with a bootable CD in the drive only when it is chosen here. A memory card that starts with a boot sector boots ahead of the CD on every model.",
        NULL,
        "system",
        {
            { "auto",    "Auto" },
            { "cdrom",   "CD-ROM" },
            { "floppy1", "Floppy 1" },
            { "floppy2", "Floppy 2" },
            { "hdd0",    "Hard Disk ID 0" },
            { "hdd1",    "Hard Disk ID 1" },
            { "hdd2",    "Hard Disk ID 2" },
            { "hdd3",    "Hard Disk ID 3" },
            { "hdd4",    "Hard Disk ID 4" },
            { "card",    "IC Memory Card" },
            { NULL, NULL },
        },
        "auto"
    },
    {
        "geartowns_drive_letters",
        "Assign Drive Letters (restart)",
        NULL,
        "Towns OS keeps hard disk drive letters in the CMOS, not on the disk. When enabled, each restart gives free letters to the partitions the CMOS doesn't list yet, so disks set up elsewhere boot. When disabled, register them yourself in Towns OS.",
        NULL,
        "system",
        {
            { "Enabled",  NULL },
            { "Disabled", NULL },
            { NULL, NULL },
        },
        "Enabled"
    },
    {
        "geartowns_memory_card",
        "IC Memory Card (restart)",
        NULL,
        "Puts an IC memory card in the slot, kept in the save directory. 'Per Game' uses a card named after the content, 'Shared' one card for all content. A missing card is created blank with the size below; Towns OS formats it with TICMFMT once CONFIG.SYS loads TICM.SYS.",
        NULL,
        "system",
        {
            { "Off",      NULL },
            { "Per Game", NULL },
            { "Shared",   NULL },
            { NULL, NULL },
        },
        "Off"
    },
    {
        "geartowns_memory_card_size",
        "IC Memory Card Size (restart)",
        NULL,
        "Size of a new blank IC memory card. An existing card keeps its size. The UG reaches the first 1 MB of a card, the Model 1/2, 1F/2F and 10F/20F the first 16 MB.",
        NULL,
        "system",
        {
            { "64",    "64 KB" },
            { "128",   "128 KB" },
            { "256",   "256 KB" },
            { "512",   "512 KB" },
            { "1024",  "1 MB" },
            { "2048",  "2 MB" },
            { "4096",  "4 MB" },
            { "8192",  "8 MB" },
            { "16384", "16 MB" },
            { "32768", "32 MB" },
            { "65536", "64 MB" },
            { NULL, NULL },
        },
        "1024"
    },
    {
        "geartowns_rtc",
        "Clock Start (restart)",
        NULL,
        "Start the FM Towns clock from the host time or from a fixed date (January 1, 1990). A fixed date makes netplay and replays independent of each computer's clock.",
        NULL,
        "system",
        {
            { "Host time",  NULL },
            { "Fixed date", NULL },
            { NULL, NULL },
        },
        "Host time"
    },
    {
        "geartowns_cmos",
        "CMOS (restart)",
        NULL,
        "Where the battery-backed CMOS with the machine settings and the OAK/2 user dictionary is kept. 'Shared' keeps one CMOS for all content in the save directory, like the real machine. 'Per Game' keeps a separate CMOS in each content's save file, a better choice for netplay.",
        NULL,
        "system",
        {
            { "Shared",   NULL },
            { "Per Game", NULL },
            { NULL, NULL },
        },
        "Shared"
    },
#if !defined(GT_DISABLE_DYNAREC)
    {
        "geartowns_dynarec",
        "CPU Dynarec",
        NULL,
        "Use dynamic recompilation. Most games will run faster but some games may run slower.",
        NULL,
        "system",
        {
            { "Enabled",  NULL },
            { "Disabled", NULL },
            { NULL, NULL },
        },
        "Enabled"
    },
#endif

    { NULL, NULL, NULL, NULL, NULL, NULL, {{0, 0}}, NULL },
};

struct retro_core_options_v2 options_us = {
    option_cats_us,
    option_defs_us
};

/*
 ********************************
 * Functions
 ********************************
 */

static void libretro_set_core_options(retro_environment_t environ_cb,
        bool *categories_supported)
{
    unsigned version = 0;

    if (!environ_cb || !categories_supported)
        return;

    *categories_supported = false;

    if (!environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))
        version = 0;

    if (version >= 2)
    {
        *categories_supported = environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2,
                &options_us);
    }
    else
    {
        size_t i, j;
        size_t option_index  = 0;
        size_t num_options   = 0;
        struct retro_core_option_definition *option_v1_defs_us = NULL;
        struct retro_variable *variables   = NULL;
        char **values_buf                  = NULL;

        while (true)
        {
            if (option_defs_us[num_options].key)
                num_options++;
            else
                break;
        }

        if (version >= 1)
        {
            option_v1_defs_us = (struct retro_core_option_definition *)
                    calloc(num_options + 1, sizeof(struct retro_core_option_definition));

            for (i = 0; i < num_options; i++)
            {
                struct retro_core_option_v2_definition *option_def_us = &option_defs_us[i];
                struct retro_core_option_value *option_values         = option_def_us->values;
                struct retro_core_option_definition *option_v1_def_us = &option_v1_defs_us[i];
                struct retro_core_option_value *option_v1_values      = option_v1_def_us->values;

                option_v1_def_us->key           = option_def_us->key;
                option_v1_def_us->desc          = option_def_us->desc;
                option_v1_def_us->info          = option_def_us->info;
                option_v1_def_us->default_value = option_def_us->default_value;

                while (option_values->value)
                {
                    option_v1_values->value = option_values->value;
                    option_v1_values->label = option_values->label;

                    option_values++;
                    option_v1_values++;
                }
            }

            environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, option_v1_defs_us);
        }
        else
        {
            variables  = (struct retro_variable *)calloc(num_options + 1,
                    sizeof(struct retro_variable));
            values_buf = (char **)calloc(num_options, sizeof(char *));

            if (!variables || !values_buf)
                goto error;

            for (i = 0; i < num_options; i++)
            {
                const char *key                        = option_defs_us[i].key;
                const char *desc                       = option_defs_us[i].desc;
                const char *default_value              = option_defs_us[i].default_value;
                struct retro_core_option_value *values  = option_defs_us[i].values;
                size_t buf_len                         = 3;
                size_t default_index                   = 0;

                values_buf[i] = NULL;

                if (desc)
                {
                    size_t num_values = 0;

                    while (true)
                    {
                        if (values[num_values].value)
                        {
                            if (default_value)
                                if (strcmp(values[num_values].value, default_value) == 0)
                                    default_index = num_values;

                            buf_len += strlen(values[num_values].value);
                            num_values++;
                        }
                        else
                            break;
                    }

                    if (num_values > 0)
                    {
                        buf_len += num_values - 1;
                        buf_len += strlen(desc);

                        values_buf[i] = (char *)calloc(buf_len, sizeof(char));
                        if (!values_buf[i])
                            goto error;

                        strcpy(values_buf[i], desc);
                        strcat(values_buf[i], "; ");

                        strcat(values_buf[i], values[default_index].value);

                        for (j = 0; j < num_values; j++)
                        {
                            if (j != default_index)
                            {
                                strcat(values_buf[i], "|");
                                strcat(values_buf[i], values[j].value);
                            }
                        }
                    }
                }

                variables[option_index].key   = key;
                variables[option_index].value = values_buf[i];
                option_index++;
            }

            environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
        }

error:
        if (option_v1_defs_us)
        {
            free(option_v1_defs_us);
            option_v1_defs_us = NULL;
        }

        if (values_buf)
        {
            for (i = 0; i < num_options; i++)
            {
                if (values_buf[i])
                {
                    free(values_buf[i]);
                    values_buf[i] = NULL;
                }
            }

            free(values_buf);
            values_buf = NULL;
        }

        if (variables)
        {
            free(variables);
            variables = NULL;
        }
    }
}

#ifdef __cplusplus
}
#endif

#endif
