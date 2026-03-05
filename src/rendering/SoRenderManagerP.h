#ifndef OBOL_SORENDERMANAGERP_H
#define OBOL_SORENDERMANAGERP_H

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

#include "config.h"

#include <vector>

#ifdef OBOL_THREADSAFE
#include <Inventor/threads/SbMutex.h>
#endif // OBOL_THREADSAFE

#include <Inventor/system/gl.h>
#include <Inventor/SbColor4f.h>
#include <Inventor/SoRenderManager.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/elements/SoLazyElement.h>
#include <Inventor/sensors/SoNodeSensor.h>
#include <Inventor/misc/SoNotification.h>

class SbMatrix;
class SoNodeSensor;
class SoInfo;
class SoNode;
class SoGetBoundingBoxAction;
class SoGetMatrixAction;
class SoSearchAction;
class SbPList;

class SoRenderManagerP {
public:
  SoRenderManagerP(SoRenderManager * publ);
  ~SoRenderManagerP();

  void setClippingPlanes(void);
  static void updateClippingPlanesCB(void * closure, SoSensor * sensor);
  void getCameraCoordinateSystem(SbMatrix & matrix,
                                 SbMatrix & inverse);
  static void redrawshotTriggeredCB(void * data, SoSensor * sensor);
  static void cleanup(void);

  void lock(void) {
#ifdef OBOL_THREADSAFE
    this->mutex.lock();
#endif // OBOL_THREADSAFE
  }
  void unlock(void) {
#ifdef OBOL_THREADSAFE
    this->mutex.unlock();
#endif // OBOL_THREADSAFE
  }

  SoRenderManager * publ;
  SoNodeSensor * rootsensor;
  SoNode * scene;
  SoCamera * camera;
  float nearplanevalue;
  SbBool doublebuffer;
  SbBool isactive;
  float stereooffset;
  SoInfo * dummynode;
  uint32_t overlaycolor;
  SoColorPacker colorpacker;
  SbViewportRegion stereostencilmaskvp;
  GLubyte * stereostencilmask;
  SbColor4f backgroundcolor;
  int backgroundindex;
  SbBool texturesenabled;
  SbBool isrgbmode;
  uint32_t redrawpri;
  SoNodeSensor * clipsensor;

  SoGetBoundingBoxAction * getbboxaction;
  SoGetMatrixAction * getmatrixaction;
  SoGLRenderAction * glaction;
  SoSearchAction * searchaction;
  SbBool deleteglaction;

  SoRenderManager::StereoMode stereostenciltype;
  SoRenderManager::RenderMode rendermode;
  SoRenderManager::StereoMode stereomode;
  SoRenderManager::AutoClippingStrategy autoclipping;

  SoRenderManagerRenderCB * rendercb;
  void * rendercbdata;
  SoOneShotSensor * redrawshot;

  SbPList * superimpositions;

  void invokePreRenderCallbacks(void);
  void invokePostRenderCallbacks(void);
  typedef std::pair<SoRenderManagerRenderCB *, void *> RenderCBTouple;
  std::vector<RenderCBTouple> preRenderCallbacks;
  std::vector<RenderCBTouple> postRenderCallbacks;

  // "private" data
  static SbBool touchtimer;
  static SbBool cleanupfunctionset;

#ifdef OBOL_THREADSAFE
  SbMutex mutex;
#endif // OBOL_THREADSAFE
};

// *************************************************************************

// This class inherits SoNodeSensor and overrides its notify() method
// to provide a means of debugging notifications on the root node.
//
// Good for debugging cases when there are continuous redraws due to
// scene graph changes we have no clue as to the source of.
//
// A sensor of this class is only made if the below debugging envvar
// is set. Otherwise, and ordinary SoNodeSensor is used instead.

class SoRenderManagerRootSensor : public SoNodeSensor {
  typedef SoNodeSensor inherited;

public:
  SoRenderManagerRootSensor(SoSensorCB * f, void * d) : inherited(f, d) { }
  virtual ~SoRenderManagerRootSensor() { }

  virtual void notify(SoNotList * l);
  static SbBool debug(void);

private:
  static int debugrootnotifications;
};

// *************************************************************************


#endif // OBOL_SORENDERMANAGERP_H
