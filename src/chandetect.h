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

#ifndef _CHANDETECT_H
#define _CHANDETECT_H

#include <sigutils/sigutils.h>
#include <sigutils/ncqo.h>
#include <sigutils/iir.h>

#include <xsigtool.h>

enum xsig_channel_detector_mode {
  XSIG_CHANNEL_DETECTOR_MODE_DISCOVERY,       /* Discover channels */
  XSIG_CHANNEL_DETECTOR_MODE_CYCLOSTATIONARY, /* To find baudrate */
  XSIG_CHANNEL_DETECTOR_MODE_ORDER_ESTIMATION /* To find constellation size */
};

struct xsig_channel_detector_params {
  enum xsig_channel_detector_mode mode;
  unsigned int samp_rate;   /* Sample rate */
  unsigned int window_size; /* Window size == FFT bins */
  SUFLOAT alpha;            /* Damping factor */
  SUFLOAT fc;               /* Center frequency */
  unsigned int decimation;
  unsigned int max_order;   /* Max constellation order */
};

#define xsig_channel_detector_params_INITIALIZER \
{                         \
  XSIG_CHANNEL_DETECTOR_MODE_DISCOVERY, /* Mode */ \
  8000, /* samp_rate */   \
  512,  /* window_size */ \
  0.25, /* alpha */       \
  0.0,  /* fc */          \
  1,    /* decimation */  \
  8,    /* max_order */   \
}


struct xsig_channel {
  SUFLOAT fc;
  SUFLOAT bw;
  SUFLOAT snr;
};

struct xsig_channel_detector {
  struct xsig_channel_detector_params params;
  su_ncqo_t lo; /* Local oscillator */
  su_iir_filt_t antialias; /* Antialiasing filter */
  XSIG_FFTW(_complex) *window;
  XSIG_FFTW(_plan) fft_plan;
  XSIG_FFTW(_complex) *fft;
  SUFLOAT *averaged_fft;
  unsigned int decim_ptr;
  unsigned int ptr; /* Sample in window */
  unsigned int iters;
  PTR_LIST(struct xsig_channel, channel);
};

typedef struct xsig_channel_detector xsig_channel_detector_t;

xsig_channel_detector_t *xsig_channel_detector_new(const struct xsig_channel_detector_params *params);
void xsig_channel_detector_destroy(xsig_channel_detector_t *detector);
SUBOOL xsig_channel_detector_feed(xsig_channel_detector_t *detector, SUCOMPLEX samp);
void xsig_channel_detector_get_channel_list(
    const xsig_channel_detector_t *detector,
    struct xsig_channel ***channel_list,
    unsigned int *channel_count);

#endif /* _CHANDETECT_H */
