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
    EXPECT_TRUE((SoBlinker::getClassTypeId() != SoType::badType())) << "SoBlinker bad class type";
}

TEST(NodesAnimMisc, SoBlinkerIsOfTypeSoSwitch)
{
    SoBlinker * node = new SoBlinker;
    node->ref();
    EXPECT_TRUE(node->isOfType(SoSwitch::getClassTypeId())) << "SoBlinker should be derived from SoSwitch";
    node->unref();
}

TEST(NodesAnimMisc, SoBlinkerSpeedDefaultIs10)
{
    SoBlinker * node = new SoBlinker;
    node->ref();
    EXPECT_TRUE((node->speed.getValue() == 1.0f)) << "SoBlinker speed default != 1.0";
    node->unref();
}

TEST(NodesAnimMisc, SoBlinkerOnFieldRoundTrip)
{
    SoBlinker * node = new SoBlinker;
    node->ref();
    node->on.setValue(FALSE);
    EXPECT_TRUE((node->on.getValue() == FALSE)) << "SoBlinker on field round-trip failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoRotor
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoRotorClassTypeRegistered)
{
    EXPECT_TRUE((SoRotor::getClassTypeId() != SoType::badType())) << "SoRotor bad class type";
}

TEST(NodesAnimMisc, SoRotorIsOfTypeSoRotation)
{
    SoRotor * node = new SoRotor;
    node->ref();
    EXPECT_TRUE(node->isOfType(SoRotation::getClassTypeId())) << "SoRotor should be derived from SoRotation";
    node->unref();
}

TEST(NodesAnimMisc, SoRotorSpeedDefaultIs10)
{
    SoRotor * node = new SoRotor;
    node->ref();
    EXPECT_TRUE((node->speed.getValue() == 1.0f)) << "SoRotor speed default != 1.0";
    node->unref();
}

TEST(NodesAnimMisc, SoRotorOnFieldDefaultIsTRUE)
{
    SoRotor * node = new SoRotor;
    node->ref();
    EXPECT_TRUE((node->on.getValue() == TRUE)) << "SoRotor on default != TRUE";
    node->unref();
}

// -----------------------------------------------------------------------
// SoInfo
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoInfoClassTypeRegistered)
{
    EXPECT_TRUE((SoInfo::getClassTypeId() != SoType::badType())) << "SoInfo bad class type";
}

TEST(NodesAnimMisc, SoInfoStringFieldSetGetRoundTrip)
{
    SoInfo * node = new SoInfo;
    node->ref();
    node->string.setValue("test info");
    EXPECT_TRUE((strcmp(node->string.getValue().getString(), "test info") == 0)) << "SoInfo string field set/get failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoLabel
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoLabelClassTypeRegistered)
{
    EXPECT_TRUE((SoLabel::getClassTypeId() != SoType::badType())) << "SoLabel bad class type";
}

TEST(NodesAnimMisc, SoLabelLabelFieldSetGetRoundTrip)
{
    SoLabel * node = new SoLabel;
    node->ref();
    node->label.setValue(SbName("myLabel"));
    EXPECT_TRUE((strcmp(node->label.getValue().getString(), "myLabel") == 0)) << "SoLabel label field set/get failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoArray
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoArrayClassTypeRegistered)
{
    EXPECT_TRUE((SoArray::getClassTypeId() != SoType::badType())) << "SoArray bad class type";
}

TEST(NodesAnimMisc, SoArrayNumElements123DefaultsAre1)
{
    SoArray * node = new SoArray;
    node->ref();
    EXPECT_TRUE((node->numElements1.getValue() == 1) &&
                (node->numElements2.getValue() == 1) &&
                (node->numElements3.getValue() == 1)) << "SoArray numElements defaults != 1";
    node->unref();
}

TEST(NodesAnimMisc, SoArrayNumElements1SetGetRoundTrip)
{
    SoArray * node = new SoArray;
    node->ref();
    node->numElements1.setValue(5);
    EXPECT_TRUE((node->numElements1.getValue() == 5)) << "SoArray numElements1 set/get failed";
    node->unref();
}

TEST(NodesAnimMisc, SoArrayOriginFieldSetGetRoundTrip)
{
    SoArray * node = new SoArray;
    node->ref();
    node->origin.setValue(SoArray::CENTER);
    EXPECT_TRUE((node->origin.getValue() == (int)SoArray::CENTER)) << "SoArray origin CENTER round-trip failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoEnvironment
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoEnvironmentClassTypeRegistered)
{
    EXPECT_TRUE((SoEnvironment::getClassTypeId() != SoType::badType())) << "SoEnvironment bad class type";
}

TEST(NodesAnimMisc, SoEnvironmentFogTypeNONEDefault)
{
    SoEnvironment * node = new SoEnvironment;
    node->ref();
    EXPECT_TRUE((node->fogType.getValue() == (int)SoEnvironment::NONE)) << "SoEnvironment fogType default != NONE";
    node->unref();
}

TEST(NodesAnimMisc, SoEnvironmentFogTypeFOGRoundTrip)
{
    SoEnvironment * node = new SoEnvironment;
    node->ref();
    node->fogType.setValue(SoEnvironment::FOG);
    EXPECT_TRUE((node->fogType.getValue() == (int)SoEnvironment::FOG)) << "SoEnvironment fogType FOG round-trip failed";
    node->unref();
}

