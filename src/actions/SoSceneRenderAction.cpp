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
  \class SoSceneRenderAction SoSceneRenderAction.h Inventor/actions/SoSceneRenderAction.h
  \brief Action for traversing a scene graph to collect geometry for a rendering backend.

  \ingroup coin_actions

  See the class header documentation and docs/BACKEND_SURVEY.md for full
  details on how to use this action with raytracing backends such as nanort
  or OSPRay.
*/

#include <Inventor/actions/SoSceneRenderAction.h>

#include <Inventor/elements/SoLightElement.h>
#include <Inventor/elements/SoViewportRegionElement.h>
#include <Inventor/misc/SoState.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoLight.h>

#include "actions/SoSubActionP.h"

SO_ACTION_SOURCE(SoSceneRenderAction);

// Internal pre-callback used to collect enabled light nodes and their
// world-space transforms during traversal.
static SoCallbackAction::Response
raytrace_light_pre_cb(void * userdata, SoCallbackAction * action,
                      const SoNode * node)
{
  SoSceneRenderAction::LightCbData * data =
    static_cast<SoSceneRenderAction::LightCbData *>(userdata);
  // Capture the light node.  We cast away const since SoNodeList stores
  // non-const SoNode pointers, and we only read the light data later.
  data->lights->append(const_cast<SoNode *>(node));
  data->transforms->append(action->getModelMatrix());
  return SoCallbackAction::CONTINUE;
}

/*!
  \copydetails SoAction::initClass(void)
*/
void
SoSceneRenderAction::initClass(void)
{
  SO_ACTION_INTERNAL_INIT_CLASS(SoSceneRenderAction, SoCallbackAction);
}

/*!
  Constructor.  Pass in the viewport region that defines the dimensions of
  the render target (width/height in pixels).  This is used by shape nodes
  that compute screen-space geometry (e.g. SoText2, SoImage).
*/
SoSceneRenderAction::SoSceneRenderAction(const SbViewportRegion & viewportregion)
  : inherited(viewportregion),
    vpregion(viewportregion)
{
  SO_ACTION_CONSTRUCTOR(SoSceneRenderAction);

  // Initialize the callback data struct with pointers to the two caches.
  this->light_cb_data.lights     = &this->lights_cache;
  this->light_cb_data.transforms = &this->light_transforms_cache;

  // Register a pre-callback for all SoLight subclasses so that each
  // light node and its world-space transform are captured into the
  // caches during traversal.
  this->addPreCallback(SoLight::getClassTypeId(),
                       raytrace_light_pre_cb,
                       &this->light_cb_data);
}

/*!
  Destructor.
*/
SoSceneRenderAction::~SoSceneRenderAction(void)
{
}

/*!
  Set the viewport region for the render target.
*/
void
SoSceneRenderAction::setViewportRegion(const SbViewportRegion & newregion)
{
  this->vpregion = newregion;
  this->inherited::setViewportRegion(newregion);
}

/*!
  Returns the current viewport region.
*/
const SbViewportRegion &
SoSceneRenderAction::getViewportRegion(void) const
{
  return this->vpregion;
}

/*!
  Returns the list of \c SoLight nodes that were visited during the most
  recent \c apply() traversal.

  Each entry is a \c SoLight subclass --- typically \c SoDirectionalLight,
  \c SoPointLight, or \c SoSpotLight.  Cast the entries as needed:

  \code
  const SoNodeList & lights = rta.getLights();
  for (int i = 0; i < lights.getLength(); ++i) {
    SoLight * light = static_cast<SoLight *>(lights[i]);
    if (light->isOfType(SoDirectionalLight::getClassTypeId())) {
      SoDirectionalLight * dl = static_cast<SoDirectionalLight *>(light);
      // Access: dl->direction, dl->color, dl->intensity ...
    }
  }
  \endcode

  \note The returned list is repopulated on each call to \c apply().
        Call this method \e after \c apply() completes.
  \note All \c SoLight nodes encountered in the scene graph are listed,
        including disabled ones (SoLight::on == FALSE).  Check the \c on
        field before using a light if you only want enabled lights.
*/
const SoNodeList &
SoSceneRenderAction::getLights(void) const
{
  return this->lights_cache;
}

// Override beginTraversal to clear the lights caches before each traversal,
// then delegate to SoCallbackAction which sets up the state and traverses
// the scene graph.  The raytrace_light_pre_cb callback registered in the
// constructor populates both caches as each SoLight node is visited.
void
SoSceneRenderAction::beginTraversal(SoNode * node)
{
  this->lights_cache.truncate(0);
  this->light_transforms_cache.truncate(0);
  this->inherited::beginTraversal(node);
}

/*!
  Returns the world-space model matrix for each light in getLights(),
  captured at the moment the light node was first encountered during
  traversal.

  \note Call this after apply(); the list is populated during traversal.
*/
const SbList<SbMatrix> &
SoSceneRenderAction::getLightTransforms(void) const
{
  return this->light_transforms_cache;
}
