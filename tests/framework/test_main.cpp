#include "test_context.h"

#include <gtest/gtest.h>

#ifdef OBOL_RENDER_TEST_MAIN
#include "headless_utils.h"
#endif

int main(int argc, char ** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
#ifdef OBOL_RENDER_TEST_MAIN
    initCoinHeadless();
#else
    ObolTestSupport::initializeObol();
#endif
    return RUN_ALL_TESTS();
}
