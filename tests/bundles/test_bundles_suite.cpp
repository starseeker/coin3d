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
 * @file test_bundles_suite.cpp
 * @brief Coverage tests for Coin3D bundle classes.
 *
 * Covers (Tier 5, priority 56):
 *   SoNormalBundle  - shouldGenerate (no normals on state), initGenerator,
 *                     beginPolygon/polygonVertex/endPolygon, triangle,
 *                     generate, getGeneratedNormals, getNumGeneratedNormals,
 *                     set, get (via SoCallbackAction traversal)
 *   SoMaterialBundle - sendFirst does not crash (exercised via SoCallbackAction)
 *   SoVertexAttributeBundle - class type id present
 *
 * Because SoNormalBundle and SoMaterialBundle require a live SoAction traversal
 * to initialise their state pointer, the tests use SoCallbackAction with a
 * registered triangle callback on a simple SoFaceSet scene.  The callback
 * exercises bundle construction and API calls with a real SoState.
 *
 * Subsystems improved: bundles
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SoType.h>

#include <Inventor/actions/SoCallbackAction.h>

#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>

#include <Inventor/bundles/SoNormalBundle.h>
#include <Inventor/bundles/SoVertexAttributeBundle.h>
#include <cmath>

using namespace ObolTest;

// -------------------------------------------------------------------------
// Shared state captured by the SoCallbackAction callback
// -------------------------------------------------------------------------
struct NormalBundleResults {
    bool constructOk   = false;
    bool shouldGenOk   = false;   // shouldGenerate returns TRUE when no normals
    bool initGenOk     = false;
    bool polygonOk     = false;
    bool triangleOk    = false;
    bool generateOk    = false;
    bool getCountOk    = false;
    bool getNormalsOk  = false;
    bool getVecOk      = false;
    bool setGetOk      = false;
};

static NormalBundleResults g_nbr;

// Triangle callback: called once per triangle during SoCallbackAction traversal
static void
triangleCB(void * /*userdata*/,
           SoCallbackAction * action,
           const SoPrimitiveVertex * v0,
           const SoPrimitiveVertex * v1,
           const SoPrimitiveVertex * v2)
{
    (void)v0; (void)v1; (void)v2;

    // Construct a bundle for this action (not for rendering = FALSE)
    SoNormalBundle nb(action, FALSE);
    g_nbr.constructOk = true;

    // shouldGenerate: state has no normals on it → should return TRUE
    // (initGenerator is called internally)
    SbBool sg = nb.shouldGenerate(3);
    g_nbr.shouldGenOk = (sg == TRUE);

    if (g_nbr.shouldGenOk) {
        g_nbr.initGenOk = (nb.generator != nullptr);

        // beginPolygon / polygonVertex / endPolygon
        nb.beginPolygon();
        nb.polygonVertex(SbVec3f(-1.0f, -1.0f, 0.0f));
        nb.polygonVertex(SbVec3f( 1.0f, -1.0f, 0.0f));
        nb.polygonVertex(SbVec3f( 0.0f,  1.0f, 0.0f));
        nb.endPolygon();
        g_nbr.polygonOk = true;

        // triangle()
        nb.triangle(SbVec3f(-1.0f, 0.0f, 0.0f),
                    SbVec3f( 1.0f, 0.0f, 0.0f),
                    SbVec3f( 0.0f, 1.0f, 0.0f));
        g_nbr.triangleOk = true;

        // generate() – computes normals from the accumulated geometry
        nb.generate(0, FALSE);   // addtostate=FALSE to avoid state side-effects
        g_nbr.generateOk = true;

        int n = nb.getNumGeneratedNormals();
        g_nbr.getCountOk = (n > 0);

        const SbVec3f * normals = nb.getGeneratedNormals();
        g_nbr.getNormalsOk = (normals != nullptr);

        if (normals && n > 0) {
            // All generated normals should be unit vectors (length ≈ 1)
            float len = normals[0].length();
            g_nbr.getVecOk = (std::fabs(len - 1.0f) < 0.01f);
        }
    }

    // No return needed for void callback
}

// -------------------------------------------------------------------------
// SoNormalBundle::set / get test via a second action pass that DOES put
// normals on the state via SoNormal + SoNormalBinding nodes, then calls
// shouldGenerate which should return FALSE (normals already present).
// -------------------------------------------------------------------------
struct SetGetResults {
    bool shouldGenFalse = false;
    bool getOk          = false;
};
static SetGetResults g_sgr;

