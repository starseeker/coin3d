#ifndef OBOL_SOTEXT2_H
#define OBOL_SOTEXT2_H

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

#include <Inventor/nodes/SoSubNode.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/fields/SoMFString.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFEnum.h>
#include <vector>

class OBOL_DLL_API SoText2 : public SoShape {
  typedef SoShape inherited;

  SO_NODE_HEADER(SoText2);

public:
  static void initClass(void);
  SoText2(void);

  enum Justification {
    LEFT = 1,
    RIGHT,
    CENTER
  };

  SoMFString string;
  SoSFFloat spacing;
  SoSFEnum justification;
  /*!
   * When TRUE (the default), text pixels are depth-tested against geometry
   * that was drawn earlier in the scene — matching upstream Coin behaviour.
   * Set to FALSE to draw the label always on top of (i.e. unoccluded by)
   * any 3D geometry, which is the classic SGI OpenInventor 2.1 behaviour.
   */
  SoSFBool depthTest;

  virtual void GLRender(SoGLRenderAction * action);
  virtual void rayPick(SoRayPickAction * action);
  virtual void getPrimitiveCount(SoGetPrimitiveCountAction * action);

  SbBool getTextQuad(SoState * state,
                     SbVec3f & v0, SbVec3f & v1,
                     SbVec3f & v2, SbVec3f & v3) const;

  /*!
   * Generate per-glyph billboard quads for non-GL rendering backends
   * (e.g. ray-tracers).
   *
   * For each visible glyph in the text, four vertices are appended to
   * \a quads in object space, counter-clockwise:
   *   v[4*i+0] = top-left,    v[4*i+1] = top-right,
   *   v[4*i+2] = bottom-right, v[4*i+3] = bottom-left.
   * The quads are screen-aligned at the anchor-point depth and sized to
   * exactly match the pixel extent of each glyph bitmap.  Whitespace
   * characters (zero-bitmap-size glyphs) are silently skipped.
   *
   * \param state  Current traversal state.  Must contain valid view volume,
   *               model matrix, viewport region, and font elements.
   * \param quads  Output vector: 4 × SbVec3f per glyph in groups of four.
   * \return       Number of quads written (0 = nothing renderable).
   *
   * \sa getTextQuad()
   */
  int buildGlyphQuads(SoState * state,
                      std::vector<SbVec3f> & quads) const;

  /*!
   * Build a ready-to-composite RGBA pixel buffer for non-GL rendering backends
   * (e.g. CPU ray-tracers) that cannot call glDrawPixels directly.
   *
   * The returned buffer uses the same binary-threshold alpha that GLRender uses:
   * each pixel is either fully opaque (stb coverage >= 50%) or transparent.
   * Rows are stored bottom-to-top (GL convention).
   *
   * \param state   Current traversal state (view volume, viewport, font, material).
   * \param pixbuf  Filled with width × height × 4 RGBA bytes (bottom-to-top rows).
   * \param out_x   Left edge of the buffer in viewport coordinates (pixels).
   * \param out_y   Bottom edge of the buffer in viewport coordinates (pixels).
   * \param out_w   Width of the buffer in pixels.
   * \param out_h   Height of the buffer in pixels.
   * \return        TRUE if any text was written; FALSE if nothing to render.
   */
  SbBool buildPixelBuffer(SoState * state,
                          std::vector<unsigned char> & pixbuf,
                          int & out_x, int & out_y,
                          int & out_w, int & out_h) const;

protected:
  virtual ~SoText2();

  virtual void generatePrimitives(SoAction * action);
  virtual void computeBBox(SoAction * action, SbBox3f & box, SbVec3f & center);

private:
  class SoText2P * pimpl;
  friend class SoText2P;                     
};

#endif // !OBOL_SOTEXT2_H
