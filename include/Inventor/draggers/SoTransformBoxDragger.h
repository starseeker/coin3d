#ifndef COIN_SOTRANSFORMBOXDRAGGER_H
#define COIN_SOTRANSFORMBOXDRAGGER_H

// Minimal stub header for SoTransformBoxDragger
// TODO: Complete implementation

#include <Inventor/draggers/SoDragger.h>

class COIN_DLL_API SoTransformBoxDragger : public SoDragger {
  typedef SoDragger inherited;

  SO_KIT_HEADER(SoTransformBoxDragger);

public:
  static void initClass(void);
  SoTransformBoxDragger(void);

protected:
  virtual ~SoTransformBoxDragger();
};

#endif // !COIN_SOTRANSFORMBOXDRAGGER_H
