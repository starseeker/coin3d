/*
 * arb8_dragger_example.cpp
 *
 * Self-contained demonstration of SoProceduralShape with interactive
 * SoDragger handles for parameter editing.
 *
 * The "ARB8" shape type is an arbitrary 8-vertex solid (a generalized
 * hexahedron whose faces are not required to be planar or rectangular).
 * It is the kind of primitive used in solid-modelling tools such as BRL-CAD.
 *
 * Shape parameters (24 floats = 8 vertices × 3 coordinates):
 *   params[ 0.. 2]  vertex 0 (x,y,z)
 *   params[ 3.. 5]  vertex 1
 *   ...
 *   params[21..23]  vertex 7
 *
 * Vertex ordering (bottom face CCW, top face CW when viewed from outside):
 *   Bottom: 0,1,2,3    Top: 4,5,6,7
 *
 * Three classes of interactive handles are demonstrated:
 *
 *   VERTEX handles (8, DRAG_POINT)
 *     Each vertex gets a SoDragPointDragger.  The user can pull any corner
 *     freely in 3-D space.  The drag callback updates the three floats that
 *     encode that vertex's xyz.
 *
 *   EDGE handles (12, DRAG_ON_PLANE — the face that the edge borders)
 *     Each edge gets a SoTranslate2Dragger at the edge midpoint.  Dragging
 *     translates both endpoint vertices by the same delta, keeping the edge
 *     length constant while moving the edge.
 *
 *   FACE handles (6, DRAG_ALONG_AXIS — the face normal)
 *     Each face gets a SoTranslate1Dragger at the face centre.  Dragging
 *     pushes or pulls all four face vertices along the outward face normal,
 *     uniformly extruding or retracting the face.
 *
 * Scene layout:
 *   Left    : ARB8 rendered as a filled solid (default box, no handles)
 *   Centre  : ARB8 with all vertex draggers visible (no pre-drag motion here;
 *              the draggers are placed but not animated)
 *   Right   : ARB8 with face handles illustrating the DRAG_ALONG_AXIS pattern
 *
 * Output: argv[1]+".rgb"  (800×600 SGI RGB, suitable for compare with PNG)
 */

#include "headless_utils.h"

#include <Inventor/nodes/SoProceduralShape.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoDrawStyle.h>

#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <array>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// ARB8 geometry helpers
// ============================================================================

// Standard face definitions (quad vertex indices, CCW from outside)
static const int kArb8Faces[6][4] = {
  {0,3,2,1},  // bottom  (-Y)
  {4,5,6,7},  // top     (+Y)
  {0,1,5,4},  // front   (-Z)
  {1,2,6,5},  // right   (+X)
  {2,3,7,6},  // back    (+Z)
  {3,0,4,7}   // left    (-X)
};

// Standard edge definitions (vertex index pairs)
static const int kArb8Edges[12][2] = {
  {0,1},{1,2},{2,3},{3,0},   // bottom ring
  {4,5},{5,6},{6,7},{7,4},   // top ring
  {0,4},{1,5},{2,6},{3,7}    // vertical
};

// Retrieve vertex i from params (3 floats per vertex)
static inline SbVec3f arb8Vertex(const float* params, int i)
{
  return SbVec3f(params[3*i], params[3*i+1], params[3*i+2]);
}

// Face centroid
static SbVec3f faceCentroid(const float* params, const int faceVerts[4])
{
  SbVec3f c(0,0,0);
  for (int k=0;k<4;++k) c += arb8Vertex(params, faceVerts[k]);
  return c * 0.25f;
}

// Face normal (approx, averaged from two triangle normals)
static SbVec3f faceNormal(const float* params, const int faceVerts[4])
{
  SbVec3f v0 = arb8Vertex(params, faceVerts[0]);
  SbVec3f v1 = arb8Vertex(params, faceVerts[1]);
  SbVec3f v2 = arb8Vertex(params, faceVerts[2]);
  SbVec3f v3 = arb8Vertex(params, faceVerts[3]);
  SbVec3f n = (v1-v0).cross(v2-v0) + (v2-v0).cross(v3-v0);
  n.normalize();
  return n;
}

// Default unit-box params (same as a 2×2×2 box centred at origin)
static const float kArb8Default[24] = {
  -1,-1,-1,   1,-1,-1,   1,-1, 1,  -1,-1, 1,   // bottom (y=-1)
  -1, 1,-1,   1, 1,-1,   1, 1, 1,  -1, 1, 1    // top    (y=+1)
};

