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

#include "waterfall.h"

void
xsig_waterfall_destroy(xsig_waterfall_t *wf) {
  unsigned int i;

  assert(wf != NULL);

  if (wf->history != NULL) {
    for (i = 0; i < wf->params.height; ++i)
      if (wf->history[i] != NULL)
        free(wf->history[i]);

    free(wf->history);
  }

  free(wf);
}

xsig_waterfall_t *
xsig_waterfall_new(const struct xsig_waterfall_params *params) {
  xsig_waterfall_t *new = NULL;
  unsigned int i;

  assert(params != NULL);
  assert(params->fft_size > 0);
  assert(params->width > 0);
  assert(params->height > 0);

  if ((new = calloc(1, sizeof(xsig_waterfall_t))) == NULL)
    goto fail;

  if ((new->history = malloc(params->height * sizeof (SUCOMPLEX))) == NULL)
    goto fail;

  for (i = 0; i < params->height; ++i)
    if ((new->history[i] = calloc(params->width, sizeof (SUFLOAT))) == NULL)
      goto fail;

  new->params = *params;

  return new;

fail:
  if (new != NULL)
    xsig_waterfall_destroy(new);

  return NULL;
}


void
xsig_waterfall_feed(xsig_waterfall_t *wf, const SUCOMPLEX *s) {
  unsigned int i;
  unsigned int s_i;
  SUFLOAT s_index;
  SUFLOAT t;

  for (i = 0; i < wf->params.width; ++i) {
    s_index = (SUFLOAT) i / (SUFLOAT) (wf->params.width - 1)
        * (SUFLOAT) (wf->params.fft_size - 1);
    s_i = (unsigned int) floor(s_index);
    t = s_index - s_i;

    wf->history[wf->ptr][i] =
        s_i == wf->params.fft_size - 1
        ? SU_C_ABS(s[s_i])
        : (1. - t) * SU_C_ABS(s[s_i]) + t * SU_C_ABS(s[s_i + 1]);
  }

  if (++wf->ptr == wf->params.height)
    wf->ptr = 0;
}

SUPRIVATE SUFLOAT
xsig_waterfall_saturation(SUFLOAT x) {
  if (x > 1)
    return 1;

  return x;
}

static inline int
calc_color_wf (double idx)
{
  idx = xsig_waterfall_saturation(idx);

  return RED   ((int) (255.0 * idx))
        | GREEN ((int) (255.0 * idx))
        | BLUE  ((int) (255.0 * idx));
}

void
xsig_waterfall_redraw(const xsig_waterfall_t *wf, display_t *disp)
{
  unsigned int i, j;
  unsigned int row;
  box(
      disp,
      wf->params.x,
      wf->params.y,
      wf->params.x + wf->params.width  + 1,
      wf->params.y + wf->params.height + 1,
      OPAQUE(0x7f7f7f));

  for (j = 0; j < wf->params.height; ++j) {
    row = (j + wf->ptr) % wf->params.height;
    for (i = 0; i < wf->params.width; ++i)
      pset_abs(
          disp,
          i + wf->params.x + 1,
          j + wf->params.y + 1,
          OPAQUE(calc_color_wf(.5 * wf->history[row][i])));
  }
}

