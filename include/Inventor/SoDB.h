#ifndef COIN_SODB_H
#define COIN_SODB_H

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
#include <Inventor/SbString.h>
#include <Inventor/SoType.h>
#include <Inventor/sensors/SoSensorManager.h>

class SbName;
class SbTime;
class SoBase;
class SoField;
class SoInput;
class SoNode;
class SoPath;
class SoSeparator;
class SoGroup;

typedef void SoDBHeaderCB(void * data, SoInput * input);


class COIN_DLL_API SoDB {
public:
  // Forward declaration of ContextManager for init function
  class ContextManager;

  static void init(ContextManager * context_manager);
  static void finish(void);
  static void cleanup(void);

  static const char * getVersion(void);
  static SbBool read(SoInput * input, SoPath *& path);
  static SbBool read(SoInput * input, SoBase *& base);
  static SbBool read(SoInput * input, SoNode *& rootnode);
  static SoSeparator * readAll(SoInput * input);
  static SbBool isValidHeader(const char * teststring);
  static SbBool registerHeader(const SbString & headerstring,
                               SbBool isbinary,
                               float ivversion,
                               SoDBHeaderCB * precallback,
                               SoDBHeaderCB * postcallback,
                               void * userdata = NULL);
  static SbBool getHeaderData(const SbString & headerstring,
                              SbBool & isbinary,
                              float & ivversion,
                              SoDBHeaderCB *& precallback,
                              SoDBHeaderCB *& postcallback,
                              void *& userdata,
                              SbBool substringok = FALSE);
  static int getNumHeaders(void);
  static SbString getHeaderString(const int i);
  static SoField * createGlobalField(const SbName & name, SoType type);
  static SoField * getGlobalField(const SbName & name);
  static void renameGlobalField(const SbName & from, const SbName & to);

  static void setRealTimeInterval(const SbTime & interval);
  static const SbTime & getRealTimeInterval(void);
  static void enableRealTimeSensor(SbBool on);

  static SoSensorManager * getSensorManager(void);
  static void setDelaySensorTimeout(const SbTime & t);
  static const SbTime & getDelaySensorTimeout(void);
  static int doSelect(int nfds, void * readfds, void * writefds,
                      void * exceptfds, struct timeval * usertimeout);

  static void addConverter(SoType from, SoType to, SoType converter);
  static SoType getConverter(SoType from, SoType to);

  static SbBool isInitialized(void);

  static void startNotify(void);
  static SbBool isNotifying(void);
  static void endNotify(void);

  typedef SbBool ProgressCallbackType(const SbName & itemid, float fraction,
                                      SbBool interruptible, void * userdata);
  static void addProgressCallback(ProgressCallbackType * func, void * userdata);
  static void removeProgressCallback(ProgressCallbackType * func, void * userdata);

  static SbBool isMultiThread(void);
  static void readlock(void);
  static void readunlock(void);
  static void writelock(void);
  static void writeunlock(void);

  static void createRoute(SoNode * from, const char * eventout,
                          SoNode * to, const char * eventin);
  static void removeRoute(SoNode * from, const char * eventout,
                          SoNode * to, const char * eventin);

  // Context Management API
  //
  // The ContextManager provides two complementary rendering paths:
  //
  // GL path (pure-virtual, must be implemented):
  //   createOffscreenContext / makeContextCurrent / restorePreviousContext /
  //   destroyContext – lifecycle management of an OpenGL offscreen context.
  //   SoOffscreenRenderer uses these when GL rendering is active.
  //
  // Alternative render path (optional override, default returns FALSE):
  //   renderScene() – fill a pre-allocated pixel buffer with rendered output
  //   without using OpenGL.  When this returns TRUE, SoOffscreenRenderer
  //   uses the resulting pixels directly and skips the entire GL pipeline.
  //   SoNanoRTContextManager (tests/utils/nanort_context_manager.h) is a
  //   reference implementation that uses nanort for ray-triangle intersection.
  //
  // The two paths are independent: a subclass may implement only the GL path
  // (existing OSMesa / GLX managers), only the alternative path (a pure
  // software raytracer with no-op GL methods), or both.
  class ContextManager {
  public:
    virtual ~ContextManager() {}

