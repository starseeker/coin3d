#ifndef COIN_SOTRANSFORMERDRAGGER_H
#define COIN_SOTRANSFORMERDRAGGER_H

// Minimal stub header for SoTransformerDragger
// TODO: Complete implementation

#include <Inventor/draggers/SoDragger.h>

class COIN_DLL_API SoTransformerDragger : public SoDragger {
  typedef SoDragger inherited;

  SO_KIT_HEADER(SoTransformerDragger);

public:
  static void initClass(void);
  SoTransformerDragger(void);

protected:
  virtual ~SoTransformerDragger();
};

#endif // !COIN_SOTRANSFORMERDRAGGER_H