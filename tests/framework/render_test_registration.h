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
