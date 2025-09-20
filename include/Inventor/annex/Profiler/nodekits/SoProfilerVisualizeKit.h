#ifndef COIN_SOPROFILERRVISUALIZEKIT_H
#define COIN_SOPROFILERRVISUALIZEKIT_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * SoProfilerVisualizeKit - profiler visualization kit
 **************************************************************************/

#include <Inventor/nodekits/SoSubKit.h>
#include <Inventor/nodekits/SoBaseKit.h>
#include <Inventor/fields/SoSFNode.h>
#include <Inventor/fields/SoSFTrigger.h>
#include <Inventor/fields/SoMFNode.h>

class SoProfilerVisualizeKitP;

class COIN_DLL_API SoProfilerVisualizeKit : public SoBaseKit {
  typedef SoBaseKit inherited;

  SO_KIT_HEADER(SoProfilerVisualizeKit);

  SO_KIT_CATALOG_ENTRY_HEADER(top);
  SO_KIT_CATALOG_ENTRY_HEADER(pretree);
  SO_KIT_CATALOG_ENTRY_HEADER(visualtree);

public:
  static void initClass(void);
  SoProfilerVisualizeKit(void);

  SoSFNode stats;
  SoSFTrigger statsTrigger;
  SoSFNode root;
  SoMFNode separatorsWithGLCaches;

protected:
  virtual ~SoProfilerVisualizeKit();

private:
  SoProfilerVisualizeKitP * pimpl;
};

#endif // !COIN_SOPROFILERRVISUALIZEKIT_H