/*
 * render_drawstyle.cpp - Visual regression test: SoDrawStyle rendering modes
 *
 * Renders the same torus-like scene three times, each with a different
 * SoDrawStyle, writing one output image per style.
 *
 *   argv[1]+".rgb"         –  FILLED  (solid, shaded surfaces)
 *   argv[1]+"_lines.rgb"   –  LINES   (wireframe edges)
 *   argv[1]+"_points.rgb"  –  POINTS  (vertex dots)
 *
 * The test scene is an icosphere-like sphere built from SoIndexedFaceSet so
 * that edge and point rendering produces clearly visible, distinct output.
 * Using SoSphere directly would give smooth implicit surfaces; IFS gives
 * explicit faces/edges/points.
 *
 * The primary output for CTest is argv[1]+".rgb" (FILLED).
 *
 * The executable writes three SGI RGB files.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <cstdio>
#include <cmath>
#include <array>
#include <vector>

// Build a low-resolution subdivided icosahedron (2 subdivisions = 80 faces)
// so that wireframe and point modes show clear polygonal edges.

static const float PHI = 1.61803398875f;   // golden ratio

// 12 icosahedron vertices on a unit sphere
static const float ICO_V[12][3] = {
    { 0,  1,  PHI}, { 0, -1,  PHI}, { 0,  1, -PHI}, { 0, -1, -PHI},
    { 1,  PHI, 0}, {-1,  PHI, 0}, { 1, -PHI, 0}, {-1, -PHI, 0},
    { PHI, 0,  1}, {-PHI, 0,  1}, { PHI, 0, -1}, {-PHI, 0, -1}
};

// 20 icosahedron faces
static const int ICO_F[20][3] = {
    {0,1,8},{0,8,4},{0,4,5},{0,5,9},{0,9,1},
    {1,6,8},{8,6,10},{8,10,4},{4,10,2},{4,2,5},
    {5,2,11},{5,11,9},{9,11,7},{9,7,1},{1,7,6},
    {3,6,7},{3,7,11},{3,11,2},{3,2,10},{3,10,6}
};

// Normalize a 3-vector in place
static void norm3(float v[3]) {
    float len = sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if (len > 1e-6f) { v[0]/=len; v[1]/=len; v[2]/=len; }
}

// Midpoint on sphere surface
static void midpoint(const float a[3], const float b[3], float out[3]) {
    out[0] = a[0]+b[0]; out[1] = a[1]+b[1]; out[2] = a[2]+b[2];
    norm3(out);
}

struct GeoSphere {
    std::vector<std::array<float,3>> v;
    std::vector<std::array<int,3>>   f;
};

static GeoSphere buildGeoSphere(int subdivisions)
{
    GeoSphere g;
    // Seed from icosahedron
    for (int i = 0; i < 12; i++) {
        float len = sqrtf(ICO_V[i][0]*ICO_V[i][0]+ICO_V[i][1]*ICO_V[i][1]+ICO_V[i][2]*ICO_V[i][2]);
        g.v.push_back({ICO_V[i][0]/len, ICO_V[i][1]/len, ICO_V[i][2]/len});
    }
    for (int i = 0; i < 20; i++)
        g.f.push_back({ICO_F[i][0], ICO_F[i][1], ICO_F[i][2]});

    for (int s = 0; s < subdivisions; s++) {
        std::vector<std::array<int,3>> newf;
        for (auto &tri : g.f) {
            int a = tri[0], b = tri[1], c = tri[2];
            float m0[3], m1[3], m2[3];
            midpoint(g.v[a].data(), g.v[b].data(), m0);
            midpoint(g.v[b].data(), g.v[c].data(), m1);
            midpoint(g.v[c].data(), g.v[a].data(), m2);
            int ia = (int)g.v.size(); g.v.push_back({m0[0],m0[1],m0[2]});
            int ib = (int)g.v.size(); g.v.push_back({m1[0],m1[1],m1[2]});
            int ic = (int)g.v.size(); g.v.push_back({m2[0],m2[1],m2[2]});
            newf.push_back({a,ia,ic});
            newf.push_back({ia,b,ib});
            newf.push_back({ic,ib,c});
            newf.push_back({ia,ib,ic});
        }
        g.f = newf;
    }
    return g;
}

static SoSeparator *buildSphereScene()
{
    GeoSphere gs = buildGeoSphere(2);  // 80 faces – clearly polygonal

    SoSeparator *sep = new SoSeparator;

    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.setNum((int)gs.v.size());
    for (int i = 0; i < (int)gs.v.size(); i++)
        coords->point.set1Value(i, gs.v[i][0], gs.v[i][1], gs.v[i][2]);
    sep->addChild(coords);

    // Per-vertex normals = vertex positions on unit sphere
    SoNormal *normals = new SoNormal;
    normals->vector.setNum((int)gs.v.size());
    for (int i = 0; i < (int)gs.v.size(); i++)
        normals->vector.set1Value(i, gs.v[i][0], gs.v[i][1], gs.v[i][2]);
    sep->addChild(normals);

    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::PER_VERTEX_INDEXED);
    sep->addChild(nb);

    SoIndexedFaceSet *ifs = new SoIndexedFaceSet;
    ifs->coordIndex.setNum((int)gs.f.size() * 4);
    ifs->normalIndex.setNum((int)gs.f.size() * 4);
    for (int i = 0; i < (int)gs.f.size(); i++) {
        ifs->coordIndex.set1Value(i*4+0, gs.f[i][0]);
        ifs->coordIndex.set1Value(i*4+1, gs.f[i][1]);
        ifs->coordIndex.set1Value(i*4+2, gs.f[i][2]);
        ifs->coordIndex.set1Value(i*4+3, -1);
        ifs->normalIndex.set1Value(i*4+0, gs.f[i][0]);
        ifs->normalIndex.set1Value(i*4+1, gs.f[i][1]);
        ifs->normalIndex.set1Value(i*4+2, gs.f[i][2]);
        ifs->normalIndex.set1Value(i*4+3, -1);
    }
    sep->addChild(ifs);
    return sep;
}

static void renderWithStyle(const char *base,
                            const char *suffix,
                            SoDrawStyle::Style style,
                            float lineWidth = 1.5f,
                            float pointSize = 3.0f)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.8f, 3.2f);
    cam->pointAt(SbVec3f(0,0,0), SbVec3f(0,1,0));
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.7f, -0.5f);
    root->addChild(light);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.3f, 0.65f, 0.9f);
    mat->specularColor.setValue(0.7f, 0.7f, 0.7f);
    mat->shininess.setValue(0.6f);
    root->addChild(mat);

    SoDrawStyle *ds = new SoDrawStyle;
    ds->style.setValue(style);
    if (style == SoDrawStyle::LINES)
        ds->lineWidth.setValue(lineWidth);
    if (style == SoDrawStyle::POINTS)
        ds->pointSize.setValue(pointSize);
    root->addChild(ds);

    root->addChild(buildSphereScene());

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s%s.rgb", base, suffix);
    renderToFile(root, outpath);
    root->unref();
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *base = (argc > 1) ? argv[1] : "render_drawstyle";

    // Primary: FILLED
    renderWithStyle(base, "",       SoDrawStyle::FILLED);
    // Secondary outputs
    renderWithStyle(base, "_lines", SoDrawStyle::LINES,  2.0f);
    renderWithStyle(base, "_points",SoDrawStyle::POINTS, 2.0f, 6.0f);

    return 0;
}
