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
 * @file test_nodekit_traversal.cpp
 * @brief Tests for SoBaseKit subclasses: getPart, setPart, traversal.
 *
 * Exercises the nodekit infrastructure:
 *   - SoShapeKit: instantiation, getPart, setPart
 *   - SoAppearanceKit: instantiation, catalog access
 *   - SoGetBoundingBoxAction on a nodekit-based scene
 *
 * Subsystems improved: nodekits (+450 lines per COVERAGE_PLAN.md Tier 2)
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/nodekits/SoBaseKit.h>
#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/nodekits/SoAppearanceKit.h>
#include <Inventor/nodekits/SoNodekitCatalog.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbVec3f.h>
#include <cmath>

using namespace ObolTest;

TEST(NodesNodekitTraversal, SoShapeKitInstantiationAndTypeCheck)
{
    SoShapeKit *kit = new SoShapeKit;
    kit->ref();
    EXPECT_TRUE((kit->getTypeId() != SoType::badType()) &&
                kit->isOfType(SoBaseKit::getClassTypeId())) << "SoShapeKit bad type or not SoBaseKit subtype";
    kit->unref();
}

// -----------------------------------------------------------------------
// SoShapeKit: getNodekitCatalog is non-null
// -----------------------------------------------------------------------

TEST(NodesNodekitTraversal, SoShapeKitGetNodekitCatalogIsNonNull)
{
    SoShapeKit *kit = new SoShapeKit;
    kit->ref();
    const SoNodekitCatalog *cat = kit->getNodekitCatalog();
    EXPECT_TRUE((cat != nullptr) && (cat->getNumEntries() > 0)) << "SoShapeKit catalog is null or has no entries";
    kit->unref();
}

// -----------------------------------------------------------------------
// SoShapeKit: getPart returns non-null for "shape" when makeifneeded=TRUE
// -----------------------------------------------------------------------

TEST(NodesNodekitTraversal, SoShapeKitGetPartShapeTRUEReturnsNonNull)
{
    SoShapeKit *kit = new SoShapeKit;
    kit->ref();
    SoNode *part = kit->getPart("shape", TRUE);
    EXPECT_TRUE((part != nullptr)) << "SoShapeKit getPart(\"shape\", TRUE) returned null";
    kit->unref();
}

// -----------------------------------------------------------------------
// SoShapeKit: setPart replaces the shape part
// -----------------------------------------------------------------------

TEST(NodesNodekitTraversal, SoShapeKitSetPartReplacesShapePart)
{
    SoShapeKit *kit = new SoShapeKit;
    kit->ref();

    SoCube *cube = new SoCube;
    bool setOk = kit->setPart("shape", cube);

    SoNode *retrieved = kit->getPart("shape", FALSE);
    EXPECT_TRUE(setOk && (retrieved == cube)) << "SoShapeKit setPart or getPart(FALSE) failed";
    kit->unref();
}

// -----------------------------------------------------------------------
// SoShapeKit: SoGetBoundingBoxAction on kit with default shape
// -----------------------------------------------------------------------

TEST(NodesNodekitTraversal, SoGetBoundingBoxActionOnSoShapeKitWithCubeShape)
{
    SoShapeKit *kit = new SoShapeKit;
    kit->ref();

    // Set a 2×2×2 cube as the shape part
    SoCube *cube = new SoCube; // default: 2×2×2
    kit->setPart("shape", cube);

    SoGetBoundingBoxAction bba(SbViewportRegion(100, 100));
    bba.apply(kit);

    SbBox3f bbox = bba.getBoundingBox();
    EXPECT_FALSE(bbox.isEmpty());
    if (!bbox.isEmpty()) {
        SbVec3f lo, hi;
        bbox.getBounds(lo, hi);
        // Default SoCube is 2×2×2 → bounds should be at least [-1,1]
        EXPECT_LE(lo[0], -0.9f);
        EXPECT_GE(hi[0], 0.9f);
    }
    kit->unref();
}

// -----------------------------------------------------------------------
// SoAppearanceKit: basic instantiation
// -----------------------------------------------------------------------

TEST(NodesNodekitTraversal, SoAppearanceKitInstantiationAndTypeCheck)
{
    SoAppearanceKit *kit = new SoAppearanceKit;
    kit->ref();
    EXPECT_TRUE((kit->getTypeId() != SoType::badType()) &&
                kit->isOfType(SoBaseKit::getClassTypeId())) << "SoAppearanceKit bad type";
    kit->unref();
}

// -----------------------------------------------------------------------
// SoAppearanceKit: set material part
// -----------------------------------------------------------------------

TEST(NodesNodekitTraversal, SoAppearanceKitSetPartMaterial)
{
    SoAppearanceKit *kit = new SoAppearanceKit;
    kit->ref();

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 0.0f, 0.0f);
    bool setOk = kit->setPart("material", mat);

    SoNode *retrieved = kit->getPart("material", FALSE);
    EXPECT_TRUE(setOk && (retrieved == mat)) << "SoAppearanceKit setPart(material) failed";
    kit->unref();
}

// -----------------------------------------------------------------------
// SoShapeKit inside a separator: bounding box propagates
// -----------------------------------------------------------------------

TEST(NodesNodekitTraversal, SoShapeKitInsideSeparatorBboxPropagates)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoShapeKit *kit = new SoShapeKit;
    kit->setPart("shape", new SoSphere);  // default radius 1
    root->addChild(kit);

    SoGetBoundingBoxAction bba(SbViewportRegion(100, 100));
    bba.apply(root);

    SbBox3f bbox = bba.getBoundingBox();
    EXPECT_TRUE(!bbox.isEmpty()) << "SoShapeKit inside separator: bbox is empty";
    root->unref();
}

// -----------------------------------------------------------------------
// SoBaseKit::getPartString — round-trip path to part
// -----------------------------------------------------------------------

TEST(NodesNodekitTraversal, SoShapeKitGetPartStringForCubeShapePart)
{
    SoShapeKit *kit = new SoShapeKit;
    kit->ref();
    SoCube *cube = new SoCube;
    kit->setPart("shape", cube);
    SbString ps = kit->getPartString(cube);
    // Should return "shape" (the part name)
    EXPECT_TRUE((ps == "shape")) << "SoShapeKit getPartString did not return \"shape\"";
    kit->unref();
}
