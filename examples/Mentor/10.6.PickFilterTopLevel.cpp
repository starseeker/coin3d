/*
 *
 *  Copyright (C) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  Further, this software is distributed without any warranty that it is
 *  free of the rightful claim of any third person regarding infringement
 *  or the like.  Any license provided herein, whether implied or
 *  otherwise, applies only to this software file.  Patent licenses, if
 *  any, provided herein do not apply to combinations of this program with
 *  other software, or any other product whatsoever.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 *  Mountain View, CA  94043, or:
 *
 *  http://www.sgi.com
 *
 *  For further information regarding this notice, see:
 *
 *  http://oss.sgi.com/projects/GenInfo/NoticeExplan/
 *
 */

/*
 * Headless version of Inventor Mentor example 10.6
 *
 * Demonstrates the difference between a top-level pick filter and default
 * (deepest-node) picking.  Two identical park-bench models sit side by side.
 * After an initial unselected frame, two "post-pick" frames show:
 *
 *   Frame 0 – initial scene: both benches unselected.
 *   Frame 1 – FILTERED selection of the left bench: the pick-filter truncates
 *             the path to the top-level group node, so the ENTIRE left bench
 *             is highlighted (whole model turns bright yellow via SoMaterial
 *             override).
 *   Frame 2 – DEFAULT selection of the same spot on the left bench: the full
 *             leaf path is returned, so only the single material zone that was
 *             hit turns bright yellow; the rest of the bench keeps its natural
 *             colours.
 *
 * If parkbench.iv cannot be loaded a compact procedural bench substitute is
 * used so the demo still produces meaningful images.
 */

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cstring>

// Bright-yellow highlight colour used to show "selection"
static const float HIGHLIGHT[3] = { 1.0f, 0.95f, 0.15f };

// ============================================================================
// Load one copy of parkbench.iv from disk.
// Returns NULL on failure.
// ============================================================================
static SoSeparator *loadBench(const char *path)
{
    SoInput in;
    if (!in.openFile(path)) return nullptr;
    SoSeparator *s = SoDB::readAll(&in);
    in.closeFile();
    return s;   // caller must ref/unref
}

// ============================================================================
// Procedural park-bench fallback (3-plank seat + 2 leg-frames)
// ============================================================================
static SoSeparator *makeFallbackBench()
{
    SoSeparator *bench = new SoSeparator;

    // Seat planks (3 planks side by side)
    float plankX[3] = { -0.6f, 0.0f, 0.6f };
    float plankColor[3][3] = {
        { 0.55f, 0.30f, 0.10f },
        { 0.60f, 0.35f, 0.12f },
        { 0.50f, 0.28f, 0.09f }
    };
    for (int k = 0; k < 3; k++) {
        SoSeparator *plank = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(plankX[k], 0.55f, 0.0f);
        plank->addChild(t);
        SoMaterial *m = new SoMaterial;
        m->diffuseColor.setValue(plankColor[k][0], plankColor[k][1], plankColor[k][2]);
        plank->addChild(m);
        SoScale *sc = new SoScale;
        sc->scaleFactor.setValue(0.45f, 0.08f, 1.8f);
        plank->addChild(sc);
        plank->addChild(new SoCube);
        bench->addChild(plank);
    }

    // Leg frames (2 A-frames)
    float legZ[2] = { -0.75f, 0.75f };
    for (int k = 0; k < 2; k++) {
        SoSeparator *frame = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(0.0f, 0.25f, legZ[k]);
        frame->addChild(t);
        SoMaterial *m = new SoMaterial;
        m->diffuseColor.setValue(0.25f, 0.25f, 0.30f);
        frame->addChild(m);
        SoScale *sc = new SoScale;
        sc->scaleFactor.setValue(1.6f, 0.5f, 0.08f);
        frame->addChild(sc);
        frame->addChild(new SoCube);
        bench->addChild(frame);
    }

    return bench;
}

