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
  \class SoGLModelMatrixElement Inventor/elements/SoGLModelMatrixElement.h
  \brief The SoGLModelMatrixElement class is yet to be documented.

  \ingroup coin_elements

  FIXME: write doc.
*/

#include <Inventor/elements/SoGLModelMatrixElement.h>
#include "glue/glp.h"
#include <Inventor/elements/SoGLViewingMatrixElement.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbVec3f.h>

#include "config.h"

#include <Inventor/system/gl.h>
#include "CoinTidbits.h"

#include "misc/SoEnvironment.h"

#if OBOL_DEBUG
#include <Inventor/errors/SoDebugError.h>
#endif // OBOL_DEBUG

SO_ELEMENT_SOURCE(SoGLModelMatrixElement);

static int OBOL_HANDLE_STACK_OVERFLOW = 0;

/*!
  \copydetails SoElement::initClass(void)
*/

void
SoGLModelMatrixElement::initClass(void)
{
  SO_ELEMENT_INIT_CLASS(SoGLModelMatrixElement, inherited);

  const char * env = CoinInternal::getEnvironmentVariableRaw("OBOL_HANDLE_STACK_OVERFLOW");
  if (env && atoi(env) > 0) OBOL_HANDLE_STACK_OVERFLOW = 1;
  else OBOL_HANDLE_STACK_OVERFLOW = 0;
}

/*!
  Destructor.
*/

SoGLModelMatrixElement::~SoGLModelMatrixElement(void)
{
}

//! FIXME: write doc.

void
SoGLModelMatrixElement::init(SoState * stateptr)
{
  this->state = stateptr;
  this->viewEltNodeId = 0;
  this->stackoverflow = FALSE;
  inherited::init(stateptr);
  this->glue = sogl_glue_from_state(stateptr);
}

//! FIXME: write doc.

void
SoGLModelMatrixElement::push(SoState * stateptr)
{
  this->glue = sogl_glue_from_state(stateptr);
  SoGLModelMatrixElement * prev = (SoGLModelMatrixElement*)
    this->getNextInStack();

  // the stackoverflow test makes it possible to have scene graphs
  // with virtually unlimited depth and with transformations inside
  // each separator. If a GL_STACK_OVERFLOW error is encountered,
  // a SoGLContext_glPopMatrix(this->glue) will not be called in the pop() method, but
  // the GL matrix will be read from SoModelMatrixElement instead.
  //                                          pederb, 2000-12-20
  this->stackoverflow = prev->stackoverflow;
  this->state = prev->state;
  this->viewEltNodeId = prev->viewEltNodeId;

  if (OBOL_HANDLE_STACK_OVERFLOW > 0) {
    if (!this->stackoverflow) {
      SoGLContext_glPushMatrix(this->glue);
      if (glGetError() == GL_STACK_OVERFLOW) {
        this->stackoverflow = TRUE;
      }
    }
  }
  else {
    SoGLContext_glPushMatrix(this->glue);
  }
  inherited::push(stateptr);
}

//! FIXME: write doc.

void
SoGLModelMatrixElement::pop(SoState * stateptr,
                            const SoElement * prevTopElement)
{
  inherited::pop(stateptr, prevTopElement);

  SoGLModelMatrixElement * prev = (SoGLModelMatrixElement*)
    prevTopElement;

  if (prev->stackoverflow) {
    SbMatrix mat = SoGLViewingMatrixElement::getResetMatrix(this->state);
    mat.multLeft(this->modelMatrix);
    SoGLContext_glLoadMatrixf(this->glue, mat[0]);
  }
  else {
    SoGLContext_glPopMatrix(this->glue);
  }
}

//! FIXME: write doc.

void
SoGLModelMatrixElement::makeEltIdentity()
{
  SbMatrix mat = SoGLViewingMatrixElement::getResetMatrix(this->state);
  SoGLContext_glLoadMatrixf(this->glue, mat[0]);
  inherited::makeEltIdentity();
}

//! FIXME: write doc.

void
SoGLModelMatrixElement::setElt(const SbMatrix & matrix)
{
  SbMatrix mat = SoGLViewingMatrixElement::getResetMatrix(this->state);
  mat.multLeft(matrix);
  SoGLContext_glLoadMatrixf(this->glue, mat[0]);
  inherited::setElt(matrix);
}

//! FIXME: write doc.

void
SoGLModelMatrixElement::multElt(const SbMatrix &matrix)
{
  SoGLContext_glMultMatrixf(this->glue, matrix[0]);
  inherited::multElt(matrix);
}

//! FIXME: write doc.

void
SoGLModelMatrixElement::translateEltBy(const SbVec3f &translation)
{
  SoGLContext_glTranslatef(this->glue, translation[0], translation[1], translation[2]);
  inherited::translateEltBy(translation);
}

//! FIXME: write doc.

void
SoGLModelMatrixElement::rotateEltBy(const SbRotation &rotation)
{
  SbVec3f axis;
  float angle;
  rotation.getValue(axis, angle);
  SoGLContext_glRotatef(this->glue, angle*180.0f/float(M_PI), axis[0], axis[1], axis[2]);
  inherited::rotateEltBy(rotation);
}

//! FIXME: write doc.

void
SoGLModelMatrixElement::scaleEltBy(const SbVec3f &scaleFactor)
{
  SoGLContext_glScalef(this->glue, scaleFactor[0], scaleFactor[1], scaleFactor[2]);
  inherited::scaleEltBy(scaleFactor);
}

//! FIXME: write doc.

SbMatrix
SoGLModelMatrixElement::pushMatrixElt()
{
  this->viewEltNodeId = SoGLViewingMatrixElement::getNodeId(this->state);
  SoGLContext_glPushMatrix(this->glue);
  return inherited::pushMatrixElt();
}

//! FIXME: write doc.

void
SoGLModelMatrixElement::popMatrixElt(const SbMatrix &matrix)
{
  SoGLContext_glPopMatrix(this->glue);
  if (this->viewEltNodeId != SoGLViewingMatrixElement::getNodeId(this->state)) {
    this->setElt(matrix);
  }
  inherited::popMatrixElt(matrix);
}
