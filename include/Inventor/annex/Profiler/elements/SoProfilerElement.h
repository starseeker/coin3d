#ifndef COIN_SOPROFILERELEMENT_H
#define COIN_SOPROFILERELEMENT_H

/**************************************************************************\
 * Full implementation for SoProfilerElement - scene graph profiling
 **************************************************************************/

#include <Inventor/elements/SoSubElement.h>
#include <Inventor/annex/Profiler/SbProfilingData.h>

class COIN_DLL_API SoProfilerElement : public SoElement {
  typedef SoElement inherited;

  SO_ELEMENT_HEADER(SoProfilerElement);

public:
  static void initClass(void);
  
  static SoProfilerElement * get(SoState * state);
  
  // Profiling data access
  SbProfilingData & getProfilingData(void);
  const SbProfilingData & getProfilingData(void) const;
  
  virtual void push(SoState * state);
  virtual void pop(SoState * state, const SoElement * prevTopElement);
  virtual SbBool matches(const SoElement * element) const;
  virtual SoElement * copyMatchInfo(void) const;

protected:
  virtual ~SoProfilerElement();

private:
  SbProfilingData data;
};

#endif // !COIN_SOPROFILERELEMENT_H