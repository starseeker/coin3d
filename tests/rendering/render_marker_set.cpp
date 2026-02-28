/*
 * render_marker_set.cpp - Integration test: SoMarkerSet rendering
 *
 * Validates SoMarkerSet API (type id, getNumDefinedMarkers, isMarkerBitSet)
 * and renders a scene with several markers.
 * The scene is built by the shared testlib factory (createMarkerSet).
 *
 * Writes argv[1]+".rgb" (SGI RGB format) and returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
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
    SoSeparator * root = ObolTest::Scenes::createMarkerSet(W, H);

    SbViewportRegion vp(W, H);

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
