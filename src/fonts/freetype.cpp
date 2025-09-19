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
#include "C/base/string.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

#define STRUETYPE_IMPLEMENTATION
#include "struetype.h"

// Simple font manager to handle font loading
struct cc_font_handle {
  stt_fontinfo font_info;
  float size;
  unsigned char* font_data;
  int data_size;
  int valid;
  float scale; /* Scale factor for font */
};

// Try to load a system font file
static unsigned char* load_system_font(const char* fontname, int* data_size) {
  const char* font_paths[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    "/System/Library/Fonts/Arial.ttf", /* macOS */
    "C:\\Windows\\Fonts\\arial.ttf", /* Windows */
    NULL
  };
  
  for (int i = 0; font_paths[i]; i++) {
    FILE* f = fopen(font_paths[i], "rb");
    if (f) {
      fseek(f, 0, SEEK_END);
      long size = ftell(f);
      fseek(f, 0, SEEK_SET);
      
      if (size > 0 && size < 10*1024*1024) { /* Reasonable font size limit */
        unsigned char* data = (unsigned char*)malloc(size);
        if (data && fread(data, 1, size, f) == size) {
          fclose(f);
          *data_size = (int)size;
          return data;
        }
        if (data) free(data);
      }
      fclose(f);
    }
  }
  
  *data_size = 0;
  return NULL;
}

// Basic embedded font data - minimal fallback font
// This is a placeholder - in practice you'd include minimal font data
static unsigned char default_font_data[] = {
  // Minimal TrueType font header - this is just a placeholder
  // Real implementation would include actual minimal font data
  0x00, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x80, 0x00, 0x03, 0x00, 0x70,
  0x68, 0x65, 0x61, 0x64, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x36
  // In a real implementation, you'd include actual TTF/OTF font data here
  // or use an embedded minimal font like the one from struetype examples
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
  
  cc_font_handle* handle = (cc_font_handle*)malloc(sizeof(cc_font_handle));
  if (!handle) return NULL;
  
  /* Initialize handle */
  handle->font_data = NULL;
  handle->data_size = 0;
  handle->size = (float)pixelsize;
  handle->valid = 0;
  handle->scale = 1.0f;
  
  /* Try to load system font first */
  handle->font_data = load_system_font(fontname, &handle->data_size);
  
  if (handle->font_data && handle->data_size > 0) {
    /* Try to initialize with loaded font */
    int result = stt_InitFont(&handle->font_info, handle->font_data, handle->data_size, 0);
    if (result) {
      handle->valid = 1;
      handle->scale = stt_ScaleForPixelHeight(&handle->font_info, handle->size);
    } else {
      /* Font loading failed, free the data */
      free(handle->font_data);
      handle->font_data = NULL;
      handle->data_size = 0;
    }
  }
  
  if (!handle->valid) {
    /* Fallback to default font data (though it's minimal) */
    handle->font_data = (unsigned char*)malloc(sizeof(default_font_data));
    if (handle->font_data) {
      memcpy(handle->font_data, default_font_data, sizeof(default_font_data));
      handle->data_size = sizeof(default_font_data);
      /* Note: This will likely fail with minimal data, but provides graceful fallback */
      int result = stt_InitFont(&handle->font_info, handle->font_data, handle->data_size, 0);
      if (result) {
        handle->valid = 1;
        handle->scale = stt_ScaleForPixelHeight(&handle->font_info, handle->size);
      }
    }
  }
  
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
  if (!font) return;
  
  cc_font_handle* handle = (cc_font_handle*)font;
  handle->size = (float)height;
  
  if (handle->valid) {
    /* Recalculate scale factor */
    handle->scale = stt_ScaleForPixelHeight(&handle->font_info, handle->size);
  }
}

void
cc_flwft_set_font_rotation(void * font, float angle)
{
  /* No-op font rotation setting */
}

int
cc_flwft_get_glyph(void * font, unsigned int charidx)
{
  if (!font) return -1;
  
  cc_font_handle* handle = (cc_font_handle*)font;
  if (!handle->valid) {
    /* Without valid font data, just return the character as glyph index */
    return (int)charidx;
  }
  
  /* With valid font data, find the actual glyph index */
  int glyph_index = stt_FindGlyphIndex(&handle->font_info, charidx);
  return glyph_index > 0 ? glyph_index : (int)charidx;
}

void
cc_flwft_get_vector_advance(void * font, int glyph, float * x, float * y)
{
  if (!x && !y) return;
  
  if (!font) {
    if (x) *x = 10.0f; /* Default advance */
    if (y) *y = 0.0f;
    return;
  }
  
  cc_font_handle* handle = (cc_font_handle*)font;
  if (!handle->valid) {
    /* Set default advance values for basic character spacing */
    if (x) *x = handle->size * 0.6f; /* Reasonable default based on font size */
    if (y) *y = 0.0f;
    return;
  }
  
  /* With valid font data, get actual metrics */
  int advanceWidth, leftSideBearing;
  stt_GetGlyphHMetrics(&handle->font_info, glyph, &advanceWidth, &leftSideBearing);
  
  if (x) *x = advanceWidth * handle->scale;
  if (y) *y = 0.0f; /* Horizontal text only */
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
  if (!font) return NULL;
  
  cc_font_handle* handle = (cc_font_handle*)font;
  if (!handle->valid) {
    return NULL; /* No bitmap available without valid font */
  }
  
  /* Get glyph metrics */
  int x0, y0, x1, y1;
  stt_GetGlyphBitmapBox(&handle->font_info, glyph, handle->scale, handle->scale, &x0, &y0, &x1, &y1);
  
  int width = x1 - x0;
  int height = y1 - y0;
  
  if (width <= 0 || height <= 0) {
    return NULL; /* No bitmap for this glyph */
  }
  
  /* Allocate bitmap structure */
  struct cc_font_bitmap* bitmap = (struct cc_font_bitmap*)malloc(sizeof(struct cc_font_bitmap));
  if (!bitmap) return NULL;
  
  /* Allocate bitmap buffer */
  bitmap->width = width;
  bitmap->rows = height;
  bitmap->pitch = width; /* 1 byte per pixel for grayscale */
  bitmap->buffer = (unsigned char*)malloc(width * height);
  
  if (!bitmap->buffer) {
    free(bitmap);
    return NULL;
  }
  
  /* Render the glyph */
  stt_MakeGlyphBitmap(&handle->font_info, bitmap->buffer, width, height, bitmap->pitch, 
                      handle->scale, handle->scale, glyph);
  
  /* Set metrics */
  bitmap->bearingX = x0;
  bitmap->bearingY = y0;
  
  /* Get advance */
  int advanceWidth, leftSideBearing;
  stt_GetGlyphHMetrics(&handle->font_info, glyph, &advanceWidth, &leftSideBearing);
  bitmap->advanceX = (int)(advanceWidth * handle->scale);
  bitmap->advanceY = 0;
  bitmap->mono = 0; /* Antialiased bitmap */
  
  return bitmap;
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