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

#include "constellation.h"

void
xsig_constellation_destroy(xsig_constellation_t *constellation) {
  assert(constellation != NULL);

  if (constellation->history != NULL)
    free(constellation->history);

  free(constellation);
}

xsig_constellation_t *
xsig_constellation_new(const struct xsig_constellation_params *params) {
  xsig_constellation_t *new = NULL;

  assert(params != NULL);
  assert(params->history_size > 0);
  assert(params->width > 0);
  assert(params->height > 0);

  if ((new = calloc(1, sizeof(xsig_constellation_t))) == NULL)
    goto fail;

  if ((new->history = malloc(params->history_size * sizeof (SUCOMPLEX))) == NULL)
    goto fail;

  new->params = *params;

  return new;

fail:
  if (new != NULL)
    xsig_constellation_destroy(new);

  return NULL;
}


void
xsig_constellation_feed(xsig_constellation_t *constellation, SUCOMPLEX s) {
  constellation->history[constellation->p++] = s;

  if (constellation->size < constellation->params.history_size)
    ++constellation->size;
  if (constellation->p == constellation->params.history_size)
    constellation->p = 0;
}

static void
xsig_draw_polar_grid(const struct xsig_constellation_params *params, display_t *disp)
{
  int i;
  float r = (params->height >> 1);
  float rx, ry;

  for (i = 1; i <= 10; ++i)
    circle(
        disp,
        (int) floor(params->x + r),
        (int) floor(params->y + r),
        (int) floor(r * .1 * i),
        OPAQUE(0x1f1f1f));

  for (i = 0; i < 6; ++i)
    line(
        disp,
        (int) floor(params->x + r * (1 + cos(M_PI / 6. * i))),
        (int) floor(params->y + r * (1 + sin(M_PI / 6. * i))),
        (int) floor(params->x - r * (-1 + cos(M_PI / 6. * i))),
        (int) floor(params->y - r * (-1 + sin(M_PI / 6. * i))),
        OPAQUE(0x1f1f1f));
}

void
xsig_constellation_redraw(const xsig_constellation_t *constellation, display_t *disp)
{
  int i, j;
  int old_i, old_j;
  double Is, Qs;
  unsigned int k;
  int whalf = constellation->params.width >> 1;
  int hhalf = constellation->params.height >> 1;

  /* Clear area */
  fbox(
      disp,
      constellation->params.x,
      constellation->params.y,
      constellation->params.x + constellation->params.width - 1,
      constellation->params.y + constellation->params.height - 1,
      OPAQUE(0));

  /* Draw polar grid */
  xsig_draw_polar_grid(&constellation->params, disp);

  /* Add axes */
  box(
      disp,
      constellation->params.x,
      constellation->params.y,
      constellation->params.x + constellation->params.width - 1,
      constellation->params.y + constellation->params.height - 1,
      OPAQUE(0x7f7f7f));

  line(
      disp,
      constellation->params.x + whalf,
      constellation->params.y,
      constellation->params.x + whalf,
      constellation->params.y + constellation->params.height - 1,
      OPAQUE(0x3f3f3f));

  line(
      disp,
      constellation->params.x,
      constellation->params.y + hhalf,
      constellation->params.x + constellation->params.width - 1,
      constellation->params.y + hhalf,
      OPAQUE(0x3f3f3f));

  /* Draw points */
  for (k = 0; k < constellation->size; ++k) {
    Is =  constellation->params.scaling * SU_C_REAL(constellation->history[k]);
    Qs = -constellation->params.scaling * SU_C_IMAG(constellation->history[k]);

    i = Is * constellation->params.width  + whalf;
    j = Qs * constellation->params.height + hhalf;

    if (i >= 2 && i < constellation->params.width - 2 &&
        j >= 2 && j < constellation->params.height - 2)
      mark(
          disp,
          constellation->params.x + i,
          constellation->params.y + j,
          1,
          0x7fffffff,
          0x7f00ff00);
/*
    if (k > 0)
      line(disp, i, j, old_i, old_j, 0x7f00ff00);
*/
    old_i = i;
    old_j = j;
  }
}

