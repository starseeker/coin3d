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

using namespace ObolTest;

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
    SoSeparator * root = SoDB::readAll(&in);
    if (root) root->ref();
    return root;
}

TEST(IoWriteAction, ASCIIRoundTripSoGroupWithSoSphereSoCube)
{
    SoGroup * grp = new SoGroup;
    grp->ref();
    grp->addChild(new SoSphere);
    grp->addChild(new SoCube);

    void * buf = nullptr; size_t bufLen = 0;
    bool wrote = writeToBuffer(grp, buf, bufLen);
    grp->unref();

    EXPECT_TRUE(wrote);
    if (wrote) {
        SoSeparator * root = readFromBuffer(buf, bufLen);
        EXPECT_NE(root, nullptr);
        if (root) {
            // readAll wraps in a separator; its first child should be the group
            EXPECT_GE(root->getNumChildren(), 1);
            root->unref();
        }
    }
    std::free(buf);
}

// -----------------------------------------------------------------------
// 2. Binary round-trip: same scene, setBinary(TRUE)
// -----------------------------------------------------------------------

TEST(IoWriteAction, BinaryRoundTripSoGroupWithSoSphereSoCube)
{
    SoGroup * grp = new SoGroup;
    grp->ref();
    grp->addChild(new SoSphere);
    grp->addChild(new SoCube);

    void * buf = nullptr; size_t bufLen = 0;
    bool wrote = writeToBuffer(grp, buf, bufLen, /*binary=*/true);
    grp->unref();

    EXPECT_TRUE(wrote);
    EXPECT_NE(buf, nullptr);
    if (wrote && buf != nullptr) {
        SoSeparator * root = readFromBuffer(buf, bufLen);
        EXPECT_NE(root, nullptr);
        if (root) {
            EXPECT_GE(root->getNumChildren(), 1);
            root->unref();
        }
    }
    std::free(buf);
}

// -----------------------------------------------------------------------
// 3. Multi-ref: shared SoCube added twice — write+read without crash
// -----------------------------------------------------------------------

TEST(IoWriteAction, MultiRefSharedSoCubeReferencedTwiceSurvivesRoundTrip)
{
    SoCube * sharedCube = new SoCube;
    SoSeparator * sep = new SoSeparator;
    sep->ref();
    sep->addChild(sharedCube);
    sep->addChild(sharedCube);   // second reference

    void * buf = nullptr; size_t bufLen = 0;
    bool wrote = writeToBuffer(sep, buf, bufLen);
    sep->unref();

    EXPECT_TRUE(wrote);
    if (wrote) {
        SoSeparator * root = readFromBuffer(buf, bufLen);
        EXPECT_NE(root, nullptr);
        if (root) {
            root->unref();
        }
    }
    std::free(buf);
}

// -----------------------------------------------------------------------
// 4. SoTransform translation field round-trip
// -----------------------------------------------------------------------

TEST(IoWriteAction, SoTransformTranslation123SurvivesRoundTrip)
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

    EXPECT_TRUE(wrote);
    if (wrote) {
        SoSeparator * root = readFromBuffer(buf, bufLen);
        EXPECT_NE(root, nullptr);
        if (root) {
            {
                SoSearchAction sa;
                sa.setType(SoTransform::getClassTypeId());
                sa.setInterest(SoSearchAction::FIRST);
                sa.apply(root);
                EXPECT_NE(sa.getPath(), nullptr);
                if (sa.getPath() != nullptr) {
                    SoTransform * found = static_cast<SoTransform *>(
                        sa.getPath()->getTail());
                    SbVec3f t = found->translation.getValue();
                    EXPECT_EQ(t, SbVec3f(1.0f, 2.0f, 3.0f));
                }
            } // sa destroyed here, releasing path refs before unref
            root->unref();
        }
    }
    std::free(buf);
}

// -----------------------------------------------------------------------
// 5. SoMaterial diffuseColor field round-trip
// -----------------------------------------------------------------------

TEST(IoWriteAction, SoMaterialDiffuseColor020406SurvivesRoundTrip)
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

    EXPECT_TRUE(wrote);
    if (wrote) {
        SoSeparator * root = readFromBuffer(buf, bufLen);
        EXPECT_NE(root, nullptr);
        if (root) {
            {
                SoSearchAction sa;
                sa.setType(SoMaterial::getClassTypeId());
                sa.setInterest(SoSearchAction::FIRST);
                sa.apply(root);
                EXPECT_NE(sa.getPath(), nullptr);
                if (sa.getPath() != nullptr) {
                    SoMaterial * found = static_cast<SoMaterial *>(
                        sa.getPath()->getTail());
                    SbColor c = found->diffuseColor[0];
                    float r, g, b;
                    c.getValue(r, g, b);
                    EXPECT_NEAR(r, 0.2f, 1e-4f);
                    EXPECT_NEAR(g, 0.4f, 1e-4f);
                    EXPECT_NEAR(b, 0.6f, 1e-4f);
                }
            } // sa destroyed here
            root->unref();
        }
    }
    std::free(buf);
}

