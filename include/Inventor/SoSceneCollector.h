#ifndef OBOL_SOSCENECOLLECTOR_H
#define OBOL_SOSCENECOLLECTOR_H

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
  \class SoSceneCollector SoSceneCollector.h Inventor/SoSceneCollector.h
  \brief Utility class for collecting world-space scene data for non-GL rendering backends.

  \ingroup coin_general

  \c SoSceneCollector traverses an Obol scene graph and extracts
  everything a CPU raytracing backend (nanort, Intel Embree, OSPRay, …) needs
  to render it — without any OpenGL dependency:

  \li \b Triangles — all scene geometry decomposed to world-space triangles
      with per-vertex positions, normals, and the active material.
  \li \b Lights — all scene lights (directional, point, spot) converted to
      world-space descriptors including position, direction, color, intensity,
      and spot cone parameters.
  \li \b Pixel overlays — rasterized RGBA bitmaps for \c SoText2 text and
      \c SoHUDLabel / \c SoHUDButton elements that must be alpha-composited
      onto the framebuffer after ray tracing.

  The collector also handles the following scene-graph patterns that would
  otherwise produce no geometry:

  \li \c SoLineSet / \c SoIndexedLineSet — replaced with thin cylinder proxies
      (via \c SoLineSet::createCylinderProxy()) sized to match the screen-space
      line width.
  \li \c SoPointSet — replaced with small sphere proxies (via
      \c SoPointSet::createSphereProxy()) sized to match the screen-space point size.
  \li \c SoCylinder with \c DrawStyle \c LINES — replaced with a ring of thin
      cylinder segments to match the wireframe ring used by rotational draggers.
  \li Shapes with \c DrawStyle \c INVISIBLE — pruned from the collected geometry
      so that picking-only shapes (e.g. the sphere inside
      \c SoRotateSphericalDragger) are not rendered as solid geometry.

  <b>Scene change detection (BVH cache management):</b>

  Building a BVH from scratch on every frame is expensive.  The collector
  tracks Coin node unique IDs so the caller can detect when the scene geometry
  has actually changed and skip unnecessary rebuilds:

  \code
  // In the context manager's renderScene():
  SoCamera * cam = findCamera(root);
  if (collector_.needsRebuild(root, cam)) {
      collector_.reset();
      collector_.collect(root, viewport);
      buildBVH(collector_.getTriangles());          // backend-specific
      collector_.updateCacheKeysAfterRebuild(root, cam);
  } else {
      collector_.collectOverlaysOnly(root, viewport); // cheap re-collect of text/HUD
  }
  // ... raytrace using cached BVH and collector_.getLights() ...
  collector_.compositeOverlays(pixels, width, height, nrcomponents);
  collector_.updateCameraId(cam);
  \endcode

  <b>Usage example (with nanort):</b>

  \code
  SoSceneCollector collector;
  SbViewportRegion vp(800, 600);
  collector.collect(root, vp);

  // Feed geometry into nanort BVH:
  for (const SoScTriangle & tri : collector.getTriangles()) {
      for (int v = 0; v < 3; ++v) {
          verts.push_back(tri.pos[v][0]);
          verts.push_back(tri.pos[v][1]);
          verts.push_back(tri.pos[v][2]);
      }
  }
  // ... build BVH, raytrace, composite overlays ...
  collector.compositeOverlays(pixels, width, height, 3);
  \endcode

  \sa SoSceneRenderAction, SoSceneRendererParams, docs/BACKEND_SURVEY.md
*/

#include <Inventor/SbBasic.h>
#include <Inventor/SbViewportRegion.h>

#include <vector>
#include <cstdint>

class SoNode;
class SoCamera;
class SoCallbackAction;
class SoSeparator;
class SoCoordinateElement;

// ==========================================================================
// World-space scene data structures
// ==========================================================================

