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
        void * probe = manager ? manager->createOffscreenContext(1, 1) : nullptr;
        if (!probe) {
            GTEST_SKIP() << "system OpenGL backend is unavailable; provide a "
                            "display server (for example Xvfb) to run this lane";
        }
        manager->destroyContext(probe);
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
