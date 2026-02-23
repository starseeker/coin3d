/*
 * render_text2.cpp - Visual regression test: SoText2 2D screen-aligned text
 *
 * Renders three lines of SoText2 text using Obol's embedded ProFont.
 * ProFont is used as the font baseline so that the control images are
 * generated from a known, reproducible font rather than whatever FreeType
 * font happens to be installed on the system.
 *
 * Scene layout:
 *   Row 1 (top-left):    white "Hello World" – LEFT justified
 *   Row 2 (center):      cyan  "Coin3D Text" – CENTER justified
 *   Row 3 (bottom-right): yellow "ABC 123"    – RIGHT justified
 *
 * The executable writes argv[1]+".rgb" (SGI RGB format).
 *
 * Control images are generated from Obol with ProFont and serve as the
 * baseline for regression testing.  Because SoText2 renders pixel-aligned
 * screen-space text, the results are deterministic for the same font and
 * display resolution.
 */

#include "headless_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoText2.h>
#include <cstdio>
#include <cstring>

int main(int argc, char **argv)
{
    initCoinHeadless();

    SoSeparator *root = new SoSeparator;
    root->ref();

    // Use an orthographic camera so 2D text appears at a predictable size
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 10.0f);
    cam->orientation.setValue(SbVec3f(0.0f, 0.0f, 1.0f), 0.0f);
    cam->height.setValue(10.0f);
    cam->nearDistance.setValue(1.0f);
    cam->farDistance.setValue(20.0f);
    root->addChild(cam);

    // Single directional light (SoText2 uses screen-space rendering but
    // material colour is still affected by lighting in the scene)
    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.5f, -0.8f);
    root->addChild(light);

    // ----- Row 1: white "Hello World", LEFT justified -----
    {
        SoSeparator *sep = new SoSeparator;

        SoTranslation *t = new SoTranslation;
        t->translation.setValue(-3.0f, 2.5f, 0.0f);
        sep->addChild(t);

        SoFont *font = new SoFont;
        font->size.setValue(14.0f);
        sep->addChild(font);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
        mat->emissiveColor.setValue(0.9f, 0.9f, 0.9f);
        sep->addChild(mat);

        SoText2 *text = new SoText2;
        text->string.setValue("Hello World");
        text->justification.setValue(SoText2::LEFT);
        sep->addChild(text);

        root->addChild(sep);
    }

    // ----- Row 2: cyan "Coin3D Text", CENTER justified -----
    {
        SoSeparator *sep = new SoSeparator;

        SoTranslation *t = new SoTranslation;
        t->translation.setValue(0.0f, 0.0f, 0.0f);
        sep->addChild(t);

        SoFont *font = new SoFont;
        font->size.setValue(18.0f);
        sep->addChild(font);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.0f, 1.0f, 1.0f);
        mat->emissiveColor.setValue(0.0f, 0.9f, 0.9f);
        sep->addChild(mat);

        SoText2 *text = new SoText2;
        text->string.setValue("Coin3D Text");
        text->justification.setValue(SoText2::CENTER);
        sep->addChild(text);

        root->addChild(sep);
    }

    // ----- Row 3: yellow multi-line, RIGHT justified -----
    {
        SoSeparator *sep = new SoSeparator;

        SoTranslation *t = new SoTranslation;
        t->translation.setValue(3.0f, -2.5f, 0.0f);
        sep->addChild(t);

        SoFont *font = new SoFont;
        font->size.setValue(12.0f);
        sep->addChild(font);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(1.0f, 1.0f, 0.0f);
        mat->emissiveColor.setValue(0.9f, 0.9f, 0.0f);
        sep->addChild(mat);

        SoText2 *text = new SoText2;
        text->string.set1Value(0, "ABC 123");
        text->string.set1Value(1, "Line Two");
        text->justification.setValue(SoText2::RIGHT);
        sep->addChild(text);

        root->addChild(sep);
    }

    // Output path
    char outpath[1024];
    if (argc > 1) {
        snprintf(outpath, sizeof(outpath), "%s.rgb", argv[1]);
    } else {
        snprintf(outpath, sizeof(outpath), "render_text2.rgb");
    }

    bool ok = renderToFile(root, outpath);
    root->unref();
    return ok ? 0 : 1;
}
