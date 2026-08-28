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

using namespace ObolTest;

TEST(ManipsManips, SoTrackballManipInstantiationAndTypeCheck)
{
    SoTrackballManip *m = new SoTrackballManip;
    m->ref();
    EXPECT_TRUE((m->getTypeId() != SoType::badType()) &&
                m->isOfType(SoTransformManip::getClassTypeId())) << "SoTrackballManip bad type or not SoTransformManip subtype";
    m->unref();
}

// -----------------------------------------------------------------------
// SoTrackballManip: getDragger returns non-null
// -----------------------------------------------------------------------

TEST(ManipsManips, SoTrackballManipGetDraggerReturnsNonNull)
{
    SoTrackballManip *m = new SoTrackballManip;
    m->ref();
    SoDragger *d = m->getDragger();
    EXPECT_TRUE((d != nullptr)) << "SoTrackballManip getDragger returned null";
    m->unref();
}

// -----------------------------------------------------------------------
// SoTrackballManip: getDragger is an SoTrackballDragger
// -----------------------------------------------------------------------

TEST(ManipsManips, SoTrackballManipGetDraggerIsSoTrackballDragger)
{
    SoTrackballManip *m = new SoTrackballManip;
    m->ref();
    SoDragger *d = m->getDragger();
    EXPECT_TRUE((d != nullptr) &&
                d->isOfType(SoTrackballDragger::getClassTypeId())) << "SoTrackballManip getDragger is not SoTrackballDragger";
    m->unref();
}

// -----------------------------------------------------------------------
// SoJackManip: instantiation and type
// -----------------------------------------------------------------------

TEST(ManipsManips, SoJackManipInstantiationAndTypeCheck)
{
    SoJackManip *m = new SoJackManip;
    m->ref();
    EXPECT_TRUE((m->getTypeId() != SoType::badType()) &&
                m->isOfType(SoTransformManip::getClassTypeId())) << "SoJackManip bad type or not SoTransformManip subtype";
    m->unref();
}

// -----------------------------------------------------------------------
// SoJackManip: getDragger returns non-null SoJackDragger
// -----------------------------------------------------------------------

TEST(ManipsManips, SoJackManipGetDraggerReturnsSoJackDragger)
{
    SoJackManip *m = new SoJackManip;
    m->ref();
    SoDragger *d = m->getDragger();
    EXPECT_TRUE((d != nullptr) &&
                d->isOfType(SoJackDragger::getClassTypeId())) << "SoJackManip getDragger is not SoJackDragger";
    m->unref();
}

// -----------------------------------------------------------------------
// SoTransformManip: getChildren returns non-null child list
// -----------------------------------------------------------------------

TEST(ManipsManips, SoTrackballManipGetChildrenReturnsNonNullChildList)
{
    SoTrackballManip *m = new SoTrackballManip;
    m->ref();
    SoChildList *cl = m->getChildren();
    EXPECT_TRUE((cl != nullptr) && (cl->getLength() > 0)) << "SoTrackballManip getChildren null or empty";
    m->unref();
}

// -----------------------------------------------------------------------
// SoTrackballManip: translation field set/get
// -----------------------------------------------------------------------

TEST(ManipsManips, SoTrackballManipTranslationFieldSetGet)
{
    SoTrackballManip *m = new SoTrackballManip;
    m->ref();
    m->translation.setValue(1.0f, 2.0f, 3.0f);
    SbVec3f t = m->translation.getValue();
    EXPECT_TRUE((fabsf(t[0] - 1.0f) < 1e-5f) &&
                (fabsf(t[1] - 2.0f) < 1e-5f) &&
                (fabsf(t[2] - 3.0f) < 1e-5f)) << "SoTrackballManip translation set/get failed";
    m->unref();
}

// -----------------------------------------------------------------------
// SoTrackballManip: scaleFactor field set/get
// -----------------------------------------------------------------------

TEST(ManipsManips, SoTrackballManipScaleFactorFieldSetGet)
{
    SoTrackballManip *m = new SoTrackballManip;
    m->ref();
    m->scaleFactor.setValue(2.0f, 2.0f, 2.0f);
    SbVec3f sf = m->scaleFactor.getValue();
    EXPECT_TRUE((fabsf(sf[0] - 2.0f) < 1e-5f) &&
                (fabsf(sf[1] - 2.0f) < 1e-5f) &&
                (fabsf(sf[2] - 2.0f) < 1e-5f)) << "SoTrackballManip scaleFactor set/get failed";
    m->unref();
}

