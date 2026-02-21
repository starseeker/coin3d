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
 * @file test_sodb.cpp
 * @brief Tests for SoDB, SoInput/SoOutput, and related I/O APIs.
 *
 * Baselined against coin_vanilla COIN_TEST_SUITE blocks.
 *
 * Vanilla sources:
 *   src/misc/SoDB.cpp  - globalRealTimeField, readChildList (IV 2.1),
 *                        read round-trip tests
 *   src/misc/SoBase.cpp - write/read round-trip
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/SbTime.h>
#include <Inventor/fields/SoSFTime.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoNode.h>

#include <cstring>
#include <cstdlib>
#include <cmath>

using namespace SimpleTest;

// ---------------------------------------------------------------------------
// Growable write buffer helper
// ---------------------------------------------------------------------------
static char*  g_buf      = nullptr;
static size_t g_buf_size = 0;

static void* bufGrow(void* ptr, size_t size)
{
    g_buf      = static_cast<char*>(std::realloc(ptr, size));
    g_buf_size = size;
    return g_buf;
}

// Convenience: write a node to a freshly allocated buffer
static void writeNode(SoNode* root, char** outBuf, size_t* outSize)
{
    g_buf = nullptr; g_buf_size = 0;
    SoOutput out;
    out.setBuffer(nullptr, 0, bufGrow);
    SoWriteAction wa(&out);
    wa.apply(root);
    *outBuf  = g_buf;
    *outSize = g_buf_size;
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoDB: realTime global field is set and close to wall-clock time
    // Baseline: src/misc/SoDB.cpp COIN_TEST_SUITE (globalRealTimeField)
    // -----------------------------------------------------------------------
    runner.startTest("SoDB realTime global field initialised");
    {
        SoDB::getSensorManager()->processTimerQueue();
        SoSFTime* realtime = static_cast<SoSFTime*>(
            SoDB::getGlobalField("realTime"));
        bool pass = (realtime != nullptr) &&
                    (realtime->getContainer() != nullptr);
        if (pass) {
            double diff = std::fabs(
                SbTime::getTimeOfDay().getValue() -
                realtime->getValue().getValue());
            pass = (diff < 5.0);
        }
        runner.endTest(pass, pass ? "" :
            "SoDB realTime global field missing or not close to wall-clock");
    }

    // -----------------------------------------------------------------------
    // SoDB::readAll: read a valid Inventor 2.1 scene from buffer
    // Baseline: standard file-read pattern from vanilla tests
    // -----------------------------------------------------------------------
    runner.startTest("SoDB::readAll valid IV 2.1 scene");
    {
        static const char scene[] =
            "#Inventor V2.1 ascii\n"
            "Separator {\n"
            "  Cube {}\n"
            "  Sphere {}\n"
            "}\n";

        SoInput in;
        in.setBuffer(const_cast<char*>(scene), std::strlen(scene));
        SoSeparator* root = SoDB::readAll(&in);
        bool pass = (root != nullptr);
        if (pass) {
            root->ref();
            pass = (root->getNumChildren() == 2);
            root->unref();
        }
        runner.endTest(pass, pass ? "" :
            "SoDB::readAll failed to read valid IV 2.1 scene");
    }

    // -----------------------------------------------------------------------
    // SoDB::readAll: read IV 2.1 scene with named DEF node
    // Baseline: src/misc/SoDB.cpp readChildList / common DEF/USE pattern
    // -----------------------------------------------------------------------
    runner.startTest("SoDB::readAll DEF/USE round-trip");
    {
        static const char scene[] =
            "#Inventor V2.1 ascii\n"
            "Separator {\n"
            "  DEF MyCube Cube {}\n"
            "  USE MyCube\n"
            "}\n";

        SoInput in;
        in.setBuffer(const_cast<char*>(scene), std::strlen(scene));
        SoSeparator* root = SoDB::readAll(&in);
        bool pass = (root != nullptr);
        if (pass) {
            root->ref();
            // Two child references, both pointing at the same SoCube
            pass = (root->getNumChildren() == 2) &&
                   (root->getChild(0) == root->getChild(1));
            root->unref();
        }
        runner.endTest(pass, pass ? "" :
            "DEF/USE round-trip: expected 2 children pointing to same node");
    }

    // Note: SoDB::readAll with invalid/garbage input can trigger SoReadError::post()
    // which crashes in Obol limited-mode (context manager NULL). Deferred.

    // -----------------------------------------------------------------------
    // Write-then-read round-trip: scene structure preserved
    // Baseline: general write/read pattern from vanilla tests
    // -----------------------------------------------------------------------
    runner.startTest("SoDB write/read round-trip preserves structure");
    {
        // Build a small scene
        SoSeparator* root = new SoSeparator;
        root->ref();
        SoCube*   cube   = new SoCube;
        SoSphere* sphere = new SoSphere;
        cube->width.setValue(3.0f);
        root->addChild(cube);
        root->addChild(sphere);

        // Write to buffer
        char*  buf  = nullptr;
        size_t bsz  = 0;
        writeNode(root, &buf, &bsz);
        root->unref();

        bool pass = (buf != nullptr && bsz > 0);

        if (pass) {
            // Read back
            SoInput in;
            in.setBuffer(buf, std::strlen(buf));
            SoSeparator* r2 = SoDB::readAll(&in);
            pass = (r2 != nullptr);
            if (pass) {
                r2->ref();
                // Verify at least the child count is preserved
                pass = (r2->getNumChildren() == 2);
                // Note: checking individual field values (e.g. cube->width)
                // after round-trip is deferred; field serialization may differ
                // in limited-mode vs full context.
                r2->unref();
            }
        }

        std::free(buf);
        runner.endTest(pass, pass ? "" :
            "Write/read round-trip did not preserve scene structure");
    }

    // -----------------------------------------------------------------------
    // SoDB: getNumHeaders / isValidHeader
    // -----------------------------------------------------------------------
    runner.startTest("SoDB header recognition");
    {
        bool pass = SoDB::isValidHeader("#Inventor V2.1 ascii") &&
                    !SoDB::isValidHeader("not an inventor file");
        runner.endTest(pass, pass ? "" :
            "SoDB::isValidHeader returned unexpected results");
    }

    // Note: SoDB::readAll with invalid/garbage input can crash in the Obol
    // limited-mode (context manager is NULL). readAll tests with expected
    // failures are deferred until full context management is in place.

    return runner.getSummary();
}
