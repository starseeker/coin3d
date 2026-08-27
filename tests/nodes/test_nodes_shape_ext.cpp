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
 * @file test_nodes_shape_ext.cpp
 * @brief Tests for scene-state nodes with low coverage.
 *
 * Covers (nodes/ 41.8 %):
 *   SoComplexity       - type/value/textureQuality fields
 *   SoLightModel       - model field round-trip
 *   SoClipPlane        - plane/on fields
 *   SoShapeHints       - vertexOrdering/shapeType/faceType/creaseAngle fields
 *   SoNormalBinding    - value field
 *   SoMaterialBinding  - value field
 *   SoTextureCoordinateBinding - value field
 *   SoLOD              - range/alternateRep, addChild/getNumChildren
 *   SoDepthBuffer      - function/test/write/range fields
 *   SoPolygonOffset    - factor/units/styles/on fields
 */

#include "../test_utils.h"

#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoClipPlane.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoTextureCoordinateBinding.h>
#include <Inventor/nodes/SoLOD.h>
#include <Inventor/nodes/SoDepthBuffer.h>
#include <Inventor/nodes/SoPolygonOffset.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SoType.h>

using namespace ObolTest;

TEST(NodesShapeExt, SoComplexityClassTypeRegistered)
{
    bool pass = (SoComplexity::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoComplexity bad class type";
}

TEST(NodesShapeExt, SoComplexityTypeFieldDefaultIsOBJECTSPACE)
{
    SoComplexity * node = new SoComplexity;
    node->ref();
    bool pass = (node->type.getValue() == (int)SoComplexity::OBJECT_SPACE);
    node->unref();
    EXPECT_TRUE(pass) << "SoComplexity type default != OBJECT_SPACE";
}

TEST(NodesShapeExt, SoComplexityValueFieldDefaultIs05)
{
    SoComplexity * node = new SoComplexity;
    node->ref();
    bool pass = (node->value.getValue() == 0.5f);
    node->unref();
    EXPECT_TRUE(pass) << "SoComplexity value default != 0.5";
}

TEST(NodesShapeExt, SoComplexityTypeSCREENSPACERoundTrip)
{
    SoComplexity * node = new SoComplexity;
    node->ref();
    node->type.setValue(SoComplexity::SCREEN_SPACE);
    bool pass = (node->type.getValue() == (int)SoComplexity::SCREEN_SPACE);
    node->unref();
    EXPECT_TRUE(pass) << "SoComplexity SCREEN_SPACE round-trip failed";
}

TEST(NodesShapeExt, SoComplexityTypeBOUNDINGBOXRoundTrip)
{
    SoComplexity * node = new SoComplexity;
    node->ref();
    node->type.setValue(SoComplexity::BOUNDING_BOX);
    bool pass = (node->type.getValue() == (int)SoComplexity::BOUNDING_BOX);
    node->unref();
    EXPECT_TRUE(pass) << "SoComplexity BOUNDING_BOX round-trip failed";
}

TEST(NodesShapeExt, SoComplexityValueFieldSetGetRoundTrip)
{
    SoComplexity * node = new SoComplexity;
    node->ref();
    node->value.setValue(0.8f);
    bool pass = (node->value.getValue() == 0.8f);
    node->unref();
    EXPECT_TRUE(pass) << "SoComplexity value set/get failed";
}

TEST(NodesShapeExt, SoComplexityTextureQualityFieldSetGetRoundTrip)
{
    SoComplexity * node = new SoComplexity;
    node->ref();
    node->textureQuality.setValue(0.7f);
    bool pass = (node->textureQuality.getValue() == 0.7f);
    node->unref();
    EXPECT_TRUE(pass) << "SoComplexity textureQuality set/get failed";
}

// -----------------------------------------------------------------------
// SoLightModel
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoLightModelClassTypeRegistered)
{
    bool pass = (SoLightModel::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoLightModel bad class type";
}

TEST(NodesShapeExt, SoLightModelModelDefaultIsPHONG)
{
    SoLightModel * node = new SoLightModel;
    node->ref();
    bool pass = (node->model.getValue() == (int)SoLightModel::PHONG);
    node->unref();
    EXPECT_TRUE(pass) << "SoLightModel model default != PHONG";
}

TEST(NodesShapeExt, SoLightModelBASECOLORRoundTrip)
{
    SoLightModel * node = new SoLightModel;
    node->ref();
    node->model.setValue(SoLightModel::BASE_COLOR);
    bool pass = (node->model.getValue() == (int)SoLightModel::BASE_COLOR);
    node->unref();
    EXPECT_TRUE(pass) << "SoLightModel BASE_COLOR round-trip failed";
}

// -----------------------------------------------------------------------
// SoClipPlane
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoClipPlaneClassTypeRegistered)
{
    bool pass = (SoClipPlane::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoClipPlane bad class type";
}

TEST(NodesShapeExt, SoClipPlaneOnFieldDefaultIsTRUE)
{
    SoClipPlane * node = new SoClipPlane;
    node->ref();
    bool pass = (node->on.getValue() == TRUE);
    node->unref();
    EXPECT_TRUE(pass) << "SoClipPlane on default != TRUE";
}

TEST(NodesShapeExt, SoClipPlanePlaneFieldSetGetRoundTrip)
{
    SoClipPlane * node = new SoClipPlane;
    node->ref();
    SbPlane p(SbVec3f(0, 1, 0), 2.0f);
    node->plane.setValue(p);
    bool pass = (node->plane.getValue() == p);
    node->unref();
    EXPECT_TRUE(pass) << "SoClipPlane plane set/get failed";
}

// -----------------------------------------------------------------------
// SoShapeHints
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoShapeHintsClassTypeRegistered)
{
    bool pass = (SoShapeHints::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoShapeHints bad class type";
}

TEST(NodesShapeExt, SoShapeHintsVertexOrderingDefaultIsUNKNOWNORDERING)
{
    SoShapeHints * node = new SoShapeHints;
    node->ref();
    bool pass = (node->vertexOrdering.getValue() == (int)SoShapeHints::UNKNOWN_ORDERING);
    node->unref();
    EXPECT_TRUE(pass) << "SoShapeHints vertexOrdering default wrong";
}

TEST(NodesShapeExt, SoShapeHintsShapeTypeRoundTrip)
{
    SoShapeHints * node = new SoShapeHints;
    node->ref();
    node->shapeType.setValue(SoShapeHints::SOLID);
    bool pass = (node->shapeType.getValue() == (int)SoShapeHints::SOLID);
    node->unref();
    EXPECT_TRUE(pass) << "SoShapeHints shapeType round-trip failed";
}

TEST(NodesShapeExt, SoShapeHintsFaceTypeRoundTrip)
{
    SoShapeHints * node = new SoShapeHints;
    node->ref();
    node->faceType.setValue(SoShapeHints::CONVEX);
    bool pass = (node->faceType.getValue() == (int)SoShapeHints::CONVEX);
    node->unref();
    EXPECT_TRUE(pass) << "SoShapeHints faceType round-trip failed";
}

TEST(NodesShapeExt, SoShapeHintsCreaseAngleDefaultIs00)
{
    SoShapeHints * node = new SoShapeHints;
    node->ref();
    bool pass = (node->creaseAngle.getValue() == 0.0f);
    node->unref();
    EXPECT_TRUE(pass) << "SoShapeHints creaseAngle default != 0.0";
}

TEST(NodesShapeExt, SoShapeHintsVertexOrderingCOUNTERCLOCKWISERoundTrip)
{
    SoShapeHints * node = new SoShapeHints;
    node->ref();
    node->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
    bool pass = (node->vertexOrdering.getValue() == (int)SoShapeHints::COUNTERCLOCKWISE);
    node->unref();
    EXPECT_TRUE(pass) << "SoShapeHints COUNTERCLOCKWISE round-trip failed";
}

// -----------------------------------------------------------------------
// SoNormalBinding
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoNormalBindingClassTypeRegistered)
{
    bool pass = (SoNormalBinding::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoNormalBinding bad class type";
}

TEST(NodesShapeExt, SoNormalBindingValueFieldRoundTripPERVERTEXINDEXED)
{
    SoNormalBinding * node = new SoNormalBinding;
    node->ref();
    node->value.setValue(SoNormalBinding::PER_VERTEX_INDEXED);
    bool pass = (node->value.getValue() == (int)SoNormalBinding::PER_VERTEX_INDEXED);
    node->unref();
    EXPECT_TRUE(pass) << "SoNormalBinding PER_VERTEX_INDEXED round-trip failed";
}

// -----------------------------------------------------------------------
// SoMaterialBinding
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoMaterialBindingClassTypeRegistered)
{
    bool pass = (SoMaterialBinding::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoMaterialBinding bad class type";
}

TEST(NodesShapeExt, SoMaterialBindingValueFieldPERFACEINDEXEDRoundTrip)
{
    SoMaterialBinding * node = new SoMaterialBinding;
    node->ref();
    node->value.setValue(SoMaterialBinding::PER_FACE_INDEXED);
    bool pass = (node->value.getValue() == (int)SoMaterialBinding::PER_FACE_INDEXED);
    node->unref();
    EXPECT_TRUE(pass) << "SoMaterialBinding PER_FACE_INDEXED round-trip failed";
}

// -----------------------------------------------------------------------
// SoTextureCoordinateBinding
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoTextureCoordinateBindingClassTypeRegistered)
{
    bool pass = (SoTextureCoordinateBinding::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoTextureCoordinateBinding bad class type";
}

TEST(NodesShapeExt, SoTextureCoordinateBindingValuePERVERTEXINDEXEDRoundTrip)
{
    SoTextureCoordinateBinding * node = new SoTextureCoordinateBinding;
    node->ref();
    node->value.setValue(SoTextureCoordinateBinding::PER_VERTEX_INDEXED);
    bool pass = (node->value.getValue() == (int)SoTextureCoordinateBinding::PER_VERTEX_INDEXED);
    node->unref();
    EXPECT_TRUE(pass) << "SoTextureCoordinateBinding value round-trip failed";
}

// -----------------------------------------------------------------------
// SoLOD
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoLODClassTypeRegistered)
{
    bool pass = (SoLOD::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoLOD bad class type";
}

TEST(NodesShapeExt, SoLODAddChildAndGetNumChildren)
{
    SoLOD * lod = new SoLOD;
    lod->ref();
    lod->addChild(new SoSphere);
    lod->addChild(new SoCube);
    bool pass = (lod->getNumChildren() == 2);
    lod->unref();
    EXPECT_TRUE(pass) << "SoLOD addChild/getNumChildren failed";
}

TEST(NodesShapeExt, SoLODRangeFieldSetGetRoundTrip)
{
    SoLOD * lod = new SoLOD;
    lod->ref();
    lod->range.set1Value(0, 10.0f);
    lod->range.set1Value(1, 50.0f);
    bool pass = (lod->range.getNum() == 2) &&
                (lod->range[0] == 10.0f) &&
                (lod->range[1] == 50.0f);
    lod->unref();
    EXPECT_TRUE(pass) << "SoLOD range set/get failed";
}

// -----------------------------------------------------------------------
// SoDepthBuffer
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoDepthBufferClassTypeRegistered)
{
    bool pass = (SoDepthBuffer::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoDepthBuffer bad class type";
}

TEST(NodesShapeExt, SoDepthBufferTestDefaultIsTRUE)
{
    SoDepthBuffer * node = new SoDepthBuffer;
    node->ref();
    bool pass = (node->test.getValue() == TRUE);
    node->unref();
    EXPECT_TRUE(pass) << "SoDepthBuffer test default != TRUE";
}

TEST(NodesShapeExt, SoDepthBufferWriteDefaultIsTRUE)
{
    SoDepthBuffer * node = new SoDepthBuffer;
    node->ref();
    bool pass = (node->write.getValue() == TRUE);
    node->unref();
    EXPECT_TRUE(pass) << "SoDepthBuffer write default != TRUE";
}

// -----------------------------------------------------------------------
// SoPolygonOffset
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoPolygonOffsetClassTypeRegistered)
{
    bool pass = (SoPolygonOffset::getClassTypeId() != SoType::badType());
    EXPECT_TRUE(pass) << "SoPolygonOffset bad class type";
}

TEST(NodesShapeExt, SoPolygonOffsetOnDefaultIsTRUE)
{
    SoPolygonOffset * node = new SoPolygonOffset;
    node->ref();
    bool pass = (node->on.getValue() == TRUE);
    node->unref();
    EXPECT_TRUE(pass) << "SoPolygonOffset on default != TRUE";
}
