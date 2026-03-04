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
 * Utility functions for headless rendering of Coin examples
 * 
 * This provides common functionality for converting interactive
 * Mentor examples to headless, offscreen rendering tests that
 * produce reference images for validation.
 *
 * Backend selection (compile-time):
 *   OBOL_OSMESA_BUILD: use OSMesa for truly headless operation
 *   default:             use system OpenGL (GLX on Linux) with Xvfb
 *
 * Both paths require a SoDB::ContextManager since this Coin fork's
 * SoDB::init() always requires one.
 */

#ifndef HEADLESS_UTILS_H
#define HEADLESS_UTILS_H

#include <Inventor/SoDB.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <cstdio>
#include <cstring>
#include <cmath>

// Default image dimensions
#define DEFAULT_WIDTH 800
#define DEFAULT_HEIGHT 600

#ifdef OBOL_NANORT_BUILD
// ============================================================================
// NanoRT Backend: CPU raytracing, no OpenGL context required
// ============================================================================
// Uses SoNanoRTContextManager which overrides SoDB::ContextManager::renderScene()
// so that SoOffscreenRenderer::render() calls the nanort raytracing pipeline
// instead of the GL pipeline.  All existing test code (renderToFile,
// writeToRGB, etc.) works unchanged with this backend.
#include "nanort_context_manager.h"

namespace {
    /* Meyer's singleton for the NanoRT context manager.  Shared between
       initCoinHeadless() and getSharedRenderer() so neither function needs
       to consult SoDB::getContextManager(). */
    inline SoNanoRTContextManager & nrt_context_manager_singleton() {
        static SoNanoRTContextManager instance;
        return instance;
    }
} // anonymous namespace

/**
 * Initialize Coin database for headless operation (NanoRT backend).
 * No OpenGL context is required.
 */
inline void initCoinHeadless() {
    SoDB::init(&nrt_context_manager_singleton());
    SoNodeKit::init();
    SoInteraction::init();
}

/**
 * Return the single persistent offscreen renderer shared by all headless
 * examples (NanoRT backend).
 */
inline SoOffscreenRenderer* getSharedRenderer() {
    static SoOffscreenRenderer *s_renderer = nullptr;
    if (!s_renderer) {
        SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        s_renderer = new SoOffscreenRenderer(&nrt_context_manager_singleton(), vp);
    }
    return s_renderer;
}

/**
 * Return the context manager installed by initCoinHeadless() (NanoRT backend).
 */
inline SoDB::ContextManager * getCoinHeadlessContextManager() {
    return &nrt_context_manager_singleton();
}

/**
 * Render a scene to an SGI RGB file (NanoRT backend).
 * Uses SoOffscreenRenderer which dispatches to SoNanoRTContextManager::renderScene().
 */
inline bool renderToFile(
    SoNode *root,
    const char *filename,
    int width = DEFAULT_WIDTH,
    int height = DEFAULT_HEIGHT,
    const SbColor &backgroundColor = SbColor(0.0f, 0.0f, 0.0f))
{
    if (!root || !filename) {
        fprintf(stderr, "Error: Invalid parameters to renderToFile\n");
        return false;
    }

    SoOffscreenRenderer *renderer = getSharedRenderer();
    SbViewportRegion viewport(width, height);
    renderer->setViewportRegion(viewport);
    renderer->setComponents(SoOffscreenRenderer::RGB);
    renderer->setBackgroundColor(backgroundColor);

    if (!renderer->render(root)) {
        fprintf(stderr, "Error: Failed to render scene (NanoRT)\n");
        return false;
    }

    if (!renderer->writeToRGB(filename)) {
        fprintf(stderr, "Error: Failed to write to RGB file %s\n", filename);
        return false;
    }

    printf("Successfully rendered to %s (%dx%d) [NanoRT]\n",
           filename, width, height);
    return true;
}

#elif defined(OBOL_EMBREE_BUILD)
// ============================================================================
// Embree Backend: CPU raytracing via Intel Embree 4, no OpenGL required
// ============================================================================
// Uses SoEmbreeContextManager (see tests/utils/embree_context_manager.h) which
// delegates scene collection to SoRaytracerSceneCollector and performs ray-
// triangle intersection via Embree's high-performance BVH.
#include "embree_context_manager.h"

namespace {
    inline SoEmbreeContextManager & emb_context_manager_singleton() {
        static SoEmbreeContextManager instance;
        return instance;
    }
} // anonymous namespace

