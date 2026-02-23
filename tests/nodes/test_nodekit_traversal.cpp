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

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoShapeKit: basic instantiation and type check
    // -----------------------------------------------------------------------
    runner.startTest("SoShapeKit instantiation and type check");
    {
        SoShapeKit *kit = new SoShapeKit;
        kit->ref();
        bool pass = (kit->getTypeId() != SoType::badType()) &&
                    kit->isOfType(SoBaseKit::getClassTypeId());
        kit->unref();
        runner.endTest(pass, pass ? "" : "SoShapeKit bad type or not SoBaseKit subtype");
    }

    // -----------------------------------------------------------------------
    // SoShapeKit: getNodekitCatalog is non-null
    // -----------------------------------------------------------------------
    runner.startTest("SoShapeKit getNodekitCatalog is non-null");
    {
        SoShapeKit *kit = new SoShapeKit;
        kit->ref();
        const SoNodekitCatalog *cat = kit->getNodekitCatalog();
        bool pass = (cat != nullptr) && (cat->getNumEntries() > 0);
        kit->unref();
        runner.endTest(pass, pass ? "" :
            "SoShapeKit catalog is null or has no entries");
    }

    // -----------------------------------------------------------------------
    // SoShapeKit: getPart returns non-null for "shape" when makeifneeded=TRUE
    // -----------------------------------------------------------------------
    runner.startTest("SoShapeKit getPart(\"shape\", TRUE) returns non-null");
    {
        SoShapeKit *kit = new SoShapeKit;
        kit->ref();
        SoNode *part = kit->getPart("shape", TRUE);
        bool pass = (part != nullptr);
        kit->unref();
        runner.endTest(pass, pass ? "" :
            "SoShapeKit getPart(\"shape\", TRUE) returned null");
    }

    // -----------------------------------------------------------------------
    // SoShapeKit: setPart replaces the shape part
    // -----------------------------------------------------------------------
    runner.startTest("SoShapeKit setPart replaces shape part");
    {
        SoShapeKit *kit = new SoShapeKit;
        kit->ref();

        SoCube *cube = new SoCube;
        bool setOk = kit->setPart("shape", cube);

        SoNode *retrieved = kit->getPart("shape", FALSE);
        bool pass = setOk && (retrieved == cube);
        kit->unref();
        runner.endTest(pass, pass ? "" :
            "SoShapeKit setPart or getPart(FALSE) failed");
    }

    // -----------------------------------------------------------------------
    // SoShapeKit: SoGetBoundingBoxAction on kit with default shape
    // -----------------------------------------------------------------------
    runner.startTest("SoGetBoundingBoxAction on SoShapeKit with cube shape");
    {
        SoShapeKit *kit = new SoShapeKit;
        kit->ref();

        // Set a 2×2×2 cube as the shape part
        SoCube *cube = new SoCube; // default: 2×2×2
        kit->setPart("shape", cube);

        SoGetBoundingBoxAction bba(SbViewportRegion(100, 100));
        bba.apply(kit);

        SbBox3f bbox = bba.getBoundingBox();
        bool pass = !bbox.isEmpty();
        if (pass) {
            SbVec3f lo, hi;
            bbox.getBounds(lo, hi);
            // Default SoCube is 2×2×2 → bounds should be at least [-1,1]
            pass = (lo[0] <= -0.9f) && (hi[0] >= 0.9f);
        }
        kit->unref();
        runner.endTest(pass, pass ? "" :
            "SoGetBoundingBoxAction on SoShapeKit returned empty/wrong bbox");
    }

    // -----------------------------------------------------------------------
    // SoAppearanceKit: basic instantiation
    // -----------------------------------------------------------------------
    runner.startTest("SoAppearanceKit instantiation and type check");
    {
        SoAppearanceKit *kit = new SoAppearanceKit;
        kit->ref();
        bool pass = (kit->getTypeId() != SoType::badType()) &&
                    kit->isOfType(SoBaseKit::getClassTypeId());
        kit->unref();
        runner.endTest(pass, pass ? "" : "SoAppearanceKit bad type");
    }

    // -----------------------------------------------------------------------
    // SoAppearanceKit: set material part
    // -----------------------------------------------------------------------
    runner.startTest("SoAppearanceKit setPart material");
    {
        SoAppearanceKit *kit = new SoAppearanceKit;
        kit->ref();

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(1.0f, 0.0f, 0.0f);
        bool setOk = kit->setPart("material", mat);

        SoNode *retrieved = kit->getPart("material", FALSE);
        bool pass = setOk && (retrieved == mat);
        kit->unref();
        runner.endTest(pass, pass ? "" :
            "SoAppearanceKit setPart(material) failed");
    }

    // -----------------------------------------------------------------------
    // SoShapeKit inside a separator: bounding box propagates
    // -----------------------------------------------------------------------
    runner.startTest("SoShapeKit inside separator: bbox propagates");
    {
        SoSeparator *root = new SoSeparator;
        root->ref();

        SoShapeKit *kit = new SoShapeKit;
        kit->setPart("shape", new SoSphere);  // default radius 1
        root->addChild(kit);

        SoGetBoundingBoxAction bba(SbViewportRegion(100, 100));
        bba.apply(root);

        SbBox3f bbox = bba.getBoundingBox();
        bool pass = !bbox.isEmpty();
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoShapeKit inside separator: bbox is empty");
    }

    // -----------------------------------------------------------------------
    // SoBaseKit::getPartString — round-trip path to part
    // -----------------------------------------------------------------------
    runner.startTest("SoShapeKit getPartString for cube shape part");
    {
        SoShapeKit *kit = new SoShapeKit;
        kit->ref();
        SoCube *cube = new SoCube;
        kit->setPart("shape", cube);
        SbString ps = kit->getPartString(cube);
        // Should return "shape" (the part name)
        bool pass = (ps == "shape");
        kit->unref();
        runner.endTest(pass, pass ? "" :
            "SoShapeKit getPartString did not return \"shape\"");
    }

    return runner.getSummary();
}
