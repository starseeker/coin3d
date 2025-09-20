/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

// Stub implementations for SoProfilerP namespace functions

#include "SoProfilerP.h"
#include <Inventor/annex/Profiler/SbProfilingData.h>

namespace SoProfilerP {

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

void parseCoinProfilerVariable(void) {
  // Stub implementation
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