inline void initCoinHeadless() {
    SoDB::init(&emb_context_manager_singleton());
    SoNodeKit::init();
    SoInteraction::init();
}

inline SoOffscreenRenderer* getSharedRenderer() {
    static SoOffscreenRenderer *s_renderer = nullptr;
    if (!s_renderer) {
        SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        s_renderer = new SoOffscreenRenderer(&emb_context_manager_singleton(), vp);
    }
    return s_renderer;
}

inline SoDB::ContextManager * getCoinHeadlessContextManager() {
    return &emb_context_manager_singleton();
}

inline bool renderToFile(
    SoNode *root,
    const char *filename,
    int width = DEFAULT_WIDTH,
    int height = DEFAULT_HEIGHT,
    const SbColor &backgroundColor = SbColor(0.0f, 0.0f, 0.0f))
{
    if (!root || !filename) {
        fprintf(stderr, "Error: Invalid parameters to renderToFile\n");
        return false;
    }

    SoOffscreenRenderer *renderer = getSharedRenderer();
    SbViewportRegion viewport(width, height);
    renderer->setViewportRegion(viewport);
    renderer->setComponents(SoOffscreenRenderer::RGB);
    renderer->setBackgroundColor(backgroundColor);

    if (!renderer->render(root)) {
        fprintf(stderr, "Error: Failed to render scene (Embree)\n");
        return false;
    }

    if (!renderer->writeToRGB(filename)) {
        fprintf(stderr, "Error: Failed to write to RGB file %s\n", filename);
        return false;
    }

    printf("Successfully rendered to %s (%dx%d) [Embree]\n",
           filename, width, height);
    return true;
}

#elif defined(OBOL_OSMESA_BUILD)
// ============================================================================
// OSMesa Backend: For offscreen/headless rendering without display server.
// Used for pure-OSMesa builds only.  In OBOL_BUILD_DUAL_GL builds the
// Coin scene traversal still uses system GL symbols, so tests for that build
// fall through to the system-GL / GLX path below (requires Xvfb).
// Headers come from the project's own submodule (external/osmesa/include/OSMesa/)
// ============================================================================
#include <OSMesa/osmesa.h>
#include <Inventor/gl.h>
#include <memory>

// OSMesa context structure for offscreen rendering
struct CoinOSMesaContext {
    OSMesaContext context;
    std::unique_ptr<unsigned char[]> buffer;
    int width, height;
    // Previous context/buffer saved at makeContextCurrent time so that
    // restorePreviousContext() can reinstate it after temporary use.
    OSMesaContext prev_context;
    void         *prev_buffer;
    GLsizei       prev_width, prev_height, prev_bytesPerRow;
    GLenum        prev_format;

    CoinOSMesaContext(int w, int h) : width(w), height(h),
        prev_context(nullptr), prev_buffer(nullptr),
        prev_width(0), prev_height(0), prev_bytesPerRow(0), prev_format(0)
    {
        context = OSMesaCreateContextExt(OSMESA_RGBA, 24, 0, 0, NULL);
        if (context) {
            buffer = std::make_unique<unsigned char[]>(width * height * 4);
        }
    }

    ~CoinOSMesaContext() {
        if (context) {
            OSMesaDestroyContext(context);
            context = nullptr;  /* defensive: zero out so any stale-ptr check is safe */
        }
    }

    bool makeCurrent() {
        if (!context) return false;
        // Save the currently-active OSMesa context so restorePreviousContext
        // can put it back.  OSMesaGetCurrentContext() returns NULL when none
        // is active, which is a valid value to restore to.
        prev_context = OSMesaGetCurrentContext();
        prev_buffer  = nullptr;
        prev_width   = prev_height = prev_bytesPerRow = 0;
        prev_format  = 0;
        if (prev_context) {
            GLint fmt = 0;
            OSMesaGetColorBuffer(prev_context, &prev_width, &prev_height,
                                 &fmt, &prev_buffer);
            prev_format = (GLenum)fmt;
        }
        return OSMesaMakeCurrent(context, buffer.get(), GL_UNSIGNED_BYTE,
                                 width, height) != 0;
    }

    bool isValid() const { return context != nullptr; }
};

// OSMesa context manager for Coin
class CoinHeadlessContextManager : public SoDB::ContextManager {
public:
    virtual void* createOffscreenContext(unsigned int width, unsigned int height) override {
        auto* ctx = new CoinOSMesaContext(width, height);
        return ctx->isValid() ? ctx : (delete ctx, nullptr);
    }

