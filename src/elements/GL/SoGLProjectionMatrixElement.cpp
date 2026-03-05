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
  \class SoGLProjectionMatrixElement Inventor/elements/SoGLProjectionMatrixElement.h
  \brief The SoGLProjectionMatrixElement class is yet to be documented.

  \ingroup coin_elements

  FIXME: write doc.
*/

#include <Inventor/elements/SoGLProjectionMatrixElement.h>
#include "glue/glp.h"
#include "config.h"



#include <Inventor/system/gl.h>

#include <cassert>

SO_ELEMENT_SOURCE(SoGLProjectionMatrixElement);

/*!
  \copydetails SoElement::initClass(void)
*/

void
SoGLProjectionMatrixElement::initClass(void)
{
  SO_ELEMENT_INIT_CLASS(SoGLProjectionMatrixElement, inherited);
}

/*!
  Destructor.
*/

SoGLProjectionMatrixElement::~SoGLProjectionMatrixElement(void)
{
}

//! FIXME: write doc.

void
SoGLProjectionMatrixElement::init(SoState * state)
{
  inherited::init(state);
  this->glue = sogl_glue_from_state(state);
}

//! FIXME: write doc.

void
SoGLProjectionMatrixElement::push(SoState * state)
{
  inherited::push(state);
  this->glue = sogl_glue_from_state(state);
}

//! FIXME: write doc.

void
SoGLProjectionMatrixElement::pop(SoState * OBOL_UNUSED_ARG(state),
                                 const SoElement * OBOL_UNUSED_ARG(prevTopElement))
{
  this->capture(state);
  this->updategl();
}

//! FIXME: write doc.

void
SoGLProjectionMatrixElement::setElt(const SbMatrix & matrix)
{
  inherited::setElt(matrix);
  this->updategl();
}

//! FIXME: write doc.

void
SoGLProjectionMatrixElement::updategl(void)
{
  SoGLContext_glMatrixMode(this->glue, GL_PROJECTION);
  SoGLContext_glLoadMatrixf(this->glue, (float*)this->projectionMatrix);
  SoGLContext_glMatrixMode(this->glue, GL_MODELVIEW);
}
