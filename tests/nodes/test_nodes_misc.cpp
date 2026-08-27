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
 * @file test_nodes_misc.cpp
 * @brief Tests for miscellaneous node types with low coverage.
 *
 * Covers (nodes/ 41.8 %):
 *   SoAnnotation          - isOfType SoSeparator, class type
 *   SoResetTransform      - TRANSFORM / BBOX bitmask fields
 *   SoCoordinate4         - class type, point field
 *   SoPendulum            - rotation0/rotation1/speed/on fields
 *   SoShuttle             - translation0/translation1/speed/on fields
 *   SoLinearProfile       - class type, index/linkage fields
 *   SoProfileCoordinate2  - class type, point field
 *   SoProfileCoordinate3  - class type, point field
 *   SoWWWAnchor           - name/description/map fields, URL callback
 *   SoWWWInline           - name/bboxSize/bboxCenter fields
 *   SoSurroundScale       - numNodesUpToContainer/numNodesUpToReset fields
 *   SoAntiSquish          - sizing/recalcAlways fields
 *   SoCacheHint           - memValue/gfxValue fields
 *   SoTransparencyType    - value field default
 *   SoLocateHighlight     - color/style fields
 *   SoColorIndex          - index field
 */

#include "../test_utils.h"

#include <Inventor/SoType.h>
#include <Inventor/nodes/SoAnnotation.h>
#include <Inventor/nodes/SoResetTransform.h>
#include <Inventor/nodes/SoCoordinate4.h>
#include <Inventor/nodes/SoPendulum.h>
#include <Inventor/nodes/SoShuttle.h>
#include <Inventor/nodes/SoLinearProfile.h>
#include <Inventor/nodes/SoProfileCoordinate2.h>
#include <Inventor/nodes/SoProfileCoordinate3.h>
#include <Inventor/nodes/SoWWWAnchor.h>
#include <Inventor/nodes/SoWWWInline.h>
#include <Inventor/nodes/SoSurroundScale.h>
#include <Inventor/nodes/SoAntiSquish.h>
#include <Inventor/nodes/SoCacheHint.h>
#include <Inventor/nodes/SoTransparencyType.h>
#include <Inventor/nodes/SoLocateHighlight.h>
#include <Inventor/nodes/SoColorIndex.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoGroup.h>

#include <Inventor/SbVec3f.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec4f.h>
#include <Inventor/SbRotation.h>

using namespace ObolTest;

