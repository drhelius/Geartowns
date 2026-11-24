/******************************************************************************
 * FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "fluid_rev.h"
#include "fluid_sys.h"
#include "fluid_rev_filters.h"
#include "fluid_rev_fdn.h"
#include "fluid_rev_freeverb.h"
#include "fluid_rev_lexverb.h"
#include "fluid_rev_dattorro.h"
#ifdef SIGNALSMITH_SUPPORT
#include "fluid_rev_signalsmith.h"
#endif

#include <exception>
#include <new>

fluid_revmodel_t *
new_fluid_revmodel(fluid_real_t sample_rate_max, fluid_real_t sample_rate, int reverb_type)
{
    fluid_revmodel_t *rev = NULL;

    if(sample_rate <= 0.0f || sample_rate_max <= 0.0f)
    {
        FLUID_LOG(FLUID_ERR, "Reverb initialization failed: sample rate must be positive");
        return NULL;
    }

    if(reverb_type == FLUID_REVERB_TYPE_FREEVERB)
    {
        rev = new fluid_revmodel_freeverb(sample_rate);
    }
    else if(reverb_type == FLUID_REVERB_TYPE_LEXVERB)
    {
        rev = new fluid_revmodel_lexverb(sample_rate);
    }
    else if(reverb_type == FLUID_REVERB_TYPE_DATTORRO)
    {
        rev = new fluid_revmodel_dattorro(sample_rate);
    }
#ifdef SIGNALSMITH_SUPPORT
    else if(reverb_type == FLUID_REVERB_TYPE_SIGNALSMITH)
    {
        rev = new fluid_revmodel_signalsmith(sample_rate);
    }
#endif
    else
    {
        rev = new fluid_revmodel_fdn(sample_rate_max, sample_rate);
    }

    return rev;
}

/*
 * free the reverb.
 * Note that while the reverb is used by calling any fluid_revmodel_processXXX()
 * function, calling delete_fluid_revmodel() isn't multi task safe because
 * delay line are freed. To deal properly with this issue follow the steps:
 *
 * 1) Stop reverb processing (i.e disable calling of any fluid_revmodel_processXXX().
 *    reverb functions.
 * 2) Delete the reverb by calling delete_fluid_revmodel().
 *
 * @param rev pointer on reverb to free.
 * Reverb API.
 */
void
delete_fluid_revmodel(fluid_revmodel_t *rev)
{
    delete rev;
}

void fluid_revmodel_processmix(fluid_revmodel_t *rev, const fluid_real_t *in,
                               fluid_real_t *left_out, fluid_real_t *right_out)
{
    fluid_return_if_fail(rev != NULL);
    rev->processmix(in, left_out, right_out);
}

void fluid_revmodel_processreplace(fluid_revmodel_t *rev, const fluid_real_t *in,
                                   fluid_real_t *left_out, fluid_real_t *right_out)
{
    fluid_return_if_fail(rev != NULL);
    rev->processreplace(in, left_out, right_out);
}

void fluid_revmodel_reset(fluid_revmodel_t *rev)
{
    fluid_return_if_fail(rev != NULL);
    rev->reset();
}

void fluid_revmodel_set(fluid_revmodel_t *rev, int set, fluid_real_t roomsize,
                        fluid_real_t damping, fluid_real_t width, fluid_real_t level)
{
    fluid_return_if_fail(rev != NULL);
    rev->set(set, roomsize, damping, width, level);
}

int fluid_revmodel_samplerate_change(fluid_revmodel_t *rev, fluid_real_t sample_rate)
{
    fluid_return_val_if_fail(rev != NULL, FLUID_FAILED);

    if(sample_rate <= 0.0f)
    {
        FLUID_LOG(FLUID_ERR, "Reverb samplerate change failed: sample rate must be positive");
        return FLUID_FAILED;
    }

    return rev->samplerate_change(sample_rate);
}
