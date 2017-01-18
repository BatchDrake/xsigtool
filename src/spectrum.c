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

#include <xsigtool.h>
#include <stdlib.h>
#include <assert.h>

#include <sigutils/sampling.h>
#include "spectrum.h"

void
xsig_spectrum_destroy(xsig_spectrum_t *s) {
  unsigned int i;

  assert(s != NULL);

  if (s->fft != NULL)
    free(s->fft);

  free(s);
}

xsig_spectrum_t *
xsig_spectrum_new(const struct xsig_spectrum_params *params) {
  xsig_spectrum_t *new = NULL;
  unsigned int i;

  assert(params != NULL);
  assert(params->fft_size > 0);
  assert(params->width > 0);
  assert(params->height > 0);
  assert(params->alpha > 0);

  if ((new = calloc(1, sizeof (xsig_spectrum_t))) == NULL)
    goto fail;

  if ((new->fft = calloc(params->width, sizeof (SUFLOAT))) == NULL)
    goto fail;

  new->params = *params;

  return new;

fail:
  if (new != NULL)
    xsig_spectrum_destroy(new);

  return NULL;
}


void
xsig_spectrum_feed(xsig_spectrum_t *s, const SUCOMPLEX *x) {
  unsigned int i;
  unsigned int s_i;
  SUFLOAT s_index;
  SUFLOAT t;

  for (i = 0; i < s->params.width; ++i) {
    s_index = (SUFLOAT) i / (SUFLOAT) (s->params.width)
        * (SUFLOAT) (s->params.fft_size);
    s_i = (unsigned int) floor(s_index);
    t = s_index - s_i;

    s->fft[i] =
        s->params.alpha * (s_i == s->params.fft_size - 1
            ? SU_C_ABS(x[s_i])
            : (1. - t) * SU_C_ABS(x[s_i]) + t * SU_C_ABS(x[s_i + 1]))
        + (1. - s->params.alpha) * s->fft[i];
  }
}

void
xsig_spectrum_redraw(const xsig_spectrum_t *s, display_t *disp)
{
  int i, j, old_j;
  int chan_start;
  int x, y_1, y_2;
  SUFLOAT K = 1.  / s->params.fft_size;
  SUFLOAT dBFS;
  SUFLOAT noise_floor = INFINITY;
  SUFLOAT channel_threshold;
  SUFLOAT signal_ceil = -INFINITY;
  SUBOOL in_channel = SU_FALSE;

  box(
      disp,
      s->params.x,
      s->params.y,
      s->params.x + s->params.width  + 1,
      s->params.y + s->params.height + 1,
      OPAQUE(0x7f7f7f));

  fbox(
      disp,
      s->params.x + 1,
      s->params.y + 1,
      s->params.x + s->params.width,
      s->params.y + s->params.height,
      OPAQUE(0x000000));

  /* Search should start at lowest point */

  for (i = 0; i < s->params.width; ++i)
    if (s->fft[i] < noise_floor)
      noise_floor = s->fft[i];

  for (i = 0; i < s->params.width; ++i)
    if (s->fft[i] > signal_ceil)
      signal_ceil = s->fft[i];


  /* TODO: compute channel threshold based on dynamic range */
  /* TODO: Try removing the signal_ceil reference and use
   * a squelch level based on a proportion of the dynamic range
   */
  channel_threshold = .15 * signal_ceil + .85 * noise_floor;

  for (i = 0; i < s->params.width; ++i) {
    channel_threshold += 1 * (.1 * signal_ceil + .90 * noise_floor - channel_threshold);

    if (!in_channel) {
      if (s->fft[i] > channel_threshold) {
        in_channel = SU_TRUE;
        chan_start = i;
      } else
        noise_floor += .05 * (s->fft[i] - noise_floor);
    } else {
      /*
       * Don't immediately leave the channel. Assume guard bands,
       * require xxx Hz of continuous low SNR to assume that the channel
       * is over. Add these xxx Hz to the beginning of the channel.
       */
      if (s->fft[i] < channel_threshold)
        in_channel = SU_FALSE;
      else
        signal_ceil += .05 * (s->fft[i] - signal_ceil);
    }

    dBFS = SU_DB_RAW(s->fft[i] * K);

    j = s->params.height * s->params.scale
            * (1. - dBFS + s->params.ref);

    if (i > 0)
      if ((j > 0 && j < s->params.height)
          || (old_j > 0 && old_j < s->params.height)) {
        y_1 = j < 0
            ? 0
            : (j >= s->params.height
                ? s->params.height - 1
                : j);

        y_2 = old_j < 0
            ? 0
            : (old_j >= s->params.height
                ? s->params.height - 1
                : old_j);
        line(
            disp,
            s->params.x + i,
            s->params.y + y_1,
            s->params.x + i - 1,
            s->params.y + y_2,
            OPAQUE(0x00ff00));
      }

    pset_abs(
        disp,
        s->params.x + i,
        s->params.y + s->params.height * s->params.scale
        * (1. - SU_DB_RAW(channel_threshold * K) + s->params.ref),
        OPAQUE(0xff0000));

    if (in_channel)
      line(
          disp,
          s->params.x + i,
          s->params.y + 1,
          s->params.x + i,
          s->params.y + s->params.height,
          0x3fff0000);
    old_j = j;
  }
}

