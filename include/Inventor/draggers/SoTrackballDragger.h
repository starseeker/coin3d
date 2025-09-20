#ifndef COIN_SOTRACKBALLDRAGGER_H
#define COIN_SOTRACKBALLDRAGGER_H

// Minimal stub header for SoTrackballDragger
// TODO: Complete implementation

#include <Inventor/draggers/SoDragger.h>

class COIN_DLL_API SoTrackballDragger : public SoDragger {
  typedef SoDragger inherited;

  SO_KIT_HEADER(SoTrackballDragger);

public:
  static void initClass(void);
  SoTrackballDragger(void);

protected:
  virtual ~SoTrackballDragger();
};

#endif // !COIN_SOTRACKBALLDRAGGER_H