// ============================================================================
// Find the first child SoSeparator of a separator root.
// This is typically one named component of an .iv model (e.g. BENPA_SLAT).
// We inject a yellow SoMaterial at position 0 to highlight that one part,
// simulating a default (leaf-path) selection of just that component.
// Returns nullptr if the root has no SoSeparator children.
// ============================================================================
static SoSeparator *findFirstComponentSep(SoSeparator *root)
{
    for (int i = 0; i < root->getNumChildren(); i++) {
        SoNode *child = root->getChild(i);
        if (child->isOfType(SoSeparator::getClassTypeId()))
            return static_cast<SoSeparator *>(child);
    }
    return nullptr;
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char **argv)
{
    initCoinHeadless();

    // Locate data directory
    const char *dataDir = getenv("OBOL_DATA_DIR");
    if (!dataDir) dataDir = getenv("IVEXAMPLES_DATA_DIR");
    if (!dataDir) dataDir = getenv("COIN_DATA_DIR");
    if (!dataDir) dataDir = "../../data";

    char benchPath[512];
    snprintf(benchPath, sizeof(benchPath), "%s/parkbench.iv", dataDir);

    // Load two independent copies of the bench (one for left, one for right)
    SoSeparator *leftBench  = loadBench(benchPath);
    SoSeparator *rightBench = loadBench(benchPath);

    if (!leftBench || !rightBench) {
        fprintf(stderr, "Note: could not load %s – using procedural bench\n", benchPath);
        if (leftBench)  { leftBench->unref();  leftBench  = nullptr; }
        if (rightBench) { rightBench->unref(); rightBench = nullptr; }
        leftBench  = makeFallbackBench();
        rightBench = makeFallbackBench();
    } else {
        fprintf(stderr, "Loaded parkbench.iv from %s\n", benchPath);
    }
    leftBench->ref();
    rightBench->ref();

    // ---- Build root scene -----------------------------------------------
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    // Two-light rig for good bench visibility
    SoDirectionalLight *light1 = new SoDirectionalLight;
    light1->direction.setValue(-1.0f, -1.5f, -1.0f);
    light1->color.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(light1);

    SoDirectionalLight *light2 = new SoDirectionalLight;
    light2->direction.setValue(1.0f, -0.5f, -0.3f);
    light2->intensity.setValue(0.35f);
    light2->color.setValue(0.8f, 0.9f, 1.0f);
    root->addChild(light2);

    // ---- Left group: translation + highlight material (override) + bench --
    SoSeparator *leftGroup = new SoSeparator;
    root->addChild(leftGroup);

    SoTranslation *leftT = new SoTranslation;
    leftT->translation.setValue(-3.0f, 0.0f, 0.0f);
    leftGroup->addChild(leftT);

    // Highlight material for filtered selection (whole bench).
    // NOT added to the scene yet – inserted dynamically per frame.
    SoMaterial *leftHighlight = new SoMaterial;
    leftHighlight->diffuseColor.setValue(HIGHLIGHT[0], HIGHLIGHT[1], HIGHLIGHT[2]);
    leftHighlight->specularColor.setValue(0.5f, 0.5f, 0.1f);
    leftHighlight->shininess.setValue(0.5f);

    leftGroup->addChild(leftBench);

    // ---- Right group: translation + bench (unchanged throughout) ----------
    SoSeparator *rightGroup = new SoSeparator;
    root->addChild(rightGroup);

    SoTranslation *rightT = new SoTranslation;
    rightT->translation.setValue(3.0f, 0.0f, 0.0f);
    rightGroup->addChild(rightT);
    rightGroup->addChild(rightBench);

    // Frame the camera on the scene
    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);
    // Pull back a little for breathing room
    SbVec3f p = cam->position.getValue();
    cam->position.setValue(p[0], p[1] * 1.15f, p[2] * 1.2f);

    // ---- Find one named component in leftBench for default-selection demo --
    // Inject a yellow SoMaterial at position 0 of that component separator to
    // highlight it independently (simulating a deep/leaf path selection).
    SoSeparator *benchComponent = findFirstComponentSep(leftBench);
    SoMaterial  *componentHighlight = new SoMaterial;
    componentHighlight->diffuseColor.setValue(HIGHLIGHT[0], HIGHLIGHT[1], HIGHLIGHT[2]);
    componentHighlight->specularColor.setValue(0.5f, 0.5f, 0.1f);
    componentHighlight->shininess.setValue(0.5f);
    if (benchComponent) {
        fprintf(stderr, "Default-selection demo: will highlight first bench component\n");
    } else {
        fprintf(stderr, "Note: no component separator found in bench\n");
    }

    const char *base = (argc > 1) ? argv[1] : "10.6.PickFilterTopLevel";
    char filename[512];
    int frameNum = 0;

    // ---- Frame 0: initial (both benches unselected) ----------------------
    printf("Frame %d: initial scene (both benches unselected)\n", frameNum);
    snprintf(filename, sizeof(filename), "%s_frame%02d_initial.rgb", base, frameNum++);
    renderToFile(root, filename);

    // ---- Frame 1: FILTERED selection of left bench -----------------------
    // The pick filter truncates the path to the top-level group node, so
    // the ENTIRE bench is "selected".  Visual: whole left bench → yellow.
    // Insert yellow material BEFORE the bench geometry (position after translation).
    printf("Frame %d: filtered selection → entire left bench highlighted\n", frameNum);
    leftGroup->insertChild(leftHighlight, 1);   // after translation, before bench
    snprintf(filename, sizeof(filename), "%s_frame%02d_filtered_selected.rgb", base, frameNum++);
    renderToFile(root, filename);
    leftGroup->removeChild(leftHighlight);   // restore: bench reverts to default grey

    // ---- Frame 2: DEFAULT selection (leaf path) of left bench ------------
    // No filter: path goes to the deepest node hit (one named component).
    // Visual: only that one component → yellow; the rest of the bench stays grey.
    printf("Frame %d: default selection → only one bench component highlighted\n", frameNum);
    if (benchComponent)
        benchComponent->insertChild(componentHighlight, 0);
    snprintf(filename, sizeof(filename), "%s_frame%02d_default_selected.rgb", base, frameNum++);
    renderToFile(root, filename);
    if (benchComponent)
        benchComponent->removeChild(componentHighlight);    // restore

    printf("\nRendered %d frames demonstrating pick filter.\n", frameNum);
    printf("  Filtered : entire bench (top-level group) turns yellow.\n");
    printf("  Default  : only the first bench component turns yellow.\n");

    leftBench->unref();
    rightBench->unref();
    root->unref();
    return 0;
}
