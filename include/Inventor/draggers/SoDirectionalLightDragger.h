#ifndef COIN_SODIRECTIONALLIGHTDRAGGER_H
#define COIN_SODIRECTIONALLIGHTDRAGGER_H

// Minimal stub header for SoDirectionalLightDragger
// TODO: Complete implementation

#include <Inventor/draggers/SoDragger.h>

class COIN_DLL_API SoDirectionalLightDragger : public SoDragger {
  typedef SoDragger inherited;

  SO_KIT_HEADER(SoDirectionalLightDragger);

public:
  static void initClass(void);
  SoDirectionalLightDragger(void);

protected:
  virtual ~SoDirectionalLightDragger();
};

#endif // !COIN_SODIRECTIONALLIGHTDRAGGER_H
