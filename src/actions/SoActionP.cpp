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


// *************************************************************************

#include "actions/SoActionP.h"

#include <Inventor/annex/Profiler/SoProfiler.h>
#ifdef HAVE_NODEKITS
#include <Inventor/annex/Profiler/nodekits/SoProfilerVisualizeKit.h>
#include <Inventor/annex/Profiler/nodekits/SoProfilerTopKit.h>
#endif // HAVE_NODEKITS

// *************************************************************************

namespace {

struct ProfilerThreadState {
  SoProfilerStats * node = NULL;
  SoNode * nodekit = NULL;

  ~ProfilerThreadState() { this->release(); }

  void release(void) {
    // The overlay owns references into the stats node, so release it first.
    if (this->nodekit) {
      this->nodekit->unref();
      this->nodekit = NULL;
    }
    if (this->node) {
      this->node->unref();
      this->node = NULL;
    }
  }
};

ProfilerThreadState & profilerThreadState(void)
{
  static thread_local ProfilerThreadState state;
  return state;
}

} // namespace

// *************************************************************************

SoProfilerStats *
SoActionP::getProfilerStatsNode(void)
{
  ProfilerThreadState & state = profilerThreadState();
  if (!state.node) {
    state.node = new SoProfilerStats;
    state.node->ref();
  }
  return state.node;
}

SoNode *
SoActionP::getProfilerOverlay(void)
{
  if (!SoProfiler::isEnabled() || !SoProfiler::isOverlayActive())
    return NULL;

  ProfilerThreadState & state = profilerThreadState();
#ifdef HAVE_NODEKITS
  if (!state.nodekit) {
    SoProfilerTopKit * kit = new SoProfilerTopKit;
    kit->ref();
    kit->setPart("profilingStats", SoActionP::getProfilerStatsNode());
    state.nodekit = kit;

    SoProfilerVisualizeKit * viskit = new SoProfilerVisualizeKit;
    viskit->stats.setValue(SoActionP::getProfilerStatsNode());
    kit->addOverlayGeometry(viskit);
  }
#endif // HAVE_NODEKITS
  return state.nodekit;
}

void
SoActionP::cleanupProfilerResources(void)
{
  profilerThreadState().release();
}

// *************************************************************************

#undef PRIVATE
