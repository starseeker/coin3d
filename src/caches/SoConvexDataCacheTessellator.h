#ifndef COIN_SOCONVEXDATACACHE_TESSELLATOR_H
#define COIN_SOCONVEXDATACACHE_TESSELLATOR_H

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

/*!
  \file SoConvexDataCacheTessellator.h
  \brief Tessellator class specifically for SoConvexDataCache
  
  This class replaces the dependency on SbGLUTessellator for SoConvexDataCache.
  It provides the same interface but uses a built-in tessellation implementation
  instead of requiring external GLU libraries.
*/

#ifndef COIN_INTERNAL
#error this is a private header file
#endif /* ! COIN_INTERNAL */

#include <Inventor/SbVec3f.h>
#include <Inventor/lists/SbList.h>
#include "SoConvexDataCacheGLU.h"

/*!
  \class SoConvexDataCacheTessellator
  \brief Tessellator for SoConvexDataCache
  
  This class provides tessellation functionality specifically designed for
  SoConvexDataCache. It uses a simplified tessellation algorithm that works
  well for the types of polygons typically processed by SoConvexDataCache.
*/
class SoConvexDataCacheTessellator {
public:
  static bool available(void) { 
    return true; // Our implementation is always available
  }

  SoConvexDataCacheTessellator(void (*callback)(void * v0, void * v1, void * v2, void * data) = nullptr, void * userdata = nullptr)
    : tessellator_(callback, userdata) {
  }

  ~SoConvexDataCacheTessellator(void) {
    // Destructor handles cleanup automatically
  }

  void beginPolygon(const SbVec3f & normal = SbVec3f(0.0f, 0.0f, 0.0f)) {
    tessellator_.beginPolygon(normal);
  }

  void addVertex(const SbVec3f & v, void * data) {
    tessellator_.addVertex(v, data);
  }

  void endPolygon(void) {
    tessellator_.endPolygon();
  }

  static bool preferred(void) {
    // Return true to prefer our implementation over SbTesselator
    // when GLU is not available
    return true;
  }

private:
  SoConvexDataCacheGLUTessellator tessellator_;
};

#endif // !COIN_SOCONVEXDATACACHE_TESSELLATOR_H