/*!
  \struct SoScMaterial
  \brief Surface material properties for a single triangle, in the Phong model.

  All colour channels are in [0, 1].  \c shininess is in [0, 1]; multiply
  by 128 to obtain the Phong specular exponent.
*/
struct SoScMaterial {
    float diffuse[3];   //!< Diffuse reflectance (RGB)
    float specular[3];  //!< Specular reflectance (RGB)
    float ambient[3];   //!< Ambient reflectance (RGB)
    float emission[3];  //!< Emissive colour (RGB)
    float shininess;    //!< Specular shininess [0,1]; Phong exponent = shininess * 128
};

/*!
  \struct SoScTriangle
  \brief A single world-space triangle with per-vertex positions, normals, and material.

  Positions and normals are stored in world space (model matrix already applied).
  Vertex ordering follows the scene graph; normals may point in either direction
  depending on the shape's winding.
*/
struct SoScTriangle {
    float      pos[3][3];   //!< World-space vertex positions [vertex 0/1/2][x/y/z]
    float      norm[3][3];  //!< World-space vertex normals   [vertex 0/1/2][x/y/z]
    SoScMaterial mat;       //!< Active material at this triangle
};

/*!
  \enum SoScLightType
  \brief Type tag for \c SoScLightInfo.
*/
enum SoScLightType {
    SO_SC_DIRECTIONAL,  //!< SoDirectionalLight — infinite parallel light
    SO_SC_POINT,        //!< SoPointLight — positional light with attenuation
    SO_SC_SPOT          //!< SoSpotLight — positional cone-limited light
};

/*!
  \struct SoScLightInfo
  \brief World-space descriptor for a single scene light.

  All positions and directions are already transformed to world space.

  For \c SO_SC_DIRECTIONAL lights, only \c dir is used.
  For \c SO_SC_POINT lights, only \c pos is used.
  For \c SO_SC_SPOT lights, both \c pos and \c dir are used;
  \c dir is the cone axis (pointing away from the light source).
*/
struct SoScLightInfo {
    SoScLightType type;       //!< Light category
    float rgb[3];             //!< Light colour (RGB)
    float intensity;          //!< Intensity scale factor
    float dir[3];             //!< World-space direction (away from source, unit vector)
    float pos[3];             //!< World-space position (POINT and SPOT only)
    float cutOffAngle;        //!< Spot cone half-angle in radians (SPOT only)
    float dropOffRate;        //!< Spot intensity drop-off exponent (SPOT only)
};

/*!
  \struct SoScTextOverlay
  \brief An RGBA pixel buffer to be alpha-composited onto the framebuffer.

  Generated for \c SoText2, \c SoHUDLabel, and \c SoHUDButton elements.
  The buffer uses GL bottom-to-top row convention (row 0 = bottom of screen)
  to match \c SoOffscreenRenderer::getBuffer().

  Composite onto the framebuffer with \c SoSceneCollector::compositeOverlays().
*/
struct SoScTextOverlay {
    std::vector<unsigned char> pixbuf;  //!< RGBA pixels, bottom-to-top rows
    int x;  //!< Viewport-space left edge (pixels from left of frame, may be negative)
    int y;  //!< Viewport-space bottom edge (pixels from bottom, may be negative)
    int w;  //!< Width of the pixel buffer
    int h;  //!< Height of the pixel buffer
};

// ==========================================================================
// SoSceneCollector
// ==========================================================================

class OBOL_DLL_API SoSceneCollector {
public:
    SoSceneCollector();
    ~SoSceneCollector();

    // ------------------------------------------------------------------
    // Scene collection
    // ------------------------------------------------------------------

