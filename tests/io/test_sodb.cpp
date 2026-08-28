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
 * Baselined against upstream OBOL_TEST_SUITE blocks.
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
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace ObolTest;

namespace {

struct ProgressEvent {
    std::string item;
    float fraction;
    bool interruptible;
};

struct ProgressState {
    std::mutex mutex;
    std::vector<ProgressEvent> events;
    bool abortAtIntermediate = false;
};

SbBool recordProgress(const SbName & item, float fraction,
                      SbBool interruptible, void * userdata)
{
    ProgressState * state = static_cast<ProgressState *>(userdata);
    {
        const std::lock_guard<std::mutex> lock(state->mutex);
        state->events.push_back(
            {item.getString(), fraction, interruptible != FALSE});
    }
    return state->abortAtIntermediate && interruptible &&
           fraction > 0.0f && fraction < 1.0f;
}

SbBool removeSelfProgress(const SbName &, float, SbBool, void * userdata)
{
    std::atomic<int> * calls = static_cast<std::atomic<int> *>(userdata);
    calls->fetch_add(1, std::memory_order_relaxed);
    SoDB::removeProgressCallback(removeSelfProgress, userdata);
    return FALSE;
}

SbBool noOpProgress(const SbName &, float, SbBool, void *)
{
    return FALSE;
}

const char progressScene[] =
    "#Inventor V2.1 ascii\n"
    "Cube {}\n"
    "Sphere {}\n";

SoSeparator * readProgressScene()
{
    SoInput input;
    input.setBuffer(const_cast<char *>(progressScene),
                    std::strlen(progressScene));
    return SoDB::readAll(&input);
}

} // namespace

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

TEST(IoSodb, SoDBRealTimeGlobalFieldInitialised)
{
    SoDB::getSensorManager()->processTimerQueue();
    SoSFTime* realtime = static_cast<SoSFTime*>(
        SoDB::getGlobalField("realTime"));
    EXPECT_NE(realtime, nullptr);
    if (realtime != nullptr) {
        EXPECT_NE(realtime->getContainer(), nullptr);
        double diff = std::fabs(
            SbTime::getTimeOfDay().getValue() -
            realtime->getValue().getValue());
        EXPECT_LT(diff, 5.0);
    }
}

// -----------------------------------------------------------------------
// SoDB::readAll: read a valid Inventor 2.1 scene from buffer
// Baseline: standard file-read pattern from vanilla tests
// -----------------------------------------------------------------------

TEST(IoSodb, SoDBReadAllValidIV21Scene)
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
    EXPECT_NE(root, nullptr);
    if (root != nullptr) {
        root->ref();
        EXPECT_EQ(root->getNumChildren(), 2);
        root->unref();
    }
}

// -----------------------------------------------------------------------
// SoDB::readAll: read IV 2.1 scene with named DEF node
// Baseline: src/misc/SoDB.cpp readChildList / common DEF/USE pattern
// -----------------------------------------------------------------------

TEST(IoSodb, SoDBReadAllDEFUSERoundTrip)
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
    EXPECT_NE(root, nullptr);
    if (root != nullptr) {
        root->ref();
        // Two child references, both pointing at the same SoCube
        EXPECT_EQ(root->getNumChildren(), 2);
        if (root->getNumChildren() == 2) {
            EXPECT_EQ(root->getChild(0), root->getChild(1));
        }
        root->unref();
    }
}

// Note: SoDB::readAll with invalid/garbage input can trigger SoReadError::post()
// which crashes in Obol limited-mode (context manager NULL). Deferred.

// -----------------------------------------------------------------------
// Write-then-read round-trip: scene structure preserved
// Baseline: general write/read pattern from vanilla tests
// -----------------------------------------------------------------------

TEST(IoSodb, SoDBWriteReadRoundTripPreservesStructure)
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

    EXPECT_NE(buf, nullptr);
    EXPECT_GT(bsz, 0u);
    if (buf != nullptr && bsz > 0) {
        // Read back
        SoInput in;
        in.setBuffer(buf, std::strlen(buf));
        SoSeparator* r2 = SoDB::readAll(&in);
        EXPECT_NE(r2, nullptr);
        if (r2 != nullptr) {
            r2->ref();
            // Verify at least the child count is preserved
            EXPECT_EQ(r2->getNumChildren(), 2);
            // Note: checking individual field values (e.g. cube->width)
            // after round-trip is deferred; field serialization may differ
            // in limited-mode vs full context.
            r2->unref();
        }
    }

    std::free(buf);
}

// -----------------------------------------------------------------------
// SoDB: getNumHeaders / isValidHeader
// -----------------------------------------------------------------------

TEST(IoSodb, SoDBHeaderRecognition)
{
    EXPECT_TRUE(SoDB::isValidHeader("#Inventor V2.1 ascii") &&
                !SoDB::isValidHeader("not an inventor file")) << "SoDB::isValidHeader returned unexpected results";
}

TEST(IoSodb, ReadAllReportsMonotonicProgressAndExactCompletion)
{
    ProgressState state;
    SoDB::addProgressCallback(recordProgress, &state);
    SoSeparator * root = readProgressScene();
    SoDB::removeProgressCallback(recordProgress, &state);

    ASSERT_NE(root, nullptr);
    root->ref();
    EXPECT_EQ(root->getNumChildren(), 2);
    root->unref();

    const std::lock_guard<std::mutex> lock(state.mutex);
    ASSERT_GE(state.events.size(), 4u);
    EXPECT_EQ(state.events.front().item, "File import");
    EXPECT_FLOAT_EQ(state.events.front().fraction, 0.0f);
    EXPECT_TRUE(state.events.front().interruptible);
    EXPECT_FLOAT_EQ(state.events.back().fraction, 1.0f);
    EXPECT_FALSE(state.events.back().interruptible);
    for (size_t i = 1; i < state.events.size(); ++i) {
        EXPECT_GE(state.events[i].fraction, state.events[i - 1].fraction);
    }
}

