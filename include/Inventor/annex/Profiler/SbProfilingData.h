#ifndef COIN_SBPROFILINGDATA_H
#define COIN_SBPROFILINGDATA_H

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

#include <Inventor/SbBasic.h>
#include <Inventor/SbTime.h>
#include <cstddef>

class SoFullPath;
class SoPath;

class SbProfilingData {
public:
  SbProfilingData() {}
  ~SbProfilingData() {}
  
  // Memory size constants
  enum { MEMORY_SIZE, VIDEO_MEMORY_SIZE, GL_CACHED_FLAG };
  
  // Methods used by SoNodeProfiling.h
  int getParentIndex(int index) const { return -1; }
  void preOffsetNodeTiming(int index, const SbTime& offset) {}
  SbTime getNodeTiming(int index) const { return SbTime::zero(); }
  void setNodeTiming(int index, const SbTime& time) {}
  
  // Additional methods needed
  int getIndex(const SoPath* path, SbBool create) { return 0; }
  void setNodeFootprint(int index, int type, int value) {}
  size_t getNodeFootprint(int index, int type) { return 0; }
  void setNodeFlag(const SoPath* path, int flag, SbBool value) {}
  void setActionStartTime(const SbTime& time) {}
  SbTime getActionStartTime() const { return SbTime::zero(); }
};

#endif // !COIN_SBPROFILINGDATA_H