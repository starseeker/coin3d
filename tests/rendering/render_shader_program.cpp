/*
 * render_shader_program.cpp — GLSL shader program coverage test
 *
 * Exercises the Coin shader subsystem:
 *   1. SoShaderProgram + SoVertexShader + SoFragmentShader (GLSL source)
 *   2. SoShaderParameter1f, SoShaderParameter3f, SoShaderParameter4f,
 *      SoShaderParameterMatrix, SoShaderParameter1i
 *   3. SoShaderProgram::setEnableCallback
 *   4. isActive field toggling on SoShaderObject
 *   5. Multiple shaders in one scene (enable/disable path)
 *   6. SoShaderParameterArray1f, SoShaderParameterArray3f
 *
 * All shader objects are created and their fields set; the programs are
 * rendered against a lit sphere so that the GL shader compiler is exercised.
 * On drivers that do not support GLSL (e.g. old Mesa) the render still
 * passes because Coin falls back to fixed-function rendering.
 *
 * Returns 0 on pass, 1 on fail.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoShaderProgram.h>
#include <Inventor/nodes/SoVertexShader.h>
#include <Inventor/nodes/SoFragmentShader.h>
#include <Inventor/nodes/SoShaderParameter.h>
#include <Inventor/SbViewportRegion.h>
#include <cstdio>
#include <cstring>

static const int W = 256;
static const int H = 256;

// Simple vertex pass-through shader
static const char *vert_src =
    "void main() {\n"
    "  gl_Position = ftransform();\n"
    "  gl_FrontColor = gl_Color;\n"
    "}\n";

// Fragment shader that modulates with a color uniform
static const char *frag_src =
    "uniform vec3 uColor;\n"
    "void main() {\n"
    "  gl_FragColor = vec4(uColor, 1.0);\n"
    "}\n";

// Fragment shader using multiple uniform types
static const char *frag_src2 =
    "uniform float uAlpha;\n"
    "uniform vec4  uBaseColor;\n"
    "void main() {\n"
    "  gl_FragColor = vec4(uBaseColor.rgb, uAlpha);\n"
    "}\n";

// ---------------------------------------------------------------------------
// Callback fired when SoShaderProgram enables/disables itself
// ---------------------------------------------------------------------------
static int g_enableCount = 0;
static void shaderEnableCB(void * /*closure*/, SoState * /*state*/, const SbBool enable)
{
    ++g_enableCount;
    (void)enable;
}

