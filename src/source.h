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

#ifndef _SOURCE_H
#define _SOURCE_H
#include <sigutils/sigutils.h>
#include <util/util.h>
#include <sndfile.h>
#include <fftw3.h>

#define XSIG_SOURCE_FFTW_PREFIX fftw
#define XSIG_SNDFILE_READ sf_read_double
#define XSIG_FFTW(method) JOIN(XSIG_SOURCE_FFTW_PREFIX, method)

struct xsig_source;

struct xsig_source_params {
  const char *file;
  SUSCOUNT window_size;
  void *private;
  void (*onacquire) (struct xsig_source *source, void *private);
};

struct xsig_source {
  struct xsig_source_params params;
  SF_INFO info;
  uint64_t samp_rate;
  SNDFILE *sf;
  SUFLOAT *window;
  XSIG_FFTW(_plan) fft_plan;
  XSIG_FFTW(_complex) *fft;
  SUSCOUNT avail;
};

void xsig_source_destroy(struct xsig_source *source);
struct xsig_source *xsig_source_new(const struct xsig_source_params *params);
SUBOOL xsig_source_read(struct xsig_source *source);
SUBOOL xsig_source_acquire(struct xsig_source *source);

su_block_t *xsig_source_create_block(const struct xsig_source_params *params);

#endif /* _SOURCE_H */
