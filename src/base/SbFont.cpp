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

/*!
  \class SbFont SbFont.h Inventor/SbFont.h
  \brief Clean, idiomatic C++ API for font management using struetype + ProFont.

  This class provides a simple interface for loading and using fonts in Coin3D.
  It uses struetype (TrueType font library) with the embedded ProFont as fallback.
  
  The design prioritizes simplicity - just load a font file or use the default,
  then get glyph information for text rendering. System font detection and
  multiple format support have been removed for clarity.
*/

#include <Inventor/SbFont.h>
#include "coindefs.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

#include <Inventor/errors/SoDebugError.h>
#include "fonts/profont_data.h"

// Include struetype header (implementation is in freetype.cpp)
#include "fonts/struetype.h"

// Include stt_glyph_mesh for 3D vector glyph generation
#include "fonts/stt_glyph_mesh.hpp"

// Internal implementation class
class SbFontP {
public:
  SbFontP();
  ~SbFontP();

  SbBool loadFontFromFile(const char * filepath);
  void loadDefaultFont();
  void cleanup();

  // Font data
  unsigned char * fontdata;
  int fontsize;
  stt_fontinfo fontinfo;
  SbBool valid;
  
  // Font properties
  SbString fontname;
  float size;
  float scale;
  
  // Cached glyph data (simple cache for performance)
  struct GlyphCache {
    int character;
    SbBool valid;
    unsigned char * bitmap;
    SbVec2s bitmapsize;
    SbVec2s bearing;
    SbVec2f advance;
    SbBox2f bounds;
    float * vertices;
    int numvertices;
    int * faceindices;
    int numfaceindices;
    int * edgeindices;
    int numedgeindices;
  };
  
  static const int CACHE_SIZE = 128;
  GlyphCache cache[CACHE_SIZE];
  int cacheindex;
  
  GlyphCache * findOrCreateGlyph(int character);
  void clearCache();
};

SbFontP::SbFontP()
  : fontdata(NULL), fontsize(0), valid(FALSE), 
    fontname(""), size(12.0f), scale(1.0f), cacheindex(0)
{
  memset(&fontinfo, 0, sizeof(fontinfo));
  clearCache();
}

SbFontP::~SbFontP()
{
  cleanup();
}

void 
SbFontP::cleanup()
{
  if (fontdata) {
    free(fontdata);
    fontdata = NULL;
  }
  fontsize = 0;
  valid = FALSE;
  clearCache();
}

void
SbFontP::clearCache()
{
  for (int i = 0; i < CACHE_SIZE; i++) {
    if (cache[i].bitmap) {
      free(cache[i].bitmap);
    }
    if (cache[i].vertices) {
      free(cache[i].vertices);
    }
    if (cache[i].faceindices) {
      free(cache[i].faceindices);
    }
    if (cache[i].edgeindices) {
      free(cache[i].edgeindices);
    }
    // Zero-initialize the struct properly
    cache[i] = GlyphCache();
  }
  cacheindex = 0;
}

SbBool
SbFontP::loadFontFromFile(const char * filepath)
{
  cleanup();
  
  if (!filepath) return FALSE;
  
  FILE * file = fopen(filepath, "rb");
  if (!file) {
    SoDebugError::postWarning("SbFont::loadFontFromFile", 
                              "Could not open font file: %s", filepath);
    return FALSE;
  }
  
  // Get file size
  fseek(file, 0, SEEK_END);
  fontsize = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  if (fontsize <= 0) {
    fclose(file);
    return FALSE;
  }
  
  // Allocate and read font data
  fontdata = (unsigned char*)malloc(fontsize);
  if (!fontdata) {
    fclose(file);
    return FALSE;
  }
  
  size_t bytesread = fread(fontdata, 1, fontsize, file);
  fclose(file);
  
  if ((int)bytesread != fontsize) {
    cleanup();
    return FALSE;
  }
  
  // Initialize font
  int result = stt_InitFont(&fontinfo, fontdata, fontsize, 0);
  if (!result) {
    SoDebugError::postWarning("SbFont::loadFontFromFile", 
                              "Failed to initialize font from: %s", filepath);
    cleanup();
    return FALSE;
  }
  
  valid = TRUE;
  fontname = filepath;
  scale = stt_ScaleForPixelHeight(&fontinfo, size);
  
  return TRUE;
}

