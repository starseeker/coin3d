/**************************************************************************\
 * Minimal stub implementations for missing profiler symbols needed by nodekits
 **************************************************************************/

#include <Inventor/SoType.h>
#include <Inventor/misc/SoState.h>
#include <Inventor/annex/Profiler/SoProfiler.h>
#include <Inventor/annex/Profiler/SbProfilingData.h>
#include <Inventor/annex/Profiler/elements/SoProfilerElement.h>

// SoProfiler minimal implementation
void 
SoProfiler::init(void)
{
  // Minimal initialization
}

SbBool 
SoProfiler::isEnabled(void)
{
  return FALSE;
}

void 
SoProfiler::enable(SbBool enabled)
{
  // Stub - profiler cannot be enabled
}

SbBool 
SoProfiler::isOverlayActive(void)
{
  return FALSE;
}

SbBool 
SoProfiler::isConsoleActive(void)
{
  return FALSE;
}

// SoProfilerP namespace implementation
namespace SoProfilerP {

void parseCoinProfilerVariable(void) {
  // Stub implementation
}

SbBool shouldContinuousRender(void) {
  return FALSE;
}

float getContinuousRenderDelay(void) {
  return 0.0f;
}

SbBool shouldSyncGL(void) {
  return FALSE;
}

SbBool shouldClearConsole(void) {
  return FALSE;
}

SbBool shouldOutputHeaderOnConsole(void) {
  return FALSE;
}

void parseCoinProfilerOverlayVariable(void) {
  // Stub implementation
}

void setActionType(SoType actiontype) {
  // Stub implementation
}

SoType getActionType(void) {
  return SoType::badType();
}

void dumpToConsole(const SbProfilingData & data) {
  // Stub implementation
}

} // namespace SoProfilerP

// If SoProfilerElement is needed, provide minimal stubs for the needed methods
// without trying to properly define the element class

// Global profiling data for getProfilingData method
static SbProfilingData global_profiling_data;

// Dummy class for SoProfilerElement that provides the necessary interface
class SoProfilerElementStub {
public:
  SbProfilingData& getProfilingData() const {
    return global_profiling_data;
  }
};

static SoProfilerElementStub profiler_element_instance;

// Implementation of missing SoProfilerElement methods
// These need to be actual C++ symbol names, not extern "C"

void SoProfilerElement::initClass(void) {
  // Stub initialization
}

int SoProfilerElement::getClassStackIndex(void) {
  return -1; // Invalid stack index
}

SoProfilerElement* SoProfilerElement::get(SoState* state) {
  // Return a dummy element - cast the stub to SoProfilerElement*
  return reinterpret_cast<SoProfilerElement*>(&profiler_element_instance);
}

SbProfilingData& SoProfilerElement::getProfilingData(void) const {
  return global_profiling_data;
}