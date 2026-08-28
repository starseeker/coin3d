#ifndef OBOL_SODB_H
#define OBOL_SODB_H

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

/*!
  \class SoDB SoDB.h Inventor/SoDB.h
  \brief Central database and initialisation point for the Obol library.

  \ingroup coin_general

  SoDB holds global state for the Obol runtime: registered file-format
  headers, global fields (including the \c realTime clock field), the
  sensor manager, and the active SoDB::ContextManager used for OpenGL
  context creation.

  All methods are static.  SoDB::init() \b must be called before any
  other Obol API, and SoDB::finish() should be called on shutdown to
  release resources cleanly.  The context-manager argument may be NULL for
  applications that do not use Obol's global OpenGL/offscreen services;
  initialization then succeeds with limited functionality.  A manager may be
  installed later with setContextManager(), or supplied per renderer.

  \section thread_safety Thread Safety

  Obol supports the following concurrent usage scenarios after one thread has
  completed library initialization:

  - **Concurrent render** — Multiple threads, each owning an independent GL
    context and action/state, may render independent scene graphs
    simultaneously.  Call SoDB::init() once, from a quiescent startup thread,
    before starting worker threads.  The shared registries used by this
    supported traversal path are protected by internal synchronization.  This
    does not make arbitrary application callbacks or mutable node instances
    implicitly thread-safe.  GL contexts
    must \e not be used by two threads at the same time; platform rebinding
    (glXMakeCurrent, wglMakeCurrent, or the equivalent) remains the
    application's responsibility.

  - **Init-then-read** — After initialization, multiple threads may traverse
    the same scene graph concurrently while no thread mutates it.  Each thread
    must own its action and state objects.

  - **Mixed read/write** — When one or more threads mutate the scene graph
    while others traverse it, the application must bracket every mutation,
    route change, or connection change with SoDB::writelock() /
    SoDB::writeunlock().  SoAction::apply() acquires the global read lock
    internally.  Explicit read locking is only needed for traversal-like code
    that does not go through SoAction.

  The following operations are not concurrent operations and require a
  quiescent application:

  - SoDB::init(), SoDB::finish(), and SoDB::cleanup().  Do not initialize,
    shut down, or reinitialize the database while worker threads use Obol.
  - SoDB::setContextManager().  The pointer update is synchronized, but the
    caller must keep the old manager alive and ensure that no render is using
    it.  Prefer per-renderer context managers for independent workers.

  The following scenario is not supported without additional application-level
  synchronization:

  - Sharing a single \c SoAction or \c SoState between multiple threads.
    Each thread must create and own its own action objects.

  Do not call SoAction::apply() while holding SoDB::writelock(); apply() takes
  the read lock internally and the write lock is not reentrant.

  \sa SoDB::ContextManager, SoOffscreenRenderer, SoDB::readlock(), SoDB::writelock()
*/
class OBOL_DLL_API SoDB {
public:
  // Forward declaration of ContextManager for init function
  class ContextManager;

  /**
   * Initialize the Obol database and class registry.
   *
   * Passing NULL on the first initialization is supported for non-rendering
   * and custom-backend use.  In that mode initialization completes,
   * getContextManager() returns NULL, and operations requiring the global GL
   * context manager remain unavailable until a manager is installed.  As with
   * every repeated init() call, passing NULL after initialization does not
   * replace an already installed manager.
   */
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
  static void addConverter(SoType from, SoType to, SoType converter);
  static SoType getConverter(SoType from, SoType to);

  static SbBool isInitialized(void);

  static void startNotify(void);
  static SbBool isNotifying(void);
  static void endNotify(void);

  /**
   * Progress callback used by long-running operations. File import reports
   * exact 0 and 1 endpoints, monotonically increasing intermediate fractions,
   * and a final -1 when an interruptible import is successfully cancelled.
   * Returning TRUE requests cancellation only when interruptible is TRUE.
   *
   * Callbacks are invoked from a registry snapshot without an internal lock,
   * so they may add or remove callbacks. Removal affects future snapshots but
   * does not wait for an invocation already in progress; callback userdata
   * must remain alive until those operations have completed.
   */
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

  /*!
    \class SoDB::ContextManager
    \brief Abstract interface for OpenGL context creation and optional software rendering.

    ContextManager provides two complementary rendering paths:

    **GL path** (pure-virtual, must be implemented):
    createOffscreenContext() / makeContextCurrent() / restorePreviousContext() /
    destroyContext() — lifecycle management of an OpenGL offscreen context.
    SoOffscreenRenderer uses these when GL rendering is active.

    **Alternative render path** (optional override, default returns FALSE):
    renderScene() — fill a pre-allocated pixel buffer without using OpenGL.
    When this returns TRUE, SoOffscreenRenderer uses the resulting pixels
    directly and skips the entire GL pipeline.
    SoNanoRTContextManager (<Obol/render/SoNanoRTContextManager.h>) is a
    reference implementation using NanoRT for ray-triangle intersection.

    The two paths are independent: a subclass may implement only the GL path
    (e.g. the OSMesa / GLX managers), only the alternative path (a pure
    software raytracer with no-op GL methods), or both.

    \sa SoDB::init(), SoOffscreenRenderer
  */
  class ContextManager {
  public:
    virtual ~ContextManager() {}

