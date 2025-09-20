#ifndef COIN_SOHANDLEBOXDRAGGER_H
#define COIN_SOHANDLEBOXDRAGGER_H

// Minimal stub header for SoHandleBoxDragger  
// TODO: Complete implementation

#include <Inventor/draggers/SoDragger.h>

class COIN_DLL_API SoHandleBoxDragger : public SoDragger {
  typedef SoDragger inherited;

  SO_KIT_HEADER(SoHandleBoxDragger);

public:
  static void initClass(void);
  SoHandleBoxDragger(void);

protected:
  virtual ~SoHandleBoxDragger();
};

#endif // !COIN_SOHANDLEBOXDRAGGER_H