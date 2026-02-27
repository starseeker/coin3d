/*
 * SoDBOSMesa.cpp  –  OSMesa-backed SoDB::ContextManager implementation
 *
 * This translation unit is compiled with the OSMesa header include paths
 * so it can use OSMesaCreateContextExt() and friends directly.  It exports
 * a single C function:
 *
 *   coin_create_osmesa_context_manager_impl()
 *
 * which is called by SoDB::createOSMesaContextManager() in SoDB.cpp.
 * Keeping the OSMesa types isolated here means neither SoDB.cpp nor any
 * caller of SoDB::createOSMesaContextManager() needs to include OSMesa
 * headers.
 *
 * This file is compiled in two scenarios:
 *   OBOL_OSMESA_BUILD      – the whole library uses OSMesa headers.
 *   OBOL_BUILD_DUAL_GL     – only special TUs (gl_osmesa.cpp, this file)
 *                              get the OSMesa include path.
 */

#include <Inventor/SoDB.h>
#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include <memory>

/* -----------------------------------------------------------------------
 * Per-context state
 * --------------------------------------------------------------------- */

struct CoinOSMesaCtxData {
  OSMesaContext ctx;
  std::unique_ptr<unsigned char[]> buf;
  int w, h;
  /* State saved by makeContextCurrent so restorePreviousContext can put
   * the previous OSMesa context back (NULL is a valid "no previous"). */
  OSMesaContext prev_ctx;
  void         *prev_buf;
  GLsizei       prev_w, prev_h, prev_bpr;
  GLenum        prev_fmt;

  CoinOSMesaCtxData(int w_, int h_)
    : ctx(nullptr), w(w_), h(h_),
      prev_ctx(nullptr), prev_buf(nullptr),
      prev_w(0), prev_h(0), prev_bpr(0), prev_fmt(0)
  {
    ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, nullptr);
    if (ctx)
      buf = std::make_unique<unsigned char[]>((size_t)w * h * 4);
  }

  ~CoinOSMesaCtxData() {
    if (ctx) OSMesaDestroyContext(ctx);
  }

  bool isValid() const { return ctx != nullptr; }

  bool makeCurrent() {
    if (!ctx) return false;
    prev_ctx = OSMesaGetCurrentContext();
    prev_buf = nullptr;
    prev_w = prev_h = prev_bpr = 0;
    prev_fmt = 0;
    if (prev_ctx) {
      GLint fmt = 0;
      OSMesaGetColorBuffer(prev_ctx, &prev_w, &prev_h, &fmt, &prev_buf);
      prev_fmt = (GLenum)fmt;
    }
    return OSMesaMakeCurrent(ctx, buf.get(), GL_UNSIGNED_BYTE, w, h) != 0;
  }
};

/* -----------------------------------------------------------------------
 * SoDB::ContextManager implementation backed by OSMesa
 * --------------------------------------------------------------------- */

class CoinOSMesaContextManagerImpl : public SoDB::ContextManager {
public:
  void * createOffscreenContext(unsigned int width, unsigned int height) override {
    auto * d = new CoinOSMesaCtxData((int)width, (int)height);
    return d->isValid() ? d : (delete d, nullptr);
  }

  SbBool isOSMesaContext(void * /*context*/) override {
    /* Every context this manager creates is an OSMesa context.  Returning
     * TRUE lets CoinOffscreenGLCanvas register it via
     * coingl_register_osmesa_context() so the GL dispatch layer routes to
     * the osmesa_ symbol implementations in dual-GL builds. */
    return TRUE;
  }

  void maxOffscreenDimensions(unsigned int & width, unsigned int & height) const override {
    /* OSMesa is limited only by available RAM; 16384×16384 is large enough
     * for any realistic offscreen render request. */
    width  = 16384;
    height = 16384;
  }

  SbBool makeContextCurrent(void * context) override {
    return context &&
           static_cast<CoinOSMesaCtxData *>(context)->makeCurrent()
           ? TRUE : FALSE;
  }

  void restorePreviousContext(void * context) override {
    auto * d = static_cast<CoinOSMesaCtxData *>(context);
    if (!d) return;
    if (d->prev_ctx && d->prev_buf)
      OSMesaMakeCurrent(d->prev_ctx, d->prev_buf,
                        GL_UNSIGNED_BYTE, d->prev_w, d->prev_h);
    else
      OSMesaMakeCurrent(nullptr, nullptr, 0, 0, 0);
  }

  void destroyContext(void * context) override {
    delete static_cast<CoinOSMesaCtxData *>(context);
  }
};

/* -----------------------------------------------------------------------
 * C entry point called by SoDB::createOSMesaContextManager()
 * --------------------------------------------------------------------- */

extern "C" {
SoDB::ContextManager * coin_create_osmesa_context_manager_impl()
{
  return new CoinOSMesaContextManagerImpl();
}
} /* extern "C" */
