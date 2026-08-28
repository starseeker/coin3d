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
    EXPECT_TRUE((SoComplexity::getClassTypeId() != SoType::badType())) << "SoComplexity bad class type";
}

TEST(NodesShapeExt, SoComplexityTypeFieldDefaultIsOBJECTSPACE)
{
    SoComplexity * node = new SoComplexity;
    node->ref();
    EXPECT_TRUE((node->type.getValue() == (int)SoComplexity::OBJECT_SPACE)) << "SoComplexity type default != OBJECT_SPACE";
    node->unref();
}

TEST(NodesShapeExt, SoComplexityValueFieldDefaultIs05)
{
    SoComplexity * node = new SoComplexity;
    node->ref();
    EXPECT_TRUE((node->value.getValue() == 0.5f)) << "SoComplexity value default != 0.5";
    node->unref();
}

TEST(NodesShapeExt, SoComplexityTypeSCREENSPACERoundTrip)
{
    SoComplexity * node = new SoComplexity;
    node->ref();
    node->type.setValue(SoComplexity::SCREEN_SPACE);
    EXPECT_TRUE((node->type.getValue() == (int)SoComplexity::SCREEN_SPACE)) << "SoComplexity SCREEN_SPACE round-trip failed";
    node->unref();
}

TEST(NodesShapeExt, SoComplexityTypeBOUNDINGBOXRoundTrip)
{
    SoComplexity * node = new SoComplexity;
    node->ref();
    node->type.setValue(SoComplexity::BOUNDING_BOX);
    EXPECT_TRUE((node->type.getValue() == (int)SoComplexity::BOUNDING_BOX)) << "SoComplexity BOUNDING_BOX round-trip failed";
    node->unref();
}

TEST(NodesShapeExt, SoComplexityValueFieldSetGetRoundTrip)
{
    SoComplexity * node = new SoComplexity;
    node->ref();
    node->value.setValue(0.8f);
    EXPECT_TRUE((node->value.getValue() == 0.8f)) << "SoComplexity value set/get failed";
    node->unref();
}

TEST(NodesShapeExt, SoComplexityTextureQualityFieldSetGetRoundTrip)
{
    SoComplexity * node = new SoComplexity;
    node->ref();
    node->textureQuality.setValue(0.7f);
    EXPECT_TRUE((node->textureQuality.getValue() == 0.7f)) << "SoComplexity textureQuality set/get failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoLightModel
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoLightModelClassTypeRegistered)
{
    EXPECT_TRUE((SoLightModel::getClassTypeId() != SoType::badType())) << "SoLightModel bad class type";
}

TEST(NodesShapeExt, SoLightModelModelDefaultIsPHONG)
{
    SoLightModel * node = new SoLightModel;
    node->ref();
    EXPECT_TRUE((node->model.getValue() == (int)SoLightModel::PHONG)) << "SoLightModel model default != PHONG";
    node->unref();
}

TEST(NodesShapeExt, SoLightModelBASECOLORRoundTrip)
{
    SoLightModel * node = new SoLightModel;
    node->ref();
    node->model.setValue(SoLightModel::BASE_COLOR);
    EXPECT_TRUE((node->model.getValue() == (int)SoLightModel::BASE_COLOR)) << "SoLightModel BASE_COLOR round-trip failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoClipPlane
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoClipPlaneClassTypeRegistered)
{
    EXPECT_TRUE((SoClipPlane::getClassTypeId() != SoType::badType())) << "SoClipPlane bad class type";
}

TEST(NodesShapeExt, SoClipPlaneOnFieldDefaultIsTRUE)
{
    SoClipPlane * node = new SoClipPlane;
    node->ref();
    EXPECT_TRUE((node->on.getValue() == TRUE)) << "SoClipPlane on default != TRUE";
    node->unref();
}

TEST(NodesShapeExt, SoClipPlanePlaneFieldSetGetRoundTrip)
{
    SoClipPlane * node = new SoClipPlane;
    node->ref();
    SbPlane p(SbVec3f(0, 1, 0), 2.0f);
    node->plane.setValue(p);
    EXPECT_TRUE((node->plane.getValue() == p)) << "SoClipPlane plane set/get failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoShapeHints
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoShapeHintsClassTypeRegistered)
{
    EXPECT_TRUE((SoShapeHints::getClassTypeId() != SoType::badType())) << "SoShapeHints bad class type";
}

TEST(NodesShapeExt, SoShapeHintsVertexOrderingDefaultIsUNKNOWNORDERING)
{
    SoShapeHints * node = new SoShapeHints;
    node->ref();
    EXPECT_TRUE((node->vertexOrdering.getValue() == (int)SoShapeHints::UNKNOWN_ORDERING)) << "SoShapeHints vertexOrdering default wrong";
    node->unref();
}

TEST(NodesShapeExt, SoShapeHintsShapeTypeRoundTrip)
{
    SoShapeHints * node = new SoShapeHints;
    node->ref();
    node->shapeType.setValue(SoShapeHints::SOLID);
    EXPECT_TRUE((node->shapeType.getValue() == (int)SoShapeHints::SOLID)) << "SoShapeHints shapeType round-trip failed";
    node->unref();
}

TEST(NodesShapeExt, SoShapeHintsFaceTypeRoundTrip)
{
    SoShapeHints * node = new SoShapeHints;
    node->ref();
    node->faceType.setValue(SoShapeHints::CONVEX);
    EXPECT_TRUE((node->faceType.getValue() == (int)SoShapeHints::CONVEX)) << "SoShapeHints faceType round-trip failed";
    node->unref();
}