    virtual SbBool isOSMesaContext(void* /*context*/) override {
        /* All contexts created by this manager are OSMesa contexts.
         * Returning TRUE lets CoinOffscreenGLCanvas call
         * coingl_register_osmesa_context() so that SoGLContext_instance()
         * routes to the osmesa_ GL dispatch path instead of the system GL path
         * (which would crash on NULL glGetString results). */
        return TRUE;
    }
    
    virtual SbBool makeContextCurrent(void* context) override {
        return context && static_cast<CoinOSMesaContext*>(context)->makeCurrent() ? TRUE : FALSE;
    }
    
    virtual void restorePreviousContext(void* context) override {
        CoinOSMesaContext *ctx = static_cast<CoinOSMesaContext*>(context);
        if (!ctx) return;
        if (ctx->prev_context && ctx->prev_buffer) {
            // Reinstate the previously-active context with its buffer.
            OSMesaMakeCurrent(ctx->prev_context, ctx->prev_buffer,
                              GL_UNSIGNED_BYTE,
                              ctx->prev_width, ctx->prev_height);
        } else {
            // No previous context was active; release the current binding.
            OSMesaMakeCurrent(nullptr, nullptr, 0, 0, 0);
        }
    }
    
    virtual void destroyContext(void* context) override {
        CoinOSMesaContext *ctx = static_cast<CoinOSMesaContext*>(context);
        if (!ctx) return;
        /* Unbind before destroying to avoid corrupting Mesa internal state
         * if the context is still current (see also CoinOSMesaContextManagerImpl). */
        if (ctx->context && OSMesaGetCurrentContext() == ctx->context)
            OSMesaMakeCurrent(nullptr, nullptr, 0, 0, 0);
        delete ctx;
    }

    virtual void * getProcAddress(const char * funcName) override {
        return reinterpret_cast<void*>(OSMesaGetProcAddress(funcName));
    }
};

namespace {
    /* The single OSMesa context manager instance shared between initCoinHeadless()
       and getCoinHeadlessContextManager().  Using a local-static (Meyer's singleton)
       pattern ensures thread-safe initialisation (C++11 and later), a well-defined
       construction order, and that the manager outlives every caller that holds a
       pointer to it – all without touching SoDB::getContextManager(). */
    inline CoinHeadlessContextManager & headless_context_manager_singleton() {
        static CoinHeadlessContextManager instance;
        return instance;
    }
} // anonymous namespace

/**
 * Initialize Coin database for headless operation (OSMesa backend)
 */
inline void initCoinHeadless() {
    SoDB::init(&headless_context_manager_singleton());
    SoNodeKit::init();
    SoInteraction::init();
}

/**
 * Return the context manager installed by initCoinHeadless().
 * Must be called after initCoinHeadless().
 *
 * Returns the same manager object passed to SoDB::init(), so that callers
 * can create SoOffscreenRenderer instances with an explicit manager instead
 * of relying on the global singleton.
 */
inline SoDB::ContextManager * getCoinHeadlessContextManager() {
    return &headless_context_manager_singleton();
}

/**
 * Get a shared persistent SoOffscreenRenderer for the OSMesa backend.
 * Some examples reuse an offscreen renderer to capture intermediate frames
 * (e.g. to generate a texture map from a rendered scene).  Providing a
 * shared instance mirrors the GLX backend behaviour.
 */
inline SoOffscreenRenderer* getSharedRenderer() {
    static SoOffscreenRenderer *s_renderer = nullptr;
    if (!s_renderer) {
        SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        s_renderer = new SoOffscreenRenderer(&headless_context_manager_singleton(), vp);
    }
    return s_renderer;
}

/**
 * Render a scene to an image file (OSMesa backend).
 *
 * Uses the shared persistent renderer to avoid creating multiple OSMesa
 * contexts in the same process.  Creating separate SoOffscreenRenderer
 * instances (each with its own OSMesa context) while rendering scenes that
 * contain SoText3 nodes triggers a pre-existing OSMesa font-cache crash
 * because each context attempts to reinitialise the font state.  Reusing a
 * single shared context prevents this.  This matches the GLX backend
 * behaviour where renderToFile also delegates to getSharedRenderer().
 */
