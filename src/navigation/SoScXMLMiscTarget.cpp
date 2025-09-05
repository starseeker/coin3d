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

#include <Inventor/navigation/SoScXMLMiscTarget.h>

/*!
  \class SoScXMLMiscTarget SoScXMLMiscTarget.h Inventor/navigation/SoScXMLMiscTarget.h
  \brief Miscellaneous navigation utility functions.

  \ingroup coin_navigation
*/

#include <cassert>

#include <Inventor/SbName.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/errors/SoDebugError.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoNode.h>
#include "coindefs.h"

// *************************************************************************

class SoScXMLMiscTarget::PImpl {
public:
  static SbName VIEW_ALL;
  static SbName REDRAW;
  static SbName POINT_AT;
  static SbName SET_FOCAL_DISTANCE;
  static SbName SET_CAMERA_POSITION;
};

SbName SoScXMLMiscTarget::PImpl::VIEW_ALL;
SbName SoScXMLMiscTarget::PImpl::REDRAW;
SbName SoScXMLMiscTarget::PImpl::POINT_AT;
SbName SoScXMLMiscTarget::PImpl::SET_FOCAL_DISTANCE;
SbName SoScXMLMiscTarget::PImpl::SET_CAMERA_POSITION;

#define PRIVATE

void
SoScXMLMiscTarget::initClass(void)
{
  // Initialize event names for compatibility
  SoScXMLMiscTarget::PImpl::VIEW_ALL = "VIEW_ALL";
  SoScXMLMiscTarget::PImpl::REDRAW = "REDRAW";
  SoScXMLMiscTarget::PImpl::POINT_AT = "POINT_AT";
  SoScXMLMiscTarget::PImpl::SET_FOCAL_DISTANCE = "SET_FOCAL_DISTANCE";
  SoScXMLMiscTarget::PImpl::SET_CAMERA_POSITION = "SET_CAMERA_POSITION";
}

void
SoScXMLMiscTarget::cleanClass(void)
{
  // Clean up
}

SoScXMLMiscTarget * SoScXMLMiscTarget::theSingleton = NULL;

SoScXMLMiscTarget *
SoScXMLMiscTarget::constructSingleton(void)
{
  if (SoScXMLMiscTarget::theSingleton == NULL) {
    SoScXMLMiscTarget::theSingleton = new SoScXMLMiscTarget;
  }
  return SoScXMLMiscTarget::theSingleton;
}

void
SoScXMLMiscTarget::destructSingleton(void)
{
  delete SoScXMLMiscTarget::theSingleton;
  SoScXMLMiscTarget::theSingleton = NULL;
}

SoScXMLMiscTarget *
SoScXMLMiscTarget::singleton(void)
{
  return SoScXMLMiscTarget::theSingleton;
}

// Legacy event name accessors for compatibility
const SbName &
SoScXMLMiscTarget::VIEW_ALL(void)
{
  return SoScXMLMiscTarget::PImpl::VIEW_ALL;
}

const SbName &
SoScXMLMiscTarget::REDRAW(void)
{
  return SoScXMLMiscTarget::PImpl::REDRAW;
}

const SbName &
SoScXMLMiscTarget::POINT_AT(void)
{
  return SoScXMLMiscTarget::PImpl::POINT_AT;
}

const SbName &
SoScXMLMiscTarget::SET_FOCAL_DISTANCE(void)
{
  return SoScXMLMiscTarget::PImpl::SET_FOCAL_DISTANCE;
}

const SbName &
SoScXMLMiscTarget::SET_CAMERA_POSITION(void)
{
  return SoScXMLMiscTarget::PImpl::SET_CAMERA_POSITION;
}

SoScXMLMiscTarget::SoScXMLMiscTarget(void)
{
}

SoScXMLMiscTarget::~SoScXMLMiscTarget(void)
{
}

// New direct C++ API methods

/*!
  Performs a viewAll operation on the camera to fit the entire scene graph in the viewport.
*/
SbBool
SoScXMLMiscTarget::viewAll(SoCamera * camera, SoNode * sceneGraph, const SbViewportRegion & viewport)
{
  if (!camera) {
    SoDebugError::post("SoScXMLMiscTarget::viewAll", "camera parameter is NULL");
    return FALSE;
  }
  
  if (!sceneGraph) {
    SoDebugError::post("SoScXMLMiscTarget::viewAll", "sceneGraph parameter is NULL");
    return FALSE;
  }

  camera->viewAll(sceneGraph, viewport);
  return TRUE;
}

/*!
  Triggers a redraw by marking the scene graph as modified.
*/
SbBool
SoScXMLMiscTarget::redraw(SoNode * sceneGraph)
{
  if (!sceneGraph) {
    SoDebugError::post("SoScXMLMiscTarget::redraw", "sceneGraph parameter is NULL");
    return FALSE;
  }

  sceneGraph->touch();
  return TRUE;
}

/*!
  Points the camera at the specified focus point.
*/
SbBool
SoScXMLMiscTarget::pointAt(SoCamera * camera, const SbVec3f & focusPoint)
{
  if (!camera) {
    SoDebugError::post("SoScXMLMiscTarget::pointAt", "camera parameter is NULL");
    return FALSE;
  }

  camera->pointAt(focusPoint);
  return TRUE;
}

/*!
  Points the camera at the specified focus point with an up vector.
*/
SbBool
SoScXMLMiscTarget::pointAt(SoCamera * camera, const SbVec3f & focusPoint, const SbVec3f & upVector)
{
  if (!camera) {
    SoDebugError::post("SoScXMLMiscTarget::pointAt", "camera parameter is NULL");
    return FALSE;
  }

  camera->pointAt(focusPoint, upVector);
  return TRUE;
}

/*!
  Sets the focal distance of the camera.
*/
SbBool
SoScXMLMiscTarget::setFocalDistance(SoCamera * camera, float distance)
{
  if (!camera) {
    SoDebugError::post("SoScXMLMiscTarget::setFocalDistance", "camera parameter is NULL");
    return FALSE;
  }

  camera->focalDistance.setValue(distance);
  return TRUE;
}

/*!
  Sets the position of the camera.
*/
SbBool
SoScXMLMiscTarget::setCameraPosition(SoCamera * camera, const SbVec3f & position)
{
  if (!camera) {
    SoDebugError::post("SoScXMLMiscTarget::setCameraPosition", "camera parameter is NULL");
    return FALSE;
  }

  camera->position.setValue(position);
  return TRUE;
}