/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 **************************************************************************/

#include <Inventor/SoEventManager.h>

#include <cstddef>
#include <cassert>
#include <cstdio>

#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoInput.h>
#include <Inventor/errors/SoDebugError.h>
#include <Inventor/events/SoEvent.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/misc/SoState.h>

// SCXML support removed - navigation now uses direct C++ APIs

#include "SbBasicP.h"

/*!
  \class SoEventManager SoEventManager.h Inventor/SoEventManager.h
  \brief The SoEventManager class provides event handling for a Coin3D viewer.

  The SoEventManager class provides a convenient event handler for Coin3D
  scenes. It can be used to do picking, camera manipulation, object
  manipulation etc.

  The event manager should be activated by calling the processEvent() method
  for user events. It may be used either directly or as a part of a viewer
  component. The SoQt, SoWin and SoXt viewers all use this class internally.

  \ingroup coin_general
*/

/*!
  \enum SoEventManager::NavigationState

  The NavigationState enum describes the different navigation modes.
*/

/*!
  \var SoEventManager::NavigationState SoEventManager::NO_NAVIGATION

  No viewer navigation will occur. Note that the current state of any
  active state machines will be maintained, just not activated.
*/

/*!
  \var SoEventManager::NavigationState SoEventManager::MIXED_NAVIGATION

  Event handling and navigation is done by both Coin's navigation
  system and the state machines.
*/

class SoEventManager::PImpl {
public:
  PImpl(void) {
    this->handleventaction = NULL;
    this->scene = NULL;
    this->camera = NULL;
    this->viewport = SbViewportRegion();
    this->navigationstate = SoEventManager::MIXED_NAVIGATION;
    // DEPRECATED: SCXML state machines removed
  }

  ~PImpl() {
    delete this->handleventaction;
  }

public:
  SoHandleEventAction * handleventaction;
  SoNode * scene;
  SoCamera * camera;
  SbViewportRegion viewport;
  SoEventManager::NavigationState navigationstate;
  // DEPRECATED: std::vector<SoScXMLStateMachine *> statemachines; // SCXML removed
};

#define PRIVATE(p) (p->pimpl)

/*!
  Constructor.
*/
SoEventManager::SoEventManager(void)
{
  PRIVATE(this)->handleventaction = new SoHandleEventAction(SbViewportRegion());
}

/*!
  Destructor.
*/
SoEventManager::~SoEventManager()
{
  // DEPRECATED: SCXML state machine cleanup removed
  // PImpl automatically cleaned up by SbPimplPtr
}

/*!
  Set scene graph.
*/
void
SoEventManager::setSceneGraph(SoNode * const sceneroot)
{
  PRIVATE(this)->scene = sceneroot;
  // DEPRECATED: SCXML state machine scene setting removed
}

/*!
  Get scene graph.
*/
SoNode *
SoEventManager::getSceneGraph(void) const
{
  return PRIVATE(this)->scene;
}

/*!
  Set camera.
*/
void
SoEventManager::setCamera(SoCamera * camera)
{
  PRIVATE(this)->camera = camera;
  // DEPRECATED: SCXML state machine camera setting removed
}

/*!
  Get camera.
*/
SoCamera *
SoEventManager::getCamera(void) const
{
  return PRIVATE(this)->camera;
}

/*!
  Set viewport.
*/
void
SoEventManager::setViewportRegion(const SbViewportRegion & newregion)
{
  PRIVATE(this)->viewport = newregion;
  PRIVATE(this)->handleventaction->setViewportRegion(newregion);
  // DEPRECATED: SCXML state machine viewport setting removed
}

/*!
  Get viewport.
*/
const SbViewportRegion &
SoEventManager::getViewportRegion(void) const
{
  return PRIVATE(this)->viewport;
}

/*!
  Set handle event action.
*/
void
SoEventManager::setHandleEventAction(SoHandleEventAction * hea)
{
  delete PRIVATE(this)->handleventaction;
  PRIVATE(this)->handleventaction = hea;
}

/*!
  Get handle event action.
*/
SoHandleEventAction *
SoEventManager::getHandleEventAction(void) const
{
  return PRIVATE(this)->handleventaction;
}

/*!
  Set navigation state.
*/
void
SoEventManager::setNavigationState(NavigationState state)
{
  PRIVATE(this)->navigationstate = state;
}

/*!
  Get navigation state.
*/
SoEventManager::NavigationState
SoEventManager::getNavigationState(void) const
{
  return PRIVATE(this)->navigationstate;
}

/*!
  Process the given event. Returns TRUE if the event was handled.
*/
SbBool
SoEventManager::processEvent(const SoEvent * event)
{
  SbBool status = FALSE;

  assert(event && "passed a NULL event");

  if (!PRIVATE(this)->scene) { return FALSE; }

  switch (PRIVATE(this)->navigationstate) {
  case SoEventManager::NO_NAVIGATION:
    break;

  case SoEventManager::MIXED_NAVIGATION:
    PRIVATE(this)->handleventaction->setEvent(event);
    PRIVATE(this)->handleventaction->apply(PRIVATE(this)->scene);
    status = PRIVATE(this)->handleventaction->isHandled();
    // DEPRECATED: SCXML state machine processing removed
    break;

  default:
    assert(FALSE && "unknown navigation type");
    break;
  }

  return status;
}

/*!
  Sets the size of the viewport region.
*/
void
SoEventManager::setSize(const SbVec2s & newsize)
{
  if (PRIVATE(this)->handleventaction) {
    SbViewportRegion vp = PRIVATE(this)->handleventaction->getViewportRegion();
    vp.setViewportPixels(vp.getViewportOriginPixels(), newsize);
    PRIVATE(this)->handleventaction->setViewportRegion(vp);
  }
}

/*!
  Sets the origin of the viewport region.
*/
void
SoEventManager::setOrigin(const SbVec2s & newOrigin)
{
  if (PRIVATE(this)->handleventaction) {
    SbViewportRegion vp = PRIVATE(this)->handleventaction->getViewportRegion();
    vp.setViewportPixels(newOrigin, vp.getViewportSizePixels());
    PRIVATE(this)->handleventaction->setViewportRegion(vp);
  }
}

/*!
  Process an event. This method is called internally by processEvent().
*/
SbBool
SoEventManager::actuallyProcessEvent(const SoEvent * const event)
{
  return this->processEvent(event);
}

// DEPRECATED: All SCXML state machine methods removed
// The following methods are no longer available:
// - getNumSoScXMLStateMachines()
// - getSoScXMLStateMachine()
// - addSoScXMLStateMachine()
// - removeSoScXMLStateMachine()
// Use direct C++ navigation APIs instead.

#undef PRIVATE