inline bool renderToFile(
    SoNode *root,
    const char *filename,
    int width = DEFAULT_WIDTH,
    int height = DEFAULT_HEIGHT,
    const SbColor &backgroundColor = SbColor(0.0f, 0.0f, 0.0f))
{
    if (!root || !filename) {
        fprintf(stderr, "Error: Invalid parameters to renderToFile\n");
        return false;
    }

    SoOffscreenRenderer *renderer = getSharedRenderer();
    SbViewportRegion viewport(width, height);
    renderer->setViewportRegion(viewport);
    renderer->setComponents(SoOffscreenRenderer::RGB);
    renderer->setBackgroundColor(backgroundColor);

    if (!renderer->render(root)) {
        fprintf(stderr, "Error: Failed to render scene\n");
        return false;
    }

    if (!renderer->writeToRGB(filename)) {
        fprintf(stderr, "Error: Failed to write to RGB file %s\n", filename);
        return false;
    }

    printf("Successfully rendered to %s (%dx%d)\n", filename, width, height);
    return true;
}

#else // !OBOL_OSMESA_BUILD
// ============================================================================
// System OpenGL Backend: GLX on Linux (use Xvfb for headless operation)
// ============================================================================
#ifdef __unix__
#include <X11/Xlib.h>
#include <GL/glx.h>
#endif

#ifdef __unix__
// GLX offscreen context (pbuffer, pixmap, or hidden window fallback)
struct GLXOffscreenCtx {
    Display  *dpy;
    int       width, height;
    GLXContext ctx;
    // pbuffer approach
    GLXPbuffer   pbuffer;
    GLXFBConfig  fbconfig;
    bool         use_pbuffer;
    // pixmap fallback
    Pixmap       xpixmap;
    GLXPixmap    glxpixmap;
    XVisualInfo *vi;
    // hidden window fallback (most compatible; actual rendering uses FBOs)
    Window       dummy_win;
    Colormap     dummy_colormap;
    bool         use_window;
    // restore state
    GLXContext   prev_ctx;
    GLXDrawable  prev_draw;
    GLXDrawable  prev_read;
};

/**
 * GLX context manager for system OpenGL headless rendering.
 * Requires a running X server (real or Xvfb).
 *
 * Context creation is attempted in order of preference:
 *   1. GLX pbuffer  – direct rendering, then indirect
 *   2. GLX pixmap   – direct rendering, then indirect
 *   3. Hidden 1×1 X window (most compatible; like FLTK's Fl_Gl_Window approach)
 *      Actual pixel data is always read from FBOs, so window size is irrelevant.
 *
 * Environment overrides:
 *   OBOL_GLXGLUE_NO_PBUFFERS=1  – skip pbuffer attempt, go straight to pixmap/window.
 */
class GLXContextManager : public SoDB::ContextManager {
public:
    GLXContextManager() : m_dpy(nullptr) {}

    virtual ~GLXContextManager() {
        if (m_dpy) {
            XCloseDisplay(m_dpy);
            m_dpy = nullptr;
        }
    }

