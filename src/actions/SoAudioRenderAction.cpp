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

/* Minimal stub implementation for audio rendering functionality - disabled for minimal build */

#include <Inventor/actions/SoAudioRenderAction.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif // HAVE_CONFIG_H

#include <Inventor/nodes/SoNode.h>
#include <Inventor/SoType.h>
#include <Inventor/SbName.h>

// Minimal pimpl class for compatibility
class SoAudioRenderActionP {
public:
  SoAudioRenderActionP() {}
  ~SoAudioRenderActionP() {}
};

// Manual type system implementation for stub
static SoType SoAudioRenderAction_classTypeId;

void
SoAudioRenderAction::initClass(void)
{
  // Audio rendering disabled in minimal build - register minimal type
  SoAudioRenderAction_classTypeId = 
    SoType::createType(SoAction::getClassTypeId(), 
                       SbName("SoAudioRenderAction"),
                       NULL, 0);
}

SoType
SoAudioRenderAction::getTypeId(void) const
{
  return SoAudioRenderAction_classTypeId;
}

SoType
SoAudioRenderAction::getClassTypeId(void)
{
  return SoAudioRenderAction_classTypeId;
}

SoAudioRenderAction::SoAudioRenderAction(void)
{
  // Audio rendering disabled in minimal build
  this->pimpl.set(new SoAudioRenderActionP);
}

SoAudioRenderAction::~SoAudioRenderAction(void)
{
  // Audio rendering disabled in minimal build
}

void
SoAudioRenderAction::callDoAction(SoAction *action, SoNode *node)
{
  // Audio rendering disabled in minimal build
}

void
SoAudioRenderAction::callAudioRender(SoAction *action, SoNode *node)
{
  // Audio rendering disabled in minimal build
}

void
SoAudioRenderAction::beginTraversal(SoNode *node)
{
  // Audio rendering disabled in minimal build
  inherited::beginTraversal(node);
}
