#ifndef OBOL_SOPROCEDURALSHAPE_H
#define OBOL_SOPROCEDURALSHAPE_H

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
  \class SoProceduralShape SoProceduralShape.h Inventor/nodes/SoProceduralShape.h
  \brief Generic scene-graph node for application-defined procedural geometry.

  \ingroup coin_nodes

  SoProceduralShape allows applications to introduce custom 3D shape types
  into the Obol scene graph without writing C++ subclasses of SoShape for
  every shape variant.  Instead, the application:

  1. Defines a \e shape \e type by calling registerShapeType(), supplying:
     - A unique string name (e.g. "TruncatedCone")
     - A JSON schema string describing the editable parameters, and optionally
       the shape topology (vertices, faces) and handle definitions.
     - A bounding-box callback (SoProceduralBBoxCB) that fills in the AABB
       for a given parameter set.
     - A geometry callback (SoProceduralGeomCB) that fills triangles and/or
       a 3-D wireframe for a given parameter set.

  2. Creates one or more SoProceduralShape nodes, sets their \c shapeType
     field to the registered name, and populates the \c params field with
     the parameter values (ordered identically to the JSON schema).

  The node participates fully in the Obol scene-graph action system:
  - \c SoGetBoundingBoxAction calls the bbox callback.
  - \c SoGLRenderAction renders triangles (FILLED draw style) or the
    wireframe (LINES draw style) via the OpenGL fixed-function path.
  - \c SoRayPickAction picks against the triangle mesh.
  - \c SoCallbackAction / \c SoGetPrimitiveCountAction work through
    the standard SoShape::generatePrimitives() mechanism.

  \b JSON schema — parameters only (minimal):
  \code
  {
    "type"  : "TruncatedCone",
    "label" : "Truncated Cone",
    "params": [
      { "name": "bottomRadius", "type": "float", "default": 1.0,
        "min": 0.001, "max": 100.0, "label": "Bottom Radius" },
      { "name": "topRadius",    "type": "float", "default": 0.5,
        "min": 0.0,   "max": 100.0, "label": "Top Radius" },
      { "name": "height",       "type": "float", "default": 2.0,
        "min": 0.001, "max": 100.0, "label": "Height" },
      { "name": "sides",        "type": "int",   "default": 16,
        "min": 3,     "max": 128,   "label": "Number of Sides" }
    ]
  }
  \endcode

  Supported parameter types: "float", "int", "bool" (stored as 0/1).

  \b JSON schema — full topology (enables auto-handle generation):

  \code
  {
    "type": "ARB8",
    "label": "Arbitrary 8-Vertex Solid",
    "params": [
      { "name": "v0x", "type": "float", "default": -1.0, "label": "V0 X" },
      { "name": "v0y", "type": "float", "default": -1.0, "label": "V0 Y" },
      { "name": "v0z", "type": "float", "default": -1.0, "label": "V0 Z" },
      ...  (24 entries total, 3 per vertex)
    ],

    "vertices": [
      { "name": "v0", "x": "v0x", "y": "v0y", "z": "v0z" },
      { "name": "v1", "x": "v1x", "y": "v1y", "z": "v1z" },
      ...  (one entry per vertex)
    ],

    "faces": [
      { "name": "bottom", "verts": ["v0","v3","v2","v1"], "opposite": "top"  },
      { "name": "top",    "verts": ["v4","v5","v6","v7"], "opposite": "bottom" },
      ...
    ],

    "handles": [
      { "name": "v0_h",     "vertex": "v0",         "dragType": "DRAG_NO_INTERSECT" },
      { "name": "e01_h",    "edge":   ["v0","v1"],   "dragType": "DRAG_ON_PLANE"     },
      { "name": "f_bot_h",  "face":   "bottom",      "dragType": "DRAG_NO_INTERSECT" }
    ]
  }
  \endcode

  \b Topology section semantics:

  - \b "vertices": maps each named vertex to its three float parameter names.
    SoProceduralShape uses this to compute handle positions and to apply drag
    deltas without any application callback.

  - \b "faces": defines quad faces by vertex name.  Vertices must appear in CCW
    order when viewed from outside the solid.  The optional \b "opposite" field
    is advisory metadata; the validity constraint is now handled entirely by the
    application's SoProceduralObjectValidateCB, which is called with the full
    proposed parameter state serialised as a JSON object.

  - \b "handles": each entry defines one interactive dragger widget.  The
    \b "vertex", \b "edge" (two-element array), or \b "face" field selects the
    affected geometry.  The \b "dragType" field controls dragger behaviour:

    | dragType              | Dragger used          | Constraint applied          |
    |-----------------------|-----------------------|-----------------------------|
    | "DRAG_POINT"          | SoDragPointDragger    | None                        |
    | "DRAG_ALONG_AXIS"     | SoTranslate1Dragger   | Face/edge normal axis       |
    | "DRAG_ON_PLANE"       | SoTranslate2Dragger   | Adjacent face plane         |
    | "DRAG_NO_INTERSECT"   | SoDragPointDragger    | App objectValidateCB called |

    \b DRAG_NO_INTERSECT uses a SoDragPointDragger (free 3-D movement) but
    before applying the drag delta, SoProceduralShape serialises the proposed
    new parameter state to a JSON object and calls the registered
    SoProceduralObjectValidateCB.  The application parses that JSON using its
    own domain knowledge and returns TRUE (accept) or FALSE (reject).  On
    rejection the dragger snaps back to its original position.

    The serialised JSON format is:
    \code
    { "type": "ARB8",
      "params": { "v0x": -1.0, "v0y": -1.0, "v0z": -1.0, ... } }
    \endcode
    This uses the parameter names from the \b "params" section of the schema.
    If the schema has no named parameters, index-based keys are used instead
    ("p0", "p1", …).

  \b When topology is present in the schema, the \c handlesCB and
  \c handleDragCB arguments to registerShapeType() may be \c nullptr.
  SoProceduralShape will auto-generate handles and apply drag deltas using
  the parsed topology.

  \b FILE FORMAT/DEFAULTS:
  \code
    ProceduralShape {
        shapeType  ""
        schemaJSON ""
        params     []
    }
  \endcode

  \sa SoShape, SoIndexedFaceSet, SoIndexedLineSet