    virtual void* createOffscreenContext(unsigned int width, unsigned int height) override {
        Display *dpy = getDisplay();
        if (!dpy) return nullptr;
        int screen = DefaultScreen(dpy);

        GLXOffscreenCtx *ctx = new GLXOffscreenCtx;
        ctx->dpy        = dpy;
        ctx->width      = width;
        ctx->height     = height;
        ctx->ctx        = nullptr;
        ctx->pbuffer    = 0;
        ctx->use_pbuffer = false;
        ctx->xpixmap    = 0;
        ctx->glxpixmap  = 0;
        ctx->vi         = nullptr;
        ctx->dummy_win  = 0;
        ctx->dummy_colormap = 0;
        ctx->use_window = false;
        ctx->prev_ctx   = nullptr;
        ctx->prev_draw  = 0;
        ctx->prev_read  = 0;

        bool no_pbuffer = false;
        const char *env = getenv("OBOL_GLXGLUE_NO_PBUFFERS");
        if (env && env[0] != '0') no_pbuffer = true;

        if (!no_pbuffer) {
            int fbattribs[] = {
                GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
                GLX_RENDER_TYPE,   GLX_RGBA_BIT,
                GLX_RED_SIZE,   8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
                GLX_DEPTH_SIZE, 16,
                GLX_DOUBLEBUFFER, False,
                None
            };
            int nfb = 0;
            GLXFBConfig *fbcfgs = glXChooseFBConfig(dpy, screen, fbattribs, &nfb);
            if (fbcfgs && nfb > 0) {
                int pbattribs[] = {
                    GLX_PBUFFER_WIDTH,  (int)width,
                    GLX_PBUFFER_HEIGHT, (int)height,
                    GLX_PRESERVED_CONTENTS, False,
                    None
                };
                ctx->fbconfig = fbcfgs[0];
                ctx->pbuffer  = glXCreatePbuffer(dpy, fbcfgs[0], pbattribs);
                if (ctx->pbuffer) {
                    // Try direct rendering first; fall back to indirect
                    ctx->ctx = glXCreateNewContext(dpy, fbcfgs[0], GLX_RGBA_TYPE, nullptr, True);
                    if (!ctx->ctx)
                        ctx->ctx = glXCreateNewContext(dpy, fbcfgs[0], GLX_RGBA_TYPE, nullptr, False);
                    if (ctx->ctx) {
                        ctx->use_pbuffer = true;
                        XFree(fbcfgs);
                        return ctx;
                    }
                    glXDestroyPbuffer(dpy, ctx->pbuffer);
                    ctx->pbuffer = 0;
                }
                XFree(fbcfgs);
            }
        }

        // Choose a visual for the pixmap and window fallbacks
        int vattribs[] = {
            GLX_RGBA, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
            GLX_DEPTH_SIZE, 16, None
        };
        ctx->vi = glXChooseVisual(dpy, screen, vattribs);
        if (!ctx->vi) { delete ctx; return nullptr; }

        // Fallback 1: GLX pixmap
        ctx->xpixmap = XCreatePixmap(dpy, RootWindow(dpy, screen),
                                     width, height, ctx->vi->depth);
        if (ctx->xpixmap) {
            ctx->glxpixmap = glXCreateGLXPixmap(dpy, ctx->vi, ctx->xpixmap);
            if (ctx->glxpixmap) {
                // Try direct rendering first (modern X servers disable indirect)
                ctx->ctx = glXCreateContext(dpy, ctx->vi, nullptr, True);
                if (!ctx->ctx)
                    ctx->ctx = glXCreateContext(dpy, ctx->vi, nullptr, False);
                if (ctx->ctx)
                    return ctx;
                glXDestroyGLXPixmap(dpy, ctx->glxpixmap);
                ctx->glxpixmap = 0;
            }
            XFreePixmap(dpy, ctx->xpixmap);
            ctx->xpixmap = 0;
        }

        // Fallback 2: small hidden window (most compatible; same approach as FLTK).
        // Actual rendering always targets an FBO, so the 1×1 window size is fine.
        XSetWindowAttributes swa;
        memset(&swa, 0, sizeof(swa));
        ctx->dummy_colormap = XCreateColormap(dpy, RootWindow(dpy, screen),
                                              ctx->vi->visual, AllocNone);
        swa.colormap         = ctx->dummy_colormap;
        swa.override_redirect = True;
        ctx->dummy_win = XCreateWindow(dpy, RootWindow(dpy, screen),
                                       -1, -1, 1, 1, 0, ctx->vi->depth,
                                       InputOutput, ctx->vi->visual,
                                       CWColormap | CWOverrideRedirect, &swa);
        if (ctx->dummy_win) {
            XMapWindow(dpy, ctx->dummy_win);
            XSync(dpy, False);
            ctx->ctx = glXCreateContext(dpy, ctx->vi, nullptr, True);
            if (!ctx->ctx)
                ctx->ctx = glXCreateContext(dpy, ctx->vi, nullptr, False);
            if (ctx->ctx) {
                ctx->use_window = true;
                return ctx;
            }
            XDestroyWindow(dpy, ctx->dummy_win);
            ctx->dummy_win = 0;
        }
        XFreeColormap(dpy, ctx->dummy_colormap);
        ctx->dummy_colormap = 0;
        XFree(ctx->vi);
        ctx->vi = nullptr;
        delete ctx;
        return nullptr;
    }

    virtual SbBool makeContextCurrent(void* context) override {
        GLXOffscreenCtx *ctx = static_cast<GLXOffscreenCtx*>(context);
        if (!ctx || !ctx->ctx) return FALSE;

        ctx->prev_ctx  = glXGetCurrentContext();
        ctx->prev_draw = glXGetCurrentDrawable();
        ctx->prev_read = glXGetCurrentReadDrawable();

        Bool ok;
        if (ctx->use_pbuffer)
            ok = glXMakeCurrent(ctx->dpy, ctx->pbuffer, ctx->ctx);
        else if (ctx->use_window)
            ok = glXMakeCurrent(ctx->dpy, ctx->dummy_win, ctx->ctx);
        else
            ok = glXMakeCurrent(ctx->dpy, ctx->glxpixmap, ctx->ctx);
        return ok ? TRUE : FALSE;
    }

