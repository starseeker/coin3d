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
 * @file test_nodes_extended.cpp
 * @brief Tests for additional node types.
 *
 * Covers:
 *   SoCallback              - setCallback, fire via SoCallbackAction
 *   SoEventCallback         - addEventCallback, dispatch
 *   SoPathSwitch            - type check
 *   SoLabel                 - label field default
 *   SoTransformSeparator    - type check, isOfType SoGroup
 *   SoUnits                 - units field default (METERS)
 *   SoPolygonOffset         - factor/units defaults
 *   SoPickStyle             - type check
 *   SoFont                  - type check, name field
 *   SoFrustumCamera         - left/right/top/bottom fields
 *   SoReversePerspectiveCamera - type check
 *   SoTextureCubeMap        - type check
 *   SoTextureUnit           - unit default (0)
 *   SoTextureCombine        - type check
 *   SoTextureMatrixTransform - type check
 *   SoVertexAttribute       - type check
 *   SoVertexAttributeBinding - value field
 */

#include "../test_utils.h"

#include <Inventor/nodes/SoCallback.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPathSwitch.h>
#include <Inventor/nodes/SoLabel.h>
#include <Inventor/nodes/SoTransformSeparator.h>
#include <Inventor/nodes/SoUnits.h>
#include <Inventor/nodes/SoPolygonOffset.h>
#include <Inventor/nodes/SoPickStyle.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoFrustumCamera.h>
#include <Inventor/nodes/SoReversePerspectiveCamera.h>
#include <Inventor/nodes/SoTextureCubeMap.h>
#include <Inventor/nodes/SoTextureUnit.h>
#include <Inventor/nodes/SoTextureCombine.h>
#include <Inventor/nodes/SoTextureMatrixTransform.h>
#include <Inventor/nodes/SoVertexAttribute.h>
#include <Inventor/nodes/SoVertexAttributeBinding.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/events/SoButtonEvent.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoType.h>
#include <Inventor/SbName.h>

using namespace SimpleTest;

// ---------------------------------------------------------------------------
// SoCallback test: capture flag
// ---------------------------------------------------------------------------
struct CallbackCapture { bool fired; };

static void callbackFn(void * userdata, SoAction * /*action*/)
{
    CallbackCapture * cap = static_cast<CallbackCapture *>(userdata);
    cap->fired = true;
}

// ---------------------------------------------------------------------------
// SoEventCallback test: capture flag
// ---------------------------------------------------------------------------
struct EventCapture { bool fired; };

