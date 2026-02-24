#ifndef COIN_SOHUDLABEL_H
#define COIN_SOHUDLABEL_H

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

  Renders one or more lines of text at a fixed pixel position.
  The \c position field specifies the anchor point in pixels from the
  lower-left corner of the viewport.

  Example:
  \code
  SoHUDLabel * fps = new SoHUDLabel;
  fps->position.setValue(10.0f, 10.0f);
  fps->string.setValue("FPS: 60");
  fps->color.setValue(SbColor(1.0f, 1.0f, 0.0f));
  fps->fontSize.setValue(14.0f);
  hud->addWidget(fps);
  \endcode

  \ingroup coin_hud
*/

#include <Inventor/nodes/SoSubNode.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/fields/SoSFVec2f.h>
#include <Inventor/fields/SoMFString.h>
#include <Inventor/fields/SoSFColor.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFEnum.h>

class SoText2;

class COIN_DLL_API SoHUDLabel : public SoNode {
  typedef SoNode inherited;
  SO_NODE_HEADER(SoHUDLabel);

public:
  static void initClass(void);
  SoHUDLabel(void);

  enum Justification {
    LEFT   = 1,
    RIGHT  = 2,
    CENTER = 3
  };

  /*! Anchor position in pixels from the lower-left viewport corner. */
  SoSFVec2f   position;
  /*! Text lines to display. */
  SoMFString  string;
  /*! Text colour (default white). */
  SoSFColor   color;
  /*! Font size in points (default 14). */
  SoSFFloat   fontSize;
  /*! Horizontal text justification relative to \c position. */
  SoSFEnum    justification;

  virtual void GLRender(SoGLRenderAction * action);

protected:
  virtual ~SoHUDLabel(void);

private:
  SoText2 * textNode; // lazy-initialised on first GLRender, ref-counted
};

#endif // !COIN_SOHUDLABEL_H
