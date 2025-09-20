#ifndef COIN_SOPROFILERTOPKIT_H
#define COIN_SOPROFILERTOPKIT_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * SoProfilerTopKit - top-level profiler visualization kit
 **************************************************************************/

#include <Inventor/nodekits/SoSubKit.h>
#include <Inventor/nodekits/SoBaseKit.h>
#include <Inventor/fields/SoSFInt32.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/fields/SoSFColor.h>

class SoProfilerTopKitP;

class COIN_DLL_API SoProfilerTopKit : public SoBaseKit {
  typedef SoBaseKit inherited;

  SO_KIT_HEADER(SoProfilerTopKit);

  SO_KIT_CATALOG_ENTRY_HEADER(profilingStats);
  SO_KIT_CATALOG_ENTRY_HEADER(topLevelSeparator);
  SO_KIT_CATALOG_ENTRY_HEADER(textSeparator);

public:
  static void initClass(void);
  SoProfilerTopKit(void);

  SoSFInt32 lines;
  SoSFVec3f position;
  SoSFColor txtColor;  // Text color field

protected:
  virtual ~SoProfilerTopKit();

public:  // For PRIVATE macro access
  SoProfilerTopKitP * pimpl;
};

#endif // !COIN_SOPROFILERTOPKIT_H