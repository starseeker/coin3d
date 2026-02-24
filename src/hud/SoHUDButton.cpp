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
  \class SoHUDButton SoHUDButton.h Inventor/annex/HUD/nodes/SoHUDButton.h
  \brief Interactive clickable button widget for use inside SoHUDKit.

  The button draws a rectangular border (as 2-D line geometry) and a
  centred text label.  When mouse button 1 is pressed within the
  bounding rectangle, all registered click callbacks are invoked and
  the event is consumed.

  Coordinates are in viewport pixels, matching the SoHUDKit convention
  (origin at the lower-left corner).

  \ingroup coin_hud
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Inventor/annex/HUD/nodes/SoHUDButton.h>

#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/elements/SoFontSizeElement.h>
#include <Inventor/elements/SoModelMatrixElement.h>
#include <Inventor/elements/SoLazyElement.h>
#include <Inventor/elements/SoViewportRegionElement.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/misc/SoState.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbColor.h>
#include <Inventor/lists/SbList.h>
#include <Inventor/system/gl.h>

#include "nodes/SoSubNodeP.h"

// ---------------------------------------------------------------------------

struct CallbackEntry {
  SoHUDButtonCB * func;
  void           * userdata;

  SbBool operator==(const CallbackEntry & o) const {
    return this->func == o.func && this->userdata == o.userdata;
  }
  SbBool operator!=(const CallbackEntry & o) const {
    return !(*this == o);
  }
};

struct SoHUDButtonP {
  SbList<CallbackEntry> callbacks;
  SoText2 * textNode; // lazy-initialised on first GLRender, ref-counted

  SoHUDButtonP(void) : textNode(NULL) {}
  ~SoHUDButtonP(void) {
    if (this->textNode) this->textNode->unref();
  }
};

// ---------------------------------------------------------------------------

SO_NODE_SOURCE(SoHUDButton);

// Doc in superclass.
void
SoHUDButton::initClass(void)
{
  SO_NODE_INTERNAL_INIT_CLASS(SoHUDButton, SO_FROM_COIN_3_0);
}

/*!
  Constructor.  Default values: position (0,0), size (80,24), empty
  label, white text, light-grey border, fontSize 12.
*/
SoHUDButton::SoHUDButton(void)
  : pimpl(new SoHUDButtonP)
{
  SO_NODE_INTERNAL_CONSTRUCTOR(SoHUDButton);

  SO_NODE_ADD_FIELD(position,    (SbVec2f(0.0f, 0.0f)));
  SO_NODE_ADD_FIELD(size,        (SbVec2f(80.0f, 24.0f)));
  SO_NODE_ADD_FIELD(string,      (""));
  SO_NODE_ADD_FIELD(color,       (SbColor(1.0f, 1.0f, 1.0f)));
  SO_NODE_ADD_FIELD(borderColor, (SbColor(0.7f, 0.7f, 0.7f)));
  SO_NODE_ADD_FIELD(fontSize,    (12.0f));
}

/*!
  Destructor.
*/
SoHUDButton::~SoHUDButton(void)
{
  delete pimpl;
}

/*!
  Register a callback to be invoked when the button is clicked.
  \a cb must not be NULL.
*/
void
SoHUDButton::addClickCallback(SoHUDButtonCB * cb, void * userdata)
{
  CallbackEntry entry;
  entry.func     = cb;
  entry.userdata = userdata;
  pimpl->callbacks.append(entry);
}

/*!
  Remove a previously registered click callback.  If the same
  function/userdata pair was added more than once only the first
  occurrence is removed.
*/
void
SoHUDButton::removeClickCallback(SoHUDButtonCB * cb, void * userdata)
{
  CallbackEntry entry;
  entry.func     = cb;
  entry.userdata = userdata;
  int idx = pimpl->callbacks.find(entry);
  if (idx >= 0) pimpl->callbacks.removeFast(idx);
}

/*!
  Render the button border and centred label.

  The border is drawn as a GL_LINE_LOOP rectangle in pixel space.
  The label uses SoText2 for glyph rendering, positioned at the
  centre of the button.
*/
void
SoHUDButton::GLRender(SoGLRenderAction * action)
{
  SoState * state = action->getState();

  SbVec2f pos  = this->position.getValue();
  SbVec2f sz   = this->size.getValue();
  SbColor bcol = this->borderColor.getValue();
  SbColor tcol = this->color.getValue();

  float x0 = pos[0];
  float y0 = pos[1];
  float x1 = x0 + sz[0];
  float y1 = y0 + sz[1];

  // Draw the border rectangle using immediate-mode GL.
  // Depth test has already been disabled by SoHUDKit.
  glColor3f(bcol[0], bcol[1], bcol[2]);
  glBegin(GL_LINE_LOOP);
    glVertex3f(x0, y0, 0.0f);
    glVertex3f(x1, y0, 0.0f);
    glVertex3f(x1, y1, 0.0f);
    glVertex3f(x0, y1, 0.0f);
  glEnd();

  // Draw the centred text label.
  state->push();

  float cx = x0 + sz[0] * 0.5f;
  float cy = y0 + sz[1] * 0.5f;
  SoModelMatrixElement::makeIdentity(state, this);
  SoModelMatrixElement::translateBy(state, this, SbVec3f(cx, cy, 0.0f));

  SoFontSizeElement::set(state, this, this->fontSize.getValue());

  SoColorPacker colorPacker;
  SoLazyElement::setDiffuse(state, this, 1, &tcol, &colorPacker);

  // Lazy-initialise persistent SoText2 for the button label.
  if (!pimpl->textNode) {
    pimpl->textNode = new SoText2;
    pimpl->textNode->ref();
    pimpl->textNode->justification.setValue(SoText2::CENTER);
  }
  pimpl->textNode->string.setValue(this->string.getValue().getString());
  pimpl->textNode->GLRender(action);

  state->pop();
}

/*!
  Handle an event.  If a mouse-button-1 press event falls within the
  button's bounding rectangle (in viewport pixel coordinates) all
  registered click callbacks are invoked and the event is consumed.
*/
void
SoHUDButton::handleEvent(SoHandleEventAction * action)
{
  const SoEvent * ev = action->getEvent();
  if (!SO_MOUSE_PRESS_EVENT(ev, BUTTON1)) return;

  // Get the mouse position in viewport pixels (Y measured from bottom).
  const SbViewportRegion & vp = action->getViewportRegion();
  const SbVec2s & rawPos = ev->getPosition(vp);

  float mx = float(rawPos[0]);
  float my = float(rawPos[1]);

  SbVec2f pos = this->position.getValue();
  SbVec2f sz  = this->size.getValue();

  if (mx >= pos[0] && mx <= pos[0] + sz[0] &&
      my >= pos[1] && my <= pos[1] + sz[1]) {
    // Invoke all click callbacks.
    for (int i = 0; i < pimpl->callbacks.getLength(); ++i) {
      pimpl->callbacks[i].func(pimpl->callbacks[i].userdata, this);
    }
    action->setHandled();
  }
}
