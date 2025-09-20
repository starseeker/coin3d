#ifndef COIN_SONODEVISUALIZE_H
#define COIN_SONODEVISUALIZE_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * SoNodeVisualize - node visualization kit for profiler
 **************************************************************************/

#include <Inventor/nodekits/SoSubKit.h>
#include <Inventor/nodekits/SoBaseKit.h>

class SoNode;

class COIN_DLL_API SoNodeVisualize : public SoBaseKit {
  typedef SoBaseKit inherited;

  SO_KIT_HEADER(SoNodeVisualize);

  SO_KIT_CATALOG_ENTRY_HEADER(texture);
  SO_KIT_CATALOG_ENTRY_HEADER(cube);
  SO_KIT_CATALOG_ENTRY_HEADER(childGeometry);

public:
  static void initClass(void);
  SoNodeVisualize(void);

  SoNodeVisualize * visualize(SoNode * node);
  void reset(void);
  bool update(void);

protected:
  virtual ~SoNodeVisualize();

private:
  SoNode * node;
  SoNodeVisualize * parent;
  bool dirty;
};

#endif // !COIN_SONODEVISUALIZE_H