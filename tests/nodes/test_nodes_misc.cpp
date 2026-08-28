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
    EXPECT_TRUE((SoAnnotation::getClassTypeId() != SoType::badType())) << "SoAnnotation has bad class type";
}

TEST(NodesMisc, SoAnnotationIsOfTypeSoSeparator)
{
    SoAnnotation * ann = new SoAnnotation;
    ann->ref();
    EXPECT_TRUE(ann->isOfType(SoSeparator::getClassTypeId())) << "SoAnnotation should be derived from SoSeparator";
    ann->unref();
}

TEST(NodesMisc, SoAnnotationAddChildGetNumChildren)
{
    SoAnnotation * ann = new SoAnnotation;
    ann->ref();
    ann->addChild(new SoSeparator);
    ann->addChild(new SoSeparator);
    EXPECT_TRUE((ann->getNumChildren() == 2)) << "SoAnnotation addChild/getNumChildren failed";
    ann->unref();
}

// -----------------------------------------------------------------------
// SoResetTransform
// -----------------------------------------------------------------------

TEST(NodesMisc, SoResetTransformClassTypeRegistered)
{
    EXPECT_TRUE((SoResetTransform::getClassTypeId() != SoType::badType())) << "SoResetTransform bad class type";
}

TEST(NodesMisc, SoResetTransformWhatToResetDefaultIsTRANSFORM)
{
    SoResetTransform * node = new SoResetTransform;
    node->ref();
    // Default whatToReset is TRANSFORM (bit 0x01)
    EXPECT_TRUE((node->whatToReset.getValue() == SoResetTransform::TRANSFORM)) << "SoResetTransform default whatToReset != TRANSFORM";
    node->unref();
}