void
SbFontP::loadDefaultFont()
{
  cleanup();
  
  // Copy embedded ProFont data
  fontsize = profont_ttf_data_size;
  fontdata = (unsigned char*)malloc(fontsize);
  if (!fontdata) {
    SoDebugError::postWarning("SbFont::loadDefaultFont", 
                              "Failed to allocate memory for default font");
    return;
  }
  
  memcpy(fontdata, profont_ttf_data, fontsize);
  
  // Initialize font
  int result = stt_InitFont(&fontinfo, fontdata, fontsize, 0);
  if (!result) {
    SoDebugError::postWarning("SbFont::loadDefaultFont", 
                              "Failed to initialize default font");
    cleanup();
    return;
  }
  
  valid = TRUE;
  fontname = "ProFont (embedded)";
  scale = stt_ScaleForPixelHeight(&fontinfo, size);
}

SbFontP::GlyphCache *
SbFontP::findOrCreateGlyph(int character)
{
  if (!valid) return NULL;
  
  // Simple linear search in cache (adequate for small cache)
  for (int i = 0; i < CACHE_SIZE; i++) {
    if (cache[i].valid && cache[i].character == character) {
      return &cache[i];
    }
  }
  
  // Cache miss - create new glyph
  GlyphCache * entry = &cache[cacheindex];
  cacheindex = (cacheindex + 1) % CACHE_SIZE;
  
  // Clear previous entry
  if (entry->bitmap) free(entry->bitmap);
  if (entry->vertices) free(entry->vertices);
  if (entry->faceindices) free(entry->faceindices);
  if (entry->edgeindices) free(entry->edgeindices);
  *entry = GlyphCache(); // Zero-initialize properly
  
  entry->character = character;
  entry->valid = TRUE;
  
  // Get glyph metrics
  int advanceWidth, leftSideBearing;
  stt_GetCodepointHMetrics(&fontinfo, character, &advanceWidth, &leftSideBearing);
  entry->advance.setValue(advanceWidth * scale, 0.0f);
  
  // Get glyph bounding box
  int x0, y0, x1, y1;
  stt_GetCodepointBitmapBox(&fontinfo, character, scale, scale, &x0, &y0, &x1, &y1);
  entry->bounds.setBounds(SbVec2f(x0, y0), SbVec2f(x1, y1));
  
  // Generate 3D mesh data using stt_glyph_mesh
  sttmesh::GlyphBuildConfig cfg;
  cfg.scale = scale;
  cfg.epsilon = 0.5f;    // curve flattening tolerance
  cfg.flipY = false;     // keep Y-up coordinate system for Coin3D
  
  try {
    sttmesh::GlyphMesh mesh = sttmesh::build_codepoint_mesh(fontinfo, character, cfg);
    
    if (!mesh.positions.empty() && !mesh.indices.empty()) {
      // Copy vertex positions (convert Vec2 to 3D coordinates with z=0)
      entry->numvertices = (int)mesh.positions.size();
      entry->vertices = (float*)malloc(entry->numvertices * 3 * sizeof(float));
      if (entry->vertices) {
        for (int i = 0; i < entry->numvertices; i++) {
          entry->vertices[i * 3 + 0] = mesh.positions[i].x;
          entry->vertices[i * 3 + 1] = mesh.positions[i].y;
          entry->vertices[i * 3 + 2] = 0.0f;  // Z coordinate is 0 for flat glyphs
        }
      }
      
      // Copy face indices (convert triangles to Coin3D format with -1 terminators)
      int numTriangles = (int)mesh.indices.size() / 3;
      entry->numfaceindices = numTriangles * 4; // 3 indices + 1 terminator per triangle
      entry->faceindices = (int*)malloc(entry->numfaceindices * sizeof(int));
      if (entry->faceindices) {
        for (int i = 0; i < numTriangles; i++) {
          entry->faceindices[i * 4 + 0] = (int)mesh.indices[i * 3 + 0];
          entry->faceindices[i * 4 + 1] = (int)mesh.indices[i * 3 + 1];
          entry->faceindices[i * 4 + 2] = (int)mesh.indices[i * 3 + 2];
          entry->faceindices[i * 4 + 3] = -1;  // Coin3D triangle terminator
        }
      }
      
      // Generate edge indices from outline contours for wireframe rendering
      if (!mesh.outlineContours.empty()) {
        // Calculate total number of edges needed
        int totalEdges = 0;
        for (const auto& contour : mesh.outlineContours) {
          totalEdges += contour.count; // Each contour creates count edge segments  
        }
        
        entry->numedgeindices = totalEdges * 3; // 2 indices + 1 terminator per edge
        entry->edgeindices = (int*)malloc(entry->numedgeindices * sizeof(int));
        if (entry->edgeindices) {
          int edgeIdx = 0;
          for (const auto& contour : mesh.outlineContours) {
            for (int i = 0; i < contour.count; i++) {
              int current = contour.start + i;
              int next = contour.start + ((i + 1) % contour.count);
              entry->edgeindices[edgeIdx++] = current;
              entry->edgeindices[edgeIdx++] = next;
              entry->edgeindices[edgeIdx++] = -1; // Coin3D edge terminator
            }
          }
        }
      }
      
      // Update bounds from mesh bbox if available
      if (mesh.bbox.valid) {
        entry->bounds.setBounds(SbVec2f(mesh.bbox.x0, mesh.bbox.y0), 
                               SbVec2f(mesh.bbox.x1, mesh.bbox.y1));
      }
    }
  } catch (...) {
    // If mesh generation fails, ensure we have clean state
    // (vertices, faceindices, edgeindices remain NULL as initialized)
  }
  
  return entry;
}

