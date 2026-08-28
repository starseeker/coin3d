#include <gtest/gtest.h>

#include "headless_utils.h"

#include <Inventor/nodes/SoFragmentShader.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShaderParameter.h>
#include <Inventor/nodes/SoShaderProgram.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoVertexShader.h>

#include <climits>

namespace {

const char * vertexProgram =
    "!!ARBvp1.0\n"
    "DP4 result.position.x, state.matrix.mvp.row[0], vertex.position;\n"
    "DP4 result.position.y, state.matrix.mvp.row[1], vertex.position;\n"
    "DP4 result.position.z, state.matrix.mvp.row[2], vertex.position;\n"
    "DP4 result.position.w, state.matrix.mvp.row[3], vertex.position;\n"
    "END\n";

const char * fragmentProgram =
    "!!ARBfp1.0\n"
    "MOV result.color, program.local[0];\n"
    "END\n";

bool runArbShaderParameterScenario()
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 0.0f, 4.0f);
    root->addChild(camera);

    SoShaderProgram * program = new SoShaderProgram;
    SoVertexShader * vertex = new SoVertexShader;
    vertex->sourceType = SoShaderObject::ARB_PROGRAM;
    vertex->sourceProgram = vertexProgram;

    SoFragmentShader * fragment = new SoFragmentShader;
    fragment->sourceType = SoShaderObject::ARB_PROGRAM;
    fragment->sourceProgram = fragmentProgram;

    SoShaderParameter1f * scalar = new SoShaderParameter1f;
    scalar->identifier = 0;
    scalar->value = 0.75f;
    fragment->parameter.addNode(scalar);

    SoShaderParameter3f * vector = new SoShaderParameter3f;
    vector->identifier = 1;
    vector->value.setValue(0.2f, 0.4f, 0.6f);
    fragment->parameter.addNode(vector);

    SoShaderParameterArray1f * array = new SoShaderParameterArray1f;
    array->identifier = 2;
    array->value.set1Value(0, 0.25f);
    array->value.set1Value(1, 0.5f);
    fragment->parameter.addNode(array);

    // Invalid ranges must be diagnosed and ignored without issuing an
    // overflowing GL parameter index.
    SoShaderParameterArray4f * invalid = new SoShaderParameterArray4f;
    invalid->identifier = INT_MAX;
    invalid->value.set1Value(0, SbVec4f(1.0f, 0.0f, 0.0f, 1.0f));
    invalid->value.set1Value(1, SbVec4f(0.0f, 1.0f, 0.0f, 1.0f));
    fragment->parameter.addNode(invalid);

    program->shaderObject.addNode(vertex);
    program->shaderObject.addNode(fragment);
    root->addChild(program);
    root->addChild(new SoSphere);
    camera->viewAll(root, SbViewportRegion(96, 96));

    SoOffscreenRenderer * renderer = getSharedRenderer();
    renderer->setViewportRegion(SbViewportRegion(96, 96));
    renderer->setComponents(SoOffscreenRenderer::RGB);
    // The bundled OSMesa lane does not advertise ARB programs and reports a
    // zero local-parameter limit. It must still traverse this program and
    // reject every invalid range without overflowing a GL index or failing
    // the render. Native-GL shader output is covered by its dedicated render
    // contracts; visible pixels are therefore not a portable assertion here.
    const bool rendered = renderer->render(root) != FALSE;

    root->unref();
    return rendered;
}

} // namespace

TEST(ArbShaderParameters, parameter_ranges)
{
    EXPECT_TRUE(runArbShaderParameterScenario());
}
