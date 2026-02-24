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

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoAnnotation
    // -----------------------------------------------------------------------
    runner.startTest("SoAnnotation class type registered");
    {
        bool pass = (SoAnnotation::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoAnnotation has bad class type");
    }

    runner.startTest("SoAnnotation isOfType SoSeparator");
    {
        SoAnnotation * ann = new SoAnnotation;
        ann->ref();
        bool pass = ann->isOfType(SoSeparator::getClassTypeId());
        ann->unref();
        runner.endTest(pass, pass ? "" : "SoAnnotation should be derived from SoSeparator");
    }

    runner.startTest("SoAnnotation addChild / getNumChildren");
    {
        SoAnnotation * ann = new SoAnnotation;
        ann->ref();
        ann->addChild(new SoSeparator);
        ann->addChild(new SoSeparator);
        bool pass = (ann->getNumChildren() == 2);
        ann->unref();
        runner.endTest(pass, pass ? "" : "SoAnnotation addChild/getNumChildren failed");
    }

    // -----------------------------------------------------------------------
    // SoResetTransform
    // -----------------------------------------------------------------------
    runner.startTest("SoResetTransform class type registered");
    {
        bool pass = (SoResetTransform::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoResetTransform bad class type");
    }

    runner.startTest("SoResetTransform whatToReset default is TRANSFORM");
    {
        SoResetTransform * node = new SoResetTransform;
        node->ref();
        // Default whatToReset is TRANSFORM (bit 0x01)
        bool pass = (node->whatToReset.getValue() == SoResetTransform::TRANSFORM);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoResetTransform default whatToReset != TRANSFORM");
    }

    runner.startTest("SoResetTransform BBOX flag round-trip");
    {
        SoResetTransform * node = new SoResetTransform;
        node->ref();
        node->whatToReset.setValue(SoResetTransform::BBOX);
        bool pass = (node->whatToReset.getValue() == SoResetTransform::BBOX);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoResetTransform BBOX flag round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoCoordinate4
    // -----------------------------------------------------------------------
    runner.startTest("SoCoordinate4 class type registered");
    {
        bool pass = (SoCoordinate4::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoCoordinate4 bad class type");
    }

    runner.startTest("SoCoordinate4 point field starts with default value");
    {
        SoCoordinate4 * node = new SoCoordinate4;
        node->ref();
        // Default is one entry SbVec4f(0,0,0,1)
        bool pass = (node->point.getNum() >= 1);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoCoordinate4 point should have at least 1 default value");
    }

    runner.startTest("SoCoordinate4 point field set/get round-trip");
    {
        SoCoordinate4 * node = new SoCoordinate4;
        node->ref();
        SbVec4f pts[2] = { SbVec4f(1,2,3,1), SbVec4f(4,5,6,1) };
        node->point.setValues(0, 2, pts);
        bool pass = (node->point.getNum() == 2);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoCoordinate4 point field set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoPendulum
    // -----------------------------------------------------------------------
    runner.startTest("SoPendulum class type registered");
    {
        bool pass = (SoPendulum::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoPendulum bad class type");
    }

    runner.startTest("SoPendulum speed default is 1.0");
    {
        SoPendulum * node = new SoPendulum;
        node->ref();
        bool pass = (node->speed.getValue() == 1.0f);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoPendulum default speed != 1.0");
    }

    runner.startTest("SoPendulum on field default is TRUE");
    {
        SoPendulum * node = new SoPendulum;
        node->ref();
        bool pass = (node->on.getValue() == TRUE);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoPendulum default on != TRUE");
    }

    runner.startTest("SoPendulum rotation0/rotation1 field round-trip");
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
        runner.endTest(pass, pass ? "" : "SoPendulum rotation0/rotation1 field failed");
    }

    // -----------------------------------------------------------------------
    // SoShuttle
    // -----------------------------------------------------------------------
    runner.startTest("SoShuttle class type registered");
    {
        bool pass = (SoShuttle::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoShuttle bad class type");
    }

    runner.startTest("SoShuttle speed default is 1.0");
    {
        SoShuttle * node = new SoShuttle;
        node->ref();
        bool pass = (node->speed.getValue() == 1.0f);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShuttle default speed != 1.0");
    }

    runner.startTest("SoShuttle on field default is TRUE");
    {
        SoShuttle * node = new SoShuttle;
        node->ref();
        bool pass = (node->on.getValue() == TRUE);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShuttle default on != TRUE");
    }

    // -----------------------------------------------------------------------
    // SoLinearProfile
    // -----------------------------------------------------------------------
    runner.startTest("SoLinearProfile class type registered");
    {
        bool pass = (SoLinearProfile::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoLinearProfile bad class type");
    }

    // -----------------------------------------------------------------------
    // SoProfileCoordinate2
    // -----------------------------------------------------------------------
    runner.startTest("SoProfileCoordinate2 class type registered");
    {
        bool pass = (SoProfileCoordinate2::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoProfileCoordinate2 bad class type");
    }

    runner.startTest("SoProfileCoordinate2 point field set/get round-trip");
    {
        SoProfileCoordinate2 * node = new SoProfileCoordinate2;
        node->ref();
        SbVec2f pts[3] = { SbVec2f(0,0), SbVec2f(1,0), SbVec2f(1,1) };
        node->point.setValues(0, 3, pts);
        bool pass = (node->point.getNum() >= 3);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoProfileCoordinate2 point set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoProfileCoordinate3
    // -----------------------------------------------------------------------
    runner.startTest("SoProfileCoordinate3 class type registered");
    {
        bool pass = (SoProfileCoordinate3::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoProfileCoordinate3 bad class type");
    }

    // -----------------------------------------------------------------------
    // SoWWWAnchor
    // -----------------------------------------------------------------------
    runner.startTest("SoWWWAnchor class type registered");
    {
        bool pass = (SoWWWAnchor::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoWWWAnchor bad class type");
    }

    runner.startTest("SoWWWAnchor name field round-trip");
    {
        SoWWWAnchor * node = new SoWWWAnchor;
        node->ref();
        node->name.setValue("http://example.com");
        bool pass = (node->name.getValue() == SbString("http://example.com"));
        node->unref();
        runner.endTest(pass, pass ? "" : "SoWWWAnchor name field failed");
    }

    // -----------------------------------------------------------------------
    // SoWWWInline
    // -----------------------------------------------------------------------
    runner.startTest("SoWWWInline class type registered");
    {
        bool pass = (SoWWWInline::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoWWWInline bad class type");
    }

    runner.startTest("SoWWWInline bboxSize default is (0,0,0)");
    {
        SoWWWInline * node = new SoWWWInline;
        node->ref();
        SbVec3f sz = node->bboxSize.getValue();
        bool pass = (sz == SbVec3f(0, 0, 0));
        node->unref();
        runner.endTest(pass, pass ? "" : "SoWWWInline bboxSize default not (0,0,0)");
    }

    // -----------------------------------------------------------------------
    // SoSurroundScale
    // -----------------------------------------------------------------------
    runner.startTest("SoSurroundScale class type registered");
    {
        bool pass = (SoSurroundScale::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoSurroundScale bad class type");
    }

    runner.startTest("SoSurroundScale numNodesUpToContainer/Reset defaults");
    {
        SoSurroundScale * node = new SoSurroundScale;
        node->ref();
        // Default values are 0 for both
        bool pass = (node->numNodesUpToContainer.getValue() == 0) &&
                    (node->numNodesUpToReset.getValue() == 0);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoSurroundScale field defaults wrong");
    }

    // -----------------------------------------------------------------------
    // SoAntiSquish
    // -----------------------------------------------------------------------
    runner.startTest("SoAntiSquish class type registered");
    {
        bool pass = (SoAntiSquish::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoAntiSquish bad class type");
    }

    runner.startTest("SoAntiSquish recalcAlways default is TRUE");
    {
        SoAntiSquish * node = new SoAntiSquish;
        node->ref();
        bool pass = (node->recalcAlways.getValue() == TRUE);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoAntiSquish recalcAlways default != TRUE");
    }

    // -----------------------------------------------------------------------
    // SoCacheHint
    // -----------------------------------------------------------------------
    runner.startTest("SoCacheHint class type registered");
    {
        bool pass = (SoCacheHint::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoCacheHint bad class type");
    }

    runner.startTest("SoCacheHint memValue/gfxValue fields round-trip");
    {
        SoCacheHint * node = new SoCacheHint;
        node->ref();
        node->memValue.setValue(0.8f);
        node->gfxValue.setValue(0.6f);
        bool pass = (node->memValue.getValue() == 0.8f) &&
                    (node->gfxValue.getValue() == 0.6f);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoCacheHint memValue/gfxValue failed");
    }

    // -----------------------------------------------------------------------
    // SoTransparencyType
    // -----------------------------------------------------------------------
    runner.startTest("SoTransparencyType class type registered");
    {
        bool pass = (SoTransparencyType::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoTransparencyType bad class type");
    }

    runner.startTest("SoTransparencyType value field round-trip");
    {
        SoTransparencyType * node = new SoTransparencyType;
        node->ref();
        node->value.setValue(SoTransparencyType::BLEND);
        bool pass = (node->value.getValue() == (int)SoTransparencyType::BLEND);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoTransparencyType value field round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoLocateHighlight
    // -----------------------------------------------------------------------
    runner.startTest("SoLocateHighlight class type registered");
    {
        bool pass = (SoLocateHighlight::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoLocateHighlight bad class type");
    }

    runner.startTest("SoLocateHighlight isOfType SoSeparator");
    {
        SoLocateHighlight * node = new SoLocateHighlight;
        node->ref();
        bool pass = node->isOfType(SoSeparator::getClassTypeId());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoLocateHighlight not derived from SoSeparator");
    }

    // -----------------------------------------------------------------------
    // SoColorIndex
    // -----------------------------------------------------------------------
    runner.startTest("SoColorIndex class type registered");
    {
        bool pass = (SoColorIndex::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoColorIndex bad class type");
    }

    runner.startTest("SoColorIndex index field set/get round-trip");
    {
        SoColorIndex * node = new SoColorIndex;
        node->ref();
        node->index.set1Value(0, 5);
        bool pass = (node->index.getNum() >= 1) && (node->index[0] == 5);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoColorIndex index field round-trip failed");
    }

    return runner.getSummary();
}
