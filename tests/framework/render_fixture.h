#ifndef OBOL_RENDER_FIXTURE_H
#define OBOL_RENDER_FIXTURE_H

#include <Inventor/SbColor.h>
#include <Inventor/SoDB.h>

#include <cstddef>
#include <memory>
#include <vector>

class SoNode;
class SoGLRenderAction;
class SoOffscreenRenderer;

namespace ObolTestSupport {

/**
 * Per-test OSMesa renderer.
 *
 * A fixture owns both its context manager and renderer.  It never mutates the
 * global context manager, so independent tests cannot leak renderer state into
 * each other.
 */
class RenderFixture {
public:
    RenderFixture(int width, int height,
                  const SbColor & background = SbColor(0.0f, 0.0f, 0.0f));
    ~RenderFixture();

    RenderFixture(const RenderFixture &) = delete;
    RenderFixture & operator=(const RenderFixture &) = delete;

    bool available() const;
    bool render(SoNode * scene);
    SoGLRenderAction * renderAction() const;

    int width() const { return width_; }
    int height() const { return height_; }
    const std::vector<unsigned char> & pixels() const { return pixels_; }

    /** Number of pixels that differ from the configured background. */
    std::size_t nonBackgroundPixels(unsigned char tolerance = 2) const;

private:
    int width_;
    int height_;
    SbColor background_;
    std::unique_ptr<SoDB::ContextManager> manager_;
    std::unique_ptr<SoOffscreenRenderer> renderer_;
    std::vector<unsigned char> pixels_;
};

} // namespace ObolTestSupport

#endif // OBOL_RENDER_FIXTURE_H