    /*!
      Traverse \a root and fill the triangle, light, and overlay lists.

      This is the primary entry point.  It sets up a \c SoCallbackAction
      with the full suite of pre-callbacks and registers all necessary
      handlers:

      \li Triangle collection (all \c SoShape subclasses)
      \li Invisible-shape pruning (\c DrawStyle \c INVISIBLE)
      \li Proxy geometry for \c SoLineSet, \c SoIndexedLineSet (→ cylinders)
      \li Proxy geometry for \c SoPointSet (→ spheres)
      \li Ring proxy for \c SoCylinder with \c DrawStyle \c LINES (→ dragger rings)
      \li Pixel overlay collection for \c SoText2 (via \c buildPixelBuffer())
      \li Pixel overlay rasterization for \c SoHUDLabel and \c SoHUDButton
      \li Light collection (directional, point, spot — world-space transformed)

      \note Call \c reset() before \c collect() when a full rebuild is needed
            (i.e. when \c needsRebuild() returns \c TRUE).
    */
    void collect(SoNode * root, const SbViewportRegion & vp);

    /*!
      Variant that accepts separate render and display viewports.

      \a renderVp is used for ray generation; \a displayVp is used for
      proxy geometry sizing (line width, point size → world-space radius).
      Pass a larger display viewport when rendering at a reduced coarse
      resolution so that proxy radii reflect the real screen pixel density.
    */
    void collect(SoNode * root,
                 const SbViewportRegion & renderVp,
                 const SbViewportRegion & displayVp);

    /*!
      Lightweight re-traversal that updates only the text / HUD overlays.

      On a cache hit (geometry unchanged, camera only moved), call this
      instead of the full \c collect() to regenerate screen-space overlay
      positions without redoing the expensive geometry collection.

      Replaces the existing overlays in place; triangles and lights are
      not modified.
    */
    void collectOverlaysOnly(SoNode * root, const SbViewportRegion & vp);

    /*!
      Clear all collected data (triangles, lights, overlays).

      Call this before \c collect() on a full scene rebuild.
    */
    void reset();

    // ------------------------------------------------------------------
    // Scene change detection (BVH cache management)
    // ------------------------------------------------------------------

    /*!
      Returns \c TRUE if the collected scene geometry / lights are stale
      and a full \c collect() + BVH rebuild is required.

      Uses \c SoNode::getNodeId() change detection: Coin propagates
      field-change notifications up through the scene graph so that any
      ancestor (including the root) has its unique ID bumped whenever a
      descendant changes.  A camera orbit bumps the root's ID too, so this
      method additionally checks whether the root-ID change is solely due
      to the camera moving (same camera pointer, changed camera ID).  If
      so, it returns \c FALSE — only an overlay re-collection is needed.

      \a cam may be \c nullptr; if the camera is unknown \c TRUE is
      returned unconditionally.

      \note Call \c resetCache() before passing a new scene root to guard
            against pointer aliasing (Coin may reuse freed node addresses).
    */
    SbBool needsRebuild(SoNode * root, SoCamera * cam) const;

    /*!
      Record the current root and camera node IDs as the "clean" baseline.
      Call once after a successful \c collect() + BVH build.
    */
    void updateCacheKeysAfterRebuild(SoNode * root, SoCamera * cam);

    /*!
      Update the cached camera and root node IDs after each render (including
      cache hits).  Required so that the next \c needsRebuild() call can
      distinguish a camera-only move from a true geometry change.

      \a root should always be provided so that \c cachedRootId_ tracks the
      scene's current node ID.  Without it, \c cachedRootId_ stays at the
      value from the last BVH rebuild; when the camera later stops moving,
      \c cameraOnlyMoved becomes \c FALSE while \c rootChanged stays \c TRUE,
      triggering a spurious BVH rebuild on every static frame.
    */
    void updateCameraId(SoCamera * cam, SoNode * root = nullptr);

    /*!
      Force the next \c needsRebuild() call to return \c TRUE.
      Call whenever the scene root is replaced.
    */
    void resetCache();

    // ------------------------------------------------------------------
    // Data accessors
    // ------------------------------------------------------------------

    //! World-space triangles collected during the last \c collect() call.
    const std::vector<SoScTriangle> & getTriangles() const;