// JSON schema with full topology.
//
// "params"   — 24 floats (8 vertices × xyz).
// "vertices" — maps each vertex name to its three param names.
// "faces"    — 6 quad faces with CCW winding viewed from outside.
//              "opposite" enables the face-crossing constraint.
// "handles"  — one handle per vertex (DRAG_NO_INTERSECT: free but validated),
//              one per edge (DRAG_ON_PLANE: plane derived from first adj face),
//              one per face (DRAG_NO_INTERSECT: free but cannot cross opposite).
//
// With this schema, registerShapeType() needs only bboxCB + geomCB.
// No handlesCB, handleDragCB, or validateCB are required.
static const char* kArb8Schema = R"({
  "type"  : "ARB8",
  "label" : "Arbitrary 8-Vertex Solid",
  "params": [
    {"name":"v0x","type":"float","default":-1.0,"label":"V0 X"},
    {"name":"v0y","type":"float","default":-1.0,"label":"V0 Y"},
    {"name":"v0z","type":"float","default":-1.0,"label":"V0 Z"},
    {"name":"v1x","type":"float","default": 1.0,"label":"V1 X"},
    {"name":"v1y","type":"float","default":-1.0,"label":"V1 Y"},
    {"name":"v1z","type":"float","default":-1.0,"label":"V1 Z"},
    {"name":"v2x","type":"float","default": 1.0,"label":"V2 X"},
    {"name":"v2y","type":"float","default":-1.0,"label":"V2 Y"},
    {"name":"v2z","type":"float","default": 1.0,"label":"V2 Z"},
    {"name":"v3x","type":"float","default":-1.0,"label":"V3 X"},
    {"name":"v3y","type":"float","default":-1.0,"label":"V3 Y"},
    {"name":"v3z","type":"float","default": 1.0,"label":"V3 Z"},
    {"name":"v4x","type":"float","default":-1.0,"label":"V4 X"},
    {"name":"v4y","type":"float","default": 1.0,"label":"V4 Y"},
    {"name":"v4z","type":"float","default":-1.0,"label":"V4 Z"},
    {"name":"v5x","type":"float","default": 1.0,"label":"V5 X"},
    {"name":"v5y","type":"float","default": 1.0,"label":"V5 Y"},
    {"name":"v5z","type":"float","default":-1.0,"label":"V5 Z"},
    {"name":"v6x","type":"float","default": 1.0,"label":"V6 X"},
    {"name":"v6y","type":"float","default": 1.0,"label":"V6 Y"},
    {"name":"v6z","type":"float","default": 1.0,"label":"V6 Z"},
    {"name":"v7x","type":"float","default":-1.0,"label":"V7 X"},
    {"name":"v7y","type":"float","default": 1.0,"label":"V7 Y"},
    {"name":"v7z","type":"float","default": 1.0,"label":"V7 Z"}
  ],
  "vertices": [
    {"name":"v0","x":"v0x","y":"v0y","z":"v0z"},
    {"name":"v1","x":"v1x","y":"v1y","z":"v1z"},
    {"name":"v2","x":"v2x","y":"v2y","z":"v2z"},
    {"name":"v3","x":"v3x","y":"v3y","z":"v3z"},
    {"name":"v4","x":"v4x","y":"v4y","z":"v4z"},
    {"name":"v5","x":"v5x","y":"v5y","z":"v5z"},
    {"name":"v6","x":"v6x","y":"v6y","z":"v6z"},
    {"name":"v7","x":"v7x","y":"v7y","z":"v7z"}
  ],
  "faces": [
    {"name":"bottom","verts":["v0","v3","v2","v1"],"opposite":"top"   },
    {"name":"top",   "verts":["v4","v5","v6","v7"],"opposite":"bottom"},
    {"name":"front", "verts":["v0","v1","v5","v4"],"opposite":"back"  },
    {"name":"right", "verts":["v1","v2","v6","v5"],"opposite":"left"  },
    {"name":"back",  "verts":["v2","v3","v7","v6"],"opposite":"front" },
    {"name":"left",  "verts":["v3","v0","v4","v7"],"opposite":"right" }
  ],
  "handles": [
    {"name":"v0_h","vertex":"v0","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v1_h","vertex":"v1","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v2_h","vertex":"v2","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v3_h","vertex":"v3","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v4_h","vertex":"v4","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v5_h","vertex":"v5","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v6_h","vertex":"v6","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v7_h","vertex":"v7","dragType":"DRAG_NO_INTERSECT"},
    {"name":"e01_h","edge":["v0","v1"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e12_h","edge":["v1","v2"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e23_h","edge":["v2","v3"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e30_h","edge":["v3","v0"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e45_h","edge":["v4","v5"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e56_h","edge":["v5","v6"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e67_h","edge":["v6","v7"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e74_h","edge":["v7","v4"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e04_h","edge":["v0","v4"],"dragType":"DRAG_NO_INTERSECT"},
    {"name":"e15_h","edge":["v1","v5"],"dragType":"DRAG_NO_INTERSECT"},
    {"name":"e26_h","edge":["v2","v6"],"dragType":"DRAG_NO_INTERSECT"},
    {"name":"e37_h","edge":["v3","v7"],"dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_bot_h", "face":"bottom","dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_top_h", "face":"top",   "dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_frt_h", "face":"front", "dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_rgt_h", "face":"right", "dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_bak_h", "face":"back",  "dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_lft_h", "face":"left",  "dragType":"DRAG_NO_INTERSECT"}
  ]
})";

// ============================================================================
// Bbox callback
// ============================================================================
static void arb8_bbox(const float* params, int numParams,
                      SbVec3f& minPt, SbVec3f& maxPt, void*)
{
  if (numParams < 24) { minPt.setValue(-1,-1,-1); maxPt.setValue(1,1,1); return; }
  float mnx=params[0],mny=params[1],mnz=params[2];
  float mxx=params[0],mxy=params[1],mxz=params[2];
  for (int i=1;i<8;++i) {
    if (params[3*i  ]<mnx) mnx=params[3*i  ];
    if (params[3*i+1]<mny) mny=params[3*i+1];
    if (params[3*i+2]<mnz) mnz=params[3*i+2];
    if (params[3*i  ]>mxx) mxx=params[3*i  ];
    if (params[3*i+1]>mxy) mxy=params[3*i+1];
    if (params[3*i+2]>mxz) mxz=params[3*i+2];
  }
  minPt.setValue(mnx,mny,mnz);
  maxPt.setValue(mxx,mxy,mxz);
}

// ============================================================================
// Geometry callback
// ============================================================================
static void arb8_geom(const float* params, int numParams,
                      SoProceduralTriangles* tris,
                      SoProceduralWireframe* wire,
                      void*)
{
  // Fall back to a unit box if params are missing
  const float* p = (numParams >= 24) ? params : kArb8Default;

  if (tris) {
    tris->vertices.clear(); tris->normals.clear(); tris->indices.clear();

    // Emit 2 triangles per quad face (12 triangles total)
    for (int f=0;f<6;++f) {
      const int* fv = kArb8Faces[f];
      SbVec3f v0 = arb8Vertex(p, fv[0]);
      SbVec3f v1 = arb8Vertex(p, fv[1]);
      SbVec3f v2 = arb8Vertex(p, fv[2]);
      SbVec3f v3 = arb8Vertex(p, fv[3]);
      SbVec3f nrm = (v1-v0).cross(v2-v0); nrm.normalize();

      int base = static_cast<int>(tris->vertices.size());
      tris->vertices.insert(tris->vertices.end(), {v0,v1,v2,v3});
      tris->normals .insert(tris->normals .end(), {nrm,nrm,nrm,nrm});
      tris->indices .insert(tris->indices.end(),
        {base,base+1,base+2, base,base+2,base+3});
    }
  }

  if (wire) {
    wire->vertices.clear(); wire->segments.clear();
    for (int i=0;i<8;++i) wire->vertices.push_back(arb8Vertex(p,i));
    for (int e=0;e<12;++e) {
      wire->segments.push_back(kArb8Edges[e][0]);
      wire->segments.push_back(kArb8Edges[e][1]);
    }
  }
}

// ============================================================================
// Object-level validation callback (SoProceduralObjectValidateCB)
//
// SoProceduralShape calls this with a JSON string encoding the proposed new
// parameter state before any DRAG_NO_INTERSECT drag is applied.  The app
// is responsible for parsing that JSON and deciding whether the configuration
// is valid.  Returning FALSE snaps the dragger back to its start position.
//
// In this example we implement a face-flip (self-intersection) check: for
// each of the six faces we verify that its outward normal still points away
// from the solid's centroid.  The application owns this logic — it can be as
// simple or as sophisticated as needed (e.g., checking convexity, minimum
// wall thickness, clearance constraints, etc.).
//
// The JSON format Obol sends:
//   { "type": "ARB8",
//     "params": { "v0x": -1.0, "v0y": -1.0, "v0z": -1.0, ... } }
// ============================================================================

// Minimal parser for the specific params JSON Obol sends.
// We only need to extract "v0x"…"v7z" as floats (24 values).
static bool parseArb8ProposedParams(const char* json, float out[24])
{
  // Locate "params":{...} and scan for "vNa": value pairs.
  const char* p = strstr(json, "\"params\"");
  if (!p) return false;
  p = strchr(p, '{');
  if (!p) return false;

  // Parse key:value pairs inside the params object.
  // Keys look like "v0x", "v0y", "v0z", "v1x", … "v7z"
  while (*p && *p != '}') {
    // Find next '"'
    const char* qs = strchr(p, '"');
    if (!qs || *qs == '}') break;
    ++qs;
    const char* qe = strchr(qs, '"');
    if (!qe) break;
    std::string key(qs, static_cast<size_t>(qe - qs));
    p = qe + 1;

    // Skip : and whitespace
    while (*p && (*p == ':' || isspace((unsigned char)*p))) ++p;

    // Parse number
    char* endp = nullptr;
    float val = strtof(p, &endp);
    if (!endp || endp == p) { p++; continue; }
    p = endp;

    // Map key to param index: "vNa" where N=0..7, a=x/y/z
    if (key.size() == 3 && key[0] == 'v') {
      int vn = key[1] - '0';
      int ax = (key[2]=='x') ? 0 : (key[2]=='y') ? 1 : (key[2]=='z') ? 2 : -1;
      if (vn >= 0 && vn <= 7 && ax >= 0)
        out[3*vn + ax] = val;
    }
    while (*p && (*p == ',' || isspace((unsigned char)*p))) ++p;
  }
  return true;
}

static SbBool arb8ObjectValidate(const char* proposedParamsJSON, void*)
{
  float verts[24];
  // Initialise to current default so partially-parsed arrays still work.
  const float kDefault[24] = {
    -1,-1,-1, 1,-1,-1, 1,-1,1,-1,-1,1,
    -1, 1,-1, 1, 1,-1, 1, 1,1,-1, 1,1
  };
  memcpy(verts, kDefault, sizeof(verts));

  if (!parseArb8ProposedParams(proposedParamsJSON, verts))
    return TRUE; // parse error → be permissive

  // Compute centroid of all 8 vertices
  SbVec3f centroid(0,0,0);
  for (int i = 0; i < 8; ++i)
    centroid += SbVec3f(verts[3*i], verts[3*i+1], verts[3*i+2]);
  centroid /= 8.f;

  // Check that each face's outward normal still points away from the centroid.
  for (int f = 0; f < 6; ++f) {
    const int* fv = kArb8Faces[f];
    SbVec3f v0(verts[3*fv[0]], verts[3*fv[0]+1], verts[3*fv[0]+2]);
    SbVec3f v1(verts[3*fv[1]], verts[3*fv[1]+1], verts[3*fv[1]+2]);
    SbVec3f v2(verts[3*fv[2]], verts[3*fv[2]+1], verts[3*fv[2]+2]);
    SbVec3f nrm = (v1 - v0).cross(v2 - v0);
    // If dot(outward_normal, v0-centroid) <= kEpsilon, the face has flipped → reject
    static const float kFaceOrientationEpsilon = -1e-6f;
    if (nrm.dot(v0 - centroid) <= kFaceOrientationEpsilon)
      return FALSE;
  }
  return TRUE; // all faces still face outward — accept
}

// ============================================================================
// Build a scene node for one ARB8 instance with optional dragger handles
// ============================================================================
static SoSeparator* makeArb8Scene(const char* typeTag,
                                   const float* paramVals,
                                   float r, float g, float b,
                                   float tx,
                                   bool withHandles)
{
  SoSeparator* sep = new SoSeparator;

  SoTranslation* t = new SoTranslation;
  t->translation.setValue(tx, 0.f, 0.f);
  sep->addChild(t);

  // Solid shape
  SoSeparator* shapeSep = new SoSeparator;
  SoMaterial* mat = new SoMaterial;
  mat->diffuseColor.setValue(r, g, b);
  mat->specularColor.setValue(0.4f,0.4f,0.4f);
  mat->shininess.setValue(0.5f);
  shapeSep->addChild(mat);

  SoProceduralShape* shape = new SoProceduralShape;
  shape->shapeType.setValue(typeTag);
  shape->params.setValues(0, 24, paramVals);
  shapeSep->addChild(shape);
  sep->addChild(shapeSep);

  // Wireframe overlay so faces are visible even without lighting
  SoSeparator* wireSep = new SoSeparator;
  SoDrawStyle* ds = new SoDrawStyle;
  ds->style.setValue(SoDrawStyle::LINES);
  wireSep->addChild(ds);
  SoMaterial* wireMat = new SoMaterial;
  wireMat->diffuseColor.setValue(0.1f,0.1f,0.1f);
  wireSep->addChild(wireMat);
  SoProceduralShape* wireShape = new SoProceduralShape;
  wireShape->shapeType.setValue(typeTag);
  wireShape->params.setValues(0, 24, paramVals);
  wireSep->addChild(wireShape);
  sep->addChild(wireSep);

  // Optionally build dragger handles
  if (withHandles) {
    SoSeparator* handles = shape->buildHandleDraggers();
    if (handles) {
      sep->addChild(handles);
    }
  }

  return sep;
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char** argv)
{
  initCoinHeadless();

  // Register the ARB8 type.
  // With JSON topology in kArb8Schema, only bboxCB + geomCB are required.
  // SoProceduralShape auto-generates handles from the "handles" section.
  SoProceduralShape::registerShapeType(
      "ARB8", kArb8Schema,
      arb8_bbox, arb8_geom);

  // Attach the object-level validation callback.
  // This is the general constraint mechanism: for every DRAG_NO_INTERSECT drag
  // Obol serialises the proposed parameter state as a JSON object and calls
  // this function.  The application parses the JSON and returns TRUE (accept)
  // or FALSE (reject / snap back).  The shape library (Obol) does not need to
  // know anything about what makes an ARB8 valid — that knowledge lives here,
  // in the application that owns the shape type.
  SoProceduralShape::setObjectValidateCallback("ARB8", arb8ObjectValidate);

  SoSeparator* root = new SoSeparator;
  root->ref();

  SoPerspectiveCamera* cam = new SoPerspectiveCamera;
  root->addChild(cam);

  SoDirectionalLight* light = new SoDirectionalLight;
  light->direction.setValue(-0.5f, -0.8f, -0.6f);
  root->addChild(light);

  // A slightly sheared box to show non-rectangular faces
  float sheared[24];
  memcpy(sheared, kArb8Default, sizeof(sheared));
  // Tilt the top face: shift top vertices by 0.3 in X
  for (int i=4;i<8;++i) sheared[3*i] += 0.4f;

  // Left: plain solid ARB8 (default box params)
  root->addChild(makeArb8Scene("ARB8", kArb8Default,
                               0.2f, 0.5f, 0.9f, -3.5f, false));

  // Centre: sheared ARB8 with vertex draggers
  root->addChild(makeArb8Scene("ARB8", sheared,
                               0.9f, 0.5f, 0.2f,  0.0f, true));

  // Right: same sheared ARB8 with wireframe only
  SoSeparator* rightSep = new SoSeparator;
  SoTranslation* rt = new SoTranslation;
  rt->translation.setValue(3.5f, 0.f, 0.f);
  rightSep->addChild(rt);
  SoDrawStyle* rds = new SoDrawStyle;
  rds->style.setValue(SoDrawStyle::LINES);
  rightSep->addChild(rds);
  SoMaterial* rMat = new SoMaterial;
  rMat->diffuseColor.setValue(0.1f, 0.9f, 0.3f);
  rightSep->addChild(rMat);
  SoProceduralShape* rShape = new SoProceduralShape;
  rShape->shapeType.setValue("ARB8");
  rShape->params.setValues(0, 24, sheared);
  rightSep->addChild(rShape);
  root->addChild(rightSep);

  SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
  cam->viewAll(root, vp);
  cam->position.setValue(cam->position.getValue() * 1.3f);

  char outpath[1024];
  if (argc > 1)
    snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
  else
    snprintf(outpath, sizeof(outpath), "arb8_dragger_example.rgb");

  bool ok = renderToFile(root, outpath);
  root->unref();
  return ok ? 0 : 1;
}
