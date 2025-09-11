#ifndef COIN_SOPROFILER_H
#define COIN_SOPROFILER_H

/**************************************************************************\
 * Stub implementation for SoProfiler - profiler functionality disabled
 **************************************************************************/

#include <Inventor/C/basic.h>

class COIN_DLL_API SoProfiler {
public:
  static void init(void);
  static SbBool isEnabled(void);
  static void enable(SbBool enabled);
  static SbBool isOverlayActive(void);
  static SbBool isConsoleActive(void);

private:
  SoProfiler(void);
  ~SoProfiler();
};

// Stub implementation of internal profiler functions
namespace SoProfilerP {
  void parseCoinProfilerVariable(void);
  SbBool shouldContinuousRender(void);
  float getContinuousRenderDelay(void);
}

#endif // !COIN_SOPROFILER_H