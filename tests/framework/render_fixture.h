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
 * Per-test renderer for the backend selected by OBOL_TEST_RENDER_BACKEND.
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
    /** The fixture-owned renderer for APIs such as SoViewport::render(). */
    SoOffscreenRenderer * renderer() const;
    /** Copy the most recent renderer buffer into pixels(). */
    bool capture();
    SoGLRenderAction * renderAction() const;

    /** Configure the renderer's vertical background gradient for later renders. */
    void setBackgroundGradient(const SbColor & bottom, const SbColor & top);
    void clearBackgroundGradient();

    int width() const { return width_; }
    int height() const { return height_; }
    const char * backendName() const { return backend_name_; }
    const std::vector<unsigned char> & pixels() const { return pixels_; }

    /** Number of pixels that differ from the configured background. */
    std::size_t nonBackgroundPixels(unsigned char tolerance = 2) const;

private:
    int width_;
    int height_;
    SbColor background_;
    SbColor gradient_bottom_;
    SbColor gradient_top_;
    bool gradient_enabled_ = false;
    const char * backend_name_ = "unavailable";
    std::unique_ptr<SoDB::ContextManager> manager_;
    std::unique_ptr<SoOffscreenRenderer> renderer_;
    std::vector<unsigned char> pixels_;
};

} // namespace ObolTestSupport

#endif // OBOL_RENDER_FIXTURE_H
