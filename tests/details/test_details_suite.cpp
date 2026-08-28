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
 * @file test_details_suite.cpp
 * @brief Tests for Coin3D detail classes.
 *
 * Covers (Tier 5, priority 57):
 *   SoPointDetail  - construct, set/get coordinate/material/normal/texcoord indices, copy
 *   SoFaceDetail   - construct, setNumPoints, setFaceIndex, setPartIndex,
 *                    getNumPoints, getFaceIndex, getPartIndex, setPoint/getPoint, copy
 *   SoLineDetail   - construct, setLineIndex, setPartIndex, setPoint0/Point1,
 *                    getLineIndex, getPartIndex, getPoint0/getPoint1, copy
 *   SoRayPickAction against SoIndexedFaceSet: verify face detail attached to picked point
 *
 * Subsystems improved: details, actions
 */

#include "../test_utils.h"

#include <Inventor/details/SoDetail.h>
#include <Inventor/details/SoPointDetail.h>
#include <Inventor/details/SoFaceDetail.h>
#include <Inventor/details/SoLineDetail.h>
#include <Inventor/SoType.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbLinear.h>

using namespace ObolTest;

TEST(DetailsSuite, SoPointDetailGetClassTypeIdIsNotBadType)
{
    EXPECT_TRUE((SoPointDetail::getClassTypeId() != SoType::badType())) << "SoPointDetail has bad class type";
}

TEST(DetailsSuite, SoPointDetailConstructAndDefaultIndicesAre0)
{
    SoPointDetail pd;
    // Coin initialises all indices to 0 by default
    EXPECT_TRUE((pd.getCoordinateIndex() == 0) &&
                (pd.getMaterialIndex()   == 0) &&
                (pd.getNormalIndex()     == 0) &&
                (pd.getTextureCoordIndex() == 0)) << "SoPointDetail default indices should be 0";
}

TEST(DetailsSuite, SoPointDetailSetCoordinateIndexGetCoordinateIndex)
{
    SoPointDetail pd;
    pd.setCoordinateIndex(7);
    EXPECT_TRUE((pd.getCoordinateIndex() == 7)) << "SoPointDetail coordinate index round-trip failed";
}

TEST(DetailsSuite, SoPointDetailSetMaterialIndexGetMaterialIndex)
{
    SoPointDetail pd;
    pd.setMaterialIndex(3);
    EXPECT_TRUE((pd.getMaterialIndex() == 3)) << "SoPointDetail material index round-trip failed";
}

TEST(DetailsSuite, SoPointDetailSetNormalIndexGetNormalIndex)
{
    SoPointDetail pd;
    pd.setNormalIndex(5);
    EXPECT_TRUE((pd.getNormalIndex() == 5)) << "SoPointDetail normal index round-trip failed";
}

TEST(DetailsSuite, SoPointDetailSetTextureCoordIndexGetTextureCoordIndex)
{
    SoPointDetail pd;
    pd.setTextureCoordIndex(2);
    EXPECT_TRUE((pd.getTextureCoordIndex() == 2)) << "SoPointDetail texture coord index round-trip failed";
}

TEST(DetailsSuite, SoPointDetailCopyProducesIndependentDuplicate)
{
    SoPointDetail pd;
    pd.setCoordinateIndex(10);
    pd.setMaterialIndex(11);
    SoDetail * copied = pd.copy();
    EXPECT_NE(copied, nullptr);
    if (copied) {
        SoPointDetail * cpd = static_cast<SoPointDetail *>(copied);
        EXPECT_EQ(cpd->getCoordinateIndex(), 10);
        EXPECT_EQ(cpd->getMaterialIndex(), 11);
    }
    delete copied;
}

// -----------------------------------------------------------------------
// SoFaceDetail
// -----------------------------------------------------------------------

TEST(DetailsSuite, SoFaceDetailGetClassTypeIdIsNotBadType)
{
    EXPECT_TRUE((SoFaceDetail::getClassTypeId() != SoType::badType())) << "SoFaceDetail has bad class type";
}

TEST(DetailsSuite, SoFaceDetailConstructDefaultGetNumPoints0)
{
    SoFaceDetail fd;
    EXPECT_TRUE((fd.getNumPoints() == 0)) << "SoFaceDetail default numPoints should be 0";
}

TEST(DetailsSuite, SoFaceDetailSetFaceIndexGetFaceIndexRoundTrip)
{
    SoFaceDetail fd;
    fd.setFaceIndex(4);
    EXPECT_TRUE((fd.getFaceIndex() == 4)) << "SoFaceDetail face index round-trip failed";
}

TEST(DetailsSuite, SoFaceDetailSetPartIndexGetPartIndexRoundTrip)
{
    SoFaceDetail fd;
    fd.setPartIndex(2);
    EXPECT_TRUE((fd.getPartIndex() == 2)) << "SoFaceDetail part index round-trip failed";
}

TEST(DetailsSuite, SoFaceDetailSetNumPoints3GetNumPoints3)
{
    SoFaceDetail fd;
    fd.setNumPoints(3);
    EXPECT_TRUE((fd.getNumPoints() == 3)) << "SoFaceDetail setNumPoints(3) failed";
}