*/

#include <vector>
#include <cstdint>

#include <Inventor/SbString.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/nodes/SoSubNode.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/fields/SoSFString.h>
#include <Inventor/fields/SoMFFloat.h>

// ---------------------------------------------------------------------------
// Geometry data containers filled by application callbacks
// ---------------------------------------------------------------------------

/*!
  \struct SoProceduralTriangles
  \brief Triangle-mesh output filled by a SoProceduralGeomCB.

  The application callback populates \c vertices, (optionally) \c normals,
  and \c indices.  \c indices must contain triples of vertex indices;
  each triple describes one triangle.
*/
struct SoProceduralTriangles {
  std::vector<SbVec3f>  vertices;  //!< One entry per unique vertex position.
  std::vector<SbVec3f>  normals;   //!< Per-vertex normals; may be empty.
  std::vector<int32_t>  indices;   //!< Three indices per triangle.
};

/*!
  \struct SoProceduralWireframe
  \brief 3-D wireframe output filled by a SoProceduralGeomCB.

  \c vertices holds the unique 3-D positions; \c segments holds pairs of
  vertex indices that define line segments.  A value of \c -1 may be used
  as a polyline-end sentinel instead of repeating endpoints.
*/
struct SoProceduralWireframe {
  std::vector<SbVec3f>  vertices;  //!< One entry per unique vertex position.
  std::vector<int32_t>  segments;  //!< Index pairs; -1 = end of polyline.
};

// ---------------------------------------------------------------------------
// Callback typedefs
// ---------------------------------------------------------------------------

/*!
  \typedef SoProceduralBBoxCB
  \brief Callback that computes the axis-aligned bounding box of a shape.

  \param params    Pointer to the current parameter values array.
  \param numParams Number of values in \a params.
  \param minPt     [out] Minimum corner of the AABB in object space.
  \param maxPt     [out] Maximum corner of the AABB in object space.
  \param userdata  Opaque pointer supplied at registration time.
*/
typedef void (*SoProceduralBBoxCB)(const float* params,
                                   int          numParams,
                                   SbVec3f&     minPt,
                                   SbVec3f&     maxPt,
                                   void*        userdata);

