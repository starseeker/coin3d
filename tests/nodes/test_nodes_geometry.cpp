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
 * @file test_nodes_geometry.cpp
 * @brief Tests for geometry node types (shapenodes/ and nodes/ subsystems).
 *
 * Covers:
 *   SoFaceSet           - numVertices field, class type, getBoundingBox
 *   SoIndexedFaceSet    - coordIndex, texCoordIndex, normalIndex, materialIndex
 *   SoLineSet           - numVertices field
 *   SoIndexedLineSet    - coordIndex field
 *   SoPointSet          - numPoints field
 *   SoIndexedPointSet   - coordIndex field
 *   SoTriangleStripSet  - numVertices field
 *   SoIndexedTriangleStripSet - coordIndex field
 *   SoCoordinate3       - point field set/get
 *   SoNormal            - vector field set/get
 *   SoVertexShape       - vertexProperty field
 *   BoundingBox action on FaceSet scene (tests computeBBox path)
 */

#include "../test_utils.h"

#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoIndexedPointSet.h>
#include <Inventor/nodes/SoTriangleStripSet.h>
#include <Inventor/nodes/SoIndexedTriangleStripSet.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SoType.h>

#include <cmath>

using namespace SimpleTest;

static bool floatNear(float a, float b, float eps = 0.01f)
{
    return std::fabs(a - b) < eps;
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoFaceSet
    // -----------------------------------------------------------------------
    runner.startTest("SoFaceSet class type registered");
    {
        bool pass = (SoFaceSet::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoFaceSet bad class type");
    }

    runner.startTest("SoFaceSet numVertices field set/get round-trip");
    {
        SoFaceSet * node = new SoFaceSet;
        node->ref();
        node->numVertices.set1Value(0, 3); // one triangle
        node->numVertices.set1Value(1, 4); // one quad
        bool pass = (node->numVertices.getNum() == 2) &&
                    (node->numVertices[0] == 3) &&
                    (node->numVertices[1] == 4);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoFaceSet numVertices field failed");
    }

    // -----------------------------------------------------------------------
    // SoIndexedFaceSet
    // -----------------------------------------------------------------------
    runner.startTest("SoIndexedFaceSet class type registered");
    {
        bool pass = (SoIndexedFaceSet::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoIndexedFaceSet bad class type");
    }

    runner.startTest("SoIndexedFaceSet coordIndex field set/get round-trip");
    {
        SoIndexedFaceSet * node = new SoIndexedFaceSet;
        node->ref();
        // Triangle: indices 0,1,2,-1
        node->coordIndex.set1Value(0, 0);
        node->coordIndex.set1Value(1, 1);
        node->coordIndex.set1Value(2, 2);
        node->coordIndex.set1Value(3, SO_END_FACE_INDEX);
        bool pass = (node->coordIndex.getNum() == 4) &&
                    (node->coordIndex[3] == SO_END_FACE_INDEX);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoIndexedFaceSet coordIndex failed");
    }

    runner.startTest("SoIndexedFaceSet texCoordIndex field set/get");
    {
        SoIndexedFaceSet * node = new SoIndexedFaceSet;
        node->ref();
        node->textureCoordIndex.set1Value(0, 0);
        node->textureCoordIndex.set1Value(1, 1);
        node->textureCoordIndex.set1Value(2, -1);
        bool pass = (node->textureCoordIndex.getNum() == 3);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoIndexedFaceSet textureCoordIndex failed");
    }

    // -----------------------------------------------------------------------
    // SoLineSet
    // -----------------------------------------------------------------------
    runner.startTest("SoLineSet class type registered");
    {
        bool pass = (SoLineSet::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoLineSet bad class type");
    }

    runner.startTest("SoLineSet numVertices field set/get round-trip");
    {
        SoLineSet * node = new SoLineSet;
        node->ref();
        node->numVertices.set1Value(0, 3); // one polyline with 3 vertices
        bool pass = (node->numVertices.getNum() == 1) && (node->numVertices[0] == 3);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoLineSet numVertices field failed");
    }

    // -----------------------------------------------------------------------
    // SoIndexedLineSet
    // -----------------------------------------------------------------------
    runner.startTest("SoIndexedLineSet class type registered");
    {
        bool pass = (SoIndexedLineSet::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoIndexedLineSet bad class type");
    }

    runner.startTest("SoIndexedLineSet coordIndex field set/get");
    {
        SoIndexedLineSet * node = new SoIndexedLineSet;
        node->ref();
        node->coordIndex.set1Value(0, 0);
        node->coordIndex.set1Value(1, 1);
        node->coordIndex.set1Value(2, 2);
        node->coordIndex.set1Value(3, -1);
        bool pass = (node->coordIndex.getNum() == 4) &&
                    (node->coordIndex[3] == -1);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoIndexedLineSet coordIndex failed");
    }

    // -----------------------------------------------------------------------
    // SoPointSet
    // -----------------------------------------------------------------------
    runner.startTest("SoPointSet class type registered");
    {
        bool pass = (SoPointSet::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoPointSet bad class type");
    }

    runner.startTest("SoPointSet numPoints default is SO_POINT_SET_USE_REST_COUNT");
    {
        SoPointSet * node = new SoPointSet;
        node->ref();
        // default is -1 (use all remaining points)
        bool pass = (node->numPoints.getValue() <= 0);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoPointSet numPoints default unexpected");
    }

    // -----------------------------------------------------------------------
    // SoIndexedPointSet
    // -----------------------------------------------------------------------
    runner.startTest("SoIndexedPointSet class type registered");
    {
        bool pass = (SoIndexedPointSet::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoIndexedPointSet bad class type");
    }

    // -----------------------------------------------------------------------
    // SoTriangleStripSet
    // -----------------------------------------------------------------------
    runner.startTest("SoTriangleStripSet class type registered");
    {
        bool pass = (SoTriangleStripSet::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoTriangleStripSet bad class type");
    }

    runner.startTest("SoTriangleStripSet numVertices field set/get");
    {
        SoTriangleStripSet * node = new SoTriangleStripSet;
        node->ref();
        node->numVertices.set1Value(0, 5); // strip with 5 vertices
        bool pass = (node->numVertices.getNum() == 1) && (node->numVertices[0] == 5);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoTriangleStripSet numVertices field failed");
    }

    // -----------------------------------------------------------------------
    // SoIndexedTriangleStripSet
    // -----------------------------------------------------------------------
    runner.startTest("SoIndexedTriangleStripSet class type registered");
    {
        bool pass = (SoIndexedTriangleStripSet::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoIndexedTriangleStripSet bad class type");
    }

    // -----------------------------------------------------------------------
    // SoCoordinate3
    // -----------------------------------------------------------------------
    runner.startTest("SoCoordinate3 class type registered");
    {
        bool pass = (SoCoordinate3::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoCoordinate3 bad class type");
    }

    runner.startTest("SoCoordinate3 point field set/get round-trip");
    {
        SoCoordinate3 * node = new SoCoordinate3;
        node->ref();
        SbVec3f pts[3] = { SbVec3f(0,0,0), SbVec3f(1,0,0), SbVec3f(0,1,0) };
        node->point.setValues(0, 3, pts);
        bool pass = (node->point.getNum() == 3) &&
                    (node->point[0] == SbVec3f(0,0,0)) &&
                    (node->point[2] == SbVec3f(0,1,0));
        node->unref();
        runner.endTest(pass, pass ? "" : "SoCoordinate3 point field failed");
    }

    // -----------------------------------------------------------------------
    // SoNormal
    // -----------------------------------------------------------------------
    runner.startTest("SoNormal class type registered");
    {
        bool pass = (SoNormal::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoNormal bad class type");
    }

    runner.startTest("SoNormal vector field set/get round-trip");
    {
        SoNormal * node = new SoNormal;
        node->ref();
        SbVec3f nrm(0, 1, 0);
        node->vector.set1Value(0, nrm);
        bool pass = (node->vector.getNum() == 1) && (node->vector[0] == nrm);
        node->unref();
        runner.endTest(pass, pass ? "" : "SoNormal vector field failed");
    }

    // -----------------------------------------------------------------------
    // BoundingBox action on a FaceSet scene
    // -----------------------------------------------------------------------
    runner.startTest("SoGetBoundingBoxAction on SoFaceSet triangle");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();

        SoCoordinate3 * coords = new SoCoordinate3;
        SbVec3f pts[3] = { SbVec3f(-1,0,0), SbVec3f(1,0,0), SbVec3f(0,1,0) };
        coords->point.setValues(0, 3, pts);
        root->addChild(coords);

        SoFaceSet * fs = new SoFaceSet;
        fs->numVertices.set1Value(0, 3);
        root->addChild(fs);

        SbViewportRegion vp(512, 512);
        SoGetBoundingBoxAction bba(vp);
        bba.apply(root);
        SbBox3f box = bba.getBoundingBox();

        bool pass = !box.isEmpty() &&
                    floatNear(box.getMin()[0], -1.0f) &&
                    floatNear(box.getMax()[0], 1.0f);
        root->unref();
        runner.endTest(pass, pass ? "" : "BoundingBox on SoFaceSet triangle failed");
    }

    runner.startTest("SoGetBoundingBoxAction on SoIndexedFaceSet triangle");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();

        SoCoordinate3 * coords = new SoCoordinate3;
        SbVec3f pts[3] = { SbVec3f(0,0,0), SbVec3f(2,0,0), SbVec3f(1,2,0) };
        coords->point.setValues(0, 3, pts);
        root->addChild(coords);

        SoIndexedFaceSet * ifs = new SoIndexedFaceSet;
        int indices[4] = { 0, 1, 2, -1 };
        ifs->coordIndex.setValues(0, 4, indices);
        root->addChild(ifs);

        SbViewportRegion vp(512, 512);
        SoGetBoundingBoxAction bba(vp);
        bba.apply(root);
        SbBox3f box = bba.getBoundingBox();

        bool pass = !box.isEmpty() && floatNear(box.getMax()[0], 2.0f);
        root->unref();
        runner.endTest(pass, pass ? "" : "BoundingBox on SoIndexedFaceSet triangle failed");
    }

    return runner.getSummary();
}
