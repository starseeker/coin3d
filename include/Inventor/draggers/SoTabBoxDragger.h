#ifndef COIN_SOTABBOXDRAGGER_H
#define COIN_SOTABBOXDRAGGER_H

// Minimal stub header for SoTabBoxDragger
// TODO: Complete implementation

#include <Inventor/draggers/SoDragger.h>

class COIN_DLL_API SoTabBoxDragger : public SoDragger {
  typedef SoDragger inherited;

  SO_KIT_HEADER(SoTabBoxDragger);

public:
  static void initClass(void);
  SoTabBoxDragger(void);

protected:
  virtual ~SoTabBoxDragger();
};

#endif // !COIN_SOTABBOXDRAGGER_H
