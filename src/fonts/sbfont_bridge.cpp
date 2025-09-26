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
  Bridge interface implementation between new SbFont API and existing text nodes.
  This provides a simple C-style interface that wraps SbFont functionality.
*/

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include "coindefs.h"
#include "sbfont_bridge.h"
#include <Inventor/SbFont.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbBox2f.h>

// Global font instance for bridge operations
// This is a simplified approach - in production code you might want per-thread fonts
static SbFont * g_bridge_font = NULL;

static SbFont *
get_bridge_font()
{
  if (!g_bridge_font) {
    g_bridge_font = new SbFont();
  }
  return g_bridge_font;
}

// Clean up function (called during library shutdown)
void sb_font_bridge_cleanup()
{
  if (g_bridge_font) {
    delete g_bridge_font;
    g_bridge_font = NULL;
  }
}

// 2D glyph interface implementation
sb_glyph2d *
sb_glyph2d_ref(uint32_t character, const cc_font_specification * spec, float angle)
{
  SbFont * font = get_bridge_font();
  
  // Update font size from specification
  if (spec && spec->size > 0) {
    font->setSize(spec->size);
  }
  
  sb_glyph2d * glyph = (sb_glyph2d*)malloc(sizeof(sb_glyph2d));
  if (!glyph) return NULL;
  
  // Initialize structure
  glyph->character = character;
  glyph->refcount = 1;
  glyph->bitmap = NULL;
  
  // Get bitmap from SbFont
  SbVec2s size, bearing;
  glyph->bitmap = font->getGlyphBitmap(character, size, bearing);
  
  if (glyph->bitmap) {
    glyph->width = size[0];
    glyph->height = size[1];
    glyph->bearing_x = bearing[0];
    glyph->bearing_y = bearing[1];
    
    // Get advance
    SbVec2f advance = font->getGlyphAdvance(character);
    glyph->advance_x = (int)advance[0];
    glyph->advance_y = (int)advance[1];
  } else {
    // No bitmap available - set defaults
    glyph->width = 0;
    glyph->height = 0;
    glyph->bearing_x = 0;
    glyph->bearing_y = 0;
    glyph->advance_x = (int)(font->getSize() * 0.6); // Rough estimate for missing chars
    glyph->advance_y = 0;
  }
  
  return glyph;
}

void 
sb_glyph2d_unref(sb_glyph2d * glyph)
{
  if (!glyph) return;
  
  glyph->refcount--;
  if (glyph->refcount <= 0) {
    // Note: we don't free glyph->bitmap because SbFont owns it
    free(glyph);
  }
}

void 
sb_glyph2d_getadvance(const sb_glyph2d * g, int * x, int * y)
{
  if (g && x && y) {
    *x = g->advance_x;
    *y = g->advance_y;
  }
}

void 
sb_glyph2d_getkerning(const sb_glyph2d * left, const sb_glyph2d * right, int * x, int * y)
{
  if (left && right && x && y) {
    SbFont * font = get_bridge_font();
    SbVec2f kern = font->getGlyphKerning(left->character, right->character);
    *x = (int)kern[0];
    *y = (int)kern[1];
  }
}

unsigned int 
sb_glyph2d_getwidth(const sb_glyph2d * g)
{
  return g ? g->width : 0;
}

const unsigned char * 
sb_glyph2d_getbitmap(const sb_glyph2d * g, int * size, int * offset)
{
  if (!g) return NULL;
  
  if (size) {
    size[0] = g->width;
    size[1] = g->height;
  }
  
  if (offset) {
    offset[0] = g->bearing_x;
    offset[1] = g->bearing_y;
  }
  
  return g->bitmap;
}

SbBool 
sb_glyph2d_getmono(const sb_glyph2d * g)
{
  // SbFont uses grayscale rendering, so return FALSE
  return FALSE;
}

