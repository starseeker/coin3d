#ifndef COIN_SOPROFILERTOPENGINE_H
#define COIN_SOPROFILERTOPENGINE_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * SoProfilerTopEngine - profiler top-level processing engine
 **************************************************************************/

#include <Inventor/engines/SoSubEngine.h>
#include <Inventor/fields/SoMFString.h>
#include <Inventor/fields/SoMFInt32.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFInt32.h>

class SoEngineOutput;
class SoProfilerTopEngineP;

class COIN_DLL_API SoProfilerTopEngine : public SoEngine {
  typedef SoEngine inherited;

  SO_ENGINE_HEADER(SoProfilerTopEngine);

public:
  static void initClass(void);
  SoProfilerTopEngine(void);

  // Input fields
  SoMFString statisticsNames;
  SoMFInt32 statisticsCounts;
  SoMFFloat statisticsTimings;
  SoMFFloat statisticsTimingsMax;
  SoSFInt32 maxLines;
  SoSFFloat decay;

  // Output fields
  SoEngineOutput * prettyText;  // SoMFString
  SoEngineOutput * topList;     // SoMFString

protected:
  virtual ~SoProfilerTopEngine();

  virtual void evaluate(void);

private:
  SoProfilerTopEngineP * pimpl;
};

#endif // !COIN_SOPROFILERTOPENGINE_H