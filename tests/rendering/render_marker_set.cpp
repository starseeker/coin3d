/*
 * render_marker_set.cpp - Integration test: SoMarkerSet rendering
 *
 * Renders a scene with a SoMarkerSet containing several markers at distinct
 * positions.  Validates:
 *   1. SoMarkerSet class type id is registered.
 *   2. Static API: getNumDefinedMarkers() returns > 0.
 *   3. Static API: isMarkerBitSet does not crash.
 *   4. The rendered buffer contains non-background pixels (markers visible).
 *
 * Writes argv[1]+".rgb" (SGI RGB format) and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoMarkerSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/SoType.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbColor.h>
#include <cstdio>

static const int W = 256;
static const int H = 256;

int main(int argc, char ** argv)
{
    initCoinHeadless();

    char outpath[1024];
    if (argc > 1)
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    else
        snprintf(outpath, sizeof(outpath), "render_marker_set.rgb");

    bool ok = true;

    // -----------------------------------------------------------------------
    // Verify class type id
    // -----------------------------------------------------------------------
    {
        SoType t = SoMarkerSet::getClassTypeId();
        if (t == SoType::badType()) {
            fprintf(stderr, "render_marker_set: FAIL - SoMarkerSet has bad class type\n");
            ok = false;
        } else {
            printf("render_marker_set: SoMarkerSet classTypeId OK\n");
        }
    }

    // -----------------------------------------------------------------------
    // Static API: getNumDefinedMarkers
    // -----------------------------------------------------------------------
    {
        int nMarkers = SoMarkerSet::getNumDefinedMarkers();
        if (nMarkers <= 0) {
            fprintf(stderr, "render_marker_set: FAIL - getNumDefinedMarkers() returned %d\n",
                    nMarkers);
            ok = false;
        } else {
            printf("render_marker_set: getNumDefinedMarkers() = %d OK\n", nMarkers);
        }
    }

    // -----------------------------------------------------------------------
    // Static API: isMarkerBitSet does not crash
    // -----------------------------------------------------------------------
    {
        // Call with valid marker index (0) and bit 0
        (void)SoMarkerSet::isMarkerBitSet(0, 0);
        printf("render_marker_set: isMarkerBitSet(0,0) OK\n");
    }

    // -----------------------------------------------------------------------
    // Build and render a scene with a SoMarkerSet
    // -----------------------------------------------------------------------
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera * cam = new SoPerspectiveCamera;
    root->addChild(cam);

    // White material for markers
    SoMaterial * mat = new SoMaterial;
    mat->emissiveColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);

    // Coordinates: 5 markers arranged in a cross pattern
    SoCoordinate3 * coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f( 0.0f,  0.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 0.5f,  0.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f(-0.5f,  0.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f( 0.0f,  0.5f, 0.0f));
    coords->point.set1Value(4, SbVec3f( 0.0f, -0.5f, 0.0f));
    root->addChild(coords);

    SoMarkerSet * markers = new SoMarkerSet;
    // Use a simple filled circle marker (5×5)
    markers->markerIndex.set1Value(0, SoMarkerSet::CIRCLE_FILLED_5_5);
    markers->markerIndex.set1Value(1, SoMarkerSet::SQUARE_FILLED_5_5);
    markers->markerIndex.set1Value(2, SoMarkerSet::DIAMOND_FILLED_5_5);
    markers->markerIndex.set1Value(3, SoMarkerSet::CIRCLE_FILLED_7_7);
    markers->markerIndex.set1Value(4, SoMarkerSet::SQUARE_FILLED_7_7);
    root->addChild(markers);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    SoOffscreenRenderer renderer(vp);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));

    if (!renderer.render(root)) {
        fprintf(stderr, "render_marker_set: FAIL - render() returned false\n");
        ok = false;
    } else {
        const unsigned char * buf = renderer.getBuffer();
        if (!buf) {
            fprintf(stderr, "render_marker_set: FAIL - getBuffer() returned null\n");
            ok = false;
        } else {
            // Count non-black pixels (markers should appear as bright spots)
            int nonBlack = 0;
            for (int i = 0; i < W * H; ++i) {
                const unsigned char * p = buf + i * 3;
                if (p[0] > 30 || p[1] > 30 || p[2] > 30)
                    ++nonBlack;
            }
            printf("render_marker_set: non-black pixels = %d / %d\n",
                   nonBlack, W * H);
            if (nonBlack == 0) {
                fprintf(stderr, "render_marker_set: FAIL - no marker pixels found\n");
                ok = false;
            } else {
                printf("render_marker_set: pixel validation OK\n");
                renderer.writeToRGB(outpath);
            }
        }
    }

    root->unref();
    printf("render_marker_set: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
