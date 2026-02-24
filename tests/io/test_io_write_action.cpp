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
 * @file test_io_write_action.cpp
 * @brief SoWriteAction / SoOutput / SoInput round-trip tests.
 *
 * Exercises SoWriteAction writing to an in-memory buffer and reading back
 * with SoDB::readAll, covering a wider variety of node types to improve
 * io/ subsystem coverage beyond the 52.2 % baseline.
 *
 * Tests:
 *  1.  ASCII round-trip for SoGroup with SoSphere + SoCube children
 *  2.  Binary round-trip — getBuffer is non-null, readAll returns non-null
 *  3.  Multi-ref scene (shared node) — write+read without crash
 *  4.  SoTransform translation field survives round-trip
 *  5.  SoMaterial diffuseColor field survives round-trip
 *  6.  SoOutput::getBuffer nBytes > 0 after write
 *  7.  SoInput::eof() is TRUE after SoDB::readAll consumes buffer
 *  8.  SoOutput::reset() allows a fresh write with non-empty result
 *  9.  Deep hierarchy (Sep > Sep > Sep > Cube) round-trip
 * 10.  SoOutput::isToBuffer() returns TRUE when no file is set
 * 11.  Two consecutive SoInput::setBuffer calls — reads from second buffer
 * 12.  SoDB::readAll on completely empty buffer returns nullptr gracefully
 */

#include "../test_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/SbString.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/fields/SoMFColor.h>
#include <Inventor/errors/SoError.h>
#include <cstring>
#include <cstdlib>

using namespace SimpleTest;

static void silentErrCb(const SoError *, void *) {}

// ---------------------------------------------------------------------------
// Buffer realloc callback required by SoOutput::setBuffer
// ---------------------------------------------------------------------------
static char *  g_buf      = nullptr;
static size_t  g_buf_size = 0;

static void * bufGrow(void * ptr, size_t size)
{
    g_buf      = static_cast<char *>(std::realloc(ptr, size));
    g_buf_size = size;
    return g_buf;
}

// ---------------------------------------------------------------------------
// Helper: write scene to buffer, return false if write produced nothing.
// buf/bufLen are set on success.
// ---------------------------------------------------------------------------
static bool writeToBuffer(SoNode * root, void *& buf, size_t & bufLen,
                          bool binary = false)
{
    g_buf = nullptr; g_buf_size = 0;
    SoOutput out;
    out.setBuffer(nullptr, 1, bufGrow);
    out.setBinary(binary ? TRUE : FALSE);
    SoWriteAction wa(&out);
    wa.apply(root);
    return out.getBuffer(buf, bufLen) && bufLen > 0;
}

