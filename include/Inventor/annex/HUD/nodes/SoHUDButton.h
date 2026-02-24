#ifndef COIN_SOHUDBUTTON_H
#define COIN_SOHUDBUTTON_H

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
  \brief Interactive, clickable button widget for use inside SoHUDKit.

  SoHUDButton draws a rectangular button with a text label.  The border
  is rendered as 2-D line geometry.  When the user clicks mouse button 1
  inside the button's bounding rectangle, all registered click callbacks
  are invoked and the event is consumed.

  Pixel coordinates are measured from the lower-left viewport corner,
  matching the SoHUDKit coordinate convention.

  Example:
  \code
  void onQuit(void * data, SoHUDButton * btn) {
      std::exit(0);
  }

  SoHUDButton * btn = new SoHUDButton;
  btn->position.setValue(20.0f, 20.0f);
  btn->size.setValue(100.0f, 28.0f);
  btn->string.setValue("Quit");
  btn->addClickCallback(onQuit, nullptr);
  hud->addWidget(btn);
  \endcode

  \ingroup coin_hud
*/

#include <Inventor/nodes/SoSubNode.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/fields/SoSFVec2f.h>
#include <Inventor/fields/SoSFString.h>
#include <Inventor/fields/SoSFColor.h>
#include <Inventor/fields/SoSFFloat.h>

class SoHUDButton;

/*! Callback type for SoHUDButton click notifications. */
typedef void SoHUDButtonCB(void * userdata, SoHUDButton * button);

class COIN_DLL_API SoHUDButton : public SoNode {
  typedef SoNode inherited;
  SO_NODE_HEADER(SoHUDButton);

public:
  static void initClass(void);
  SoHUDButton(void);

  /*! Lower-left corner of the button in viewport pixels. */
  SoSFVec2f   position;
  /*! Width and height of the button in pixels. */
  SoSFVec2f   size;
  /*! Button label text. */
  SoSFString  string;
  /*! Label text colour (default white). */
  SoSFColor   color;
  /*! Border and background line colour (default light-grey). */
  SoSFColor   borderColor;
  /*! Font size in points for the label (default 12). */
  SoSFFloat   fontSize;

  void addClickCallback(SoHUDButtonCB * cb, void * userdata = NULL);
  void removeClickCallback(SoHUDButtonCB * cb, void * userdata = NULL);

  virtual void GLRender(SoGLRenderAction * action);
  virtual void handleEvent(SoHandleEventAction * action);

protected:
  virtual ~SoHUDButton(void);

private:
  struct SoHUDButtonP * pimpl;
};

#endif // !COIN_SOHUDBUTTON_H
