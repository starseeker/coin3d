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
 *   OBOL_SWRAST_BUILD      – the whole library uses OSMesa headers.
 *   OBOL_DUAL_GL_BUILD     – only special TUs (gl_osmesa.cpp, this file)
 *                              get the OSMesa include path.
 */

#include <Inventor/SoDB.h>
#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include <cstddef>
#include <memory>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

namespace {

constexpr unsigned int maxOSMesaDimension = 16384;

/*
 * The bundled Mesa GL dispatcher starts in a single-thread fast path and
 * promotes itself to thread-specific dispatch when a second thread first
 * makes a context current.  That legacy promotion code predates the C++
 * memory model and is not safe when several threads attempt their first
 * OSMesaMakeCurrent() simultaneously.
 *
 * Perform the transition once through a deliberately serialized handoff.
 * Subsequent contexts retain Mesa's normal per-thread dispatch and may render
 * concurrently; no lock is held around rendering itself.
 */
std::recursive_mutex &
osmesaGlobalMutex()
{
  static std::recursive_mutex mutex;
  return mutex;
}

bool
prepareOSMesaThreadDispatch()
{
  static std::once_flag once;
  static bool prepared = false;
  std::call_once(once, [] {
    constexpr GLsizei width = 1;
    constexpr GLsizei height = 1;
    OSMesaContext context = OSMesaCreateContextExt(OSMESA_RGBA, 0, 0, 0, nullptr);
    std::unique_ptr<unsigned char[]> buffer(
      new (std::nothrow) unsigned char[width * height * 4]);
    if (!context || !buffer) {
      if (context) OSMesaDestroyContext(context);
      return;
    }

    OSMesaContext previous = OSMesaGetCurrentContext();
    void * previousBuffer = nullptr;
    GLsizei previousWidth = 0;
    GLsizei previousHeight = 0;
    GLint previousFormat = 0;
    GLint previousType = GL_UNSIGNED_BYTE;
    if (previous) {
      const GLboolean gotPrevious =
        OSMesaGetColorBuffer(previous, &previousWidth, &previousHeight,
                             &previousFormat, &previousBuffer);
      if (gotPrevious != GL_TRUE || !previousBuffer ||
          previousWidth <= 0 || previousHeight <= 0) {
        OSMesaDestroyContext(context);
        return;
      }
      OSMesaGetIntegerv(OSMESA_TYPE, &previousType);
    }

    const bool firstBound =
      OSMesaMakeCurrent(context, buffer.get(), GL_UNSIGNED_BYTE, width, height) == GL_TRUE;
    const bool restored = previous && previousBuffer
      ? OSMesaMakeCurrent(previous, previousBuffer,
                          static_cast<GLenum>(previousType),
                          previousWidth, previousHeight) == GL_TRUE
      : OSMesaMakeCurrent(nullptr, nullptr, 0, 0, 0) == GL_TRUE;

    if (firstBound && restored) {
      bool secondBound = false;
      try {
        std::thread secondThread([&] {
          if (OSMesaMakeCurrent(context, buffer.get(), GL_UNSIGNED_BYTE,
                                width, height) == GL_TRUE) {
            secondBound = true;
            (void)OSMesaMakeCurrent(nullptr, nullptr, 0, 0, 0);
          }
        });
        secondThread.join();
      }
      catch (...) {
        /* The manager will use its serialized fallback if a helper thread
         * cannot be created.  Context creation remains usable in
         * resource-constrained applications without exposing the race. */
      }
      prepared = secondBound;
    }
    OSMesaDestroyContext(context);
  });
  return prepared;
}

} // namespace

/* -----------------------------------------------------------------------
 * Per-context state
 * --------------------------------------------------------------------- */

struct CoinOSMesaCtxData {
  struct PreviousContext {
    OSMesaContext ctx;
    void *buf;
    GLsizei w;
    GLsizei h;
    GLenum type;

    PreviousContext()
      : ctx(nullptr), buf(nullptr), w(0), h(0), type(GL_UNSIGNED_BYTE)
    {
    }
  };

  OSMesaContext ctx;
  std::unique_ptr<unsigned char[]> buf;
  int w, h;
  /* Context activation can nest while Coin releases context-bound caches.
   * Preserve every activation frame so a nested make/restore pair cannot
   * overwrite the state needed by its caller. */
  std::vector<PreviousContext> previous;

  CoinOSMesaCtxData(int w_, int h_)
    : ctx(nullptr), w(w_), h(h_)
  {
    ctx = OSMesaCreateContextExt(OSMESA_RGBA, 24, 0, 0, nullptr);
    if (ctx) {
      buf.reset(new (std::nothrow) unsigned char[
        static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4U]);
      if (!buf) {
        OSMesaDestroyContext(ctx);
        ctx = nullptr;
      }
    }
  }

  ~CoinOSMesaCtxData() {
    if (ctx) {
      OSMesaDestroyContext(ctx);
      ctx = nullptr;  /* defensive: zero out so any stale ptr check in this TU is safe */
    }
  }

  bool isValid() const { return ctx != nullptr; }

  bool makeCurrent() {
    if (!ctx) return false;
    PreviousContext frame;
    frame.ctx = OSMesaGetCurrentContext();
    if (frame.ctx) {
      GLint fmt = 0;
      GLint type = GL_UNSIGNED_BYTE;
      OSMesaGetColorBuffer(frame.ctx, &frame.w, &frame.h, &fmt, &frame.buf);
      OSMesaGetIntegerv(OSMESA_TYPE, &type);
      frame.type = static_cast<GLenum>(type);
    }
    try {
      previous.push_back(frame);
    }
    catch (...) {
      return false;
    }
    if (!OSMesaMakeCurrent(ctx, buf.get(), GL_UNSIGNED_BYTE, w, h)) {
      previous.pop_back();
      return false;
    }
    return true;
  }
};

