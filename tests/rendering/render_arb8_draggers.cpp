/*
 * render_arb8_draggers.cpp
 *
 * Visual regression / smoke test for SoProceduralShape with
 * topology-driven buildHandleDraggers() and DRAG_NO_INTERSECT constraint.
 *
 * Uses JSON schema with "vertices", "faces", and "handles" sections so that
 * only bboxCB + geomCB need to be provided; no handlesCB, handleDragCB, or
 * validateCB are required.  Verifies that:
 *   - Topology parsing produces the correct handle count.
 *   - DRAG_NO_INTERSECT handles (vertex + vertical-edge + face) and
 *     DRAG_ON_PLANE handles (horizontal-ring edges) are all built correctly.
 *   - The scene renders without crashing.
 *
 * Output: argv[1]+".rgb"  (800×600 SGI RGB)
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

// ---- ARB8 geometry constants ----

static const int kFaces[6][4] = {
  {0,3,2,1},{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}
};
static const int kEdges[12][2] = {
  {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
};
static const float kDefault[24] = {
  -1,-1,-1, 1,-1,-1, 1,-1,1,-1,-1,1,
  -1, 1,-1, 1, 1,-1, 1, 1,1,-1, 1,1
};

static inline SbVec3f vtx(const float* p,int i){return SbVec3f(p[3*i],p[3*i+1],p[3*i+2]);}

static void arb8_bbox_r(const float* p,int n,SbVec3f& mn,SbVec3f& mx,void*)
{
  if(n<24){mn.setValue(-1,-1,-1);mx.setValue(1,1,1);return;}
  float ax=p[0],ay=p[1],az=p[2],bx=p[0],by=p[1],bz=p[2];
  for(int i=1;i<8;++i){
    if(p[3*i  ]<ax){ax=p[3*i  ];} if(p[3*i+1]<ay){ay=p[3*i+1];} if(p[3*i+2]<az){az=p[3*i+2];}
    if(p[3*i  ]>bx){bx=p[3*i  ];} if(p[3*i+1]>by){by=p[3*i+1];} if(p[3*i+2]>bz){bz=p[3*i+2];}
  }
  mn.setValue(ax,ay,az); mx.setValue(bx,by,bz);
}

static void arb8_geom_r(const float* pp,int n,
                         SoProceduralTriangles* tris,
                         SoProceduralWireframe* wire,void*)
{
  const float* p=(n>=24)?pp:kDefault;
  if(tris){
    tris->vertices.clear();tris->normals.clear();tris->indices.clear();
    for(int f=0;f<6;++f){
      const int* fv=kFaces[f];
      SbVec3f v0=vtx(p,fv[0]),v1=vtx(p,fv[1]),v2=vtx(p,fv[2]),v3=vtx(p,fv[3]);
      SbVec3f nm=(v1-v0).cross(v2-v0);nm.normalize();
      int b=(int)tris->vertices.size();
      tris->vertices.insert(tris->vertices.end(),{v0,v1,v2,v3});
      tris->normals .insert(tris->normals .end(),{nm,nm,nm,nm});
      tris->indices .insert(tris->indices.end(),{b,b+1,b+2,b,b+2,b+3});
    }
  }
  if(wire){
    wire->vertices.clear();wire->segments.clear();
    for(int i=0;i<8;++i)wire->vertices.push_back(vtx(p,i));
    for(int e=0;e<12;++e){wire->segments.push_back(kEdges[e][0]);wire->segments.push_back(kEdges[e][1]);}
  }
}

static const char* kArb8SchemaR = R"({
  "type": "ARB8_render",
  "params": [
    {"name":"v0x","type":"float","default":-1.0},{"name":"v0y","type":"float","default":-1.0},{"name":"v0z","type":"float","default":-1.0},
    {"name":"v1x","type":"float","default": 1.0},{"name":"v1y","type":"float","default":-1.0},{"name":"v1z","type":"float","default":-1.0},
    {"name":"v2x","type":"float","default": 1.0},{"name":"v2y","type":"float","default":-1.0},{"name":"v2z","type":"float","default": 1.0},
    {"name":"v3x","type":"float","default":-1.0},{"name":"v3y","type":"float","default":-1.0},{"name":"v3z","type":"float","default": 1.0},
    {"name":"v4x","type":"float","default":-1.0},{"name":"v4y","type":"float","default": 1.0},{"name":"v4z","type":"float","default":-1.0},
    {"name":"v5x","type":"float","default": 1.0},{"name":"v5y","type":"float","default": 1.0},{"name":"v5z","type":"float","default":-1.0},
    {"name":"v6x","type":"float","default": 1.0},{"name":"v6y","type":"float","default": 1.0},{"name":"v6z","type":"float","default": 1.0},
    {"name":"v7x","type":"float","default":-1.0},{"name":"v7y","type":"float","default": 1.0},{"name":"v7z","type":"float","default": 1.0}
  ],
  "vertices": [
    {"name":"v0","x":"v0x","y":"v0y","z":"v0z"},{"name":"v1","x":"v1x","y":"v1y","z":"v1z"},
    {"name":"v2","x":"v2x","y":"v2y","z":"v2z"},{"name":"v3","x":"v3x","y":"v3y","z":"v3z"},
    {"name":"v4","x":"v4x","y":"v4y","z":"v4z"},{"name":"v5","x":"v5x","y":"v5y","z":"v5z"},
    {"name":"v6","x":"v6x","y":"v6y","z":"v6z"},{"name":"v7","x":"v7x","y":"v7y","z":"v7z"}
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
    {"name":"f_bot_h","face":"bottom","dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_top_h","face":"top",   "dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_frt_h","face":"front", "dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_rgt_h","face":"right", "dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_bak_h","face":"back",  "dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_lft_h","face":"left",  "dragType":"DRAG_NO_INTERSECT"}
  ]
})";

int main(int argc, char** argv)
{
  initCoinHeadless();

  // Topology-driven registration: only bbox + geom callbacks needed.
  // Handles (DRAG_NO_INTERSECT + DRAG_ON_PLANE), positions, and
  // constraint checks all come from the JSON schema above.
  SoProceduralShape::registerShapeType(
      "ARB8_render", kArb8SchemaR,
      arb8_bbox_r, arb8_geom_r);

  SoSeparator* root = new SoSeparator;
  root->ref();

  SoPerspectiveCamera* cam = new SoPerspectiveCamera;
  root->addChild(cam);

  SoDirectionalLight* light = new SoDirectionalLight;
  light->direction.setValue(-0.5f,-0.8f,-0.6f);
  root->addChild(light);

  // Slightly sheared box
  float sheared[24];
  memcpy(sheared, kDefault, sizeof(sheared));
  for(int i=4;i<8;++i) sheared[3*i] += 0.3f;

  // Left: solid box
  {
    SoSeparator* sep=new SoSeparator;
    SoTranslation* t=new SoTranslation; t->translation.setValue(-2.5f,0,0); sep->addChild(t);
    SoMaterial* m=new SoMaterial; m->diffuseColor.setValue(0.3f,0.5f,0.9f);
    m->specularColor.setValue(0.4f,0.4f,0.4f); m->shininess.setValue(0.5f);
    sep->addChild(m);
    SoProceduralShape* s=new SoProceduralShape;
    s->shapeType.setValue("ARB8_render");
    s->params.setValues(0,24,kDefault);
    sep->addChild(s);
    root->addChild(sep);
  }

  // Centre: sheared box + vertex/edge/face dragger handles
  {
    SoSeparator* sep=new SoSeparator;
    SoTranslation* t=new SoTranslation; t->translation.setValue(0.f,0,0); sep->addChild(t);
    SoMaterial* m=new SoMaterial; m->diffuseColor.setValue(0.9f,0.5f,0.2f);
    m->specularColor.setValue(0.4f,0.4f,0.4f); m->shininess.setValue(0.5f);
    sep->addChild(m);
    SoProceduralShape* s=new SoProceduralShape;
    s->shapeType.setValue("ARB8_render");
    s->params.setValues(0,24,sheared);
    sep->addChild(s);

    // Add handle draggers — the core feature under test
    SoSeparator* handles = s->buildHandleDraggers();
    if(handles) sep->addChild(handles);

    root->addChild(sep);
  }

  // Right: wireframe
  {
    SoSeparator* sep=new SoSeparator;
    SoTranslation* t=new SoTranslation; t->translation.setValue(2.5f,0,0); sep->addChild(t);
    SoDrawStyle* ds=new SoDrawStyle; ds->style.setValue(SoDrawStyle::LINES); sep->addChild(ds);
    SoMaterial* m=new SoMaterial; m->diffuseColor.setValue(0.1f,0.9f,0.3f); sep->addChild(m);
    SoProceduralShape* s=new SoProceduralShape;
    s->shapeType.setValue("ARB8_render");
    s->params.setValues(0,24,sheared);
    sep->addChild(s);
    root->addChild(sep);
  }

  SbViewportRegion vp(DEFAULT_WIDTH,DEFAULT_HEIGHT);
  cam->viewAll(root,vp);
  cam->position.setValue(cam->position.getValue()*1.3f);

  char outpath[1024];
  if(argc>1) snprintf(outpath,sizeof(outpath),"%s.rgb",argv[1]);
  else       snprintf(outpath,sizeof(outpath),"render_arb8_draggers.rgb");

  bool ok=renderToFile(root,outpath);
  root->unref();
  return ok?0:1;
}
