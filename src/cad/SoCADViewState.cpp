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

#include <Obol/cad/SoCADViewState.h>

#include <Inventor/SoType.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/actions/SoPickAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoAction.h>
#include <Inventor/misc/SoState.h>

#include <algorithm>
#include <cmath>

SO_ELEMENT_SOURCE(SoCADViewStateElement);

void
SoCADViewStateElement::initClass(void)
{
    if (SoCADViewStateElement::getClassTypeId() != SoType::badType())
        return;
    SO_ELEMENT_INIT_CLASS(SoCADViewStateElement, inherited);
}

SoCADViewStateElement::~SoCADViewStateElement() = default;

void
SoCADViewStateElement::init(SoState *state)
{
    inherited::init(state);
    viewState_ = Obol::CadViewState();
}

SbBool
SoCADViewStateElement::matches(const SoElement *element) const
{
    const SoCADViewStateElement *other =
        static_cast<const SoCADViewStateElement *>(element);
    return this->viewState_ == other->viewState_;
}

SoElement *
SoCADViewStateElement::copyMatchInfo(void) const
{
    SoCADViewStateElement *element =
        static_cast<SoCADViewStateElement *>(this->getTypeId().createInstance());
    element->viewState_ = this->viewState_;
    return element;
}

void
SoCADViewStateElement::set(SoState *state,
                           const Obol::CadViewState& viewState)
{
    if (!state)
        return;
    SoCADViewStateElement *element =
        static_cast<SoCADViewStateElement *>(
            getElement(state, classStackIndex));
    if (element)
        element->viewState_ = viewState;
}

Obol::CadViewState
SoCADViewStateElement::get(SoState *state)
{
    if (!state)
        return Obol::CadViewState();
    const SoCADViewStateElement *element =
        static_cast<const SoCADViewStateElement *>(
            getConstElement(state, classStackIndex));
    return element ? element->viewState_ : Obol::CadViewState();
}

SO_NODE_SOURCE(SoCADViewState);

void
SoCADViewState::initClass(void)
{
    if (SoCADViewState::getClassTypeId() != SoType::badType())
        return;
    SoCADViewStateElement::initClass();
    SO_NODE_INIT_CLASS(SoCADViewState, SoNode, "Node");

    SO_ENABLE(SoGLRenderAction, SoCADViewStateElement);
    SO_ENABLE(SoCallbackAction, SoCADViewStateElement);
    SO_ENABLE(SoGetBoundingBoxAction, SoCADViewStateElement);
    SO_ENABLE(SoPickAction, SoCADViewStateElement);
    SO_ENABLE(SoRayPickAction, SoCADViewStateElement);
    SO_ENABLE(SoGetPrimitiveCountAction, SoCADViewStateElement);
}

SoCADViewState::SoCADViewState(void)
{
    SO_NODE_CONSTRUCTOR(SoCADViewState);

    SO_NODE_ADD_FIELD(viewIdHigh, (0));
    SO_NODE_ADD_FIELD(viewIdLow, (0));
    SO_NODE_ADD_FIELD(drawMode, (WIREFRAME));
    SO_NODE_ADD_FIELD(pickMode, (PICK_AUTO));
    SO_NODE_ADD_FIELD(edgePickTolerancePixels,
        (Obol::CadDefaultEdgePickTolerancePixels));
    SO_NODE_ADD_FIELD(wireframeOcclusion, (FALSE));
    SO_NODE_ADD_FIELD(progressiveCutCeiling, (-1));
    SO_NODE_ADD_FIELD(progressiveCutNextFraction, (0.0f));
    SO_NODE_ADD_FIELD(pointProxyPixelThreshold,
        (Obol::CadMinimumPointProxyPixels));
    SO_NODE_ADD_FIELD(cameraMotionFrameReuse, (FALSE));
    SO_NODE_ADD_FIELD(softwareWireMode, (SOFTWARE_WIRE_INHERIT));

    SO_NODE_DEFINE_ENUM_VALUE(DrawMode, SHADED);
    SO_NODE_DEFINE_ENUM_VALUE(DrawMode, WIREFRAME);
    SO_NODE_DEFINE_ENUM_VALUE(DrawMode, SHADED_WITH_EDGES);
    SO_NODE_DEFINE_ENUM_VALUE(DrawMode, HIDDEN_LINE);
    SO_NODE_SET_SF_ENUM_TYPE(drawMode, DrawMode);

    SO_NODE_DEFINE_ENUM_VALUE(PickMode, PICK_AUTO);
    SO_NODE_DEFINE_ENUM_VALUE(PickMode, PICK_EDGE);
    SO_NODE_DEFINE_ENUM_VALUE(PickMode, PICK_TRIANGLE);
    SO_NODE_DEFINE_ENUM_VALUE(PickMode, PICK_BOUNDS);
    SO_NODE_DEFINE_ENUM_VALUE(PickMode, PICK_HYBRID);
    SO_NODE_SET_SF_ENUM_TYPE(pickMode, PickMode);

    SO_NODE_DEFINE_ENUM_VALUE(SoftwareWireMode, SOFTWARE_WIRE_INHERIT);
    SO_NODE_DEFINE_ENUM_VALUE(SoftwareWireMode, SOFTWARE_WIRE_AUTO);
    SO_NODE_DEFINE_ENUM_VALUE(SoftwareWireMode, SOFTWARE_WIRE_QUALITY);
    SO_NODE_DEFINE_ENUM_VALUE(SoftwareWireMode, SOFTWARE_WIRE_FAST);
    SO_NODE_SET_SF_ENUM_TYPE(softwareWireMode, SoftwareWireMode);
}

