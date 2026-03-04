#ifndef OBOL_SOOFFSCREENRENDERER_H
#define OBOL_SOOFFSCREENRENDERER_H

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

#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbColor.h>
#include <Inventor/lists/SbList.h>
#include <Inventor/lists/SbPList.h>
#include <Inventor/SbString.h>
#include <Inventor/SbName.h>
#include <Inventor/SoDB.h>

#include <cstdio>

class SoBase;
class SoGLRenderAction;
class SoNode;
class SoPath;

// This shouldn't strictly be necessary, but the OSF1/cxx compiler
// complains if this is left out, while using the "friend class
// SoExtSelectionP" statement in the class definition.
class SoOffscreenRendererP;

/*!
  \class SoOffscreenRenderer SoOffscreenRenderer.h Inventor/SoOffscreenRenderer.h
  \brief Render an Open Inventor scene graph to an offscreen pixel buffer.

  \ingroup coin_general

  SoOffscreenRenderer renders a scene graph into an offscreen pixel buffer
  without requiring an on-screen window.  The rendered pixels can be read
  back and written to image files (RGB, PostScript, or any registered
  writer format).

  Rendering can use either the system OpenGL pipeline (via GLX/WGL) or the
  software OSMesa pipeline, controlled through the SoDB::ContextManager
  supplied at construction time.  When no explicit manager is provided the
  global singleton from SoDB::getContextManager() is used.

  Typical usage:
  \code
  SbViewportRegion vp(800, 600);
  SoOffscreenRenderer renderer(vp);
  renderer.setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));
  if (renderer.render(root)) {
      renderer.writeToRGB("output.rgb");
  }
  \endcode

  \sa SoDB::ContextManager, SoRenderManager
*/
class OBOL_DLL_API SoOffscreenRenderer {
public:
  enum Components {
    LUMINANCE = 1,
    LUMINANCE_TRANSPARENCY = 2,
    RGB = 3,
    RGB_TRANSPARENCY = 4
  };

  SoOffscreenRenderer(const SbViewportRegion & viewportregion);
  SoOffscreenRenderer(SoGLRenderAction * action);
  // Constructors that accept an explicit context manager, removing any
  // dependency on the global SoDB::getContextManager() singleton.
  // The \a manager must not be NULL; use the standard constructors to fall
  // back to the global singleton.
  SoOffscreenRenderer(SoDB::ContextManager * manager, const SbViewportRegion & viewportregion);
  SoOffscreenRenderer(SoDB::ContextManager * manager, SoGLRenderAction * action);
  ~SoOffscreenRenderer();

  static float getScreenPixelsPerInch(void);
  static void setScreenPixelsPerInch(float dpi);
  static SbVec2s getMaximumResolution(void);
  void setComponents(const Components components);
  Components getComponents(void) const;
  void setViewportRegion(const SbViewportRegion & region);
  const SbViewportRegion & getViewportRegion(void) const;
  void setBackgroundColor(const SbColor & color);
  const SbColor & getBackgroundColor(void) const;
  void setBackgroundGradient(const SbColor & bottom, const SbColor & top);
  void clearBackgroundGradient(void);
  SbBool hasBackgroundGradient(void) const;
  void setGLRenderAction(SoGLRenderAction * action);
  SoGLRenderAction * getGLRenderAction(void) const;
  SbBool render(SoNode * scene);
  SbBool render(SoPath * scene);
  unsigned char * getBuffer(void) const;
  const void * const & getDC(void) const;

  SbBool writeToRGB(FILE * fp) const;
  SbBool writeToPostScript(FILE * fp) const;
  SbBool writeToPostScript(FILE * fp, const SbVec2f & printsize) const;

  SbBool writeToRGB(const char * filename) const;
  SbBool writeToPostScript(const char * filename) const;
  SbBool writeToPostScript(const char * filename, const SbVec2f & printsize) const;

  SbBool isWriteSupported(const SbName & filetypeextension) const;
  int getNumWriteFiletypes(void) const;
  void getWriteFiletypeInfo(const int idx,
                            SbPList & extlist,
                            SbString & fullname,
                            SbString & description);
  SbBool writeToFile(const SbString & filename, const SbName & filetypeextension) const;

  void setPbufferEnable(SbBool enable);
  SbBool getPbufferEnable(void) const;

  // Context management and OpenGL capability detection.
  // These instance methods use the per-instance context manager.
  void getOpenGLVersion(int & major, int & minor, int & release) const;
  SbBool isOpenGLExtensionSupported(const char * extension) const;
  SbBool hasFramebufferObjectSupport(void) const;
  SbBool isVersionAtLeast(int major, int minor, int release = 0) const;

  // Per-instance context manager.  The manager-accepting constructors pass it
  // directly; the legacy constructors fall back to SoDB::getContextManager().
  // Pass NULL to setContextManager() to revert to the global singleton.
  void setContextManager(SoDB::ContextManager * manager);
  SoDB::ContextManager * getContextManager(void) const;



private:
  friend class SoOffscreenRendererP;
  class SoOffscreenRendererP * pimpl;
};

#endif // !OBOL_SOOFFSCREENRENDERER_H
