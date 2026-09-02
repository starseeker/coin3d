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
 * @file test_io_edge_cases.cpp
 * @brief I/O edge case tests supplementing test_sodb.cpp.
 *
 * Covers:
 *   Binary write  - output buffer starts with binary Inventor header
 *   Binary round-trip - write binary then read back
 *   Multiple sequential setBuffer calls on SoInput
 *   SoOutput::getBuffer returns non-null pointer and positive size
 *   Corrupt/empty header - SoDB::readAll returns null gracefully
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/errors/SoError.h>
#include <Inventor/errors/SoReadError.h>

#include <cstring>
#include <cstdlib>
#include <string>

using namespace ObolTest;

// ---------------------------------------------------------------------------
// Write helpers (mirrors test_sodb.cpp helpers)
// ---------------------------------------------------------------------------
static char*  g_buf      = nullptr;
static size_t g_buf_size = 0;

static void * bufGrow(void * ptr, size_t size)
{
    g_buf      = static_cast<char *>(std::realloc(ptr, size));
    g_buf_size = size;
    return g_buf;
}

static void writeNode(SoNode * root, char ** outBuf, size_t * outSize)
{
    g_buf = nullptr; g_buf_size = 0;
    SoOutput out;
    out.setBuffer(nullptr, 1, bufGrow);
    SoWriteAction wa(&out);
    wa.apply(root);
    void * ptr = nullptr; size_t nbytes = 0;
    out.getBuffer(ptr, nbytes);
    *outBuf  = static_cast<char *>(ptr);
    *outSize = nbytes;
}

static void writeNodeBinary(SoNode * root, char ** outBuf, size_t * outSize)
{
    g_buf = nullptr; g_buf_size = 0;
    SoOutput out;
    out.setBuffer(nullptr, 1, bufGrow);
    out.setBinary(TRUE);
    SoWriteAction wa(&out);
    wa.apply(root);
    void * ptr = nullptr; size_t nbytes = 0;
    out.getBuffer(ptr, nbytes);
    *outBuf  = static_cast<char *>(ptr);
    *outSize = nbytes;
}

// Silent callback used to suppress error output during negative tests
static void silentErrCb(const SoError * /*err*/, void * /*data*/) {}

TEST(IoEdgeCases, BinaryWriteBufferStartsWithInventorV21Binary)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);
    char * buf = nullptr; size_t sz = 0;
    writeNodeBinary(root, &buf, &sz);

    static const char header[] = "#Inventor V2.1 binary";
    EXPECT_TRUE((buf != nullptr) &&
                (sz  >= std::strlen(header)) &&
                (std::memcmp(buf, header, std::strlen(header)) == 0)) << "Binary output does not start with '#Inventor V2.1 binary'";
    std::free(buf);
    root->unref();
}

// -----------------------------------------------------------------------
// SoOutput::getBuffer: returns non-null pointer and size > 0
// -----------------------------------------------------------------------

TEST(IoEdgeCases, SoOutputGetBufferReturnsNonNullAndPositiveSize)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);
    char * buf = nullptr; size_t sz = 0;
    writeNode(root, &buf, &sz);
    EXPECT_TRUE((buf != nullptr) && (sz > 0)) << "SoOutput::getBuffer returned null or zero size";
    std::free(buf);
    root->unref();
}

// -----------------------------------------------------------------------
// Binary read round-trip
// -----------------------------------------------------------------------

TEST(IoEdgeCases, BinaryRoundTripWriteBinaryThenReadBackNonNullRoot)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);
    char * buf = nullptr; size_t sz = 0;
    writeNodeBinary(root, &buf, &sz);

    SoSeparator * readRoot = nullptr;
    if (buf != nullptr && sz > 0) {
        SoInput in;
        in.setBuffer(buf, sz);
        readRoot = SoDB::readAll(&in);
        if (readRoot) {
            readRoot->ref();
            readRoot->unref();
        }
    }
    EXPECT_NE(readRoot, nullptr);
    std::free(buf);
    root->unref();
}

TEST(IoEdgeCases, BinaryRoundTripRootHasExpectedChildren)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);
    char * buf = nullptr; size_t sz = 0;
    writeNodeBinary(root, &buf, &sz);

    SoSeparator * readRoot = nullptr;
    int childCount = 0;
    if (buf != nullptr && sz > 0) {
        SoInput in;
        in.setBuffer(buf, sz);
        readRoot = SoDB::readAll(&in);
        if (readRoot) {
            readRoot->ref();
            childCount = readRoot->getNumChildren();
            readRoot->unref();
        }
    }
    EXPECT_NE(readRoot, nullptr);
    EXPECT_GT(childCount, 0);
    std::free(buf);
    root->unref();
}