TEST(NodesShapeExt, SoShapeHintsCreaseAngleDefaultIs00)
{
    SoShapeHints * node = new SoShapeHints;
    node->ref();
    EXPECT_TRUE((node->creaseAngle.getValue() == 0.0f)) << "SoShapeHints creaseAngle default != 0.0";
    node->unref();
}

TEST(NodesShapeExt, SoShapeHintsVertexOrderingCOUNTERCLOCKWISERoundTrip)
{
    SoShapeHints * node = new SoShapeHints;
    node->ref();
    node->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
    EXPECT_TRUE((node->vertexOrdering.getValue() == (int)SoShapeHints::COUNTERCLOCKWISE)) << "SoShapeHints COUNTERCLOCKWISE round-trip failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoNormalBinding
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoNormalBindingClassTypeRegistered)
{
    EXPECT_TRUE((SoNormalBinding::getClassTypeId() != SoType::badType())) << "SoNormalBinding bad class type";
}

TEST(NodesShapeExt, SoNormalBindingValueFieldRoundTripPERVERTEXINDEXED)
{
    SoNormalBinding * node = new SoNormalBinding;
    node->ref();
    node->value.setValue(SoNormalBinding::PER_VERTEX_INDEXED);
    EXPECT_TRUE((node->value.getValue() == (int)SoNormalBinding::PER_VERTEX_INDEXED)) << "SoNormalBinding PER_VERTEX_INDEXED round-trip failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoMaterialBinding
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoMaterialBindingClassTypeRegistered)
{
    EXPECT_TRUE((SoMaterialBinding::getClassTypeId() != SoType::badType())) << "SoMaterialBinding bad class type";
}

TEST(NodesShapeExt, SoMaterialBindingValueFieldPERFACEINDEXEDRoundTrip)
{
    SoMaterialBinding * node = new SoMaterialBinding;
    node->ref();
    node->value.setValue(SoMaterialBinding::PER_FACE_INDEXED);
    EXPECT_TRUE((node->value.getValue() == (int)SoMaterialBinding::PER_FACE_INDEXED)) << "SoMaterialBinding PER_FACE_INDEXED round-trip failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoTextureCoordinateBinding
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoTextureCoordinateBindingClassTypeRegistered)
{
    EXPECT_TRUE((SoTextureCoordinateBinding::getClassTypeId() != SoType::badType())) << "SoTextureCoordinateBinding bad class type";
}

TEST(NodesShapeExt, SoTextureCoordinateBindingValuePERVERTEXINDEXEDRoundTrip)
{
    SoTextureCoordinateBinding * node = new SoTextureCoordinateBinding;
    node->ref();
    node->value.setValue(SoTextureCoordinateBinding::PER_VERTEX_INDEXED);
    EXPECT_TRUE((node->value.getValue() == (int)SoTextureCoordinateBinding::PER_VERTEX_INDEXED)) << "SoTextureCoordinateBinding value round-trip failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoLOD
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoLODClassTypeRegistered)
{
    EXPECT_TRUE((SoLOD::getClassTypeId() != SoType::badType())) << "SoLOD bad class type";
}

TEST(NodesShapeExt, SoLODAddChildAndGetNumChildren)
{
    SoLOD * lod = new SoLOD;
    lod->ref();
    lod->addChild(new SoSphere);
    lod->addChild(new SoCube);
    EXPECT_TRUE((lod->getNumChildren() == 2)) << "SoLOD addChild/getNumChildren failed";
    lod->unref();
}

TEST(NodesShapeExt, SoLODRangeFieldSetGetRoundTrip)
{
    SoLOD * lod = new SoLOD;
    lod->ref();
    lod->range.set1Value(0, 10.0f);
    lod->range.set1Value(1, 50.0f);
    EXPECT_TRUE((lod->range.getNum() == 2) &&
                (lod->range[0] == 10.0f) &&
                (lod->range[1] == 50.0f)) << "SoLOD range set/get failed";
    lod->unref();
}

// -----------------------------------------------------------------------
// SoDepthBuffer
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoDepthBufferClassTypeRegistered)
{
    EXPECT_TRUE((SoDepthBuffer::getClassTypeId() != SoType::badType())) << "SoDepthBuffer bad class type";
}

TEST(NodesShapeExt, SoDepthBufferTestDefaultIsTRUE)
{
    SoDepthBuffer * node = new SoDepthBuffer;
    node->ref();
    EXPECT_TRUE((node->test.getValue() == TRUE)) << "SoDepthBuffer test default != TRUE";
    node->unref();
}

TEST(NodesShapeExt, SoDepthBufferWriteDefaultIsTRUE)
{
    SoDepthBuffer * node = new SoDepthBuffer;
    node->ref();
    EXPECT_TRUE((node->write.getValue() == TRUE)) << "SoDepthBuffer write default != TRUE";
    node->unref();
}

// -----------------------------------------------------------------------
// SoPolygonOffset
// -----------------------------------------------------------------------

TEST(NodesShapeExt, SoPolygonOffsetClassTypeRegistered)
{
    EXPECT_TRUE((SoPolygonOffset::getClassTypeId() != SoType::badType())) << "SoPolygonOffset bad class type";
}

TEST(NodesShapeExt, SoPolygonOffsetOnDefaultIsTRUE)
{
    SoPolygonOffset * node = new SoPolygonOffset;
    node->ref();
    EXPECT_TRUE((node->on.getValue() == TRUE)) << "SoPolygonOffset on default != TRUE";
    node->unref();
}
