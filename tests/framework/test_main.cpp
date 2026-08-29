#include "test_context.h"

#include <gtest/gtest.h>
#include <cstdio>

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
#if defined(_WIN32) && defined(OBOL_RENDER_TEST_MAIN)
    // CTest captures redirected stdout as fully buffered.  Keep startup and
    // the last GoogleTest case visible if a Windows graphics driver raises a
    // structured exception before normal process teardown can flush it.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
#endif
    ::testing::InitGoogleTest(&argc, argv);
#ifdef OBOL_RENDER_TEST_MAIN
#ifdef _WIN32
    const char * backend = std::getenv("OBOL_TEST_RENDER_BACKEND");
    std::fprintf(stderr, "Obol render test startup: backend=%s\n",
                 backend ? backend : "default");
    std::fprintf(stderr, "Obol render test startup: initializing Obol\n");
#endif
    initCoinHeadless();
#ifdef _WIN32
    std::fprintf(stderr, "Obol render test startup: initialization complete\n");
#endif
    ::testing::AddGlobalTestEnvironment(new RenderBackendEnvironment);
#else
    ObolTestSupport::initializeObol();
#endif
    const int result = RUN_ALL_TESTS();
    SoDB::finish();
    return result;
}
