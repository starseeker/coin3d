#ifndef COIN_SODRAGPOINTDRAGGER_H
#define COIN_SODRAGPOINTDRAGGER_H

// Minimal stub header for SoDragPointDragger
// TODO: Complete implementation

#include <Inventor/draggers/SoDragger.h>

class COIN_DLL_API SoDragPointDragger : public SoDragger {
  typedef SoDragger inherited;

  SO_KIT_HEADER(SoDragPointDragger);

public:
  static void initClass(void);
  SoDragPointDragger(void);

protected:
  virtual ~SoDragPointDragger();
};

#endif // !COIN_SODRAGPOINTDRAGGER_H