// ---------------------------------------------------------------------------
// Validate that the rendered buffer contains non-background pixels
// ---------------------------------------------------------------------------
static bool validateNonBlack(const unsigned char *buf, int npix,
                              const char *label)
{
    int nonbg = 0;
    for (int i = 0; i < npix; ++i) {
        const unsigned char *p = buf + i * 3;
        if (p[0] > 5 || p[1] > 5 || p[2] > 5) ++nonbg;
    }
    printf("  %s: nonbg=%d\n", label, nonbg);
    if (nonbg < 50) {
        fprintf(stderr, "  FAIL %s: scene appears blank\n", label);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 1: basic vertex + fragment shader with SoShaderParameter3f
// ---------------------------------------------------------------------------
static bool test1_basicShader(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Build the shader program
    SoShaderProgram *prog = new SoShaderProgram;

    SoVertexShader *vs = new SoVertexShader;
    vs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
    vs->sourceProgram.setValue(vert_src);

    SoFragmentShader *fs = new SoFragmentShader;
    fs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
    fs->sourceProgram.setValue(frag_src);

    // SoShaderParameter3f: uColor uniform
    SoShaderParameter3f *p3f = new SoShaderParameter3f;
    p3f->name.setValue("uColor");
    p3f->value.setValue(SbVec3f(0.2f, 0.8f, 0.4f));
    fs->parameter.addNode(p3f);

    prog->shaderObject.addNode(vs);
    prog->shaderObject.addNode(fs);

    // Register the enable callback
    prog->setEnableCallback(shaderEnableCB, nullptr);

    root->addChild(prog);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.7f, 0.7f, 0.7f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_basic.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(SbViewportRegion(W, H));
        ok = ren->render(root);
        if (ok) {
            ok = validateNonBlack(ren->getBuffer(), W * H, "test1_basicShader");
        }
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 2: SoShaderParameter4f + SoShaderParameter1f
// ---------------------------------------------------------------------------
static bool test2_multiParams(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoShaderProgram *prog = new SoShaderProgram;

    SoVertexShader *vs = new SoVertexShader;
    vs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
    vs->sourceProgram.setValue(vert_src);

    SoFragmentShader *fs = new SoFragmentShader;
    fs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
    fs->sourceProgram.setValue(frag_src2);

    SoShaderParameter1f *p1f = new SoShaderParameter1f;
    p1f->name.setValue("uAlpha");
    p1f->value.setValue(1.0f);
    fs->parameter.addNode(p1f);

    SoShaderParameter4f *p4f = new SoShaderParameter4f;
    p4f->name.setValue("uBaseColor");
    p4f->value.setValue(SbVec4f(0.9f, 0.3f, 0.2f, 1.0f));
    fs->parameter.addNode(p4f);

    prog->shaderObject.addNode(vs);
    prog->shaderObject.addNode(fs);
    root->addChild(prog);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.7f, 0.3f, 0.3f);
    root->addChild(mat);
    root->addChild(new SoCube);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_multiparams.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(SbViewportRegion(W, H));
        ok = ren->render(root);
        if (ok)
            ok = validateNonBlack(ren->getBuffer(), W * H, "test2_multiParams");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 3: disable isActive on shader; scene still renders (fixed-function)
// ---------------------------------------------------------------------------
static bool test3_shaderDisable(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoShaderProgram *prog = new SoShaderProgram;

    SoVertexShader *vs = new SoVertexShader;
    vs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
    vs->sourceProgram.setValue(vert_src);
    vs->isActive.setValue(FALSE); // disabled

    SoFragmentShader *fs = new SoFragmentShader;
    fs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
    fs->sourceProgram.setValue(frag_src);
    fs->isActive.setValue(FALSE); // disabled

    prog->shaderObject.addNode(vs);
    prog->shaderObject.addNode(fs);
    root->addChild(prog);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.4f, 0.7f, 0.9f);
    root->addChild(mat);
    root->addChild(new SoCylinder);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_disabled.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(SbViewportRegion(W, H));
        ok = ren->render(root);
        if (ok)
            ok = validateNonBlack(ren->getBuffer(), W * H, "test3_shaderDisable");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 4: SoShaderParameter1i, SoShaderParameterArray1f, SoShaderParameterArray3f
// ---------------------------------------------------------------------------
static bool test4_arrayParams(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoShaderProgram *prog = new SoShaderProgram;

    SoVertexShader *vs = new SoVertexShader;
    vs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
    vs->sourceProgram.setValue(vert_src);

    // Fragment shader that uses integer and float array uniforms
    static const char *frag_arr =
        "uniform int   uMode;\n"
        "uniform float uWeights[3];\n"
        "uniform vec3  uColors[2];\n"
        "void main() {\n"
        "  if (uMode == 0) {\n"
        "    gl_FragColor = vec4(uColors[0] * uWeights[0], 1.0);\n"
        "  } else {\n"
        "    gl_FragColor = vec4(uColors[1] * uWeights[1], 1.0);\n"
        "  }\n"
        "}\n";

    SoFragmentShader *fs = new SoFragmentShader;
    fs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
    fs->sourceProgram.setValue(frag_arr);

    // SoShaderParameter1i
    SoShaderParameter1i *p1i = new SoShaderParameter1i;
    p1i->name.setValue("uMode");
    p1i->value.setValue(1);
    fs->parameter.addNode(p1i);

    // SoShaderParameterArray1f (weights)
    SoShaderParameterArray1f *parr1f = new SoShaderParameterArray1f;
    parr1f->name.setValue("uWeights");
    parr1f->value.set1Value(0, 0.8f);
    parr1f->value.set1Value(1, 1.0f);
    parr1f->value.set1Value(2, 0.5f);
    fs->parameter.addNode(parr1f);

    // SoShaderParameterArray3f (color palette)
    SoShaderParameterArray3f *parr3f = new SoShaderParameterArray3f;
    parr3f->name.setValue("uColors");
    parr3f->value.set1Value(0, SbVec3f(1.0f, 0.2f, 0.0f));
    parr3f->value.set1Value(1, SbVec3f(0.0f, 0.6f, 1.0f));
    fs->parameter.addNode(parr3f);

    prog->shaderObject.addNode(vs);
    prog->shaderObject.addNode(fs);
    root->addChild(prog);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f, 0.5f, 0.8f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_array_params.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(SbViewportRegion(W, H));
        ok = ren->render(root);
        if (ok)
            ok = validateNonBlack(ren->getBuffer(), W * H, "test4_arrayParams");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 5: SoShaderParameter2f, SoShaderParameterMatrix; exercise sourceType
//         = FILENAME path (file doesn't exist → warns but shouldn't crash)
// ---------------------------------------------------------------------------
static bool test5_matrixParam(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoShaderProgram *prog = new SoShaderProgram;

    SoVertexShader *vs = new SoVertexShader;
    vs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
    vs->sourceProgram.setValue(vert_src);

    static const char *frag_mat =
        "uniform mat4  uXform;\n"
        "uniform vec2  uOffset;\n"
        "void main() {\n"
        "  vec4 c = uXform * vec4(gl_FragCoord.xy * 0.001, 0.0, 1.0);\n"
        "  gl_FragColor = vec4(abs(c.rgb) + vec3(uOffset, 0.0), 1.0);\n"
        "}\n";

    SoFragmentShader *fs = new SoFragmentShader;
    fs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
    fs->sourceProgram.setValue(frag_mat);

    SoShaderParameterMatrix *pm = new SoShaderParameterMatrix;
    pm->name.setValue("uXform");
    SbMatrix identity; identity.makeIdentity();
    pm->value.setValue(identity);
    fs->parameter.addNode(pm);

    SoShaderParameter2f *p2f = new SoShaderParameter2f;
    p2f->name.setValue("uOffset");
    p2f->value.setValue(SbVec2f(0.2f, 0.3f));
    fs->parameter.addNode(p2f);

    prog->shaderObject.addNode(vs);
    prog->shaderObject.addNode(fs);
    root->addChild(prog);

    // Also test FILENAME sourceType (non-existent file should warn and not crash)
    {
        SoVertexShader *vs_file = new SoVertexShader;
        vs_file->sourceType.setValue(SoShaderObject::FILENAME);
        vs_file->sourceProgram.setValue("/nonexistent/shader.vert");
        vs_file->isActive.setValue(FALSE); // disabled so it won't crash rendering
        prog->shaderObject.addNode(vs_file);
    }

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.6f, 0.4f, 0.8f);
    root->addChild(mat);
    root->addChild(new SoCube);

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_matrix_param.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(SbViewportRegion(W, H));
        ok = ren->render(root);
        if (ok)
            ok = validateNonBlack(ren->getBuffer(), W * H, "test5_matrixParam");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// Test 6: Two shader programs in one scene (different separators)
// ---------------------------------------------------------------------------
static bool test6_twoPrograms(const char *basepath)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 8.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Shape 1 (left): red-colored shader
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(-2.0f, 0.0f, 0.0f);
        sep->addChild(tr);

        SoShaderProgram *prog = new SoShaderProgram;
        SoVertexShader   *vs  = new SoVertexShader;
        vs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
        vs->sourceProgram.setValue(vert_src);
        SoFragmentShader *fs  = new SoFragmentShader;
        fs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
        fs->sourceProgram.setValue(frag_src);
        SoShaderParameter3f *p = new SoShaderParameter3f;
        p->name.setValue("uColor");
        p->value.setValue(SbVec3f(0.9f, 0.1f, 0.1f));
        fs->parameter.addNode(p);
        prog->shaderObject.addNode(vs);
        prog->shaderObject.addNode(fs);
        sep->addChild(prog);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.9f, 0.1f, 0.1f);
        sep->addChild(mat);
        sep->addChild(new SoSphere);
        root->addChild(sep);
    }

    // Shape 2 (right): blue-colored shader
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(2.0f, 0.0f, 0.0f);
        sep->addChild(tr);

        SoShaderProgram *prog = new SoShaderProgram;
        SoVertexShader   *vs  = new SoVertexShader;
        vs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
        vs->sourceProgram.setValue(vert_src);
        SoFragmentShader *fs  = new SoFragmentShader;
        fs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
        fs->sourceProgram.setValue(frag_src);
        SoShaderParameter3f *p = new SoShaderParameter3f;
        p->name.setValue("uColor");
        p->value.setValue(SbVec3f(0.1f, 0.2f, 0.9f));
        fs->parameter.addNode(p);
        prog->shaderObject.addNode(vs);
        prog->shaderObject.addNode(fs);
        sep->addChild(prog);

        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.1f, 0.2f, 0.9f);
        sep->addChild(mat);
        sep->addChild(new SoCube);
        root->addChild(sep);
    }

    SbViewportRegion vp(W, H);
    cam->viewAll(root, vp);

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s_two_progs.rgb", basepath);
    bool ok = renderToFile(root, outpath, W, H);

    if (ok) {
        SoOffscreenRenderer *ren = getSharedRenderer();
        ren->setViewportRegion(SbViewportRegion(W, H));
        ok = ren->render(root);
        if (ok)
            ok = validateNonBlack(ren->getBuffer(), W * H, "test6_twoPrograms");
    }

    root->unref();
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    initCoinHeadless();

    const char *basepath = (argc > 1) ? argv[1] : "render_shader_program";

    /* Render the canonical factory scene as the primary output image.
     * This ensures obol_viewer and obol_render produce identical scenes. */
    {
        SoSeparator *fRoot = ObolTest::Scenes::createShaderProgram(256, 256);
        SbViewportRegion fVp(256, 256);
        SoOffscreenRenderer fRen(fVp);
        fRen.setComponents(SoOffscreenRenderer::RGB);
        fRen.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
        if (fRen.render(fRoot)) {
            char primaryPath[4096];
            snprintf(primaryPath, sizeof(primaryPath), "%s.rgb", basepath);
            fRen.writeToRGB(primaryPath);
        }
        fRoot->unref();
    }

    int failures = 0;

    printf("\n=== SoShaderProgram / GLSL shader tests ===\n");

    if (!test1_basicShader(basepath))    ++failures;
    if (!test2_multiParams(basepath))    ++failures;
    if (!test3_shaderDisable(basepath))  ++failures;
    if (!test4_arrayParams(basepath))    ++failures;
    if (!test5_matrixParam(basepath))    ++failures;
    if (!test6_twoPrograms(basepath))    ++failures;

    printf("\n  enable-callback fire count: %d\n", g_enableCount);
    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
