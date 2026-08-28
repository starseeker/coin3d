#pragma once

// Long-form render scenarios include this adapter after their scene
// implementation. A system-GL build may therefore have pulled in Xlib first.
// Its generic macros collide with identifiers in GoogleTest and are not needed
// by the registration code below.
#ifdef None
#undef None
#endif
#ifdef Bool
#undef Bool
#endif

#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace ObolTest {

// Return a build-local output stem for optional diagnostic images.  Rendering
// scenarios are ordinary GTests; this helper only keeps their artifacts away
// from the source tree and prevents system-GL/OSMesa jobs from colliding.
inline std::string renderingOutputStem(const char *name)
{
#ifdef OBOL_TEST_OUTPUT_DIR
    const std::filesystem::path directory(OBOL_TEST_OUTPUT_DIR);
#else
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "obol-render-tests";
#endif
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        throw std::runtime_error(
            "cannot create rendering-test output directory '" +
            directory.string() + "': " + error.message());
    }
    return (directory / name).string();
}

} // namespace ObolTest

// Register one independently discoverable contract from a long-form render
// source.  The expression may use outputStem.c_str(); each case receives a
// distinct artifact prefix and initializes the selected runtime GL backend.
#define OBOL_RENDER_TEST_CASE(suite, case_name, artifact_name, expression) \
    TEST(suite, case_name) {                                               \
        initCoinHeadless();                                               \
        [[maybe_unused]] const std::string outputStem =                   \
            ObolTest::renderingOutputStem(artifact_name);                 \
        const bool obolRenderContractPassed = (expression);               \
        EXPECT_TRUE(obolRenderContractPassed)                             \
            << "Render contract failed: " #expression                    \
            << "\nArtifact prefix: " << outputStem;                       \
    }
