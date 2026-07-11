#ifndef OBOL_SOCADVIEWSTATE_H
#define OBOL_SOCADVIEWSTATE_H

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
 * @file SoCADViewState.h
 * @brief Inventor traversal state for per-view CAD rendering policy.
 */

#include <obol/cad/CadViewState.h>

#include <Inventor/elements/SoSubElement.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoSFEnum.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFUInt32.h>
#include <Inventor/nodes/SoSubNode.h>

class SoState;

/**
 * @brief Stores generic CAD view policy on the Inventor traversal stack.
 */
class OBOL_DLL_API SoCADViewStateElement : public SoElement {
    typedef SoElement inherited;

    SO_ELEMENT_HEADER(SoCADViewStateElement);

public:
    static void initClass(void);

    virtual void init(SoState *state) override;
    virtual SbBool matches(const SoElement *element) const override;
    virtual SoElement *copyMatchInfo(void) const override;

    static void set(SoState *state, const obol::CadViewState& viewState);
    static obol::CadViewState get(SoState *state);

protected:
    virtual ~SoCADViewStateElement();

private:
    obol::CadViewState viewState_;
};

/**
 * @brief Node that supplies view-local CAD policy to later CAD nodes.
 *
 * Place one of these in a view-specific branch before shared CAD assembly
 * nodes. The shared assembly data remains immutable across views; the
 * traversal state carries view-local policy such as LoD enablement.
 */
class OBOL_DLL_API SoCADViewState : public SoNode {
    typedef SoNode inherited;

    SO_NODE_HEADER(SoCADViewState);

public:
    enum LodMode {
        LOD_INHERIT  = -1,
        LOD_DISABLED = 0,
        LOD_ENABLED  = 1
    };

    enum SoftwareWireMode {
        SOFTWARE_WIRE_INHERIT = -1,
        SOFTWARE_WIRE_AUTO = 0,
        SOFTWARE_WIRE_QUALITY = 1,
        SOFTWARE_WIRE_FAST = 2
    };

    static void initClass(void);
    SoCADViewState(void);

    SoSFUInt32 viewIdHigh;
    SoSFUInt32 viewIdLow;
    SoSFEnum   lodMode;
    SoSFFloat  lodScale;
    SoSFBool   selectedFullDetail;
    SoSFEnum   softwareWireMode;

    virtual SbBool affectsState(void) const override;
    virtual void doAction(SoAction *action) override;
    virtual void GLRender(SoGLRenderAction *action) override;
    virtual void callback(SoCallbackAction *action) override;
    virtual void getBoundingBox(SoGetBoundingBoxAction *action) override;
    virtual void pick(SoPickAction *action) override;
    virtual void rayPick(SoRayPickAction *action) override;
    virtual void getPrimitiveCount(SoGetPrimitiveCountAction *action) override;

protected:
    virtual ~SoCADViewState();
};

#endif // OBOL_SOCADVIEWSTATE_H
