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

/*!
  \class SoHUDKit SoHUDKit.h Inventor/annex/HUD/nodekits/SoHUDKit.h
  \brief Root nodekit for a Head-Up Display overlay.

  SoHUDKit renders its children on top of the 3-D scene by switching to a
  pixel-space orthographic camera and disabling the depth buffer.

  The coordinate origin is at the lower-left corner of the viewport; one
  unit equals one pixel.  The \c viewportSize field is updated
  automatically each render cycle.

  Use addWidget() and removeWidget() to manage child HUD nodes such as
  SoHUDLabel or SoHUDButton.

  \ingroup coin_hud
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_NODEKITS

#include <Inventor/annex/HUD/nodekits/SoHUDKit.h>
#include "coindefs.h"

#include <Inventor/system/gl.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/misc/SoState.h>
#include <Inventor/actions/SoAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/nodes/SoCallback.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/elements/SoViewportRegionElement.h>

#include "nodekits/SoSubKitP.h"

namespace {

void enableDepthTest(void * COIN_UNUSED_ARG(userdata),
                     SoAction * COIN_UNUSED_ARG(action))
{
  glEnable(GL_DEPTH_TEST);
}

void disableDepthTest(void * COIN_UNUSED_ARG(userdata),
                      SoAction * COIN_UNUSED_ARG(action))
{
  glDisable(GL_DEPTH_TEST);
}

} // anonymous namespace

// ---------------------------------------------------------------------------

/*!
  Viewport-tracking callback (static member so it may call protected
  SoBaseKit::getAnyPart).
*/
void
SoHUDKit::grabViewportInfo(void * userdata, SoAction * action)
{
  SoState * state = action->getState();
  const int eltindex = SoViewportRegionElement::getClassStackIndex();
  if (state->isElementEnabled(eltindex)) {
    const SbViewportRegion & vp = SoViewportRegionElement::get(state);
    SbVec2s pixels = vp.getViewportSizePixels();
    SoHUDKit * kit = static_cast<SoHUDKit *>(userdata);
    kit->viewportSize.setValue(SbVec2f(float(pixels[0]), float(pixels[1])));

    // Keep the orthographic camera calibrated so that 1 unit == 1 pixel.
    SoOrthographicCamera * cam =
      static_cast<SoOrthographicCamera *>(
        kit->getAnyPart("overlayCamera", FALSE));
    if (cam) {
      float w = float(pixels[0]);
      float h = float(pixels[1]);
      cam->position.setValue(w * 0.5f, h * 0.5f, 1.0f);
      cam->height.setValue(h);
      // Aspect ratio is set by LEAVE_ALONE so the viewport handles it.
    }
  }
}

SO_KIT_SOURCE(SoHUDKit);

// Doc in superclass.
void
SoHUDKit::initClass(void)
{
  SO_KIT_INTERNAL_INIT_CLASS(SoHUDKit, SO_FROM_COIN_3_0);
}

/*!
  Constructor.
*/
SoHUDKit::SoHUDKit(void)
{
  SO_KIT_INTERNAL_CONSTRUCTOR(SoHUDKit);

  SO_KIT_ADD_CATALOG_ENTRY(topSeparator, SoSeparator, TRUE,
                            this, "", FALSE);
  SO_KIT_ADD_CATALOG_ENTRY(viewportInfo, SoCallback, TRUE,
                            topSeparator, overlayCamera, FALSE);
  SO_KIT_ADD_CATALOG_ENTRY(overlayCamera, SoOrthographicCamera, TRUE,
                            topSeparator, depthTestOff, FALSE);
  SO_KIT_ADD_CATALOG_ENTRY(depthTestOff, SoCallback, TRUE,
                            topSeparator, widgetsSep, FALSE);
  SO_KIT_ADD_CATALOG_ENTRY(widgetsSep, SoSeparator, TRUE,
                            topSeparator, depthTestOn, FALSE);
  SO_KIT_ADD_CATALOG_ENTRY(depthTestOn, SoCallback, TRUE,
                            topSeparator, "", FALSE);

  SO_KIT_INIT_INSTANCE();

  SO_KIT_ADD_FIELD(viewportSize, (SbVec2f(100.0f, 100.0f)));

  // Configure the orthographic camera for pixel-space rendering.
  SoOrthographicCamera * camera =
    static_cast<SoOrthographicCamera *>(
      this->getAnyPart("overlayCamera", TRUE));
  camera->viewportMapping = SoCamera::LEAVE_ALONE;
  camera->nearDistance.setValue(0.1f);
  camera->farDistance.setValue(10.0f);
  camera->position.setValue(50.0f, 50.0f, 1.0f);
  camera->height.setValue(100.0f);

  SoCallback * vpCB =
    static_cast<SoCallback *>(this->getAnyPart("viewportInfo", TRUE));
  vpCB->setCallback(grabViewportInfo, this);

  SoCallback * beforeCB =
    static_cast<SoCallback *>(this->getAnyPart("depthTestOff", TRUE));
  beforeCB->setCallback(disableDepthTest);

  SoCallback * afterCB =
    static_cast<SoCallback *>(this->getAnyPart("depthTestOn", TRUE));
  afterCB->setCallback(enableDepthTest);
}

/*!
  Destructor.
*/
SoHUDKit::~SoHUDKit(void)
{
}

/*!
  Add a HUD widget (e.g. SoHUDLabel, SoHUDButton, or any SoNode) to the
  display list.  Widgets are rendered in the order they are added.
*/
void
SoHUDKit::addWidget(SoNode * widget)
{
  SoNode * sep = this->getAnyPart("widgetsSep", TRUE);
  assert(sep->isOfType(SoGroup::getClassTypeId()));
  static_cast<SoGroup *>(sep)->addChild(widget);
}

/*!
  Remove a previously added widget.  Does nothing if the widget is not
  present.
*/
void
SoHUDKit::removeWidget(SoNode * widget)
{
  SoNode * sep = this->getAnyPart("widgetsSep", FALSE);
  if (!sep) return;
  assert(sep->isOfType(SoGroup::getClassTypeId()));
  SoGroup * grp = static_cast<SoGroup *>(sep);
  int idx = grp->findChild(widget);
  if (idx >= 0) grp->removeChild(idx);
}

/*!
  Override to propagate events to child widgets so that interactive
  HUD nodes such as SoHUDButton can handle mouse input.
*/
void
SoHUDKit::handleEvent(SoHandleEventAction * action)
{
  // Traverse child widgets so they can consume the event.
  SoNode * sep = this->getAnyPart("widgetsSep", FALSE);
  if (sep && sep->isOfType(SoGroup::getClassTypeId())) {
    SoGroup * grp = static_cast<SoGroup *>(sep);
    for (int i = 0; i < grp->getNumChildren(); ++i) {
      grp->getChild(i)->handleEvent(action);
      if (action->isHandled()) return;
    }
  }
}

#endif // HAVE_NODEKITS