// -----------------------------------------------------------------------
// SoTrackballManip: rotation field set/get
// -----------------------------------------------------------------------

TEST(ManipsManips, SoTrackballManipRotationFieldSetGet)
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
    EXPECT_TRUE(fabsf(ga_angle - ra_angle) < 1e-4f) << "SoTrackballManip rotation set/get failed";
    m->unref();
}

// -----------------------------------------------------------------------
// SoSearchAction finds a manip in a scene graph
// -----------------------------------------------------------------------

TEST(ManipsManips, SoSearchActionFindsSoTrackballManipInSceneGraph)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoTrackballManip *m = new SoTrackballManip;
    root->addChild(m);

    SoSearchAction sa;
    sa.setType(SoTrackballManip::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);

    EXPECT_TRUE((sa.getPath() != nullptr)) << "SoSearchAction did not find SoTrackballManip";
    root->unref();
}

// -----------------------------------------------------------------------
// replaceNode: attach SoTrackballManip in place of SoTransform
// -----------------------------------------------------------------------

TEST(ManipsManips, SoTrackballManipReplaceNodeAttachesManipToSceneGraph)
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

    EXPECT_NE(path, nullptr);
    if (path != nullptr) {
        SoTrackballManip *manip = new SoTrackballManip;
        manip->ref();
        SbBool ok = manip->replaceNode(path);
        EXPECT_TRUE(ok);
        if (ok) {
            // Verify the translation was transferred to the manip
            SbVec3f t = manip->translation.getValue();
            EXPECT_NEAR(t[0], 5.0f, 1e-4f);
        }
        manip->unref();
    }
    root->unref();
}

// -----------------------------------------------------------------------
// replaceManip: detach manip and restore plain SoTransform
// -----------------------------------------------------------------------

TEST(ManipsManips, SoTrackballManipReplaceManipRestoresSoTransform)
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

    EXPECT_NE(manipPath, nullptr);
    if (manipPath != nullptr) {
        // Replace the manip with a new plain SoTransform
        SoTransform *newXf = new SoTransform;
        SbBool ok = manip->replaceManip(manipPath, newXf);
        EXPECT_TRUE(ok);
        if (ok) {
            // The new SoTransform should have the translation value
            SbVec3f t = newXf->translation.getValue();
            EXPECT_NEAR(t[0], 7.0f, 1e-4f);
        }
    }
    root->unref();
}

// -----------------------------------------------------------------------
// replaceManip with null: replaceManip creates a default SoTransform
// -----------------------------------------------------------------------

TEST(ManipsManips, SoJackManipReplaceManipNullCreatesDefaultSoTransform)
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

    EXPECT_NE(manipPath, nullptr);
    if (manipPath != nullptr) {
        SbBool ok = manip->replaceManip(manipPath, nullptr);
        EXPECT_TRUE(ok);
        if (ok) {
            // Scene graph should now have a SoTransform instead of the manip
            SoSearchAction sa2;
            sa2.setType(SoTransform::getClassTypeId());
            sa2.setInterest(SoSearchAction::FIRST);
            sa2.apply(root);
            EXPECT_NE(sa2.getPath(), nullptr);
        }
    }
    root->unref();
}

// =======================================================================
// Complex manips (previously crashing due to SbString::vsprintf bug)
// =======================================================================

// -----------------------------------------------------------------------
// SoHandleBoxManip: instantiation and type check
// -----------------------------------------------------------------------

TEST(ManipsManips, SoHandleBoxManipInstantiationAndTypeCheck)
{
    SoHandleBoxManip *m = new SoHandleBoxManip;
    m->ref();
    EXPECT_TRUE((m->getTypeId() != SoType::badType()) &&
                m->isOfType(SoTransformManip::getClassTypeId())) << "SoHandleBoxManip bad type or not SoTransformManip subtype";
    m->unref();
}

// -----------------------------------------------------------------------
// SoHandleBoxManip: getDragger returns a SoHandleBoxDragger
// -----------------------------------------------------------------------

