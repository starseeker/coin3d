#pragma once

#include <gtest/gtest.h>

namespace ObolTest {

// Keep grouped upstream-derived checks together where their existing shared
// setup is meaningful, while registering the source directly with GTest.
inline int runUpstreamCase(int (*entry)())
{
    return entry();
}

} // namespace ObolTest

