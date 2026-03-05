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
 * QtContextManager: Cross-platform OpenGL context manager for Obol
 * that uses Qt's QOpenGLContext and QOffscreenSurface.
 *
 * Qt abstracts the platform-specific GL context setup:
 *   - WGL on Windows
 *   - CGL / NSOpenGL on macOS
 *   - GLX / EGL on Linux/X11/Wayland
 *
 * A single QOpenGLContext + QOffscreenSurface pair is created lazily on
 * the first render request and kept alive for the lifetime of the manager.
 * Offscreen rendering uses framebuffer objects (FBOs) managed by the Obol
 * library; the surface itself is never drawn to.
 *
 * Note: QApplication (or QGuiApplication) must be created before the first
 * call to initCoinHeadless() so that Qt's platform plugin is initialised.
 *
 * This header provides drop-in replacements for the same functions that
 * headless_utils.h and fltk_context_manager.h expose:
 *
 *   initCoinHeadless()              – initialise SoDB with the Qt manager
 *   getCoinHeadlessContextManager() – return the installed context manager
 *   getSharedRenderer()             – return a shared SoOffscreenRenderer
 *
 * Compile obol_qt_viewer with -DOBOL_VIEWER_QT_GL to select this header.
 */

#ifndef QT_CONTEXT_MANAGER_H
#define QT_CONTEXT_MANAGER_H

#include <Inventor/SoDB.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>

#include <QCoreApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>

/* Match default dimensions used in headless_utils.h */
#ifndef DEFAULT_WIDTH
#  define DEFAULT_WIDTH  800
#  define DEFAULT_HEIGHT 600
#endif

/* =========================================================================
 * QtOffscreenCtx  –  per-context bookkeeping
 *
 * All QtOffscreenCtx instances share the same underlying QOpenGLContext.
 * This struct records the dimensions requested when the context was created.
 * ======================================================================= */
struct QtOffscreenCtx {
    unsigned int width;
    unsigned int height;
};

/* =========================================================================
 * QtContextManager
 *
 * Implements SoDB::ContextManager using Qt's QOpenGLContext and
 * QOffscreenSurface so that context creation is handled by Qt on every
 * supported platform.
 * ======================================================================= */
class QtContextManager : public SoDB::ContextManager {
public:
    QtContextManager() : context_(nullptr), surface_(nullptr), cleaned_(false) {}

    /* Release GL resources while Qt is still alive.
     * Registered via qAddPostRoutine so it runs during QCoreApplication
     * destruction, before Qt's thread-local storage is torn down.
     * Safe to call multiple times. */
    void cleanup() {
        if (cleaned_) return;
        cleaned_ = true;
        if (context_) {
            context_->doneCurrent();
            delete context_;
            context_ = nullptr;
        }
        if (surface_) {
            surface_->destroy();
            delete surface_;
            surface_ = nullptr;
        }
    }

    virtual ~QtContextManager() {
        /* Normally cleanup() has already been called by the qAddPostRoutine
         * registered in initCoinHeadless(), while Qt is still valid.
         * If it hasn't been called yet (e.g. context manager was constructed
         * but initCoinHeadless was not used), skip Qt calls to avoid
         * crashing into already-torn-down Qt internals. */
        if (!cleaned_) {
            cleaned_ = true;
            delete context_;
            context_ = nullptr;
            delete surface_;
            surface_ = nullptr;
        }
    }

    virtual void* createOffscreenContext(unsigned int width,
                                         unsigned int height) override
    {
        if (!ensureContext()) return nullptr;
        return new QtOffscreenCtx{width, height};
    }

    virtual SbBool makeContextCurrent(void* /*context*/) override {
        if (!ensureContext()) return FALSE;
        return context_->makeCurrent(surface_) ? TRUE : FALSE;
    }

    virtual void restorePreviousContext(void* /*context*/) override {
        /* Qt does not require explicit context unbinding between FBO renders.
         * Intentionally a no-op. */
    }

    virtual void destroyContext(void* context) override {
        delete static_cast<QtOffscreenCtx*>(context);
    }

    virtual void* getProcAddress(const char* funcName) override {
        if (!ensureContext()) return nullptr;
        return reinterpret_cast<void*>(
            context_->getProcAddress(funcName));
    }

private:
    QOpenGLContext*    context_;
    QOffscreenSurface* surface_;
    bool               cleaned_;

    /* Create the QOpenGLContext + QOffscreenSurface on first use.
     * Deferred so that QApplication is fully initialised before we
     * attempt GL context creation. */
    bool ensureContext() {
        if (context_) return true;

        QSurfaceFormat fmt;
        fmt.setDepthBufferSize(24);
        fmt.setVersion(2, 1);
        fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
        fmt.setRenderableType(QSurfaceFormat::OpenGL);

        surface_ = new QOffscreenSurface();
        surface_->setFormat(fmt);
        surface_->create();

        if (!surface_->isValid()) {
            delete surface_;
            surface_ = nullptr;
            return false;
        }

        context_ = new QOpenGLContext();
        context_->setFormat(fmt);
        if (!context_->create()) {
            delete context_;
            context_ = nullptr;
            surface_->destroy();
            delete surface_;
            surface_ = nullptr;
            return false;
        }

        /* Activate once to confirm validity and trigger deferred GL init. */
        context_->makeCurrent(surface_);
        return true;
    }
};

/* =========================================================================
 * Singleton and compatibility helpers
 *
 * These provide the same interface as headless_utils.h and
 * fltk_context_manager.h so that obol_qt_viewer.cpp requires no
 * conditional logic beyond the header selection guard.
 * ======================================================================= */
namespace {
    /* Meyer's singleton: thread-safe construction (C++11). */
    inline QtContextManager& qt_context_manager_singleton() {
        static QtContextManager instance;
        return instance;
    }

    /* Plain function pointer required by qAddPostRoutine. */
    void qt_context_manager_cleanup() {
        qt_context_manager_singleton().cleanup();
    }
} // anonymous namespace

/**
 * Initialize Coin database with the Qt GL context manager.
 * Must be called after QApplication is constructed.
 */
inline void initCoinHeadless() {
    SoDB::init(&qt_context_manager_singleton());
    SoNodeKit::init();
    SoInteraction::init();
    /* Ensure GL context is released while Qt is still alive, before its
     * thread-local storage is torn down on application exit. */
    qAddPostRoutine(qt_context_manager_cleanup);
}

/**
 * Return the Qt context manager registered with SoDB.
 * Must be called after initCoinHeadless().
 */
inline SoDB::ContextManager* getCoinHeadlessContextManager() {
    return &qt_context_manager_singleton();
}

/**
 * Return the single persistent offscreen renderer shared by all callers.
 */
inline SoOffscreenRenderer* getSharedRenderer() {
    static SoOffscreenRenderer* s_renderer = nullptr;
    if (!s_renderer) {
        SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        s_renderer = new SoOffscreenRenderer(
            &qt_context_manager_singleton(), vp);
    }
    return s_renderer;
}

#endif /* QT_CONTEXT_MANAGER_H */
