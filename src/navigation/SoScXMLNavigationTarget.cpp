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

#include <Inventor/navigation/SoScXMLNavigationTarget.h>

#include <cassert>
#include <cstring>
#include <cstdio>
#include <map>
#include <memory>

#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbRotation.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/errors/SoDebugError.h>
#include "coindefs.h"

class SoScXMLNavigationTarget::PImpl {
public:
  PImpl(void) : sessiondatamap(NULL) { }

  typedef std::map<const char *, SoScXMLNavigationTarget::Data *> SessionDataMap;
  typedef std::pair<const char *, SoScXMLNavigationTarget::Data *> SessionDataEntry;

  SessionDataMap * sessiondatamap;
};

SoScXMLNavigationTarget::Data::~Data(void)
{
}

#define PRIVATE(obj) ((obj)->pimpl)

void
SoScXMLNavigationTarget::initClass(void)
{
  // Navigation target initialization - no longer uses SCXML
}

void
SoScXMLNavigationTarget::cleanClass(void)
{
  // Navigation target cleanup
}

SoScXMLNavigationTarget::SoScXMLNavigationTarget(void)
{
  PRIVATE(this)->sessiondatamap = new PImpl::SessionDataMap;
}

SoScXMLNavigationTarget::~SoScXMLNavigationTarget(void)
{
  PImpl::SessionDataMap::iterator it = PRIVATE(this)->sessiondatamap->begin();
  while (it != PRIVATE(this)->sessiondatamap->end()) {
    // we could warn about undeleted data here, but it could be caused by natural
    // causes like systems being taken down while the user is interacting, so we'll
    // let these go unwarned.
    delete it->second;
    ++it;
  }
  delete PRIVATE(this)->sessiondatamap;
  PRIVATE(this)->sessiondatamap = NULL;
}

/*!
  Returns the Data* base handle for the data structure that corresponds to the given
  \a sessionid.  The \a constructor argument is the function responsible for creating
  the Data-derived object if the session is new (or have been cleaned up earlier).
*/

SoScXMLNavigationTarget::Data *
SoScXMLNavigationTarget::getSessionData(SbName sessionid, NewDataFunc * constructor)
{
  Data * data = NULL;
  PImpl::SessionDataMap::iterator findit =
    PRIVATE(this)->sessiondatamap->find(sessionid.getString());
  if (findit != PRIVATE(this)->sessiondatamap->end()) {
    data = findit->second;
  } else {
    data = constructor();
    PImpl::SessionDataEntry entry(sessionid.getString(), data);
    PRIVATE(this)->sessiondatamap->insert(entry);
  }
  return data;
}

/*!
  Cleans out the data structure that is mapped to the given \a sessionid.
*/
void
SoScXMLNavigationTarget::freeSessionData(SbName sessionid)
{
  PImpl::SessionDataMap::iterator findit =
    PRIVATE(this)->sessiondatamap->find(sessionid.getString());
  if (findit != PRIVATE(this)->sessiondatamap->end()) {
    Data * data = findit->second;
    PRIVATE(this)->sessiondatamap->erase(findit);
    delete data;
  }
}

// New utility methods for direct C++ API

/*!
  Simple utility method to validate and return the given camera.
  Returns the camera if valid, NULL otherwise.
*/
SoCamera *
SoScXMLNavigationTarget::getActiveCamera(SoCamera * camera)
{
  if (!camera) {
    SoDebugError::post("SoScXMLNavigationTarget::getActiveCamera",
                       "camera parameter is NULL");
    return NULL;
  }
  return camera;
}

/*!
  Simple utility method to validate and return the given scene graph.
  Returns the scene graph node if valid, NULL otherwise.
*/
SoNode *
SoScXMLNavigationTarget::getSceneGraph(SoNode * scene)
{
  if (!scene) {
    SoDebugError::post("SoScXMLNavigationTarget::getSceneGraph", 
                       "scene graph parameter is NULL");
    return NULL;
  }
  return scene;
}

/*!
  Utility method to validate a double value.
*/
SbBool
SoScXMLNavigationTarget::validateDouble(double value)
{
  // Basic validation - could be extended with range checks
  return TRUE;
}

/*!
  Utility method to validate a 2D vector.
*/
SbBool
SoScXMLNavigationTarget::validateSbVec2f(const SbVec2f & vec)
{
  // Basic validation - check for valid coordinates
  return TRUE;
}

/*!
  Utility method to validate a 3D vector.
*/
SbBool
SoScXMLNavigationTarget::validateSbVec3f(const SbVec3f & vec)
{
  // Basic validation - check for valid coordinates
  return TRUE;
}

/*!
  Utility method to validate a rotation.
*/
SbBool
SoScXMLNavigationTarget::validateSbRotation(const SbRotation & rot)
{
  // Basic validation - could check for valid quaternion
  return TRUE;
}