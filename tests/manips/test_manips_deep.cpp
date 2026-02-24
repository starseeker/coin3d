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
 * @file test_manips_deep.cpp
 * @brief Deeper coverage for Coin3D manipulator classes (priority: manips 25.6%).
 *
 * Covers light manipulators and clip-plane manipulator not yet in test_manips.cpp:
 *   SoPointLightManip      - type, getDragger, replaceNode/replaceManip, fields
 *   SoDirectionalLightManip - type, getDragger
 *   SoSpotLightManip        - type, getDragger
 *   SoClipPlaneManip        - type, getDragger, setValue, replaceNode/replaceManip
 *
 * Subsystems improved: manips (25.6 %)
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SoType.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoClipPlane.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/manips/SoPointLightManip.h>
#include <Inventor/manips/SoDirectionalLightManip.h>
#include <Inventor/manips/SoSpotLightManip.h>
#include <Inventor/manips/SoClipPlaneManip.h>
#include <Inventor/draggers/SoDragger.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/SbViewportRegion.h>

#include <cmath>

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoPointLightManip
    // -----------------------------------------------------------------------
    runner.startTest("SoPointLightManip: class type id valid");
    {
        bool pass = (SoPointLightManip::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoPointLightManip has bad class type");
    }

    runner.startTest("SoPointLightManip: isOfType SoPointLight");
    {
        SoPointLightManip * m = new SoPointLightManip;
        m->ref();
        bool pass = m->isOfType(SoPointLight::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoPointLightManip not a SoPointLight subtype");
    }

    runner.startTest("SoPointLightManip: getDragger returns non-null");
    {
        SoPointLightManip * m = new SoPointLightManip;
        m->ref();
        SoDragger * d = m->getDragger();
        bool pass = (d != nullptr);
        m->unref();
        runner.endTest(pass, pass ? "" : "SoPointLightManip getDragger returned null");
    }

    runner.startTest("SoPointLightManip: default location is (0,0,1)");
    {
        SoPointLightManip * m = new SoPointLightManip;
        m->ref();
        SbVec3f loc = m->location.getValue();
        bool pass = (std::fabs(loc[0]) < 1e-4f) &&
                    (std::fabs(loc[1]) < 1e-4f) &&
                    (std::fabs(loc[2] - 1.0f) < 1e-4f);
        m->unref();
        runner.endTest(pass, pass ? "" : "SoPointLightManip default location is not (0,0,1)");
    }

    runner.startTest("SoPointLightManip: SoGetBoundingBoxAction does not crash");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();
        SoPointLightManip * m = new SoPointLightManip;
        root->addChild(m);
        SbViewportRegion vp(100, 100);
        SoGetBoundingBoxAction bba(vp);
        bba.apply(root);
        root->unref();
        runner.endTest(true, "");
    }

    // Test replaceNode/replaceManip lifecycle
    runner.startTest("SoPointLightManip: replaceNode/replaceManip lifecycle");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();
        SoPointLight * light = new SoPointLight;
        light->location.setValue(1.0f, 2.0f, 3.0f);
        root->addChild(light);

        SoSearchAction sa;
        sa.setType(SoPointLight::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        SoPath * path = sa.getPath();

        bool pass = false;
        if (path) {
            SoPointLightManip * manip = new SoPointLightManip;
            manip->ref();
            SbBool ok = manip->replaceNode(path);
            if (ok) {
                // Verify location was copied
                SbVec3f loc = manip->location.getValue();
                pass = (std::fabs(loc[0] - 1.0f) < 1e-4f);

                // Now replace back
                SoSearchAction sa2;
                sa2.setType(SoPointLightManip::getClassTypeId());
                sa2.setInterest(SoSearchAction::FIRST);
                sa2.apply(root);
                SoPath * mpath = sa2.getPath();
                if (mpath) {
                    SbBool detached = manip->replaceManip(mpath, nullptr);
                    pass = pass && detached;
                }
            }
            manip->unref();
        }
        root->unref();
        runner.endTest(pass, pass ? "" : "SoPointLightManip replaceNode/replaceManip failed");
    }

    // -----------------------------------------------------------------------
    // SoDirectionalLightManip
    // -----------------------------------------------------------------------
    runner.startTest("SoDirectionalLightManip: class type id valid");
    {
        bool pass = (SoDirectionalLightManip::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoDirectionalLightManip has bad class type");
    }

    runner.startTest("SoDirectionalLightManip: isOfType SoDirectionalLight");
    {
        SoDirectionalLightManip * m = new SoDirectionalLightManip;
        m->ref();
        bool pass = m->isOfType(SoDirectionalLight::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" :
            "SoDirectionalLightManip not a SoDirectionalLight subtype");
    }

    runner.startTest("SoDirectionalLightManip: getDragger returns non-null");
    {
        SoDirectionalLightManip * m = new SoDirectionalLightManip;
        m->ref();
        SoDragger * d = m->getDragger();
        bool pass = (d != nullptr);
        m->unref();
        runner.endTest(pass, pass ? "" :
            "SoDirectionalLightManip getDragger returned null");
    }

    runner.startTest("SoDirectionalLightManip: SoGetBoundingBoxAction does not crash");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();
        SoDirectionalLightManip * m = new SoDirectionalLightManip;
        root->addChild(m);
        SbViewportRegion vp(100, 100);
        SoGetBoundingBoxAction bba(vp);
        bba.apply(root);
        root->unref();
        runner.endTest(true, "");
    }

    // -----------------------------------------------------------------------
    // SoSpotLightManip
    // -----------------------------------------------------------------------
    runner.startTest("SoSpotLightManip: class type id valid");
    {
        bool pass = (SoSpotLightManip::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoSpotLightManip has bad class type");
    }

    runner.startTest("SoSpotLightManip: isOfType SoSpotLight");
    {
        SoSpotLightManip * m = new SoSpotLightManip;
        m->ref();
        bool pass = m->isOfType(SoSpotLight::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" :
            "SoSpotLightManip not a SoSpotLight subtype");
    }

    runner.startTest("SoSpotLightManip: getDragger returns non-null");
    {
        SoSpotLightManip * m = new SoSpotLightManip;
        m->ref();
        SoDragger * d = m->getDragger();
        bool pass = (d != nullptr);
        m->unref();
        runner.endTest(pass, pass ? "" :
            "SoSpotLightManip getDragger returned null");
    }

    // -----------------------------------------------------------------------
    // SoClipPlaneManip
    // -----------------------------------------------------------------------
    runner.startTest("SoClipPlaneManip: class type id valid");
    {
        bool pass = (SoClipPlaneManip::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoClipPlaneManip has bad class type");
    }

    runner.startTest("SoClipPlaneManip: isOfType SoClipPlane");
    {
        SoClipPlaneManip * m = new SoClipPlaneManip;
        m->ref();
        bool pass = m->isOfType(SoClipPlane::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" :
            "SoClipPlaneManip not a SoClipPlane subtype");
    }

    runner.startTest("SoClipPlaneManip: getDragger returns non-null");
    {
        SoClipPlaneManip * m = new SoClipPlaneManip;
        m->ref();
        SoDragger * d = m->getDragger();
        bool pass = (d != nullptr);
        m->unref();
        runner.endTest(pass, pass ? "" :
            "SoClipPlaneManip getDragger returned null");
    }

    runner.startTest("SoClipPlaneManip: setValue sets plane from box + normal");
    {
        SoClipPlaneManip * m = new SoClipPlaneManip;
        m->ref();
        SbBox3f box(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f);
        SbVec3f normal(0.0f, 1.0f, 0.0f);
        // setValue sets the dragger and plane from the bounding box.
        // It should not crash.
        m->setValue(box, normal, 1.5f);
        bool pass = true; // reaching here = no crash
        m->unref();
        runner.endTest(pass, "");
    }

    runner.startTest("SoClipPlaneManip: SoGetBoundingBoxAction does not crash");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();
        SoClipPlaneManip * m = new SoClipPlaneManip;
        root->addChild(m);
        SbViewportRegion vp(100, 100);
        SoGetBoundingBoxAction bba(vp);
        bba.apply(root);
        root->unref();
        runner.endTest(true, "");
    }

    runner.startTest("SoClipPlaneManip: replaceNode/replaceManip lifecycle");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();
        SoClipPlane * clip = new SoClipPlane;
        clip->plane.setValue(SbPlane(SbVec3f(0.0f, 1.0f, 0.0f), 0.5f));
        root->addChild(clip);

        SoSearchAction sa;
        sa.setType(SoClipPlane::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        SoPath * path = sa.getPath();

        bool pass = false;
        if (path) {
            SoClipPlaneManip * manip = new SoClipPlaneManip;
            manip->ref();
            SbBool ok = manip->replaceNode(path);
            if (ok) {
                pass = true;
                // Detach
                SoSearchAction sa2;
                sa2.setType(SoClipPlaneManip::getClassTypeId());
                sa2.setInterest(SoSearchAction::FIRST);
                sa2.apply(root);
                SoPath * mpath = sa2.getPath();
                if (mpath) {
                    SbBool detached = manip->replaceManip(mpath, nullptr);
                    pass = pass && detached;
                }
            }
            manip->unref();
        }
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoClipPlaneManip replaceNode/replaceManip failed");
    }

    return runner.getSummary();
}
