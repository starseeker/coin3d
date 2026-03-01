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
  \class SoContextManagerElement SoContextManagerElement.h Inventor/elements/SoContextManagerElement.h
  \brief Carries the active SoDB::ContextManager through the GL render state.

  \ingroup coin_elements
*/

#include <Inventor/elements/SoContextManagerElement.h>
#include "SbBasicP.h"
#include <cassert>

SO_ELEMENT_SOURCE(SoContextManagerElement);

/*!
  \copydetails SoElement::initClass(void)
*/
void
SoContextManagerElement::initClass(void)
{
  SO_ELEMENT_INIT_CLASS(SoContextManagerElement, inherited);
}

/*!
  Destructor.
*/
SoContextManagerElement::~SoContextManagerElement(void)
{
}

// doc in parent
void
SoContextManagerElement::init(SoState * state)
{
  inherited::init(state);
  this->manager = NULL;
}

// doc in parent
SbBool
SoContextManagerElement::matches(const SoElement * elt) const
{
  const SoContextManagerElement * other =
    static_cast<const SoContextManagerElement *>(elt);
  return this->manager == other->manager;
}

// doc in parent
SoElement *
SoContextManagerElement::copyMatchInfo(void) const
{
  SoContextManagerElement * elem =
    static_cast<SoContextManagerElement *>(this->getTypeId().createInstance());
  elem->manager = this->manager;
  return elem;
}

/*!
  Push a new context manager onto the state.
*/
void
SoContextManagerElement::set(SoState * state, SoDB::ContextManager * manager)
{
  SoContextManagerElement * elem =
    static_cast<SoContextManagerElement *>(state->getElement(classStackIndex));
  if (elem) {
    elem->manager = manager;
  }
}

/*!
  Returns the current context manager from the state.  Returns NULL
  when no manager has been pushed onto this state yet.
*/
SoDB::ContextManager *
SoContextManagerElement::get(SoState * state)
{
  const SoContextManagerElement * elem =
    static_cast<const SoContextManagerElement *>(
      state->getConstElement(classStackIndex));
  return elem ? elem->manager : NULL;
}
