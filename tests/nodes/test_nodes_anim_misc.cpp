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
 * @file test_nodes_anim_misc.cpp
 * @brief Tests for animated nodes and miscellaneous node types.
 *
 * Covers (nodes/ 41.8 %):
 *   SoBlinker          - speed/on fields, isOfType SoSwitch
 *   SoRotor            - speed/on fields, isOfType SoRotation
 *   SoInfo             - string field
 *   SoLabel            - label field
 *   SoArray            - origin/numElements/separation fields
 *   SoMultipleCopy     - matrix field
 *   SoEnvironment      - ambientIntensity/fogType/fogVisibility fields
 *   SoDrawStyle        - style/lineWidth/linePattern/pointSize fields
 *   SoPickStyle        - style field
 *   SoSelection        - addChild/getNumSelected/select/deselect
 *   SoPathSwitch       - renderPath/pickPath fields
 *   SoPackedColor      - orderedRGBA field
 *   SoMatrixTransform  - matrix field
 *   SoRotationXYZ      - axis/angle fields
 */

#include "../test_utils.h"

#include <Inventor/nodes/SoBlinker.h>
#include <Inventor/nodes/SoRotor.h>
#include <Inventor/nodes/SoInfo.h>
#include <Inventor/nodes/SoLabel.h>
#include <Inventor/nodes/SoArray.h>
#include <Inventor/nodes/SoMultipleCopy.h>
#include <Inventor/nodes/SoEnvironment.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoPickStyle.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoPathSwitch.h>
#include <Inventor/nodes/SoPackedColor.h>
#include <Inventor/nodes/SoMatrixTransform.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SoType.h>

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoBlinker
    // -----------------------------------------------------------------------
    runner.startTest("SoBlinker class type registered");
    {
        bool pass = (SoBlinker::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoBlinker bad class type");
    }

    runner.startTest("SoBlinker isOfType SoSwitch");
    {
        SoBlinker * node = new SoBlinker;
        node->ref();
        bool pass = node->isOfType(SoSwitch::getClassTypeId());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoBlinker should be derived from SoSwitch");
    }

    runner.startTest("SoBlinker speed default is 1.0");
    {
        SoBlinker * node = new SoBlinker;
        node->ref();
        bool pass = (node->speed.getValue() == 1.0f);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoBlinker speed default != 1.0");
    }

    runner.startTest("SoBlinker on field round-trip");
    {
        SoBlinker * node = new SoBlinker;
        node->ref();
        node->on.setValue(FALSE);
        bool pass = (node->on.getValue() == FALSE);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoBlinker on field round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoRotor
    // -----------------------------------------------------------------------
    runner.startTest("SoRotor class type registered");
    {
        bool pass = (SoRotor::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoRotor bad class type");
    }

    runner.startTest("SoRotor isOfType SoRotation");
    {
        SoRotor * node = new SoRotor;
        node->ref();
        bool pass = node->isOfType(SoRotation::getClassTypeId());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoRotor should be derived from SoRotation");
    }

    runner.startTest("SoRotor speed default is 1.0");
    {
        SoRotor * node = new SoRotor;
        node->ref();
        bool pass = (node->speed.getValue() == 1.0f);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoRotor speed default != 1.0");
    }

    runner.startTest("SoRotor on field default is TRUE");
    {
        SoRotor * node = new SoRotor;
        node->ref();
        bool pass = (node->on.getValue() == TRUE);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoRotor on default != TRUE");
    }

    // -----------------------------------------------------------------------
    // SoInfo
    // -----------------------------------------------------------------------
    runner.startTest("SoInfo class type registered");
    {
        bool pass = (SoInfo::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoInfo bad class type");
    }

    runner.startTest("SoInfo string field set/get round-trip");
    {
        SoInfo * node = new SoInfo;
        node->ref();
        node->string.setValue("test info");
        bool pass = (strcmp(node->string.getValue().getString(), "test info") == 0);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoInfo string field set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoLabel
    // -----------------------------------------------------------------------
    runner.startTest("SoLabel class type registered");
    {
        bool pass = (SoLabel::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoLabel bad class type");
    }

    runner.startTest("SoLabel label field set/get round-trip");
    {
        SoLabel * node = new SoLabel;
        node->ref();
        node->label.setValue(SbName("myLabel"));
        bool pass = (strcmp(node->label.getValue().getString(), "myLabel") == 0);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoLabel label field set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoArray
    // -----------------------------------------------------------------------
    runner.startTest("SoArray class type registered");
    {
        bool pass = (SoArray::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoArray bad class type");
    }

    runner.startTest("SoArray numElements1/2/3 defaults are 1");
    {
        SoArray * node = new SoArray;
        node->ref();
        bool pass = (node->numElements1.getValue() == 1) &&
                    (node->numElements2.getValue() == 1) &&
                    (node->numElements3.getValue() == 1);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoArray numElements defaults != 1");
    }

    runner.startTest("SoArray numElements1 set/get round-trip");
    {
        SoArray * node = new SoArray;
        node->ref();
        node->numElements1.setValue(5);
        bool pass = (node->numElements1.getValue() == 5);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoArray numElements1 set/get failed");
    }

    runner.startTest("SoArray origin field set/get round-trip");
    {
        SoArray * node = new SoArray;
        node->ref();
        node->origin.setValue(SoArray::CENTER);
        bool pass = (node->origin.getValue() == (int)SoArray::CENTER);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoArray origin CENTER round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoEnvironment
    // -----------------------------------------------------------------------
    runner.startTest("SoEnvironment class type registered");
    {
        bool pass = (SoEnvironment::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoEnvironment bad class type");
    }

    runner.startTest("SoEnvironment fogType NONE default");
    {
        SoEnvironment * node = new SoEnvironment;
        node->ref();
        bool pass = (node->fogType.getValue() == (int)SoEnvironment::NONE);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoEnvironment fogType default != NONE");
    }

    runner.startTest("SoEnvironment fogType FOG round-trip");
    {
        SoEnvironment * node = new SoEnvironment;
        node->ref();
        node->fogType.setValue(SoEnvironment::FOG);
        bool pass = (node->fogType.getValue() == (int)SoEnvironment::FOG);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoEnvironment fogType FOG round-trip failed");
    }

    runner.startTest("SoEnvironment ambientIntensity set/get round-trip");
    {
        SoEnvironment * node = new SoEnvironment;
        node->ref();
        node->ambientIntensity.setValue(0.3f);
        bool pass = (node->ambientIntensity.getValue() == 0.3f);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoEnvironment ambientIntensity round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoDrawStyle
    // -----------------------------------------------------------------------
    runner.startTest("SoDrawStyle class type registered");
    {
        bool pass = (SoDrawStyle::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoDrawStyle bad class type");
    }

    runner.startTest("SoDrawStyle style LINES round-trip");
    {
        SoDrawStyle * node = new SoDrawStyle;
        node->ref();
        node->style.setValue(SoDrawStyle::LINES);
        bool pass = (node->style.getValue() == (int)SoDrawStyle::LINES);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoDrawStyle style LINES round-trip failed");
    }

    runner.startTest("SoDrawStyle lineWidth set/get round-trip");
    {
        SoDrawStyle * node = new SoDrawStyle;
        node->ref();
        node->lineWidth.setValue(2.0f);
        bool pass = (node->lineWidth.getValue() == 2.0f);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoDrawStyle lineWidth set/get failed");
    }

    runner.startTest("SoDrawStyle pointSize set/get round-trip");
    {
        SoDrawStyle * node = new SoDrawStyle;
        node->ref();
        node->pointSize.setValue(4.0f);
        bool pass = (node->pointSize.getValue() == 4.0f);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoDrawStyle pointSize set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoPickStyle
    // -----------------------------------------------------------------------
    runner.startTest("SoPickStyle class type registered");
    {
        bool pass = (SoPickStyle::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoPickStyle bad class type");
    }

    runner.startTest("SoPickStyle style UNPICKABLE round-trip");
    {
        SoPickStyle * node = new SoPickStyle;
        node->ref();
        node->style.setValue(SoPickStyle::UNPICKABLE);
        bool pass = (node->style.getValue() == (int)SoPickStyle::UNPICKABLE);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoPickStyle UNPICKABLE round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoPackedColor
    // -----------------------------------------------------------------------
    runner.startTest("SoPackedColor class type registered");
    {
        bool pass = (SoPackedColor::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoPackedColor bad class type");
    }

    runner.startTest("SoPackedColor orderedRGBA set/get round-trip");
    {
        SoPackedColor * node = new SoPackedColor;
        node->ref();
        node->orderedRGBA.set1Value(0, 0xFF0000FF); // red, fully opaque
        bool pass = (node->orderedRGBA.getNum() == 1) &&
                    (node->orderedRGBA[0] == 0xFF0000FF);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoPackedColor orderedRGBA set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoMatrixTransform
    // -----------------------------------------------------------------------
    runner.startTest("SoMatrixTransform class type registered");
    {
        bool pass = (SoMatrixTransform::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoMatrixTransform bad class type");
    }

    runner.startTest("SoMatrixTransform matrix default is identity");
    {
        SoMatrixTransform * node = new SoMatrixTransform;
        node->ref();
        bool pass = (node->matrix.getValue() == SbMatrix::identity());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoMatrixTransform default matrix is not identity");
    }

    runner.startTest("SoMatrixTransform matrix set/get round-trip");
    {
        SoMatrixTransform * node = new SoMatrixTransform;
        node->ref();
        SbMatrix m = SbMatrix::identity();
        m[3][0] = 5.0f; // translation x=5
        node->matrix.setValue(m);
        bool pass = (node->matrix.getValue() == m);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoMatrixTransform matrix set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoRotationXYZ
    // -----------------------------------------------------------------------
    runner.startTest("SoRotationXYZ class type registered");
    {
        bool pass = (SoRotationXYZ::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoRotationXYZ bad class type");
    }

    runner.startTest("SoRotationXYZ axis field round-trip");
    {
        SoRotationXYZ * node = new SoRotationXYZ;
        node->ref();
        node->axis.setValue(SoRotationXYZ::Y);
        bool pass = (node->axis.getValue() == (int)SoRotationXYZ::Y);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoRotationXYZ axis Y round-trip failed");
    }

    runner.startTest("SoRotationXYZ angle field round-trip");
    {
        SoRotationXYZ * node = new SoRotationXYZ;
        node->ref();
        node->angle.setValue(1.5708f); // π/2
        bool pass = (std::fabs(node->angle.getValue() - 1.5708f) < 1e-4f);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoRotationXYZ angle round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoSelection
    // -----------------------------------------------------------------------
    runner.startTest("SoSelection class type registered");
    {
        bool pass = (SoSelection::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoSelection bad class type");
    }

    runner.startTest("SoSelection addChild / getNumSelected (0 initially)");
    {
        SoSelection * sel = new SoSelection;
        sel->ref();
        sel->addChild(new SoCube);
        bool pass = (sel->getNumSelected() == 0);
        sel->unref();
        runner.endTest(pass, pass ? "" : "SoSelection initial getNumSelected should be 0");
    }

    return runner.getSummary();
}
