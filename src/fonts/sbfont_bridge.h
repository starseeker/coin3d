#ifndef COIN_SBFONT_BRIDGE_H
#define COIN_SBFONT_BRIDGE_H

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
  Bridge interface between new SbFont API and existing text nodes.
  This provides a simple C-style interface that wraps SbFont functionality
  for backward compatibility with existing text rendering code.
*/

#ifndef COIN_INTERNAL
#error this is a private header file
#endif /* ! COIN_INTERNAL */

#include "Inventor/basic.h"
#include "fontspec.h"
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Simplified glyph structures that replace cc_glyph2d and cc_glyph3d
typedef struct {
  unsigned char * bitmap;
  int width, height;
  int bearing_x, bearing_y;
  int advance_x, advance_y;
  uint32_t character;
  int refcount;
} sb_glyph2d;

typedef struct {
  const float * vertices;
  const int * face_indices;
  const int * edge_indices;
  int num_vertices;
  int num_face_indices;
  int num_edge_indices;
  float advance_x, advance_y;
  float width;
  float bbox[4]; // xmin, ymin, xmax, ymax
  uint32_t character;
  int refcount;
} sb_glyph3d;

// 2D glyph interface (for SoText2)
sb_glyph2d * sb_glyph2d_ref(uint32_t character, const cc_font_specification * spec, float angle);
void sb_glyph2d_unref(sb_glyph2d * glyph);
void sb_glyph2d_getadvance(const sb_glyph2d * g, int * x, int * y);
void sb_glyph2d_getkerning(const sb_glyph2d * left, const sb_glyph2d * right, int * x, int * y);
unsigned int sb_glyph2d_getwidth(const sb_glyph2d * g);
const unsigned char * sb_glyph2d_getbitmap(const sb_glyph2d * g, int * size, int * offset);
SbBool sb_glyph2d_getmono(const sb_glyph2d * g);

// 3D glyph interface (for SoText3)  
sb_glyph3d * sb_glyph3d_ref(uint32_t character, const cc_font_specification * spec);
void sb_glyph3d_unref(sb_glyph3d * glyph);
const float * sb_glyph3d_getcoords(const sb_glyph3d * g);
const int * sb_glyph3d_getfaceindices(const sb_glyph3d * g);
const int * sb_glyph3d_getedgeindices(const sb_glyph3d * g);
float sb_glyph3d_getwidth(const sb_glyph3d * g);
const float * sb_glyph3d_getboundingbox(const sb_glyph3d * g);
void sb_glyph3d_getadvance(const sb_glyph3d * g, float * x, float * y);
void sb_glyph3d_getkerning(const sb_glyph3d * left, const sb_glyph3d * right, float * x, float * y);

#ifdef __cplusplus
}
#endif

#endif /* !COIN_SBFONT_BRIDGE_H */