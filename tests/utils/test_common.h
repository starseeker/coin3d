#ifndef TEST_COMMON_H
#define TEST_COMMON_H

/**************************************************************************\
* Copyright (c) Kongsberg Oil & Gas Technologies AS
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
* Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* Neither the name of the copyright holder nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

#include <catch2/catch_test_macros.hpp>
#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <stdexcept>

// Forward declarations for comprehensive testing utilities
namespace CoinTestUtils {
    class OSMesaTestContext;
    class OSMesaTestFixture;
    class StandardTestScenes;
    class SceneGraphValidator;
    class RenderingTestUtils;
    class ActionTestUtils;
    class FieldTestUtils;
    class ComprehensiveTestRunner;
}

namespace CoinTestUtils {

// Test fixture for Coin initialization - simplified to avoid issues
class CoinTestFixture {
public:
    CoinTestFixture() {
        // Do nothing - global setup handles everything
    }

    ~CoinTestFixture() {
        // Don't cleanup here as other tests may need Coin
    }
};

// Helper macros to replace Boost.Test equivalents  
#define COIN_CHECK(condition) CHECK(condition)
#define CHECK_WITH_MESSAGE(condition, message) INFO(message); CHECK(condition)
#define COIN_CHECK_MESSAGE(condition, message) INFO(message); CHECK(condition)
#define COIN_CHECK_EQUAL(left, right) CHECK((left) == (right))
#define COIN_REQUIRE(condition) REQUIRE(condition)

// Comprehensive testing macros
#define COIN_TEST_SCENE(scene_name) \
    SECTION(#scene_name " scene validation") { \
        auto scene = StandardTestScenes::create##scene_name##Scene(); \
        REQUIRE(scene != nullptr); \
        CHECK(SceneGraphValidator::validateSceneStructure(scene)); \
        scene->unref(); \
    }

#define COIN_RENDER_TEST(scene_name, test_name) \
    SECTION(#scene_name " " #test_name) { \
        COIN_TEST_WITH_OSMESA_CONTEXT(256, 256) { \
            auto scene = StandardTestScenes::create##scene_name##Scene(); \
            REQUIRE(scene != nullptr); \
            RenderingTestUtils::RenderTestFixture fixture(256, 256); \
            CHECK(fixture.renderScene(scene)); \
            CHECK(RenderingTestUtils::validateRenderOutput(fixture)); \
            scene->unref(); \
        } \
    }

#ifdef COIN3D_OSMESA_BUILD
#define COIN_TEST_WITH_OSMESA_CONTEXT(width, height) \
    if (true)
#else
#define COIN_TEST_WITH_OSMESA_CONTEXT(width, height) \
    if (false)
#endif

} // namespace CoinTestUtils

#endif // TEST_COMMON_H