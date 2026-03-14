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
 * FLTKContextManager: Cross-platform OpenGL context manager for Obol.
 *
 * On Windows and macOS, context creation is delegated to FLTK's
 * Fl_Gl_Window (WGL / CGL respectively).  A small hidden 1×1 window is
 * created lazily on the first render request.
 *
 * On X11/Linux, FLTK's GLX context creation path is intentionally bypassed.
 * FLTK uses the legacy glXChooseVisual + glXCreateContext API, which produces
 * a context whose GLX_VISUAL_ID cannot be reliably queried.  This prevents
 * the creation of properly-sharing child contexts that SoSceneTexture2 needs
 * for FBO isolation: without context isolation the outer renderer's FBO
 * binding leaks into the inner render, causing _mesa_ReadPixels crashes.
 *
 * Instead, on X11 the manager creates the GL context directly via the modern
 * glXChooseFBConfig + glXCreatePbuffer + glXCreateNewContext API, exactly
 * mirroring the approach used by the older working obol implementation.
 * Each offscreen context also gets its own small Pbuffer drawable so that
 * glXMakeCurrent() can switch cleanly between contexts.
 *
 * This header provides drop-in replacements for the same functions that
 * headless_utils.h exposes for the system-GL / GLX path:
 *
 *   initCoinHeadless()              – initialise SoDB with the FLTK manager
 *   getCoinHeadlessContextManager() – return the installed context manager
 *   getSharedRenderer()             – return a shared SoOffscreenRenderer
 *
 * Compile obol_viewer with -DOBOL_VIEWER_FLTK_GL to select this header
 * instead of headless_utils.h for the system-GL panel.
 *
 * Note: on Linux/X11 the getProcAddress() implementation still calls
 * glXGetProcAddress() directly because that is the standard GL extension
 * function loader on that platform regardless of which code created the
 * GL context.  The libGL / OpenGL::GL link target is therefore still
 * required on Linux even when using this FLTK-based manager.
 */

#ifndef FLTK_CONTEXT_MANAGER_H
#define FLTK_CONTEXT_MANAGER_H

#include <Inventor/SoDB.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>

#include <FL/Fl.H>
#include <FL/Fl_Gl_Window.H>

#include <cstdio>
#include <cstdlib>

/* Platform-specific GL function pointer loader */
#if defined(_WIN32)
#  include <windows.h>
#  include <GL/gl.h>
#elif defined(__APPLE__)
#  include <dlfcn.h>
#  include <OpenGL/gl.h>
#else
/* Linux / X11: glXGetProcAddress is always available via libGL. */
#  include <GL/glx.h>
/* FL/platform.H exposes fl_display (the X11 Display* FLTK uses) which is
 * needed for XSync() in ensureWindow(). */
#  include <FL/platform.H>
#endif

/* Match default dimensions used in headless_utils.h */
#ifndef DEFAULT_WIDTH
#  define DEFAULT_WIDTH  800
#  define DEFAULT_HEIGHT 600
#endif

/* =========================================================================
 * GL diagnostic helper
 *
 * reportGL(where) checks whether a valid OpenGL context is current by
 * calling glGetString(GL_VERSION).
 *
 * Printing policy:
 *   - Always prints for the first kAlwaysPrintCalls invocations so that
 *     GL context lifecycle problems appear in crash logs unconditionally.
 *   - Always prints when glGetString returns NULL (that is always a bug).
 *   - Prints on all subsequent calls when OBOL_GL_DIAG=1 is set.
 *
 * The output goes to stderr immediately (flushed) so it appears even if
 * the process crashes shortly after.
 *
 * Returns true if a GL context is current, false otherwise.
 * ======================================================================= */