    //! World-space light descriptors collected during the last \c collect() call.
    const std::vector<SoScLightInfo> & getLights() const;

    /*!
      Pixel overlays collected during the last \c collect() or
      \c collectOverlaysOnly() call.  Composite these onto the framebuffer
      after ray tracing with \c compositeOverlays().
    */
    const std::vector<SoScTextOverlay> & getOverlays() const;

    // ------------------------------------------------------------------
    // Pixel overlay compositing
    // ------------------------------------------------------------------

    /*!
      Alpha-composite all collected overlays onto \a pixels.

      \a pixels is a row-major buffer of \a width × \a height ×
      \a nrcomponents bytes in GL bottom-to-top row order (as returned by
      \c SoOffscreenRenderer::getBuffer()).  \a nrcomponents must be 3 (RGB)
      or 4 (RGBA).

      Compositing uses the standard "src over dst" formula with per-pixel
      alpha derived from the stb_truetype grayscale glyph coverage values,
      giving smooth anti-aliased text edges.
    */
    void compositeOverlays(unsigned char * pixels,
                           unsigned int width, unsigned int height,
                           unsigned int nrcomponents) const;

    // ------------------------------------------------------------------
    // Proxy geometry utilities
    // ------------------------------------------------------------------

    /*!
      Compute the local-space proxy radius that corresponds to \a sizePx
      pixels at the depth of the current shape.

      The returned value is suitable for passing directly to
      \c createCylinderProxy() or \c createSphereProxy().  It accounts for
      any \c Scale nodes in the current model matrix so that after
      \c collectProxy() (or \c src_collectProxy) re-applies the full model
      matrix the net world-space radius matches the pixel-size target.

      Used internally for line/point proxy sizing but exposed here so
      custom backends can size their own proxy geometry consistently.

      \a action must be inside a \c SoCallbackAction traversal.
      \a sizePx is the screen-space size in pixels (line width / point size).
      \a viewportHeightPx is the display viewport height in pixels.
    */
    static float computeWorldSpaceRadius(SoCallbackAction * action,
                                         float sizePx,
                                         float viewportHeightPx);

    /*!
      Build a ring-of-cylinders proxy for a wireframe cylinder (i.e.
      \c SoCylinder rendered with \c DrawStyle \c LINES).

      Returns a reference-counted \c SoSeparator containing \a numSegments
      thin \c SoCylinder segments arranged in a circle of radius
      \a cylRadius.  Each segment has a cross-section radius of
      \a tubeRadius.

      The caller owns the returned separator (ref count = 0 after
      \c unrefNoDelete(); call \c ref() immediately if needed).

      This proxy matches the appearance of the circular ring rendered by
      \c SoGLRenderAction for rotational dragger widgets
      (\c SoRotateSphericalDragger, \c SoTrackballDragger, etc.).
    */
    static SoSeparator * createWireframeCylinderProxy(float cylRadius,
                                                      float tubeRadius,
                                                      int   numSegments = 32);

private:
    // Internal helpers (implementations in SoSceneCollector.cpp).
    void collectImpl(SoNode * root,
                     const SbViewportRegion & renderVp,
                     const SbViewportRegion & proxyVp);
    void collectProxy(SoCallbackAction * action, SoSeparator * proxy);

    // Collected scene data
    std::vector<SoScTriangle>    tris_;
    std::vector<SoScLightInfo>   lights_;
    std::vector<SoScTextOverlay> overlays_;

    // Cache state for scene change detection
    SoNode *   cachedRoot_   = nullptr;
    SbUniqueId cachedRootId_ = 0;
    SoCamera * cachedCamPtr_ = nullptr;
    SbUniqueId cachedCamId_  = 0;

    // Non-copyable
    SoSceneCollector(const SoSceneCollector &);
    SoSceneCollector & operator=(const SoSceneCollector &);
};

#endif // !OBOL_SOSCENECOLLECTOR_H
