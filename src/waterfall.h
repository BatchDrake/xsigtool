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

#ifndef _WATERFALL_H
#define _WATERFALL_H

#include <sigutils/sigutils.h>

struct xsig_waterfall_params {
  unsigned int fft_size;
  unsigned int width;
  unsigned int height;
  unsigned int x;
  unsigned int y;
};

struct xsig_waterfall {
  struct xsig_waterfall_params params;
  SUFLOAT **history;
  unsigned int ptr;
  SUFLOAT k; /* dynamic atenuation */
};

typedef struct xsig_waterfall xsig_waterfall_t;

void xsig_waterfall_destroy(xsig_waterfall_t *wf);
xsig_waterfall_t *xsig_waterfall_new(const struct xsig_waterfall_params *params);
void xsig_waterfall_feed(xsig_waterfall_t *wf, const SUCOMPLEX *fft);
void xsig_waterfall_redraw(const xsig_waterfall_t *wf, display_t *disp);

#endif /* _WATERFALL_H */