inline bool reportGL(const char* where)
{
    static const bool diag_env = (getenv("OBOL_GL_DIAG") != nullptr);
    const GLubyte* ver = glGetString(GL_VERSION);
    if (diag_env || !ver) {
        if (ver) {
            const GLubyte* ren  = glGetString(GL_RENDERER);
            const GLubyte* vend = glGetString(GL_VENDOR);
            const GLubyte* glsl = glGetString(GL_SHADING_LANGUAGE_VERSION);
            fprintf(stderr,
                    "[GL diag] %s:\n"
                    "          GL_VERSION                  = \"%s\"\n"
                    "          GL_RENDERER                 = \"%s\"\n"
                    "          GL_VENDOR                   = \"%s\"\n"
                    "          GL_SHADING_LANGUAGE_VERSION = \"%s\"\n",
                    where,
                    (const char*)ver,
                    ren  ? (const char*)ren  : "(null)",
                    vend ? (const char*)vend : "(null)",
                    glsl ? (const char*)glsl : "(null)");
        } else {
            fprintf(stderr,
                    "[GL diag] %s:\n"
                    "          glGetString(GL_VERSION) = NULL"
                    "\n          (no current GL context!)\n",
                    where);
        }
        fflush(stderr);
    }
    return (ver != nullptr);
}

/* =========================================================================
 * FLTKGLContextWindow
 *
 * Minimal hidden GL window.  Its only purpose is to give Obol a valid
 * OpenGL context to render into FBOs.  FLTK's Fl_Gl_Window::show()
 * creates the platform GL context; make_current() activates it.
 * ======================================================================= */
class FLTKGLContextWindow : public Fl_Gl_Window {
public:
    FLTKGLContextWindow()
        : Fl_Gl_Window(1, 1, "obol_ctx")
    {
        /* Single-buffered RGBA8 context with a 24-bit depth buffer.
         * We use FBOs for all offscreen rendering and never draw to the
         * window surface, so double-buffering is unnecessary.  Using
         * FL_SINGLE avoids FLTK calling glXSwapBuffers() on this hidden
         * window which can leave stale GL errors in the queue that are
         * then mis-attributed to subsequent glUseProgramObjectARB calls
         * (manifesting as spurious GL_INVALID_OPERATION after a camera
         * drag in scenes that use SoShadowGroup). */
        mode(FL_RGB8 | FL_DEPTH | FL_SINGLE);
    }

    /* Required override; rendering targets FBOs so nothing is drawn here. */
    void draw() override {}

    /* Suppress FLTK's automatic flush/swap cycle for this hidden window.
     * We never render to the window surface; all rendering is FBO-based.
     * Without this override, FLTK's expose-event handling calls flush()
     * which calls make_current() + swap_buffers(), potentially corrupting
     * the GL context state between offscreen renders. */
    void flush() override { clear_damage(0); }
};

/* =========================================================================
 * FLTKOffscreenCtx  –  per-context bookkeeping
 *
 * On X11/GLX builds this holds a *real* shared GLXContext created with
 * glXCreateNewContext() from the FBConfig used for the main Pbuffer context.
 * Each offscreen context gets its own GLXPbuffer drawable so that
 * glXMakeCurrent() can switch into it cleanly.
 *
 * Using glXCreateNewContext() (FBConfig-based) rather than the legacy
 * glXCreateContext() (visual-based) avoids the glXQueryContext(GLX_VISUAL_ID)
 * failure that occurs when FLTK creates an FBConfig-backed core-profile
 * context, which caused all contexts to fall back to a single shared FLTK
 * window context and produced FBO-state conflicts that crashed the renderer.
 *
 * On Windows / macOS the struct degrades to a plain dimension record; the
 * single FLTK window context is reused as before (same behaviour as prior
 * commits – FBO-only paths still work correctly there).
 * ======================================================================= */
struct FLTKOffscreenCtx {
    unsigned int width;
    unsigned int height;
#if !defined(_WIN32) && !defined(__APPLE__)
    /* X11/GLX: real shared context -------------------------------------- */
    GLXContext   ctx;        /* shared child context; nullptr if creation failed */
    Display*     dpy;        /* X11 Display* (fl_display at creation time)       */
    GLXDrawable  drawable;   /* per-context Pbuffer owned by this struct         */
    /* Saved state for restorePreviousContext() */
    GLXContext   prev_ctx;
    GLXDrawable  prev_draw;
    GLXDrawable  prev_read;
#endif
};

