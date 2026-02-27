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

/**
 * @file test_registry.h
 * @brief Central registry for Obol tests (unit and visual).
 *
 * Tests register themselves via TestRegistry::instance().registerTest()
 * or via the REGISTER_TEST helper macro. The unified CLI runner
 * (obol_test) and the FLTK GUI viewer (obol_viewer) both consume the
 * same registry, so every test is available through both interfaces.
 */

#ifndef OBOL_TEST_REGISTRY_H
#define OBOL_TEST_REGISTRY_H

#include <string>
#include <vector>
#include <functional>

#include <Inventor/nodes/SoSeparator.h>

namespace ObolTest {

// -------------------------------------------------------------------------
// Test category enumeration
// -------------------------------------------------------------------------
enum class TestCategory {
    Actions,
    Base,
    Bundles,
    Caches,
    Elements,
    Engines,
    Errors,
    Events,
    Fields,
    IO,
    Lists,
    Manips,
    Misc,
    Nodes,
    Projectors,
    Rendering,
    Sensors,
    Threads,
    Draggers,
    Details,
    Tools,
    Profiler,
    Unknown
};

/** Return a human-readable string for a TestCategory. */
std::string categoryToString(TestCategory cat);

/** Parse a string into a TestCategory (case-insensitive). */
TestCategory categoryFromString(const std::string& s);

// -------------------------------------------------------------------------
// TestEntry: describes one registered test
// -------------------------------------------------------------------------
struct TestEntry {
    std::string   name;         /**< Unique test identifier (e.g. "colored_cube") */
    TestCategory  category;
    std::string   description;
    bool          has_visual;       /**< Can produce a rendered image */
    bool          has_interactive;  /**< Scene supports camera/event interaction */
    bool          nanort_ok;        /**< NanoRT raytracer can render this scene (false = GL-only) */

    /**
     * Unit-test runner.
     * Returns 0 on pass, non-zero on failure.
     * May be empty if the test is visual-only.
     */
    std::function<int()> run_unit;

    /**
     * Scene factory for visual / interactive use.
     * Returns a *ref'd* SoSeparator – caller must unref.
     * May be empty if the test has no visual component.
     *
     * @param width  Suggested viewport width.
     * @param height Suggested viewport height.
     */
    std::function<SoSeparator*(int width, int height)> create_scene;
};

// -------------------------------------------------------------------------
// TestRegistry: singleton holding all registered tests
// -------------------------------------------------------------------------
class TestRegistry {
public:
    /** Access the singleton instance. */
    static TestRegistry& instance();

    /** Register a test entry. */
    void registerTest(const TestEntry& entry);

    /** All registered tests in registration order. */
    const std::vector<TestEntry>& allTests() const { return tests_; }

    /** Look up a test by name; returns nullptr if not found. */
    const TestEntry* findTest(const std::string& name) const;

    /** Return all tests belonging to the given category. */
    std::vector<const TestEntry*> getByCategory(TestCategory cat) const;

    /**
     * Return the set of category strings that have at least one test,
     * in alphabetical order.
     */
    std::vector<std::string> getCategories() const;

    /**
     * Run every test that has a run_unit function.
     * Prints pass/fail information to stdout.
     * @return Number of failures (0 = all passed).
     */
    int runAllUnitTests() const;

    /**
     * Render a visual test scene to an SGI RGB file.
     *
     * @param name     Test name.
     * @param outpath  Output file path (written as-is; caller should add ".rgb").
     * @param width    Render width in pixels.
     * @param height   Render height in pixels.
     * @return true on success.
     */
    bool renderTestToFile(const std::string& name, const std::string& outpath,
                          int width = 800, int height = 600) const;

private:
    TestRegistry() = default;
    std::vector<TestEntry> tests_;
};

// -------------------------------------------------------------------------
// REGISTER_TEST helper macro
//
// Usage:
//   REGISTER_TEST(my_test, ObolTest::TestCategory::Rendering,
//                 "Short description",
//                 e.has_visual = true;
//                 e.create_scene = MySceneFactory;
//                 e.run_unit = MyUnitTestFn;);
// -------------------------------------------------------------------------
#define REGISTER_TEST(reg_name, cat, desc, ...)                          \
    static bool _obol_reg_##reg_name = ([]() -> bool {                   \
        ObolTest::TestEntry e;                                            \
        e.name        = #reg_name;                                        \
        e.category    = (cat);                                            \
        e.description = (desc);                                           \
        e.has_visual      = false;                                        \
        e.has_interactive = false;                                        \
        e.nanort_ok       = true;                                         \
        __VA_ARGS__;                                                      \
        ObolTest::TestRegistry::instance().registerTest(e);               \
        return true;                                                      \
    }(), true)

} // namespace ObolTest

#endif // OBOL_TEST_REGISTRY_H
