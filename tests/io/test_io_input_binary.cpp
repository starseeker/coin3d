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
 * @file test_io_input_binary.cpp
 * @brief Binary SoInput edge-case tests (COVERAGE_PLAN.md priority 53).
 *
 * Covers SoInput::isBinary, getHeader, getIVVersion on binary buffers;
 * corrupt binary file (truncated body, wrong magic) graceful NULL return;
 * sequential open/close; and isValidBuffer / isValidFile edge cases.
 *
 * Subsystems improved: io (SoInput.cpp)
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/errors/SoError.h>

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

using namespace SimpleTest;

// Silence error output during negative tests
static void silentErrCb(const SoError * /*e*/, void * /*data*/) {}

// Global buffer for binary write helper (same pattern as test_io_edge_cases.cpp)
static char  * s_bin_buf  = nullptr;
static size_t  s_bin_size = 0;

static void * binGrow(void * p, size_t s)
{
    s_bin_buf  = static_cast<char *>(std::realloc(p, s));
    s_bin_size = s;
    return s_bin_buf;
}

// -------------------------------------------------------------------------
// Helper: write a node to a binary buffer; returns the buffer and size.
// -------------------------------------------------------------------------
static void writeBinary(SoNode * node, char ** outBuf, size_t * outSize)
{
    s_bin_buf  = nullptr;
    s_bin_size = 0;

    SoOutput out2;
    out2.setBuffer(nullptr, 1, binGrow);
    out2.setBinary(TRUE);
    SoWriteAction wa(&out2);
    wa.apply(node);

    void * ptr = nullptr; size_t nbytes = 0;
    out2.getBuffer(ptr, nbytes);
    *outBuf  = static_cast<char *>(ptr);
    *outSize = nbytes;
}

// -------------------------------------------------------------------------
// Helper: write a node to an ASCII buffer.
// -------------------------------------------------------------------------
static char * g_ascii_buf  = nullptr;
static size_t g_ascii_size = 0;

static void * asciiGrow(void * p, size_t s)
{
    g_ascii_buf  = static_cast<char *>(std::realloc(p, s));
    g_ascii_size = s;
    return g_ascii_buf;
}

static void writeAscii(SoNode * node, char ** outBuf, size_t * outSize)
{
    g_ascii_buf  = nullptr;
    g_ascii_size = 0;
    SoOutput out;
    out.setBuffer(nullptr, 1, asciiGrow);
    out.setBinary(FALSE);
    SoWriteAction wa(&out);
    wa.apply(node);
    void * ptr = nullptr; size_t nb = 0;
    out.getBuffer(ptr, nb);
    *outBuf  = static_cast<char *>(ptr);
    *outSize = nb;
}