/* =========================================================================
 * FLTKContextManager
 *
 * Implements SoDB::ContextManager using FLTK's Fl_Gl_Window so that
 * context creation is handled by FLTK on every supported platform.
 *
 * On X11/GLX, the GL context is created directly via glXChooseFBConfig +
 * glXCreatePbuffer + glXCreateNewContext rather than relying on FLTK's
 * legacy glXChooseVisual / glXCreateContext path.  Each offscreen context
 * gets its own small Pbuffer drawable so glXMakeCurrent() can switch into
 * it cleanly without needing a window.
 *
 * On Windows / macOS, a small hidden 1×1 FLTKGLContextWindow is created
 * lazily on the first render request.  All offscreen rendering is
 * FBO-based; the window surface is never drawn to directly.
 * ======================================================================= */
class FLTKContextManager : public SoDB::ContextManager {
public:
    FLTKContextManager()
        : win_(nullptr)
#if !defined(_WIN32) && !defined(__APPLE__)
        , mainFBConfig_(0)
        , mainPbuffer_(0)
        , mainGLCtx_(nullptr)
        , mainDpy_(nullptr)
#endif
    {}

    virtual ~FLTKContextManager() {
#if !defined(_WIN32) && !defined(__APPLE__)
        if (mainGLCtx_ && mainDpy_) {
            if (glXGetCurrentContext() == mainGLCtx_)
                glXMakeCurrent(mainDpy_, None, nullptr);
            glXDestroyContext(mainDpy_, mainGLCtx_);
            mainGLCtx_ = nullptr;
        }
        if (mainPbuffer_ && mainDpy_) {
            glXDestroyPbuffer(mainDpy_, mainPbuffer_);
            mainPbuffer_ = 0;
        }
#endif
        if (win_) {
            win_->hide();
            delete win_;
            win_ = nullptr;
        }
    }

    virtual void* createOffscreenContext(unsigned int width,
                                         unsigned int height) override
    {
        if (!ensureWindow()) return nullptr;
        FLTKOffscreenCtx* ctx = new FLTKOffscreenCtx;
        ctx->width  = width;
        ctx->height = height;
#if !defined(_WIN32) && !defined(__APPLE__)
        ctx->ctx      = nullptr;
        ctx->dpy      = mainDpy_;
        ctx->drawable = 0;
        ctx->prev_ctx  = nullptr;
        ctx->prev_draw = 0;
        ctx->prev_read = 0;

        /* Create a per-context Pbuffer and a new GLXContext that shares
         * display lists, textures and buffer objects with the main context.
         *
         * Using glXCreateNewContext() (FBConfig-based) is essential here.
         * The legacy glXCreateContext() requires a visual obtained via
         * glXQueryContext(GLX_VISUAL_ID), which returns GLX_BAD_ATTRIBUTE
         * on contexts created with glXCreateNewContext() or
         * glXCreateContextAttribsARB(), causing shared-context creation to
         * silently fall back to a single context and producing FBO state
         * conflicts that crash the renderer (GL_INVALID_FRAMEBUFFER_OPERATION
         * or _mesa_ReadPixels crash inside SoSceneTexture2::updatePBuffer). */
        if (mainGLCtx_ && mainDpy_ && mainFBConfig_) {
            /* A 1×1 Pbuffer is sufficient — all rendering targets FBOs; the
             * Pbuffer itself is only the surface bound to glXMakeCurrent. */
            const int pbAttribs[] = {
                GLX_PBUFFER_WIDTH,  1,
                GLX_PBUFFER_HEIGHT, 1,
                GLX_PRESERVED_CONTENTS, False,
                None
            };
            ctx->drawable = glXCreatePbuffer(mainDpy_, mainFBConfig_, pbAttribs);
            if (ctx->drawable) {
                ctx->ctx = glXCreateNewContext(mainDpy_, mainFBConfig_,
                                               GLX_RGBA_TYPE, mainGLCtx_, True);
                if (!ctx->ctx) {
                    /* Context creation failed; release the Pbuffer so that
                     * makeContextCurrent() falls back gracefully. */
                    glXDestroyPbuffer(mainDpy_, ctx->drawable);
                    ctx->drawable = 0;
                }
            }
        }
#endif
        return ctx;
    }

