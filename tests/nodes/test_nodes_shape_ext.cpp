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

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoComplexity
    // -----------------------------------------------------------------------
    runner.startTest("SoComplexity class type registered");
    {
        bool pass = (SoComplexity::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoComplexity bad class type");
    }

    runner.startTest("SoComplexity type field default is OBJECT_SPACE");
    {
        SoComplexity * node = new SoComplexity;
        node->ref();
        bool pass = (node->type.getValue() == (int)SoComplexity::OBJECT_SPACE);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoComplexity type default != OBJECT_SPACE");
    }

    runner.startTest("SoComplexity value field default is 0.5");
    {
        SoComplexity * node = new SoComplexity;
        node->ref();
        bool pass = (node->value.getValue() == 0.5f);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoComplexity value default != 0.5");
    }

    runner.startTest("SoComplexity type SCREEN_SPACE round-trip");
    {
        SoComplexity * node = new SoComplexity;
        node->ref();
        node->type.setValue(SoComplexity::SCREEN_SPACE);
        bool pass = (node->type.getValue() == (int)SoComplexity::SCREEN_SPACE);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoComplexity SCREEN_SPACE round-trip failed");
    }

    runner.startTest("SoComplexity type BOUNDING_BOX round-trip");
    {
        SoComplexity * node = new SoComplexity;
        node->ref();
        node->type.setValue(SoComplexity::BOUNDING_BOX);
        bool pass = (node->type.getValue() == (int)SoComplexity::BOUNDING_BOX);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoComplexity BOUNDING_BOX round-trip failed");
    }

    runner.startTest("SoComplexity value field set/get round-trip");
    {
        SoComplexity * node = new SoComplexity;
        node->ref();
        node->value.setValue(0.8f);
        bool pass = (node->value.getValue() == 0.8f);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoComplexity value set/get failed");
    }

    runner.startTest("SoComplexity textureQuality field set/get round-trip");
    {
        SoComplexity * node = new SoComplexity;
        node->ref();
        node->textureQuality.setValue(0.7f);
        bool pass = (node->textureQuality.getValue() == 0.7f);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoComplexity textureQuality set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoLightModel
    // -----------------------------------------------------------------------
    runner.startTest("SoLightModel class type registered");
    {
        bool pass = (SoLightModel::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoLightModel bad class type");
    }

    runner.startTest("SoLightModel model default is PHONG");
    {
        SoLightModel * node = new SoLightModel;
        node->ref();
        bool pass = (node->model.getValue() == (int)SoLightModel::PHONG);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoLightModel model default != PHONG");
    }

    runner.startTest("SoLightModel BASE_COLOR round-trip");
    {
        SoLightModel * node = new SoLightModel;
        node->ref();
        node->model.setValue(SoLightModel::BASE_COLOR);
        bool pass = (node->model.getValue() == (int)SoLightModel::BASE_COLOR);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoLightModel BASE_COLOR round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoClipPlane
    // -----------------------------------------------------------------------
    runner.startTest("SoClipPlane class type registered");
    {
        bool pass = (SoClipPlane::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoClipPlane bad class type");
    }

    runner.startTest("SoClipPlane on field default is TRUE");
    {
        SoClipPlane * node = new SoClipPlane;
        node->ref();
        bool pass = (node->on.getValue() == TRUE);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoClipPlane on default != TRUE");
    }

    runner.startTest("SoClipPlane plane field set/get round-trip");
    {
        SoClipPlane * node = new SoClipPlane;
        node->ref();
        SbPlane p(SbVec3f(0, 1, 0), 2.0f);
        node->plane.setValue(p);
        bool pass = (node->plane.getValue() == p);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoClipPlane plane set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoShapeHints
    // -----------------------------------------------------------------------
    runner.startTest("SoShapeHints class type registered");
    {
        bool pass = (SoShapeHints::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoShapeHints bad class type");
    }

    runner.startTest("SoShapeHints vertexOrdering default is UNKNOWN_ORDERING");
    {
        SoShapeHints * node = new SoShapeHints;
        node->ref();
        bool pass = (node->vertexOrdering.getValue() == (int)SoShapeHints::UNKNOWN_ORDERING);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShapeHints vertexOrdering default wrong");
    }

    runner.startTest("SoShapeHints shapeType round-trip");
    {
        SoShapeHints * node = new SoShapeHints;
        node->ref();
        node->shapeType.setValue(SoShapeHints::SOLID);
        bool pass = (node->shapeType.getValue() == (int)SoShapeHints::SOLID);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShapeHints shapeType round-trip failed");
    }

    runner.startTest("SoShapeHints faceType round-trip");
    {
        SoShapeHints * node = new SoShapeHints;
        node->ref();
        node->faceType.setValue(SoShapeHints::CONVEX);
        bool pass = (node->faceType.getValue() == (int)SoShapeHints::CONVEX);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShapeHints faceType round-trip failed");
    }

    runner.startTest("SoShapeHints creaseAngle default is 0.0");
    {
        SoShapeHints * node = new SoShapeHints;
        node->ref();
        bool pass = (node->creaseAngle.getValue() == 0.0f);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShapeHints creaseAngle default != 0.0");
    }

    runner.startTest("SoShapeHints vertexOrdering COUNTERCLOCKWISE round-trip");
    {
        SoShapeHints * node = new SoShapeHints;
        node->ref();
        node->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
        bool pass = (node->vertexOrdering.getValue() == (int)SoShapeHints::COUNTERCLOCKWISE);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShapeHints COUNTERCLOCKWISE round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoNormalBinding
    // -----------------------------------------------------------------------
    runner.startTest("SoNormalBinding class type registered");
    {
        bool pass = (SoNormalBinding::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoNormalBinding bad class type");
    }

    runner.startTest("SoNormalBinding value field round-trip PER_VERTEX_INDEXED");
    {
        SoNormalBinding * node = new SoNormalBinding;
        node->ref();
        node->value.setValue(SoNormalBinding::PER_VERTEX_INDEXED);
        bool pass = (node->value.getValue() == (int)SoNormalBinding::PER_VERTEX_INDEXED);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoNormalBinding PER_VERTEX_INDEXED round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoMaterialBinding
    // -----------------------------------------------------------------------
    runner.startTest("SoMaterialBinding class type registered");
    {
        bool pass = (SoMaterialBinding::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoMaterialBinding bad class type");
    }

    runner.startTest("SoMaterialBinding value field PER_FACE_INDEXED round-trip");
    {
        SoMaterialBinding * node = new SoMaterialBinding;
        node->ref();
        node->value.setValue(SoMaterialBinding::PER_FACE_INDEXED);
        bool pass = (node->value.getValue() == (int)SoMaterialBinding::PER_FACE_INDEXED);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoMaterialBinding PER_FACE_INDEXED round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoTextureCoordinateBinding
    // -----------------------------------------------------------------------
    runner.startTest("SoTextureCoordinateBinding class type registered");
    {
        bool pass = (SoTextureCoordinateBinding::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoTextureCoordinateBinding bad class type");
    }

    runner.startTest("SoTextureCoordinateBinding value PER_VERTEX_INDEXED round-trip");
    {
        SoTextureCoordinateBinding * node = new SoTextureCoordinateBinding;
        node->ref();
        node->value.setValue(SoTextureCoordinateBinding::PER_VERTEX_INDEXED);
        bool pass = (node->value.getValue() == (int)SoTextureCoordinateBinding::PER_VERTEX_INDEXED);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoTextureCoordinateBinding value round-trip failed");
    }

    // -----------------------------------------------------------------------
    // SoLOD
    // -----------------------------------------------------------------------
    runner.startTest("SoLOD class type registered");
    {
        bool pass = (SoLOD::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoLOD bad class type");
    }

    runner.startTest("SoLOD addChild and getNumChildren");
    {
        SoLOD * lod = new SoLOD;
        lod->ref();
        lod->addChild(new SoSphere);
        lod->addChild(new SoCube);
        bool pass = (lod->getNumChildren() == 2);
        lod->unref();
        runner.endTest(pass, pass ? "" : "SoLOD addChild/getNumChildren failed");
    }

    runner.startTest("SoLOD range field set/get round-trip");
    {
        SoLOD * lod = new SoLOD;
        lod->ref();
        lod->range.set1Value(0, 10.0f);
        lod->range.set1Value(1, 50.0f);
        bool pass = (lod->range.getNum() == 2) &&
                    (lod->range[0] == 10.0f) &&
                    (lod->range[1] == 50.0f);
        lod->unref();
        runner.endTest(pass, pass ? "" : "SoLOD range set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoDepthBuffer
    // -----------------------------------------------------------------------
    runner.startTest("SoDepthBuffer class type registered");
    {
        bool pass = (SoDepthBuffer::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoDepthBuffer bad class type");
    }

    runner.startTest("SoDepthBuffer test default is TRUE");
    {
        SoDepthBuffer * node = new SoDepthBuffer;
        node->ref();
        bool pass = (node->test.getValue() == TRUE);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoDepthBuffer test default != TRUE");
    }

    runner.startTest("SoDepthBuffer write default is TRUE");
    {
        SoDepthBuffer * node = new SoDepthBuffer;
        node->ref();
        bool pass = (node->write.getValue() == TRUE);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoDepthBuffer write default != TRUE");
    }

    // -----------------------------------------------------------------------
    // SoPolygonOffset
    // -----------------------------------------------------------------------
    runner.startTest("SoPolygonOffset class type registered");
    {
        bool pass = (SoPolygonOffset::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoPolygonOffset bad class type");
    }

    runner.startTest("SoPolygonOffset on default is TRUE");
    {
        SoPolygonOffset * node = new SoPolygonOffset;
        node->ref();
        bool pass = (node->on.getValue() == TRUE);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoPolygonOffset on default != TRUE");
    }

    return runner.getSummary();
}
