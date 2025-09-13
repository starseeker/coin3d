/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

#ifndef COIN_TIDBITSP_H
#define COIN_TIDBITSP_H

#ifndef COIN_INTERNAL
#error this is a private header file
#endif

#include <stdio.h>
#include "CoinTidbits.h"
#include "C/base/string.h"

// These are already defined in CoinTidbits.h

/* Forward declaration to avoid header conflicts */
/* typedef struct cc_string cc_string; */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if 0 /* to get proper auto-indentation in emacs */
}
#endif /* emacs indentation */

/* ********************************************************************** */

void coin_init_tidbits(void);

/* ********************************************************************** */

FILE * coin_get_stdin(void);
FILE * coin_get_stdout(void);
FILE * coin_get_stderr(void);

/* ********************************************************************** */

#define coin_atexit(func, priority) \
        coin_atexit_func(SO__QUOTE(func), func, priority)

void coin_atexit_cleanup(void);

SbBool coin_is_exiting(void);

void coin_atexit_func(const char * name, coin_atexit_f * fp, int priority);

/* ********************************************************************** */

/*
  We're using these to ensure portable import and export even when the
  application sets a locale with different thousands separator and
  decimal point than the default "C" locale.

  Use these functions to wrap locale-aware functions where
  necessary:

  \code
  cc_string storedlocale;
  SbBool changed = coin_locale_set_portable(&storedlocale);

  // [code with locale-aware functions]

  if (changed) { coin_locale_reset(&storedlocale); }
  \endcode

  Possibly locale-aware functions includes at least atof(), atoi(),
  atol(), strtol(), strtoul(), strtod(), strtof(), strtold(), and all
  the *printf() functions.
*/

SbBool coin_locale_set_portable(cc_string * storeold);
void coin_locale_reset(cc_string * storedold);

/*
  Portable atof() function, which will not cause any trouble due to
  underlying locale's decimal point setting.
*/
double coin_atof(const char * ptr);

/* ********************************************************************** */

/*
  Functions to output ascii85 encoded data. Used for instance for PostScript
  image rendering.
*/
void coin_output_ascii85(FILE * fp,
                         const unsigned char val,
                         unsigned char * tuple,
                         unsigned char * linebuf,
                         int * tuplecnt, int * linecnt,
                         const int rowlen,
                         const SbBool flush);

void coin_flush_ascii85(FILE * fp,
                        unsigned char * tuple,
                        unsigned char * linebuf,
                        int * tuplecnt, int * linecnt,
                        const int rowlen);

/* ********************************************************************** */

/*
  Parse version string of type <major>.<minor>.<patch>. <minor> or
  <patch> might not be in the string. It's possible to supply NULL for
  minor and/or patch if you're not interested in minor and/or patch.
*/
SbBool coin_parse_versionstring(const char * versionstr,
                                int * major,
                                int * minor,
                                int * patch);

/* ********************************************************************** */

SbBool coin_getcwd(cc_string * str);

/* ********************************************************************** */

int coin_isinf(double value);
int coin_isnan(double value);
int coin_finite(double value);

/* ********************************************************************** */

unsigned long coin_geq_prime_number(unsigned long num);

/* ********************************************************************** */

int coin_runtime_os(void);

#define COIN_MAC_FRAMEWORK_IDENTIFIER_CSTRING ("org.coin3d.Coin.framework")

/* ********************************************************************** */

int coin_debug_extra(void);
int coin_debug_normalize(void);
int coin_debug_caching_level(void);

/* ********************************************************************** */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

/* ********************************************************************** */

#endif /* !COIN_TIDBITS_H */
