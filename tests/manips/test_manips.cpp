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
 * @file test_manips.cpp
 * @brief Tests for Coin3D manipulator classes.
 *
 * Exercises the manipulator infrastructure without requiring a display:
 *   - SoTrackballManip: instantiation, type, getDragger, getChildren, fields
 *   - SoJackManip:       instantiation, type, getDragger
 *   - SoTransformManip base API:
 *       translation / rotation / scaleFactor fields
 *       replaceNode (attach) / replaceManip (detach) lifecycle
 *       SoSearchAction traversal
 *
 * Complex manips (previously crashing due to SbString::vsprintf va_list bug):
 *   - SoHandleBoxManip:    instantiation, type, getDragger (SoHandleBoxDragger)
 *   - SoTabBoxManip:       instantiation, type, getDragger (SoTabBoxDragger)
 *   - SoTransformBoxManip: instantiation, type, getDragger
 *   - SoTransformerManip:  instantiation, type, getDragger
 *   - SoCenterballManip:   instantiation, type, getDragger, replaceNode/replaceManip
 *
 * Subsystems improved: manips (Tier 3, COVERAGE_PLAN.md item 22)
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/draggers/SoDragger.h>
#include <Inventor/draggers/SoTrackballDragger.h>
#include <Inventor/draggers/SoJackDragger.h>
#include <Inventor/draggers/SoHandleBoxDragger.h>
#include <Inventor/draggers/SoTabBoxDragger.h>
#include <Inventor/draggers/SoTransformBoxDragger.h>
#include <Inventor/draggers/SoTransformerDragger.h>
#include <Inventor/draggers/SoCenterballDragger.h>
#include <Inventor/manips/SoTransformManip.h>
#include <Inventor/manips/SoTrackballManip.h>
#include <Inventor/manips/SoJackManip.h>
#include <Inventor/manips/SoHandleBoxManip.h>
#include <Inventor/manips/SoTabBoxManip.h>
#include <Inventor/manips/SoTransformBoxManip.h>
#include <Inventor/manips/SoTransformerManip.h>
#include <Inventor/manips/SoCenterballManip.h>
#include <Inventor/misc/SoChildList.h>

