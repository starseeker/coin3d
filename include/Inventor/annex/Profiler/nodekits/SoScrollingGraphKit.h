#ifndef COIN_SOSCROLLINGGRAPHKIT_H
#define COIN_SOSCROLLINGGRAPHKIT_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * SoScrollingGraphKit - scrolling graph visualization kit
 **************************************************************************/

#include <Inventor/nodekits/SoSubKit.h>
#include <Inventor/nodekits/SoBaseKit.h>
#include <Inventor/fields/SoSFEnum.h>
#include <Inventor/fields/SoSFTime.h>
#include <Inventor/fields/SoMFColor.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/fields/SoMFName.h>
#include <Inventor/fields/SoMFFloat.h>

class SoSensor;

class SoScrollingGraphKitP;

class COIN_DLL_API SoScrollingGraphKit : public SoBaseKit {
  typedef SoBaseKit inherited;

  SO_KIT_HEADER(SoScrollingGraphKit);

  SO_KIT_CATALOG_ENTRY_HEADER(scene);

public:
  static void initClass(void);
  SoScrollingGraphKit(void);

  enum GraphicsType {
    LINES,
    STACKED_BARS,
    DEFAULT_GRAPHICS = LINES
  };

  enum RangeType {
    ABSOLUTE_ACCUMULATIVE,
    DEFAULT_RANGETYPE = ABSOLUTE_ACCUMULATIVE
  };

  SoSFEnum graphicsType;
  SoSFEnum rangeType;
  SoSFTime seconds;
  SoMFColor colors;
  SoSFVec3f viewportSize;
  SoSFVec3f position;
  SoSFVec3f size;
  SoMFName addKeys;  // Changed from SoSFName to SoMFName
  SoMFFloat addValues;

  // Callback functions
  static void addValuesCB(void * closure, SoSensor * sensor);

protected:
  virtual ~SoScrollingGraphKit();

private:
  SoScrollingGraphKitP * pimpl;
};

#endif // !COIN_SOSCROLLINGGRAPHKIT_H