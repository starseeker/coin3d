#ifndef COIN_SOJACKDRAGGER_H
#define COIN_SOJACKDRAGGER_H

// Minimal stub header for SoJackDragger
// TODO: Complete implementation

#include <Inventor/draggers/SoDragger.h>

class COIN_DLL_API SoJackDragger : public SoDragger {
  typedef SoDragger inherited;

  SO_KIT_HEADER(SoJackDragger);

public:
  static void initClass(void);
  SoJackDragger(void);

protected:
  virtual ~SoJackDragger();
};

#endif // !COIN_SOJACKDRAGGER_H