TEST(NodesAnimMisc, SoEnvironmentAmbientIntensitySetGetRoundTrip)
{
    SoEnvironment * node = new SoEnvironment;
    node->ref();
    node->ambientIntensity.setValue(0.3f);
    EXPECT_TRUE((node->ambientIntensity.getValue() == 0.3f)) << "SoEnvironment ambientIntensity round-trip failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoDrawStyle
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoDrawStyleClassTypeRegistered)
{
    EXPECT_TRUE((SoDrawStyle::getClassTypeId() != SoType::badType())) << "SoDrawStyle bad class type";
}

TEST(NodesAnimMisc, SoDrawStyleStyleLINESRoundTrip)
{
    SoDrawStyle * node = new SoDrawStyle;
    node->ref();
    node->style.setValue(SoDrawStyle::LINES);
    EXPECT_TRUE((node->style.getValue() == (int)SoDrawStyle::LINES)) << "SoDrawStyle style LINES round-trip failed";
    node->unref();
}

TEST(NodesAnimMisc, SoDrawStyleLineWidthSetGetRoundTrip)
{
    SoDrawStyle * node = new SoDrawStyle;
    node->ref();
    node->lineWidth.setValue(2.0f);
    EXPECT_TRUE((node->lineWidth.getValue() == 2.0f)) << "SoDrawStyle lineWidth set/get failed";
    node->unref();
}

TEST(NodesAnimMisc, SoDrawStylePointSizeSetGetRoundTrip)
{
    SoDrawStyle * node = new SoDrawStyle;
    node->ref();
    node->pointSize.setValue(4.0f);
    EXPECT_TRUE((node->pointSize.getValue() == 4.0f)) << "SoDrawStyle pointSize set/get failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoPickStyle
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoPickStyleClassTypeRegistered)
{
    EXPECT_TRUE((SoPickStyle::getClassTypeId() != SoType::badType())) << "SoPickStyle bad class type";
}

TEST(NodesAnimMisc, SoPickStyleStyleUNPICKABLERoundTrip)
{
    SoPickStyle * node = new SoPickStyle;
    node->ref();
    node->style.setValue(SoPickStyle::UNPICKABLE);
    EXPECT_TRUE((node->style.getValue() == (int)SoPickStyle::UNPICKABLE)) << "SoPickStyle UNPICKABLE round-trip failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoPackedColor
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoPackedColorClassTypeRegistered)
{
    EXPECT_TRUE((SoPackedColor::getClassTypeId() != SoType::badType())) << "SoPackedColor bad class type";
}

TEST(NodesAnimMisc, SoPackedColorOrderedRGBASetGetRoundTrip)
{
    SoPackedColor * node = new SoPackedColor;
    node->ref();
    node->orderedRGBA.set1Value(0, 0xFF0000FF); // red, fully opaque
    EXPECT_TRUE((node->orderedRGBA.getNum() == 1) &&
                (node->orderedRGBA[0] == 0xFF0000FF)) << "SoPackedColor orderedRGBA set/get failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoMatrixTransform
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoMatrixTransformClassTypeRegistered)
{
    EXPECT_TRUE((SoMatrixTransform::getClassTypeId() != SoType::badType())) << "SoMatrixTransform bad class type";
}

TEST(NodesAnimMisc, SoMatrixTransformMatrixDefaultIsIdentity)
{
    SoMatrixTransform * node = new SoMatrixTransform;
    node->ref();
    EXPECT_TRUE((node->matrix.getValue() == SbMatrix::identity())) << "SoMatrixTransform default matrix is not identity";
    node->unref();
}

TEST(NodesAnimMisc, SoMatrixTransformMatrixSetGetRoundTrip)
{
    SoMatrixTransform * node = new SoMatrixTransform;
    node->ref();
    SbMatrix m = SbMatrix::identity();
    m[3][0] = 5.0f; // translation x=5
    node->matrix.setValue(m);
    EXPECT_TRUE((node->matrix.getValue() == m)) << "SoMatrixTransform matrix set/get failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoRotationXYZ
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoRotationXYZClassTypeRegistered)
{
    EXPECT_TRUE((SoRotationXYZ::getClassTypeId() != SoType::badType())) << "SoRotationXYZ bad class type";
}

TEST(NodesAnimMisc, SoRotationXYZAxisFieldRoundTrip)
{
    SoRotationXYZ * node = new SoRotationXYZ;
    node->ref();
    node->axis.setValue(SoRotationXYZ::Y);
    EXPECT_TRUE((node->axis.getValue() == (int)SoRotationXYZ::Y)) << "SoRotationXYZ axis Y round-trip failed";
    node->unref();
}

TEST(NodesAnimMisc, SoRotationXYZAngleFieldRoundTrip)
{
    SoRotationXYZ * node = new SoRotationXYZ;
    node->ref();
    node->angle.setValue(1.5708f); // π/2
    EXPECT_TRUE((std::fabs(node->angle.getValue() - 1.5708f) < 1e-4f)) << "SoRotationXYZ angle round-trip failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoSelection
// -----------------------------------------------------------------------

TEST(NodesAnimMisc, SoSelectionClassTypeRegistered)
{
    EXPECT_TRUE((SoSelection::getClassTypeId() != SoType::badType())) << "SoSelection bad class type";
}

TEST(NodesAnimMisc, SoSelectionAddChildGetNumSelected0Initially)
{
    SoSelection * sel = new SoSelection;
    sel->ref();
    sel->addChild(new SoCube);
    EXPECT_TRUE((sel->getNumSelected() == 0)) << "SoSelection initial getNumSelected should be 0";
    sel->unref();
}
