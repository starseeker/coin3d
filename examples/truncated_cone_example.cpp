/*
 * truncated_cone_example.cpp
 *
 * Self-contained demonstration of SoProceduralShape using a truncated general
 * cone (frustum) as the custom geometry type.
 *
 * This example:
 *   1. Defines bbox and geometry callbacks for a "TruncatedCone" shape type.
 *   2. Registers the type with Obol via SoProceduralShape::registerShapeType().
 *   3. Builds a scene graph containing two TruncatedCone instances:
 *        Left:  rendered as a filled solid (FILLED draw style)
 *        Right: rendered as a wireframe (LINES draw style via SoDrawStyle)
 *   4. Renders to an SGI RGB file (argv[1]+".rgb") using the headless backend.
 *
 * JSON parameter schema for "TruncatedCone":
 *   params[0]  bottomRadius  float  default 1.0
 *   params[1]  topRadius     float  default 0.5
 *   params[2]  height        float  default 2.0
 *   params[3]  sides         int    default 16
 *
 * Compile as part of the Obol test suite via tests/rendering/CMakeLists.txt.
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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Parameter indices (must match schema order)
// ============================================================================
enum TruncConeParam {
  P_BOTTOM_RADIUS = 0,
  P_TOP_RADIUS    = 1,
  P_HEIGHT        = 2,
  P_SIDES         = 3,
  P_COUNT
};

// JSON schema describing the TruncatedCone parameters.
// This string is stored in SoProceduralShape::schemaJSON and can be used by
// host-application editors to auto-generate parameter UI at runtime.
static const char* kTruncConeSchema = R"({
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
})";

// ============================================================================
// Bbox callback
// ============================================================================
static void
truncCone_bbox(const float* params, int numParams,
               SbVec3f& minPt, SbVec3f& maxPt,
               void* /*userdata*/)
{
  float rb = (numParams > P_BOTTOM_RADIUS) ? params[P_BOTTOM_RADIUS] : 1.0f;
  float rt = (numParams > P_TOP_RADIUS)    ? params[P_TOP_RADIUS]    : 0.5f;
  float h  = (numParams > P_HEIGHT)        ? params[P_HEIGHT]        : 2.0f;

  float rmax = (rb > rt) ? rb : rt;
  minPt.setValue(-rmax, -h * 0.5f, -rmax);
  maxPt.setValue( rmax,  h * 0.5f,  rmax);
}