/*!
  \typedef SoProceduralGeomCB
  \brief Callback that generates triangle and/or wireframe geometry.

  Either \a tris or \a wire (but not both) may be \c nullptr, indicating
  that the caller does not need that geometry variant.  The callback should
  only populate the non-null output.

  \param params    Pointer to the current parameter values array.
  \param numParams Number of values in \a params.
  \param tris      [out] Triangle mesh; may be \c nullptr.
  \param wire      [out] Wireframe geometry; may be \c nullptr.
  \param userdata  Opaque pointer supplied at registration time.
*/
typedef void (*SoProceduralGeomCB)(const float*             params,
                                   int                      numParams,
                                   SoProceduralTriangles*   tris,
                                   SoProceduralWireframe*   wire,
                                   void*                    userdata);

// ---------------------------------------------------------------------------
// Interactive handle API — used by buildHandleDraggers()
// ---------------------------------------------------------------------------

/*!
  \struct SoProceduralHandle
  \brief One interactive manipulation handle for a SoProceduralShape.

  The application fills a vector of these in a SoProceduralHandlesCB to tell
  Obol where to place dragger widgets and how they are constrained.

  \note All positions are in the shape's local (object) space.
*/
struct SoProceduralHandle {
  /*! Constraint types for the dragger associated with this handle. */
  enum DragType {
    DRAG_POINT,         //!< Free 3-D movement (uses SoDragPointDragger, no constraint).
    DRAG_ALONG_AXIS,    //!< Constrained to a single axis (uses SoTranslate1Dragger).
    DRAG_ON_PLANE,      //!< Constrained to a plane (uses SoTranslate2Dragger).
    DRAG_NO_INTERSECT   /*!< Free 3-D movement (uses SoDragPointDragger).
                             Before each delta is applied, Obol serialises the
                             proposed new parameter state to a JSON object and
                             calls the registered SoProceduralObjectValidateCB.
                             The application decides whether the new state is
                             valid and returns TRUE (accept) or FALSE (reject).
                             On rejection the dragger snaps back.  If no
                             SoProceduralObjectValidateCB has been registered the
                             move is always accepted (same as DRAG_POINT).
                             SoProceduralHandleValidateCB, if set, is called
                             after objectValidateCB to allow positional clamping. */
  };

  SbVec3f  position;   //!< Handle position in object space (from current params).
  DragType dragType;   //!< Constraint type.
  SbVec3f  axis;       //!< DRAG_ALONG_AXIS: constraint direction (need not be unit).
  SbVec3f  normal;     //!< DRAG_ON_PLANE: plane normal (need not be unit).
  SbString name;       //!< Display / debug name (optional).

  SoProceduralHandle()
    : position(0.f,0.f,0.f), dragType(DRAG_POINT),
      axis(1.f,0.f,0.f), normal(0.f,1.f,0.f) {}
};

/*!
  \typedef SoProceduralHandlesCB
  \brief Callback that computes the set of interactive handles from current params.

  Called once per \c buildHandleDraggers() call and whenever the shape's
  \c params field changes while a live dragger set is active.

  \param params    Current parameter values (read-only).
  \param numParams Number of values in \a params.
  \param handles   [out] Vector to fill with handle descriptors.
  \param userdata  Opaque pointer supplied at registration time.
*/
typedef void (*SoProceduralHandlesCB)(const float*                     params,
                                      int                              numParams,
                                      std::vector<SoProceduralHandle>& handles,
                                      void*                            userdata);

/*!
  \typedef SoProceduralHandleDragCB
  \brief Callback invoked each time a handle dragger moves.

  The callback receives a mutable copy of the current \c params array.
  It must update \a params in-place to reflect the new handle position.
  Obol will then assign the modified array back to the shape's \c params field,
  causing the shape to redraw automatically.

  \param params       In/out: current parameter values; modify to reflect drag.
  \param numParams    Size of the \a params array.
  \param handleIndex  Index of the handle that moved (same ordering as returned
                      by the SoProceduralHandlesCB).
  \param oldPos       Handle position before this drag step (object space).
  \param newPos       Handle position after this drag step (object space).
  \param userdata     Opaque pointer supplied at registration time.
*/
typedef void (*SoProceduralHandleDragCB)(float*         params,
                                         int            numParams,
                                         int            handleIndex,
                                         const SbVec3f& oldPos,
                                         const SbVec3f& newPos,
                                         void*          userdata);

