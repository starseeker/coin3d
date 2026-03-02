/*
 * osmesa_context_manager.h  –  reusable OSMesa SoDB::ContextManager
 *
 * Provides CoinOSMesaContextManager: a SoDB::ContextManager implementation
 * that creates offscreen GL contexts using OSMesa.  This can be included
 * independently of headless_utils.h so that individual panels in a
 * multi-backend viewer can own their own OSMesa context manager.
 *
 * Usage:
 *
 *   #include "osmesa_context_manager.h"
 *
 *   // Create a dedicated OSMesa manager per renderer:
 *   static CoinOSMesaContextManager osmesa_mgr;
 *   SoOffscreenRenderer renderer(&osmesa_mgr, viewport);
 *   renderer.render(root);
 *
 *   // Each SoOffscreenRenderer holds its own context manager reference so
 *   // multiple renderers with different backends (system GL, OSMesa, etc.)
 *   // can coexist in the same process without swapping any global state.
 *
 * Requires:
 *   - OSMesa headers in <OSMesa/osmesa.h> and <Inventor/gl.h>
 *   - Link against the OSMesa library (or a Coin built with OSMesa)
 */

#ifndef OBOL_OSMESA_CONTEXT_MANAGER_H
#define OBOL_OSMESA_CONTEXT_MANAGER_H

#include <Inventor/SoDB.h>
#include <OSMesa/osmesa.h>
#include <Inventor/gl.h>
#include <memory>

// ---------------------------------------------------------------------------
// Per-context data for one OSMesa offscreen surface
// ---------------------------------------------------------------------------

struct CoinOSMesaContextData {
    OSMesaContext context;
    std::unique_ptr<unsigned char[]> buffer;
    int width, height;
    // State saved at makeContextCurrent() time so restorePreviousContext()
    // can reinstate it.
    OSMesaContext prev_context;
    void         *prev_buffer;
    GLsizei       prev_width, prev_height, prev_bytesPerRow;
    GLenum        prev_format;

    CoinOSMesaContextData(int w, int h)
        : width(w), height(h),
          prev_context(nullptr), prev_buffer(nullptr),
          prev_width(0), prev_height(0), prev_bytesPerRow(0), prev_format(0)
    {
        context = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
        if (context)
            buffer = std::make_unique<unsigned char[]>(w * h * 4);
    }

    ~CoinOSMesaContextData() {
        if (context) OSMesaDestroyContext(context);
    }

    bool makeCurrent() {
        if (!context) return false;
        prev_context = OSMesaGetCurrentContext();
        prev_buffer  = nullptr;
        prev_width = prev_height = prev_bytesPerRow = 0;
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

// ---------------------------------------------------------------------------
// SoDB::ContextManager implementation for OSMesa
// ---------------------------------------------------------------------------

class CoinOSMesaContextManager : public SoDB::ContextManager {
public:
    virtual void* createOffscreenContext(unsigned int width, unsigned int height) override {
        auto* ctx = new CoinOSMesaContextData((int)width, (int)height);
        return ctx->isValid() ? ctx : (delete ctx, nullptr);
    }

    virtual SbBool isOSMesaContext(void * /*context*/) override {
        /* All contexts created by this manager are OSMesa contexts.
         * Returning TRUE lets CoinOffscreenGLCanvas call
         * coingl_register_osmesa_context() so GL dispatch routes to the
         * osmesa_ implementation instead of the system-GL symbols. */
        return TRUE;
    }

    virtual void maxOffscreenDimensions(unsigned int & width,
                                        unsigned int & height) const override {
        width  = 16384;
        height = 16384;
    }

    virtual SbBool makeContextCurrent(void* context) override {
        return context && static_cast<CoinOSMesaContextData*>(context)->makeCurrent()
               ? TRUE : FALSE;
    }

    virtual void restorePreviousContext(void* context) override {
        CoinOSMesaContextData* ctx = static_cast<CoinOSMesaContextData*>(context);
        if (!ctx) return;
        if (ctx->prev_context && ctx->prev_buffer) {
            OSMesaMakeCurrent(ctx->prev_context, ctx->prev_buffer,
                              GL_UNSIGNED_BYTE,
                              ctx->prev_width, ctx->prev_height);
        } else {
            OSMesaMakeCurrent(nullptr, nullptr, 0, 0, 0);
        }
    }

    virtual void destroyContext(void* context) override {
        delete static_cast<CoinOSMesaContextData*>(context);
    }
};

#endif // OBOL_OSMESA_CONTEXT_MANAGER_H