    virtual SbBool makeContextCurrent(void* context) override {
        if (!ensureWindow()) return FALSE;
#if !defined(_WIN32) && !defined(__APPLE__)
        FLTKOffscreenCtx* c = static_cast<FLTKOffscreenCtx*>(context);
        if (c && c->ctx && c->dpy && c->drawable) {
            /* Save the currently-active context so restorePreviousContext()
             * can switch back to it (e.g. the outer SoOffscreenRenderer's
             * shared context after updatePBuffer finishes). */
            c->prev_ctx  = glXGetCurrentContext();
            c->prev_draw = glXGetCurrentDrawable();
            c->prev_read = glXGetCurrentReadDrawable();
            if (!glXMakeCurrent(c->dpy, c->drawable, c->ctx)) {
                /* Switch failed: clear the saved state so that
                 * restorePreviousContext() does not attempt a restore
                 * against an inconsistent/unknown prior state. */
                c->prev_ctx  = nullptr;
                c->prev_draw = 0;
                c->prev_read = 0;
                return FALSE;
            }
            return glGetString(GL_VERSION) ? TRUE : FALSE;
        }
        /* Fallback (X11, shared context creation failed): use main context. */
        if (mainGLCtx_ && mainDpy_ && mainPbuffer_) {
            glXMakeCurrent(mainDpy_, mainPbuffer_, mainGLCtx_);
            return glGetString(GL_VERSION) ? TRUE : FALSE;
        }
#endif
        /* Fallback (Windows, macOS, or X11 GLX entirely unavailable):
         * reuse the single FLTK window context. */
        (void)context;
        if (!win_) return FALSE;
        win_->make_current();
        return glGetString(GL_VERSION) ? TRUE : FALSE;
    }

    virtual void restorePreviousContext(void* context) override {
#if !defined(_WIN32) && !defined(__APPLE__)
        FLTKOffscreenCtx* c = static_cast<FLTKOffscreenCtx*>(context);
        if (c && c->ctx && c->dpy) {
            if (c->prev_ctx) {
                /* Ignore glXMakeCurrent failure here: we are in a
                 * cleanup/restore path; there is no useful recovery
                 * action other than logging. */
                glXMakeCurrent(c->dpy, c->prev_draw, c->prev_ctx);
            } else {
                glXMakeCurrent(c->dpy, None, nullptr);
            }
            return;
        }
#endif
        /* Fallback: no-op — the FLTK window context stays current. */
        (void)context;
    }

    virtual void destroyContext(void* context) override {
        FLTKOffscreenCtx* c = static_cast<FLTKOffscreenCtx*>(context);
#if !defined(_WIN32) && !defined(__APPLE__)
        if (c && c->ctx && c->dpy) {
            /* Deactivate before destroying to satisfy GLX requirements. */
            if (glXGetCurrentContext() == c->ctx)
                glXMakeCurrent(c->dpy, None, nullptr);
            glXDestroyContext(c->dpy, c->ctx);
            c->ctx = nullptr;
        }
        /* Destroy the per-context Pbuffer created in createOffscreenContext(). */
        if (c && c->drawable && c->dpy) {
            glXDestroyPbuffer(c->dpy, c->drawable);
            c->drawable = 0;
        }
#endif
        delete c;
    }

