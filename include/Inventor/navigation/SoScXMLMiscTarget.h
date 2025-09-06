#ifndef COIN_SOSCXMLMISCTARGET_H
#define COIN_SOSCXMLMISCTARGET_H

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

class SoCamera;
class SoNode;
class SbViewportRegion;

class COIN_DLL_API SoScXMLMiscTarget : public SoScXMLNavigationTarget {

public:
  static void initClass(void);
  static void cleanClass(void);

  static SoScXMLMiscTarget * constructSingleton(void);
  static void destructSingleton(void);
  static SoScXMLMiscTarget * singleton(void);

  // Direct C++ API methods replacing SCXML event handling
  static SbBool viewAll(SoCamera * camera, SoNode * sceneGraph, const SbViewportRegion & viewport);
  static SbBool redraw(SoNode * sceneGraph);
  static SbBool pointAt(SoCamera * camera, const SbVec3f & focusPoint);
  static SbBool pointAt(SoCamera * camera, const SbVec3f & focusPoint, const SbVec3f & upVector);
  static SbBool setFocalDistance(SoCamera * camera, float distance);
  static SbBool setCameraPosition(SoCamera * camera, const SbVec3f & position);

  // Legacy event name access (for compatibility)
  static const SbName & VIEW_ALL(void);
  static const SbName & REDRAW(void);
  static const SbName & POINT_AT(void);
  static const SbName & SET_FOCAL_DISTANCE(void);
  static const SbName & SET_CAMERA_POSITION(void);

protected:
  SoScXMLMiscTarget(void);
  virtual ~SoScXMLMiscTarget(void);

private:
  class PImpl;
  static SoScXMLMiscTarget * theSingleton;

}; // SoScXMLMiscTarget

#endif // !COIN_SOSCXMLMISCTARGET_H
