/*
 * render_procedural_shape.cpp
 *
 * Visual regression test for SoProceduralShape: renders a solid filled
 * truncated cone (left) and a wireframe truncated cone (right).
 *
 * Parameters for "TruncatedCone":
 *   [0] bottomRadius  [1] topRadius  [2] height  [3] sides
 *
 * When compiled with OBOL_NANORT_BUILD the test additionally validates that
 * the wireframe cone (rendered as thin cylinder proxies) produces orange-toned
 * pixels in the right half of the image, confirming that the cylinder-proxy
 * path in SoProceduralShape::generatePrimitives() is working correctly.
 *
 * Output: argv[1]+".rgb"
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"

#include <Inventor/nodes/SoProceduralShape.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoDrawStyle.h>

#include <cstdio>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---- Parameter indices ----
enum { P_BOT=0, P_TOP=1, P_H=2, P_SIDES=3 };

static const char* kSchema = R"({
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

static void cone_bbox(const float* p, int n,
                      SbVec3f& mn, SbVec3f& mx, void*)
{
  float rb = (n>P_BOT)   ? p[P_BOT]   : 1.0f;
  float rt = (n>P_TOP)   ? p[P_TOP]   : 0.5f;
  float h  = (n>P_H)     ? p[P_H]     : 2.0f;
  float r  = (rb>rt) ? rb : rt;
  mn.setValue(-r, -h*0.5f, -r);
  mx.setValue( r,  h*0.5f,  r);
}

static void cone_geom(const float* p, int n,
                      SoProceduralTriangles* tris,
                      SoProceduralWireframe* wire,
                      void*)
{
  float rb    = (n>P_BOT)   ? p[P_BOT]   : 1.0f;
  float rt    = (n>P_TOP)   ? p[P_TOP]   : 0.5f;
  float h     = (n>P_H)     ? p[P_H]     : 2.0f;
  int   sides = (n>P_SIDES) ? (int)p[P_SIDES] : 16;
  if (sides < 3) sides = 3;

  const float yb = -h*0.5f;
  const float yt =  h*0.5f;

  if (tris) {
    tris->vertices.clear(); tris->normals.clear(); tris->indices.clear();

    for (int i=0;i<sides;++i) {
      float a=(float)(2.0*M_PI*i/sides);
      tris->vertices.push_back(SbVec3f(rb*cosf(a),yb,rb*sinf(a)));
    }
    for (int i=0;i<sides;++i) {
      float a=(float)(2.0*M_PI*i/sides);
      tris->vertices.push_back(SbVec3f(rt*cosf(a),yt,rt*sinf(a)));
    }
    float sl=(rb-rt)/h;
    for (int i=0;i<sides*2;++i) {
      float a=(float)(2.0*M_PI*(i%sides)/sides);
      SbVec3f nrm(cosf(a),sl,sinf(a)); nrm.normalize();
      tris->normals.push_back(nrm);
    }
    for (int i=0;i<sides;++i) {
      int i0=i, i1=(i+1)%sides, t0=sides+i, t1=sides+(i+1)%sides;
      tris->indices.push_back(i0); tris->indices.push_back(i1);
      tris->indices.push_back(t0);
      tris->indices.push_back(i1); tris->indices.push_back(t1);
      tris->indices.push_back(t0);
    }
    if (rb>0.0f) {
      int cc=(int)tris->vertices.size();
      tris->vertices.push_back(SbVec3f(0,yb,0));
      tris->normals .push_back(SbVec3f(0,-1,0));
      for (int i=0;i<sides;++i) {
        tris->indices.push_back(cc);
        tris->indices.push_back((i+1)%sides);
        tris->indices.push_back(i);
      }
    }
    if (rt>0.0f) {
      int cc=(int)tris->vertices.size();
      tris->vertices.push_back(SbVec3f(0,yt,0));
      tris->normals .push_back(SbVec3f(0,1,0));
      for (int i=0;i<sides;++i) {
        tris->indices.push_back(cc);
        tris->indices.push_back(sides+i);
        tris->indices.push_back(sides+(i+1)%sides);
      }
    }
  }

  if (wire) {
    wire->vertices.clear(); wire->segments.clear();
    for (int i=0;i<sides;++i) {
      float a=(float)(2.0*M_PI*i/sides);
      wire->vertices.push_back(SbVec3f(rb*cosf(a),yb,rb*sinf(a)));
    }
    for (int i=0;i<sides;++i) {
      float a=(float)(2.0*M_PI*i/sides);
      wire->vertices.push_back(SbVec3f(rt*cosf(a),yt,rt*sinf(a)));
    }
    for (int i=0;i<sides;++i) {
      wire->segments.push_back(i); wire->segments.push_back((i+1)%sides);
    }
    for (int i=0;i<sides;++i) {
      wire->segments.push_back(sides+i);
      wire->segments.push_back(sides+(i+1)%sides);
    }
    int step=(sides>=12)?sides/8:1;
    for (int i=0;i<sides;i+=step) {
      wire->segments.push_back(i); wire->segments.push_back(sides+i);
    }
  }
}

#ifdef OBOL_NANORT_BUILD
// ---- colour-detection thresholds for the NanoRT wireframe validation -------
// Matches the wireframe cone material: diffuseColor(0.9, 0.6, 0.1) → orange.
// After Phong shading the channel values are scaled, so we use broad bounds.
static const int ORANGE_MIN_RED   = 100;  // R dominant
static const int ORANGE_MIN_GREEN =  30;  // G moderate
static const int ORANGE_MAX_BLUE  =  80;  // B low
#endif

int main(int argc, char** argv)
{
  initCoinHeadless();

  SoProceduralShape::registerShapeType("TruncatedCone_render",
                                       kSchema, cone_bbox, cone_geom);

  SoSeparator* root = new SoSeparator;
  root->ref();

  SoPerspectiveCamera* cam = new SoPerspectiveCamera;
  root->addChild(cam);

  SoDirectionalLight* light = new SoDirectionalLight;
  light->direction.setValue(-0.5f, -0.8f, -0.6f);
  root->addChild(light);

  // Left: solid filled cone
  {
    SoSeparator* sep = new SoSeparator;
    SoTranslation* t = new SoTranslation;
    t->translation.setValue(-2.0f, 0.0f, 0.0f);
    sep->addChild(t);
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.2f, 0.5f, 0.9f);
    mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
    mat->shininess.setValue(0.6f);
    sep->addChild(mat);
    SoProceduralShape* shape = new SoProceduralShape;
    shape->setShapeType("TruncatedCone_render");
    sep->addChild(shape);
    root->addChild(sep);
  }

  // Right: wireframe
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
    shape->setShapeType("TruncatedCone_render");
    float p[] = { 1.2f, 0.0f, 3.0f, 8.0f };
    shape->params.setValues(0, 4, p);
    sep->addChild(shape);
    root->addChild(sep);
  }

  SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
  cam->viewAll(root, vp);
  cam->position.setValue(cam->position.getValue() * 1.2f);

  char outpath[1024];
  if (argc > 1)
    snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
  else
    snprintf(outpath, sizeof(outpath), "render_procedural_shape.rgb");


    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createProceduralShape(256, 256);
        SbViewportRegion fVp(256, 256);
        SoOffscreenRenderer fRen(fVp);
        fRen.setComponents(SoOffscreenRenderer::RGB);
        fRen.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        if (fRen.render(fRoot)) {
            fRen.writeToRGB(outpath);
        }
        fRoot->unref();
    }
  bool ok = renderToFile(root, outpath);

#ifdef OBOL_NANORT_BUILD
  // Validate that the wireframe cone (orange, right half of image) produced
  // visible cylinder-proxy geometry.  The orange diffuse material produces
  // pixels with high R, medium G, and low B (after Phong shading).
  if (ok) {
    const unsigned char* buf = getSharedRenderer()->getBuffer();
    if (buf) {
      const int W = DEFAULT_WIDTH;
      const int H = DEFAULT_HEIGHT;
      int orangeCount = 0;
      // Check the right half of the image for orange-toned pixels.
      for (int y = 0; y < H; ++y) {
        for (int x = W / 2; x < W; ++x) {
          const unsigned char* p = buf + (y * W + x) * 3;
          if (p[0] > ORANGE_MIN_RED && p[1] > ORANGE_MIN_GREEN &&
              p[2] < ORANGE_MAX_BLUE && p[0] > p[1])
            ++orangeCount;
        }
      }
      printf("render_procedural_shape (nanort): wireframe orange pixels=%d\n",
             orangeCount);
      if (orangeCount == 0) {
        fprintf(stderr, "render_procedural_shape: FAIL - no wireframe (orange) "
                "cylinder-proxy pixels found in right half\n");
        ok = false;
      } else {
        printf("render_procedural_shape: PASS - wireframe cylinder proxies visible\n");
      }
    }
  }
#endif

  root->unref();
  return ok ? 0 : 1;
}