    // --- GL context lifecycle (required) -----------------------------------
    virtual void * createOffscreenContext(unsigned int width, unsigned int height) = 0;
    virtual SbBool makeContextCurrent(void * context) = 0;
    virtual void restorePreviousContext(void * context) = 0;
    virtual void destroyContext(void * context) = 0;

    /**
     * Return TRUE if the given context handle was created against the OSMesa
     * backend rather than the system OpenGL/GLX/WGL backend.
     *
     * The default implementation returns FALSE, which is appropriate for
     * applications that only use one backend.  Dual-backend implementations
     * (COIN3D_BUILD_DUAL_GL) should override this and return TRUE for
     * contexts created via OSMesa so that the GL-glue dispatch layer can
     * route SoGLContext_instance() to the correct (osmesa_*) implementation.
     */
    virtual SbBool isOSMesaContext(void * /*context*/) { return FALSE; }

    /**
     * Report the maximum offscreen rendering dimensions supported by this
     * backend.  CoinOffscreenGLCanvas calls this instead of probing the
     * global GL pipeline so that per-instance managers (e.g. an OSMesa
     * renderer living alongside a system-GL renderer) can declare the right
     * limits for their backend.
     *
     * The default implementation returns {0,0}, which causes CoinOffscreenGLCanvas
     * to fall back to its global GL-probing logic (the traditional behaviour for
     * the global context manager).  An OSMesa implementation should return a
     * large value (e.g. 16384 × 16384) since OSMesa is only RAM-limited.
     */
    virtual void maxOffscreenDimensions(unsigned int & width,
                                        unsigned int & height) const
    { width = 0; height = 0; }

    // --- Optional alternative rendering path -------------------------------
    // If this returns TRUE, SoOffscreenRenderer uses 'pixels' directly and
    // skips the GL pipeline.  'pixels' is a pre-allocated row-major buffer of
    // width*height*nrcomponents bytes (RGB or RGBA, values 0-255, top-to-bottom
    // row order matching SoOffscreenRenderer::getBuffer()).
    // 'background_rgb' is a 3-element float array [R,G,B] in [0,1].
    // Default implementation returns FALSE (GL path is used).
    virtual SbBool renderScene(SoNode * scene,
                               unsigned int width, unsigned int height,
                               unsigned char * pixels,
                               unsigned int nrcomponents,
                               const float background_rgb[3]) { (void)scene; (void)width; (void)height; (void)pixels; (void)nrcomponents; (void)background_rgb; return FALSE; }
  };

  static ContextManager * getContextManager(void);

  /**
   * Replace the active context manager at runtime without re-running
   * SoDB initialisation.  Useful for temporarily switching to a different
   * rendering backend (e.g. swapping between system GL and OSMesa) within
   * the same process.  The caller is responsible for ensuring that no
   * render is in progress when this is called.  Passing NULL is a no-op.
   */
  static void setContextManager(ContextManager * manager);

  /**
   * Create a new OSMesa-backed context manager.  Returns NULL when the
   * library was not built with OSMesa support (i.e. COIN3D_OSMESA_BUILD
   * and COIN3D_BUILD_DUAL_GL are both absent).
   *
   * The caller owns the returned object and is responsible for deleting it
   * after all SoOffscreenRenderer instances that reference it have been
   * destroyed.  A typical use is to store it in a std::unique_ptr:
   *
   *   auto mgr = std::unique_ptr<SoDB::ContextManager>(
   *                   SoDB::createOSMesaContextManager());
   *   if (mgr) renderer->setContextManager(mgr.get());
   *
   * This API lets applications use a dedicated OSMesa rendering backend
   * per SoOffscreenRenderer instance without needing to include any
   * OSMesa headers or link directly against the OSMesa library.
   */
  static ContextManager * createOSMesaContextManager();

private:
  static SoGroup * readAllWrapper(SoInput * input, const SoType & grouptype);
};

#endif // !COIN_SODB_H
