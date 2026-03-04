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
 * FLTKContextManager: Cross-platform OpenGL context manager for Obol
 * that delegates context creation to FLTK's Fl_Gl_Window.
 *
 * FLTK abstracts the platform-specific GL context setup:
 *   - WGL on Windows
 *   - CGL / NSOpenGL on macOS
 *   - GLX on X11 / Linux
 *
 * A single hidden 1×1 Fl_Gl_Window is created lazily on the first render
 * request and kept alive for the lifetime of the manager.  Offscreen
 * rendering uses framebuffer objects (FBOs) managed by the Obol library;
 * the FLTK window surface itself is never drawn to.
 *
 * This header provides drop-in replacements for the same functions that
 * headless_utils.h exposes for the system-GL / GLX path:
 *
 *   initCoinHeadless()              – initialise SoDB with the FLTK manager
 *   getCoinHeadlessContextManager() – return the installed context manager
 *   getSharedRenderer()             – return a shared SoOffscreenRenderer
 *
 * Compile obol_viewer with -DOBOL_VIEWER_FLTK_GL to select this header
 * instead of headless_utils.h for the system-GL panel.  The GLX header
 * (included by headless_utils.h) is then not required by the viewer itself,
 * which means the viewer can build and run on any platform that FLTK
 * supports without needing Xvfb or a direct X11 connection.
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

/* Platform-specific GL function pointer loader */
#if defined(_WIN32)
#  include <windows.h>
#  include <GL/gl.h>
#elif defined(__APPLE__)
#  include <dlfcn.h>
#else
/* Linux / X11: glXGetProcAddress is always available via libGL. */
#  include <GL/glx.h>
#endif

/* Match default dimensions used in headless_utils.h */
#ifndef DEFAULT_WIDTH
#  define DEFAULT_WIDTH  800
#  define DEFAULT_HEIGHT 600
#endif

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
 * All FLTKOffscreenCtx instances share the same underlying GL context
 * (the hidden FLTKGLContextWindow).  This struct records the dimensions
 * that were requested when the context was created.
 * ======================================================================= */
struct FLTKOffscreenCtx {
    unsigned int width;
    unsigned int height;
};

/* =========================================================================
 * FLTKContextManager
 *
 * Implements SoDB::ContextManager using FLTK's Fl_Gl_Window so that
 * context creation is handled by FLTK on every supported platform.
 * ======================================================================= */
class FLTKContextManager : public SoDB::ContextManager {
public:
    FLTKContextManager() : win_(nullptr) {}

    virtual ~FLTKContextManager() {
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
        return new FLTKOffscreenCtx{width, height};
    }

    virtual SbBool makeContextCurrent(void* /*context*/) override {
        if (!ensureWindow()) return FALSE;
        win_->make_current();
        return TRUE;
    }

    virtual void restorePreviousContext(void* /*context*/) override {
        /* FLTK does not expose a context-stack API.  Offscreen FBO
         * rendering does not require unbinding the context between calls,
         * so this is intentionally a no-op. */
    }

    virtual void destroyContext(void* context) override {
        delete static_cast<FLTKOffscreenCtx*>(context);
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
    FLTKGLContextWindow* win_;

    /* Create and show the hidden GL context window on first use.
     * Deferred until the first render request so that FLTK's display
     * connection (Fl::visual / Fl_Window::show) is already established
     * before we try to create a GL context. */
    bool ensureWindow() {
        if (win_) return true;
        win_ = new FLTKGLContextWindow();
        /* Position the 1×1 window off-screen.  show() creates the native
         * window handle and the platform GL context without making the
         * window visible to the user. */
        win_->position(-100, -100);
        win_->show();
        /* Flush the FLTK event queue so the native GL context is fully
         * initialised before the first make_current() call. */
        Fl::check();
        /* Activate the context once to confirm it is valid and to
         * trigger any deferred GL initialisation inside FLTK. */
        win_->make_current();
        return true;
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