/*!
  \typedef SoProceduralHandleValidateCB
  \brief Optional callback that spatially clamps a proposed DRAG_NO_INTERSECT move.

  Called by buildHandleDraggers() sensor infrastructure whenever a handle
  with \c DRAG_NO_INTERSECT drag type is dragged.  It runs \e after
  SoProceduralObjectValidateCB (if registered) and \e before the delta is
  applied to \c params.

  This callback's purpose is \e positional clamping — constraining where the
  dragger handle may physically land (e.g., restricting a vertex to the upper
  hemisphere, or clamping a radius to a minimum).  Object-level validity
  decisions (e.g., "does this parameter combination create a degenerate solid?")
  should be implemented in SoProceduralObjectValidateCB instead.

  The callback should:
  - Return \c TRUE and leave \a acceptedPos unchanged if the position is valid.
  - Return \c FALSE and set \a acceptedPos to the nearest valid position to clamp.
  - Return \c FALSE and set \a acceptedPos equal to \a oldPos to fully reject.

  \param params         Current parameter values (read-only during validation).
  \param numParams      Number of elements in \a params.
  \param handleIndex    Index of the handle being dragged.
  \param oldPos         Handle position at drag start (object space).
  \param proposedNewPos Proposed new position (object space).
  \param acceptedPos    [out] Set to the position that will actually be applied.
                        Initialised to \a proposedNewPos on entry.
  \param userdata       Opaque pointer supplied at registration time.
  \return TRUE if \a proposedNewPos is fully accepted; FALSE if clamped/rejected.
*/
typedef SbBool (*SoProceduralHandleValidateCB)(const float*   params,
                                               int            numParams,
                                               int            handleIndex,
                                               const SbVec3f& oldPos,
                                               const SbVec3f& proposedNewPos,
                                               SbVec3f&       acceptedPos,
                                               void*          userdata);

/*!
  \typedef SoProceduralObjectValidateCB
  \brief Object-level validation callback called for every DRAG_NO_INTERSECT drag.

  SoProceduralShape calls this callback \e before applying any
  DRAG_NO_INTERSECT drag delta to the shape's \c params field.  The full
  proposed parameter state is serialised to a JSON object and passed to the
  callback.  The application parses the JSON using its own domain knowledge
  and decides whether the configuration is valid.

  \b JSON format of \a proposedParamsJSON:
  \code
  { "type": "ARB8",
    "params": { "v0x": -1.0, "v0y": -1.0, "v0z": -1.0,
                "v1x":  1.0, "v1y": -1.0, "v1z": -1.0, ... } }
  \endcode
  Parameter names are taken from the \b "params" section of the registered
  JSON schema.  If the schema has no named parameters, index-based keys are
  used instead ("p0", "p1", …).

  \param proposedParamsJSON  NUL-terminated JSON string encoding the proposed state.
  \param userdata            Opaque pointer supplied at registration time.
  \return TRUE to accept the move and update \c params; FALSE to reject it
          (the dragger will snap back to its start position).

  \sa SoProceduralHandleValidateCB (for positional clamping of handle positions)
*/
typedef SbBool (*SoProceduralObjectValidateCB)(const char* proposedParamsJSON,
                                               void*       userdata);

/*!
  \typedef SoProceduralSelectCB
  \brief Optional ray-based selection callback for custom hit-testing of handles.

  Called when the application wants to determine which handle a view-space ray
  hits (e.g., from a mouse click processed by the parent application).  The
  callback receives the pick ray in the shape's local (object) space and should
  return the 0-based index of the handle it considers hit, or \c -1 for no hit.

  This callback supplements the built-in vertex-proximity test used by
  buildSelectionDisplay().  Whether the built-in test runs first (as a fallback)
  or is skipped entirely is controlled by the \c useCustomSelectCB flag passed
  to setSelectCallback().

  \param rayOrigin   Origin of the pick ray in object (local) space.
  \param rayDir      Direction of the pick ray (need not be normalised).
  \param numHandles  Number of handles currently available on this shape.
  \param userdata    Opaque pointer supplied to setSelectCallback().
  \return 0-based index of the hit handle, or -1 for no hit.

  \sa SoProceduralShape::setSelectCallback()
  \sa SoProceduralShape::buildSelectionDisplay()
*/
typedef int (*SoProceduralSelectCB)(const SbVec3f& rayOrigin,
                                    const SbVec3f& rayDir,
                                    int            numHandles,
                                    void*          userdata);

// ---------------------------------------------------------------------------
// SoProceduralShape node
// ---------------------------------------------------------------------------

class SoSeparator;