    // --- GL context lifecycle (required) -----------------------------------
    /** \brief Create an offscreen GL context of the given dimensions. \return Opaque context handle, or NULL on failure. */
    virtual void * createOffscreenContext(unsigned int width, unsigned int height) = 0;
    /** \brief Make \a context current for subsequent GL calls. \return TRUE on success. */
    virtual SbBool makeContextCurrent(void * context) = 0;
    /** \brief Restore the context that was current before the last makeContextCurrent() call. */
    virtual void restorePreviousContext(void * context) = 0;
    /** \brief Release all resources associated with \a context. */
    virtual void destroyContext(void * context) = 0;

    /**
     * Return TRUE if the given context handle was created against the OSMesa
     * backend rather than the system OpenGL/GLX/WGL backend.
     *
     * The default implementation returns FALSE, which is appropriate for
     * applications that only use one backend.  Dual-backend implementations
     * (OBOL_DUAL_GL_BUILD) should override this and return TRUE for
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

    /**
     * Look up a GL extension function pointer by name.
     *
     * Called by Obol's GL glue layer when dlsym() cannot locate a function
     * (common for ARB/EXT extension entry points on modern split-GL systems
     * where libOpenGL.so does not export them directly).
     *
     * Platform-specific implementations should delegate to the appropriate
     * platform function-pointer resolver (e.g. glXGetProcAddress on X11,
     * wglGetProcAddress on Windows, eglGetProcAddress on EGL systems).
     * The OSMesa context manager built into Obol overrides this to call
     * OSMesaGetProcAddress(), keeping all OSMesa symbol resolution within
     * the OSMesa library and preventing cross-contamination with system GL
     * function pointers.
     *
     * The default implementation returns nullptr; applications that do not
     * override this will fall back to the existing dlsym-based lookup.
     */
    virtual void * getProcAddress(const char * /*funcName*/) { return nullptr; }

    /**
     * Report the actual pixel dimensions of the backing surface (window,
     * Pbuffer, or renderbuffer) associated with \a context.
     *
     * Obol calls this before issuing a raw glReadPixels() to determine
     * whether the surface is large enough to hold the requested number of
     * pixels.  If the surface is smaller than the requested render target
     * **and** framebuffer objects are unavailable to compensate, Obol will
     * skip the readback and post a diagnostic warning explaining what the
     * application must provide for the feature to work — preventing the
     * memory corruption that results from reading beyond a tiny framebuffer.
     *
     * The default returns (0, 0), which means "unknown".  If framebuffer
     * objects are unavailable, Obol will skip raw readback from a surface of
     * unknown size rather than risk reading beyond it.  Context managers that
     * support direct framebuffer readback must override this method.
     */
    virtual void getActualSurfaceSize(void * /*context*/,
                                      unsigned int & width,
                                      unsigned int & height) const
    { width = 0; height = 0; }

    /**
     * Return the writable color buffer for the software context that is
     * current on this thread.  The pointer is transient and is valid only
     * while that context remains current.  Backends with no directly
     * writable framebuffer return FALSE.
     */
    virtual SbBool getCurrentSoftwareFramebuffer(unsigned char *& pixels,
                                                  unsigned int & width,
                                                  unsigned int & height,
                                                  unsigned int & components)
    { pixels = nullptr; width = height = components = 0; return FALSE; }

    // --- Optional alternative rendering path -------------------------------
    // If this returns TRUE, SoOffscreenRenderer uses 'pixels' directly and
    // skips the GL pipeline.  'pixels' is a pre-allocated row-major buffer of
    // width*height*nrcomponents bytes.  Components 1 through 4 represent
    // luminance, luminance-alpha, RGB, and RGBA respectively.  Rows use the
    // bottom-to-top SoOffscreenRenderer::getBuffer() convention.  The renderer
    // initializes the buffer to its background (including any gradient) before
    // this call, so an implementation may leave uncovered pixels unchanged.
    // 'background_rgb' is a 3-element float array [R,G,B] in [0,1].
    // Default implementation returns FALSE (GL path is used).
    virtual SbBool renderScene(SoNode * scene,
                               unsigned int width, unsigned int height,
                               unsigned char * pixels,
                               unsigned int nrcomponents,
                               const float background_rgb[3]) { (void)scene; (void)width; (void)height; (void)pixels; (void)nrcomponents; (void)background_rgb; return FALSE; }

    /**
     * Optional diagnostic hook for backend and renderer messages.  The default
     * implementation is a no-op; applications that want structured backend
     * diagnostics can override this instead of scraping SoDebugError output.
     */
    virtual void reportDiagnostic(const char * component,
                                  const char * message)
    { (void)component; (void)message; }
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
   * library was not built with OSMesa support (i.e. OBOL_SWRAST_BUILD
   * and OBOL_DUAL_GL_BUILD are both absent).
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

#endif // !OBOL_SODB_H
