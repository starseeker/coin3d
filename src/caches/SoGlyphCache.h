#ifndef COIN_SOGLYPHCACHE_H
#define COIN_SOGLYPHCACHE_H

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

#ifndef COIN_INTERNAL
#error this is a private header file
#endif /* !COIN_INTERNAL */

// *************************************************************************

#include <Inventor/caches/SoCache.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbVec4f.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbBox2f.h>
#include "../src/fonts/fontspec.h"

class SoGlyphCacheP;
class SoState;

// Modern glyph data structures using SbFont data
struct SbGlyph2D {
  unsigned char * bitmap;
  SbVec2s size;
  SbVec2s bearing;
  SbVec2f advance;
  SbVec2f kerning;  // for next character
  SbBox2f bounds;
  int character;
  int refcount;
  
  SbGlyph2D() : bitmap(nullptr), character(0), refcount(1) {}
  ~SbGlyph2D() { 
    // Note: bitmap is owned by SbFont, don't delete
  }
};

struct SbGlyph3D {
  const float * vertices;
  const int * face_indices;
  const int * edge_indices;
  const int * edge_connectivity;
  int num_vertices;
  int num_face_indices;
  int num_edge_indices;
  int num_edges;
  SbVec2f advance;
  SbBox2f bounds;
  float width;
  int character;
  int refcount;
  
  SbGlyph3D() : vertices(nullptr), face_indices(nullptr), edge_indices(nullptr),
                edge_connectivity(nullptr), num_vertices(0), num_face_indices(0),
                num_edge_indices(0), num_edges(0), width(0.0f), character(0), refcount(1) {}
  ~SbGlyph3D() {
    // Note: data is owned by SbFont, don't delete
  }
};

class SoGlyphCacheP;
class SoState;

// *************************************************************************

class SoGlyphCache : public SoCache {
  typedef SoCache inherited;

public:
  SoGlyphCache(SoState * state);
  virtual ~SoGlyphCache();

  void readFontspec(SoState * state);
  const cc_font_specification * getCachedFontspec(void) const;
  
  // Modern glyph caching using SbFont data
  void addGlyph(SbGlyph2D * glyph);
  void addGlyph(SbGlyph3D * glyph);
  
  // Get cached glyphs for performance
  SbGlyph2D * getGlyph2D(int character, class SbFont * font);
  SbGlyph3D * getGlyph3D(int character, class SbFont * font);

private:
  friend class SoGlyphCacheP;
  SoGlyphCacheP * pimpl;
};

// *************************************************************************

#endif // !COIN_SOGLYPHCACHE_H