class OBOL_DLL_API SoProceduralShape : public SoShape {
  typedef SoShape inherited;

  SO_NODE_HEADER(SoProceduralShape);

public:
  static void initClass();

  SoProceduralShape();

  // ----- Fields -----

  //! Registered shape type name (e.g. "TruncatedCone").
  SoSFString shapeType;

  //! JSON parameter-schema string (informational — see class docs).
  //! Populated automatically from the registry when \c shapeType is set
  //! via setShapeType(), or set manually by the application.
  SoSFString schemaJSON;

  //! Current parameter values, ordered as defined in the JSON schema.
  SoMFFloat  params;

  // ----- Convenience -----

  //! Set \c shapeType and automatically populate \c schemaJSON and
  //! \c params (with schema defaults) from the type registry.
  void setShapeType(const char* typeName);

  // ----- Registry -----

  /*!
    Register a new procedural shape type (geometry + bounding box only).

    \param typeName   Unique identifier string for this shape type.
    \param schemaJSON JSON string describing the editable parameters
                      (see class documentation for format).  Pass an
                      empty string if no schema is provided.
    \param bboxCB     Bounding-box computation callback.  Must not be null.
    \param geomCB     Geometry generation callback.  Must not be null.
    \param userdata   Opaque pointer passed through to all callbacks.
    \return TRUE on success; FALSE if \a typeName is already registered.
  */
  static SbBool registerShapeType(const char*          typeName,
                                  const char*          schemaJSON,
                                  SoProceduralBBoxCB   bboxCB,
                                  SoProceduralGeomCB   geomCB,
                                  void*                userdata = nullptr);

  /*!
    Register a new procedural shape type with interactive dragger handles.

    This overload adds two optional callbacks that Obol uses to create and
    wire SoDragger nodes when buildHandleDraggers() is called.  Shapes that
    do not need interactive editing can use the four-argument overload instead.

    \param handlesCB    Callback that computes handle positions from params.
                        May be \c nullptr if the JSON schema defines a
                        \b "handles" section with full topology.
    \param handleDragCB Callback that updates params when a handle is dragged.
                        May be \c nullptr when topology is present in the schema.
  */
  static SbBool registerShapeType(const char*              typeName,
                                  const char*              schemaJSON,
                                  SoProceduralBBoxCB       bboxCB,
                                  SoProceduralGeomCB       geomCB,
                                  SoProceduralHandlesCB    handlesCB,
                                  SoProceduralHandleDragCB handleDragCB,
                                  void*                    userdata = nullptr);

  /*!
    Register a new procedural shape type with all optional callbacks.

    This is the full registration overload.  All callback parameters may be
    \c nullptr; see SoProceduralHandleValidateCB and
    SoProceduralObjectValidateCB for their respective purposes.

    \param handleValidateCB  Optional per-position spatial clamping callback.
    \param objectValidateCB  Optional object-level JSON validation callback
                             called for every DRAG_NO_INTERSECT drag.
  */
  static SbBool registerShapeType(const char*                  typeName,
                                  const char*                  schemaJSON,
                                  SoProceduralBBoxCB           bboxCB,
                                  SoProceduralGeomCB           geomCB,
                                  SoProceduralHandlesCB        handlesCB,
                                  SoProceduralHandleDragCB     handleDragCB,
                                  SoProceduralHandleValidateCB handleValidateCB,
                                  SoProceduralObjectValidateCB objectValidateCB,
                                  void*                        userdata = nullptr);

  /*!
    Attach (or replace) the object-level validation callback for an already-
    registered shape type.

    This is a convenience setter for cases where the shape type is registered
    with the 4-argument overload (geometry only) and the validation callback
    is added separately, or where the callback needs to be changed at runtime.

    \param typeName        Registered shape type name.
    \param objectValidateCB  Callback to install; may be \c nullptr to remove.
    \param userdata          Opaque pointer passed to the callback.
    \return TRUE on success; FALSE if \a typeName is not registered.
  */
  static SbBool setObjectValidateCallback(const char*                  typeName,
                                          SoProceduralObjectValidateCB objectValidateCB,
                                          void*                        userdata = nullptr);

  //! Return the JSON schema string for a registered type, or \c nullptr.
  static const char* getSchemaJSON(const char* typeName);

  //! Return true if a shape type with this name has been registered.
  static SbBool isRegistered(const char* typeName);

