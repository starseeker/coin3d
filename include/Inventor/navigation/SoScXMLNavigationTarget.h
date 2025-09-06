#ifndef COIN_SOSCXMLNAVIGATIONTARGET_H
#define COIN_SOSCXMLNAVIGATIONTARGET_H

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

#include <Inventor/SbBasic.h>
#include <Inventor/SbName.h>
#include <Inventor/SbString.h>

#include <Inventor/tools/SbPimplPtr.h>

class SbVec2f;
class SbVec3f;
class SbRotation;
class SoCamera;
class SoNode;
class SbViewportRegion;

class COIN_DLL_API SoScXMLNavigationTarget {

public:
  static void initClass(void);
  static void cleanClass(void);

  class COIN_DLL_API Data { // virtual base for subclasses using getSessionData()...
  public:
    virtual ~Data(void);
  };

protected:
  SoScXMLNavigationTarget(void);
  virtual ~SoScXMLNavigationTarget(void);

  typedef Data * NewDataFunc(void);
  Data * getSessionData(SbName sessionid, NewDataFunc * constructor);
  void freeSessionData(SbName sessionid);

  // Direct C++ API methods for navigation operations
  static SoCamera * getActiveCamera(SoCamera * camera);
  static SoNode * getSceneGraph(SoNode * scene);

  // Utility methods for parameter validation
  static SbBool validateDouble(double value);
  static SbBool validateSbVec2f(const SbVec2f & vec);
  static SbBool validateSbVec3f(const SbVec3f & vec);
  static SbBool validateSbRotation(const SbRotation & rot);

private:
  class PImpl;
  SbPimplPtr<PImpl> pimpl;

}; // SoScXMLNavigationTarget

#endif // !COIN_SOSCXMLNAVIGATIONTARGET_H
