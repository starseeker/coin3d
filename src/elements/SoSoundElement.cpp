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

/* Minimal stub implementation for sound element functionality - disabled for minimal build */

#include <Inventor/elements/SoSoundElement.h>

#include "coindefs.h"
#include "SbBasicP.h"

#include <Inventor/nodes/SoNode.h>

SO_ELEMENT_SOURCE(SoSoundElement);

void
SoSoundElement::initClass(void)
{
  SO_ELEMENT_INIT_CLASS(SoSoundElement, inherited);
}

SoSoundElement::~SoSoundElement(void)
{
  // Sound support disabled in minimal build
}

void
SoSoundElement::init(SoState * state)
{
  inherited::init(state);
  this->setDefaultValues();
}

void
SoSoundElement::set(SoState * const state,
                    SoNode * const COIN_UNUSED_ARG(node),
                    SbBool scenegraphhassoundnode,
                    SbBool soundnodeisplaying,
                    SbBool ispartofactivescenegraph)
{
  SoSoundElement * elem =
    coin_safe_cast<SoSoundElement *>(SoElement::getElement(state, classStackIndex));
  if (elem) {
    elem->scenegraphhassoundnode = scenegraphhassoundnode;
    elem->soundnodeisplaying = soundnodeisplaying;
    elem->ispartofactivescenegraph = ispartofactivescenegraph;
  }
}

SbBool
SoSoundElement::setSceneGraphHasSoundNode(SoState * const state, SoNode * const node,
                                          SbBool flag)
{
  return FALSE; /* Sound support disabled in minimal build */
}

SbBool
SoSoundElement::sceneGraphHasSoundNode(SoState * const state)
{
  return FALSE; /* Sound support disabled in minimal build */
}

SbBool
SoSoundElement::setSoundNodeIsPlaying(SoState * const state, SoNode * const node,
                                      SbBool flag)
{
  return FALSE; /* Sound support disabled in minimal build */
}

SbBool
SoSoundElement::soundNodeIsPlaying(SoState * const state)
{
  return FALSE; /* Sound support disabled in minimal build */
}

SbBool
SoSoundElement::setIsPartOfActiveSceneGraph(SoState * const state, SoNode * const node,
                                            SbBool flag)
{
  return FALSE; /* Sound support disabled in minimal build */
}

SbBool
SoSoundElement::isPartOfActiveSceneGraph(SoState * const state)
{
  return FALSE; /* Sound support disabled in minimal build */
}

void
SoSoundElement::push(SoState * state)
{
  inherited::push(state);
}

void
SoSoundElement::pop(SoState * state, const SoElement * prevTopElement)
{
  inherited::pop(state, prevTopElement);
}

void
SoSoundElement::print(FILE * file) const
{
  inherited::print(file);
}

void
SoSoundElement::setDefaultValues()
{
  this->scenegraphhassoundnode = FALSE;
  this->soundnodeisplaying = FALSE;
  this->ispartofactivescenegraph = FALSE;
}

