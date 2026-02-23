/*
 * render_clip_plane.cpp - Integration test: SoClipPlane geometry clipping
 *
 * Renders a large red sphere that fills most of the viewport, then adds a
 * SoClipPlane that cuts the sphere exactly in half horizontally (along the
 * equator plane Y=0, clipping everything below).  The result should have:
 *   - The upper half of the image populated with sphere pixels.
 *   - The lower half of the image completely clipped (background colour).
 *
 * This validates that SoClipPlane correctly restricts rendering to the
 * specified half-space.
 *
 * Writes argv[1]+".rgb" and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoClipPlane.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbPlane.h>
#include <cstdio>
#include <cmath>

static const int W = 256;
static const int H = 256;

static bool validateClipPlane(const unsigned char *buf)
{
    // Sphere is red (emissive), background is very dark grey.
    // Upper half of the image (rows H/2..H-1 in OpenGL order, y=0=bottom)
    // should contain red sphere pixels.
    // Lower half (rows 0..H/2-1) should be background (dark).
    int redUpper    = 0;
    int darkLower   = 0;
    int totalUpper  = 0;
    int totalLower  = 0;

    // Sample the upper half (avoid the topmost row which may have edge artifacts)
    for (int y = H / 2 + 4; y < H - 4; y += 4) {
        for (int x = W / 4; x < 3 * W / 4; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] > 150 && p[1] < 80 && p[2] < 80) ++redUpper;
            ++totalUpper;
        }
    }

    // Sample the lower half (strictly below the clip plane)
    for (int y = 4; y < H / 2 - 4; y += 4) {
        for (int x = W / 4; x < 3 * W / 4; x += 4) {
            const unsigned char *p = buf + (y * W + x) * 3;
            if (p[0] < 60 && p[1] < 60 && p[2] < 60) ++darkLower;
            ++totalLower;
        }
    }

    printf("render_clip_plane: redUpper=%d/%d  darkLower=%d/%d\n",
           redUpper, totalUpper, darkLower, totalLower);

    bool upperOk = (totalUpper > 0) && (redUpper * 10 > totalUpper);   // >10%
    bool lowerOk = (totalLower > 0) && (darkLower * 2 > totalLower);   // >50%

    if (!upperOk) {
        fprintf(stderr, "render_clip_plane: FAIL - expected red sphere in upper half\n");
        return false;
    }
    if (!lowerOk) {
        fprintf(stderr, "render_clip_plane: FAIL - expected dark background in lower half\n");
        return false;
    }
    printf("render_clip_plane: PASS\n");
    return true;
}

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Orthographic camera looking along -Z, world height = 2.4
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 5);
    cam->nearDistance = 1.0f;
    cam->farDistance  = 20.0f;
    cam->height       = 2.4f;
    root->addChild(cam);

    // Dim directional light from above
    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(0.0f, -1.0f, -1.0f);
    root->addChild(light);

    // Clip plane: keep everything with Y >= 0 (clip away the lower half-space)
    // SoClipPlane is defined by a plane; geometry on the side the normal points
    // toward is KEPT.  Normal pointing up (+Y) keeps Y >= 0.
    SoClipPlane *cp = new SoClipPlane;
    cp->plane.setValue(SbPlane(SbVec3f(0.0f, 1.0f, 0.0f), 0.0f));
    root->addChild(cp);

    // Large emissive red sphere centred at origin
    SoMaterial *mat = new SoMaterial;
    mat->emissiveColor.setValue(0.9f, 0.1f, 0.1f);
    mat->diffuseColor .setValue(0.0f, 0.0f, 0.0f);
    root->addChild(mat);

    SoSphere *sph = new SoSphere;
    sph->radius = 1.0f;
    root->addChild(sph);

    SbViewportRegion vp(W, H);
    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.02f, 0.02f, 0.02f));

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_clip_plane.rgb");

    bool ok = false;
    if (renderer.render(root)) {
        const unsigned char *buf = renderer.getBuffer();
        bool pixOk = (buf != nullptr) && validateClipPlane(buf);
        ok = pixOk && renderer.writeToRGB(outpath);
    } else {
        fprintf(stderr, "render_clip_plane: render() failed\n");
    }

    root->unref();
    return ok ? 0 : 1;
}
