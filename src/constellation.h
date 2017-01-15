#ifndef _CONSTELLATION_H
#define _CONSTELLATION_H

#include <sigutils/sigutils.h>
#include "xsigtool.h"

struct xsig_constellation_params {
  double scaling;

  unsigned int history_size;
  unsigned int x;
  unsigned int y;
  unsigned int width;
  unsigned int height;
};

#define xsig_constellation_params_INITIALIZER { 1, 20, 2, 2, 256, 256 }

typedef struct xsig_constellation {
  struct xsig_constellation_params params;
  SUCOMPLEX *history;
  unsigned int size;
  unsigned int p;
}
xsig_constellation_t;

void xsig_constellation_destroy(xsig_constellation_t *constellation);
xsig_constellation_t *xsig_constellation_new(const struct xsig_constellation_params *params);
void xsig_constellation_feed(xsig_constellation_t *constellation, SUCOMPLEX s);
void xsig_constellation_redraw(const xsig_constellation_t *constellation, display_t *disp);

#endif /* _CONSTELLATION_H */
