#ifndef OBOL_SCENE_TEST_UTILS_H
#define OBOL_SCENE_TEST_UTILS_H

#include "render_fixture.h"

#include <Inventor/nodes/SoSeparator.h>

namespace ObolTestSupport {

/** RAII owner for the ref-counted roots returned by the scene catalogue. */
class OwnedScene {
public:
    // Scene catalogue factories transfer one existing reference to the
    // caller.  Take ownership of that reference; do not ref() it again.
    explicit OwnedScene(SoSeparator * root) : root_(root) {}

    ~OwnedScene()
    {
        if (root_) root_->unref();
    }

    OwnedScene(const OwnedScene &) = delete;
    OwnedScene & operator=(const OwnedScene &) = delete;

    OwnedScene(OwnedScene && other) noexcept : root_(other.root_)
    {
        other.root_ = nullptr;
    }

    OwnedScene & operator=(OwnedScene && other) noexcept
    {
        if (this == &other) return *this;
        if (root_) root_->unref();
        root_ = other.root_;
        other.root_ = nullptr;
        return *this;
    }

    SoSeparator * root() const { return root_; }

private:
    SoSeparator * root_ = nullptr;
};

template <typename SceneFactory>
OwnedScene makeScene(SceneFactory factory, const RenderFixture & fixture)
{
    return OwnedScene(factory(fixture.width(), fixture.height()));
}

} // namespace ObolTestSupport

#endif // OBOL_SCENE_TEST_UTILS_H