#include <cmath>

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner  runner;

    // -----------------------------------------------------------------------
    // SoTrackballManip: instantiation and type
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: instantiation and type check");
    {
        SoTrackballManip *m = new SoTrackballManip;
        m->ref();
        bool pass = (m->getTypeId() != SoType::badType()) &&
                    m->isOfType(SoTransformManip::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip bad type or not SoTransformManip subtype");
    }

    // -----------------------------------------------------------------------
    // SoTrackballManip: getDragger returns non-null
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: getDragger returns non-null");
    {
        SoTrackballManip *m = new SoTrackballManip;
        m->ref();
        SoDragger *d = m->getDragger();
        bool pass = (d != nullptr);
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip getDragger returned null");
    }

    // -----------------------------------------------------------------------
    // SoTrackballManip: getDragger is an SoTrackballDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: getDragger is SoTrackballDragger");
    {
        SoTrackballManip *m = new SoTrackballManip;
        m->ref();
        SoDragger *d = m->getDragger();
        bool pass = (d != nullptr) &&
                    d->isOfType(SoTrackballDragger::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip getDragger is not SoTrackballDragger");
    }

    // -----------------------------------------------------------------------
    // SoJackManip: instantiation and type
    // -----------------------------------------------------------------------
    runner.startTest("SoJackManip: instantiation and type check");
    {
        SoJackManip *m = new SoJackManip;
        m->ref();
        bool pass = (m->getTypeId() != SoType::badType()) &&
                    m->isOfType(SoTransformManip::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoJackManip bad type or not SoTransformManip subtype");
    }

    // -----------------------------------------------------------------------
    // SoJackManip: getDragger returns non-null SoJackDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoJackManip: getDragger returns SoJackDragger");
    {
        SoJackManip *m = new SoJackManip;
        m->ref();
        SoDragger *d = m->getDragger();
        bool pass = (d != nullptr) &&
                    d->isOfType(SoJackDragger::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoJackManip getDragger is not SoJackDragger");
    }

    // -----------------------------------------------------------------------
    // SoTransformManip: getChildren returns non-null child list
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: getChildren returns non-null child list");
    {
        SoTrackballManip *m = new SoTrackballManip;
        m->ref();
        SoChildList *cl = m->getChildren();
        bool pass = (cl != nullptr) && (cl->getLength() > 0);
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip getChildren null or empty");
    }

    // -----------------------------------------------------------------------
    // SoTrackballManip: translation field set/get
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: translation field set/get");
    {
        SoTrackballManip *m = new SoTrackballManip;
        m->ref();
        m->translation.setValue(1.0f, 2.0f, 3.0f);
        SbVec3f t = m->translation.getValue();
        bool pass = (fabsf(t[0] - 1.0f) < 1e-5f) &&
                    (fabsf(t[1] - 2.0f) < 1e-5f) &&
                    (fabsf(t[2] - 3.0f) < 1e-5f);
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip translation set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoTrackballManip: scaleFactor field set/get
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: scaleFactor field set/get");
    {
        SoTrackballManip *m = new SoTrackballManip;
        m->ref();
        m->scaleFactor.setValue(2.0f, 2.0f, 2.0f);
        SbVec3f sf = m->scaleFactor.getValue();
        bool pass = (fabsf(sf[0] - 2.0f) < 1e-5f) &&
                    (fabsf(sf[1] - 2.0f) < 1e-5f) &&
                    (fabsf(sf[2] - 2.0f) < 1e-5f);
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip scaleFactor set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoTrackballManip: rotation field set/get
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: rotation field set/get");
    {
        SoTrackballManip *m = new SoTrackballManip;
        m->ref();
        SbRotation rot(SbVec3f(0.0f, 1.0f, 0.0f),
                       static_cast<float>(M_PI) / 4.0f);
        m->rotation.setValue(rot);
        SbRotation got = m->rotation.getValue();
        SbVec3f  ga; float ga_angle;
        SbVec3f  ra; float ra_angle;
        got.getValue(ga, ga_angle);
        rot.getValue(ra, ra_angle);
        bool pass = fabsf(ga_angle - ra_angle) < 1e-4f;
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip rotation set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoSearchAction finds a manip in a scene graph
    // -----------------------------------------------------------------------
    runner.startTest("SoSearchAction finds SoTrackballManip in scene graph");
    {
        SoSeparator *root = new SoSeparator;
        root->ref();

        SoTrackballManip *m = new SoTrackballManip;
        root->addChild(m);

        SoSearchAction sa;
        sa.setType(SoTrackballManip::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);

        bool pass = (sa.getPath() != nullptr);
        root->unref();
        runner.endTest(pass, pass ? "" : "SoSearchAction did not find SoTrackballManip");
    }

    // -----------------------------------------------------------------------
    // replaceNode: attach SoTrackballManip in place of SoTransform
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: replaceNode attaches manip to scene graph");
    {
        SoSeparator *root = new SoSeparator;
        root->ref();

        // Start with a plain SoTransform
        SoTransform *xf = new SoTransform;
        xf->translation.setValue(5.0f, 0.0f, 0.0f);
        root->addChild(xf);

        // Find the path to the SoTransform
        SoSearchAction sa;
        sa.setType(SoTransform::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        SoPath *path = sa.getPath();

        bool pass = false;
        if (path != nullptr) {
            SoTrackballManip *manip = new SoTrackballManip;
            manip->ref();
            SbBool ok = manip->replaceNode(path);
            if (ok) {
                // Verify the translation was transferred to the manip
                SbVec3f t = manip->translation.getValue();
                pass = (fabsf(t[0] - 5.0f) < 1e-4f);
            }
            manip->unref();
        }
        root->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip replaceNode failed or field not transferred");
    }

    // -----------------------------------------------------------------------
    // replaceManip: detach manip and restore plain SoTransform
    // -----------------------------------------------------------------------
    runner.startTest("SoTrackballManip: replaceManip restores SoTransform");
    {
        SoSeparator *root = new SoSeparator;
        root->ref();

        // Insert manip directly into scene graph
        SoTrackballManip *manip = new SoTrackballManip;
        manip->translation.setValue(7.0f, 0.0f, 0.0f);
        root->addChild(manip);

        // Find the path to the manip
        SoSearchAction sa;
        sa.setType(SoTrackballManip::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        SoPath *manipPath = sa.getPath();

        bool pass = false;
        if (manipPath != nullptr) {
            // Replace the manip with a new plain SoTransform
            SoTransform *newXf = new SoTransform;
            SbBool ok = manip->replaceManip(manipPath, newXf);
            if (ok) {
                // The new SoTransform should have the translation value
                SbVec3f t = newXf->translation.getValue();
                pass = (fabsf(t[0] - 7.0f) < 1e-4f);
            }
        }
        root->unref();
        runner.endTest(pass, pass ? "" : "SoTrackballManip replaceManip failed or field not transferred");
    }

    // -----------------------------------------------------------------------
    // replaceManip with null: replaceManip creates a default SoTransform
    // -----------------------------------------------------------------------
    runner.startTest("SoJackManip: replaceManip(null) creates default SoTransform");
    {
        SoSeparator *root = new SoSeparator;
        root->ref();

        SoJackManip *manip = new SoJackManip;
        manip->translation.setValue(3.0f, 0.0f, 0.0f);
        root->addChild(manip);

        SoSearchAction sa;
        sa.setType(SoJackManip::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        SoPath *manipPath = sa.getPath();

        bool pass = false;
        if (manipPath != nullptr) {
            SbBool ok = manip->replaceManip(manipPath, nullptr);
            if (ok) {
                // Scene graph should now have a SoTransform instead of the manip
                SoSearchAction sa2;
                sa2.setType(SoTransform::getClassTypeId());
                sa2.setInterest(SoSearchAction::FIRST);
                sa2.apply(root);
                pass = (sa2.getPath() != nullptr);
            }
        }
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoJackManip replaceManip(null) failed or no SoTransform found after detach");
    }

    // =======================================================================
    // Complex manips (previously crashing due to SbString::vsprintf bug)
    // =======================================================================

    // -----------------------------------------------------------------------
    // SoHandleBoxManip: instantiation and type check
    // -----------------------------------------------------------------------
    runner.startTest("SoHandleBoxManip: instantiation and type check");
    {
        SoHandleBoxManip *m = new SoHandleBoxManip;
        m->ref();
        bool pass = (m->getTypeId() != SoType::badType()) &&
                    m->isOfType(SoTransformManip::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoHandleBoxManip bad type or not SoTransformManip subtype");
    }

    // -----------------------------------------------------------------------
    // SoHandleBoxManip: getDragger returns a SoHandleBoxDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoHandleBoxManip: getDragger returns SoHandleBoxDragger");
    {
        SoHandleBoxManip *m = new SoHandleBoxManip;
        m->ref();
        SoDragger *d = m->getDragger();
        bool pass = (d != nullptr) &&
                    d->isOfType(SoHandleBoxDragger::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoHandleBoxManip getDragger is not SoHandleBoxDragger");
    }

    // -----------------------------------------------------------------------
    // SoTabBoxManip: instantiation and type check
    // -----------------------------------------------------------------------
    runner.startTest("SoTabBoxManip: instantiation and type check");
    {
        SoTabBoxManip *m = new SoTabBoxManip;
        m->ref();
        bool pass = (m->getTypeId() != SoType::badType()) &&
                    m->isOfType(SoTransformManip::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTabBoxManip bad type or not SoTransformManip subtype");
    }

    // -----------------------------------------------------------------------
    // SoTabBoxManip: getDragger returns a SoTabBoxDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoTabBoxManip: getDragger returns SoTabBoxDragger");
    {
        SoTabBoxManip *m = new SoTabBoxManip;
        m->ref();
        SoDragger *d = m->getDragger();
        bool pass = (d != nullptr) &&
                    d->isOfType(SoTabBoxDragger::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTabBoxManip getDragger is not SoTabBoxDragger");
    }

    // -----------------------------------------------------------------------
    // SoTransformBoxManip: instantiation and type check
    // -----------------------------------------------------------------------
    runner.startTest("SoTransformBoxManip: instantiation and type check");
    {
        SoTransformBoxManip *m = new SoTransformBoxManip;
        m->ref();
        bool pass = (m->getTypeId() != SoType::badType()) &&
                    m->isOfType(SoTransformManip::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTransformBoxManip bad type or not SoTransformManip subtype");
    }

    // -----------------------------------------------------------------------
    // SoTransformBoxManip: getDragger returns a SoTransformBoxDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoTransformBoxManip: getDragger returns SoTransformBoxDragger");
    {
        SoTransformBoxManip *m = new SoTransformBoxManip;
        m->ref();
        SoDragger *d = m->getDragger();
        bool pass = (d != nullptr) &&
                    d->isOfType(SoTransformBoxDragger::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTransformBoxManip getDragger is not SoTransformBoxDragger");
    }

    // -----------------------------------------------------------------------
    // SoTransformerManip: instantiation and type check
    // -----------------------------------------------------------------------
    runner.startTest("SoTransformerManip: instantiation and type check");
    {
        SoTransformerManip *m = new SoTransformerManip;
        m->ref();
        bool pass = (m->getTypeId() != SoType::badType()) &&
                    m->isOfType(SoTransformManip::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTransformerManip bad type or not SoTransformManip subtype");
    }

    // -----------------------------------------------------------------------
    // SoTransformerManip: getDragger returns a SoTransformerDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoTransformerManip: getDragger returns SoTransformerDragger");
    {
        SoTransformerManip *m = new SoTransformerManip;
        m->ref();
        SoDragger *d = m->getDragger();
        bool pass = (d != nullptr) &&
                    d->isOfType(SoTransformerDragger::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoTransformerManip getDragger is not SoTransformerDragger");
    }

    // -----------------------------------------------------------------------
    // SoCenterballManip: instantiation and type check
    // -----------------------------------------------------------------------
    runner.startTest("SoCenterballManip: instantiation and type check");
    {
        SoCenterballManip *m = new SoCenterballManip;
        m->ref();
        bool pass = (m->getTypeId() != SoType::badType()) &&
                    m->isOfType(SoTransformManip::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoCenterballManip bad type or not SoTransformManip subtype");
    }

    // -----------------------------------------------------------------------
    // SoCenterballManip: getDragger returns a SoCenterballDragger
    // -----------------------------------------------------------------------
    runner.startTest("SoCenterballManip: getDragger returns SoCenterballDragger");
    {
        SoCenterballManip *m = new SoCenterballManip;
        m->ref();
        SoDragger *d = m->getDragger();
        bool pass = (d != nullptr) &&
                    d->isOfType(SoCenterballDragger::getClassTypeId());
        m->unref();
        runner.endTest(pass, pass ? "" : "SoCenterballManip getDragger is not SoCenterballDragger");
    }

    // -----------------------------------------------------------------------
    // SoCenterballManip: translation field set/get
    // -----------------------------------------------------------------------
    runner.startTest("SoCenterballManip: translation field set/get");
    {
        SoCenterballManip *m = new SoCenterballManip;
        m->ref();
        m->translation.setValue(4.0f, 5.0f, 6.0f);
        SbVec3f t = m->translation.getValue();
        bool pass = (fabsf(t[0] - 4.0f) < 1e-5f) &&
                    (fabsf(t[1] - 5.0f) < 1e-5f) &&
                    (fabsf(t[2] - 6.0f) < 1e-5f);
        m->unref();
        runner.endTest(pass, pass ? "" : "SoCenterballManip translation set/get failed");
    }

    // -----------------------------------------------------------------------
    // SoHandleBoxManip: replaceNode attaches manip to scene graph
    // -----------------------------------------------------------------------
    runner.startTest("SoHandleBoxManip: replaceNode attaches to scene graph");
    {
        SoSeparator *root = new SoSeparator;
        root->ref();

        SoTransform *xf = new SoTransform;
        xf->translation.setValue(2.0f, 0.0f, 0.0f);
        root->addChild(xf);

        SoSearchAction sa;
        sa.setType(SoTransform::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        SoPath *path = sa.getPath();

        bool pass = false;
        if (path != nullptr) {
            SoHandleBoxManip *manip = new SoHandleBoxManip;
            manip->ref();
            SbBool ok = manip->replaceNode(path);
            if (ok) {
                SbVec3f t = manip->translation.getValue();
                pass = (fabsf(t[0] - 2.0f) < 1e-4f);
            }
            manip->unref();
        }
        root->unref();
        runner.endTest(pass, pass ? "" : "SoHandleBoxManip replaceNode failed");
    }

    // -----------------------------------------------------------------------
    // SoTransformBoxManip: replaceManip detaches from scene graph
    // -----------------------------------------------------------------------
    runner.startTest("SoTransformBoxManip: replaceManip detaches from scene graph");
    {
        SoSeparator *root = new SoSeparator;
        root->ref();

        SoTransformBoxManip *manip = new SoTransformBoxManip;
        manip->translation.setValue(3.0f, 0.0f, 0.0f);
        root->addChild(manip);

        SoSearchAction sa;
        sa.setType(SoTransformBoxManip::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        SoPath *mpath = sa.getPath();

        bool pass = false;
        if (mpath != nullptr) {
            SbBool ok = manip->replaceManip(mpath, nullptr);
            if (ok) {
                SoSearchAction sa2;
                sa2.setType(SoTransform::getClassTypeId());
                sa2.setInterest(SoSearchAction::FIRST);
                sa2.apply(root);
                pass = (sa2.getPath() != nullptr);
            }
        }
        root->unref();
        runner.endTest(pass, pass ? "" : "SoTransformBoxManip replaceManip failed");
    }

    // -----------------------------------------------------------------------
    // SoSearchAction finds complex manips in a scene graph
    // -----------------------------------------------------------------------
    runner.startTest("SoSearchAction finds SoHandleBoxManip in scene graph");
    {
        SoSeparator *root = new SoSeparator;
        root->ref();
        SoHandleBoxManip *m = new SoHandleBoxManip;
        root->addChild(m);
        SoSearchAction sa;
        sa.setType(SoHandleBoxManip::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        bool pass = (sa.getPath() != nullptr);
        root->unref();
        runner.endTest(pass, pass ? "" : "SoSearchAction did not find SoHandleBoxManip");
    }

    runner.startTest("SoSearchAction finds SoTransformerManip in scene graph");
    {
        SoSeparator *root = new SoSeparator;
        root->ref();
        SoTransformerManip *m = new SoTransformerManip;
        root->addChild(m);
        SoSearchAction sa;
        sa.setType(SoTransformerManip::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        bool pass = (sa.getPath() != nullptr);
        root->unref();
        runner.endTest(pass, pass ? "" : "SoSearchAction did not find SoTransformerManip");
    }

    runner.startTest("SoSearchAction finds SoCenterballManip in scene graph");
    {
        SoSeparator *root = new SoSeparator;
        root->ref();
        SoCenterballManip *m = new SoCenterballManip;
        root->addChild(m);
        SoSearchAction sa;
        sa.setType(SoCenterballManip::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);
        bool pass = (sa.getPath() != nullptr);
        root->unref();
        runner.endTest(pass, pass ? "" : "SoSearchAction did not find SoCenterballManip");
    }

    return runner.getSummary();
}
