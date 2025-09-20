#ifndef COIN_SOPOINTLIGHTDRAGGER_H
#define COIN_SOPOINTLIGHTDRAGGER_H

// Minimal stub header for SoPointLightDragger
// TODO: Complete implementation

#include <Inventor/draggers/SoDragger.h>

class COIN_DLL_API SoPointLightDragger : public SoDragger {
  typedef SoDragger inherited;

  SO_KIT_HEADER(SoPointLightDragger);

public:
  static void initClass(void);
  SoPointLightDragger(void);

protected:
  virtual ~SoPointLightDragger();
};

#endif // !COIN_SOPOINTLIGHTDRAGGER_H
