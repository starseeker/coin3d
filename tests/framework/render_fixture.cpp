#include "render_fixture.h"

#include "test_context.h"

#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoNode.h>

#include <algorithm>
#include <cmath>

namespace ObolTestSupport {

RenderFixture::RenderFixture(const int width, const int height,
                             const SbColor & background)
    : width_(width), height_(height), background_(background)
{
    initializeObol();
    manager_.reset(SoDB::createOSMesaContextManager());
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
    if (!renderer_->render(scene)) return false;

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
