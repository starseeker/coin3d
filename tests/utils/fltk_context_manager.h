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
        /* The OpenGL framework is always present on macOS.  The static handle
         * is intentional: dlopen with RTLD_LAZY is idempotent and the
         * framework either exists at this path or it does not – retrying
         * would not help. */
        static void* lib = dlopen(
            "/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
        return lib ? dlsym(lib, funcName) : nullptr;
#else
        return reinterpret_cast<void*>(
            glXGetProcAddress(reinterpret_cast<const GLubyte*>(funcName)));
#endif
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

#endif /* FLTK_CONTEXT_MANAGER_H */
