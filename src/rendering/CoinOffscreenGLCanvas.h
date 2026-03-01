#ifndef OBOL_COINOFFSCREENGLCANVAS_H
#define OBOL_COINOFFSCREENGLCANVAS_H

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

#ifndef OBOL_INTERNAL
#error this is a private header file
#endif /* ! OBOL_INTERNAL */

// *************************************************************************

#include <Inventor/SbVec2s.h>
#include <Inventor/SoDB.h>

// *************************************************************************

class CoinOffscreenGLCanvas {
public:
  CoinOffscreenGLCanvas(void);
  virtual ~CoinOffscreenGLCanvas();

  uint32_t activateGLContext(void);
  void deactivateGLContext(void);

  void setWantedSize(SbVec2s size);
  const SbVec2s & getActualSize(void) const;

  void readPixels(uint8_t * dst, const SbVec2s & vpdims,
                  unsigned int dstrowsize,
                  unsigned int nrcomponents) const;

  // Per-instance context manager override.  When set, this manager is used
  // for all context lifecycle operations.  SoOffscreenRenderer always calls
  // setContextManager() before using the canvas (capturing the global at
  // renderer construction time), so this is never NULL in normal operation.
  void setContextManager(SoDB::ContextManager * mgr);

  static SbBool debug(void);

  static SbBool allowResourcehog(void);

  const void * const & getHDC(void) const; // ugliness to support SoOffscreenRenderer::getDC()
  void updateDCBitmap();	
private:
  SbBool clampSize(SbVec2s & s) const;
  static void clampToPixelSizeRoof(SbVec2s & s);
  static SbVec2s getMaxTileSize(SoDB::ContextManager * mgr);
  static unsigned int tilesizeroof;
  uint32_t tryActivateGLContext(void);
  void destructContext(void);
  
  // FBO management helpers
  SbBool initializeFBO(void);
  void cleanupFBO(void);
  SbBool bindFBO(void);
  void unbindFBO(void);

  // Returns the per-instance context manager.  Set by SoOffscreenRenderer
  // before each use so this is never NULL in normal operation.
  SoDB::ContextManager * effectiveMgr(void) const;
  
  SbVec2s size;

  void * context;
  uint32_t renderid;
  const void * current_hdc;

  // Per-instance context manager (NULL → use global singleton)
  SoDB::ContextManager * instance_mgr;
  
  // FBO-based offscreen rendering support
  unsigned int fbo;          // Framebuffer object ID
  unsigned int color_rb;     // Color renderbuffer ID  
  unsigned int depth_rb;     // Depth renderbuffer ID
  SbBool fbo_initialized;
};

// *************************************************************************

#endif // !OBOL_COINOFFSCREENGLCANVAS_H
