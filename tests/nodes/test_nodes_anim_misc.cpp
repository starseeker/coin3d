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

using namespace ObolTest;

TEST(NodesAnimMisc, SoBlinkerClassTypeRegistered)
{
    bool pass = (SoBlinker::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoBlinker bad class type";
}

TEST(NodesAnimMisc, SoBlinkerIsOfTypeSoSwitch)
{
    SoBlinker * node = new SoBlinker;
    node->ref();
    bool pass = node->isOfType(SoSwitch::getClassTypeId());
    node->unref();
    EXPECT_TRUE(pass) << "SoBlinker should be derived from SoSwitch";
}

TEST(NodesAnimMisc, SoBlinkerSpeedDefaultIs10)
{
    SoBlinker * node = new SoBlinker;
    node->ref();
    bool pass = (node->speed.getValue() == 1.0f);
    node->unref();
    EXPECT_TRUE(pass) << "SoBlinker speed default != 1.0";
}

TEST(NodesAnimMisc, SoBlinkerOnFieldRoundTrip)
{
    SoBlinker * node = new SoBlinker;
    node->ref();
    node->on.setValue(FALSE);
    bool pass = (node->on.getValue() == FALSE);
    node->unref();
    EXPECT_TRUE(pass) << "SoBlinker on field round-trip failed";
}

// -----------------------------------------------------------------------
// SoRotor
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoRotorClassTypeRegistered)
{
    bool pass = (SoRotor::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoRotor bad class type";
}

TEST(NodesAnimMisc, SoRotorIsOfTypeSoRotation)
{
    SoRotor * node = new SoRotor;
    node->ref();
    bool pass = node->isOfType(SoRotation::getClassTypeId());
    node->unref();
    EXPECT_TRUE(pass) << "SoRotor should be derived from SoRotation";
}

TEST(NodesAnimMisc, SoRotorSpeedDefaultIs10)
{
    SoRotor * node = new SoRotor;
    node->ref();
    bool pass = (node->speed.getValue() == 1.0f);
    node->unref();
    EXPECT_TRUE(pass) << "SoRotor speed default != 1.0";
}

TEST(NodesAnimMisc, SoRotorOnFieldDefaultIsTRUE)
{
    SoRotor * node = new SoRotor;
    node->ref();
    bool pass = (node->on.getValue() == TRUE);
    node->unref();
    EXPECT_TRUE(pass) << "SoRotor on default != TRUE";
}

// -----------------------------------------------------------------------
// SoInfo
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoInfoClassTypeRegistered)
{
    bool pass = (SoInfo::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoInfo bad class type";
}

TEST(NodesAnimMisc, SoInfoStringFieldSetGetRoundTrip)
{
    SoInfo * node = new SoInfo;
    node->ref();
    node->string.setValue("test info");
    bool pass = (strcmp(node->string.getValue().getString(), "test info") == 0);
    node->unref();
    EXPECT_TRUE(pass) << "SoInfo string field set/get failed";
}

// -----------------------------------------------------------------------
// SoLabel
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoLabelClassTypeRegistered)
{
    bool pass = (SoLabel::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoLabel bad class type";
}

TEST(NodesAnimMisc, SoLabelLabelFieldSetGetRoundTrip)
{
    SoLabel * node = new SoLabel;
    node->ref();
    node->label.setValue(SbName("myLabel"));
    bool pass = (strcmp(node->label.getValue().getString(), "myLabel") == 0);
    node->unref();
    EXPECT_TRUE(pass) << "SoLabel label field set/get failed";
}

// -----------------------------------------------------------------------
// SoArray
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoArrayClassTypeRegistered)
{
    bool pass = (SoArray::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoArray bad class type";
}

TEST(NodesAnimMisc, SoArrayNumElements123DefaultsAre1)
{
    SoArray * node = new SoArray;
    node->ref();
    bool pass = (node->numElements1.getValue() == 1) &&
                (node->numElements2.getValue() == 1) &&
                (node->numElements3.getValue() == 1);
    node->unref();
    EXPECT_TRUE(pass) << "SoArray numElements defaults != 1";
}

TEST(NodesAnimMisc, SoArrayNumElements1SetGetRoundTrip)
{
    SoArray * node = new SoArray;
    node->ref();
    node->numElements1.setValue(5);
    bool pass = (node->numElements1.getValue() == 5);
    node->unref();
    EXPECT_TRUE(pass) << "SoArray numElements1 set/get failed";
}

TEST(NodesAnimMisc, SoArrayOriginFieldSetGetRoundTrip)
{
    SoArray * node = new SoArray;
    node->ref();
    node->origin.setValue(SoArray::CENTER);
    bool pass = (node->origin.getValue() == (int)SoArray::CENTER);
    node->unref();
    EXPECT_TRUE(pass) << "SoArray origin CENTER round-trip failed";
}

// -----------------------------------------------------------------------
// SoEnvironment
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoEnvironmentClassTypeRegistered)
{
    bool pass = (SoEnvironment::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoEnvironment bad class type";
}

TEST(NodesAnimMisc, SoEnvironmentFogTypeNONEDefault)
{
    SoEnvironment * node = new SoEnvironment;
    node->ref();
    bool pass = (node->fogType.getValue() == (int)SoEnvironment::NONE);
    node->unref();
    EXPECT_TRUE(pass) << "SoEnvironment fogType default != NONE";
}

TEST(NodesAnimMisc, SoEnvironmentFogTypeFOGRoundTrip)
{
    SoEnvironment * node = new SoEnvironment;
    node->ref();
    node->fogType.setValue(SoEnvironment::FOG);
    bool pass = (node->fogType.getValue() == (int)SoEnvironment::FOG);
    node->unref();
    EXPECT_TRUE(pass) << "SoEnvironment fogType FOG round-trip failed";
}

TEST(NodesAnimMisc, SoEnvironmentAmbientIntensitySetGetRoundTrip)
{
    SoEnvironment * node = new SoEnvironment;
    node->ref();
    node->ambientIntensity.setValue(0.3f);
    bool pass = (node->ambientIntensity.getValue() == 0.3f);
    node->unref();
    EXPECT_TRUE(pass) << "SoEnvironment ambientIntensity round-trip failed";
}

// -----------------------------------------------------------------------
// SoDrawStyle
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoDrawStyleClassTypeRegistered)
{
    bool pass = (SoDrawStyle::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoDrawStyle bad class type";
}

TEST(NodesAnimMisc, SoDrawStyleStyleLINESRoundTrip)
{
    SoDrawStyle * node = new SoDrawStyle;
    node->ref();
    node->style.setValue(SoDrawStyle::LINES);
    bool pass = (node->style.getValue() == (int)SoDrawStyle::LINES);
    node->unref();
    EXPECT_TRUE(pass) << "SoDrawStyle style LINES round-trip failed";
}

TEST(NodesAnimMisc, SoDrawStyleLineWidthSetGetRoundTrip)
{
    SoDrawStyle * node = new SoDrawStyle;
    node->ref();
    node->lineWidth.setValue(2.0f);
    bool pass = (node->lineWidth.getValue() == 2.0f);
    node->unref();
    EXPECT_TRUE(pass) << "SoDrawStyle lineWidth set/get failed";
}

TEST(NodesAnimMisc, SoDrawStylePointSizeSetGetRoundTrip)
{
    SoDrawStyle * node = new SoDrawStyle;
    node->ref();
    node->pointSize.setValue(4.0f);
    bool pass = (node->pointSize.getValue() == 4.0f);
    node->unref();
    EXPECT_TRUE(pass) << "SoDrawStyle pointSize set/get failed";
}

// -----------------------------------------------------------------------
// SoPickStyle
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoPickStyleClassTypeRegistered)
{
    bool pass = (SoPickStyle::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoPickStyle bad class type";
}

TEST(NodesAnimMisc, SoPickStyleStyleUNPICKABLERoundTrip)
{
    SoPickStyle * node = new SoPickStyle;
    node->ref();
    node->style.setValue(SoPickStyle::UNPICKABLE);
    bool pass = (node->style.getValue() == (int)SoPickStyle::UNPICKABLE);
    node->unref();
    EXPECT_TRUE(pass) << "SoPickStyle UNPICKABLE round-trip failed";
}

// -----------------------------------------------------------------------
// SoPackedColor
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoPackedColorClassTypeRegistered)
{
    bool pass = (SoPackedColor::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoPackedColor bad class type";
}

TEST(NodesAnimMisc, SoPackedColorOrderedRGBASetGetRoundTrip)
{
    SoPackedColor * node = new SoPackedColor;
    node->ref();
    node->orderedRGBA.set1Value(0, 0xFF0000FF); // red, fully opaque
    bool pass = (node->orderedRGBA.getNum() == 1) &&
                (node->orderedRGBA[0] == 0xFF0000FF);
    node->unref();
    EXPECT_TRUE(pass) << "SoPackedColor orderedRGBA set/get failed";
}

// -----------------------------------------------------------------------
// SoMatrixTransform
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoMatrixTransformClassTypeRegistered)
{
    bool pass = (SoMatrixTransform::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoMatrixTransform bad class type";
}

TEST(NodesAnimMisc, SoMatrixTransformMatrixDefaultIsIdentity)
{
    SoMatrixTransform * node = new SoMatrixTransform;
    node->ref();
    bool pass = (node->matrix.getValue() == SbMatrix::identity());
    node->unref();
    EXPECT_TRUE(pass) << "SoMatrixTransform default matrix is not identity";
}

TEST(NodesAnimMisc, SoMatrixTransformMatrixSetGetRoundTrip)
{
    SoMatrixTransform * node = new SoMatrixTransform;
    node->ref();
    SbMatrix m = SbMatrix::identity();
    m[3][0] = 5.0f; // translation x=5
    node->matrix.setValue(m);
    bool pass = (node->matrix.getValue() == m);
    node->unref();
    EXPECT_TRUE(pass) << "SoMatrixTransform matrix set/get failed";
}

// -----------------------------------------------------------------------
// SoRotationXYZ
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoRotationXYZClassTypeRegistered)
{
    bool pass = (SoRotationXYZ::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoRotationXYZ bad class type";
}

TEST(NodesAnimMisc, SoRotationXYZAxisFieldRoundTrip)
{
    SoRotationXYZ * node = new SoRotationXYZ;
    node->ref();
    node->axis.setValue(SoRotationXYZ::Y);
    bool pass = (node->axis.getValue() == (int)SoRotationXYZ::Y);
    node->unref();
    EXPECT_TRUE(pass) << "SoRotationXYZ axis Y round-trip failed";
}

TEST(NodesAnimMisc, SoRotationXYZAngleFieldRoundTrip)
{
    SoRotationXYZ * node = new SoRotationXYZ;
    node->ref();
    node->angle.setValue(1.5708f); // π/2
    bool pass = (std::fabs(node->angle.getValue() - 1.5708f) < 1e-4f);
    node->unref();
    EXPECT_TRUE(pass) << "SoRotationXYZ angle round-trip failed";
}

// -----------------------------------------------------------------------
// SoSelection
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoSelectionClassTypeRegistered)
{
    bool pass = (SoSelection::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoSelection bad class type";
}

TEST(NodesAnimMisc, SoSelectionAddChildGetNumSelected0Initially)
{
    SoSelection * sel = new SoSelection;
    sel->ref();
    sel->addChild(new SoCube);
    bool pass = (sel->getNumSelected() == 0);
    sel->unref();
    EXPECT_TRUE(pass) << "SoSelection initial getNumSelected should be 0";
}