TEST(DetailsSuite, SoFaceDetailSetPointGetPointRoundTrip)
{
    SoFaceDetail fd;
    fd.setNumPoints(2);
    SoPointDetail pd0, pd1;
    pd0.setCoordinateIndex(10);
    pd1.setCoordinateIndex(20);
    fd.setPoint(0, &pd0);
    fd.setPoint(1, &pd1);
    const SoPointDetail * got0 = fd.getPoint(0);
    const SoPointDetail * got1 = fd.getPoint(1);
    EXPECT_TRUE((got0 != nullptr) && (got1 != nullptr) &&
                (got0->getCoordinateIndex() == 10) &&
                (got1->getCoordinateIndex() == 20)) << "SoFaceDetail setPoint/getPoint round-trip failed";
}

TEST(DetailsSuite, SoFaceDetailIncFaceIndexIncrementsBy1)
{
    SoFaceDetail fd;
    fd.setFaceIndex(5);
    fd.incFaceIndex();
    EXPECT_TRUE((fd.getFaceIndex() == 6)) << "SoFaceDetail incFaceIndex should increment by 1";
}

TEST(DetailsSuite, SoFaceDetailIncPartIndexIncrementsBy1)
{
    SoFaceDetail fd;
    fd.setPartIndex(3);
    fd.incPartIndex();
    EXPECT_TRUE((fd.getPartIndex() == 4)) << "SoFaceDetail incPartIndex should increment by 1";
}

TEST(DetailsSuite, SoFaceDetailCopyProducesIndependentDuplicate)
{
    SoFaceDetail fd;
    fd.setFaceIndex(7);
    fd.setPartIndex(2);
    SoDetail * copied = fd.copy();
    EXPECT_NE(copied, nullptr);
    if (copied) {
        SoFaceDetail * cfd = static_cast<SoFaceDetail *>(copied);
        EXPECT_EQ(cfd->getFaceIndex(), 7);
        EXPECT_EQ(cfd->getPartIndex(), 2);
    }
    delete copied;
}

// -----------------------------------------------------------------------
// SoLineDetail
// -----------------------------------------------------------------------

TEST(DetailsSuite, SoLineDetailGetClassTypeIdIsNotBadType)
{
    EXPECT_TRUE((SoLineDetail::getClassTypeId() != SoType::badType())) << "SoLineDetail has bad class type";
}

TEST(DetailsSuite, SoLineDetailConstructDefaultIndicesAre0)
{
    SoLineDetail ld;
    // Default indices should be 0
    EXPECT_TRUE((ld.getLineIndex() == 0) && (ld.getPartIndex() == 0)) << "SoLineDetail default indices should be 0";
}

TEST(DetailsSuite, SoLineDetailSetLineIndexGetLineIndexRoundTrip)
{
    SoLineDetail ld;
    ld.setLineIndex(8);
    EXPECT_TRUE((ld.getLineIndex() == 8)) << "SoLineDetail line index round-trip failed";
}

TEST(DetailsSuite, SoLineDetailSetPartIndexGetPartIndexRoundTrip)
{
    SoLineDetail ld;
    ld.setPartIndex(3);
    EXPECT_TRUE((ld.getPartIndex() == 3)) << "SoLineDetail part index round-trip failed";
}

TEST(DetailsSuite, SoLineDetailSetPoint0GetPoint0RoundTrip)
{
    SoLineDetail ld;
    SoPointDetail pd;
    pd.setCoordinateIndex(42);
    ld.setPoint0(&pd);
    const SoPointDetail * got = ld.getPoint0();
    EXPECT_TRUE((got != nullptr) && (got->getCoordinateIndex() == 42)) << "SoLineDetail setPoint0/getPoint0 round-trip failed";
}

TEST(DetailsSuite, SoLineDetailSetPoint1GetPoint1RoundTrip)
{
    SoLineDetail ld;
    SoPointDetail pd;
    pd.setCoordinateIndex(99);
    ld.setPoint1(&pd);
    const SoPointDetail * got = ld.getPoint1();
    EXPECT_TRUE((got != nullptr) && (got->getCoordinateIndex() == 99)) << "SoLineDetail setPoint1/getPoint1 round-trip failed";
}

TEST(DetailsSuite, SoLineDetailIncLineIndexIncrementsBy1)
{
    SoLineDetail ld;
    ld.setLineIndex(2);
    ld.incLineIndex();
    EXPECT_TRUE((ld.getLineIndex() == 3)) << "SoLineDetail incLineIndex should increment by 1";
}

TEST(DetailsSuite, SoLineDetailIncPartIndexIncrementsBy1)
{
    SoLineDetail ld;
    ld.setPartIndex(1);
    ld.incPartIndex();
    EXPECT_TRUE((ld.getPartIndex() == 2)) << "SoLineDetail incPartIndex should increment by 1";
}

