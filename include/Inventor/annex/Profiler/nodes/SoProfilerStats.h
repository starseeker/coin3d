#ifndef COIN_SOPROFILERSTATS_H
#define COIN_SOPROFILERSTATS_H

/**************************************************************************\
 * SoProfilerStats - profiler statistics node
 **************************************************************************/

#include <Inventor/nodes/SoSubNode.h>
#include <Inventor/fields/SoSFInt32.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoMFString.h>
#include <Inventor/fields/SoMFInt32.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoSFTrigger.h>

class COIN_DLL_API SoProfilerStats : public SoNode {
  typedef SoNode inherited;
  SO_NODE_HEADER(SoProfilerStats);

public:
  static void initClass(void);
  SoProfilerStats(void);

  // Profiler output fields
  SoSFInt32 profiledAction;
  SoSFFloat profiledActionTime;
  SoMFString renderedNodeType;
  SoMFInt32 renderedNodeTypeCount;
  SoMFFloat renderingTimePerNodeType;
  SoMFFloat renderingTimeMaxPerNodeType;
  SoSFTrigger profilingUpdate;

protected:
  virtual ~SoProfilerStats();
};

#endif // !COIN_SOPROFILERSTATS_H