  /*!
    Build and return a separator containing one SoDragger per registered handle.

    Handles come from the application's SoProceduralHandlesCB (if set) or,
    when topology has been parsed from the JSON schema, are auto-generated from
    the "handles" section.

    For DRAG_NO_INTERSECT handles, the sensor fires the registered
    SoProceduralObjectValidateCB with the proposed params JSON before applying
    the drag.  The SoProceduralHandleValidateCB (if set) runs afterward for
    positional clamping.  The shape redraws automatically when params change.

    Returns \c nullptr when no handles can be derived (no handle callbacks and
    no topology in the schema).

    Ownership: caller is responsible for ref-counting the returned separator.
  */
  SoSeparator* buildHandleDraggers();

  /*!
    Build and return a separator containing one visual marker per handle.

    Each marker consists of:
    - An SoTranslation placing it at the handle's computed position.
    - An SoSphere whose radius reflects the handle element type
      (vertex = 0.05, edge = 0.07, face = 0.10).
    - An SoText2 label showing the handle's name as defined in the JSON
      schema (the \b "name" field of each entry in the "handles" array),
      or an empty label when no name was provided.

    This separator is intended to be added to the scene graph alongside the
    shape to give the user visual targets for picking.  Picking logic can be
    supplied via setSelectCallback(); by default, proximity to vertex
    positions is used.

    Returns \c nullptr when no handles can be derived (no handle callbacks
    and no topology in the schema).

    Ownership: caller is responsible for ref-counting the returned separator.
  */
  SoSeparator* buildSelectionDisplay() const;

  /*!
    Register a ray-based selection callback for this shape instance.

    When the parent application receives a mouse-click event and wants to
    determine which handle was hit, it should call this function and later
    invoke its own pick logic.  The callback receives the pick ray in
    \e object (local) space and should return the 0-based handle index, or
    -1 for no hit.

    \param selectCB      The callback function; \c nullptr to remove.
    \param useCustomCB   If \c TRUE, the built-in vertex-proximity test is
                         skipped entirely and \a selectCB has full
                         responsibility.  If \c FALSE, the built-in test
                         runs first and \a selectCB is used only when the
                         built-in test returns -1.
    \param userdata      Opaque pointer forwarded to the callback.
  */
  void setSelectCallback(SoProceduralSelectCB selectCB,
                         SbBool               useCustomCB,
                         void*                userdata = nullptr);

  /*!
    Return the index of the handle hit by a pick ray in object (local) space.

    Applies the selection strategy set by setSelectCallback():
    - If \c useCustomCB is \c TRUE, only the application callback is used.
    - Otherwise, built-in vertex-proximity testing is tried first (within a
      squared distance of \a proximityThreshSq), and the application callback
      runs as a fallback only when the built-in test returns no hit.

    \param rayOrigin        Origin of the ray in object space.
    \param rayDir           Direction of the ray (need not be normalised).
    \param proximityThreshSq  Squared hit radius for the built-in vertex test.
    \return 0-based handle index, or -1 for no hit.
  */
  int pickHandle(const SbVec3f& rayOrigin,
                 const SbVec3f& rayDir,
                 float          proximityThreshSq = 0.01f) const;

  /*!
    Serialise the shape's current parameter values as a JSON string.

    The format is identical to the JSON passed to SoProceduralObjectValidateCB:
    \code
    { "type": "MyShape", "params": { "paramName": 1.23, ... } }
    \endcode
    Named parameters are used when the schema defines them; otherwise
    index-based keys ("p0", "p1", …) are substituted.

    The returned SbString is empty when no shape type has been set.

    \return JSON string encoding the current \c params field.
  */
  SbString getCurrentParamsJSON() const;

  virtual void getPrimitiveCount(SoGetPrimitiveCountAction* action) override;

protected:
  virtual ~SoProceduralShape();

  virtual void generatePrimitives(SoAction* action) override;
  virtual void computeBBox(SoAction* action, SbBox3f& box,
                           SbVec3f & center) override;

private:
  void emitTriangles(SoAction* action,
                     const SoProceduralTriangles& tris);
  void emitWireframe(SoAction* action,
                     const SoProceduralWireframe& wire);

  // Per-instance selection callback state
  SoProceduralSelectCB d_selectCB;
  SbBool               d_useCustomSelectCB;
  void*                d_selectUserdata;
};

#endif // !OBOL_SOPROCEDURALSHAPE_H
