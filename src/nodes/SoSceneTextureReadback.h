#ifndef OBOL_NODES_SOSCENETEXTUREREADBACK_H
#define OBOL_NODES_SOSCENETEXTUREREADBACK_H

#include <Inventor/SbVec2s.h>

#include <cstddef>
#include <cstdint>
#include <limits>

#include "misc/CoinTidbits.h"
#include "glue/glp.h"

namespace ObolSceneTextureInternal {

inline bool
normalizeSize(const SbVec2s & requested, SbVec2s & normalized)
{
  if (requested[0] <= 0 || requested[1] <= 0) return false;

  const std::uint32_t width =
    coin_geq_power_of_two(static_cast<std::uint32_t>(requested[0]));
  const std::uint32_t height =
    coin_geq_power_of_two(static_cast<std::uint32_t>(requested[1]));
  const std::uint32_t shortmax =
    static_cast<std::uint32_t>(std::numeric_limits<short>::max());
  if (width == 0 || height == 0 || width > shortmax || height > shortmax) {
    return false;
  }

  normalized.setValue(static_cast<short>(width), static_cast<short>(height));
  return true;
}

inline bool
rgbaByteCount(const SbVec2s & size, std::size_t faces, std::size_t & bytes)
{
  if (size[0] <= 0 || size[1] <= 0 || faces == 0) return false;
  const std::size_t width = static_cast<std::size_t>(size[0]);
  const std::size_t height = static_cast<std::size_t>(size[1]);
  const std::size_t maximum = std::numeric_limits<std::size_t>::max();
  if (width > maximum / height) return false;
  const std::size_t pixels = width * height;
  if (pixels > maximum / 4U) return false;
  const std::size_t facebytes = pixels * 4U;
  if (facebytes > maximum / faces) return false;
  bytes = facebytes * faces;
  return true;
}

class PixelPackStateGuard {
public:
  explicit PixelPackStateGuard(const SoGLContext * context)
    : glue(context)
  {
    SoGLContext_glGetIntegerv(this->glue, GL_PACK_ALIGNMENT, &this->alignment);
    SoGLContext_glGetIntegerv(this->glue, GL_PACK_ROW_LENGTH, &this->rowLength);
    SoGLContext_glGetIntegerv(this->glue, GL_PACK_SKIP_ROWS, &this->skipRows);
    SoGLContext_glGetIntegerv(this->glue, GL_PACK_SKIP_PIXELS, &this->skipPixels);
    SoGLContext_glGetIntegerv(this->glue, GL_PACK_SWAP_BYTES, &this->swapBytes);
    SoGLContext_glGetIntegerv(this->glue, GL_PACK_LSB_FIRST, &this->lsbFirst);

    // A bound pixel-pack buffer changes glReadPixels' final argument from a
    // client pointer into a byte offset. Scene-texture readback always uses
    // client memory, so temporarily unbind a PBO when one is supported. Use
    // numeric enum values to remain compatible with legacy platform headers.
    this->hasPixelPackBuffer =
      SoGLContext_has_vertex_buffer_object(this->glue) &&
      (SoGLContext_glversion_matches_at_least(this->glue, 2, 1, 0) ||
       SoGLContext_glext_supported(this->glue, "GL_ARB_pixel_buffer_object") ||
       SoGLContext_glext_supported(this->glue, "GL_EXT_pixel_buffer_object"));
    if (this->hasPixelPackBuffer) {
      SoGLContext_glGetIntegerv(this->glue, pixelPackBufferBinding,
                                &this->pixelPackBufferObject);
      SoGLContext_glBindBuffer(this->glue, pixelPackBuffer, 0);
    }

    SoGLContext_glPixelStorei(this->glue, GL_PACK_ALIGNMENT, 1);
    SoGLContext_glPixelStorei(this->glue, GL_PACK_ROW_LENGTH, 0);
    SoGLContext_glPixelStorei(this->glue, GL_PACK_SKIP_ROWS, 0);
    SoGLContext_glPixelStorei(this->glue, GL_PACK_SKIP_PIXELS, 0);
    SoGLContext_glPixelStorei(this->glue, GL_PACK_SWAP_BYTES, 0);
    SoGLContext_glPixelStorei(this->glue, GL_PACK_LSB_FIRST, 0);
  }

  ~PixelPackStateGuard()
  {
    SoGLContext_glPixelStorei(this->glue, GL_PACK_ALIGNMENT, this->alignment);
    SoGLContext_glPixelStorei(this->glue, GL_PACK_ROW_LENGTH, this->rowLength);
    SoGLContext_glPixelStorei(this->glue, GL_PACK_SKIP_ROWS, this->skipRows);
    SoGLContext_glPixelStorei(this->glue, GL_PACK_SKIP_PIXELS, this->skipPixels);
    SoGLContext_glPixelStorei(this->glue, GL_PACK_SWAP_BYTES, this->swapBytes);
    SoGLContext_glPixelStorei(this->glue, GL_PACK_LSB_FIRST, this->lsbFirst);
    if (this->hasPixelPackBuffer) {
      SoGLContext_glBindBuffer(this->glue, pixelPackBuffer,
                               static_cast<GLuint>(this->pixelPackBufferObject));
    }
  }

  PixelPackStateGuard(const PixelPackStateGuard &) = delete;
  PixelPackStateGuard & operator=(const PixelPackStateGuard &) = delete;

private:
  static constexpr GLenum pixelPackBuffer = 0x88EB;
  static constexpr GLenum pixelPackBufferBinding = 0x88ED;

  const SoGLContext * glue;
  GLint alignment = 4;
  GLint rowLength = 0;
  GLint skipRows = 0;
  GLint skipPixels = 0;
  GLint swapBytes = 0;
  GLint lsbFirst = 0;
  GLint pixelPackBufferObject = 0;
  SbBool hasPixelPackBuffer = FALSE;
};

} // namespace ObolSceneTextureInternal

#endif // OBOL_NODES_SOSCENETEXTUREREADBACK_H
