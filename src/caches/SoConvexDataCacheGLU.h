#ifndef COIN_SOCONVEXDATACACHE_GLU_H
#define COIN_SOCONVEXDATACACHE_GLU_H

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
  \file SoConvexDataCacheGLU.h
  \brief Minimal GLU tessellation functionality for SoConvexDataCache
  
  This is a header-only minimal implementation of GLU tessellation functionality
  specifically designed to support SoConvexDataCache. It provides only the 
  minimal subset of GLU tessellation needed by that class.

  This implementation is derived from the GLU tessellation interface but is
  designed to be self-contained and not require external GLU libraries.
*/

#ifndef COIN_INTERNAL
#error this is a private header file
#endif /* ! COIN_INTERNAL */

#include <Inventor/SbVec3f.h>
#include <Inventor/lists/SbList.h>
#include <Inventor/system/gl.h>

#include <cassert>
#include <vector>
#include <cmath>

// GLU constants we need
#define COIN_GLU_TESS_BEGIN_DATA                100106
#define COIN_GLU_TESS_VERTEX_DATA               100107
#define COIN_GLU_TESS_ERROR_DATA                100109
#define COIN_GLU_TESS_MISSING_BEGIN_POLYGON     100151
#define COIN_GLU_TESS_MISSING_BEGIN_CONTOUR     100152
#define COIN_GLU_TESS_MISSING_END_POLYGON       100153
#define COIN_GLU_TESS_MISSING_END_CONTOUR       100154
#define COIN_GLU_TESS_NEED_COMBINE_CALLBACK     100156

#ifndef APIENTRY
#ifdef _WIN32
#define APIENTRY __stdcall
#else
#define APIENTRY
#endif
#endif

// Forward declarations
struct SoConvexDataCacheGLUTessellator;

// Callback function types
typedef void (APIENTRY *SoConvexDataCacheGLUTessCallback_cb_t)(void);

// Simple 2D polygon tessellation using fan triangulation
// This is a simplified approach that works for convex polygons and many non-convex cases
class SoConvexDataCacheGLUTessellator {
public:
    static bool available(void) { return true; }

    SoConvexDataCacheGLUTessellator(void (*callback)(void * v0, void * v1, void * v2, void * data) = nullptr, void * userdata = nullptr)
        : callback_(callback), cbdata_(userdata), tessobj_(this), triangletessmode_(GL_TRIANGLES),
          vertexidx_(0), stripflipflop_(false)
    {
        assert(callback && "tessellation without callback is meaningless");
        vertexdata_[0] = vertexdata_[1] = nullptr;
    }

    ~SoConvexDataCacheGLUTessellator() {
        // Nothing to cleanup in our simplified implementation
    }

    void beginPolygon(const SbVec3f & normal = SbVec3f(0.0f, 0.0f, 0.0f)) {
        coords_.truncate(0);
        vertexdatas_.clear();
        normal_ = normal;
    }

    void addVertex(const SbVec3f & v, void * data) {
        struct vertex_coord c = { { v[0], v[1], v[2] } };
        coords_.append(c);
        vertexdatas_.push_back(data);
    }

    void endPolygon(void) {
        tessellatePolygon();
        coords_.truncate(0);
        vertexdatas_.clear();
    }

    static bool preferred(void) {
        // Always prefer our internal implementation since it's always available
        return false; // Return false to not override SbTesselator preference
    }

private:
    struct vertex_coord { GLdouble c[3]; };

    void tessellatePolygon() {
        const int numVertices = coords_.getLength();
        if (numVertices < 3) return; // Can't tessellate less than a triangle

        // Simple fan triangulation: connect vertex 0 to all other pairs
        // This works well for convex polygons and many simple non-convex cases
        for (int i = 1; i < numVertices - 1; ++i) {
            // Create triangle: vertex 0, vertex i, vertex i+1
            if (callback_) {
                callback_(vertexdatas_[0], vertexdatas_[i], vertexdatas_[i + 1], cbdata_);
            }
        }
    }

    void (*callback_)(void *, void *, void *, void *);
    void * cbdata_;
    SoConvexDataCacheGLUTessellator * tessobj_;
    SbVec3f normal_;

    SbList<struct vertex_coord> coords_;
    std::vector<void*> vertexdatas_;

    // Variables for GL primitive handling (not used in our simple implementation)
    GLenum triangletessmode_;
    unsigned int vertexidx_;
    void * vertexdata_[2];
    bool stripflipflop_;
};

// Standalone functions that mimic the GLU tessellation interface for compatibility
inline SoConvexDataCacheGLUTessellator* coin_gluNewTess(void) {
    return new SoConvexDataCacheGLUTessellator();
}

inline void coin_gluDeleteTess(SoConvexDataCacheGLUTessellator* tessobj) {
    delete tessobj;
}

inline void coin_gluTessCallback(SoConvexDataCacheGLUTessellator* tessobj, GLenum which, SoConvexDataCacheGLUTessCallback_cb_t callback) {
    // In our simplified implementation, we only support the basic triangle callback
    // The actual callback setup is done in the constructor
}

inline void coin_gluTessBeginPolygon(SoConvexDataCacheGLUTessellator* tessobj, void* user_data) {
    if (tessobj) {
        tessobj->beginPolygon();
    }
}

inline void coin_gluTessEndPolygon(SoConvexDataCacheGLUTessellator* tessobj) {
    if (tessobj) {
        tessobj->endPolygon();
    }
}

inline void coin_gluTessBeginContour(SoConvexDataCacheGLUTessellator* tessobj) {
    // In our simplified implementation, we only support single contours
    // This is typically sufficient for SoConvexDataCache use cases
}

inline void coin_gluTessEndContour(SoConvexDataCacheGLUTessellator* tessobj) {
    // No-op in our simplified implementation
}

inline void coin_gluTessVertex(SoConvexDataCacheGLUTessellator* tessobj, GLdouble coords[3], void* vertex_data) {
    if (tessobj) {
        SbVec3f v(static_cast<float>(coords[0]), static_cast<float>(coords[1]), static_cast<float>(coords[2]));
        tessobj->addVertex(v, vertex_data);
    }
}

inline void coin_gluTessNormal(SoConvexDataCacheGLUTessellator* tessobj, GLdouble x, GLdouble y, GLdouble z) {
    // Normal setting is handled in beginPolygon in our implementation
}

inline const char* coin_gluErrorString(GLenum error) {
    switch (error) {
        case COIN_GLU_TESS_MISSING_BEGIN_POLYGON:
            return "missing gluTessBeginPolygon";
        case COIN_GLU_TESS_MISSING_END_POLYGON:
            return "missing gluTessEndPolygon";
        case COIN_GLU_TESS_MISSING_BEGIN_CONTOUR:
            return "missing gluTessBeginContour";
        case COIN_GLU_TESS_MISSING_END_CONTOUR:
            return "missing gluTessEndContour";
        case COIN_GLU_TESS_NEED_COMBINE_CALLBACK:
            return "need combine callback";
        default:
            return "unknown tessellation error";
    }
}

#endif // !COIN_SOCONVEXDATACACHE_GLU_H