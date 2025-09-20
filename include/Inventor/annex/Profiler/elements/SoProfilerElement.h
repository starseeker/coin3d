#ifndef COIN_SOPROFILERELEMENT_H
#define COIN_SOPROFILERELEMENT_H

/**************************************************************************\
 * Stub implementation for SoProfilerElement - profiler functionality disabled
 **************************************************************************/

#include <Inventor/elements/SoElement.h>
#include <Inventor/annex/Profiler/SbProfilingData.h>

class COIN_DLL_API SoProfilerElement : public SoElement {
  typedef SoElement inherited;

public:
  static void initClass(void);
  static SoType getClassTypeId(void);
  static int getClassStackIndex(void);
  
  static SoProfilerElement * get(SoState * state);
  
  // Stub method for profiling data access
  SbProfilingData& getProfilingData() const;
  
  virtual void init(SoState * state);
  virtual void push(SoState * state);
  virtual void pop(SoState * state, const SoElement * prevTopElement);
  virtual SbBool matches(const SoElement * element) const;
  virtual SoElement * copyMatchInfo(void) const;

protected:
  virtual ~SoProfilerElement();

private:
  static SoType classTypeId;
  static int classStackIndex;
  
  SbProfilingData data;
};

#endif // !COIN_SOPROFILERELEMENT_H