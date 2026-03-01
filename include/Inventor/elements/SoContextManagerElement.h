#ifndef OBOL_SOCONTEXTMANAGERELEMENT_H
#define OBOL_SOCONTEXTMANAGERELEMENT_H

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

#include <Inventor/elements/SoSubElement.h>
#include <Inventor/SoDB.h>

/*!
  \class SoContextManagerElement SoContextManagerElement.h Inventor/elements/SoContextManagerElement.h
  \brief Carries the active SoDB::ContextManager through the GL render state.

  SoOffscreenRenderer pushes the per-instance context manager onto the
  state before traversal.  Scene-graph nodes that need to create their
  own offscreen GL contexts (SoSceneTexture2, SoSceneTextureCubeMap,
  SoShadowGroup, etc.) read it from the state so that no node ever has
  to call SoDB::getContextManager() directly.

  \ingroup coin_elements
*/

class OBOL_DLL_API SoContextManagerElement : public SoElement {
  typedef SoElement inherited;

  SO_ELEMENT_HEADER(SoContextManagerElement);

public:
  static void initClass(void);
protected:
  virtual ~SoContextManagerElement();

public:
  virtual void init(SoState * state);

  virtual SbBool matches(const SoElement * elt) const;
  virtual SoElement * copyMatchInfo(void) const;

  static void set(SoState * state, SoDB::ContextManager * manager);
  static SoDB::ContextManager * get(SoState * state);

protected:
  SoDB::ContextManager * manager;
};

#endif // !OBOL_SOCONTEXTMANAGERELEMENT_H
