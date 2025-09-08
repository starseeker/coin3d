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

/*
  Basic/no-op implementations for FreeType wrapper functions.
  These are stub implementations to resolve link-time issues.
*/

#include "coindefs.h"
#include "fonts/freetype.h"
#include "fonts/common.h"
#include <Inventor/C/base/string.h>
#include <cstdlib>

#ifdef __cplusplus
extern "C" {
#endif

SbBool
cc_flwft_initialize(void)
{
  /* Basic initialization - always return FALSE indicating FreeType is not available */
  return FALSE;
}

void
cc_flwft_exit(void)
{
  /* No-op cleanup */
}

void *
cc_flwft_get_font(const char * fontname, unsigned int pixelsize)
{
  /* Return NULL indicating no font could be loaded */
  return NULL;
}

void
cc_flwft_get_font_name(void * font, cc_string * str)
{
  /* Set empty string for font name */
  if (str) {
    cc_string_set_text(str, "");
  }
}

void
cc_flwft_done_font(void * font)
{
  /* No-op font cleanup since get_font returns NULL */
}

int
cc_flwft_get_num_charmaps(void * font)
{
  /* Return 0 indicating no character maps available */
  return 0;
}

const char *
cc_flwft_get_charmap_name(void * font, int charmap)
{
  /* Return NULL indicating no character map name available */
  return NULL;
}

void
cc_flwft_set_charmap(void * font, int charmap)
{
  /* No-op character map setting */
}

void
cc_flwft_set_char_size(void * font, int height)
{
  /* No-op character size setting */
}

void
cc_flwft_set_font_rotation(void * font, float angle)
{
  /* No-op font rotation setting */
}

int
cc_flwft_get_glyph(void * font, unsigned int charidx)
{
  /* Return -1 indicating no glyph available */
  return -1;
}

void
cc_flwft_get_vector_advance(void * font, int glyph, float * x, float * y)
{
  /* Set zero advance values */
  if (x) *x = 0.0f;
  if (y) *y = 0.0f;
}

void
cc_flwft_get_bitmap_kerning(void * font, int glyph1, int glyph2, int * x, int * y)
{
  /* Set zero kerning values */
  if (x) *x = 0;
  if (y) *y = 0;
}

void
cc_flwft_get_vector_kerning(void * font, int glyph1, int glyph2, float * x, float * y)
{
  /* Set zero kerning values */
  if (x) *x = 0.0f;
  if (y) *y = 0.0f;
}

void
cc_flwft_done_glyph(void * font, int glyph)
{
  /* No-op glyph cleanup */
}

struct cc_font_bitmap *
cc_flwft_get_bitmap(void * font, unsigned int glyph)
{
  /* Return NULL indicating no bitmap available */
  return NULL;
}

struct cc_font_vector_glyph *
cc_flwft_get_vector_glyph(void * font, unsigned int glyph, float complexity)
{
  /* Return NULL indicating no vector glyph available */
  return NULL;
}

#ifdef __cplusplus
}
#endif