TEST(DetailsSuite, SoLineDetailCopyProducesIndependentDuplicate)
{
    SoLineDetail ld;
    ld.setLineIndex(5);
    ld.setPartIndex(1);
    SoDetail * copied = ld.copy();
    EXPECT_NE(copied, nullptr);
    if (copied) {
        SoLineDetail * cld = static_cast<SoLineDetail *>(copied);
        EXPECT_EQ(cld->getLineIndex(), 5);
        EXPECT_EQ(cld->getPartIndex(), 1);
    }
    delete copied;
}

// -----------------------------------------------------------------------
// SoRayPickAction: face detail extracted from SoIndexedFaceSet
// -----------------------------------------------------------------------

TEST(DetailsSuite, SoRayPickActionSoFaceDetailAttachedToPickedIFSPoint)
{
    //
    // Build a simple scene: a quad at z=0 facing the camera
    //   coords: (-1,-1,0), (1,-1,0), (1,1,0), (-1,1,0)
    //   face: 0, 1, 2, 3, -1
    //
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoCoordinate3 * coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoIndexedFaceSet * ifs = new SoIndexedFaceSet;
    int32_t indices[] = { 0, 1, 2, 3, -1 };
    ifs->coordIndex.setValues(0, 5, indices);
    root->addChild(ifs);

    // Shoot a ray from z=10 toward z=0 through the centre of the quad
    SbViewportRegion vp(256, 256);
    SoRayPickAction rpa(vp);
    rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f),
               SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    const SoPickedPoint * pp = rpa.getPickedPoint();
    EXPECT_NE(pp, nullptr);
    if (pp != nullptr) {
        const SoDetail * detail = pp->getDetail();
        EXPECT_NE(detail, nullptr);
        if (detail) EXPECT_TRUE(detail->isOfType(SoFaceDetail::getClassTypeId()));
    }
    root->unref();
}

TEST(DetailsSuite, SoRayPickActionFaceDetailFaceIndex0)
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoCoordinate3 * coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoIndexedFaceSet * ifs = new SoIndexedFaceSet;
    int32_t indices[] = { 0, 1, 2, 3, -1 };
    ifs->coordIndex.setValues(0, 5, indices);
    root->addChild(ifs);

    SbViewportRegion vp(256, 256);
    SoRayPickAction rpa(vp);
    rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f),
               SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    const SoPickedPoint * pp = rpa.getPickedPoint();
    EXPECT_NE(pp, nullptr);
    if (pp != nullptr) {
        const SoDetail * d = pp->getDetail();
        EXPECT_NE(d, nullptr);
        EXPECT_TRUE(d && d->isOfType(SoFaceDetail::getClassTypeId()));
        if (d && d->isOfType(SoFaceDetail::getClassTypeId())) {
            const SoFaceDetail * fd =
                static_cast<const SoFaceDetail *>(d);
            EXPECT_GE(fd->getFaceIndex(), 0);
        }
    }
    root->unref();
}

TEST(DetailsSuite, SoRayPickActionFaceDetailNumPoints0)
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoCoordinate3 * coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoIndexedFaceSet * ifs = new SoIndexedFaceSet;
    int32_t indices[] = { 0, 1, 2, 3, -1 };
    ifs->coordIndex.setValues(0, 5, indices);
    root->addChild(ifs);

    SbViewportRegion vp(256, 256);
    SoRayPickAction rpa(vp);
    rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f),
               SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    const SoPickedPoint * pp = rpa.getPickedPoint();
    EXPECT_NE(pp, nullptr);
    if (pp != nullptr) {
        const SoDetail * d = pp->getDetail();
        EXPECT_NE(d, nullptr);
        EXPECT_TRUE(d && d->isOfType(SoFaceDetail::getClassTypeId()));
        if (d && d->isOfType(SoFaceDetail::getClassTypeId())) {
            const SoFaceDetail * fd =
                static_cast<const SoFaceDetail *>(d);
            EXPECT_GT(fd->getNumPoints(), 0);
        }
    }
    root->unref();
}

// -----------------------------------------------------------------------
// SoRayPickAction: line detail from SoIndexedLineSet
// -----------------------------------------------------------------------

TEST(DetailsSuite, SoRayPickActionSoLineDetailAttachedToPickedILSPoint)
{
    // Two-point line segment along the ray axis
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoCoordinate3 * coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-5.0f, 0.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 5.0f, 0.0f, 0.0f));
    root->addChild(coords);

    SoIndexedLineSet * ils = new SoIndexedLineSet;
    int32_t indices[] = { 0, 1, -1 };
    ils->coordIndex.setValues(0, 3, indices);
    root->addChild(ils);

    // Ray from above, along Y axis — intersects the line at (0,0,0)
    SbViewportRegion vp(256, 256);
    SoRayPickAction rpa(vp);
    rpa.setRay(SbVec3f(0.0f, 10.0f, 0.0f),
               SbVec3f(0.0f, -1.0f, 0.0f),
               0.1f); // near radius for line picking
    rpa.apply(root);

    const SoPickedPoint * pp = rpa.getPickedPoint();
    EXPECT_NE(pp, nullptr);
    if (pp != nullptr) {
        const SoDetail * d = pp->getDetail();
        EXPECT_NE(d, nullptr);
        if (d) EXPECT_TRUE(d->isOfType(SoLineDetail::getClassTypeId()));
    }
    root->unref();
}
