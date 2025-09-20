#ifndef COIN_SOCENTERBALLDRAGGER_H
#define COIN_SOCENTERBALLDRAGGER_H

// Minimal stub header for SoCenterballDragger
// TODO: Complete implementation

#include <Inventor/draggers/SoDragger.h>

class COIN_DLL_API SoCenterballDragger : public SoDragger {
  typedef SoDragger inherited;

  SO_KIT_HEADER(SoCenterballDragger);

public:
  static void initClass(void);
  SoCenterballDragger(void);

protected:
  virtual ~SoCenterballDragger();
};

#endif // !COIN_SOCENTERBALLDRAGGER_H