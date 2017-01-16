/*
 * xsigtool.h: headers, prototypes and declarations for xsigtool
 * Creation date: Wed Jan 11 20:35:22 2017
 */

#ifndef _MAIN_INCLUDE_H
#define _MAIN_INCLUDE_H

#include <config.h> /* General compile-time configuration parameters */
#include <util.h> /* From util: Common utility library */
#include <wbmp.h> /* From sim-static: Built-in simulation library over SDL */
#include <draw.h> /* From sim-static: Built-in simulation library over SDL */
#include <cpi.h> /* From sim-static: Built-in simulation library over SDL */
#include <ega9.h> /* From sim-static: Built-in simulation library over SDL */
#include <hook.h> /* From sim-static: Built-in simulation library over SDL */
#include <layout.h> /* From sim-static: Built-in simulation library over SDL */
#include <pearl-m68k.h> /* From sim-static: Built-in simulation library over SDL */
#include <pixel.h> /* From sim-static: Built-in simulation library over SDL */
#include <sndfile.h>
#include <fftw3.h>

#define XSIG_SOURCE_FFTW_PREFIX fftw
#define XSIG_SNDFILE_READ sf_read_double
#define XSIG_FFTW(method) JOIN(XSIG_SOURCE_FFTW_PREFIX, method)

#endif /* _MAIN_INCLUDE_H */
