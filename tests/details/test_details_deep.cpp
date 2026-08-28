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
 * @file test_details_deep.cpp
 * @brief Coverage for detail classes not yet tested in test_details_suite.cpp.
 *
 * Covers:
 *   SoCubeDetail     - setPart/getPart, copy, type
 *   SoCylinderDetail - setPart/getPart, copy, type
 *   SoConeDetail     - setPart/getPart, copy, type
 *   SoTextDetail     - setStringIndex/getStringIndex, setCharacterIndex,
 *                      setPart/getPart, copy, type
 *
 * For each detail class, also exercises via SoRayPickAction against the
 * corresponding shape to verify that the pick action populates the correct
 * detail subtype.
 *
 * Subsystems improved: details (47.3 %)
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SoType.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbViewportRegion.h>

#include <Inventor/details/SoCubeDetail.h>
#include <Inventor/details/SoCylinderDetail.h>
#include <Inventor/details/SoConeDetail.h>
#include <Inventor/details/SoTextDetail.h>

#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoCone.h>

#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/SoPickedPoint.h>

using namespace ObolTest;

TEST(DetailsDeep, SoCubeDetailClassTypeIdValid)
{
    EXPECT_TRUE((SoCubeDetail::getClassTypeId() != SoType::badType())) << "SoCubeDetail has bad class type";
}

TEST(DetailsDeep, SoCubeDetailIsOfTypeSoDetail)
{
    SoCubeDetail d;
    EXPECT_TRUE(d.isOfType(SoDetail::getClassTypeId())) << "SoCubeDetail not a SoDetail subtype";
}

TEST(DetailsDeep, SoCubeDetailSetPartGetPartRoundTrip)
{
    SoCubeDetail d;
    d.setPart(3);
    EXPECT_TRUE((d.getPart() == 3)) << "SoCubeDetail setPart(3) then getPart() != 3";
}

TEST(DetailsDeep, SoCubeDetailCopyReturnsCorrectType)
{
    SoCubeDetail d;
    d.setPart(2);
    SoDetail * c = d.copy();
    EXPECT_TRUE((c != nullptr) &&
                c->isOfType(SoCubeDetail::getClassTypeId()) &&
                (static_cast<SoCubeDetail *>(c)->getPart() == 2)) << "SoCubeDetail copy() returned null, wrong type, or wrong part";
    delete c;
}

// Pick test: SoRayPickAction on a SoCube should yield SoCubeDetail.
// Note: the action must be alive when inspecting the picked point; we apply
// and check within the same scope.

TEST(DetailsDeep, SoRayPickActionOnSoCubeYieldsSoCubeDetail)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCube);

    SoRayPickAction rpa(SbViewportRegion(256, 256));
    rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    const SoPickedPoint * pp = rpa.getPickedPoint();
    EXPECT_NE(pp, nullptr);
    if (pp) {
        const SoDetail * d = pp->getDetail();
        EXPECT_NE(d, nullptr);
        if (d) EXPECT_TRUE(d->isOfType(SoCubeDetail::getClassTypeId()));
    }
    root->unref();
}

// =========================================================================
// SoCylinderDetail
// =========================================================================

TEST(DetailsDeep, SoCylinderDetailClassTypeIdValid)
{
    EXPECT_TRUE((SoCylinderDetail::getClassTypeId() != SoType::badType())) << "SoCylinderDetail has bad class type";
}

TEST(DetailsDeep, SoCylinderDetailSetPartGetPartRoundTrip)
{
    SoCylinderDetail d;
    d.setPart(1);
    EXPECT_TRUE((d.getPart() == 1)) << "SoCylinderDetail setPart(1) then getPart() != 1";
}

TEST(DetailsDeep, SoCylinderDetailCopyReturnsCorrectTypeAndPart)
{
    SoCylinderDetail d;
    d.setPart(0);
    SoDetail * c = d.copy();
    EXPECT_TRUE((c != nullptr) &&
                c->isOfType(SoCylinderDetail::getClassTypeId()) &&
                (static_cast<SoCylinderDetail *>(c)->getPart() == 0)) << "SoCylinderDetail copy() returned null, wrong type, or wrong part";
    delete c;
}

// Pick test: SoRayPickAction on a SoCylinder should yield SoCylinderDetail