    virtual void* getProcAddress(const char* funcName) override {
#if defined(_WIN32)
        void* p = reinterpret_cast<void*>(wglGetProcAddress(funcName));
        if (!p) {
            HMODULE mod = GetModuleHandleA("opengl32.dll");
            if (mod)
                p = reinterpret_cast<void*>(GetProcAddress(mod, funcName));
        }
        return p;
#elif defined(__APPLE__)
        static void* lib = dlopen(
            "/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
        return lib ? dlsym(lib, funcName) : nullptr;
#else
        return reinterpret_cast<void*>(
            glXGetProcAddress(reinterpret_cast<const GLubyte*>(funcName)));
#endif
    }

    /**
     * Report the actual dimensions of the backing surface for \a context.
     *
     * On X11/GLX: per-context Pbuffers are 1×1 (all rendering uses FBOs).
     * On Windows/macOS (FLTK fallback): the hidden 1×1 window surface.
     * The caller (SoSceneTexture2) uses this to detect when a temporary FBO
     * is needed and, if FBOs are also unavailable, to skip rendering with a
     * diagnostic warning rather than corrupting memory.
     */
    virtual void getActualSurfaceSize(void* context,
                                      unsigned int& width,
                                      unsigned int& height) const override {
#if !defined(_WIN32) && !defined(__APPLE__)
        const FLTKOffscreenCtx* c = static_cast<const FLTKOffscreenCtx*>(context);
        if (c && c->ctx && c->drawable) {
            /* X11: each offscreen context gets its own 1×1 Pbuffer.
             * All rendering is FBO-based; the Pbuffer is only needed as a
             * glXMakeCurrent() target. */
            width = 1; height = 1;
            return;
        }
        if (mainPbuffer_) {
            /* Main Pbuffer is 32×32 — large enough for probe queries but
             * still too small for direct scene-texture readback. */
            width = 32; height = 32;
            return;
        }
#endif
        /* FLTK window fallback — report actual window dimensions. */
        if (win_) {
            width  = static_cast<unsigned int>(win_->w());
            height = static_cast<unsigned int>(win_->h());
            return;
        }
        (void)context;
        width = 1; height = 1;
    }

private:
    /* Hidden 1×1 context window used on Windows / macOS (and as an X11
     * fallback when direct GLX setup fails). */
    FLTKGLContextWindow* win_;

#if !defined(_WIN32) && !defined(__APPLE__)
    /* X11/GLX: FBConfig, main Pbuffer, and main GLXContext created directly
     * (bypassing FLTK) in ensureWindow().  mainFBConfig_ is reused by
     * createOffscreenContext() to create per-context Pbuffers and shared
     * child contexts via glXCreateNewContext(). */
    GLXFBConfig mainFBConfig_;
    GLXPbuffer  mainPbuffer_;
    GLXContext  mainGLCtx_;
    Display*    mainDpy_;
#endif

    /* Ensure a valid GL context is available.
     *
     * On X11: creates the main context directly via glXChooseFBConfig +
     * glXCreatePbuffer + glXCreateNewContext, bypassing FLTK's legacy
     * glXChooseVisual / glXCreateContext path.  Falls back to a hidden
     * FLTKGLContextWindow if FBConfig setup fails.
     *
     * On Windows / macOS: creates a hidden 1×1 FLTKGLContextWindow. */
    bool ensureWindow() {
#if !defined(_WIN32) && !defined(__APPLE__)
        if (mainGLCtx_) return true;

        /* Open the X11 display if FLTK hasn't done so yet. */
        fl_open_display();
        Display* dpy = fl_display;
        if (!dpy) goto fltk_fallback;

        {
            /* Attributes for the FBConfig: Pbuffer-capable RGBA context with
             * 8-bit colour channels, 16-bit depth, single-buffered.
             * These match the attributes that were used by the older working
             * obol implementation that produced a correct apitrace. */
            const int fbAttribs[] = {
                GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
                GLX_RENDER_TYPE,   GLX_RGBA_BIT,
                GLX_RED_SIZE,   8,
                GLX_GREEN_SIZE, 8,
                GLX_BLUE_SIZE,  8,
                GLX_DEPTH_SIZE, 16,
                GLX_DOUBLEBUFFER, False,
                None
            };
            int nConfigs = 0;
            GLXFBConfig* configs = glXChooseFBConfig(
                dpy, DefaultScreen(dpy), fbAttribs, &nConfigs);
            if (!configs || nConfigs == 0) {
                if (configs) XFree(configs);
                goto fltk_fallback;
            }
            mainFBConfig_ = configs[0];
            XFree(configs);

            /* Create a small 32×32 Pbuffer as the surface for the main
             * context.  Size does not matter for FBO rendering; 32×32 is
             * chosen to match the older working probe context. */
            const int pbAttribs[] = {
                GLX_PBUFFER_WIDTH,  32,
                GLX_PBUFFER_HEIGHT, 32,
                GLX_PRESERVED_CONTENTS, False,
                None
            };
            mainPbuffer_ = glXCreatePbuffer(dpy, mainFBConfig_, pbAttribs);
            if (!mainPbuffer_) {
                mainFBConfig_ = 0;
                goto fltk_fallback;
            }

            mainGLCtx_ = glXCreateNewContext(
                dpy, mainFBConfig_, GLX_RGBA_TYPE, nullptr, True);
            if (!mainGLCtx_) {
                glXDestroyPbuffer(dpy, mainPbuffer_);
                mainPbuffer_ = 0;
                mainFBConfig_ = 0;
                goto fltk_fallback;
            }

            if (!glXMakeCurrent(dpy, mainPbuffer_, mainGLCtx_)) {
                glXDestroyContext(dpy, mainGLCtx_);
                mainGLCtx_ = nullptr;
                glXDestroyPbuffer(dpy, mainPbuffer_);
                mainPbuffer_ = 0;
                mainFBConfig_ = 0;
                goto fltk_fallback;
            }

            mainDpy_ = dpy;
            reportGL("FLTKContextManager::ensureWindow (FBConfig/Pbuffer)");
            return true;
        }

    fltk_fallback:
#endif
        /* Windows, macOS, or X11 GLX FBConfig setup failed: fall back to a
         * hidden FLTK GL window to create the platform context. */
        if (win_) return true;
        win_ = new FLTKGLContextWindow();
        win_->position(-100, -100);
        win_->show();
        /* Flush the FLTK event queue so the native GL context is fully
         * initialised before the first make_current() call. */
        Fl::check();
#if !defined(_WIN32) && !defined(__APPLE__)
        if (fl_display) XSync(fl_display, False);
#endif
        win_->make_current();
        reportGL("FLTKContextManager::ensureWindow (FLTK fallback)");
        return (bool)glGetString(GL_VERSION);
    }
};

/* =========================================================================
 * Singleton and compatibility helpers
 *
 * These provide the same interface as the headless_utils.h system-GL path
 * so that obol_viewer.cpp requires no conditional logic beyond the header
 * selection guard.
 * ======================================================================= */
namespace {
    /* Meyer's singleton: thread-safe construction (C++11), destroyed at
     * program exit after all static objects in this translation unit. */
    inline FLTKContextManager& fltk_context_manager_singleton() {
        static FLTKContextManager instance;
        return instance;
    }
} // anonymous namespace

/**
 * Initialize Coin database with the FLTK GL context manager.
 * Drop-in replacement for headless_utils.h :: initCoinHeadless() for
 * system-GL builds compiled with -DOBOL_VIEWER_FLTK_GL.
 *
 * Call this once before the FLTK event loop starts.  The actual GL
 * context window is created lazily on the first render request, so
 * Fl::visual() and the main FLTK window do not need to be set up yet.
 */
inline void initCoinHeadless() {
    SoDB::init(&fltk_context_manager_singleton());
    SoNodeKit::init();
    SoInteraction::init();
}

/**
 * Return the FLTK context manager registered with SoDB.
 * Drop-in replacement for headless_utils.h :: getCoinHeadlessContextManager().
 * Must be called after initCoinHeadless().
 */
inline SoDB::ContextManager* getCoinHeadlessContextManager() {
    return &fltk_context_manager_singleton();
}

/**
 * Return the single persistent offscreen renderer shared by all callers.
 * Drop-in replacement for headless_utils.h :: getSharedRenderer().
 */
inline SoOffscreenRenderer* getSharedRenderer() {
    static SoOffscreenRenderer* s_renderer = nullptr;
    if (!s_renderer) {
        SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        s_renderer = new SoOffscreenRenderer(
            &fltk_context_manager_singleton(), vp);
    }
    return s_renderer;
}

/* =========================================================================
 * BasicFLTKContextManager
 *
 * A deliberately minimal SoDB::ContextManager that uses only FLTK's native
 * Fl_Gl_Window path — no Pbuffers, no FBConfig, no per-context sharing.
 * All offscreen contexts share the same 1×1 hidden window context.
 *
 * PURPOSE: Test harness for "basic application-provided context" scenarios.
 * An application that builds its own window with Fl_Gl_Window and hands
 * that context to Obol gets semantics equivalent to this manager:
 *   - The backing surface is the 1×1 hidden window (reported via
 *     getActualSurfaceSize so Obol can detect the limitation).
 *   - FBO availability depends entirely on what the driver supports for
 *     the chosen visual — a compatibility-profile context may not have it.
 *   - Features requiring a proper-sized render target (SoSceneTexture2,
 *     shadow maps) will degrade gracefully and emit diagnostic warnings
 *     rather than crashing or corrupting memory.
 *
 * Use BasicFLTKContextManager in tests to verify that Obol never crashes
 * on a limited context, and that the warnings are actionable.
 * ========================================================================= */
class BasicFLTKContextManager : public SoDB::ContextManager {
public:
    struct Ctx {
        unsigned int width;
        unsigned int height;
    };

