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

#include <obol/cad/SoCADViewState.h>

#include <Inventor/SoType.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/actions/SoPickAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoAction.h>
#include <Inventor/misc/SoState.h>

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
    viewState_ = obol::CadViewState();
}

SbBool
SoCADViewStateElement::matches(const SoElement *element) const
{
    const SoCADViewStateElement *other =
        static_cast<const SoCADViewStateElement *>(element);
    return this->viewState_.viewId == other->viewState_.viewId &&
        this->viewState_.lodMode == other->viewState_.lodMode &&
        this->viewState_.lodScale == other->viewState_.lodScale &&
        this->viewState_.selectedFullDetail ==
            other->viewState_.selectedFullDetail &&
        this->viewState_.softwareWireMode ==
            other->viewState_.softwareWireMode;
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
                           const obol::CadViewState& viewState)
{
    if (!state)
        return;
    SoCADViewStateElement *element =
        static_cast<SoCADViewStateElement *>(
            getElement(state, classStackIndex));
    if (element)
        element->viewState_ = viewState;
}

obol::CadViewState
SoCADViewStateElement::get(SoState *state)
{
    if (!state)
        return obol::CadViewState();
    const SoCADViewStateElement *element =
        static_cast<const SoCADViewStateElement *>(
            getConstElement(state, classStackIndex));
    return element ? element->viewState_ : obol::CadViewState();
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
    SO_NODE_ADD_FIELD(lodMode, (LOD_INHERIT));
    SO_NODE_ADD_FIELD(lodScale, (1.0f));
    SO_NODE_ADD_FIELD(selectedFullDetail, (TRUE));
    SO_NODE_ADD_FIELD(softwareWireMode, (SOFTWARE_WIRE_INHERIT));

    SO_NODE_DEFINE_ENUM_VALUE(LodMode, LOD_INHERIT);
    SO_NODE_DEFINE_ENUM_VALUE(LodMode, LOD_DISABLED);
    SO_NODE_DEFINE_ENUM_VALUE(LodMode, LOD_ENABLED);
    SO_NODE_SET_SF_ENUM_TYPE(lodMode, LodMode);

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
    obol::CadViewState viewState = SoCADViewStateElement::get(state);

    if (!this->viewIdHigh.isIgnored() || !this->viewIdLow.isIgnored()) {
        const uint64_t hi =
            static_cast<uint64_t>(this->viewIdHigh.getValue()) << 32;
        const uint64_t lo =
            static_cast<uint64_t>(this->viewIdLow.getValue());
        viewState.viewId = hi | lo;
    }
    if (!this->lodMode.isIgnored()) {
        switch (this->lodMode.getValue()) {
            case LOD_DISABLED:
                viewState.lodMode = obol::CadLodMode::DISABLED;
                break;
            case LOD_ENABLED:
                viewState.lodMode = obol::CadLodMode::ENABLED;
                break;
            default:
                break;
        }
    }
    if (!this->lodScale.isIgnored())
        viewState.lodScale = this->lodScale.getValue();
    if (!this->selectedFullDetail.isIgnored())
        viewState.selectedFullDetail =
            this->selectedFullDetail.getValue() ? true : false;
    if (!this->softwareWireMode.isIgnored()) {
        switch (this->softwareWireMode.getValue()) {
            case SOFTWARE_WIRE_QUALITY:
                viewState.softwareWireMode =
                    obol::CadSoftwareWireMode::QUALITY;
                break;
            case SOFTWARE_WIRE_FAST:
                viewState.softwareWireMode = obol::CadSoftwareWireMode::FAST;
                break;
            case SOFTWARE_WIRE_AUTO:
                viewState.softwareWireMode = obol::CadSoftwareWireMode::AUTO;
                break;
            default:
                break;
        }
    }

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
