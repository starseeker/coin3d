#include "test_context.h"

#include <gtest/gtest.h>

int main(int argc, char ** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ObolTestSupport::initializeObol();
    return RUN_ALL_TESTS();
}
