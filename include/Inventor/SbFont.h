#ifndef COIN_SBFONT_H
#define COIN_SBFONT_H

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

#include <Inventor/SbBasic.h>
#include <Inventor/SbBox2f.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbString.h>

class SbFontP;

/*!
  \class SbFont SbFont.h Inventor/SbFont.h
  \brief The SbFont class is a clean, idiomatic C++ API for font management.

  This class provides a simple interface for loading and using fonts in Coin3D.
  It uses struetype (TrueType font library) with the embedded ProFont as fallback.
  
  The design is intentionally simple - just load a font file or use the default,
  then get glyph information for text rendering. All system font detection and
  multiple format support has been removed for simplicity.

  \COIN_CLASS_EXTENSION

  \since Coin 4.1
*/
class COIN_DLL_API SbFont {
public:
  SbFont(void);
  SbFont(const char * fontpath);
  SbFont(const SbString & fontpath);
  SbFont(const SbFont & other);
  ~SbFont();

  SbFont & operator=(const SbFont & other);

  SbBool loadFont(const char * fontpath);
  SbBool loadFont(const SbString & fontpath);
  void useDefaultFont(void);

  SbBool isValid(void) const;
  SbString getFontName(void) const;
  float getSize(void) const;
  void setSize(float size);

  // Glyph metrics
  SbVec2f getGlyphAdvance(int character) const;
  SbVec2f getGlyphKerning(int char1, int char2) const;
  SbBox2f getGlyphBounds(int character) const;

  // Bitmap glyph rendering (for 2D text)
  unsigned char * getGlyphBitmap(int character, SbVec2s & size, SbVec2s & bearing) const;

  // Vector glyph rendering (for 3D text)
  const float * getGlyphVertices(int character, int & numvertices) const;
  const int * getGlyphFaceIndices(int character, int & numindices) const;
  const int * getGlyphEdgeIndices(int character, int & numindices) const;

  // String metrics
  SbVec2f getStringBounds(const char * text) const;
  float getStringWidth(const char * text) const;

private:
  SbFontP * pimpl;
};

#endif // !COIN_SBFONT_H