    virtual void restorePreviousContext(void* context) override {
        GLXOffscreenCtx *ctx = static_cast<GLXOffscreenCtx*>(context);
        if (!ctx) return;
        if (ctx->prev_ctx)
            glXMakeCurrent(ctx->dpy, ctx->prev_draw, ctx->prev_ctx);
        else
            glXMakeCurrent(ctx->dpy, None, nullptr);
    }

    virtual void destroyContext(void* context) override {
        GLXOffscreenCtx *ctx = static_cast<GLXOffscreenCtx*>(context);
        if (!ctx) return;
        glXMakeCurrent(ctx->dpy, None, nullptr);
        if (ctx->ctx) glXDestroyContext(ctx->dpy, ctx->ctx);
        if (ctx->use_pbuffer) {
            if (ctx->pbuffer) glXDestroyPbuffer(ctx->dpy, ctx->pbuffer);
        } else if (ctx->use_window) {
            if (ctx->dummy_win) {
                XUnmapWindow(ctx->dpy, ctx->dummy_win);
                XDestroyWindow(ctx->dpy, ctx->dummy_win);
            }
            if (ctx->dummy_colormap) XFreeColormap(ctx->dpy, ctx->dummy_colormap);
        } else {
            if (ctx->glxpixmap) glXDestroyGLXPixmap(ctx->dpy, ctx->glxpixmap);
            if (ctx->xpixmap)   XFreePixmap(ctx->dpy, ctx->xpixmap);
        }
        if (ctx->vi) XFree(ctx->vi);
        delete ctx;
    }

    virtual void * getProcAddress(const char * funcName) override {
        return reinterpret_cast<void*>(
            glXGetProcAddress(reinterpret_cast<const GLubyte*>(funcName)));
    }

private:
    Display *m_dpy;

    Display* getDisplay() {
        if (!m_dpy) {
            m_dpy = XOpenDisplay(nullptr);
            if (!m_dpy) {
                fprintf(stderr,
                    "GLXContextManager: Cannot open X display. "
                    "Make sure DISPLAY is set (run under Xvfb).\n");
            }
        }
        return m_dpy;
    }
};
#endif // __unix__

/**
 * Initialize Coin database for headless operation (system OpenGL backend).
 *
 * On X11 systems, a non-exiting X error handler is installed to prevent
 * spurious BadMatch errors from Mesa/llvmpipe from aborting the process.
 * A GLXContextManager is provided so SoDB::init() gets a valid context manager.
 */

namespace {
    /* Storage for the system-GL context manager set by initCoinHeadless().
       Accessed via a Meyer's singleton so the same pointer is shared between
       the setter (called once from initCoinHeadless) and all readers. */
    inline SoDB::ContextManager *& sysgl_mgr_storage() {
        static SoDB::ContextManager * ptr = nullptr;
        return ptr;
    }
    /* Setter: only called from initCoinHeadless(). */
    inline void set_sysgl_context_manager(SoDB::ContextManager * mgr) {
        sysgl_mgr_storage() = mgr;
    }
    /* Getter: returns the pointer set by initCoinHeadless(). */
    inline SoDB::ContextManager * get_sysgl_context_manager() {
        return sysgl_mgr_storage();
    }
} // anonymous namespace

inline void initCoinHeadless() {
#ifdef __unix__
    XSetErrorHandler([](Display *, XErrorEvent *err) -> int {
        fprintf(stderr, "Coin headless: X error ignored (code=%d opcode=%d/%d)\n",
                (int)err->error_code, (int)err->request_code, (int)err->minor_code);
        return 0;
    });
    static GLXContextManager glx_context_manager;
    set_sysgl_context_manager(&glx_context_manager);
    SoDB::init(&glx_context_manager);
#else
    // Non-Unix: provide a stub context manager (rendering may not work)
    class StubContextManager : public SoDB::ContextManager {
    public:
        virtual void* createOffscreenContext(unsigned int, unsigned int) override { return nullptr; }
        virtual SbBool makeContextCurrent(void*) override { return FALSE; }
        virtual void restorePreviousContext(void*) override {}
        virtual void destroyContext(void*) override {}
    };
    static StubContextManager stub;
    set_sysgl_context_manager(&stub);
    SoDB::init(&stub);
#endif
    SoNodeKit::init();
    SoInteraction::init();
}