// -----------------------------------------------------------------------
// Multiple sequential setBuffer calls on same SoInput
// -----------------------------------------------------------------------

TEST(IoEdgeCases, MultipleSequentialSetBufferReadsOnOneSoInput)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);
    static const char scene1[] =
        "#Inventor V2.1 ascii\nSeparator { Cube {} }\n";
    static const char scene2[] =
        "#Inventor V2.1 ascii\nSeparator { Sphere {} }\n";

    SoInput in;
    in.setBuffer(const_cast<char *>(scene1), std::strlen(scene1));
    SoSeparator * r1 = SoDB::readAll(&in);
    if (r1) r1->ref();

    in.setBuffer(const_cast<char *>(scene2), std::strlen(scene2));
    SoSeparator * r2 = SoDB::readAll(&in);
    if (r2) r2->ref();

    EXPECT_TRUE((r1 != nullptr) && (r2 != nullptr)) << "Sequential setBuffer reads: one or both reads returned null";
    if (r1) r1->unref();
    if (r2) r2->unref();
    root->unref();
}

// -----------------------------------------------------------------------
// Corrupt header: SoDB::readAll should return null gracefully
// -----------------------------------------------------------------------

TEST(IoEdgeCases, CorruptHeaderSoDBReadAllReturnsNull)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);
    static const char garbage[] = "THIS IS NOT AN INVENTOR FILE\n{ garbage }\n";

    // Suppress error output for this negative test
    SoErrorCB * oldCb = SoError::getHandlerCallback();
    SoError::setHandlerCallback(silentErrCb, nullptr);

    SoInput in;
    in.setBuffer(const_cast<char *>(garbage), std::strlen(garbage));
    SoSeparator * r = SoDB::readAll(&in);

    SoError::setHandlerCallback(oldCb, nullptr);

    EXPECT_TRUE((r == nullptr)) << "Corrupt header: SoDB::readAll should return null";
    if (r) {
        r->ref();
        r->unref();
    }
    root->unref();
}

// -----------------------------------------------------------------------
// Empty buffer: SoDB::readAll returns null
// -----------------------------------------------------------------------

TEST(IoEdgeCases, EmptyBufferSoDBReadAllReturnsNull)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);
    SoErrorCB * oldCb = SoError::getHandlerCallback();
    SoError::setHandlerCallback(silentErrCb, nullptr);

    static const char empty[] = "";
    SoInput in;
    in.setBuffer(const_cast<char *>(empty), 0);
    SoSeparator * r = SoDB::readAll(&in);

    SoError::setHandlerCallback(oldCb, nullptr);

    EXPECT_TRUE((r == nullptr)) << "Empty buffer: SoDB::readAll should return null";
    if (r) {
        r->ref();
        r->unref();
    }
    root->unref();
}

TEST(IoEdgeCases, IntegerParsingHasNoFixedTokenLimit)
{
    std::string digits(4096, '0');
    digits += '7';

    SoInput input;
    input.setBuffer(digits.data(), digits.size());
    unsigned int value = 0;
    EXPECT_TRUE(input.read(value));
    EXPECT_EQ(value, 7u);
}

static SoSeparator * readMalformedSceneWithoutDiagnostics(const std::string & scene)
{
    SoErrorCB * old_error_callback = SoError::getHandlerCallback();
    void * old_error_data = SoError::getHandlerData();
    SoErrorCB * old_read_callback = SoReadError::getHandlerCallback();
    void * old_read_data = SoReadError::getHandlerData();
    SoError::setHandlerCallback(silentErrCb, nullptr);
    SoReadError::setHandlerCallback(silentErrCb, nullptr);

    SoInput input;
    input.setBuffer(scene.data(), scene.size());
    SoSeparator * root = SoDB::readAll(&input);

    SoReadError::setHandlerCallback(old_read_callback, old_read_data);
    SoError::setHandlerCallback(old_error_callback, old_error_data);
    return root;
}

TEST(IoEdgeCases, NegativePathLengthFailsGracefully)
{
    const std::string scene =
        "#Inventor V2.1 ascii\n"
        "PathSwitch {\n"
        "  path Path { Separator { Cube {} } -1 }\n"
        "}\n";

    SoSeparator * root = readMalformedSceneWithoutDiagnostics(scene);
    EXPECT_EQ(root, nullptr);
    if (root) {
        root->ref();
        root->unref();
    }
}
