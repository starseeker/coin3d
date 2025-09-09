#ifndef COIN_SOPROFILERELEMENT_H
#define COIN_SOPROFILERELEMENT_H

/**************************************************************************\
 * Stub implementation for SoProfilerElement - profiler functionality disabled
 **************************************************************************/

#include <Inventor/elements/SoElement.h>

class COIN_DLL_API SoProfilerElement : public SoElement {
  typedef SoElement inherited;

public:
  static void initClass(void);
  
  static SoProfilerElement * get(SoState * state);
  
  virtual void init(SoState * state);
  virtual void push(SoState * state);
  virtual void pop(SoState * state, const SoElement * prevTopElement);
  virtual SbBool matches(const SoElement * element) const;
  virtual SoElement * copyMatchInfo(void) const;

protected:
  virtual ~SoProfilerElement();

private:
  static int classStackIndex;
};

#endif // !COIN_SOPROFILERELEMENT_H