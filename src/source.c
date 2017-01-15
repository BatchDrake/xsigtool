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

#include <string.h>
#include "source.h"

SUPRIVATE SUBOOL xsig_source_block_class_registered = SU_FALSE;

SUPRIVATE void
xsig_source_params_finalize(struct xsig_source_params *params)
{
  if (params->file != NULL)
    free((void *) params->file);
}

SUPRIVATE SUBOOL
xsig_source_params_copy(
    struct xsig_source_params *dest,
    const struct xsig_source_params *orig)
{
  memset(dest, 0, sizeof (struct xsig_source_params));

  if ((dest->file = strdup(orig->file)) == NULL)
    goto fail;

  dest->window_size = orig->window_size;
  dest->onacquire = orig->onacquire;
  dest->private = orig->private;

  return SU_TRUE;

fail:
  xsig_source_params_finalize(dest);

  return SU_FALSE;
}

void
xsig_source_destroy(struct xsig_source *source)
{
  xsig_source_params_finalize(&source->params);

  if (source->sf != NULL)
    sf_close(source->sf);

  if (source->fft_plan != NULL)
    XSIG_FFTW(_destroy_plan)(source->fft_plan);

  if (source->window != NULL)
    fftw_free(source->window);

  if (source->fft != NULL)
    fftw_free(source->fft);

  free(source);
}

struct xsig_source *
xsig_source_new(const struct xsig_source_params *params)
{
  struct xsig_source *new = NULL;

  if ((new = calloc(1, sizeof (struct xsig_source))) == NULL)
    goto fail;

  if (!xsig_source_params_copy(&new->params, params)) {
    SU_ERROR("failed to copy source params\n");
    goto fail;
  }

  if ((new->sf = sf_open(params->file, SFM_READ, &new->info)) == NULL) {
    SU_ERROR(
        "failed to open `%s': %s\n",
        params->file,
        sf_error(NULL));
    goto fail;
  }

  new->samp_rate = new->info.samplerate;

  if ((new->window = fftw_malloc(params->window_size * sizeof(SUFLOAT)))
      == NULL) {
    SU_ERROR("cannot allocate memory for read window\n");
    goto fail;
  }

  if ((new->fft
      = fftw_malloc(
          params->window_size * sizeof(XSIG_FFTW(_complex)))) == NULL) {
    SU_ERROR("cannot allocate memory for FFT\n");
    goto fail;
  }

  if ((new->fft_plan = XSIG_FFTW(_plan_dft_r2c_1d)(
      params->window_size,
      new->window,
      new->fft,
      FFTW_ESTIMATE)) == NULL) {
    SU_ERROR("failed to create FFT plan\n");
    goto fail;
  }

  return new;

fail:
  if (new != NULL)
    xsig_source_destroy(new);

  return NULL;
}

SUBOOL
xsig_source_read(struct xsig_source *source)
{
  return XSIG_SNDFILE_READ(
      source->sf,
      source->window,
      source->params.window_size) == source->params.window_size;
}

SUBOOL
xsig_source_acquire(struct xsig_source *source)
{
  if (!xsig_source_read(source)) {
    SU_ERROR("read failed\n");
    return SU_FALSE;
  }

  source->avail = source->params.window_size;
  XSIG_FFTW(_execute(source->fft_plan));

  if (source->params.onacquire != NULL)
    (source->params.onacquire)(source, source->params.private);

  return SU_TRUE;
}

/* Source as block */
SUPRIVATE SUBOOL
xsig_source_block_ctor(struct sigutils_block *block, void **private, va_list ap)
{
  SUBOOL ok = SU_FALSE;
  struct xsig_source *source = NULL;
  const struct xsig_source_params *params;

  params = va_arg(ap, const struct xsig_source_params *);

  if ((source = xsig_source_new(params)) == NULL) {
    SU_ERROR("Failed to initialize signal source");
    goto done;
  }

  ok = SU_TRUE;

  ok = ok && su_block_set_property_ref(
      block,
      SU_PROPERTY_TYPE_INTEGER,
      "samp_rate",
      &source->samp_rate);

  ok = ok && su_block_set_property_ref(
      block,
      SU_PROPERTY_TYPE_OBJECT,
      "instance",
      source);

done:
  if (!ok) {
    if (source != NULL)
      xsig_source_destroy(source);
  }
  else
    *private = source;

  return ok;
}

SUPRIVATE void
xsig_source_block_dtor(void *private)
{
  struct xsig_source *source = (struct xsig_source *) private;

  xsig_source_destroy(source);
}

SUPRIVATE SUSDIFF
xsig_source_block_acquire(void *private, su_stream_t *out, su_block_port_t *in)
{
  SUCOMPLEX *start;
  SUSDIFF size;
  SUSDIFF i;
  SUSDIFF ptr;
  struct xsig_source *source = (struct xsig_source *) private;

  size = su_stream_get_contiguous(out, &start, out->size);

  /* Ensure we can deliver something */
  if (source->avail == 0)
    if (!xsig_source_acquire(source))
      return SU_BLOCK_PORT_READ_END_OF_STREAM;

  if (size > source->avail)
    size = source->avail;

  ptr = source->params.window_size - source->avail;

  /* Convert data */
  for (i = 0; i < size; ++i)
    start[i] = (SUCOMPLEX) source->window[i + ptr];

  /* Advance in stream */
  if (su_stream_advance_contiguous(out, size) != size) {
    SU_ERROR("Unexpected size after su_stream_advance_contiguous\n");
    return -1;
  }

  /* Mark these samples as consumed */
  source->avail -= size;

  return size;
}

SUPRIVATE struct sigutils_block_class xsig_source_block_class = {
    "xsig_source", /* name */
    0,     /* in_size */
    1,     /* out_size */
    xsig_source_block_ctor,    /* constructor */
    xsig_source_block_dtor,    /* destructor */
    xsig_source_block_acquire  /* acquire */
};

SUPRIVATE SUBOOL
xsig_source_assert_block_class(void)
{
  if (!xsig_source_block_class_registered) {
    if (!su_block_class_register(&xsig_source_block_class)) {
      SU_ERROR("Failed to initialize xsig source block class\n");
      return SU_FALSE;
    }

    xsig_source_block_class_registered = SU_TRUE;
  }

  return SU_TRUE;
}

su_block_t *
xsig_source_create_block(const struct xsig_source_params *params)
{
  su_block_t *block = NULL;

  if (!xsig_source_assert_block_class()) {
    SU_ERROR("cannot assert xsig source block class\n");
    return NULL;
  }

  if ((block = su_block_new("xsig_source", params)) == NULL) {
    SU_ERROR("cannot initialize signal source block\n");
    return NULL;
  }

  return block;
}