// 3D glyph interface implementation
sb_glyph3d * 
sb_glyph3d_ref(uint32_t character, const cc_font_specification * spec)
{
  SbFont * font = get_bridge_font();
  
  // Update font size from specification
  if (spec && spec->size > 0) {
    font->setSize(spec->size);
  }
  
  sb_glyph3d * glyph = (sb_glyph3d*)malloc(sizeof(sb_glyph3d));
  if (!glyph) return NULL;
  
  // Initialize structure
  glyph->character = character;
  glyph->refcount = 1;
  
  // Get vertex data from SbFont (currently not implemented in SbFont)
  // For now, provide simple fallback geometry for 3D text
  int numvertices = 0;
  glyph->vertices = font->getGlyphVertices(character, numvertices);
  glyph->num_vertices = numvertices;
  
  int numfaceindices = 0;
  glyph->face_indices = font->getGlyphFaceIndices(character, numfaceindices);
  glyph->num_face_indices = numfaceindices;
  
  int numedgeindices = 0;
  glyph->edge_indices = font->getGlyphEdgeIndices(character, numedgeindices);
  glyph->num_edge_indices = numedgeindices;
  
  // If no 3D data available, create minimal fallback
  if (!glyph->vertices || glyph->num_vertices == 0) {
    // Create a simple rectangular glyph as fallback
    // This prevents crashes but text won't have proper 3D appearance
    static float fallback_vertices[] = {
      0.0f, 0.0f, 0.0f,    // bottom-left
      1.0f, 0.0f, 0.0f,    // bottom-right
      1.0f, 1.0f, 0.0f,    // top-right
      0.0f, 1.0f, 0.0f     // top-left
    };
    static int fallback_faces[] = {
      0, 1, 2, -1,         // triangle 1
      0, 2, 3, -1          // triangle 2  
    };
    static int fallback_edges[] = {
      0, 1, -1,            // bottom edge
      1, 2, -1,            // right edge
      2, 3, -1,            // top edge
      3, 0, -1             // left edge
    };
    
    glyph->vertices = fallback_vertices;
    glyph->num_vertices = 4;
    glyph->face_indices = fallback_faces;
    glyph->num_face_indices = 8;
    glyph->edge_indices = fallback_edges;
    glyph->num_edge_indices = 12;
  }
  
  // Get advance and bounds
  SbVec2f advance = font->getGlyphAdvance(character);
  glyph->advance_x = advance[0];
  glyph->advance_y = advance[1];
  
  SbBox2f bounds = font->getGlyphBounds(character);
  if (!bounds.isEmpty()) {
    glyph->bbox[0] = bounds.getMin()[0]; // xmin
    glyph->bbox[1] = bounds.getMin()[1]; // ymin
    glyph->bbox[2] = bounds.getMax()[0]; // xmax
    glyph->bbox[3] = bounds.getMax()[1]; // ymax
    glyph->width = bounds.getMax()[0] - bounds.getMin()[0];
  } else {
    // Default bounds
    glyph->bbox[0] = 0;
    glyph->bbox[1] = 0;
    glyph->bbox[2] = font->getSize() * 0.6f;
    glyph->bbox[3] = font->getSize();
    glyph->width = font->getSize() * 0.6f;
  }
  
  return glyph;
}

void 
sb_glyph3d_unref(sb_glyph3d * glyph)
{
  if (!glyph) return;
  
  glyph->refcount--;
  if (glyph->refcount <= 0) {
    // Note: we don't free vertex data because SbFont owns it
    free(glyph);
  }
}

const float * 
sb_glyph3d_getcoords(const sb_glyph3d * g)
{
  return g ? g->vertices : NULL;
}

const int * 
sb_glyph3d_getfaceindices(const sb_glyph3d * g)
{
  return g ? g->face_indices : NULL;
}

const int * 
sb_glyph3d_getedgeindices(const sb_glyph3d * g)
{
  return g ? g->edge_indices : NULL;
}

const int * 
sb_glyph3d_getnextccwedge(const sb_glyph3d * g, int edgeidx)
{
  // This function is used for edge traversal in 3D glyphs
  // Since we're using a simplified approach, return NULL
  // This will cause the text to render without edge information
  return NULL;
}

const int * 
sb_glyph3d_getnextcwedge(const sb_glyph3d * g, int edgeidx)
{
  // This function is used for edge traversal in 3D glyphs
  // Since we're using a simplified approach, return NULL
  // This will cause the text to render without edge information
  return NULL;
}

float 
sb_glyph3d_getwidth(const sb_glyph3d * g)
{
  return g ? g->width : 0.0f;
}

const float * 
sb_glyph3d_getboundingbox(const sb_glyph3d * g)
{
  return g ? g->bbox : NULL;
}

void 
sb_glyph3d_getadvance(const sb_glyph3d * g, float * x, float * y)
{
  if (g && x && y) {
    *x = g->advance_x;
    *y = g->advance_y;
  }
}

void 
sb_glyph3d_getkerning(const sb_glyph3d * left, const sb_glyph3d * right, float * x, float * y)
{
  if (left && right && x && y) {
    SbFont * font = get_bridge_font();
    SbVec2f kern = font->getGlyphKerning(left->character, right->character);
    *x = kern[0];
    *y = kern[1];
  }
}