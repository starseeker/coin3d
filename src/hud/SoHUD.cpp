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
  \class SoHUD SoHUD.h Inventor/annex/HUD/SoHUD.h
  \brief Module initializer for the HUD (Head-Up Display) subsystem.

  Call SoHUD::init() once after SoDB::init() has been called.  This is
  done automatically by SoDB::init(), so application code does not
  normally need to call it directly.

  \ingroup coin_hud
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Inventor/annex/HUD/SoHUD.h>
#include <Inventor/annex/HUD/nodes/SoHUDLabel.h>
#include <Inventor/annex/HUD/nodes/SoHUDButton.h>

/*!
  Initialize all HUD leaf-node classes.  This is called automatically
  from SoDB::init().

  SoHUDKit (which depends on SoBaseKit) is initialized separately from
  SoNodeKit::init() so that the nodekit class hierarchy is available.
*/
void
SoHUD::init(void)
{
  SoHUDLabel::initClass();
  SoHUDButton::initClass();
}
