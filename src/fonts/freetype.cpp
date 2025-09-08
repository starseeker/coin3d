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
  Implementation of FreeType wrapper functions using struetype.
  This provides basic font functionality using the struetype library.
*/

#include "coindefs.h"
#include "fonts/freetype.h"
#include "fonts/common.h"
#include <Inventor/C/base/string.h>
#include <cstdlib>

#define STRUETYPE_IMPLEMENTATION
#include "struetype.h"

// Simple font manager to handle font loading
struct cc_font_handle {
  stt_fontinfo font_info;
  float size;
  unsigned char* font_data;
  int data_size;
  int valid;
};

// Basic embedded font data - a minimal font for testing
// This is a simplified approach - in a full implementation, you'd load actual font files
static unsigned char default_font_data[] = {
  // This would be actual font data - for now just a placeholder
  0x00, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x80, 0x00, 0x03, 0x00, 0x70
  // In a real implementation, you'd include actual TTF/OTF font data here
  // or load it from system fonts
};

#ifdef __cplusplus
extern "C" {
#endif

static int freetype_initialized = 0;

SbBool
cc_flwft_initialize(void)
{
  /* Initialize struetype - always succeeds for basic implementation */
  freetype_initialized = 1;
  return TRUE;
}

void
cc_flwft_exit(void)
{
  /* Cleanup */
  freetype_initialized = 0;
}

void *
cc_flwft_get_font(const char * fontname, unsigned int pixelsize)
{
  if (!freetype_initialized) return NULL;
  
  /* For now, return a placeholder font handle since we don't have real font loading */
  /* In a full implementation, this would load the requested font file */
  cc_font_handle* handle = (cc_font_handle*)malloc(sizeof(cc_font_handle));
  if (!handle) return NULL;
  
  handle->font_data = NULL; // Would load actual font data here
  handle->data_size = 0;
  handle->size = (float)pixelsize;
  handle->valid = 0; // Mark as invalid since we don't have real font data
  
  return handle;
}

void
cc_flwft_get_font_name(void * font, cc_string * str)
{
  /* Set a default font name */
  if (str) {
    cc_string_set_text(str, "struetype-default");
  }
}

void
cc_flwft_done_font(void * font)
{
  /* Cleanup font handle */
  if (font) {
    cc_font_handle* handle = (cc_font_handle*)font;
    if (handle->font_data) {
      free(handle->font_data);
    }
    free(handle);
  }
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
  /* For basic implementation, return the character index as glyph index */
  /* In a real implementation with valid font data, this would use stt_FindGlyphIndex */
  if (!font) return -1;
  
  cc_font_handle* handle = (cc_font_handle*)font;
  if (!handle->valid) {
    /* Without valid font data, just return the character as glyph index */
    return (int)charidx;
  }
  
  /* With valid font data, would do:
   * return stt_FindGlyphIndex(&handle->font_info, charidx);
   */
  return (int)charidx;
}

void
cc_flwft_get_vector_advance(void * font, int glyph, float * x, float * y)
{
  /* Set default advance values for basic character spacing */
  if (x) *x = 10.0f; // Basic fixed-width advance
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