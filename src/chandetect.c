/*

  Copyright (C) 2016 Gonzalo José Carracedo Carballal

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see
  <http://www.gnu.org/licenses/>

*/

#include <sigutils/sampling.h>
#include <assert.h>

#include "chandetect.h"

/*
 * Channel detector use case will work as follows:
 *
 * ---- CHANNEL DISCOVERY ----
 * 1. Initialize with a given damping factor for the FFT.
 * 2. Feed it with samples.
 * 3. After some runs, perform channel detection and register a channel.
 * 4. Optionally, rerun channel detection and adjust channel parameters.
 *
 * ---- CYCLOSTATIONARY ANALYSIS ----
 * 1. Initialize:
 *    1. Same damping factor.
 *    2. Tune to the center frequency of the selected channel
 *    3. Set mode to XSIG_CHANNEL_DETECTOR_MODE_CYCLOSTATIONARY
 * 2. Feed it with samples. Channel detector will compute the FFT of
 *    the current signal times the conjugate of the previous sinal.
 * 3. Apply peak detector in the channel frequency range.
 * 4. First peak should be the baudrate. Accuracy of the estimation can be
 *    improved by increasing the decimation.
 *
 * ---- ORDER ESTIMATION ----
 * 1. Initialize:
 *    1. Same damping factor.
 *    2. Tune to the center frequency of the selected channel
 *    3. Set mode to XSIG_CHANNEL_DETECTOR_MODE_ORDER ESTIMATION
 * 2. Feed it with samples (this will compute a power).
 * 3. Apply peak detector in the channel frequency range after some runs.
 * 4. At least two peaks should appear. If a third peak appears in the
 *    center frequency, order has been found. Otherwise, estimator should
 *    be reset and start again.
 */

void
xsig_channel_detector_destroy(xsig_channel_detector_t *detector)
{
  if (detector->fft_plan != NULL)
    XSIG_FFTW(_destroy_plan)(detector->fft_plan);

  if (detector->window != NULL)
    fftw_free(detector->window);

  if (detector->fft != NULL)
    fftw_free(detector->fft);

  if (detector->averaged_fft != NULL)
    free(detector->averaged_fft);

  free(detector);
}

xsig_channel_detector_t *
xsig_channel_detector_new(const struct xsig_channel_detector_params *params)
{
  xsig_channel_detector_t *new = NULL;

  assert(params->alpha > .0);
  assert(params->samp_rate > 0);
  assert(params->window_size > 0);
  assert(params->decimation > 0);

  if (params->mode != XSIG_CHANNEL_DETECTOR_MODE_DISCOVERY) {
    SU_ERROR("unsupported mode\n");
    goto fail;
  }

  if ((new = calloc(1, sizeof (xsig_channel_detector_t))) == NULL)
    goto fail;

  new->params = *params;

  if ((new->window
      = fftw_malloc(
          params->window_size * sizeof(XSIG_FFTW(_complex)))) == NULL) {
    SU_ERROR("cannot allocate memory for window\n");
    goto fail;
  }

  if ((new->fft
      = fftw_malloc(
          params->window_size * sizeof(XSIG_FFTW(_complex)))) == NULL) {
    SU_ERROR("cannot allocate memory for FFT\n");
    goto fail;
  }

  if ((new->averaged_fft
        = calloc(params->window_size, sizeof(SUFLOAT))) == NULL) {
      SU_ERROR("cannot allocate memory for averaged FFT\n");
      goto fail;
    }

  if ((new->fft_plan = XSIG_FFTW(_plan_dft_1d)(
      params->window_size,
      new->window,
      new->fft,
      FFTW_FORWARD,
      FFTW_ESTIMATE)) == NULL) {
    SU_ERROR("failed to create FFT plan\n");
    goto fail;
  }

  su_ncqo_init(
      &new->lo,
      SU_ABS2NORM_FREQ(params->samp_rate, params->fc * params->decimation));

  if (params->decimation > 1) {
    su_iir_bwlpf_init(&new->antialias, 5, .5 / params->decimation);
  }

  return new;

fail:
  if (new != NULL)
    xsig_channel_detector_destroy(new);

  return NULL;
}

SUPRIVATE SUBOOL
xsig_channel_perform_discovery(xsig_channel_detector_t *detector)
{
  int i;
  SUBOOL in_channel = SU_FALSE;
  SUFLOAT min = INFINITY;
  SUFLOAT max = -INFINITY;

  SUFLOAT curr_max;

  /* Analyze signal's dynamic range */
  for (i = 0; i < detector->params.window_size; ++i) {
    if (detector->averaged_fft[i] < min)
      min = detector->averaged_fft[i];

    if (detector->averaged_fft[i] > max)
      max = detector->averaged_fft[i];
  }

  curr_max = .5 * max + .5 * min;

  SU_INFO(
      "----------- Channels %d/%d --------------\n",
      detector->params.samp_rate,
      detector->params.decimation);

  for (i = 0; i < (detector->params.window_size >> 1); ++i) {
    if (detector->averaged_fft[i] > curr_max) {
      if (!in_channel) {
        SU_INFO(
            "Channel start (%lg Hz)\n",
            SU_NORM2ABS_FREQ(
                detector->params.samp_rate * detector->params.decimation,
                2 * (SUFLOAT) i / (SUFLOAT) detector->params.window_size));
        in_channel = SU_TRUE;
      }
    } else {
      if (in_channel) {
        SU_INFO(
            "Channel stop  (%lg Hz)\n",
            SU_NORM2ABS_FREQ(
                detector->params.samp_rate * detector->params.decimation,
                2 * (SUFLOAT) i / (SUFLOAT) detector->params.window_size));
        in_channel = SU_FALSE;
      }
    }
  }
  SU_INFO("----------- End of channel analysis --------------\n");

  return SU_TRUE;
}

SUBOOL
xsig_channel_detector_feed(xsig_channel_detector_t *detector, SUCOMPLEX samp)
{
  unsigned int i;
  SUCOMPLEX x;

  if (detector->params.decimation > 1) {
    /* If we are decimating, we take samples from the antialias filter */
    samp = su_iir_filt_feed(&detector->antialias, samp);

    /* Decimation takes place here */
    if (++detector->decim_ptr < detector->params.decimation)
      return SU_TRUE;

    detector->decim_ptr = 0; /* Reset decimation pointer */
  }

  switch (detector->params.mode) {
    case XSIG_CHANNEL_DETECTOR_MODE_DISCOVERY:
      /* Channel discovery is performed on the current sample only */
      x = samp;
      break;

    default:
      SU_WARNING("Mode not implemented\n");
      return SU_FALSE;
  }

  detector->window[detector->ptr++] = x;

  if (detector->ptr == detector->params.window_size) {
    detector->ptr = 0;

    /* Window is full, perform FFT */
    XSIG_FFTW(_execute(detector->fft_plan));

    /* Average FFT */
    for (i = 0; i < detector->params.window_size; ++i)
      detector->averaged_fft[i] =
          detector->params.alpha * SU_C_ABS(detector->fft[i]) +
          (1. - detector->params.alpha) * detector->averaged_fft[i];


    switch (detector->params.mode) {
      case XSIG_CHANNEL_DETECTOR_MODE_DISCOVERY:
        return xsig_channel_perform_discovery(detector);

      default:
        SU_WARNING("Mode not implemented\n");
        return SU_FALSE;
    }
  }

  return SU_TRUE;
}