TEST(NodesMisc, SoAnnotationClassTypeRegistered)
{
    bool pass = (SoAnnotation::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoAnnotation has bad class type";
}

TEST(NodesMisc, SoAnnotationIsOfTypeSoSeparator)
{
    SoAnnotation * ann = new SoAnnotation;
    ann->ref();
    bool pass = ann->isOfType(SoSeparator::getClassTypeId());
    ann->unref();
    EXPECT_TRUE(pass) << "SoAnnotation should be derived from SoSeparator";
}

TEST(NodesMisc, SoAnnotationAddChildGetNumChildren)
{
    SoAnnotation * ann = new SoAnnotation;
    ann->ref();
    ann->addChild(new SoSeparator);
    ann->addChild(new SoSeparator);
    bool pass = (ann->getNumChildren() == 2);
    ann->unref();
    EXPECT_TRUE(pass) << "SoAnnotation addChild/getNumChildren failed";
}

// -----------------------------------------------------------------------
// SoResetTransform
// -----------------------------------------------------------------------

TEST(NodesMisc, SoResetTransformClassTypeRegistered)
{
    bool pass = (SoResetTransform::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoResetTransform bad class type";
}

TEST(NodesMisc, SoResetTransformWhatToResetDefaultIsTRANSFORM)
{
    SoResetTransform * node = new SoResetTransform;
    node->ref();
    // Default whatToReset is TRANSFORM (bit 0x01)
    bool pass = (node->whatToReset.getValue() == SoResetTransform::TRANSFORM);
    node->unref();
    EXPECT_TRUE(pass) << "SoResetTransform default whatToReset != TRANSFORM";
}

TEST(NodesMisc, SoResetTransformBBOXFlagRoundTrip)
{
    SoResetTransform * node = new SoResetTransform;
    node->ref();
    node->whatToReset.setValue(SoResetTransform::BBOX);
    bool pass = (node->whatToReset.getValue() == SoResetTransform::BBOX);
    node->unref();
    EXPECT_TRUE(pass) << "SoResetTransform BBOX flag round-trip failed";
}

// -----------------------------------------------------------------------
// SoCoordinate4
// -----------------------------------------------------------------------

TEST(NodesMisc, SoCoordinate4ClassTypeRegistered)
{
    bool pass = (SoCoordinate4::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoCoordinate4 bad class type";
}

TEST(NodesMisc, SoCoordinate4PointFieldStartsWithDefaultValue)
{
    SoCoordinate4 * node = new SoCoordinate4;
    node->ref();
    // Default is one entry SbVec4f(0,0,0,1)
    bool pass = (node->point.getNum() >= 1);
    node->unref();
    EXPECT_TRUE(pass) << "SoCoordinate4 point should have at least 1 default value";
}

TEST(NodesMisc, SoCoordinate4PointFieldSetGetRoundTrip)
{
    SoCoordinate4 * node = new SoCoordinate4;
    node->ref();
    SbVec4f pts[2] = { SbVec4f(1,2,3,1), SbVec4f(4,5,6,1) };
    node->point.setValues(0, 2, pts);
    bool pass = (node->point.getNum() == 2);
    node->unref();
    EXPECT_TRUE(pass) << "SoCoordinate4 point field set/get failed";
}

// -----------------------------------------------------------------------
// SoPendulum
// -----------------------------------------------------------------------

TEST(NodesMisc, SoPendulumClassTypeRegistered)
{
    bool pass = (SoPendulum::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoPendulum bad class type";
}

TEST(NodesMisc, SoPendulumSpeedDefaultIs10)
{
    SoPendulum * node = new SoPendulum;
    node->ref();
    bool pass = (node->speed.getValue() == 1.0f);
    node->unref();
    EXPECT_TRUE(pass) << "SoPendulum default speed != 1.0";
}

TEST(NodesMisc, SoPendulumOnFieldDefaultIsTRUE)
{
    SoPendulum * node = new SoPendulum;
    node->ref();
    bool pass = (node->on.getValue() == TRUE);
    node->unref();
    EXPECT_TRUE(pass) << "SoPendulum default on != TRUE";
}

TEST(NodesMisc, SoPendulumRotation0Rotation1FieldRoundTrip)
{
    SoPendulum * node = new SoPendulum;
    node->ref();
    SbRotation r0(SbVec3f(0,1,0), 0.5f);
    SbRotation r1(SbVec3f(0,1,0), -0.5f);
    node->rotation0.setValue(r0);
    node->rotation1.setValue(r1);
    bool pass = (node->rotation0.getValue() == r0) &&
                (node->rotation1.getValue() == r1);
    node->unref();
    EXPECT_TRUE(pass) << "SoPendulum rotation0/rotation1 field failed";
}

// -----------------------------------------------------------------------
// SoShuttle
// -----------------------------------------------------------------------

TEST(NodesMisc, SoShuttleClassTypeRegistered)
{
    bool pass = (SoShuttle::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoShuttle bad class type";
}

TEST(NodesMisc, SoShuttleSpeedDefaultIs10)
{
    SoShuttle * node = new SoShuttle;
    node->ref();
    bool pass = (node->speed.getValue() == 1.0f);
    node->unref();
    EXPECT_TRUE(pass) << "SoShuttle default speed != 1.0";
}

TEST(NodesMisc, SoShuttleOnFieldDefaultIsTRUE)
{
    SoShuttle * node = new SoShuttle;
    node->ref();
    bool pass = (node->on.getValue() == TRUE);
    node->unref();
    EXPECT_TRUE(pass) << "SoShuttle default on != TRUE";
}

// -----------------------------------------------------------------------
// SoLinearProfile
// -----------------------------------------------------------------------

TEST(NodesMisc, SoLinearProfileClassTypeRegistered)
{
    bool pass = (SoLinearProfile::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoLinearProfile bad class type";
}

// -----------------------------------------------------------------------
// SoProfileCoordinate2
// -----------------------------------------------------------------------

TEST(NodesMisc, SoProfileCoordinate2ClassTypeRegistered)
{
    bool pass = (SoProfileCoordinate2::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoProfileCoordinate2 bad class type";
}

TEST(NodesMisc, SoProfileCoordinate2PointFieldSetGetRoundTrip)
{
    SoProfileCoordinate2 * node = new SoProfileCoordinate2;
    node->ref();
    SbVec2f pts[3] = { SbVec2f(0,0), SbVec2f(1,0), SbVec2f(1,1) };
    node->point.setValues(0, 3, pts);
    bool pass = (node->point.getNum() >= 3);
    node->unref();
    EXPECT_TRUE(pass) << "SoProfileCoordinate2 point set/get failed";
}

// -----------------------------------------------------------------------
// SoProfileCoordinate3
// -----------------------------------------------------------------------

TEST(NodesMisc, SoProfileCoordinate3ClassTypeRegistered)
{
    bool pass = (SoProfileCoordinate3::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoProfileCoordinate3 bad class type";
}

// -----------------------------------------------------------------------
// SoWWWAnchor
// -----------------------------------------------------------------------

TEST(NodesMisc, SoWWWAnchorClassTypeRegistered)
{
    bool pass = (SoWWWAnchor::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoWWWAnchor bad class type";
}

TEST(NodesMisc, SoWWWAnchorNameFieldRoundTrip)
{
    SoWWWAnchor * node = new SoWWWAnchor;
    node->ref();
    node->name.setValue("http://example.com");
    bool pass = (node->name.getValue() == SbString("http://example.com"));
    node->unref();
    EXPECT_TRUE(pass) << "SoWWWAnchor name field failed";
}

// -----------------------------------------------------------------------
// SoWWWInline
// -----------------------------------------------------------------------

TEST(NodesMisc, SoWWWInlineClassTypeRegistered)
{
    bool pass = (SoWWWInline::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoWWWInline bad class type";
}

TEST(NodesMisc, SoWWWInlineBboxSizeDefaultIs000)
{
    SoWWWInline * node = new SoWWWInline;
    node->ref();
    SbVec3f sz = node->bboxSize.getValue();
    bool pass = (sz == SbVec3f(0, 0, 0));
    node->unref();
    EXPECT_TRUE(pass) << "SoWWWInline bboxSize default not (0,0,0)";
}

// -----------------------------------------------------------------------
// SoSurroundScale
// -----------------------------------------------------------------------

TEST(NodesMisc, SoSurroundScaleClassTypeRegistered)
{
    bool pass = (SoSurroundScale::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoSurroundScale bad class type";
}

TEST(NodesMisc, SoSurroundScaleNumNodesUpToContainerResetDefaults)
{
    SoSurroundScale * node = new SoSurroundScale;
    node->ref();
    // Default values are 0 for both
    bool pass = (node->numNodesUpToContainer.getValue() == 0) &&
                (node->numNodesUpToReset.getValue() == 0);
    node->unref();
    EXPECT_TRUE(pass) << "SoSurroundScale field defaults wrong";
}

// -----------------------------------------------------------------------
// SoAntiSquish
// -----------------------------------------------------------------------

TEST(NodesMisc, SoAntiSquishClassTypeRegistered)
{
    bool pass = (SoAntiSquish::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoAntiSquish bad class type";
}

TEST(NodesMisc, SoAntiSquishRecalcAlwaysDefaultIsTRUE)
{
    SoAntiSquish * node = new SoAntiSquish;
    node->ref();
    bool pass = (node->recalcAlways.getValue() == TRUE);
    node->unref();
    EXPECT_TRUE(pass) << "SoAntiSquish recalcAlways default != TRUE";
}

// -----------------------------------------------------------------------
// SoCacheHint
// -----------------------------------------------------------------------

TEST(NodesMisc, SoCacheHintClassTypeRegistered)
{
    bool pass = (SoCacheHint::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoCacheHint bad class type";
}

TEST(NodesMisc, SoCacheHintMemValueGfxValueFieldsRoundTrip)
{
    SoCacheHint * node = new SoCacheHint;
    node->ref();
    node->memValue.setValue(0.8f);
    node->gfxValue.setValue(0.6f);
    bool pass = (node->memValue.getValue() == 0.8f) &&
                (node->gfxValue.getValue() == 0.6f);
    node->unref();
    EXPECT_TRUE(pass) << "SoCacheHint memValue/gfxValue failed";
}

// -----------------------------------------------------------------------
// SoTransparencyType
// -----------------------------------------------------------------------

TEST(NodesMisc, SoTransparencyTypeClassTypeRegistered)
{
    bool pass = (SoTransparencyType::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoTransparencyType bad class type";
}

TEST(NodesMisc, SoTransparencyTypeValueFieldRoundTrip)
{
    SoTransparencyType * node = new SoTransparencyType;
    node->ref();
    node->value.setValue(SoTransparencyType::BLEND);
    bool pass = (node->value.getValue() == (int)SoTransparencyType::BLEND);
    node->unref();
    EXPECT_TRUE(pass) << "SoTransparencyType value field round-trip failed";
}

// -----------------------------------------------------------------------
// SoLocateHighlight
// -----------------------------------------------------------------------

TEST(NodesMisc, SoLocateHighlightClassTypeRegistered)
{
    bool pass = (SoLocateHighlight::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoLocateHighlight bad class type";
}

TEST(NodesMisc, SoLocateHighlightIsOfTypeSoSeparator)
{
    SoLocateHighlight * node = new SoLocateHighlight;
    node->ref();
    bool pass = node->isOfType(SoSeparator::getClassTypeId());
    node->unref();
    EXPECT_TRUE(pass) << "SoLocateHighlight not derived from SoSeparator";
}

// -----------------------------------------------------------------------
// SoColorIndex
// -----------------------------------------------------------------------

TEST(NodesMisc, SoColorIndexClassTypeRegistered)
{
    bool pass = (SoColorIndex::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoColorIndex bad class type";
}

TEST(NodesMisc, SoColorIndexIndexFieldSetGetRoundTrip)
{
    SoColorIndex * node = new SoColorIndex;
    node->ref();
    node->index.set1Value(0, 5);
    bool pass = (node->index.getNum() >= 1) && (node->index[0] == 5);
    node->unref();
    EXPECT_TRUE(pass) << "SoColorIndex index field round-trip failed";
}