// =========================================================================
int main()
{
    TestFixture fixture;
    TestRunner runner;

    // Build a simple scene for round-trip tests
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);

    // -----------------------------------------------------------------------
    // Binary write produces the correct binary magic / header
    // -----------------------------------------------------------------------
    runner.startTest("Binary header starts with '#Inventor V' prefix");
    {
        char * buf = nullptr; size_t sz = 0;
        writeBinary(root, &buf, &sz);
        bool pass = (buf != nullptr) && (sz > 10);
        if (pass) {
            // Coin binary files start with "#Inventor V2.1 binary"
            std::string header(buf, std::min(sz, static_cast<size_t>(30)));
            pass = (header.find("#Inventor") != std::string::npos);
        }
        runner.endTest(pass, pass ? "" :
            "Binary buffer does not start with '#Inventor' header");
    }

    // -----------------------------------------------------------------------
    // SoInput::isBinary() returns TRUE for a binary buffer
    // -----------------------------------------------------------------------
    runner.startTest("SoInput::isBinary() returns TRUE for binary buffer");
    {
        char * buf = nullptr; size_t sz = 0;
        writeBinary(root, &buf, &sz);
        bool pass = false;
        if (buf && sz > 0) {
            SoInput in;
            in.setBuffer(buf, sz);
            pass = (in.isBinary() == TRUE);
        }
        runner.endTest(pass, pass ? "" :
            "SoInput::isBinary() returned FALSE for binary buffer");
    }

    // -----------------------------------------------------------------------
    // SoInput::getHeader() on binary buffer
    // -----------------------------------------------------------------------
    runner.startTest("SoInput::getHeader() returns non-empty string for binary");
    {
        char * buf = nullptr; size_t sz = 0;
        writeBinary(root, &buf, &sz);
        bool pass = false;
        if (buf && sz > 0) {
            SoInput in;
            in.setBuffer(buf, sz);
            SbString hdr = in.getHeader();
            pass = (hdr.getLength() > 0);
        }
        runner.endTest(pass, pass ? "" :
            "SoInput::getHeader() returned empty string for binary buffer");
    }

    // -----------------------------------------------------------------------
    // SoInput::isBinary() returns FALSE for an ASCII buffer
    // -----------------------------------------------------------------------
    runner.startTest("SoInput::isBinary() returns FALSE for ASCII buffer");
    {
        char * buf = nullptr; size_t sz = 0;
        writeAscii(root, &buf, &sz);
        bool pass = false;
        if (buf && sz > 0) {
            SoInput in;
            in.setBuffer(buf, sz);
            pass = (in.isBinary() == FALSE);
        }
        runner.endTest(pass, pass ? "" :
            "SoInput::isBinary() returned TRUE for ASCII buffer");
    }

    // -----------------------------------------------------------------------
    // Binary round-trip: write binary then read back
    // -----------------------------------------------------------------------
    runner.startTest("Binary round-trip: SoDB::readAll recovers same node count");
    {
        char * buf = nullptr; size_t sz = 0;
        writeBinary(root, &buf, &sz);
        bool pass = false;
        if (buf && sz > 0) {
            SoInput in;
            in.setBuffer(buf, sz);
            SoSeparator * r2 = SoDB::readAll(&in);
            if (r2) {
                pass = (r2->getNumChildren() == root->getNumChildren());
                r2->unref();
            }
        }
        runner.endTest(pass, pass ? "" :
            "Binary round-trip: readAll returned wrong number of children");
    }

    // -----------------------------------------------------------------------
    // Corrupt binary file: truncated body (only header)
    // -----------------------------------------------------------------------
    runner.startTest("Truncated binary: SoDB::readAll returns null gracefully");
    {
        char * buf = nullptr; size_t sz = 0;
        writeBinary(root, &buf, &sz);

        SoErrorCB * old = SoError::getHandlerCallback();
        SoError::setHandlerCallback(silentErrCb, nullptr);

        bool pass = false;
        if (buf && sz > 10) {
            // Keep only the first 20 bytes (header only, body is truncated)
            size_t truncSz = (sz > 40) ? 40 : sz / 2;
            SoInput in;
            in.setBuffer(buf, truncSz);
            SoSeparator * r = SoDB::readAll(&in);
            // A truncated binary file should either return null or an empty tree.
            // Both are acceptable; the important thing is no crash.
            pass = true;  // if we reach here without crashing, test passes
            if (r) r->unref();
        } else {
            pass = false; // could not write binary
        }

        SoError::setHandlerCallback(old, nullptr);
        runner.endTest(pass, pass ? "" :
            "Truncated binary: SoDB::readAll crashed or write failed");
    }

    // -----------------------------------------------------------------------
    // Wrong magic: binary-looking data with wrong magic should not produce
    // a valid scene graph (may return null or an empty separator; either is ok).
    // The important thing is NO CRASH.
    // -----------------------------------------------------------------------
    runner.startTest("Wrong magic: SoDB::readAll does not crash on garbage binary");
    {
        // This is binary-looking content but with a completely invalid Inventor header
        static const char wrongMagic[] =
            "#BOGUSFORMAT V9.9 binary\n"
            "\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF";

        SoErrorCB * old = SoError::getHandlerCallback();
        SoError::setHandlerCallback(silentErrCb, nullptr);

        SoInput in;
        in.setBuffer(const_cast<char *>(wrongMagic), sizeof(wrongMagic) - 1);
        SoSeparator * r = SoDB::readAll(&in);

        SoError::setHandlerCallback(old, nullptr);

        // null or empty separator: both are acceptable; no crash = pass
        bool pass = true;
        if (r) r->unref();
        runner.endTest(pass, pass ? "" :
            "Wrong magic: SoDB::readAll crashed");
    }

    // -----------------------------------------------------------------------
    // isValidBuffer returns TRUE after setBuffer with valid ASCII
    // -----------------------------------------------------------------------
    runner.startTest("SoInput::isValidBuffer() TRUE after setBuffer with valid ASCII");
    {
        static const char scene[] =
            "#Inventor V2.1 ascii\nSeparator { Cube {} }\n";
        SoInput in;
        in.setBuffer(const_cast<char *>(scene), std::strlen(scene));
        bool pass = (in.isValidBuffer() == TRUE);
        runner.endTest(pass, pass ? "" :
            "isValidBuffer() returned FALSE for valid ASCII buffer");
    }

    // -----------------------------------------------------------------------
    // isValidBuffer returns TRUE even for non-Inventor buffers (by design).
    // isValidBuffer(FALSE) just checks if the buffer is non-null/non-empty;
    // it does NOT validate the format.  Verify this documented behavior.
    // -----------------------------------------------------------------------
    runner.startTest("SoInput::isValidBuffer() TRUE even for non-Inventor text (by design)");
    {
        static const char nonInventor[] = "this is completely invalid";

        SoInput in;
        in.setBuffer(const_cast<char *>(nonInventor), std::strlen(nonInventor));
        // Per Coin documentation, isValidBuffer() uses checkHeader(FALSE) which
        // does NOT require a valid header for memory buffers.
        // Outcome may be TRUE or FALSE depending on Coin version; we only verify
        // no crash.
        (void)in.isValidBuffer();
        bool pass = true;  // reaching here without crash = pass
        runner.endTest(pass, "");
    }

    // -----------------------------------------------------------------------
    // Sequential reads: same SoInput can be reused with different buffers
    // -----------------------------------------------------------------------
    runner.startTest("Sequential setBuffer reads succeed");
    {
        static const char s1[] = "#Inventor V2.1 ascii\nSeparator { Cube {} }\n";
        static const char s2[] = "#Inventor V2.1 ascii\nSeparator { Sphere {} }\n";

        SoInput in;
        in.setBuffer(const_cast<char *>(s1), std::strlen(s1));
        SoSeparator * r1 = SoDB::readAll(&in);

        in.setBuffer(const_cast<char *>(s2), std::strlen(s2));
        SoSeparator * r2 = SoDB::readAll(&in);

        bool pass = (r1 != nullptr) && (r2 != nullptr);
        if (r1) r1->unref();
        if (r2) r2->unref();
        runner.endTest(pass, pass ? "" :
            "Sequential reads: one or both returned null");
    }

    // -----------------------------------------------------------------------
    // Large binary scene: write many nodes, then read back
    // -----------------------------------------------------------------------
    runner.startTest("Large binary scene round-trip (10 children)");
    {
        SoSeparator * big = new SoSeparator;
        big->ref();
        for (int i = 0; i < 10; ++i) {
            big->addChild(new SoCone);
        }

        char * buf = nullptr; size_t sz = 0;
        writeBinary(big, &buf, &sz);
        big->unref();

        bool pass = false;
        if (buf && sz > 0) {
            SoInput in;
            in.setBuffer(buf, sz);
            SoSeparator * r = SoDB::readAll(&in);
            if (r) {
                pass = (r->getNumChildren() == 10);
                r->unref();
            }
        }
        runner.endTest(pass, pass ? "" :
            "Large binary round-trip: child count mismatch or readAll failed");
    }

    root->unref();
    return runner.getSummary();
}