    BasicFLTKContextManager() : win_(nullptr) {}

    virtual ~BasicFLTKContextManager() {
        if (win_) { win_->hide(); Fl::check(); delete win_; win_ = nullptr; }
    }

    virtual void* createOffscreenContext(unsigned int w, unsigned int h) override {
        if (!ensureWindow()) return nullptr;
        Ctx* c = new Ctx;
        c->width  = w;
        c->height = h;
        return c;
    }

    virtual SbBool makeContextCurrent(void* /*context*/) override {
        if (!ensureWindow()) return FALSE;
        win_->make_current();
        return glGetString(GL_VERSION) ? TRUE : FALSE;
    }

    virtual void restorePreviousContext(void* /*context*/) override {
        /* Single shared context — nothing to restore. */
    }

    virtual void destroyContext(void* context) override {
        delete static_cast<Ctx*>(context);
    }

    /**
     * The actual backing surface is the 1×1 hidden window.
     * Reporting this lets SoSceneTexture2 detect it cannot do a safe
     * glReadPixels without a proper FBO, and warn instead of crashing.
     */
    virtual void getActualSurfaceSize(void* /*context*/,
                                      unsigned int& width,
                                      unsigned int& height) const override {
        if (win_) {
            width  = static_cast<unsigned int>(win_->w());
            height = static_cast<unsigned int>(win_->h());
        } else {
            width = 1; height = 1;
        }
    }

