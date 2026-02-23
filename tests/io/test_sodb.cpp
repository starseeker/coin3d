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
 * Baselined against upstream COIN_TEST_SUITE blocks.
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
#include <Inventor/SbName.h>
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

// Convenience: write a node to a freshly allocated buffer.
// Returns the actual bytes written via SoOutput::getBuffer(), not
// the allocated buffer size (g_buf_size may be larger due to realloc).
static void writeNode(SoNode* root, char** outBuf, size_t* outSize)
{
    g_buf = nullptr; g_buf_size = 0;
    SoOutput out;
    out.setBuffer(nullptr, 1, bufGrow);
    SoWriteAction wa(&out);
    wa.apply(root);
    // getBuffer() reports actual bytes written, not allocated size
    void* ptr = nullptr; size_t nbytes = 0;
    out.getBuffer(ptr, nbytes);
    *outBuf  = static_cast<char*>(ptr);
    *outSize = nbytes;
}

// Convenience: write a node in binary format to a freshly allocated buffer.
// Returns the actual bytes written via SoOutput::getBuffer().
static void writeNodeBinary(SoNode* root, char** outBuf, size_t* outSize)
{
    g_buf = nullptr; g_buf_size = 0;
    SoOutput out;
    out.setBuffer(nullptr, 1, bufGrow);
    out.setBinary(TRUE);
    SoWriteAction wa(&out);
    wa.apply(root);
    // getBuffer() reports actual bytes written, not allocated size
    void* ptr = nullptr; size_t nbytes = 0;
    out.getBuffer(ptr, nbytes);
    *outBuf  = static_cast<char*>(ptr);
    *outSize = nbytes;
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

    // -----------------------------------------------------------------------
    // SoBase write/read: name disambiguation for multiply-referenced nodes
    // Baseline: src/misc/SoBase.cpp COIN_TEST_SUITE (checkWriteWithMultiref)
    //
    // When multiple nodes share the same name and are referenced more than
    // once, the writer must append "+N" suffixes so that DEF labels are
    // unique and USE back-references work correctly.
    // -----------------------------------------------------------------------
    runner.startTest("SoBase write unnamed multi-ref node uses DEF/USE");
    {
        SoSeparator* root = new SoSeparator;
        root->ref();

        // Same unnamed node added at two places -> must be written as DEF/USE
        SoSeparator* shared = new SoSeparator;
        root->addChild(shared);
        root->addChild(shared); // second reference

        char* buf = nullptr; size_t bsz = 0;
        writeNode(root, &buf, &bsz);
        root->unref();

        bool hasDef = buf && std::strstr(buf, "DEF") != nullptr;
        bool hasUse = buf && std::strstr(buf, "USE") != nullptr;

        std::free(buf);
        bool pass = hasDef && hasUse;
        runner.endTest(pass, pass ? "" :
            "Unnamed multi-ref node should produce DEF/USE in output");
    }

    runner.startTest("SoBase write same-named multi-ref nodes disambiguates names");
    {
        // Build a scene where two distinct same-named nodes are added at
        // multiple locations, mirroring the vanilla checkWriteWithMultiref
        // test structure.  The writer must suffix duplicate names with "+N"
        // so that each DEF label is unique.
        SoSeparator* root = new SoSeparator;
        root->ref();

        SoSeparator* n1 = new SoSeparator;
        SoSeparator* n2 = new SoSeparator;
        n1->setName(SbName("MyNode"));
        n2->setName(SbName("MyNode")); // same name, different object

        // Reference n1 twice and n2 twice so both need DEF/USE treatment
        root->addChild(n1);
        root->addChild(n1); // USE n1
        root->addChild(n2);
        root->addChild(n2); // USE n2

        char* buf = nullptr; size_t bsz = 0;
        writeNode(root, &buf, &bsz);
        root->unref();

        // Both "MyNode" and a disambiguation suffix ("+") must appear
        bool hasMyNode  = buf && std::strstr(buf, "MyNode") != nullptr;
        bool hasPlus    = buf && std::strstr(buf, "+") != nullptr;
        bool hasDef     = buf && std::strstr(buf, "DEF") != nullptr;
        bool hasUse     = buf && std::strstr(buf, "USE") != nullptr;

        std::free(buf);
        bool pass = hasMyNode && hasPlus && hasDef && hasUse;
        runner.endTest(pass, pass ? "" :
            "Same-named multi-ref nodes should produce disambiguation ('+N') in output");
    }

    // -----------------------------------------------------------------------
    // Binary format I/O: write in binary, read back, verify structure
    // -----------------------------------------------------------------------
    runner.startTest("Binary format write produces non-ASCII output");
    {
        SoSeparator* root = new SoSeparator;
        root->ref();
        SoCube* cube = new SoCube;
        root->addChild(cube);

        char* buf = nullptr; size_t bsz = 0;
        writeNodeBinary(root, &buf, &bsz);
        root->unref();

        // Binary Inventor format starts with "#Inventor V2.1 binary" header.
        bool hasData   = (buf != nullptr) && (bsz > 0);
        bool hasHeader = hasData && bsz >= 21 &&
            std::memcmp(buf, "#Inventor V2.1 binary", 21) == 0;
        std::free(buf);
        bool pass = hasData && hasHeader;
        runner.endTest(pass, pass ? "" :
            "Binary write produced empty or header-less output");
    }

    runner.startTest("Binary format write/read round-trip preserves structure");
    {
        SoSeparator* root = new SoSeparator;
        root->ref();
        SoCube*   cube   = new SoCube;
        SoSphere* sphere = new SoSphere;
        root->addChild(cube);
        root->addChild(sphere);

        char* buf = nullptr; size_t bsz = 0;
        writeNodeBinary(root, &buf, &bsz);
        root->unref();

        // Verify the binary output has the binary IV header and
        // sufficient content (actual bytes written, not allocated size)
        bool hasHeader = buf && bsz > 0 &&
            std::memcmp(buf, "#Inventor V2.1 binary", 21) == 0;
        bool hasSufficientData = (bsz > 30);

        // Read back the binary buffer and verify child count is preserved
        bool readOk = false;
        if (hasHeader && hasSufficientData) {
            SoInput in;
            in.setBuffer(buf, bsz);
            SoSeparator* r2 = SoDB::readAll(&in);
            readOk = (r2 != nullptr);
            if (readOk) {
                r2->ref();
                readOk = (r2->getNumChildren() == 2);
                r2->unref();
            }
        }

        std::free(buf);
        bool pass = hasHeader && hasSufficientData && readOk;
        runner.endTest(pass, pass ? "" :
            "Binary write/read round-trip failed: header, data, or child count mismatch");
    }

    return runner.getSummary();
}