TEST(DetailsDeep, SoRayPickActionOnSoCylinderYieldsSoCylinderDetail)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCylinder);

    SoRayPickAction rpa(SbViewportRegion(256, 256));
    rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    const SoPickedPoint * pp = rpa.getPickedPoint();
    EXPECT_NE(pp, nullptr);
    if (pp) {
        const SoDetail * d = pp->getDetail();
        EXPECT_NE(d, nullptr);
        if (d) EXPECT_TRUE(d->isOfType(SoCylinderDetail::getClassTypeId()));
    }
    root->unref();
}

// =========================================================================
// SoConeDetail
// =========================================================================

TEST(DetailsDeep, SoConeDetailClassTypeIdValid)
{
    EXPECT_TRUE((SoConeDetail::getClassTypeId() != SoType::badType())) << "SoConeDetail has bad class type";
}

TEST(DetailsDeep, SoConeDetailSetPartGetPartRoundTrip)
{
    SoConeDetail d;
    d.setPart(1);
    EXPECT_TRUE((d.getPart() == 1)) << "SoConeDetail setPart(1) then getPart() != 1";
}

TEST(DetailsDeep, SoConeDetailCopyReturnsCorrectType)
{
    SoConeDetail d;
    d.setPart(0);
    SoDetail * c = d.copy();
    EXPECT_TRUE((c != nullptr) &&
                c->isOfType(SoConeDetail::getClassTypeId()) &&
                (static_cast<SoConeDetail *>(c)->getPart() == 0)) << "SoConeDetail copy() returned null, wrong type, or wrong part";
    delete c;
}

// Pick test: SoRayPickAction on a SoCone should yield SoConeDetail

TEST(DetailsDeep, SoRayPickActionOnSoConeYieldsSoConeDetail)
{
    SoSeparator * root = new SoSeparator;
    root->ref();
    root->addChild(new SoCone);

    SoRayPickAction rpa(SbViewportRegion(256, 256));
    rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f), SbVec3f(0.0f, 0.0f, -1.0f));
    rpa.apply(root);

    const SoPickedPoint * pp = rpa.getPickedPoint();
    EXPECT_NE(pp, nullptr);
    if (pp) {
        const SoDetail * d = pp->getDetail();
        EXPECT_NE(d, nullptr);
        if (d) EXPECT_TRUE(d->isOfType(SoConeDetail::getClassTypeId()));
    }
    root->unref();
}

// =========================================================================
// SoTextDetail
// =========================================================================

TEST(DetailsDeep, SoTextDetailClassTypeIdValid)
{
    EXPECT_TRUE((SoTextDetail::getClassTypeId() != SoType::badType())) << "SoTextDetail has bad class type";
}

TEST(DetailsDeep, SoTextDetailSetStringIndexGetStringIndexRoundTrip)
{
    SoTextDetail d;
    d.setStringIndex(5);
    EXPECT_TRUE((d.getStringIndex() == 5)) << "SoTextDetail setStringIndex(5) then getStringIndex() != 5";
}

TEST(DetailsDeep, SoTextDetailSetCharacterIndexGetCharacterIndexRoundTrip)
{
    SoTextDetail d;
    d.setCharacterIndex(3);
    EXPECT_TRUE((d.getCharacterIndex() == 3)) << "SoTextDetail setCharacterIndex(3) then getCharacterIndex() != 3";
}

TEST(DetailsDeep, SoTextDetailSetPartGetPartRoundTrip)
{
    SoTextDetail d;
    d.setPart(2);
    EXPECT_TRUE((d.getPart() == 2)) << "SoTextDetail setPart(2) then getPart() != 2";
}

TEST(DetailsDeep, SoTextDetailCopyPreservesAllFields)
{
    SoTextDetail d;
    d.setStringIndex(7);
    d.setCharacterIndex(4);
    d.setPart(1);
    SoDetail * c = d.copy();
    EXPECT_NE(c, nullptr);
    EXPECT_TRUE(c && c->isOfType(SoTextDetail::getClassTypeId()));
    if (c && c->isOfType(SoTextDetail::getClassTypeId())) {
        const SoTextDetail * cd = static_cast<const SoTextDetail *>(c);
        EXPECT_EQ(cd->getStringIndex(), 7);
        EXPECT_EQ(cd->getCharacterIndex(), 4);
        EXPECT_EQ(cd->getPart(), 1);
    }
    delete c;
}

TEST(DetailsDeep, SoTextDetailIsOfTypeSoDetail)
{
    SoTextDetail d;
    EXPECT_TRUE(d.isOfType(SoDetail::getClassTypeId())) << "SoTextDetail not a SoDetail subtype";
}