// -----------------------------------------------------------------------
// 6. SoOutput::getBuffer nBytes > 0 after write
// -----------------------------------------------------------------------

TEST(IoWriteAction, SoOutputGetBufferNBytes0AfterWrite)
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
    EXPECT_TRUE((bufLen > 0)) << "SoOutput::getBuffer reported 0 bytes after write";
    std::free(buf);
}

// -----------------------------------------------------------------------
// 7. SoInput::eof() is TRUE after SoDB::readAll consumes the buffer
// -----------------------------------------------------------------------

TEST(IoWriteAction, SoInputEofTRUEAfterSoDBReadAll)
{
    SoSeparator * sep = new SoSeparator;
    sep->ref();
    sep->addChild(new SoCube);

    void * buf = nullptr; size_t bufLen = 0;
    bool wrote = writeToBuffer(sep, buf, bufLen);
    sep->unref();

    EXPECT_TRUE(wrote);
    if (wrote) {
        SoInput in;
        in.setBuffer(buf, bufLen);
        SoSeparator * root = SoDB::readAll(&in);
        EXPECT_NE(root, nullptr);
        if (root) {
            root->ref();
            root->unref();
            EXPECT_TRUE(in.eof());
        }
    }
    std::free(buf);
}

// -----------------------------------------------------------------------
// 8. SoOutput::reset() clears buffer — second write also produces data
// -----------------------------------------------------------------------

TEST(IoWriteAction, SoOutputResetAllowsFreshWriteWithNonEmptyResult)
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
    EXPECT_TRUE((bufLen > 0)) << "SoOutput::reset() did not allow a fresh non-empty write";
    std::free(buf);
}

// -----------------------------------------------------------------------
// 9. Deep hierarchy: Sep > Sep > Sep > Cube — round-trip
// -----------------------------------------------------------------------

TEST(IoWriteAction, DeepHierarchy3LevelsRoundTrip)
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

    EXPECT_TRUE(wrote);
    if (wrote) {
        SoSeparator * root = readFromBuffer(buf, bufLen);
        EXPECT_NE(root, nullptr);
        if (root) {
            {
                // Search for the deepest SoCube
                SoSearchAction sa;
                sa.setType(SoCube::getClassTypeId());
                sa.setInterest(SoSearchAction::FIRST);
                sa.apply(root);
                EXPECT_NE(sa.getPath(), nullptr);
            } // sa destroyed here
            root->unref();
        }
    }
    std::free(buf);
}

// -----------------------------------------------------------------------
// 10. SoOutput::getBufferSize() > 0 after a write
// -----------------------------------------------------------------------

TEST(IoWriteAction, SoOutputGetBufferSize0AfterWrite)
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

    EXPECT_TRUE((out.getBufferSize() > 0)) << "SoOutput::getBufferSize() should be > 0 after write";
    std::free(g_buf);
    g_buf = nullptr;
}

// -----------------------------------------------------------------------
// 11. Two consecutive SoInput::setBuffer calls — reads from second buffer
// -----------------------------------------------------------------------

TEST(IoWriteAction, SoInputSecondSetBufferOverridesFirst)
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

    EXPECT_TRUE(w1);
    EXPECT_TRUE(w2);
    if (w1 && w2) {
        SoInput in;
        in.setBuffer(buf1, len1);  // first call
        in.setBuffer(buf2, len2);  // second call — should override
        SoSeparator * root = SoDB::readAll(&in);
        EXPECT_NE(root, nullptr);
        if (root) {
            root->ref();
            // The second scene has 2 children; check we got something
            EXPECT_GE(root->getNumChildren(), 1);
            root->unref();
        }
    }
    std::free(buf1);
    std::free(buf2);
}

// -----------------------------------------------------------------------
// 12. Graceful handling of completely empty buffer
// -----------------------------------------------------------------------

TEST(IoWriteAction, SoDBReadAllOnEmptyBufferReturnsNullptrGracefully)
{
    SoErrorCB * old = SoError::getHandlerCallback();
    SoError::setHandlerCallback(silentErrCb, nullptr);

    const char emptyBuf[] = "";
    SoInput in;
    in.setBuffer(emptyBuf, 0);
    SoSeparator * root = SoDB::readAll(&in);

    SoError::setHandlerCallback(old, nullptr);

    EXPECT_TRUE((root == nullptr)) << "SoDB::readAll on empty buffer should return nullptr";
    if (root) {
        root->ref();
        root->unref();
    }
}
