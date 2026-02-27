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
  \class SoHUDLabel SoHUDLabel.h Inventor/annex/HUD/nodes/SoHUDLabel.h
  \brief Screen-space text label for use inside SoHUDKit.

  Renders one or more lines of text at a fixed pixel position using the
  pixel-space orthographic projection established by SoHUDKit.

  \ingroup coin_hud
*/

#include "config.h"

#include <Inventor/annex/HUD/nodes/SoHUDLabel.h>

#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/elements/SoFontSizeElement.h>
#include <Inventor/elements/SoModelMatrixElement.h>
#include <Inventor/elements/SoLazyElement.h>
#include <Inventor/misc/SoState.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbColor.h>

#include "nodes/SoSubNodeP.h"

SO_NODE_SOURCE(SoHUDLabel);

// Doc in superclass.
void
SoHUDLabel::initClass(void)
{
  SO_NODE_INTERNAL_INIT_CLASS(SoHUDLabel, SO_FROM_OBOL_3_0);
}

/*!
  Constructor.  Default values: position (0,0), color white, fontSize 14,
  justification LEFT.
*/
SoHUDLabel::SoHUDLabel(void)
  : textNode(NULL)
{
  SO_NODE_INTERNAL_CONSTRUCTOR(SoHUDLabel);

  SO_NODE_ADD_FIELD(position,      (SbVec2f(0.0f, 0.0f)));
  SO_NODE_ADD_FIELD(string,        (""));
  SO_NODE_ADD_FIELD(color,         (SbColor(1.0f, 1.0f, 1.0f)));
  SO_NODE_ADD_FIELD(fontSize,      (14.0f));
  SO_NODE_ADD_FIELD(justification, (SoHUDLabel::LEFT));

  SO_NODE_DEFINE_ENUM_VALUE(Justification, LEFT);
  SO_NODE_DEFINE_ENUM_VALUE(Justification, RIGHT);
  SO_NODE_DEFINE_ENUM_VALUE(Justification, CENTER);
  SO_NODE_SET_SF_ENUM_TYPE(justification, Justification);
}

/*!
  Destructor.
*/
SoHUDLabel::~SoHUDLabel(void)
{
  if (this->textNode) this->textNode->unref();
}

/*!
  Render the text label at the specified screen-space position.

  This method modifies the model matrix, font-size and diffuse-colour
  elements in the traversal state, then delegates to SoText2 to
  perform the actual glyph rendering.  The state modifications are
  scoped by a push/pop pair so they do not leak to sibling nodes.
*/
void
SoHUDLabel::GLRender(SoGLRenderAction * action)
{
  SoState * state = action->getState();
  state->push();

  // Move the model-matrix origin to the requested pixel position.
  SbVec2f pos = this->position.getValue();
  SoModelMatrixElement::makeIdentity(state, this);
  SoModelMatrixElement::translateBy(state, this,
                                    SbVec3f(pos[0], pos[1], 0.0f));

  // Set font size from our field.
  SoFontSizeElement::set(state, this, this->fontSize.getValue());

  // Set the diffuse colour so SoText2 picks it up.
  SbColor col = this->color.getValue();
  SoColorPacker colorPacker;
  SoLazyElement::setDiffuse(state, this, 1, &col, &colorPacker);

  // Lazy-initialise persistent SoText2 (not created until first GLRender so
  // that constructing a label before SoDB is initialised is safe).
  if (!this->textNode) {
    this->textNode = new SoText2;
    this->textNode->ref();
  }
  this->textNode->string = this->string;
  // Map our justification enum to SoText2's enum (values are identical).
  this->textNode->justification.setValue(this->justification.getValue());
  this->textNode->GLRender(action);

  state->pop();
}
