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
 * @file obol_test.cpp
 * @brief Unified CLI test runner for the Obol test library.
 *
 * Usage:
 *   obol_test                          Run all unit tests
 *   obol_test list                     List all registered tests
 *   obol_test list --categories        List tests grouped by category
 *   obol_test run <name>               Run one test by name
 *   obol_test run <category>           Run all tests in a category
 *   obol_test render <name> <outpath>  Render visual test to file
 *   obol_test help                     Show this help
 *
 * The <outpath> for render is written as-is; the ".rgb" extension is NOT
 * appended automatically (for compatibility with renderToFile which writes
 * the exact path given).
 */

#include "testlib/test_registry.h"
#include "utils/headless_utils.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

static void printHelp(const char* argv0)
{
    printf(
        "Usage:\n"
        "  %s                             Run all unit tests\n"
        "  %s list                        List all registered tests\n"
        "  %s list --categories           List tests grouped by category\n"
        "  %s run <name|category>         Run test(s) by name or category\n"
        "  %s render <name> <outpath>     Render visual test to file\n"
        "  %s help                        Show this help\n"
        "\n"
        "Exit codes: 0 = all passed, non-zero = failures or error.\n",
        argv0, argv0, argv0, argv0, argv0, argv0);
}

static void cmdList(bool byCategory)
{
    ObolTest::TestRegistry& reg = ObolTest::TestRegistry::instance();

    if (byCategory) {
        std::vector<std::string> cats = reg.getCategories();
        for (const auto& cat : cats) {
            ObolTest::TestCategory tc = ObolTest::categoryFromString(cat);
            auto tests = reg.getByCategory(tc);
            if (tests.empty()) continue;
            printf("[%s]\n", cat.c_str());
            for (const auto* e : tests) {
                const char* flags = "";
                if (e->has_visual && e->has_interactive)      flags = " [visual,interactive]";
                else if (e->has_visual)                        flags = " [visual]";
                else if (e->run_unit)                          flags = " [unit]";
                printf("  %-30s  %s%s\n",
                       e->name.c_str(), e->description.c_str(), flags);
            }
            printf("\n");
        }
    } else {
        const auto& tests = reg.allTests();
        printf("%-30s  %-16s  %s\n", "Name", "Category", "Description");
        printf("%-30s  %-16s  %s\n",
               "------------------------------",
               "----------------",
               "------------------------------------");
        for (const auto& e : tests) {
            printf("%-30s  %-16s  %s\n",
                   e.name.c_str(),
                   ObolTest::categoryToString(e.category).c_str(),
                   e.description.c_str());
        }
        printf("\nTotal: %zu test(s)\n", tests.size());
    }
}

static int cmdRun(const std::string& target)
{
    ObolTest::TestRegistry& reg = ObolTest::TestRegistry::instance();

    // First try exact name match
    const ObolTest::TestEntry* exact = reg.findTest(target);
    if (exact) {
        if (!exact->run_unit) {
            fprintf(stderr, "Test '%s' has no unit-test function.\n", target.c_str());
            return 1;
        }
        printf("[RUN ] %s\n", exact->name.c_str());
        fflush(stdout);
        int rc = exact->run_unit();
        printf("[%s] %s\n", (rc == 0 ? "PASS" : "FAIL"), exact->name.c_str());
        return rc;
    }

    // Try category
    ObolTest::TestCategory cat = ObolTest::categoryFromString(target);
    if (cat != ObolTest::TestCategory::Unknown) {
        auto tests = reg.getByCategory(cat);
        if (tests.empty()) {
            fprintf(stderr, "No tests in category '%s'.\n", target.c_str());
            return 1;
        }
        int failures = 0;
        for (const auto* e : tests) {
            if (!e->run_unit) continue;
            printf("[RUN ] %s\n", e->name.c_str());
            fflush(stdout);
            int rc = e->run_unit();
            printf("[%s] %s\n", (rc == 0 ? "PASS" : "FAIL"), e->name.c_str());
            if (rc != 0) ++failures;
        }
        return failures;
    }

    fprintf(stderr, "Unknown test name or category: '%s'\n", target.c_str());
    return 1;
}

static int cmdRender(const std::string& name, const std::string& outpath,
                     int width, int height)
{
    bool ok = ObolTest::TestRegistry::instance().renderTestToFile(
        name, outpath, width, height);
    return ok ? 0 : 1;
}

// -------------------------------------------------------------------------
// main
// -------------------------------------------------------------------------
int main(int argc, char** argv)
{
    // Initialise Coin (OSMesa or GLX backend depending on build)
    initCoinHeadless();

    if (argc < 2 || strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0) {
        if (argc < 2) {
            // No arguments: run all unit tests
            return ObolTest::TestRegistry::instance().runAllUnitTests();
        }
        printHelp(argv[0]);
        return 0;
    }

    const std::string cmd = argv[1];

    // ---- list -----------------------------------------------------------
    if (cmd == "list") {
        bool byCat = (argc >= 3 && strcmp(argv[2], "--categories") == 0);
        cmdList(byCat);
        return 0;
    }

    // ---- run ------------------------------------------------------------
    if (cmd == "run") {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s run <name|category>\n", argv[0]);
            return 1;
        }
        return cmdRun(argv[2]);
    }

    // ---- render ---------------------------------------------------------
    if (cmd == "render") {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s render <name> <outpath> [width height]\n", argv[0]);
            return 1;
        }
        int w = (argc >= 6) ? atoi(argv[4]) : 800;
        int h = (argc >= 6) ? atoi(argv[5]) : 600;
        if (w <= 0) w = 800;
        if (h <= 0) h = 600;
        return cmdRender(argv[2], argv[3], w, h);
    }

    fprintf(stderr, "Unknown command '%s'. Run '%s help' for usage.\n",
            argv[1], argv[0]);
    return 1;
}
