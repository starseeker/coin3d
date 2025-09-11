/**************************************************************************\
 * Stub implementation for profiler functionality - NodeKit elimination
 **************************************************************************/

#include <Inventor/annex/Profiler/SoProfiler.h>
#include <Inventor/annex/Profiler/elements/SoProfilerElement.h>
#include <Inventor/annex/Profiler/nodes/SoProfilerStats.h>
#include <Inventor/nodes/SoSubNode.h>
#include <Inventor/elements/SoSubElement.h>

// SoProfiler stub implementation
void 
SoProfiler::init(void)
{
  // Stub - profiler disabled
}

SbBool 
SoProfiler::isEnabled(void)
{
  return FALSE; // Always disabled
}

void 
SoProfiler::enable(SbBool enabled)
{
  // Stub - profiler cannot be enabled
}

SbBool 
SoProfiler::isOverlayActive(void)
{
  return FALSE; // Always disabled
}

SbBool 
SoProfiler::isConsoleActive(void)
{
  return FALSE; // Always disabled
}

// SoProfilerP stub implementation
void 
SoProfilerP::parseCoinProfilerVariable(void)
{
  // Stub - no environment variables to parse
}

SbBool 
SoProfilerP::shouldContinuousRender(void)
{
  return FALSE; // Never need continuous render
}

float 
SoProfilerP::getContinuousRenderDelay(void)
{
  return 0.0f; // No delay needed
}

// SoProfilerElement stub implementation 
SoType SoProfilerElement::classTypeId;
int SoProfilerElement::classStackIndex = -1;

void 
SoProfilerElement::initClass(void)
{
  // Simple stub initialization 
  classStackIndex = 999; // Arbitrary high number to avoid conflicts
  classTypeId = SoType::badType(); // Invalid type since this is a stub
}

SoType 
SoProfilerElement::getClassTypeId(void) 
{
  return classTypeId;
}

int 
SoProfilerElement::getClassStackIndex(void) 
{
  return classStackIndex;
}

SoProfilerElement * 
SoProfilerElement::get(SoState * state)
{
  return (SoProfilerElement *) getConstElement(state, classStackIndex);
}

void 
SoProfilerElement::init(SoState * state)
{
  inherited::init(state);
}

void 
SoProfilerElement::push(SoState * state)
{
  inherited::push(state);
}

void 
SoProfilerElement::pop(SoState * state, const SoElement * prevTopElement)
{
  inherited::pop(state, prevTopElement);
}

SbBool 
SoProfilerElement::matches(const SoElement * element) const
{
  return TRUE; // Stub always matches
}

SoElement * 
SoProfilerElement::copyMatchInfo(void) const
{
  return new SoProfilerElement;
}

SoProfilerElement::~SoProfilerElement()
{
}

// SoProfilerStats stub implementation
SO_NODE_SOURCE(SoProfilerStats);

void 
SoProfilerStats::initClass(void)
{
  SO_NODE_INIT_CLASS(SoProfilerStats, SoNode, "Node");
}

SoProfilerStats::SoProfilerStats(void)
{
  SO_NODE_CONSTRUCTOR(SoProfilerStats);
}

SoProfilerStats::~SoProfilerStats()
{
}