static void eventCbFn(void * userdata, SoEventCallback * /*node*/)
{
    EventCapture * cap = static_cast<EventCapture *>(userdata);
    cap->fired = true;
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoCallback: setCallback fires when SoCallbackAction traverses node
    // -----------------------------------------------------------------------
    runner.startTest("SoCallback: callback fires during SoCallbackAction traversal");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();
        SoCallback * cb = new SoCallback;
        CallbackCapture cap; cap.fired = false;
        cb->setCallback(callbackFn, &cap);
        root->addChild(cb);

        SbViewportRegion vp(128, 128);
        SoCallbackAction action(vp);
        action.apply(root);

        bool pass = cap.fired;
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoCallback: callback did not fire during traversal");
    }

    // -----------------------------------------------------------------------
    // SoEventCallback: addEventCallback, fire via SoHandleEventAction
    // -----------------------------------------------------------------------
    runner.startTest("SoEventCallback: callback fires for matching event");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();
        SoEventCallback * ecb = new SoEventCallback;
        EventCapture cap; cap.fired = false;
        ecb->addEventCallback(SoKeyboardEvent::getClassTypeId(),
                              eventCbFn, &cap);
        root->addChild(ecb);

        SoKeyboardEvent evt;
        evt.setKey(SoKeyboardEvent::A);
        evt.setState(SoButtonEvent::DOWN);

        SbViewportRegion vp(128, 128);
        SoHandleEventAction action(vp);
        action.setEvent(&evt);
        action.apply(root);

        bool pass = cap.fired;
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoEventCallback: callback did not fire for matching event");
    }

    // -----------------------------------------------------------------------
    // SoPathSwitch: type check
    // -----------------------------------------------------------------------
    runner.startTest("SoPathSwitch: type check");
    {
        SoPathSwitch * n = new SoPathSwitch;
        n->ref();
        bool pass = (n->getTypeId() != SoType::badType());
        n->unref();
        runner.endTest(pass, pass ? "" : "SoPathSwitch has bad type");
    }

    // -----------------------------------------------------------------------
    // SoLabel: label field default is empty name
    // -----------------------------------------------------------------------
    runner.startTest("SoLabel: label field default is '<Undefined label>'");
    {
        SoLabel * n = new SoLabel;
        n->ref();
        bool pass = (n->label.getValue() == SbName("<Undefined label>"));
        n->unref();
        runner.endTest(pass, pass ? "" : "SoLabel default label should be '<Undefined label>'");
    }

    // -----------------------------------------------------------------------
    // SoTransformSeparator: type check, is a SoGroup subtype
    // -----------------------------------------------------------------------
    runner.startTest("SoTransformSeparator: type check and isOfType SoGroup");
    {
        SoTransformSeparator * n = new SoTransformSeparator;
        n->ref();
        bool pass = (n->getTypeId() != SoType::badType()) &&
                    n->isOfType(SoGroup::getClassTypeId());
        n->unref();
        runner.endTest(pass, pass ? "" :
            "SoTransformSeparator bad type or not SoGroup subtype");
    }

    // -----------------------------------------------------------------------
    // SoUnits: units field default is METERS
    // -----------------------------------------------------------------------
    runner.startTest("SoUnits: units field default is METERS");
    {
        SoUnits * n = new SoUnits;
        n->ref();
        bool pass = (n->units.getValue() == SoUnits::METERS);
        n->unref();
        runner.endTest(pass, pass ? "" : "SoUnits default units should be METERS");
    }

    // -----------------------------------------------------------------------
    // SoPolygonOffset: factor and units defaults
    // -----------------------------------------------------------------------
    runner.startTest("SoPolygonOffset: factor default is 1.0");
    {
        SoPolygonOffset * n = new SoPolygonOffset;
        n->ref();
        bool pass = (n->factor.getValue() == 1.0f);
        n->unref();
        runner.endTest(pass, pass ? "" : "SoPolygonOffset factor default should be 1.0");
    }

    runner.startTest("SoPolygonOffset: units default is 1.0");
    {
        SoPolygonOffset * n = new SoPolygonOffset;
        n->ref();
        bool pass = (n->units.getValue() == 1.0f);
        n->unref();
        runner.endTest(pass, pass ? "" : "SoPolygonOffset units default should be 1.0");
    }

    // -----------------------------------------------------------------------
    // SoPickStyle: type check
    // -----------------------------------------------------------------------
    runner.startTest("SoPickStyle: type check");
    {
        SoPickStyle * n = new SoPickStyle;
        n->ref();
        bool pass = (n->getTypeId() != SoType::badType());
        n->unref();
        runner.endTest(pass, pass ? "" : "SoPickStyle has bad type");
    }

    // -----------------------------------------------------------------------
    // SoFont: type check
    // -----------------------------------------------------------------------
    runner.startTest("SoFont: type check");
    {
        SoFont * n = new SoFont;
        n->ref();
        bool pass = (n->getTypeId() != SoType::badType());
        n->unref();
        runner.endTest(pass, pass ? "" : "SoFont has bad type");
    }

    // -----------------------------------------------------------------------
    // SoFrustumCamera: left/right/top/bottom fields accessible
    // -----------------------------------------------------------------------
    runner.startTest("SoFrustumCamera: type check and frustum fields");
    {
        SoFrustumCamera * n = new SoFrustumCamera;
        n->ref();
        n->left  .setValue(-1.0f);
        n->right .setValue( 1.0f);
        n->top   .setValue( 1.0f);
        n->bottom.setValue(-1.0f);
        bool pass = (n->getTypeId() != SoType::badType()) &&
                    (n->left.getValue()   == -1.0f) &&
                    (n->right.getValue()  ==  1.0f) &&
                    (n->top.getValue()    ==  1.0f) &&
                    (n->bottom.getValue() == -1.0f);
        n->unref();
        runner.endTest(pass, pass ? "" :
            "SoFrustumCamera bad type or frustum field mismatch");
    }

    // -----------------------------------------------------------------------
    // SoReversePerspectiveCamera: type check
    // -----------------------------------------------------------------------
    runner.startTest("SoReversePerspectiveCamera: type check");
    {
        SoReversePerspectiveCamera * n = new SoReversePerspectiveCamera;
        n->ref();
        bool pass = (n->getTypeId() != SoType::badType());
        n->unref();
        runner.endTest(pass, pass ? "" :
            "SoReversePerspectiveCamera has bad type");
    }

    // -----------------------------------------------------------------------
    // SoTextureCubeMap: type check
    // -----------------------------------------------------------------------
    runner.startTest("SoTextureCubeMap: type check");
    {
        SoTextureCubeMap * n = new SoTextureCubeMap;
        n->ref();
        bool pass = (n->getTypeId() != SoType::badType());
        n->unref();
        runner.endTest(pass, pass ? "" : "SoTextureCubeMap has bad type");
    }

    // -----------------------------------------------------------------------
    // SoTextureUnit: unit default is 0
    // -----------------------------------------------------------------------
    runner.startTest("SoTextureUnit: unit field default is 0");
    {
        SoTextureUnit * n = new SoTextureUnit;
        n->ref();
        bool pass = (n->unit.getValue() == 0);
        n->unref();
        runner.endTest(pass, pass ? "" : "SoTextureUnit unit default should be 0");
    }

    // -----------------------------------------------------------------------
    // SoTextureCombine: type check
    // -----------------------------------------------------------------------
    runner.startTest("SoTextureCombine: type check");
    {
        SoTextureCombine * n = new SoTextureCombine;
        n->ref();
        bool pass = (n->getTypeId() != SoType::badType());
        n->unref();
        runner.endTest(pass, pass ? "" : "SoTextureCombine has bad type");
    }

    // -----------------------------------------------------------------------
    // SoTextureMatrixTransform: type check
    // -----------------------------------------------------------------------
    runner.startTest("SoTextureMatrixTransform: type check");
    {
        SoTextureMatrixTransform * n = new SoTextureMatrixTransform;
        n->ref();
        bool pass = (n->getTypeId() != SoType::badType());
        n->unref();
        runner.endTest(pass, pass ? "" : "SoTextureMatrixTransform has bad type");
    }

    // -----------------------------------------------------------------------
    // SoVertexAttribute: type check
    // -----------------------------------------------------------------------
    runner.startTest("SoVertexAttribute: type check");
    {
        SoVertexAttribute * n = new SoVertexAttribute;
        n->ref();
        bool pass = (n->getTypeId() != SoType::badType());
        n->unref();
        runner.endTest(pass, pass ? "" : "SoVertexAttribute has bad type");
    }

    // -----------------------------------------------------------------------
    // SoVertexAttributeBinding: value field accessible
    // -----------------------------------------------------------------------
    runner.startTest("SoVertexAttributeBinding: value field accessible");
    {
        SoVertexAttributeBinding * n = new SoVertexAttributeBinding;
        n->ref();
        bool pass = (n->getTypeId() != SoType::badType());
        n->unref();
        runner.endTest(pass, pass ? "" : "SoVertexAttributeBinding has bad type");
    }

    return runner.getSummary();
}