// SbFont implementation

SbFont::SbFont(void)
{
  pimpl = new SbFontP;
  pimpl->loadDefaultFont();
}

SbFont::SbFont(const char * fontpath)
{
  pimpl = new SbFontP;
  if (!pimpl->loadFontFromFile(fontpath)) {
    pimpl->loadDefaultFont();
  }
}

SbFont::SbFont(const SbString & fontpath)
{
  pimpl = new SbFontP;
  if (!pimpl->loadFontFromFile(fontpath.getString())) {
    pimpl->loadDefaultFont();
  }
}

SbFont::SbFont(const SbFont & other)
{
  pimpl = new SbFontP;
  // For simplicity, just load the same font
  if (other.pimpl->fontname == "ProFont (embedded)") {
    pimpl->loadDefaultFont();
  } else {
    if (!pimpl->loadFontFromFile(other.pimpl->fontname.getString())) {
      pimpl->loadDefaultFont();
    }
  }
  pimpl->size = other.pimpl->size;
  if (pimpl->valid) {
    pimpl->scale = stt_ScaleForPixelHeight(&pimpl->fontinfo, pimpl->size);
  }
}

SbFont::~SbFont()
{
  delete pimpl;
}

SbFont &
SbFont::operator=(const SbFont & other)
{
  if (this != &other) {
    // For simplicity, just load the same font
    if (other.pimpl->fontname == "ProFont (embedded)") {
      pimpl->loadDefaultFont();
    } else {
      if (!pimpl->loadFontFromFile(other.pimpl->fontname.getString())) {
        pimpl->loadDefaultFont();
      }
    }
    pimpl->size = other.pimpl->size;
    if (pimpl->valid) {
      pimpl->scale = stt_ScaleForPixelHeight(&pimpl->fontinfo, pimpl->size);
    }
  }
  return *this;
}

SbBool
SbFont::loadFont(const char * fontpath)
{
  if (pimpl->loadFontFromFile(fontpath)) {
    pimpl->scale = stt_ScaleForPixelHeight(&pimpl->fontinfo, pimpl->size);
    return TRUE;
  }
  return FALSE;
}

SbBool
SbFont::loadFont(const SbString & fontpath)
{
  return loadFont(fontpath.getString());
}

void
SbFont::useDefaultFont(void)
{
  pimpl->loadDefaultFont();
}

SbBool
SbFont::isValid(void) const
{
  return pimpl->valid;
}

SbString
SbFont::getFontName(void) const
{
  return pimpl->fontname;
}

float
SbFont::getSize(void) const
{
  return pimpl->size;
}

void
SbFont::setSize(float size)
{
  if (size > 0.0f) {
    pimpl->size = size;
    if (pimpl->valid) {
      pimpl->scale = stt_ScaleForPixelHeight(&pimpl->fontinfo, size);
      pimpl->clearCache(); // Clear cache when size changes
    }
  }
}