    virtual void* getProcAddress(const char* funcName) override {
#if defined(_WIN32)
        void* p = reinterpret_cast<void*>(wglGetProcAddress(funcName));
        if (!p) {
            HMODULE mod = GetModuleHandleA("opengl32.dll");
            if (mod) p = reinterpret_cast<void*>(GetProcAddress(mod, funcName));
        }
        return p;
#elif defined(__APPLE__)
        static void* lib = dlopen(
            "/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
        return lib ? dlsym(lib, funcName) : nullptr;
#else
        return reinterpret_cast<void*>(
            glXGetProcAddress(reinterpret_cast<const GLubyte*>(funcName)));
#endif
    }

    /* ------------------------------------------------------------------
     * queryCapabilities()
     *
     * Makes the GL context current and probes what it actually supports.
     * Call after the first successful render so the context exists.
     * The result is cached; subsequent calls return the same struct.
     * ------------------------------------------------------------------ */
    struct GLCapabilities {
        std::string version;        ///< GL_VERSION string
        std::string vendor;         ///< GL_VENDOR string
        std::string renderer;       ///< GL_RENDERER string
        bool        has_fbo         = false;
        bool        has_texobj      = false; ///< texture objects (GL >= 1.1)
        bool        has_vbo         = false; ///< vertex buffer objects
        bool        has_multitex    = false; ///< multitexture (GL >= 1.3)
        bool        has_3dtex       = false; ///< 3-D textures (GL >= 1.2)
        bool        has_vertex_arr  = false; ///< vertex arrays (GL >= 1.1)
        bool        has_compressed  = false; ///< compressed textures
        bool        has_glsl        = false; ///< GLSL shaders
        bool        initialized     = false;
    };

    const GLCapabilities& queryCapabilities() {
        if (caps_.initialized) return caps_;
        if (!ensureWindow()) return caps_;
        win_->make_current();

        auto glstr = [](GLenum e) -> std::string {
            const char* s = (const char*)glGetString(e);
            return s ? s : "(null)";
        };
        caps_.version  = glstr(GL_VERSION);
        caps_.vendor   = glstr(GL_VENDOR);
        caps_.renderer = glstr(GL_RENDERER);

        /* Parse major.minor from version string for quick comparisons. */
        int major = 0, minor = 0;
        sscanf(caps_.version.c_str(), "%d.%d", &major, &minor);
        const int glver = major * 100 + minor * 10;

        /* Check extensions / version for each capability. */
        auto hasExt = [&](const char* ext) -> bool {
            const char* exts = (const char*)glGetString(GL_EXTENSIONS);
            if (exts && strstr(exts, ext)) return true;
            /* Also try glGetStringi for core profiles (no GL_EXTENSIONS). */
            typedef const GLubyte* (APIENTRY* PFNGLGETSTRINGI)(GLenum,GLuint);
            PFNGLGETSTRINGI gsi = (PFNGLGETSTRINGI)getProcAddress("glGetStringi");
            if (!gsi) return false;
            GLint n = 0; glGetIntegerv(GL_NUM_EXTENSIONS, &n);
            for (GLint i = 0; i < n; ++i) {
                const char* e = (const char*)gsi(GL_EXTENSIONS, (GLuint)i);
                if (e && strcmp(e, ext) == 0) return true;
            }
            return false;
        };

        caps_.has_texobj     = (glver >= 110) || hasExt("GL_EXT_texture_object");
        caps_.has_vertex_arr = (glver >= 110);
        caps_.has_3dtex      = (glver >= 120) || hasExt("GL_EXT_texture3D");
        caps_.has_multitex   = (glver >= 130) || hasExt("GL_ARB_multitexture");
        caps_.has_compressed = (glver >= 130) || hasExt("GL_ARB_texture_compression");
        caps_.has_vbo        = (glver >= 150) || hasExt("GL_ARB_vertex_buffer_object");
        caps_.has_fbo        = (glver >= 300) || hasExt("GL_EXT_framebuffer_object")
                                               || hasExt("GL_ARB_framebuffer_object");
        caps_.has_glsl       = (glver >= 200) || hasExt("GL_ARB_shading_language_100");

        caps_.initialized = true;
        return caps_;
    }

private:
    FLTKGLContextWindow* win_;
    GLCapabilities       caps_;

    bool ensureWindow() {
        if (win_) return true;
        win_ = new FLTKGLContextWindow();
        win_->position(-200, -200);
        win_->show();
        Fl::check();
#if !defined(_WIN32) && !defined(__APPLE__)
        if (fl_display) XSync(fl_display, False);
#endif
        win_->make_current();
        return (bool)glGetString(GL_VERSION);
    }
};

#endif /* FLTK_CONTEXT_MANAGER_H */


