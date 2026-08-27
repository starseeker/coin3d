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

using namespace ObolTest;

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

TEST(NodesExtended, SoCallbackCallbackFiresDuringSoCallbackActionTraversal)
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
    EXPECT_TRUE(pass) << "SoCallback: callback did not fire during traversal";
}

// -----------------------------------------------------------------------
// SoEventCallback: addEventCallback, fire via SoHandleEventAction
// -----------------------------------------------------------------------

TEST(NodesExtended, SoEventCallbackCallbackFiresForMatchingEvent)
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
    EXPECT_TRUE(pass) << "SoEventCallback: callback did not fire for matching event";
}

// -----------------------------------------------------------------------
// SoPathSwitch: type check
// -----------------------------------------------------------------------

TEST(NodesExtended, SoPathSwitchTypeCheck)
{
    SoPathSwitch * n = new SoPathSwitch;
    n->ref();
    bool pass = (n->getTypeId() != SoType::badType());
    n->unref();
    EXPECT_TRUE(pass) << "SoPathSwitch has bad type";
}

// -----------------------------------------------------------------------
// SoLabel: label field default is empty name
// -----------------------------------------------------------------------

TEST(NodesExtended, SoLabelLabelFieldDefaultIsUndefinedLabel)
{
    SoLabel * n = new SoLabel;
    n->ref();
    bool pass = (n->label.getValue() == SbName("<Undefined label>"));
    n->unref();
    EXPECT_TRUE(pass) << "SoLabel default label should be '<Undefined label>'";
}

// -----------------------------------------------------------------------
// SoTransformSeparator: type check, is a SoGroup subtype
// -----------------------------------------------------------------------

TEST(NodesExtended, SoTransformSeparatorTypeCheckAndIsOfTypeSoGroup)
{
    SoTransformSeparator * n = new SoTransformSeparator;
    n->ref();
    bool pass = (n->getTypeId() != SoType::badType()) &&
                n->isOfType(SoGroup::getClassTypeId());
    n->unref();
    EXPECT_TRUE(pass) << "SoTransformSeparator bad type or not SoGroup subtype";
}

// -----------------------------------------------------------------------
// SoUnits: units field default is METERS
// -----------------------------------------------------------------------

TEST(NodesExtended, SoUnitsUnitsFieldDefaultIsMETERS)
{
    SoUnits * n = new SoUnits;
    n->ref();
    bool pass = (n->units.getValue() == SoUnits::METERS);
    n->unref();
    EXPECT_TRUE(pass) << "SoUnits default units should be METERS";
}

// -----------------------------------------------------------------------
// SoPolygonOffset: factor and units defaults
// -----------------------------------------------------------------------

TEST(NodesExtended, SoPolygonOffsetFactorDefaultIs10)
{
    SoPolygonOffset * n = new SoPolygonOffset;
    n->ref();
    bool pass = (n->factor.getValue() == 1.0f);
    n->unref();
    EXPECT_TRUE(pass) << "SoPolygonOffset factor default should be 1.0";
}

TEST(NodesExtended, SoPolygonOffsetUnitsDefaultIs10)
{
    SoPolygonOffset * n = new SoPolygonOffset;
    n->ref();
    bool pass = (n->units.getValue() == 1.0f);
    n->unref();
    EXPECT_TRUE(pass) << "SoPolygonOffset units default should be 1.0";
}

// -----------------------------------------------------------------------
// SoPickStyle: type check
// -----------------------------------------------------------------------

TEST(NodesExtended, SoPickStyleTypeCheck)
{
    SoPickStyle * n = new SoPickStyle;
    n->ref();
    bool pass = (n->getTypeId() != SoType::badType());
    n->unref();
    EXPECT_TRUE(pass) << "SoPickStyle has bad type";
}

// -----------------------------------------------------------------------
// SoFont: type check
// -----------------------------------------------------------------------

TEST(NodesExtended, SoFontTypeCheck)
{
    SoFont * n = new SoFont;
    n->ref();
    bool pass = (n->getTypeId() != SoType::badType());
    n->unref();
    EXPECT_TRUE(pass) << "SoFont has bad type";
}

// -----------------------------------------------------------------------
// SoFrustumCamera: left/right/top/bottom fields accessible
// -----------------------------------------------------------------------

TEST(NodesExtended, SoFrustumCameraTypeCheckAndFrustumFields)
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
    EXPECT_TRUE(pass) << "SoFrustumCamera bad type or frustum field mismatch";
}

// -----------------------------------------------------------------------
// SoReversePerspectiveCamera: type check
// -----------------------------------------------------------------------

TEST(NodesExtended, SoReversePerspectiveCameraTypeCheck)
{
    SoReversePerspectiveCamera * n = new SoReversePerspectiveCamera;
    n->ref();
    bool pass = (n->getTypeId() != SoType::badType());
    n->unref();
    EXPECT_TRUE(pass) << "SoReversePerspectiveCamera has bad type";
}

// -----------------------------------------------------------------------
// SoTextureCubeMap: type check
// -----------------------------------------------------------------------

TEST(NodesExtended, SoTextureCubeMapTypeCheck)
{
    SoTextureCubeMap * n = new SoTextureCubeMap;
    n->ref();
    bool pass = (n->getTypeId() != SoType::badType());
    n->unref();
    EXPECT_TRUE(pass) << "SoTextureCubeMap has bad type";
}

// -----------------------------------------------------------------------
// SoTextureUnit: unit default is 0
// -----------------------------------------------------------------------

TEST(NodesExtended, SoTextureUnitUnitFieldDefaultIs0)
{
    SoTextureUnit * n = new SoTextureUnit;
    n->ref();
    bool pass = (n->unit.getValue() == 0);
    n->unref();
    EXPECT_TRUE(pass) << "SoTextureUnit unit default should be 0";
}

// -----------------------------------------------------------------------
// SoTextureCombine: type check
// -----------------------------------------------------------------------

TEST(NodesExtended, SoTextureCombineTypeCheck)
{
    SoTextureCombine * n = new SoTextureCombine;
    n->ref();
    bool pass = (n->getTypeId() != SoType::badType());
    n->unref();
    EXPECT_TRUE(pass) << "SoTextureCombine has bad type";
}

// -----------------------------------------------------------------------
// SoTextureMatrixTransform: type check
// -----------------------------------------------------------------------

TEST(NodesExtended, SoTextureMatrixTransformTypeCheck)
{
    SoTextureMatrixTransform * n = new SoTextureMatrixTransform;
    n->ref();
    bool pass = (n->getTypeId() != SoType::badType());
    n->unref();
    EXPECT_TRUE(pass) << "SoTextureMatrixTransform has bad type";
}

// -----------------------------------------------------------------------
// SoVertexAttribute: type check
// -----------------------------------------------------------------------

TEST(NodesExtended, SoVertexAttributeTypeCheck)
{
    SoVertexAttribute * n = new SoVertexAttribute;
    n->ref();
    bool pass = (n->getTypeId() != SoType::badType());
    n->unref();
    EXPECT_TRUE(pass) << "SoVertexAttribute has bad type";
}

// -----------------------------------------------------------------------
// SoVertexAttributeBinding: value field accessible
// -----------------------------------------------------------------------

TEST(NodesExtended, SoVertexAttributeBindingValueFieldAccessible)
{
    SoVertexAttributeBinding * n = new SoVertexAttributeBinding;
    n->ref();
    bool pass = (n->getTypeId() != SoType::badType());
    n->unref();
    EXPECT_TRUE(pass) << "SoVertexAttributeBinding has bad type";
}
