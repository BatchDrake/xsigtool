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

#ifndef _SPECTRUM_H
#define _SPECTRUM_H

#include <sigutils/sigutils.h>

struct xsig_spectrum_params {
  unsigned int fft_size;
  unsigned int width;
  unsigned int height;
  unsigned int x;
  unsigned int y;
  SUFLOAT alpha;
  SUFLOAT scale;
  SUFLOAT ref;
};

struct xsig_spectrum {
  struct xsig_spectrum_params params;
  SUFLOAT *fft;
};

typedef struct xsig_spectrum xsig_spectrum_t;

void xsig_spectrum_destroy(xsig_spectrum_t *s);
xsig_spectrum_t *xsig_spectrum_new(const struct xsig_spectrum_params *params);
void xsig_spectrum_feed(xsig_spectrum_t *s, const SUCOMPLEX *fft);
void xsig_spectrum_redraw(const xsig_spectrum_t *s, display_t *disp);

#endif /* _SPECTRUM_H */