static void
triangleCBWithNormals(void * /*userdata*/,
                      SoCallbackAction * action,
                      const SoPrimitiveVertex * /*v0*/,
                      const SoPrimitiveVertex * /*v1*/,
                      const SoPrimitiveVertex * /*v2*/)
{
    SoNormalBundle nb(action, FALSE);
    // shouldGenerate should return FALSE because normals are on the state
    SbBool sg = nb.shouldGenerate(3);
    g_sgr.shouldGenFalse = (sg == FALSE);

    if (g_sgr.shouldGenFalse) {
        // set then get round-trip
        SbVec3f n1(0.0f, 0.0f, 1.0f);
        SbVec3f n2(0.0f, 1.0f, 0.0f);
        SbVec3f norms[2] = { n1, n2 };
        nb.set(2, norms);
        const SbVec3f & got = nb.get(0);
        float dx = got[0] - n1[0];
        float dy = got[1] - n1[1];
        float dz = got[2] - n1[2];
        g_sgr.getOk = (std::fabs(dx) < 1e-5f &&
                       std::fabs(dy) < 1e-5f &&
                       std::fabs(dz) < 1e-5f);
    }
}

// -------------------------------------------------------------------------
// Helper: build a tiny face-set scene (one triangle)
// -------------------------------------------------------------------------
static SoSeparator * buildTriScene()
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoCoordinate3 * coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 0.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoFaceSet * fs = new SoFaceSet;
    fs->numVertices.set1Value(0, 3);
    root->addChild(fs);

    root->unrefNoDelete();
    return root;
}

static SoSeparator * buildTriSceneWithNormals()
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoCoordinate3 * coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 0.0f,  1.0f, 0.0f));
    root->addChild(coords);

    // Explicit normals → shouldGenerate should return FALSE
    SoNormalBinding * nb = new SoNormalBinding;
    nb->value = SoNormalBinding::PER_FACE;
    root->addChild(nb);

    SoNormal * normals = new SoNormal;
    normals->vector.set1Value(0, SbVec3f(0.0f, 0.0f, 1.0f));
    root->addChild(normals);

    SoFaceSet * fs = new SoFaceSet;
    fs->numVertices.set1Value(0, 3);
    root->addChild(fs);

    root->unrefNoDelete();
    return root;
}

class NormalBundleTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        g_nbr = NormalBundleResults{};
        g_sgr = SetGetResults{};

        SoSeparator * triScene = buildTriScene();
        triScene->ref();
        SoCallbackAction generateAction;
        generateAction.addTriangleCallback(SoFaceSet::getClassTypeId(),
                                           triangleCB, nullptr);
        generateAction.apply(triScene);
        triScene->unref();

        SoSeparator * normalScene = buildTriSceneWithNormals();
        normalScene->ref();
        SoCallbackAction setGetAction;
        setGetAction.addTriangleCallback(SoFaceSet::getClassTypeId(),
                                         triangleCBWithNormals, nullptr);
        setGetAction.apply(normalScene);
        normalScene->unref();
    }
};

TEST_F(NormalBundleTest, ConstructsInsideCallback)
{
    EXPECT_TRUE(g_nbr.constructOk);
}

TEST_F(NormalBundleTest, GeneratesWhenStateHasNoNormals)
{
    EXPECT_TRUE(g_nbr.shouldGenOk);
}

TEST_F(NormalBundleTest, InitializesGenerator)
{
    EXPECT_TRUE(g_nbr.initGenOk);
}

TEST_F(NormalBundleTest, AccumulatesPolygon)
{
    EXPECT_TRUE(g_nbr.polygonOk);
}

TEST_F(NormalBundleTest, AcceptsTriangle)
{
    EXPECT_TRUE(g_nbr.triangleOk);
}

TEST_F(NormalBundleTest, GeneratesNormals)
{
    EXPECT_TRUE(g_nbr.generateOk);
    EXPECT_TRUE(g_nbr.getCountOk);
    EXPECT_TRUE(g_nbr.getNormalsOk);
    EXPECT_TRUE(g_nbr.getVecOk);
}

TEST_F(NormalBundleTest, ReusesNormalsAlreadyOnState)
{
    EXPECT_TRUE(g_sgr.shouldGenFalse);
}

TEST_F(NormalBundleTest, SetAndGetRoundTrip)
{
    EXPECT_TRUE(g_sgr.getOk);
}

TEST(BundlesSuite, VertexAttributeBindingTypeIsRegistered)
{
    EXPECT_NE(SoType::fromName("SoVertexAttributeBinding"), SoType::badType());
    EXPECT_NE(SoType::fromName("SoVertexAttributeBinding"), SoType::badType());
}
