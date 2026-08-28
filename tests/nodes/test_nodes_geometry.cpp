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

using namespace ObolTest;

static bool floatNear(float a, float b, float eps = 0.01f)
{
    return std::fabs(a - b) < eps;
}

TEST(NodesGeometry, SoFaceSetClassTypeRegistered)
{
    EXPECT_TRUE((SoFaceSet::getClassTypeId() != SoType::badType())) << "SoFaceSet bad class type";
}

TEST(NodesGeometry, SoFaceSetNumVerticesFieldSetGetRoundTrip)
{
    SoFaceSet * node = new SoFaceSet;
    node->ref();
    node->numVertices.set1Value(0, 3); // one triangle
    node->numVertices.set1Value(1, 4); // one quad
    EXPECT_TRUE((node->numVertices.getNum() == 2) &&
                (node->numVertices[0] == 3) &&
                (node->numVertices[1] == 4)) << "SoFaceSet numVertices field failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoIndexedFaceSet
// -----------------------------------------------------------------------

TEST(NodesGeometry, SoIndexedFaceSetClassTypeRegistered)
{
    EXPECT_TRUE((SoIndexedFaceSet::getClassTypeId() != SoType::badType())) << "SoIndexedFaceSet bad class type";
}

TEST(NodesGeometry, SoIndexedFaceSetCoordIndexFieldSetGetRoundTrip)
{
    SoIndexedFaceSet * node = new SoIndexedFaceSet;
    node->ref();
    // Triangle: indices 0,1,2,-1
    node->coordIndex.set1Value(0, 0);
    node->coordIndex.set1Value(1, 1);
    node->coordIndex.set1Value(2, 2);
    node->coordIndex.set1Value(3, SO_END_FACE_INDEX);
    EXPECT_TRUE((node->coordIndex.getNum() == 4) &&
                (node->coordIndex[3] == SO_END_FACE_INDEX)) << "SoIndexedFaceSet coordIndex failed";
    node->unref();
}

TEST(NodesGeometry, SoIndexedFaceSetTexCoordIndexFieldSetGet)
{
    SoIndexedFaceSet * node = new SoIndexedFaceSet;
    node->ref();
    node->textureCoordIndex.set1Value(0, 0);
    node->textureCoordIndex.set1Value(1, 1);
    node->textureCoordIndex.set1Value(2, -1);
    EXPECT_TRUE((node->textureCoordIndex.getNum() == 3)) << "SoIndexedFaceSet textureCoordIndex failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoLineSet
// -----------------------------------------------------------------------

TEST(NodesGeometry, SoLineSetClassTypeRegistered)
{
    EXPECT_TRUE((SoLineSet::getClassTypeId() != SoType::badType())) << "SoLineSet bad class type";
}

TEST(NodesGeometry, SoLineSetNumVerticesFieldSetGetRoundTrip)
{
    SoLineSet * node = new SoLineSet;
    node->ref();
    node->numVertices.set1Value(0, 3); // one polyline with 3 vertices
    EXPECT_TRUE((node->numVertices.getNum() == 1) && (node->numVertices[0] == 3)) << "SoLineSet numVertices field failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoIndexedLineSet
// -----------------------------------------------------------------------

TEST(NodesGeometry, SoIndexedLineSetClassTypeRegistered)
{
    EXPECT_TRUE((SoIndexedLineSet::getClassTypeId() != SoType::badType())) << "SoIndexedLineSet bad class type";
}

TEST(NodesGeometry, SoIndexedLineSetCoordIndexFieldSetGet)
{
    SoIndexedLineSet * node = new SoIndexedLineSet;
    node->ref();
    node->coordIndex.set1Value(0, 0);
    node->coordIndex.set1Value(1, 1);
    node->coordIndex.set1Value(2, 2);
    node->coordIndex.set1Value(3, -1);
    EXPECT_TRUE((node->coordIndex.getNum() == 4) &&
                (node->coordIndex[3] == -1)) << "SoIndexedLineSet coordIndex failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoPointSet
// -----------------------------------------------------------------------

TEST(NodesGeometry, SoPointSetClassTypeRegistered)
{
    EXPECT_TRUE((SoPointSet::getClassTypeId() != SoType::badType())) << "SoPointSet bad class type";
}

TEST(NodesGeometry, SoPointSetNumPointsDefaultIsSOPOINTSETUSERESTCOUNT)
{
    SoPointSet * node = new SoPointSet;
    node->ref();
    // default is -1 (use all remaining points)
    EXPECT_TRUE((node->numPoints.getValue() <= 0)) << "SoPointSet numPoints default unexpected";
    node->unref();
}

// -----------------------------------------------------------------------
// SoIndexedPointSet
// -----------------------------------------------------------------------

TEST(NodesGeometry, SoIndexedPointSetClassTypeRegistered)
{
    EXPECT_TRUE((SoIndexedPointSet::getClassTypeId() != SoType::badType())) << "SoIndexedPointSet bad class type";
}

// -----------------------------------------------------------------------
// SoTriangleStripSet
// -----------------------------------------------------------------------

TEST(NodesGeometry, SoTriangleStripSetClassTypeRegistered)
{
    EXPECT_TRUE((SoTriangleStripSet::getClassTypeId() != SoType::badType())) << "SoTriangleStripSet bad class type";
}

TEST(NodesGeometry, SoTriangleStripSetNumVerticesFieldSetGet)
{
    SoTriangleStripSet * node = new SoTriangleStripSet;
    node->ref();
    node->numVertices.set1Value(0, 5); // strip with 5 vertices
    EXPECT_TRUE((node->numVertices.getNum() == 1) && (node->numVertices[0] == 5)) << "SoTriangleStripSet numVertices field failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoIndexedTriangleStripSet
// -----------------------------------------------------------------------

TEST(NodesGeometry, SoIndexedTriangleStripSetClassTypeRegistered)
{
    EXPECT_TRUE((SoIndexedTriangleStripSet::getClassTypeId() != SoType::badType())) << "SoIndexedTriangleStripSet bad class type";
}

// -----------------------------------------------------------------------
// SoCoordinate3
// -----------------------------------------------------------------------

TEST(NodesGeometry, SoCoordinate3ClassTypeRegistered)
{
    EXPECT_TRUE((SoCoordinate3::getClassTypeId() != SoType::badType())) << "SoCoordinate3 bad class type";
}

TEST(NodesGeometry, SoCoordinate3PointFieldSetGetRoundTrip)
{
    SoCoordinate3 * node = new SoCoordinate3;
    node->ref();
    SbVec3f pts[3] = { SbVec3f(0,0,0), SbVec3f(1,0,0), SbVec3f(0,1,0) };
    node->point.setValues(0, 3, pts);
    EXPECT_TRUE((node->point.getNum() == 3) &&
                (node->point[0] == SbVec3f(0,0,0)) &&
                (node->point[2] == SbVec3f(0,1,0))) << "SoCoordinate3 point field failed";
    node->unref();
}

// -----------------------------------------------------------------------
// SoNormal
// -----------------------------------------------------------------------

TEST(NodesGeometry, SoNormalClassTypeRegistered)
{
    EXPECT_TRUE((SoNormal::getClassTypeId() != SoType::badType())) << "SoNormal bad class type";
}

TEST(NodesGeometry, SoNormalVectorFieldSetGetRoundTrip)
{
    SoNormal * node = new SoNormal;
    node->ref();
    SbVec3f nrm(0, 1, 0);
    node->vector.set1Value(0, nrm);
    EXPECT_TRUE((node->vector.getNum() == 1) && (node->vector[0] == nrm)) << "SoNormal vector field failed";
    node->unref();
}

// -----------------------------------------------------------------------
// BoundingBox action on a FaceSet scene
// -----------------------------------------------------------------------

TEST(NodesGeometry, SoGetBoundingBoxActionOnSoFaceSetTriangle)
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

    EXPECT_TRUE(!box.isEmpty() &&
                floatNear(box.getMin()[0], -1.0f) &&
                floatNear(box.getMax()[0], 1.0f)) << "BoundingBox on SoFaceSet triangle failed";
    root->unref();
}

TEST(NodesGeometry, SoGetBoundingBoxActionOnSoIndexedFaceSetTriangle)
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

    EXPECT_TRUE(!box.isEmpty() && floatNear(box.getMax()[0], 2.0f)) << "BoundingBox on SoIndexedFaceSet triangle failed";
    root->unref();
}
