#include "test_context.h"

#include <gtest/gtest.h>

#ifdef OBOL_RENDER_TEST_MAIN
#include "headless_utils.h"

#include <cstdlib>
#include <cstring>

namespace {

class RenderBackendEnvironment final : public ::testing::Environment {
public:
    void SetUp() override
    {
        const char * backend = std::getenv("OBOL_TEST_RENDER_BACKEND");
        if (!backend || std::strcmp(backend, "system") != 0) return;

        SoDB::ContextManager * manager = SoDB::getContextManager();
        if (!manager) {
            GTEST_SKIP() << "system OpenGL backend is unavailable; provide a "
                            "display server (for example Xvfb) to run this lane";
        }

        void * context = manager->createOffscreenContext(1, 1);
        if (!context) {
            GTEST_SKIP() << "system OpenGL backend is unavailable; provide a "
                            "display server (for example Xvfb) to run this lane";
        }
        manager->destroyContext(context);

        // SoOffscreenRenderer uses framebuffer objects for isolation from the
        // application drawable.  The Microsoft software WGL implementation on
        // hosted Windows runners exposes only OpenGL 1.1, so creating a context
        // alone is not enough to establish that this test suite can render.
        SoOffscreenRenderer probe(manager, SbViewportRegion(32, 32));
        if (!probe.hasFramebufferObjectSupport()) {
            GTEST_SKIP() << "system OpenGL has no framebuffer-object support; "
                            "the render suite requires OpenGL 3.0 or "
                            "GL_EXT_framebuffer_object";
        }
    }
};

} // namespace
#endif

int main(int argc, char ** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
#ifdef OBOL_RENDER_TEST_MAIN
    initCoinHeadless();
    ::testing::AddGlobalTestEnvironment(new RenderBackendEnvironment);
#else
    ObolTestSupport::initializeObol();
#endif
    return RUN_ALL_TESTS();
}
