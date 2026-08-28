#include "render_fixture.h"

#include "test_context.h"

#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoNode.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <string>

#if defined(OBOL_TEST_HAVE_SYSTEM_GL) && \
    (defined(__unix__) || (defined(_WIN32) && defined(OBOL_TEST_WGL)))
#include "headless_utils.h"
#endif

namespace ObolTestSupport {

RenderFixture::RenderFixture(const int width, const int height,
                             const SbColor & background)
    : width_(width), height_(height), background_(background),
      gradient_bottom_(background), gradient_top_(background)
{
    initializeObol();

    const char * requested_backend = std::getenv("OBOL_TEST_RENDER_BACKEND");
    const bool request_system = requested_backend &&
                                std::string(requested_backend) == "system";

    if (request_system) {
#if defined(OBOL_TEST_HAVE_SYSTEM_GL) && defined(__unix__)
        XSetErrorHandler([](Display *, XErrorEvent * error) -> int {
            std::fprintf(stderr,
                         "Obol system-GL test: X error ignored "
                         "(code=%d opcode=%d/%d)\n",
                         static_cast<int>(error->error_code),
                         static_cast<int>(error->request_code),
                         static_cast<int>(error->minor_code));
            return 0;
        });
        manager_ = std::make_unique<GLXContextManager>();
        backend_name_ = "system-gl";
#elif defined(OBOL_TEST_HAVE_SYSTEM_GL) && defined(_WIN32) && defined(OBOL_TEST_WGL)
        manager_ = std::make_unique<FLTKContextManager>();
        backend_name_ = "system-gl";
#endif
    }
    else {
#if defined(OBOL_TEST_HAVE_SWRAST)
        manager_.reset(SoDB::createOSMesaContextManager());
        backend_name_ = "swrast";
#elif defined(OBOL_TEST_HAVE_SYSTEM_GL) && defined(__unix__)
        manager_ = std::make_unique<GLXContextManager>();
        backend_name_ = "system-gl";
#elif defined(OBOL_TEST_HAVE_SYSTEM_GL) && defined(_WIN32) && defined(OBOL_TEST_WGL)
        manager_ = std::make_unique<FLTKContextManager>();
        backend_name_ = "system-gl";
#endif
    }
    if (!manager_) return;

    renderer_ = std::make_unique<SoOffscreenRenderer>(
        manager_.get(), SbViewportRegion(width_, height_));
    renderer_->setComponents(SoOffscreenRenderer::RGB);
    renderer_->setBackgroundColor(background_);
}

RenderFixture::~RenderFixture() = default;

bool RenderFixture::available() const
{
    return static_cast<bool>(renderer_);
}

bool RenderFixture::render(SoNode * scene)
{
    if (!renderer_ || !scene) return false;

    renderer_->setViewportRegion(SbViewportRegion(width_, height_));
    renderer_->setComponents(SoOffscreenRenderer::RGB);
    renderer_->setBackgroundColor(background_);
    if (gradient_enabled_) {
        renderer_->setBackgroundGradient(gradient_bottom_, gradient_top_);
    } else {
        renderer_->clearBackgroundGradient();
    }
    if (!renderer_->render(scene)) return false;

    return capture();
}

SoOffscreenRenderer * RenderFixture::renderer() const
{
    return renderer_.get();
}

bool RenderFixture::capture()
{
    if (!renderer_) return false;

    const unsigned char * source = renderer_->getBuffer();
    if (!source) return false;

    const std::size_t byte_count =
        static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 3;
    pixels_.assign(source, source + byte_count);
    return true;
}

SoGLRenderAction * RenderFixture::renderAction() const
{
    return renderer_ ? renderer_->getGLRenderAction() : nullptr;
}

void RenderFixture::setBackgroundGradient(const SbColor & bottom,
                                          const SbColor & top)
{
    gradient_bottom_ = bottom;
    gradient_top_ = top;
    gradient_enabled_ = true;
}

void RenderFixture::clearBackgroundGradient()
{
    gradient_enabled_ = false;
    if (renderer_) renderer_->clearBackgroundGradient();
}

std::size_t RenderFixture::nonBackgroundPixels(const unsigned char tolerance) const
{
    const unsigned char background[3] = {
        static_cast<unsigned char>(std::lround(background_[0] * 255.0f)),
        static_cast<unsigned char>(std::lround(background_[1] * 255.0f)),
        static_cast<unsigned char>(std::lround(background_[2] * 255.0f))
    };

    std::size_t count = 0;
    for (std::size_t i = 0; i + 2 < pixels_.size(); i += 3) {
        const int red = std::abs(static_cast<int>(pixels_[i]) - background[0]);
        const int green = std::abs(static_cast<int>(pixels_[i + 1]) - background[1]);
        const int blue = std::abs(static_cast<int>(pixels_[i + 2]) - background[2]);
        if (red > tolerance || green > tolerance || blue > tolerance) ++count;
    }
    return count;
}

} // namespace ObolTestSupport