SbVec2f
SbFont::getGlyphAdvance(int character) const
{
  if (!pimpl->valid) return SbVec2f(0, 0);
  
  SbFontP::GlyphCache * glyph = pimpl->findOrCreateGlyph(character);
  if (glyph) {
    return glyph->advance;
  }
  
  // Fallback calculation
  int advanceWidth, leftSideBearing;
  stt_GetCodepointHMetrics(&pimpl->fontinfo, character, &advanceWidth, &leftSideBearing);
  return SbVec2f(advanceWidth * pimpl->scale, 0.0f);
}

SbVec2f
SbFont::getGlyphKerning(int char1, int char2) const
{
  if (!pimpl->valid) return SbVec2f(0, 0);
  
  int kern = stt_GetCodepointKernAdvance(&pimpl->fontinfo, char1, char2);
  return SbVec2f(kern * pimpl->scale, 0.0f);
}

SbBox2f
SbFont::getGlyphBounds(int character) const
{
  if (!pimpl->valid) return SbBox2f();
  
  SbFontP::GlyphCache * glyph = pimpl->findOrCreateGlyph(character);
  if (glyph) {
    return glyph->bounds;
  }
  
  // Fallback calculation  
  int x0, y0, x1, y1;
  stt_GetCodepointBitmapBox(&pimpl->fontinfo, character, pimpl->scale, pimpl->scale, &x0, &y0, &x1, &y1);
  return SbBox2f(SbVec2f(x0, y0), SbVec2f(x1, y1));
}

unsigned char *
SbFont::getGlyphBitmap(int character, SbVec2s & size, SbVec2s & bearing) const
{
  if (!pimpl->valid) {
    size.setValue(0, 0);
    bearing.setValue(0, 0);
    return NULL;
  }
  
  SbFontP::GlyphCache * glyph = pimpl->findOrCreateGlyph(character);
  if (!glyph) {
    size.setValue(0, 0);
    bearing.setValue(0, 0);
    return NULL;
  }
  
  // Generate bitmap if not cached
  if (!glyph->bitmap) {
    int width, height, xoff, yoff;
    glyph->bitmap = stt_GetCodepointBitmap(&pimpl->fontinfo, pimpl->scale, pimpl->scale, 
                                           character, &width, &height, &xoff, &yoff);
    if (glyph->bitmap) {
      glyph->bitmapsize.setValue(width, height);
      glyph->bearing.setValue(xoff, yoff);
    } else {
      glyph->bitmapsize.setValue(0, 0);
      glyph->bearing.setValue(0, 0);
    }
  }
  
  size = glyph->bitmapsize;
  bearing = glyph->bearing;
  return glyph->bitmap;
}

const float *
SbFont::getGlyphVertices(int character, int & numvertices) const
{
  numvertices = 0;
  if (!pimpl->valid) return NULL;
  
  SbFontP::GlyphCache * glyph = pimpl->findOrCreateGlyph(character);
  if (!glyph) return NULL;
  
  numvertices = glyph->numvertices;
  return glyph->vertices;
}

const int *
SbFont::getGlyphFaceIndices(int character, int & numindices) const
{
  numindices = 0;
  if (!pimpl->valid) return NULL;
  
  SbFontP::GlyphCache * glyph = pimpl->findOrCreateGlyph(character);
  if (!glyph) return NULL;
  
  numindices = glyph->numfaceindices;
  return glyph->faceindices;
}

const int *
SbFont::getGlyphEdgeIndices(int character, int & numindices) const
{
  numindices = 0;
  if (!pimpl->valid) return NULL;
  
  SbFontP::GlyphCache * glyph = pimpl->findOrCreateGlyph(character);
  if (!glyph) return NULL;
  
  numindices = glyph->numedgeindices;
  return glyph->edgeindices;
}

SbVec2f
SbFont::getStringBounds(const char * text) const
{
  if (!text || !pimpl->valid) return SbVec2f(0, 0);
  
  float width = 0.0f;
  float height = pimpl->size;
  
  for (const char * p = text; *p; p++) {
    SbVec2f advance = getGlyphAdvance(*p);
    width += advance[0];
    
    // Add kerning for next character
    if (*(p+1)) {
      SbVec2f kern = getGlyphKerning(*p, *(p+1));
      width += kern[0];
    }
  }
  
  return SbVec2f(width, height);
}

float
SbFont::getStringWidth(const char * text) const
{
  return getStringBounds(text)[0];
}