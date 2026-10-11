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
        "Select which aspect ratio will be presented by the core. '1:1 PAR' produces square pixels.",
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
        "Speed up how fast the drive reads sectors. Some software may not expect a faster drive.",
        NULL,
        "cdrom",
        {
            { "1x", "1x (Original)" },
            { "2x", NULL },
            { "4x", NULL },
            { "8x", NULL },
            { NULL, NULL },
        },
        "1x"
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
        "geartowns_floppy_second",
        "Second Floppy at Start (restart)",
        NULL,
        "Insert the next floppy disk of the content into floppy drive 2 at start, for software that uses both drives.",
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
        "geartowns_ram_size",
        "RAM Size (restart)",
        NULL,
        "Main memory of the FM Towns Model 1/2. Some software needs more than the original 2 MB. Save states grow with it.",
        NULL,
        "system",
        {
            { "1 MB", NULL },
            { "2 MB", "2 MB (Original)" },
            { "4 MB", NULL },
            { "6 MB", NULL },
            { NULL, NULL },
        },
        "2 MB"
    },
    {
        "geartowns_cpu_speed",
        "CPU Speed (restart)",
        NULL,
        "Run the 80386 faster than the original 16 MHz. Some software may not expect a faster CPU.",
        NULL,
        "system",
        {
            { "16 MHz",  "16 MHz (Original)" },
            { "20 MHz",  NULL },
            { "25 MHz",  NULL },
            { "33 MHz",  NULL },
            { "40 MHz",  NULL },
            { "50 MHz",  NULL },
            { "66 MHz",  NULL },
            { "100 MHz", NULL },
            { NULL, NULL },
        },
        "16 MHz"
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
        "geartowns_floppy_drives",
        "Floppy Drives (restart)",
        NULL,
        "Number of internal floppy drives.",
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { NULL, NULL },
        },
        "2"
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
