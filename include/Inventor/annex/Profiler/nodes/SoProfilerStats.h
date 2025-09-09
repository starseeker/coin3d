#ifndef COIN_SOPROFILERSTATS_H
#define COIN_SOPROFILERSTATS_H

/**************************************************************************\
 * Stub implementation for SoProfilerStats - profiler functionality disabled
 **************************************************************************/

#include <Inventor/nodes/SoSubNode.h>

class COIN_DLL_API SoProfilerStats : public SoNode {
  typedef SoNode inherited;
  SO_NODE_HEADER(SoProfilerStats);

public:
  static void initClass(void);
  SoProfilerStats(void);

protected:
  virtual ~SoProfilerStats();
};

#endif // !COIN_SOPROFILERSTATS_H