// ============================================================================
// Geometry callback
// ============================================================================
static void
truncCone_geom(const float* params, int numParams,
               SoProceduralTriangles* tris,
               SoProceduralWireframe* wire,
               void* /*userdata*/)
{
  const float rb    = (numParams > P_BOTTOM_RADIUS) ? params[P_BOTTOM_RADIUS] : 1.0f;
  const float rt    = (numParams > P_TOP_RADIUS)    ? params[P_TOP_RADIUS]    : 0.5f;
  const float h     = (numParams > P_HEIGHT)        ? params[P_HEIGHT]        : 2.0f;
  const int   sides = (numParams > P_SIDES)
                        ? static_cast<int>(params[P_SIDES]) : 16;
  const int   n     = (sides < 3) ? 3 : sides;

  const float yBot = -h * 0.5f;
  const float yTop =  h * 0.5f;

  // --- Build the side-wall geometry ---
  //
  // Vertex layout:
  //   0 .. n-1   bottom ring
  //   n .. 2n-1  top ring

  if (tris) {
    tris->vertices.clear();
    tris->normals.clear();
    tris->indices.clear();

    // Side-wall vertices
    for (int i = 0; i < n; ++i) {
      float angle = static_cast<float>(2.0 * M_PI * i / n);
      float cx = cosf(angle);
      float cz = sinf(angle);
      tris->vertices.push_back(SbVec3f(rb * cx, yBot, rb * cz));
    }
    for (int i = 0; i < n; ++i) {
      float angle = static_cast<float>(2.0 * M_PI * i / n);
      float cx = cosf(angle);
      float cz = sinf(angle);
      tris->vertices.push_back(SbVec3f(rt * cx, yTop, rt * cz));
    }

    // Side normals: outward-pointing, slanted by the taper
    float slope = (rb - rt) / h;   // dr/dy (positive = widens toward bottom)
    for (int i = 0; i < n; ++i) {
      float angle = static_cast<float>(2.0 * M_PI * i / n);
      float cx = cosf(angle);
      float cz = sinf(angle);
      SbVec3f nrm(cx, slope, cz);
      nrm.normalize();
      tris->normals.push_back(nrm); // bottom ring vertex
    }
    for (int i = 0; i < n; ++i) {
      float angle = static_cast<float>(2.0 * M_PI * i / n);
      float cx = cosf(angle);
      float cz = sinf(angle);
      SbVec3f nrm(cx, slope, cz);
      nrm.normalize();
      tris->normals.push_back(nrm); // top ring vertex
    }

    // Side triangles: two per quad strip segment
    for (int i = 0; i < n; ++i) {
      int i0b = i;
      int i1b = (i + 1) % n;
      int i0t = n + i;
      int i1t = n + (i + 1) % n;

      // Triangle 1
      tris->indices.push_back(i0b);
      tris->indices.push_back(i1b);
      tris->indices.push_back(i0t);
      // Triangle 2
      tris->indices.push_back(i1b);
      tris->indices.push_back(i1t);
      tris->indices.push_back(i0t);
    }

    // Bottom cap (if rb > 0)
    if (rb > 0.0f) {
      int capCenter = static_cast<int>(tris->vertices.size());
      tris->vertices.push_back(SbVec3f(0.0f, yBot, 0.0f));
      tris->normals .push_back(SbVec3f(0.0f, -1.0f, 0.0f));
      for (int i = 0; i < n; ++i) {
        tris->indices.push_back(capCenter);
        tris->indices.push_back((i + 1) % n);
        tris->indices.push_back(i);
      }
    }

    // Top cap (if rt > 0)
    if (rt > 0.0f) {
      int capCenter = static_cast<int>(tris->vertices.size());
      tris->vertices.push_back(SbVec3f(0.0f, yTop, 0.0f));
      tris->normals .push_back(SbVec3f(0.0f, 1.0f, 0.0f));
      for (int i = 0; i < n; ++i) {
        tris->indices.push_back(capCenter);
        tris->indices.push_back(n + i);
        tris->indices.push_back(n + (i + 1) % n);
      }
    }
  }

  if (wire) {
    wire->vertices.clear();
    wire->segments.clear();

    // Bottom ring vertices: 0..n-1
    // Top ring vertices:    n..2n-1
    for (int i = 0; i < n; ++i) {
      float angle = static_cast<float>(2.0 * M_PI * i / n);
      wire->vertices.push_back(SbVec3f(rb * cosf(angle), yBot, rb * sinf(angle)));
    }
    for (int i = 0; i < n; ++i) {
      float angle = static_cast<float>(2.0 * M_PI * i / n);
      wire->vertices.push_back(SbVec3f(rt * cosf(angle), yTop, rt * sinf(angle)));
    }

    // Bottom ring edges
    for (int i = 0; i < n; ++i) {
      wire->segments.push_back(i);
      wire->segments.push_back((i + 1) % n);
    }
    // Top ring edges
    for (int i = 0; i < n; ++i) {
      wire->segments.push_back(n + i);
      wire->segments.push_back(n + (i + 1) % n);
    }
    // Vertical (lateral) edges — use every 4th edge for clarity
    int step = (n >= 12) ? n / 8 : 1;
    for (int i = 0; i < n; i += step) {
      wire->segments.push_back(i);
      wire->segments.push_back(n + i);
    }
  }
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char** argv)
{
  initCoinHeadless();

  // Register the TruncatedCone shape type once at application startup.
  SoProceduralShape::registerShapeType("TruncatedCone",
                                       kTruncConeSchema,
                                       truncCone_bbox,
                                       truncCone_geom);

  // Build the scene graph.
  SoSeparator* root = new SoSeparator;
  root->ref();

  SoPerspectiveCamera* cam = new SoPerspectiveCamera;
  root->addChild(cam);

  SoDirectionalLight* light = new SoDirectionalLight;
  light->direction.setValue(-0.5f, -0.8f, -0.6f);
  root->addChild(light);

  // ---- Left: solid filled cone ----
  {
    SoSeparator* sep = new SoSeparator;

    SoTranslation* t = new SoTranslation;
    t->translation.setValue(-2.0f, 0.0f, 0.0f);
    sep->addChild(t);

    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor .setValue(0.2f, 0.5f, 0.9f);
    mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
    mat->shininess    .setValue(0.6f);
    sep->addChild(mat);

    SoProceduralShape* shape = new SoProceduralShape;
    shape->setShapeType("TruncatedCone");
    // params populated with defaults via setShapeType(); override if desired:
    //   [0] bottomRadius=1.0, [1] topRadius=0.5, [2] height=2.0, [3] sides=16

    sep->addChild(shape);
    root->addChild(sep);
  }

  // ---- Right: same cone rendered as wireframe ----
  {
    SoSeparator* sep = new SoSeparator;

    SoTranslation* t = new SoTranslation;
    t->translation.setValue(2.0f, 0.0f, 0.0f);
    sep->addChild(t);

    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.9f, 0.6f, 0.1f);
    sep->addChild(mat);

    SoDrawStyle* ds = new SoDrawStyle;
    ds->style.setValue(SoDrawStyle::LINES);
    sep->addChild(ds);

    SoProceduralShape* shape = new SoProceduralShape;
    shape->setShapeType("TruncatedCone");
    // Custom params: taller, fewer sides, for visual distinction
    float p[] = { 1.2f, 0.0f, 3.0f, 8.0f };
    shape->params.setValues(0, 4, p);

    sep->addChild(shape);
    root->addChild(sep);
  }

  // Frame the scene with the camera.
  SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
  cam->viewAll(root, vp);
  cam->position.setValue(cam->position.getValue() * 1.2f);

  // Determine output path.
  char outpath[1024];
  if (argc > 1)
    snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
  else
    snprintf(outpath, sizeof(outpath), "truncated_cone_example.rgb");

  bool ok = renderToFile(root, outpath);
  root->unref();
  return ok ? 0 : 1;
}
