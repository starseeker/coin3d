#ifndef OBOL_SORAYTRACERENDERACTION_H
#define OBOL_SORAYTRACERENDERACTION_H

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
  \class SoRaytraceRenderAction SoRaytraceRenderAction.h Inventor/actions/SoRaytraceRenderAction.h
  \brief Action for traversing a scene graph to collect geometry for a raytracing backend.

  \ingroup coin_actions

  This action traverses a scene graph and generates geometry primitives
  (triangles, line segments, points) via the \c SoShape::generatePrimitives()
  mechanism.  It has \b no OpenGL dependency and works with any rendering
  backend, including CPU-based raytracers such as
  <a href="https://github.com/lighttransport/nanort">nanort</a> or
  <a href="https://www.ospray.org/">OSPRay</a>.

  \c SoRaytraceRenderAction inherits from \c SoCallbackAction, which already
  provides all the machinery needed to collect scene geometry and inspect
  scene state during traversal.  This class adds:

  \li A distinct type id so that shape nodes can detect the raytracing path
      via \c action->isOfType(SoRaytraceRenderAction::getClassTypeId()).
  \li A \c SbViewportRegion constructor argument, mirroring
      \c SoGLRenderAction for symmetry.
  \li A \c getLights() convenience method that returns the scene lights
      accumulated during traversal.

  \b Usage example:

  \code
  // Build a triangle list for a nanort BVH
  struct MyScene {
    std::vector<float> verts;   // flat xyz triples
    std::vector<int>   indices; // triangle indices
  };

  static void
  triangle_cb(void * userdata, SoCallbackAction * action,
              const SoPrimitiveVertex * v1,
              const SoPrimitiveVertex * v2,
              const SoPrimitiveVertex * v3)
  {
    MyScene * scene = static_cast<MyScene *>(userdata);
    const SbMatrix & mm = action->getModelMatrix();
    const SoPrimitiveVertex * verts[3] = { v1, v2, v3 };
    for (int i = 0; i < 3; ++i) {
      SbVec3f p;
      mm.multVecMatrix(verts[i]->getPoint(), p);
      scene->indices.push_back(static_cast<int>(scene->verts.size() / 3));
      scene->verts.push_back(p[0]);
      scene->verts.push_back(p[1]);
      scene->verts.push_back(p[2]);
    }
  }

  // ... in render code:
  MyScene scene;
  SoRaytraceRenderAction rta(SbViewportRegion(800, 600));
  rta.addTriangleCallback(SoShape::getClassTypeId(), triangle_cb, &scene);
  rta.apply(root);
  // Now scene.verts and scene.indices contain the full tessellated geometry.
  // Use rta.getLights() to retrieve scene lights for shading.
  \endcode

  During the triangle callback, the following \c SoCallbackAction accessors
  are available to query the current traversal state:

  \li \c getModelMatrix() — world transform for the current shape
  \li \c getMaterial() — ambient/diffuse/specular/emission/shininess/transparency
  \li \c getTextureImage() — raw pixel data for the current 2D texture
  \li \c getViewingMatrix() / \c getProjectionMatrix() / \c getViewVolume() — camera data
  \li \c getLightModel() — PHONG or BASE_COLOR

  Additionally, each \c SoPrimitiveVertex carries:
  \li \c getPoint() — vertex position in object space (apply model matrix for world space)
  \li \c getNormal() — surface normal in object space
  \li \c getTextureCoords() — texture coordinate

  \sa SoCallbackAction, SoShape::generatePrimitives(), docs/BACKEND_SURVEY.md
*/

#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/lists/SbList.h>
#include <Inventor/lists/SoNodeList.h>

class OBOL_DLL_API SoRaytraceRenderAction : public SoCallbackAction {
  typedef SoCallbackAction inherited;

  SO_ACTION_HEADER(SoRaytraceRenderAction);

public:
  static void initClass(void);

  SoRaytraceRenderAction(const SbViewportRegion & viewportregion);
  virtual ~SoRaytraceRenderAction(void);

  void setViewportRegion(const SbViewportRegion & newregion);
  const SbViewportRegion & getViewportRegion(void) const;

  // Implementation detail: callback userdata struct used by the internal
  // light-collection callback.  Public to allow access from the file-scope
  // static callback function in the .cpp.
  struct LightCbData {
    SoNodeList *       lights;
    SbList<SbMatrix> * transforms;
  };

  /*!
    Returns the list of \c SoLight nodes that were visited during the most
    recent \c apply() traversal.  Each entry is a \c SoLight subclass
    (\c SoDirectionalLight, \c SoPointLight, or \c SoSpotLight).

    To query a light's world-space transform at the time it was encountered,
    register an additional pre-callback on \c SoLight::getClassTypeId() and
    call \c getModelMatrix() inside it, or use the parallel list returned by
    \c getLightTransforms().

    \note Call this after \c apply(); the list is populated during traversal.
    \note All \c SoLight nodes encountered are listed regardless of their
          \c on field.  Check the field before using a light if you only want
          enabled lights.
  */
  const SoNodeList & getLights(void) const;

  /*!
    Returns the world-space model matrix for each light in \c getLights(),
    captured at the moment the light node was first encountered during
    traversal.  The \a i-th entry corresponds to \c getLights()[i].

    This is particularly important for positional lights (\c SoPointLight,
    \c SoSpotLight) whose position and direction fields are defined in
    object space and must be transformed to world space for shading.

    For \c SoDirectionalLight the direction field is typically already in
    world space (i.e. the light is placed at the top of the graph outside
    any transform), but using \c getLightTransforms() is more robust.

    \note Call this after \c apply(); the list is populated during traversal.
  */
  const SbList<SbMatrix> & getLightTransforms(void) const;

protected:
  virtual void beginTraversal(SoNode * node);

private:
  SbViewportRegion vpregion;
  SoNodeList lights_cache;
  SbList<SbMatrix> light_transforms_cache;
  LightCbData light_cb_data;

  SoRaytraceRenderAction(const SoRaytraceRenderAction & rhs);
  SoRaytraceRenderAction & operator=(const SoRaytraceRenderAction & rhs);
};

#endif // !OBOL_SORAYTRACERENDERACTION_H