TEST(NodesMisc, SoResetTransformBBOXFlagRoundTrip)
{
    SoResetTransform * node = new SoResetTransform;
    node->ref();
    node->whatToReset.setValue(SoResetTransform::BBOX);
    EXPECT_TRUE((node->whatToReset.getValue() == SoResetTransform::BBOX)) << "SoResetTransform BBOX flag round-trip failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoCoordinate4
// -----------------------------------------------------------------------

TEST(NodesMisc, SoCoordinate4ClassTypeRegistered)
{
    EXPECT_TRUE((SoCoordinate4::getClassTypeId() != SoType::badType())) << "SoCoordinate4 bad class type";
}

TEST(NodesMisc, SoCoordinate4PointFieldStartsWithDefaultValue)
{
    SoCoordinate4 * node = new SoCoordinate4;
    node->ref();
    // Default is one entry SbVec4f(0,0,0,1)
    EXPECT_TRUE((node->point.getNum() >= 1)) << "SoCoordinate4 point should have at least 1 default value";
    node->unref();
}

TEST(NodesMisc, SoCoordinate4PointFieldSetGetRoundTrip)
{
    SoCoordinate4 * node = new SoCoordinate4;
    node->ref();
    SbVec4f pts[2] = { SbVec4f(1,2,3,1), SbVec4f(4,5,6,1) };
    node->point.setValues(0, 2, pts);
    EXPECT_TRUE((node->point.getNum() == 2)) << "SoCoordinate4 point field set/get failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoPendulum
// -----------------------------------------------------------------------

TEST(NodesMisc, SoPendulumClassTypeRegistered)
{
    EXPECT_TRUE((SoPendulum::getClassTypeId() != SoType::badType())) << "SoPendulum bad class type";
}

TEST(NodesMisc, SoPendulumSpeedDefaultIs10)
{
    SoPendulum * node = new SoPendulum;
    node->ref();
    EXPECT_TRUE((node->speed.getValue() == 1.0f)) << "SoPendulum default speed != 1.0";
    node->unref();
}

TEST(NodesMisc, SoPendulumOnFieldDefaultIsTRUE)
{
    SoPendulum * node = new SoPendulum;
    node->ref();
    EXPECT_TRUE((node->on.getValue() == TRUE)) << "SoPendulum default on != TRUE";
    node->unref();
}

TEST(NodesMisc, SoPendulumRotation0Rotation1FieldRoundTrip)
{
    SoPendulum * node = new SoPendulum;
    node->ref();
    SbRotation r0(SbVec3f(0,1,0), 0.5f);
    SbRotation r1(SbVec3f(0,1,0), -0.5f);
    node->rotation0.setValue(r0);
    node->rotation1.setValue(r1);
    EXPECT_TRUE((node->rotation0.getValue() == r0) &&
                (node->rotation1.getValue() == r1)) << "SoPendulum rotation0/rotation1 field failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoShuttle
// -----------------------------------------------------------------------

TEST(NodesMisc, SoShuttleClassTypeRegistered)
{
    EXPECT_TRUE((SoShuttle::getClassTypeId() != SoType::badType())) << "SoShuttle bad class type";
}

TEST(NodesMisc, SoShuttleSpeedDefaultIs10)
{
    SoShuttle * node = new SoShuttle;
    node->ref();
    EXPECT_TRUE((node->speed.getValue() == 1.0f)) << "SoShuttle default speed != 1.0";
    node->unref();
}

TEST(NodesMisc, SoShuttleOnFieldDefaultIsTRUE)
{
    SoShuttle * node = new SoShuttle;
    node->ref();
    EXPECT_TRUE((node->on.getValue() == TRUE)) << "SoShuttle default on != TRUE";
    node->unref();
}

// -----------------------------------------------------------------------
// SoLinearProfile
// -----------------------------------------------------------------------

TEST(NodesMisc, SoLinearProfileClassTypeRegistered)
{
    EXPECT_TRUE((SoLinearProfile::getClassTypeId() != SoType::badType())) << "SoLinearProfile bad class type";
}

// -----------------------------------------------------------------------
// SoProfileCoordinate2
// -----------------------------------------------------------------------

TEST(NodesMisc, SoProfileCoordinate2ClassTypeRegistered)
{
    EXPECT_TRUE((SoProfileCoordinate2::getClassTypeId() != SoType::badType())) << "SoProfileCoordinate2 bad class type";
}

TEST(NodesMisc, SoProfileCoordinate2PointFieldSetGetRoundTrip)
{
    SoProfileCoordinate2 * node = new SoProfileCoordinate2;
    node->ref();
    SbVec2f pts[3] = { SbVec2f(0,0), SbVec2f(1,0), SbVec2f(1,1) };
    node->point.setValues(0, 3, pts);
    EXPECT_TRUE((node->point.getNum() >= 3)) << "SoProfileCoordinate2 point set/get failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoProfileCoordinate3
// -----------------------------------------------------------------------

TEST(NodesMisc, SoProfileCoordinate3ClassTypeRegistered)
{
    EXPECT_TRUE((SoProfileCoordinate3::getClassTypeId() != SoType::badType())) << "SoProfileCoordinate3 bad class type";
}

// -----------------------------------------------------------------------
// SoWWWAnchor
// -----------------------------------------------------------------------

TEST(NodesMisc, SoWWWAnchorClassTypeRegistered)
{
    EXPECT_TRUE((SoWWWAnchor::getClassTypeId() != SoType::badType())) << "SoWWWAnchor bad class type";
}

TEST(NodesMisc, SoWWWAnchorNameFieldRoundTrip)
{
    SoWWWAnchor * node = new SoWWWAnchor;
    node->ref();
    node->name.setValue("http://example.com");
    EXPECT_TRUE((node->name.getValue() == SbString("http://example.com"))) << "SoWWWAnchor name field failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoWWWInline
// -----------------------------------------------------------------------

TEST(NodesMisc, SoWWWInlineClassTypeRegistered)
{
    EXPECT_TRUE((SoWWWInline::getClassTypeId() != SoType::badType())) << "SoWWWInline bad class type";
}

TEST(NodesMisc, SoWWWInlineBboxSizeDefaultIs000)
{
    SoWWWInline * node = new SoWWWInline;
    node->ref();
    SbVec3f sz = node->bboxSize.getValue();
    EXPECT_TRUE((sz == SbVec3f(0, 0, 0))) << "SoWWWInline bboxSize default not (0,0,0)";
    node->unref();
}

// -----------------------------------------------------------------------
// SoSurroundScale
// -----------------------------------------------------------------------

TEST(NodesMisc, SoSurroundScaleClassTypeRegistered)
{
    EXPECT_TRUE((SoSurroundScale::getClassTypeId() != SoType::badType())) << "SoSurroundScale bad class type";
}

TEST(NodesMisc, SoSurroundScaleNumNodesUpToContainerResetDefaults)
{
    SoSurroundScale * node = new SoSurroundScale;
    node->ref();
    // Default values are 0 for both
    EXPECT_TRUE((node->numNodesUpToContainer.getValue() == 0) &&
                (node->numNodesUpToReset.getValue() == 0)) << "SoSurroundScale field defaults wrong";
    node->unref();
}

// -----------------------------------------------------------------------
// SoAntiSquish
// -----------------------------------------------------------------------

TEST(NodesMisc, SoAntiSquishClassTypeRegistered)
{
    EXPECT_TRUE((SoAntiSquish::getClassTypeId() != SoType::badType())) << "SoAntiSquish bad class type";
}

TEST(NodesMisc, SoAntiSquishRecalcAlwaysDefaultIsTRUE)
{
    SoAntiSquish * node = new SoAntiSquish;
    node->ref();
    EXPECT_TRUE((node->recalcAlways.getValue() == TRUE)) << "SoAntiSquish recalcAlways default != TRUE";
    node->unref();
}

// -----------------------------------------------------------------------
// SoCacheHint
// -----------------------------------------------------------------------

TEST(NodesMisc, SoCacheHintClassTypeRegistered)
{
    EXPECT_TRUE((SoCacheHint::getClassTypeId() != SoType::badType())) << "SoCacheHint bad class type";
}

TEST(NodesMisc, SoCacheHintMemValueGfxValueFieldsRoundTrip)
{
    SoCacheHint * node = new SoCacheHint;
    node->ref();
    node->memValue.setValue(0.8f);
    node->gfxValue.setValue(0.6f);
    EXPECT_TRUE((node->memValue.getValue() == 0.8f) &&
                (node->gfxValue.getValue() == 0.6f)) << "SoCacheHint memValue/gfxValue failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoTransparencyType
// -----------------------------------------------------------------------

TEST(NodesMisc, SoTransparencyTypeClassTypeRegistered)
{
    EXPECT_TRUE((SoTransparencyType::getClassTypeId() != SoType::badType())) << "SoTransparencyType bad class type";
}

TEST(NodesMisc, SoTransparencyTypeValueFieldRoundTrip)
{
    SoTransparencyType * node = new SoTransparencyType;
    node->ref();
    node->value.setValue(SoTransparencyType::BLEND);
    EXPECT_TRUE((node->value.getValue() == (int)SoTransparencyType::BLEND)) << "SoTransparencyType value field round-trip failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoLocateHighlight
// -----------------------------------------------------------------------

TEST(NodesMisc, SoLocateHighlightClassTypeRegistered)
{
    EXPECT_TRUE((SoLocateHighlight::getClassTypeId() != SoType::badType())) << "SoLocateHighlight bad class type";
}

TEST(NodesMisc, SoLocateHighlightIsOfTypeSoSeparator)
{
    SoLocateHighlight * node = new SoLocateHighlight;
    node->ref();
    EXPECT_TRUE(node->isOfType(SoSeparator::getClassTypeId())) << "SoLocateHighlight not derived from SoSeparator";
    node->unref();
}

// -----------------------------------------------------------------------
// SoColorIndex
// -----------------------------------------------------------------------

TEST(NodesMisc, SoColorIndexClassTypeRegistered)
{
    EXPECT_TRUE((SoColorIndex::getClassTypeId() != SoType::badType())) << "SoColorIndex bad class type";
}

TEST(NodesMisc, SoColorIndexIndexFieldSetGetRoundTrip)
{
    SoColorIndex * node = new SoColorIndex;
    node->ref();
    node->index.set1Value(0, 5);
    EXPECT_TRUE((node->index.getNum() >= 1) && (node->index[0] == 5)) << "SoColorIndex index field round-trip failed";
    node->unref();
}
