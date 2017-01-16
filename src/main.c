/*
 * main.c: entry point for xsigtool
 * Creation date: Wed Jan 11 20:35:22 2017
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include <xsigtool.h>

#include <sigutils/sampling.h>
#include <sigutils/ncqo.h>
#include <sigutils/iir.h>
#include <sigutils/agc.h>
#include <sigutils/pll.h>

#include <sigutils/sigutils.h>

#include "constellation.h"
#include "waterfall.h"
#include "source.h"
#include "chandetect.h"

#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480

struct xsig_interface {
  xsig_waterfall_t *wf;
  xsig_channel_detector_t *cd;
};

SUPRIVATE void
xsigtool_onacquire(struct xsig_source *source, void *private)
{
  unsigned int i;
  struct xsig_interface *iface = (struct xsig_interface *) private;

  xsig_waterfall_feed(iface->wf, source->fft);

  for (i = 0; i < source->params.window_size; ++i)
    xsig_channel_detector_feed(iface->cd, source->window[i]);
}

SUBOOL
su_modem_set_xsig_source(
    su_modem_t *modem,
    const char *path,
    struct xsig_source **instance)
{
  su_block_t *xsig_source_block = NULL;
  const uint64_t *samp_rate = NULL;
  struct xsig_source_params params;

  params.file = path;
  params.onacquire = NULL;
  params.private = NULL;
  params.window_size = 512;
  params.onacquire = xsigtool_onacquire;

  if ((xsig_source_block = xsig_source_create_block(&params)) == NULL)
    goto fail;

  if ((samp_rate = su_block_get_property_ref(
      xsig_source_block,
      SU_PROPERTY_TYPE_INTEGER,
      "samp_rate")) == NULL) {
    SU_ERROR("failed to acquire xsig source sample rate\n");
    goto fail;
  }

  if ((*instance = su_block_get_property_ref(
      xsig_source_block,
      SU_PROPERTY_TYPE_OBJECT,
      "instance")) == NULL) {
    SU_ERROR("failed to acquire xsig source instance\n");
    goto fail;
  }

  if (!su_modem_register_block(modem, xsig_source_block)) {
    SU_ERROR("failed to register wav source\n");
    su_block_destroy(xsig_source_block);
    goto fail;
  }

  if (!su_modem_set_int(modem, "samp_rate", *samp_rate)) {
    SU_ERROR("failed to set modem sample rate\n");
    goto fail;
  }

  if (!su_modem_set_source(modem, xsig_source_block))
    goto fail;

  return SU_TRUE;

fail:
  if (xsig_source_block != NULL)
    su_block_destroy(xsig_source_block);

  return SU_FALSE;
}

su_modem_t *
xsig_modem_init(const char *file, struct xsig_source **instance)
{
  su_modem_t *modem = NULL;

  if ((modem = su_modem_new("qpsk")) == NULL) {
    fprintf(stderr, "xsig_modem_init: failed to initialize QPSK modem\n", file);
    return NULL;
  }

  if (!su_modem_set_xsig_source(modem, file, instance)) {
    fprintf(
        stderr,
        "xsig_modem_init: failed to set modem wav source to %s\n",
        file);
    su_modem_destroy(modem);
    return NULL;
  }

  su_modem_set_bool(modem, "abc", SU_FALSE);
  su_modem_set_bool(modem, "afc", SU_FALSE);

  su_modem_set_int(modem, "mf_span", 6);
  su_modem_set_float(modem, "baud", 468);
  su_modem_set_float(modem, "fc", 909);
  su_modem_set_float(modem, "rolloff", .35);

  if (!su_modem_start(modem)) {
    fprintf(stderr, "xsig_modem_init: failed to start modem\n");
    su_modem_destroy(modem);
    return NULL;
  }

  return modem;
}

SUPRIVATE void
xsigtool_redraw_channels(display_t *disp, const struct xsig_interface *iface)
{
  struct xsig_channel **channel_list;
  unsigned int channel_count;
  unsigned int i;
  unsigned int a, b;

  xsig_channel_detector_get_channel_list(
      iface->cd,
      &channel_list,
      &channel_count);

  fbox(
      disp,
      iface->wf->params.x,
      iface->wf->params.y + iface->wf->params.height + 2,
      iface->wf->params.x + iface->wf->params.width,
      iface->wf->params.y + iface->wf->params.height + 2 + 8 * 8,
      OPAQUE(0));

  for (i = 0; i < channel_count; ++i)
    if (channel_list[i] != NULL) {
      a = iface->wf->params.width
          * SU_ABS2NORM_FREQ(
              iface->cd->params.samp_rate,
              iface->cd->params.decimation
              * (channel_list[i]->fc - channel_list[i]->bw * .5));
      b = iface->wf->params.width
          * SU_ABS2NORM_FREQ(
              iface->cd->params.samp_rate,
              iface->cd->params.decimation
              * (channel_list[i]->fc + channel_list[i]->bw * .5));

      fbox(
          disp,
          iface->wf->params.x + a,
          iface->wf->params.y + 1,
          iface->wf->params.x + b,
          iface->wf->params.y + iface->wf->params.height - 1,
          0x7f00ff00);
      display_printf(
          disp,
          iface->wf->params.x,
          iface->wf->params.y + iface->wf->params.height + 3 + i * 8,
          OPAQUE(0x7f7f7f),
          OPAQUE(0),
          "Channel %d: %lg Hz (bw: %lg Hz)",
          i,
          channel_list[i]->fc,
          channel_list[i]->bw);
    }
}

int
main(int argc, char *argv[])
{
  su_modem_t *modem = NULL;
  struct xsig_constellation_params cons_params = xsig_constellation_params_INITIALIZER;
  struct xsig_waterfall_params wf_params;
  struct xsig_channel_detector_params cd_params =
      xsig_channel_detector_params_INITIALIZER;
  xsig_constellation_t *cons = NULL;
  textarea_t *area;
  display_t *disp;
  SUCOMPLEX sample = 0;
  unsigned int count = 0;
  struct xsig_source *instance;
  struct xsig_interface interface;
  SUFLOAT *fc;
  SUBOOL *abc;
  SUBOOL *afc;
  uint32_t colors[4] = {0xffffff7f, 0xff7f7fff, 0xff7fff7f, 0xffff7f7f};
  char sym;

  if (argc != 2) {
    fprintf(stderr, "Usage:\n\t%s file.wav\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if (!su_lib_init()) {
    fprintf(stderr, "%s: failed to initialize library\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if ((modem = xsig_modem_init(argv[1], &instance)) == NULL)
    exit(EXIT_FAILURE);

  if ((fc = su_modem_get_state_property_ref(
      modem,
      "fc",
      SU_PROPERTY_TYPE_FLOAT)) == NULL) {
    fprintf(stderr, "%s: failed to retrieve fc property\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if ((abc = su_modem_get_state_property_ref(
      modem,
      "abc",
      SU_PROPERTY_TYPE_BOOL)) == NULL) {
    fprintf(stderr, "%s: failed to retrieve abc property\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if ((afc = su_modem_get_state_property_ref(
      modem,
      "afc",
      SU_PROPERTY_TYPE_BOOL)) == NULL) {
    fprintf(stderr, "%s: failed to retrieve afc property\n", argv[0]);
    exit(EXIT_FAILURE);
  }


  if ((disp = display_new(SCREEN_WIDTH, SCREEN_HEIGHT)) == NULL) {
    fprintf(stderr, "%s: failed to initialize display\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  cons_params.scaling = .25;
  cons_params.history_size = 20;
  cons_params.width = 128;
  cons_params.height = 128;

  if ((cons = xsig_constellation_new(&cons_params)) == NULL) {
    fprintf(stderr, "%s: cannot create constellation\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  wf_params.fft_size = 256;
  wf_params.width = 512;
  wf_params.height = 128;
  wf_params.x = 3;
  wf_params.y = 133;

  if ((interface.wf = xsig_waterfall_new(&wf_params)) == NULL) {
    fprintf(stderr, "%s: cannot create waterfall\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  cd_params.samp_rate = instance->samp_rate;
  cd_params.alpha = 1e-2;

  if ((interface.cd = xsig_channel_detector_new(&cd_params)) == NULL) {
    fprintf(stderr, "%s: cannot create channel detector\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if ((area = display_textarea_new (disp, 133, 3, 48, 16, NULL, 850, 8))
      == NULL) {
    fprintf(stderr, "%s: failed to create textarea\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  area->autorefresh = 0;

  instance->params.private = &interface;

  while (!isnan(SU_C_ABS(sample = su_modem_read_sample(modem)))) {
    sym = ((SU_C_REAL(sample) > 0) << 1) | (SU_C_IMAG(sample) > 0);
    xsig_constellation_feed(cons, sample);
    if (++count % cons_params.history_size == 0) {
      xsig_constellation_redraw(cons, disp);
      xsig_waterfall_redraw(interface.wf, disp);
      xsigtool_redraw_channels(disp, &interface);
      display_printf(
          disp,
          2,
          disp->height - 9,
          OPAQUE(0xbfbfbf),
          OPAQUE(0),
          "Carrier: %8.3lf Hz", *fc);
      textarea_set_fore_color(area, colors[sym]);
      cprintf(area, "%c", sym + 'A');
      display_refresh(disp);
    }
    usleep(1000);
  }

  display_end(disp);

  su_modem_destroy(modem);

  return 0;
}