/**
 * Return the context manager installed by initCoinHeadless() (system GL backend).
 * Must be called after initCoinHeadless().
 */
inline SoDB::ContextManager * getCoinHeadlessContextManager() {
    SoDB::ContextManager * mgr = get_sysgl_context_manager();
    assert(mgr && "getCoinHeadlessContextManager: call initCoinHeadless() first");
    return mgr;
}

/**
 * Return the single persistent offscreen renderer shared by all headless
 * examples.
 *
 * Only ONE GLX offscreen context can be successfully created per process in
 * Mesa/llvmpipe headless environments.  Sharing a single renderer object
 * across all render calls avoids this limitation.
 */
inline SoOffscreenRenderer* getSharedRenderer() {
    static SoOffscreenRenderer *s_renderer = nullptr;
    if (!s_renderer) {
        SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        s_renderer = new SoOffscreenRenderer(get_sysgl_context_manager(), vp);
    }
    return s_renderer;
}

/**
 * Render a scene to an image file (system OpenGL backend).
 * Uses the shared renderer to avoid GLX context recreation issues.
 */
inline bool renderToFile(
    SoNode *root,
    const char *filename,
    int width = DEFAULT_WIDTH,
    int height = DEFAULT_HEIGHT,
    const SbColor &backgroundColor = SbColor(0.0f, 0.0f, 0.0f))
{
    if (!root || !filename) {
        fprintf(stderr, "Error: Invalid parameters to renderToFile\n");
        return false;
    }

    SoOffscreenRenderer *renderer = getSharedRenderer();
    renderer->setComponents(SoOffscreenRenderer::RGB);
    renderer->setBackgroundColor(backgroundColor);

    if (!renderer->render(root)) {
        fprintf(stderr, "Error: Failed to render scene\n");
        return false;
    }

    if (!renderer->writeToRGB(filename)) {
        fprintf(stderr, "Error: Failed to write to RGB file %s\n", filename);
        return false;
    }

    printf("Successfully rendered to %s (%dx%d)\n", filename, width, height);
    return true;
}

#endif // OBOL_OSMESA_BUILD

/**
 * Find camera in scene graph
 */
inline SoCamera* findCamera(SoNode *root) {
    SoSearchAction search;
    search.setType(SoCamera::getClassTypeId());
    search.setInterest(SoSearchAction::FIRST);
    search.apply(root);
    
    if (search.getPath()) {
        return (SoCamera*)search.getPath()->getTail();
    }
    return NULL;
}

/**
 * Ensure scene has a camera, add one if missing
 */
inline SoCamera* ensureCamera(SoSeparator *root) {
    SoCamera *camera = findCamera(root);
    if (camera) {
        return camera;
    }
    
    SoPerspectiveCamera *newCam = new SoPerspectiveCamera;
    root->insertChild(newCam, 0);
    return newCam;
}

/**
 * Ensure scene has a light, add one if missing
 */
inline void ensureLight(SoSeparator *root) {
    SoSearchAction search;
    search.setType(SoDirectionalLight::getClassTypeId());
    search.setInterest(SoSearchAction::FIRST);
    search.apply(root);
    
    if (!search.getPath()) {
        SoDirectionalLight *light = new SoDirectionalLight;
        SoCamera *cam = findCamera(root);
        int insertPos = 0;
        if (cam) {
            for (int i = 0; i < root->getNumChildren(); i++) {
                if (root->getChild(i) == cam) {
                    insertPos = i + 1;
                    break;
                }
            }
        }
        root->insertChild(light, insertPos);
    }
}

/**
 * Setup camera to view entire scene
 */
inline void viewAll(SoNode *root, SoCamera *camera, const SbViewportRegion &viewport) {
    if (!camera) return;
    camera->viewAll(root, viewport);
}

/**
 * Orbit camera around the scene center by specified angles.
 *
 * The camera position is moved along the surface of a sphere centered at the
 * origin (the default target of viewAll()), keeping the camera pointed at the
 * center. This produces correct non-blank images for side/angle views even
 * when the scene is small relative to the camera distance.
 *
 * @param camera   Camera to reposition
 * @param azimuth  Horizontal orbit angle in radians (around world Y axis)
 * @param elevation Vertical orbit angle in radians (positive = higher vantage)
 */
