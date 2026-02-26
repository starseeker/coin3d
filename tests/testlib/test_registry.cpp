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

#include "test_registry.h"
#include "headless_utils.h"

#include <Inventor/SbViewportRegion.h>

#include <algorithm>
#include <cstdio>
#include <set>
#include <string>

namespace ObolTest {

// -------------------------------------------------------------------------
// Category helpers
// -------------------------------------------------------------------------
std::string categoryToString(TestCategory cat)
{
    switch (cat) {
    case TestCategory::Actions:    return "Actions";
    case TestCategory::Base:       return "Base";
    case TestCategory::Bundles:    return "Bundles";
    case TestCategory::Caches:     return "Caches";
    case TestCategory::Elements:   return "Elements";
    case TestCategory::Engines:    return "Engines";
    case TestCategory::Errors:     return "Errors";
    case TestCategory::Events:     return "Events";
    case TestCategory::Fields:     return "Fields";
    case TestCategory::IO:         return "IO";
    case TestCategory::Lists:      return "Lists";
    case TestCategory::Manips:     return "Manips";
    case TestCategory::Misc:       return "Misc";
    case TestCategory::Nodes:      return "Nodes";
    case TestCategory::Projectors: return "Projectors";
    case TestCategory::Rendering:  return "Rendering";
    case TestCategory::Sensors:    return "Sensors";
    case TestCategory::Threads:    return "Threads";
    case TestCategory::Draggers:   return "Draggers";
    case TestCategory::Details:    return "Details";
    case TestCategory::Tools:      return "Tools";
    case TestCategory::Profiler:   return "Profiler";
    default:                       return "Unknown";
    }
}

TestCategory categoryFromString(const std::string& s)
{
    // Case-insensitive comparison helper
    auto iequal = [](const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
                return false;
        return true;
    };

    if (iequal(s, "actions"))    return TestCategory::Actions;
    if (iequal(s, "base"))       return TestCategory::Base;
    if (iequal(s, "bundles"))    return TestCategory::Bundles;
    if (iequal(s, "caches"))     return TestCategory::Caches;
    if (iequal(s, "elements"))   return TestCategory::Elements;
    if (iequal(s, "engines"))    return TestCategory::Engines;
    if (iequal(s, "errors"))     return TestCategory::Errors;
    if (iequal(s, "events"))     return TestCategory::Events;
    if (iequal(s, "fields"))     return TestCategory::Fields;
    if (iequal(s, "io"))         return TestCategory::IO;
    if (iequal(s, "lists"))      return TestCategory::Lists;
    if (iequal(s, "manips"))     return TestCategory::Manips;
    if (iequal(s, "misc"))       return TestCategory::Misc;
    if (iequal(s, "nodes"))      return TestCategory::Nodes;
    if (iequal(s, "projectors")) return TestCategory::Projectors;
    if (iequal(s, "rendering"))  return TestCategory::Rendering;
    if (iequal(s, "sensors"))    return TestCategory::Sensors;
    if (iequal(s, "threads"))    return TestCategory::Threads;
    if (iequal(s, "draggers"))   return TestCategory::Draggers;
    if (iequal(s, "details"))    return TestCategory::Details;
    if (iequal(s, "tools"))      return TestCategory::Tools;
    if (iequal(s, "profiler"))   return TestCategory::Profiler;
    return TestCategory::Unknown;
}

// -------------------------------------------------------------------------
// TestRegistry
// -------------------------------------------------------------------------
TestRegistry& TestRegistry::instance()
{
    static TestRegistry inst;
    return inst;
}

void TestRegistry::registerTest(const TestEntry& entry)
{
    // Silently ignore duplicate names
    for (const auto& e : tests_)
        if (e.name == entry.name) return;
    tests_.push_back(entry);
}

const TestEntry* TestRegistry::findTest(const std::string& name) const
{
    for (const auto& e : tests_)
        if (e.name == name) return &e;
    return nullptr;
}

std::vector<const TestEntry*> TestRegistry::getByCategory(TestCategory cat) const
{
    std::vector<const TestEntry*> result;
    for (const auto& e : tests_)
        if (e.category == cat) result.push_back(&e);
    return result;
}

std::vector<std::string> TestRegistry::getCategories() const
{
    std::set<std::string> seen;
    for (const auto& e : tests_)
        seen.insert(categoryToString(e.category));
    return std::vector<std::string>(seen.begin(), seen.end());
}

int TestRegistry::runAllUnitTests() const
{
    int passed = 0, failed = 0;
    for (const auto& e : tests_) {
        if (!e.run_unit) continue;
        printf("[RUN ] %s\n", e.name.c_str());
        fflush(stdout);
        int rc = e.run_unit();
        if (rc == 0) {
            printf("[PASS] %s\n", e.name.c_str());
            ++passed;
        } else {
            printf("[FAIL] %s (rc=%d)\n", e.name.c_str(), rc);
            ++failed;
        }
    }
    printf("\nResults: %d passed, %d failed (total %d)\n",
           passed, failed, passed + failed);
    return failed;
}

bool TestRegistry::renderTestToFile(const std::string& name,
                                    const std::string& outpath,
                                    int width, int height) const
{
    const TestEntry* entry = findTest(name);
    if (!entry) {
        fprintf(stderr, "renderTestToFile: test '%s' not found\n", name.c_str());
        return false;
    }
    if (!entry->create_scene) {
        fprintf(stderr, "renderTestToFile: test '%s' has no scene factory\n",
                name.c_str());
        return false;
    }

    SoSeparator* root = entry->create_scene(width, height);
    if (!root) {
        fprintf(stderr, "renderTestToFile: scene factory returned null\n");
        return false;
    }

    bool ok = renderToFile(root, outpath.c_str(), width, height);
    root->unref();
    return ok;
}

} // namespace ObolTest