TEST(IoSodb, ReadAllHonorsProgressCancellationAndReportsAbort)
{
    ProgressState state;
    state.abortAtIntermediate = true;
    SoDB::addProgressCallback(recordProgress, &state);
    SoSeparator * root = readProgressScene();
    SoDB::removeProgressCallback(recordProgress, &state);

    EXPECT_EQ(root, nullptr);
    const std::lock_guard<std::mutex> lock(state.mutex);
    ASSERT_GE(state.events.size(), 3u);
    EXPECT_FLOAT_EQ(state.events.front().fraction, 0.0f);
    EXPECT_FLOAT_EQ(state.events.back().fraction, -1.0f);
    EXPECT_FALSE(state.events.back().interruptible);
}

TEST(IoSodb, ProgressCallbackMayRemoveItself)
{
    std::atomic<int> calls{0};
    SoDB::addProgressCallback(removeSelfProgress, &calls);
    SoSeparator * first = readProgressScene();
    SoSeparator * second = readProgressScene();
    SoDB::removeProgressCallback(removeSelfProgress, &calls);

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    first->ref();
    second->ref();
    first->unref();
    second->unref();
    EXPECT_EQ(calls.load(std::memory_order_relaxed), 1);
}

TEST(IoSodb, ProgressRegistrySupportsConcurrentMutationAndDelivery)
{
    ProgressState stableState;
    SoDB::addProgressCallback(recordProgress, &stableState);
    std::atomic<bool> failed{false};
    std::vector<std::thread> readers;
    for (int thread = 0; thread < 4; ++thread) {
        readers.emplace_back([&] {
            for (int i = 0; i < 100; ++i) {
                SoSeparator * root = readProgressScene();
                if (root == nullptr) {
                    failed.store(true, std::memory_order_relaxed);
                }
                else {
                    root->ref();
                    root->unref();
                }
            }
        });
    }
    std::thread mutator([] {
        for (int i = 0; i < 2000; ++i) {
            SoDB::addProgressCallback(noOpProgress, nullptr);
            SoDB::removeProgressCallback(noOpProgress, nullptr);
        }
    });

    for (std::thread & reader : readers) reader.join();
    mutator.join();
    SoDB::removeProgressCallback(recordProgress, &stableState);

    EXPECT_FALSE(failed.load(std::memory_order_relaxed));
    const std::lock_guard<std::mutex> lock(stableState.mutex);
    EXPECT_GE(stableState.events.size(), 4u * 100u * 4u);
}

TEST(IoSodb, HeaderRegistrySupportsConcurrentRegistrationAndQueries)
{
    constexpr int threadCount = 6;
    constexpr int headersPerThread = 40;
    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;
    threads.reserve(threadCount);

    for (int thread = 0; thread < threadCount; ++thread) {
        threads.emplace_back([&, thread] {
            for (int header = 0; header < headersPerThread; ++header) {
                const std::string text = "#Obol Concurrent " +
                    std::to_string(thread) + " " + std::to_string(header);
                if (!SoDB::registerHeader(SbString(text.c_str()), FALSE, 9.5f,
                                          nullptr, nullptr, nullptr)) {
                    failed.store(true, std::memory_order_relaxed);
                    continue;
                }
                SbBool binary = TRUE;
                float version = 0.0f;
                SoDBHeaderCB * pre = nullptr;
                SoDBHeaderCB * post = nullptr;
                void * userdata = nullptr;
                if (!SoDB::getHeaderData(SbString(text.c_str()), binary,
                                         version, pre, post, userdata) ||
                    binary || version != 9.5f || pre != nullptr ||
                    post != nullptr || userdata != nullptr) {
                    failed.store(true, std::memory_order_relaxed);
                }
            }
        });
    }
    for (std::thread & thread : threads) thread.join();

    EXPECT_FALSE(failed.load(std::memory_order_relaxed));
    EXPECT_GE(SoDB::getNumHeaders(), threadCount * headersPerThread);
}

// -----------------------------------------------------------------------
// SoBase write/read: name disambiguation for multiply-referenced nodes
// Baseline: src/misc/SoBase.cpp OBOL_TEST_SUITE (checkWriteWithMultiref)
//
// When multiple nodes share the same name and are referenced more than
// once, the writer must append "+N" suffixes so that DEF labels are
// unique and USE back-references work correctly.
// -----------------------------------------------------------------------

TEST(IoSodb, SoBaseWriteUnnamedMultiRefNodeUsesDEFUSE)
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
    EXPECT_TRUE(hasDef && hasUse) << "Unnamed multi-ref node should produce DEF/USE in output";
}

TEST(IoSodb, SoBaseWriteSameNamedMultiRefNodesDisambiguatesNames)
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
    EXPECT_TRUE(hasMyNode && hasPlus && hasDef && hasUse) << "Same-named multi-ref nodes should produce disambiguation ('+N') in output";
}

// -----------------------------------------------------------------------
// Binary format I/O: write in binary, read back, verify structure
// -----------------------------------------------------------------------

TEST(IoSodb, BinaryFormatWriteProducesNonASCIIOutput)
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
    EXPECT_TRUE(hasData && hasHeader) << "Binary write produced empty or header-less output";
}

TEST(IoSodb, BinaryFormatWriteReadRoundTripPreservesStructure)
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
    EXPECT_TRUE(hasHeader && hasSufficientData && readOk) << "Binary write/read round-trip failed: header, data, or child count mismatch";
}