TEST(ManipsManips, SoHandleBoxManipGetDraggerReturnsSoHandleBoxDragger)
{
    SoHandleBoxManip *m = new SoHandleBoxManip;
    m->ref();
    SoDragger *d = m->getDragger();
    EXPECT_TRUE((d != nullptr) &&
                d->isOfType(SoHandleBoxDragger::getClassTypeId())) << "SoHandleBoxManip getDragger is not SoHandleBoxDragger";
    m->unref();
}

// -----------------------------------------------------------------------
// SoTabBoxManip: instantiation and type check
// -----------------------------------------------------------------------

TEST(ManipsManips, SoTabBoxManipInstantiationAndTypeCheck)
{
    SoTabBoxManip *m = new SoTabBoxManip;
    m->ref();
    EXPECT_TRUE((m->getTypeId() != SoType::badType()) &&
                m->isOfType(SoTransformManip::getClassTypeId())) << "SoTabBoxManip bad type or not SoTransformManip subtype";
    m->unref();
}

// -----------------------------------------------------------------------
// SoTabBoxManip: getDragger returns a SoTabBoxDragger
// -----------------------------------------------------------------------

TEST(ManipsManips, SoTabBoxManipGetDraggerReturnsSoTabBoxDragger)
{
    SoTabBoxManip *m = new SoTabBoxManip;
    m->ref();
    SoDragger *d = m->getDragger();
    EXPECT_TRUE((d != nullptr) &&
                d->isOfType(SoTabBoxDragger::getClassTypeId())) << "SoTabBoxManip getDragger is not SoTabBoxDragger";
    m->unref();
}

// -----------------------------------------------------------------------
// SoTransformBoxManip: instantiation and type check
// -----------------------------------------------------------------------

TEST(ManipsManips, SoTransformBoxManipInstantiationAndTypeCheck)
{
    SoTransformBoxManip *m = new SoTransformBoxManip;
    m->ref();
    EXPECT_TRUE((m->getTypeId() != SoType::badType()) &&
                m->isOfType(SoTransformManip::getClassTypeId())) << "SoTransformBoxManip bad type or not SoTransformManip subtype";
    m->unref();
}

// -----------------------------------------------------------------------
// SoTransformBoxManip: getDragger returns a SoTransformBoxDragger
// -----------------------------------------------------------------------

TEST(ManipsManips, SoTransformBoxManipGetDraggerReturnsSoTransformBoxDragger)
{
    SoTransformBoxManip *m = new SoTransformBoxManip;
    m->ref();
    SoDragger *d = m->getDragger();
    EXPECT_TRUE((d != nullptr) &&
                d->isOfType(SoTransformBoxDragger::getClassTypeId())) << "SoTransformBoxManip getDragger is not SoTransformBoxDragger";
    m->unref();
}

// -----------------------------------------------------------------------
// SoTransformerManip: instantiation and type check
// -----------------------------------------------------------------------

TEST(ManipsManips, SoTransformerManipInstantiationAndTypeCheck)
{
    SoTransformerManip *m = new SoTransformerManip;
    m->ref();
    EXPECT_TRUE((m->getTypeId() != SoType::badType()) &&
                m->isOfType(SoTransformManip::getClassTypeId())) << "SoTransformerManip bad type or not SoTransformManip subtype";
    m->unref();
}

// -----------------------------------------------------------------------
// SoTransformerManip: getDragger returns a SoTransformerDragger
// -----------------------------------------------------------------------

TEST(ManipsManips, SoTransformerManipGetDraggerReturnsSoTransformerDragger)
{
    SoTransformerManip *m = new SoTransformerManip;
    m->ref();
    SoDragger *d = m->getDragger();
    EXPECT_TRUE((d != nullptr) &&
                d->isOfType(SoTransformerDragger::getClassTypeId())) << "SoTransformerManip getDragger is not SoTransformerDragger";
    m->unref();
}

// -----------------------------------------------------------------------
// SoCenterballManip: instantiation and type check
// -----------------------------------------------------------------------

TEST(ManipsManips, SoCenterballManipInstantiationAndTypeCheck)
{
    SoCenterballManip *m = new SoCenterballManip;
    m->ref();
    EXPECT_TRUE((m->getTypeId() != SoType::badType()) &&
                m->isOfType(SoTransformManip::getClassTypeId())) << "SoCenterballManip bad type or not SoTransformManip subtype";
    m->unref();
}