/* -----------------------------------------------------------------------
 * SoDB::ContextManager implementation backed by OSMesa
 * --------------------------------------------------------------------- */

class CoinOSMesaContextManagerImpl : public SoDB::ContextManager {
public:
  CoinOSMesaContextManagerImpl()
    : concurrentDispatch(prepareOSMesaThreadDispatch())
  {
  }

  void * createOffscreenContext(unsigned int width, unsigned int height) override {
    if (width == 0 || height == 0 ||
        width > maxOSMesaDimension || height > maxOSMesaDimension) {
      return nullptr;
    }
    /* This bundled Mesa predates reliable concurrent context lifetime
     * operations.  Context creation also reaches process-global one-time
     * tables and allocator state.  Serialize creation while allowing the
     * resulting contexts to render concurrently. */
    const std::lock_guard<std::recursive_mutex> lock(osmesaGlobalMutex());
    auto * d = new (std::nothrow) CoinOSMesaCtxData(
      static_cast<int>(width), static_cast<int>(height));
    return d && d->isValid() ? d : (delete d, nullptr);
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
    width = maxOSMesaDimension;
    height = maxOSMesaDimension;
  }

  void getActualSurfaceSize(void * context,
                            unsigned int & width,
                            unsigned int & height) const override {
    const auto * data = static_cast<const CoinOSMesaCtxData *>(context);
    if (!data || data->w <= 0 || data->h <= 0) {
      width = height = 0;
      return;
    }
    width = static_cast<unsigned int>(data->w);
    height = static_cast<unsigned int>(data->h);
  }

  SbBool makeContextCurrent(void * context) override {
    if (!this->concurrentDispatch) osmesaGlobalMutex().lock();
    const bool current = context &&
      static_cast<CoinOSMesaCtxData *>(context)->makeCurrent();
    if (!current && !this->concurrentDispatch) osmesaGlobalMutex().unlock();
    return current ? TRUE : FALSE;
  }

  void restorePreviousContext(void * context) override {
    auto * d = static_cast<CoinOSMesaCtxData *>(context);
    bool restored = false;
    if (d && !d->previous.empty()) {
      const CoinOSMesaCtxData::PreviousContext frame = d->previous.back();
      d->previous.pop_back();
      if (frame.ctx && frame.buf)
        OSMesaMakeCurrent(frame.ctx, frame.buf,
                          frame.type, frame.w, frame.h);
      else
        OSMesaMakeCurrent(nullptr, nullptr, 0, 0, 0);
      restored = true;
    }
    if (!this->concurrentDispatch && restored) osmesaGlobalMutex().unlock();
  }

  void destroyContext(void * context) override {
    /* Match creation serialization: old Mesa context destruction touches
     * shared tables even when the contexts do not share GL objects. */
    const std::lock_guard<std::recursive_mutex> lock(osmesaGlobalMutex());
    auto * d = static_cast<CoinOSMesaCtxData *>(context);
    if (!d) return;
    /* Unbind the context before destroying it.  If it is still current (e.g.
     * because restorePreviousContext could not unbind it on an earlier call),
     * OSMesaDestroyContext on a live current context corrupts internal Mesa
     * state.  After Fix-1 in osmesa.c, OSMesaMakeCurrent(NULL,NULL,0,0,0)
     * now properly calls _mesa_make_current(NULL,NULL,NULL). */
    if (d->ctx && OSMesaGetCurrentContext() == d->ctx)
      OSMesaMakeCurrent(nullptr, nullptr, 0, 0, 0);
    delete d;
  }

  void * getProcAddress(const char * funcName) override {
    /* Route all function-pointer lookups through OSMesaGetProcAddress so
     * that OSMesa contexts always get OSMesa function pointers, never
     * accidentally picking up system GL symbols from the process handle. */
    return reinterpret_cast<void*>(OSMesaGetProcAddress(funcName));
  }

  SbBool getCurrentSoftwareFramebuffer(unsigned char *& pixels,
                                       unsigned int & width,
                                       unsigned int & height,
                                       unsigned int & components) override {
    OSMesaContext current = OSMesaGetCurrentContext();
    GLsizei w = 0, h = 0;
    GLint format = 0;
    void *buffer = nullptr;
    const GLboolean gotBuffer = current ?
      OSMesaGetColorBuffer(current, &w, &h, &format, &buffer) : GL_FALSE;
    if (!current || !gotBuffer || !buffer || w <= 0 || h <= 0 ||
        format != OSMESA_RGBA) {
      pixels = nullptr;
      width = height = components = 0;
      return FALSE;
    }
    pixels = static_cast<unsigned char *>(buffer);
    width = static_cast<unsigned int>(w);
    height = static_cast<unsigned int>(h);
    components = 4;
    return TRUE;
  }

private:
  const bool concurrentDispatch;
};

/* -----------------------------------------------------------------------
 * C entry point called by SoDB::createOSMesaContextManager()
 * --------------------------------------------------------------------- */

extern "C" {
SoDB::ContextManager * coin_create_osmesa_context_manager_impl();
SoDB::ContextManager * coin_create_osmesa_context_manager_impl()
{
  return new CoinOSMesaContextManagerImpl();
}
} /* extern "C" */