inline void rotateCamera(SoCamera *camera, float azimuth, float elevation) {
    if (!camera) return;

    const SbVec3f center(0.0f, 0.0f, 0.0f);
    SbVec3f offset = camera->position.getValue() - center;

    SbRotation azimuthRot(SbVec3f(0.0f, 1.0f, 0.0f), azimuth);
    azimuthRot.multVec(offset, offset);

    SbVec3f viewDir = -offset;
    viewDir.normalize();
    SbVec3f up(0.0f, 1.0f, 0.0f);
    SbVec3f rightVec = up.cross(viewDir);
    float rLen = rightVec.length();
    if (rLen < 1e-4f) {
        rightVec = SbVec3f(1.0f, 0.0f, 0.0f);
    } else {
        rightVec *= (1.0f / rLen);
    }

    SbRotation elevationRot(rightVec, elevation);
    elevationRot.multVec(offset, offset);

    camera->position.setValue(center + offset);
    camera->pointAt(center, SbVec3f(0.0f, 1.0f, 0.0f));
}

/**
 * Simulate a mouse button press event
 */
inline void simulateMousePress(
    SoNode *root,
    const SbViewportRegion &viewport,
    int x, int y,
    SoMouseButtonEvent::Button button = SoMouseButtonEvent::BUTTON1)
{
    SoMouseButtonEvent event;
    event.setButton(button);
    event.setState(SoButtonEvent::DOWN);
    event.setPosition(SbVec2s((short)x, (short)y));
    event.setTime(SbTime::getTimeOfDay());
    
    SoHandleEventAction action(viewport);
    action.setEvent(&event);
    action.apply(root);
}

/**
 * Simulate a mouse button release event
 */
inline void simulateMouseRelease(
    SoNode *root,
    const SbViewportRegion &viewport,
    int x, int y,
    SoMouseButtonEvent::Button button = SoMouseButtonEvent::BUTTON1)
{
    SoMouseButtonEvent event;
    event.setButton(button);
    event.setState(SoButtonEvent::UP);
    event.setPosition(SbVec2s((short)x, (short)y));
    event.setTime(SbTime::getTimeOfDay());
    
    SoHandleEventAction action(viewport);
    action.setEvent(&event);
    action.apply(root);
}

/**
 * Simulate mouse motion event
 */
inline void simulateMouseMotion(
    SoNode *root,
    const SbViewportRegion &viewport,
    int x, int y)
{
    SoLocation2Event event;
    event.setPosition(SbVec2s((short)x, (short)y));
    event.setTime(SbTime::getTimeOfDay());
    
    SoHandleEventAction action(viewport);
    action.setEvent(&event);
    action.apply(root);
}

/**
 * Simulate a mouse drag gesture from start to end position
 */
inline void simulateMouseDrag(
    SoNode *root,
    const SbViewportRegion &viewport,
    int startX, int startY,
    int endX, int endY,
    int steps = 10,
    SoMouseButtonEvent::Button button = SoMouseButtonEvent::BUTTON1)
{
    simulateMousePress(root, viewport, startX, startY, button);
    
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        int x = (int)(startX + t * (endX - startX));
        int y = (int)(startY + t * (endY - startY));
        simulateMouseMotion(root, viewport, x, y);
    }
    
    simulateMouseRelease(root, viewport, endX, endY, button);
}

/**
 * Simulate a keyboard key press event
 */
inline void simulateKeyPress(
    SoNode *root,
    const SbViewportRegion &viewport,
    SoKeyboardEvent::Key key)
{
    SoKeyboardEvent event;
    event.setKey(key);
    event.setState(SoButtonEvent::DOWN);
    event.setTime(SbTime::getTimeOfDay());
    
    SoHandleEventAction action(viewport);
    action.setEvent(&event);
    action.apply(root);
}

/**
 * Simulate a keyboard key release event
 */
inline void simulateKeyRelease(
    SoNode *root,
    const SbViewportRegion &viewport,
    SoKeyboardEvent::Key key)
{
    SoKeyboardEvent event;
    event.setKey(key);
    event.setState(SoButtonEvent::UP);
    event.setTime(SbTime::getTimeOfDay());
    
    SoHandleEventAction action(viewport);
    action.setEvent(&event);
    action.apply(root);
}

#endif // HEADLESS_UTILS_H
