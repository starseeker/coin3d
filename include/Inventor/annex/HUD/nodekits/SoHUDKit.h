#ifndef COIN_SOHUDKIT_H
#define COIN_SOHUDKIT_H

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

  SoHUDKit renders its children on top of the 3-D scene using a
  pixel-space orthographic camera with the depth buffer disabled.
  The coordinate origin (0, 0) is at the lower-left corner of the
  viewport; one unit equals one pixel.

  Usage:

  \code
  SoHUDKit * hud = new SoHUDKit;

  SoHUDLabel * label = new SoHUDLabel;
  label->position.setValue(10.0f, 10.0f);
  label->string.setValue("Hello, HUD!");
  hud->addWidget(label);

  SoHUDButton * btn = new SoHUDButton;
  btn->position.setValue(100.0f, 50.0f);
  btn->size.setValue(120.0f, 30.0f);
  btn->string.setValue("Click me");
  btn->addClickCallback(myCallback, myData);
  hud->addWidget(btn);

  // Add hud as a sibling of the 3-D scene root in your top-level separator.
  root->addChild(hud);
  \endcode

  \ingroup coin_hud
*/

#include <Inventor/nodekits/SoBaseKit.h>
#include <Inventor/nodekits/SoSubKit.h>
#include <Inventor/fields/SoSFVec2f.h>

class COIN_DLL_API SoHUDKit : public SoBaseKit {
  typedef SoBaseKit inherited;
  SO_KIT_HEADER(SoHUDKit);
  SO_KIT_CATALOG_ENTRY_HEADER(topSeparator);
  SO_KIT_CATALOG_ENTRY_HEADER(viewportInfo);
  SO_KIT_CATALOG_ENTRY_HEADER(overlayCamera);
  SO_KIT_CATALOG_ENTRY_HEADER(depthTestOff);
  SO_KIT_CATALOG_ENTRY_HEADER(widgetsSep);
  SO_KIT_CATALOG_ENTRY_HEADER(depthTestOn);

public:
  static void initClass(void);
  SoHUDKit(void);

  /*! Current viewport size in pixels (updated automatically during render). */
  SoSFVec2f viewportSize;

  void addWidget(SoNode * widget);
  void removeWidget(SoNode * widget);

  virtual void handleEvent(SoHandleEventAction * action);

protected:
  virtual ~SoHUDKit(void);

private:
  static void grabViewportInfo(void * userdata, SoAction * action);
};

#endif // !COIN_SOHUDKIT_H