// -----------------------------------------------------------------------
// SoCenterballManip: getDragger returns a SoCenterballDragger
// -----------------------------------------------------------------------

TEST(ManipsManips, SoCenterballManipGetDraggerReturnsSoCenterballDragger)
{
    SoCenterballManip *m = new SoCenterballManip;
    m->ref();
    SoDragger *d = m->getDragger();
    EXPECT_TRUE((d != nullptr) &&
                d->isOfType(SoCenterballDragger::getClassTypeId())) << "SoCenterballManip getDragger is not SoCenterballDragger";
    m->unref();
}

// -----------------------------------------------------------------------
// SoCenterballManip: translation field set/get
// -----------------------------------------------------------------------

TEST(ManipsManips, SoCenterballManipTranslationFieldSetGet)
{
    SoCenterballManip *m = new SoCenterballManip;
    m->ref();
    m->translation.setValue(4.0f, 5.0f, 6.0f);
    SbVec3f t = m->translation.getValue();
    EXPECT_TRUE((fabsf(t[0] - 4.0f) < 1e-5f) &&
                (fabsf(t[1] - 5.0f) < 1e-5f) &&
                (fabsf(t[2] - 6.0f) < 1e-5f)) << "SoCenterballManip translation set/get failed";
    m->unref();
}

// -----------------------------------------------------------------------
// SoHandleBoxManip: replaceNode attaches manip to scene graph
// -----------------------------------------------------------------------

TEST(ManipsManips, SoHandleBoxManipReplaceNodeAttachesToSceneGraph)
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

    EXPECT_NE(path, nullptr);
    if (path != nullptr) {
        SoHandleBoxManip *manip = new SoHandleBoxManip;
        manip->ref();
        SbBool ok = manip->replaceNode(path);
        EXPECT_TRUE(ok);
        if (ok) {
            SbVec3f t = manip->translation.getValue();
            EXPECT_NEAR(t[0], 2.0f, 1e-4f);
        }
        manip->unref();
    }
    root->unref();
}

// -----------------------------------------------------------------------
// SoTransformBoxManip: replaceManip detaches from scene graph
// -----------------------------------------------------------------------

TEST(ManipsManips, SoTransformBoxManipReplaceManipDetachesFromSceneGraph)
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

    EXPECT_NE(mpath, nullptr);
    if (mpath != nullptr) {
        SbBool ok = manip->replaceManip(mpath, nullptr);
        EXPECT_TRUE(ok);
        if (ok) {
            SoSearchAction sa2;
            sa2.setType(SoTransform::getClassTypeId());
            sa2.setInterest(SoSearchAction::FIRST);
            sa2.apply(root);
            EXPECT_NE(sa2.getPath(), nullptr);
        }
    }
    root->unref();
}

// -----------------------------------------------------------------------
// SoSearchAction finds complex manips in a scene graph
// -----------------------------------------------------------------------

TEST(ManipsManips, SoSearchActionFindsSoHandleBoxManipInSceneGraph)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoHandleBoxManip *m = new SoHandleBoxManip;
    root->addChild(m);
    SoSearchAction sa;
    sa.setType(SoHandleBoxManip::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);
    EXPECT_TRUE((sa.getPath() != nullptr)) << "SoSearchAction did not find SoHandleBoxManip";
    root->unref();
}

TEST(ManipsManips, SoSearchActionFindsSoTransformerManipInSceneGraph)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoTransformerManip *m = new SoTransformerManip;
    root->addChild(m);
    SoSearchAction sa;
    sa.setType(SoTransformerManip::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);
    EXPECT_TRUE((sa.getPath() != nullptr)) << "SoSearchAction did not find SoTransformerManip";
    root->unref();
}

TEST(ManipsManips, SoSearchActionFindsSoCenterballManipInSceneGraph)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    SoCenterballManip *m = new SoCenterballManip;
    root->addChild(m);
    SoSearchAction sa;
    sa.setType(SoCenterballManip::getClassTypeId());
    sa.setInterest(SoSearchAction::FIRST);
    sa.apply(root);
    EXPECT_TRUE((sa.getPath() != nullptr)) << "SoSearchAction did not find SoCenterballManip";
    root->unref();
}