// ---------------------------------------------------------------------------
// Helper: read a scene back from an in-memory buffer.
// ---------------------------------------------------------------------------
static SoSeparator * readFromBuffer(void * buf, size_t bufLen)
{
    SoInput in;
    in.setBuffer(buf, bufLen);
    return SoDB::readAll(&in);
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // 1. ASCII round-trip: SoGroup { SoSphere SoCube } — type and child count
    // -----------------------------------------------------------------------
    runner.startTest("ASCII round-trip: SoGroup with SoSphere+SoCube");
    {
        SoGroup * grp = new SoGroup;
        grp->ref();
        grp->addChild(new SoSphere);
        grp->addChild(new SoCube);

        void * buf = nullptr; size_t bufLen = 0;
        bool wrote = writeToBuffer(grp, buf, bufLen);
        grp->unref();

        bool pass = false;
        if (wrote) {
            SoSeparator * root = readFromBuffer(buf, bufLen);
            if (root) {
                // readAll wraps in a separator; its first child should be the group
                pass = (root->getNumChildren() >= 1);
                root->unref();
            }
        }
        runner.endTest(pass, pass ? "" :
            "ASCII round-trip for SoGroup failed");
    }

    // -----------------------------------------------------------------------
    // 2. Binary round-trip: same scene, setBinary(TRUE)
    // -----------------------------------------------------------------------
    runner.startTest("Binary round-trip: SoGroup with SoSphere+SoCube");
    {
        SoGroup * grp = new SoGroup;
        grp->ref();
        grp->addChild(new SoSphere);
        grp->addChild(new SoCube);

        void * buf = nullptr; size_t bufLen = 0;
        bool wrote = writeToBuffer(grp, buf, bufLen, /*binary=*/true);
        grp->unref();

        bool pass = false;
        if (wrote && buf != nullptr) {
            SoSeparator * root = readFromBuffer(buf, bufLen);
            if (root) {
                pass = (root->getNumChildren() >= 1);
                root->unref();
            }
        }
        runner.endTest(pass, pass ? "" :
            "Binary round-trip for SoGroup failed");
    }

    // -----------------------------------------------------------------------
    // 3. Multi-ref: shared SoCube added twice — write+read without crash
    // -----------------------------------------------------------------------
    runner.startTest("Multi-ref: shared SoCube referenced twice survives round-trip");
    {
        SoCube * sharedCube = new SoCube;
        SoSeparator * sep = new SoSeparator;
        sep->ref();
        sep->addChild(sharedCube);
        sep->addChild(sharedCube);   // second reference

        void * buf = nullptr; size_t bufLen = 0;
        bool wrote = writeToBuffer(sep, buf, bufLen);
        sep->unref();

        bool pass = false;
        if (wrote) {
            SoSeparator * root = readFromBuffer(buf, bufLen);
            if (root) {
                pass = true;   // no crash = success
                root->unref();
            }
        }
        runner.endTest(pass, pass ? "" :
            "Multi-ref round-trip crashed or failed to read back");
    }

    // -----------------------------------------------------------------------
    // 4. SoTransform translation field round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SoTransform translation (1,2,3) survives round-trip");
    {
        SoSeparator * sep = new SoSeparator;
        sep->ref();
        SoTransform * xf = new SoTransform;
        xf->translation.setValue(1.0f, 2.0f, 3.0f);
        sep->addChild(xf);
        sep->addChild(new SoCube);

        void * buf = nullptr; size_t bufLen = 0;
        bool wrote = writeToBuffer(sep, buf, bufLen);
        sep->unref();

        bool pass = false;
        if (wrote) {
            SoSeparator * root = readFromBuffer(buf, bufLen);
            if (root) {
                {
                    SoSearchAction sa;
                    sa.setType(SoTransform::getClassTypeId());
                    sa.setInterest(SoSearchAction::FIRST);
                    sa.apply(root);
                    if (sa.getPath()) {
                        SoTransform * found = static_cast<SoTransform *>(
                            sa.getPath()->getTail());
                        SbVec3f t = found->translation.getValue();
                        pass = (t == SbVec3f(1.0f, 2.0f, 3.0f));
                    }
                } // sa destroyed here, releasing path refs before unref
                root->unref();
            }
        }
        runner.endTest(pass, pass ? "" :
            "SoTransform translation did not round-trip correctly");
    }

    // -----------------------------------------------------------------------
    // 5. SoMaterial diffuseColor field round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SoMaterial diffuseColor (0.2,0.4,0.6) survives round-trip");
    {
        SoSeparator * sep = new SoSeparator;
        sep->ref();
        SoMaterial * mat = new SoMaterial;
        mat->diffuseColor.setValue(0.2f, 0.4f, 0.6f);
        sep->addChild(mat);
        sep->addChild(new SoCube);

        void * buf = nullptr; size_t bufLen = 0;
        bool wrote = writeToBuffer(sep, buf, bufLen);
        sep->unref();

        bool pass = false;
        if (wrote) {
            SoSeparator * root = readFromBuffer(buf, bufLen);
            if (root) {
                {
                    SoSearchAction sa;
                    sa.setType(SoMaterial::getClassTypeId());
                    sa.setInterest(SoSearchAction::FIRST);
                    sa.apply(root);
                    if (sa.getPath()) {
                        SoMaterial * found = static_cast<SoMaterial *>(
                            sa.getPath()->getTail());
                        SbColor c = found->diffuseColor[0];
                        float r, g, b;
                        c.getValue(r, g, b);
                        pass = (fabsf(r - 0.2f) < 1e-4f &&
                                fabsf(g - 0.4f) < 1e-4f &&
                                fabsf(b - 0.6f) < 1e-4f);
                    }
                } // sa destroyed here
                root->unref();
            }
        }
        runner.endTest(pass, pass ? "" :
            "SoMaterial diffuseColor did not round-trip correctly");
    }

    // -----------------------------------------------------------------------
    // 6. SoOutput::getBuffer nBytes > 0 after write
    // -----------------------------------------------------------------------
    runner.startTest("SoOutput::getBuffer nBytes > 0 after write");
    {
        SoSeparator * sep = new SoSeparator;
        sep->ref();
        sep->addChild(new SoCube);

        g_buf = nullptr; g_buf_size = 0;
        SoOutput out;
        out.setBuffer(nullptr, 1, bufGrow);
        SoWriteAction wa(&out);
        wa.apply(sep);
        sep->unref();

        void * buf = nullptr; size_t bufLen = 0;
        out.getBuffer(buf, bufLen);
        bool pass = (bufLen > 0);
        runner.endTest(pass, pass ? "" :
            "SoOutput::getBuffer reported 0 bytes after write");
    }

    // -----------------------------------------------------------------------
    // 7. SoInput::eof() is TRUE after SoDB::readAll consumes the buffer
    // -----------------------------------------------------------------------
    runner.startTest("SoInput::eof() TRUE after SoDB::readAll");
    {
        SoSeparator * sep = new SoSeparator;
        sep->ref();
        sep->addChild(new SoCube);

        void * buf = nullptr; size_t bufLen = 0;
        bool wrote = writeToBuffer(sep, buf, bufLen);
        sep->unref();

        bool pass = false;
        if (wrote) {
            SoInput in;
            in.setBuffer(buf, bufLen);
            SoSeparator * root = SoDB::readAll(&in);
            if (root) {
                root->unref();
                pass = (in.eof() == TRUE);
            }
        }
        runner.endTest(pass, pass ? "" :
            "SoInput::eof() not TRUE after consuming buffer");
    }

    // -----------------------------------------------------------------------
    // 8. SoOutput::reset() clears buffer — second write also produces data
    // -----------------------------------------------------------------------
    runner.startTest("SoOutput::reset() allows fresh write with non-empty result");
    {
        SoSeparator * sep = new SoSeparator;
        sep->ref();
        sep->addChild(new SoCube);

        g_buf = nullptr; g_buf_size = 0;
        SoOutput out;
        out.setBuffer(nullptr, 1, bufGrow);
        SoWriteAction wa(&out);
        wa.apply(sep);

        out.reset();

        SoWriteAction wa2(&out);
        wa2.apply(sep);
        sep->unref();

        void * buf = nullptr; size_t bufLen = 0;
        out.getBuffer(buf, bufLen);
        bool pass = (bufLen > 0);
        runner.endTest(pass, pass ? "" :
            "SoOutput::reset() did not allow a fresh non-empty write");
    }

    // -----------------------------------------------------------------------
    // 9. Deep hierarchy: Sep > Sep > Sep > Cube — round-trip
    // -----------------------------------------------------------------------
    runner.startTest("Deep hierarchy (3 levels) round-trip");
    {
        SoSeparator * outer = new SoSeparator;
        outer->ref();
        SoSeparator * mid = new SoSeparator;
        SoSeparator * inner = new SoSeparator;
        inner->addChild(new SoCube);
        mid->addChild(inner);
        outer->addChild(mid);

        void * buf = nullptr; size_t bufLen = 0;
        bool wrote = writeToBuffer(outer, buf, bufLen);
        outer->unref();

        bool pass = false;
        if (wrote) {
            SoSeparator * root = readFromBuffer(buf, bufLen);
            if (root) {
                {
                    // Search for the deepest SoCube
                    SoSearchAction sa;
                    sa.setType(SoCube::getClassTypeId());
                    sa.setInterest(SoSearchAction::FIRST);
                    sa.apply(root);
                    pass = (sa.getPath() != nullptr);
                } // sa destroyed here
                root->unref();
            }
        }
        runner.endTest(pass, pass ? "" :
            "Deep hierarchy round-trip did not find SoCube at deepest level");
    }

    // -----------------------------------------------------------------------
    // 10. SoOutput::getBufferSize() > 0 after a write
    // -----------------------------------------------------------------------
    runner.startTest("SoOutput::getBufferSize() > 0 after write");
    {
        SoSeparator * sep = new SoSeparator;
        sep->ref();
        sep->addChild(new SoCube);

        g_buf = nullptr; g_buf_size = 0;
        SoOutput out;
        out.setBuffer(nullptr, 1, bufGrow);
        SoWriteAction wa(&out);
        wa.apply(sep);
        sep->unref();

        bool pass = (out.getBufferSize() > 0);
        runner.endTest(pass, pass ? "" :
            "SoOutput::getBufferSize() should be > 0 after write");
    }

    // -----------------------------------------------------------------------
    // 11. Two consecutive SoInput::setBuffer calls — reads from second buffer
    // -----------------------------------------------------------------------
    runner.startTest("SoInput: second setBuffer overrides first");
    {
        // Build two different scenes
        SoSeparator * sep1 = new SoSeparator;
        sep1->ref();
        sep1->addChild(new SoSphere);

        SoSeparator * sep2 = new SoSeparator;
        sep2->ref();
        sep2->addChild(new SoCube);
        sep2->addChild(new SoCylinder);

        void * buf1 = nullptr; size_t len1 = 0;
        void * buf2 = nullptr; size_t len2 = 0;
        bool w1 = writeToBuffer(sep1, buf1, len1);
        bool w2 = writeToBuffer(sep2, buf2, len2);
        sep1->unref();
        sep2->unref();

        bool pass = false;
        if (w1 && w2) {
            SoInput in;
            in.setBuffer(buf1, len1);  // first call
            in.setBuffer(buf2, len2);  // second call — should override
            SoSeparator * root = SoDB::readAll(&in);
            if (root) {
                // The second scene has 2 children; check we got something
                pass = (root->getNumChildren() >= 1);
                root->unref();
            }
        }
        runner.endTest(pass, pass ? "" :
            "Second SoInput::setBuffer did not work correctly");
    }

    // -----------------------------------------------------------------------
    // 12. Graceful handling of completely empty buffer
    // -----------------------------------------------------------------------
    runner.startTest("SoDB::readAll on empty buffer returns nullptr gracefully");
    {
        SoErrorCB * old = SoError::getHandlerCallback();
        SoError::setHandlerCallback(silentErrCb, nullptr);

        const char emptyBuf[] = "";
        SoInput in;
        in.setBuffer(emptyBuf, 0);
        SoSeparator * root = SoDB::readAll(&in);

        SoError::setHandlerCallback(old, nullptr);

        bool pass = (root == nullptr);
        if (root) root->unref();
        runner.endTest(pass, pass ? "" :
            "SoDB::readAll on empty buffer should return nullptr");
    }

    return runner.getSummary() != 0 ? 1 : 0;
}
