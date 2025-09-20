#ifndef COIN_SODRAGGER_H
#define COIN_SODRAGGER_H

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

#include <Inventor/nodekits/SoInteractionKit.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/lists/SoCallbackList.h>
#include <Inventor/misc/SoState.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbName.h>
#include <Inventor/SbRotation.h>

class SoEvent;
class SoHandleEventAction;
class SoPath;
class SoPickedPoint;
class SoDragger;

typedef void SoDraggerCB(void *userData, SoDragger *dragger);

class COIN_DLL_API SoDragger : public SoInteractionKit {
  typedef SoInteractionKit inherited;

  SO_KIT_HEADER(SoDragger);

  SO_KIT_CATALOG_ENTRY_HEADER(motionMatrix);

public:
  SoSFBool isActive;

  enum ProjectorFrontSetting {
    FRONT, BACK, USE_PICK
  };

public:
  SoDragger(void);
  static void initClass(void);
  static void initClasses(void);

  virtual void handleEvent(SoHandleEventAction *action);
  virtual void callback(SoCallbackAction *action);
  virtual void GLRender(SoGLRenderAction *action);
  virtual void rayPick(SoRayPickAction *action);
  virtual void search(SoSearchAction *action);
  virtual void write(SoWriteAction *action);
  virtual void getPrimitiveCount(SoGetPrimitiveCountAction *action);
  virtual void getBoundingBox(SoGetBoundingBoxAction *action);
  virtual void getMatrix(SoGetMatrixAction *action);

  void addStartCallback(SoDraggerCB *func, void *data = NULL);
  void removeStartCallback(SoDraggerCB *func, void *data = NULL);
  void addMotionCallback(SoDraggerCB *func, void *data = NULL);
  void removeMotionCallback(SoDraggerCB *func, void *data = NULL);
  void addFinishCallback(SoDraggerCB *func, void *data = NULL);
  void removeFinishCallback(SoDraggerCB *func, void *data = NULL);
  void addValueChangedCallback(SoDraggerCB *func, void *data = NULL);
  void removeValueChangedCallback(SoDraggerCB *func, void *data = NULL);
  void addOtherEventCallback(SoDraggerCB *func, void *data = NULL);
  void removeOtherEventCallback(SoDraggerCB *func, void *data = NULL);

  void setMinGesture(int pixels);
  int getMinGesture(void) const;
  SbBool enableValueChangedCallbacks(SbBool newval);

  const SbMatrix &getMotionMatrix(void);
  void setMotionMatrix(const SbMatrix &matrix);

  void setViewportRegion(const SbViewportRegion &newRegion);
  const SbViewportRegion &getViewportRegion(void);

  void setViewVolume(const SbViewVolume &vol);
  const SbViewVolume &getViewVolume(void);

  void setPickPath(const SoPath *newPath);
  const SoPath *getPickPath(void) const;

  void setTempPathToThis(const SoPath *somethingClose);

  void registerChildDragger(SoDragger *child);
  void unregisterChildDragger(SoDragger *child);
  void registerChildDraggerMovingIndependently(SoDragger *child);
  void unregisterChildDraggerMovingIndependently(SoDragger *child);

  static void setMinScale(float newMinScale);
  static float getMinScale(void);

  SoDragger *getActiveChildDragger(void) const;

  SbMatrix getLocalToWorldMatrix(void);
  SbMatrix getWorldToLocalMatrix(void);

  void setProjectorEpsilon(float epsilon);
  float getProjectorEpsilon(void) const;

  void setIgnoreInBbox(SbBool val);
  SbBool getIgnoreInBbox(void) const;

  SbVec3f getLocalStartingPoint(void);
  SbVec3f getWorldStartingPoint(void);
  void getPartToLocalMatrix(const SbName &partname, SbMatrix &parttolocal, SbMatrix &localtopart);
  void transformMatrixLocalToWorld(const SbMatrix &frommatrix, SbMatrix &tomatrix);
  void transformMatrixWorldToLocal(const SbMatrix &frommatrix, SbMatrix &tomatrix);
  void transformMatrixToLocalSpace(const SbMatrix &frommatrix, SbMatrix &tomatrix, const SbName &fromspacepartname);
  void valueChanged(void);
  const SbMatrix &getStartMotionMatrix(void);
  const SoEvent *getEvent(void) const;
  SoPath *createPathToThis(void);
  const SoPath *getSurrogatePartPickedOwner(void) const;
  const SbName &getSurrogatePartPickedName(void) const;
  const SoPath *getSurrogatePartPickedPath(void) const;
  void setStartingPoint(const SoPickedPoint *newPoint);
  void setStartingPoint(const SbVec3f &newPoint);
  SoHandleEventAction *getHandleEventAction(void) const;
  void setHandleEventAction(SoHandleEventAction *action);

  void workFieldsIntoTransform(SbMatrix &matrix);
  void setFrontOnProjector(ProjectorFrontSetting setting);
  ProjectorFrontSetting getFrontOnProjector(void) const;

  void workValuesIntoTransform(SbMatrix &matrix, const SbVec3f *translation, const SbRotation *rotation, const SbVec3f *scaleFactor, const SbRotation *scaleOrientation, const SbVec3f *center);
  void getTransformFast(SbMatrix &matrix, SbVec3f &translation, SbRotation &rotation, SbVec3f &scaleFactor, SbRotation &scaleOrientation, const SbVec3f &center);
  void getTransformFast(SbMatrix &matrix, SbVec3f &translation, SbRotation &rotation, SbVec3f &scaleFactor, SbRotation &scaleOrientation);
  
  SbMatrix appendTranslation(const SbMatrix &matrix, const SbVec3f &translation, const SbMatrix *conversion = NULL);
  SbMatrix appendScale(const SbMatrix &matrix, const SbVec3f &scale, const SbVec3f &scalecenter, const SbMatrix *conversion = NULL);
  SbMatrix appendRotation(const SbMatrix &matrix, const SbRotation &rot, const SbVec3f &rotcenter, const SbMatrix *conversion = NULL);

protected:
  virtual ~SoDragger();

  virtual void saveStartParameters(void);
  virtual SbBool shouldGrabBasedOnFlags(SoHandleEventAction *action);
  virtual void transferMotion(SoDragger *child);

  void grabEventsSetup(void);
  void grabEventsCleanup(void);

  static float minscale;

private:
  void updateElements(SoState *state);
  
  static void childStartCB(void *data, SoDragger *child);
  static void childMotionCB(void *data, SoDragger *child);
  static void childFinishCB(void *data, SoDragger *child);
  static void childOtherEventCB(void *data, SoDragger *child);
  static void childTransferMotionAndValueChangedCB(void *data, SoDragger *child);
  static void childValueChangedCB(void *data, SoDragger *child);

  class SoDraggerP *pimpl;
  SoDragger(const SoDragger &rhs); // N/A
  SoDragger &operator = (const SoDragger &rhs); // N/A
};

#endif // !COIN_SODRAGGER_H