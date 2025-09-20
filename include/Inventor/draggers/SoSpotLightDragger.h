#ifndef COIN_SOSPOTLIGHTDRAGGER_H
#define COIN_SOSPOTLIGHTDRAGGER_H

// Minimal stub header for SoSpotLightDragger
// TODO: Complete implementation

#include <Inventor/draggers/SoDragger.h>
#include <Inventor/fields/SoSFFloat.h>

class COIN_DLL_API SoSpotLightDragger : public SoDragger {
  typedef SoDragger inherited;

  SO_KIT_HEADER(SoSpotLightDragger);

public:
  SoSFFloat angle;

  static void initClass(void);
  SoSpotLightDragger(void);

protected:
  virtual ~SoSpotLightDragger();
};

#endif // !COIN_SOSPOTLIGHTDRAGGER_H
