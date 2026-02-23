/*
 * render_text3.cpp - Visual regression test: SoText3 extruded 3D text
 *
 * Renders a scene with SoText3 extruded text using Obol's embedded ProFont.
 * ProFont is used as the font baseline so that the control images are
 * generated from a known, reproducible font rather than whatever FreeType
 * font happens to be installed on the system.
 *
 * Scene layout:
 *   Top:    white  "Obol 3D"  - ALL parts (front+back+sides), left-justified
 *   Bottom: orange "Text3"    - FRONT only (flat face), center-justified
 *
 * The executable writes argv[1]+".rgb" (SGI RGB format).
 *
 * Control images are generated from Obol with ProFont and serve as the
 * baseline for regression testing.  SoText3 produces 3D geometry, so the
 * rendering is deterministic for the same font and camera setup.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoText3.h>
#include <cstdio>
#include <cstring>

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    // Two directional lights to show extruded geometry depth
    SoDirectionalLight *light1 = new SoDirectionalLight;
    light1->direction.setValue(-0.5f, -0.8f, -0.6f);
    root->addChild(light1);

    SoDirectionalLight *light2 = new SoDirectionalLight;
    light2->direction.setValue(0.5f, -0.3f, -0.8f);
    light2->intensity.setValue(0.4f);
    root->addChild(light2);

    // ----- Top row: white extruded text, ALL parts -----
    {
        SoSeparator *sep = new SoSeparator;

        SoTranslation *t = new SoTranslation;
        t->translation.setValue(0.0f, 1.2f, 0.0f);
        sep->addChild(t);

        SoFont *font = new SoFont;
        font->size.setValue(1.5f);
        sep->addChild(font);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.9f, 0.9f, 0.9f);
        mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
        mat->shininess.setValue(0.5f);
        sep->addChild(mat);

        SoText3 *text = new SoText3;
        text->string.setValue("Obol 3D");
        text->parts.setValue(SoText3::ALL);
        text->justification.setValue(SoText3::LEFT);
        sep->addChild(text);

        root->addChild(sep);
    }

    // ----- Bottom row: orange flat (FRONT only) text -----
    {
        SoSeparator *sep = new SoSeparator;

        SoTranslation *t = new SoTranslation;
        t->translation.setValue(0.0f, -1.2f, 0.0f);
        sep->addChild(t);

        SoFont *font = new SoFont;
        font->size.setValue(2.0f);
        sep->addChild(font);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);
        mat->specularColor.setValue(0.4f, 0.3f, 0.1f);
        mat->shininess.setValue(0.3f);
        sep->addChild(mat);

        SoText3 *text = new SoText3;
        text->string.setValue("Text3");
        text->parts.setValue(SoText3::FRONT);
        text->justification.setValue(SoText3::CENTER);
        sep->addChild(text);

        root->addChild(sep);
    }

    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    cam->viewAll(root, vp);
    cam->position.setValue(cam->position.getValue() * 1.2f);

    // Output path
    char outpath[1024];
    if (argc > 1) {
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    } else {
        snprintf(outpath, sizeof(outpath), "render_text3.rgb");
    }

    bool ok = renderToFile(root, outpath);
    root->unref();
    return ok ? 0 : 1;
}