SoCADViewState::~SoCADViewState() = default;

SbBool
SoCADViewState::affectsState(void) const
{
    return TRUE;
}

void
SoCADViewState::doAction(SoAction *action)
{
    SoState *state = action ? action->getState() : NULL;
    Obol::CadViewState viewState = SoCADViewStateElement::get(state);

    if (!this->viewIdHigh.isIgnored() || !this->viewIdLow.isIgnored()) {
        const uint64_t hi =
            static_cast<uint64_t>(this->viewIdHigh.getValue()) << 32;
        const uint64_t lo =
            static_cast<uint64_t>(this->viewIdLow.getValue());
        viewState.viewId = hi | lo;
    }
    if (!this->softwareWireMode.isIgnored()) {
        switch (this->softwareWireMode.getValue()) {
            case SOFTWARE_WIRE_QUALITY:
                viewState.softwareWireMode =
                    Obol::CadSoftwareWireMode::QUALITY;
                break;
            case SOFTWARE_WIRE_FAST:
                viewState.softwareWireMode = Obol::CadSoftwareWireMode::FAST;
                break;
            case SOFTWARE_WIRE_AUTO:
                viewState.softwareWireMode = Obol::CadSoftwareWireMode::AUTO;
                break;
            default:
                break;
        }
    }
    if (!this->drawMode.isIgnored()) {
        switch (this->drawMode.getValue()) {
            case SHADED:
                viewState.drawMode = Obol::CadDrawMode::Shaded;
                break;
            case SHADED_WITH_EDGES:
                viewState.drawMode = Obol::CadDrawMode::ShadedWithEdges;
                break;
            case HIDDEN_LINE:
                viewState.drawMode = Obol::CadDrawMode::HiddenLine;
                break;
            default:
                viewState.drawMode = Obol::CadDrawMode::Wireframe;
                break;
        }
    }
    if (!this->pickMode.isIgnored()) {
        switch (this->pickMode.getValue()) {
            case PICK_EDGE:
                viewState.pickMode = Obol::CadPickMode::Edge;
                break;
            case PICK_TRIANGLE:
                viewState.pickMode = Obol::CadPickMode::Triangle;
                break;
            case PICK_BOUNDS:
                viewState.pickMode = Obol::CadPickMode::Bounds;
                break;
            case PICK_HYBRID:
                viewState.pickMode = Obol::CadPickMode::Hybrid;
                break;
            default:
                viewState.pickMode = Obol::CadPickMode::Automatic;
                break;
        }
    }
    if (!this->edgePickTolerancePixels.isIgnored()) {
        const float tolerance = this->edgePickTolerancePixels.getValue();
        viewState.edgePickTolerancePixels =
            std::isfinite(tolerance) && tolerance >= 0.0f ?
                tolerance : Obol::CadDefaultEdgePickTolerancePixels;
    }
    if (!this->wireframeOcclusion.isIgnored())
        viewState.wireframeOcclusion =
            this->wireframeOcclusion.getValue() != FALSE;
    if (!this->progressiveCutCeiling.isIgnored()) {
        const int ceiling = this->progressiveCutCeiling.getValue();
        viewState.progressiveCutCeiling = ceiling < 0 ? -1 :
            std::min(ceiling,
                static_cast<int>(Obol::ProgressiveCutUnspecified) - 1);
    }
    if (!this->progressiveCutNextFraction.isIgnored()) {
        const float fraction = this->progressiveCutNextFraction.getValue();
        viewState.progressiveCutNextFraction = std::isfinite(fraction) ?
            std::max(0.0f, std::min(1.0f, fraction)) : 0.0f;
    }
    if (!this->pointProxyPixelThreshold.isIgnored()) {
        const float pixels = this->pointProxyPixelThreshold.getValue();
        viewState.pointProxyPixelThreshold = std::isfinite(pixels) ?
            std::max(Obol::CadMinimumPointProxyPixels,
                std::min(Obol::CadMaximumPointProxyPixels, pixels)) :
            Obol::CadMinimumPointProxyPixels;
    }
    if (!this->cameraMotionFrameReuse.isIgnored())
        viewState.cameraMotionFrameReuse =
            this->cameraMotionFrameReuse.getValue() != FALSE;

    SoCADViewStateElement::set(state, viewState);
}

void
SoCADViewState::GLRender(SoGLRenderAction *action)
{
    this->doAction(action);
}

void
SoCADViewState::callback(SoCallbackAction *action)
{
    this->doAction(action);
}

void
SoCADViewState::getBoundingBox(SoGetBoundingBoxAction *action)
{
    this->doAction(action);
}

void
SoCADViewState::pick(SoPickAction *action)
{
    this->doAction(action);
}

void
SoCADViewState::rayPick(SoRayPickAction *action)
{
    this->doAction(action);
}

void
SoCADViewState::getPrimitiveCount(SoGetPrimitiveCountAction *action)
{
    this->doAction(action);
}
