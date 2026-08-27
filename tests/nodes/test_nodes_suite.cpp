/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

/**
 * @file test_nodes_suite.cpp
 * @brief Tests for Coin3D SoNode subclasses.
 *
 * Baselined against upstream OBOL_TEST_SUITE blocks.
 *
 * Vanilla sources:
 *   src/nodes/SoAnnotation.cpp - initialized (getTypeId, ref/unref)
 *
 * Also covers SoType system (SoType::createType / removeType) as used
 * throughout the node hierarchy:
 *   src/misc/SoType.cpp - testRemoveType
 */

#include "../test_utils.h"

#include <Inventor/SoType.h>
#include <Inventor/SbName.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbRotation.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoAnnotation.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoMatrixTransform.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoBlinker.h>
#include <Inventor/nodes/SoRotor.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoTriangleStripSet.h>
#include <Inventor/nodes/SoIndexedTriangleStripSet.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoTextureCoordinateBinding.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoEnvironment.h>
#include <Inventor/nodes/SoClipPlane.h>
#include <Inventor/nodes/SoFile.h>
#include <Inventor/nodes/SoInfo.h>
#include <Inventor/nodes/SoLOD.h>
#include <Inventor/nodes/SoVertexProperty.h>
#include <Inventor/nodes/SoShaderProgram.h>
#include <Inventor/nodes/SoFragmentShader.h>
#include <Inventor/nodes/SoVertexShader.h>
#include <Inventor/nodes/SoGeometryShader.h>
#include <Inventor/nodes/SoShaderParameter.h>
#include <Inventor/annex/FXViz/nodes/SoShadowGroup.h>
#include <Inventor/annex/FXViz/nodes/SoShadowStyle.h>
// Dragger headers
#include <Inventor/draggers/SoCenterballDragger.h>
#include <Inventor/draggers/SoDirectionalLightDragger.h>
#include <Inventor/draggers/SoDragPointDragger.h>
#include <Inventor/draggers/SoHandleBoxDragger.h>
#include <Inventor/draggers/SoJackDragger.h>
#include <Inventor/draggers/SoPointLightDragger.h>
#include <Inventor/draggers/SoRotateCylindricalDragger.h>
#include <Inventor/draggers/SoRotateDiscDragger.h>
#include <Inventor/draggers/SoRotateSphericalDragger.h>
#include <Inventor/draggers/SoScale1Dragger.h>
#include <Inventor/draggers/SoScale2Dragger.h>
#include <Inventor/draggers/SoScale2UniformDragger.h>
#include <Inventor/draggers/SoScaleUniformDragger.h>
#include <Inventor/draggers/SoSpotLightDragger.h>
#include <Inventor/draggers/SoTabBoxDragger.h>
#include <Inventor/draggers/SoTabPlaneDragger.h>
#include <Inventor/draggers/SoTrackballDragger.h>
#include <Inventor/draggers/SoTransformBoxDragger.h>
#include <Inventor/draggers/SoTransformerDragger.h>
#include <Inventor/draggers/SoTranslate1Dragger.h>
#include <Inventor/draggers/SoTranslate2Dragger.h>

using namespace ObolTest;

// Factory function needed by SoType::createType
static void* createDummyInstance(void*) { return reinterpret_cast<void*>(0x1); }

TEST(NodesSuite, SoAnnotationClassInitialized)
{
    SoAnnotation* node = new SoAnnotation;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoAnnotation has bad typeId";
}

// -----------------------------------------------------------------------
// SoType: createType / removeType
// Baseline: src/misc/SoType.cpp OBOL_TEST_SUITE (testRemoveType)
// -----------------------------------------------------------------------

TEST(NodesSuite, SoTypeCreateTypeAndRemoveType)
{
    const SbName typeName("__TestNodeType__");

    // Should not exist yet
    bool notYet = (SoType::fromName(typeName) == SoType::badType());

    // Create it
    SoType::createType(SoNode::getClassTypeId(), typeName,
                       createDummyInstance, 0);
    bool created = (SoType::fromName(typeName) != SoType::badType());

    // Remove it
    bool removed = SoType::removeType(typeName);
    bool gone    = (SoType::fromName(typeName) == SoType::badType());

    bool pass = notYet && created && removed && gone;
    EXPECT_TRUE(pass) << "SoType createType/removeType did not behave as expected";
}

// -----------------------------------------------------------------------
// SoNode: isOfType hierarchy
// -----------------------------------------------------------------------

TEST(NodesSuite, SoCubeIsOfTypeSoNode)
{
    SoCube* cube = new SoCube;
    cube->ref();
    bool pass = cube->isOfType(SoNode::getClassTypeId());
    cube->unref();
    EXPECT_TRUE(pass) << "SoCube should be of type SoNode";
}

TEST(NodesSuite, SoSeparatorIsOfTypeSoGroup)
{
    SoSeparator* sep = new SoSeparator;
    sep->ref();
    bool pass = sep->isOfType(SoGroup::getClassTypeId());
    sep->unref();
    EXPECT_TRUE(pass) << "SoSeparator should be a SoGroup";
}

// -----------------------------------------------------------------------
// SoGroup / SoSeparator: child management
// -----------------------------------------------------------------------

TEST(NodesSuite, SoSeparatorAddChildGetNumChildrenRemoveChild)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoCube* c1 = new SoCube;
    SoCube* c2 = new SoCube;
    root->addChild(c1);
    root->addChild(c2);

    bool pass = (root->getNumChildren() == 2);
    root->removeChild(c1);
    pass = pass && (root->getNumChildren() == 1);
    pass = pass && (root->getChild(0) == c2);

    root->unref();
    EXPECT_TRUE(pass) << "SoSeparator child management failed";
}

TEST(NodesSuite, SoSeparatorInsertChild)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    SoCube*   c1 = new SoCube;
    SoSphere* s1 = new SoSphere;
    root->addChild(c1);
    root->insertChild(s1, 0); // insert at front

    bool pass = (root->getNumChildren() == 2) &&
                (root->getChild(0) == s1) &&
                (root->getChild(1) == c1);
    root->unref();
    EXPECT_TRUE(pass) << "SoSeparator insertChild failed";
}

// -----------------------------------------------------------------------
// SoNode: setName / getName
// -----------------------------------------------------------------------

TEST(NodesSuite, SoNodeSetNameGetName)
{
    SoCube* cube = new SoCube;
    cube->ref();
    cube->setName("TestCube");
    bool pass = (cube->getName() == SbName("TestCube"));
    cube->unref();
    EXPECT_TRUE(pass) << "SoNode setName/getName failed";
}

// -----------------------------------------------------------------------
// SoNode: SoNode::getByName
// -----------------------------------------------------------------------

TEST(NodesSuite, SoNodeGetByName)
{
    SoCylinder* cyl = new SoCylinder;
    cyl->ref();
    cyl->setName("UniqueCylinder");
    SoNode* found = SoNode::getByName(SbName("UniqueCylinder"));
    bool pass = (found == cyl);
    cyl->unref();
    EXPECT_TRUE(pass) << "SoNode::getByName did not find the named node";
}

// -----------------------------------------------------------------------
// Geometry nodes: default field values
// -----------------------------------------------------------------------

TEST(NodesSuite, SoCubeDefaultFields)
{
    SoCube* cube = new SoCube;
    cube->ref();
    bool pass = (cube->width.getValue()  == 2.0f) &&
                (cube->height.getValue() == 2.0f) &&
                (cube->depth.getValue()  == 2.0f);
    cube->unref();
    EXPECT_TRUE(pass) << "SoCube default field values wrong";
}

TEST(NodesSuite, SoSphereDefaultRadius)
{
    SoSphere* sphere = new SoSphere;
    sphere->ref();
    bool pass = (sphere->radius.getValue() == 1.0f);
    sphere->unref();
    EXPECT_TRUE(pass) << "SoSphere default radius != 1.0";
}

TEST(NodesSuite, SoConeDefaultFields)
{
    SoCone* cone = new SoCone;
    cone->ref();
    bool pass = (cone->bottomRadius.getValue() == 1.0f) &&
                (cone->height.getValue()        == 2.0f);
    cone->unref();
    EXPECT_TRUE(pass) << "SoCone default field values wrong";
}

// -----------------------------------------------------------------------
// SoMaterial: default field count
// -----------------------------------------------------------------------

TEST(NodesSuite, SoMaterialDefaultDiffuseColorField)
{
    SoMaterial* mat = new SoMaterial;
    mat->ref();
    // Default diffuseColor is one value (0.8, 0.8, 0.8)
    bool pass = (mat->diffuseColor.getNum() == 1);
    mat->unref();
    EXPECT_TRUE(pass) << "SoMaterial default diffuseColor should have 1 value";
}

// -----------------------------------------------------------------------
// SoCylinder: default field values
// -----------------------------------------------------------------------

TEST(NodesSuite, SoCylinderDefaultFields)
{
    SoCylinder* cyl = new SoCylinder;
    cyl->ref();
    bool pass = (cyl->radius.getValue() == 1.0f) &&
                (cyl->height.getValue() == 2.0f);
    cyl->unref();
    EXPECT_TRUE(pass) << "SoCylinder default field values wrong";
}

// -----------------------------------------------------------------------
// Light nodes: default fields
// -----------------------------------------------------------------------

TEST(NodesSuite, SoDirectionalLightClassInitialized)
{
    SoDirectionalLight* light = new SoDirectionalLight;
    light->ref();
    bool pass = (light->getTypeId() != SoType::badType());
    light->unref();
    EXPECT_TRUE(pass) << "SoDirectionalLight has bad type";
}

TEST(NodesSuite, SoPointLightClassInitialized)
{
    SoPointLight* light = new SoPointLight;
    light->ref();
    bool pass = (light->getTypeId() != SoType::badType());
    light->unref();
    EXPECT_TRUE(pass) << "SoPointLight has bad type";
}

TEST(NodesSuite, SoSpotLightClassInitialized)
{
    SoSpotLight* light = new SoSpotLight;
    light->ref();
    bool pass = (light->getTypeId() != SoType::badType());
    light->unref();
    EXPECT_TRUE(pass) << "SoSpotLight has bad type";
}

// -----------------------------------------------------------------------
// Transform nodes: default field values
// -----------------------------------------------------------------------

TEST(NodesSuite, SoTranslationDefaultTranslation)
{
    SoTranslation* t = new SoTranslation;
    t->ref();
    SbVec3f v = t->translation.getValue();
    bool pass = (v == SbVec3f(0, 0, 0));
    t->unref();
    EXPECT_TRUE(pass) << "SoTranslation default translation != (0,0,0)";
}

TEST(NodesSuite, SoRotationDefaultRotation)
{
    SoRotation* r = new SoRotation;
    r->ref();
    // Default rotation is identity (0,0,1,0) = zero angle around z
    SbVec3f axis; float angle;
    r->rotation.getValue().getValue(axis, angle);
    bool pass = (angle == 0.0f);
    r->unref();
    EXPECT_TRUE(pass) << "SoRotation default rotation is not identity";
}

TEST(NodesSuite, SoScaleDefaultScaleFactor)
{
    SoScale* s = new SoScale;
    s->ref();
    SbVec3f sf = s->scaleFactor.getValue();
    bool pass = (sf == SbVec3f(1, 1, 1));
    s->unref();
    EXPECT_TRUE(pass) << "SoScale default scaleFactor != (1,1,1)";
}

TEST(NodesSuite, SoTransformDefaultTranslation)
{
    SoTransform* xf = new SoTransform;
    xf->ref();
    SbVec3f t = xf->translation.getValue();
    bool pass = (t == SbVec3f(0, 0, 0));
    xf->unref();
    EXPECT_TRUE(pass) << "SoTransform default translation != (0,0,0)";
}

// -----------------------------------------------------------------------
// Camera nodes: default fields
// -----------------------------------------------------------------------

TEST(NodesSuite, SoPerspectiveCameraClassInitialized)
{
    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    cam->ref();
    bool pass = (cam->getTypeId() != SoType::badType());
    cam->unref();
    EXPECT_TRUE(pass) << "SoPerspectiveCamera has bad type";
}

TEST(NodesSuite, SoOrthographicCameraClassInitialized)
{
    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->ref();
    bool pass = (cam->getTypeId() != SoType::badType());
    cam->unref();
    EXPECT_TRUE(pass) << "SoOrthographicCamera has bad type";
}

// -----------------------------------------------------------------------
// Camera default field values
// Baseline: SoCamera base class documented defaults
// -----------------------------------------------------------------------

TEST(NodesSuite, SoPerspectiveCameraDefaultNearDistance)
{
    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    cam->ref();
    // Default nearDistance is 1.0
    bool pass = (cam->nearDistance.getValue() == 1.0f);
    cam->unref();
    EXPECT_TRUE(pass) << "SoPerspectiveCamera nearDistance default != 1.0";
}

TEST(NodesSuite, SoPerspectiveCameraDefaultFarDistance)
{
    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    cam->ref();
    // Default farDistance is 10.0
    bool pass = (cam->farDistance.getValue() == 10.0f);
    cam->unref();
    EXPECT_TRUE(pass) << "SoPerspectiveCamera farDistance default != 10.0";
}

TEST(NodesSuite, SoOrthographicCameraDefaultHeight)
{
    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->ref();
    // Default height is 2.0
    bool pass = (cam->height.getValue() == 2.0f);
    cam->unref();
    EXPECT_TRUE(pass) << "SoOrthographicCamera height default != 2.0";
}

// -----------------------------------------------------------------------
// SoSwitch: whichChild default value
// -----------------------------------------------------------------------

TEST(NodesSuite, SoSwitchDefaultWhichChild)
{
    SoSwitch* sw = new SoSwitch;
    sw->ref();
    // Default whichChild is SO_SWITCH_NONE (-1)
    bool pass = (sw->whichChild.getValue() == SO_SWITCH_NONE);
    sw->unref();
    EXPECT_TRUE(pass) << "SoSwitch default whichChild != SO_SWITCH_NONE";
}

// -----------------------------------------------------------------------
// Geometry support nodes: class initialized
// -----------------------------------------------------------------------

TEST(NodesSuite, SoCoordinate3ClassInitialized)
{
    SoCoordinate3* coord = new SoCoordinate3;
    coord->ref();
    bool pass = (coord->getTypeId() != SoType::badType());
    coord->unref();
    EXPECT_TRUE(pass) << "SoCoordinate3 has bad type";
}

TEST(NodesSuite, SoNormalClassInitialized)
{
    SoNormal* norm = new SoNormal;
    norm->ref();
    bool pass = (norm->getTypeId() != SoType::badType());
    norm->unref();
    EXPECT_TRUE(pass) << "SoNormal has bad type";
}

// -----------------------------------------------------------------------
// Shader nodes: class initialized
// Baseline: src/shaders/SoShaderProgram.cpp, SoFragmentShader.cpp, etc.
// -----------------------------------------------------------------------

TEST(NodesSuite, SoShaderProgramClassInitialized)
{
    SoShaderProgram* prog = new SoShaderProgram;
    prog->ref();
    bool pass = (prog->getTypeId() != SoType::badType());
    prog->unref();
    EXPECT_TRUE(pass) << "SoShaderProgram has bad type";
}

TEST(NodesSuite, SoFragmentShaderClassInitialized)
{
    SoFragmentShader* fs = new SoFragmentShader;
    fs->ref();
    bool pass = (fs->getTypeId() != SoType::badType());
    fs->unref();
    EXPECT_TRUE(pass) << "SoFragmentShader has bad type";
}

TEST(NodesSuite, SoVertexShaderClassInitialized)
{
    SoVertexShader* vs = new SoVertexShader;
    vs->ref();
    bool pass = (vs->getTypeId() != SoType::badType());
    vs->unref();
    EXPECT_TRUE(pass) << "SoVertexShader has bad type";
}

TEST(NodesSuite, SoGeometryShaderClassInitialized)
{
    SoGeometryShader* gs = new SoGeometryShader;
    gs->ref();
    bool pass = (gs->getTypeId() != SoType::badType());
    gs->unref();
    EXPECT_TRUE(pass) << "SoGeometryShader has bad type";
}

// -----------------------------------------------------------------------
// Shadow nodes: class initialized
// Baseline: src/shadows/SoShadowGroup.cpp, SoShadowStyle.cpp OBOL_TEST_SUITE
// -----------------------------------------------------------------------

TEST(NodesSuite, SoShadowGroupClassInitialized)
{
    SoShadowGroup* node = new SoShadowGroup;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoShadowGroup has bad type";
}

TEST(NodesSuite, SoShadowStyleClassInitialized)
{
    SoShadowStyle* node = new SoShadowStyle;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoShadowStyle has bad type";
}

// -----------------------------------------------------------------------
// Additional transform nodes: default fields
// -----------------------------------------------------------------------

TEST(NodesSuite, SoRotationXYZClassInitialized)
{
    SoRotationXYZ* node = new SoRotationXYZ;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoRotationXYZ has bad type";
}

TEST(NodesSuite, SoMatrixTransformClassInitialized)
{
    SoMatrixTransform* node = new SoMatrixTransform;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoMatrixTransform has bad type";
}

// -----------------------------------------------------------------------
// Property / appearance nodes
// -----------------------------------------------------------------------

TEST(NodesSuite, SoLightModelClassInitialized)
{
    SoLightModel* node = new SoLightModel;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoLightModel has bad type";
}

TEST(NodesSuite, SoDrawStyleClassInitialized)
{
    SoDrawStyle* node = new SoDrawStyle;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoDrawStyle has bad type";
}

TEST(NodesSuite, SoComplexityClassInitialized)
{
    SoComplexity* node = new SoComplexity;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoComplexity has bad type";
}

TEST(NodesSuite, SoEnvironmentClassInitialized)
{
    SoEnvironment* node = new SoEnvironment;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoEnvironment has bad type";
}

TEST(NodesSuite, SoClipPlaneClassInitialized)
{
    SoClipPlane* node = new SoClipPlane;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoClipPlane has bad type";
}

TEST(NodesSuite, SoNormalBindingClassInitialized)
{
    SoNormalBinding* node = new SoNormalBinding;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoNormalBinding has bad type";
}

TEST(NodesSuite, SoMaterialBindingClassInitialized)
{
    SoMaterialBinding* node = new SoMaterialBinding;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoMaterialBinding has bad type";
}

TEST(NodesSuite, SoVertexPropertyClassInitialized)
{
    SoVertexProperty* node = new SoVertexProperty;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoVertexProperty has bad type";
}

// -----------------------------------------------------------------------
// Texture nodes
// -----------------------------------------------------------------------

TEST(NodesSuite, SoTexture2ClassInitialized)
{
    SoTexture2* node = new SoTexture2;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoTexture2 has bad type";
}

TEST(NodesSuite, SoTextureCoordinate2ClassInitialized)
{
    SoTextureCoordinate2* node = new SoTextureCoordinate2;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoTextureCoordinate2 has bad type";
}

TEST(NodesSuite, SoTextureCoordinateBindingClassInitialized)
{
    SoTextureCoordinateBinding* node = new SoTextureCoordinateBinding;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoTextureCoordinateBinding has bad type";
}

// -----------------------------------------------------------------------
// Geometry / shape nodes
// -----------------------------------------------------------------------

TEST(NodesSuite, SoFaceSetClassInitialized)
{
    SoFaceSet* node = new SoFaceSet;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoFaceSet has bad type";
}

TEST(NodesSuite, SoIndexedFaceSetClassInitialized)
{
    SoIndexedFaceSet* node = new SoIndexedFaceSet;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoIndexedFaceSet has bad type";
}

TEST(NodesSuite, SoTriangleStripSetClassInitialized)
{
    SoTriangleStripSet* node = new SoTriangleStripSet;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoTriangleStripSet has bad type";
}

TEST(NodesSuite, SoIndexedTriangleStripSetClassInitialized)
{
    SoIndexedTriangleStripSet* node = new SoIndexedTriangleStripSet;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoIndexedTriangleStripSet has bad type";
}

TEST(NodesSuite, SoLineSetClassInitialized)
{
    SoLineSet* node = new SoLineSet;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoLineSet has bad type";
}

TEST(NodesSuite, SoIndexedLineSetClassInitialized)
{
    SoIndexedLineSet* node = new SoIndexedLineSet;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoIndexedLineSet has bad type";
}

TEST(NodesSuite, SoPointSetClassInitialized)
{
    SoPointSet* node = new SoPointSet;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoPointSet has bad type";
}

// -----------------------------------------------------------------------
// Misc utility nodes
// -----------------------------------------------------------------------

TEST(NodesSuite, SoFileClassInitialized)
{
    SoFile* node = new SoFile;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoFile has bad type";
}

TEST(NodesSuite, SoInfoClassInitialized)
{
    SoInfo* node = new SoInfo;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoInfo has bad type";
}

TEST(NodesSuite, SoLODClassInitialized)
{
    SoLOD* node = new SoLOD;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoLOD has bad type";
}

// -----------------------------------------------------------------------
// Animation nodes
// -----------------------------------------------------------------------

TEST(NodesSuite, SoBlinkerClassInitialized)
{
    SoBlinker* node = new SoBlinker;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoBlinker has bad type";
}

TEST(NodesSuite, SoRotorClassInitialized)
{
    SoRotor* node = new SoRotor;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoRotor has bad type";
}

// -----------------------------------------------------------------------
// Shader parameter nodes
// Baseline: src/shaders/SoShaderParameter.cpp OBOL_TEST_SUITE (initialized)
// -----------------------------------------------------------------------

TEST(NodesSuite, SoShaderParameter1fClassInitialized)
{
    SoShaderParameter1f* node = new SoShaderParameter1f;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoShaderParameter1f has bad type";
}

TEST(NodesSuite, SoShaderParameter1iClassInitialized)
{
    SoShaderParameter1i* node = new SoShaderParameter1i;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoShaderParameter1i has bad type";
}

TEST(NodesSuite, SoShaderParameter2fClassInitialized)
{
    SoShaderParameter2f* node = new SoShaderParameter2f;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoShaderParameter2f has bad type";
}

TEST(NodesSuite, SoShaderParameter2iClassInitialized)
{
    SoShaderParameter2i* node = new SoShaderParameter2i;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoShaderParameter2i has bad type";
}

TEST(NodesSuite, SoShaderParameter3fClassInitialized)
{
    SoShaderParameter3f* node = new SoShaderParameter3f;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoShaderParameter3f has bad type";
}

TEST(NodesSuite, SoShaderParameter3iClassInitialized)
{
    SoShaderParameter3i* node = new SoShaderParameter3i;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoShaderParameter3i has bad type";
}

TEST(NodesSuite, SoShaderParameter4fClassInitialized)
{
    SoShaderParameter4f* node = new SoShaderParameter4f;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoShaderParameter4f has bad type";
}

TEST(NodesSuite, SoShaderParameter4iClassInitialized)
{
    SoShaderParameter4i* node = new SoShaderParameter4i;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoShaderParameter4i has bad type";
}

TEST(NodesSuite, SoShaderParameterArray1fClassInitialized)
{
    SoShaderParameterArray1f* node = new SoShaderParameterArray1f;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoShaderParameterArray1f has bad type";
}

TEST(NodesSuite, SoShaderParameterArray1iClassInitialized)
{
    SoShaderParameterArray1i* node = new SoShaderParameterArray1i;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoShaderParameterArray1i has bad type";
}

TEST(NodesSuite, SoShaderParameterMatrixClassInitialized)
{
    SoShaderParameterMatrix* node = new SoShaderParameterMatrix;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoShaderParameterMatrix has bad type";
}

TEST(NodesSuite, SoShaderParameterMatrixArrayClassInitialized)
{
    SoShaderParameterMatrixArray* node = new SoShaderParameterMatrixArray;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoShaderParameterMatrixArray has bad type";
}

TEST(NodesSuite, SoShaderStateMatrixParameterClassInitialized)
{
    SoShaderStateMatrixParameter* node = new SoShaderStateMatrixParameter;
    node->ref();
    bool pass = (node->getTypeId() != SoType::badType());
    node->unref();
    EXPECT_TRUE(pass) << "SoShaderStateMatrixParameter has bad type";
}

// -----------------------------------------------------------------------
// Dragger class types: all dragger classes registered in the type system
// Baseline: src/draggers/SoXxxDragger.cpp OBOL_TEST_SUITE blocks
// -----------------------------------------------------------------------

TEST(NodesSuite, AllDraggerClassTypesRegistered)
{
    bool pass =
        (SoCenterballDragger::getClassTypeId()       != SoType::badType()) &&
        (SoDirectionalLightDragger::getClassTypeId() != SoType::badType()) &&
        (SoDragPointDragger::getClassTypeId()        != SoType::badType()) &&
        (SoHandleBoxDragger::getClassTypeId()        != SoType::badType()) &&
        (SoJackDragger::getClassTypeId()             != SoType::badType()) &&
        (SoPointLightDragger::getClassTypeId()       != SoType::badType()) &&
        (SoRotateCylindricalDragger::getClassTypeId()!= SoType::badType()) &&
        (SoRotateDiscDragger::getClassTypeId()       != SoType::badType()) &&
        (SoRotateSphericalDragger::getClassTypeId()  != SoType::badType()) &&
        (SoScale1Dragger::getClassTypeId()           != SoType::badType()) &&
        (SoScale2Dragger::getClassTypeId()           != SoType::badType()) &&
        (SoScale2UniformDragger::getClassTypeId()    != SoType::badType()) &&
        (SoScaleUniformDragger::getClassTypeId()     != SoType::badType()) &&
        (SoSpotLightDragger::getClassTypeId()        != SoType::badType()) &&
        (SoTabBoxDragger::getClassTypeId()           != SoType::badType()) &&
        (SoTabPlaneDragger::getClassTypeId()         != SoType::badType()) &&
        (SoTrackballDragger::getClassTypeId()        != SoType::badType()) &&
        (SoTransformBoxDragger::getClassTypeId()     != SoType::badType()) &&
        (SoTransformerDragger::getClassTypeId()      != SoType::badType()) &&
        (SoTranslate1Dragger::getClassTypeId()       != SoType::badType()) &&
        (SoTranslate2Dragger::getClassTypeId()       != SoType::badType());
    EXPECT_TRUE(pass) << "One or more dragger class types not registered";
}

// -----------------------------------------------------------------------
// Dragger deep-copy: construction and copy() for single-piece draggers
// Baseline: src/draggers/SoTransformerDragger.cpp OBOL_TEST_SUITE
//           dragger_deep_copy test
//
// Compound draggers (SoCenterballDragger, SoHandleBoxDragger,
// SoTabBoxDragger, SoTabPlaneDragger, SoTransformerDragger,
// SoTrackballDragger, etc.) load IV resources at construction time and
// require a rendering context (SoDB::ContextManager).  They are tested
// visually in the rendering suite.
//
// Single-piece draggers can be constructed and deep-copied without a
// rendering context and are verified here.
// -----------------------------------------------------------------------

TEST(NodesSuite, SoTranslate1DraggerDeepCopyProducesIndependentNodes)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    root->addChild(new SoTranslate1Dragger);
    SoSeparator* copy = static_cast<SoSeparator*>(root->copy());
    bool pass = (copy != nullptr);
    if (pass) {
        copy->ref();
        pass = (copy->getNumChildren() == 1) &&
               (copy->getChild(0) != root->getChild(0));
        copy->unref();
    }
    root->unref();
    EXPECT_TRUE(pass) << "SoTranslate1Dragger deep copy failed or shared child pointer";
}

TEST(NodesSuite, SoTranslate2DraggerDeepCopyProducesIndependentNodes)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    root->addChild(new SoTranslate2Dragger);
    SoSeparator* copy = static_cast<SoSeparator*>(root->copy());
    bool pass = (copy != nullptr);
    if (pass) {
        copy->ref();
        pass = (copy->getNumChildren() == 1) &&
               (copy->getChild(0) != root->getChild(0));
        copy->unref();
    }
    root->unref();
    EXPECT_TRUE(pass) << "SoTranslate2Dragger deep copy failed or shared child pointer";
}

TEST(NodesSuite, SoDragPointDraggerDeepCopyProducesIndependentNodes)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    root->addChild(new SoDragPointDragger);
    SoSeparator* copy = static_cast<SoSeparator*>(root->copy());
    bool pass = (copy != nullptr);
    if (pass) {
        copy->ref();
        pass = (copy->getNumChildren() == 1) &&
               (copy->getChild(0) != root->getChild(0));
        copy->unref();
    }
    root->unref();
    EXPECT_TRUE(pass) << "SoDragPointDragger deep copy failed or shared child pointer";
}

TEST(NodesSuite, SoRotateDiscDraggerDeepCopyProducesIndependentNodes)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    root->addChild(new SoRotateDiscDragger);
    SoSeparator* copy = static_cast<SoSeparator*>(root->copy());
    bool pass = (copy != nullptr);
    if (pass) {
        copy->ref();
        pass = (copy->getNumChildren() == 1) &&
               (copy->getChild(0) != root->getChild(0));
        copy->unref();
    }
    root->unref();
    EXPECT_TRUE(pass) << "SoRotateDiscDragger deep copy failed or shared child pointer";
}

TEST(NodesSuite, SoRotateCylindricalDraggerDeepCopyProducesIndependentNodes)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    root->addChild(new SoRotateCylindricalDragger);
    SoSeparator* copy = static_cast<SoSeparator*>(root->copy());
    bool pass = (copy != nullptr);
    if (pass) {
        copy->ref();
        pass = (copy->getNumChildren() == 1) &&
               (copy->getChild(0) != root->getChild(0));
        copy->unref();
    }
    root->unref();
    EXPECT_TRUE(pass) << "SoRotateCylindricalDragger deep copy failed or shared child pointer";
}

TEST(NodesSuite, SoRotateSphericalDraggerDeepCopyProducesIndependentNodes)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    root->addChild(new SoRotateSphericalDragger);
    SoSeparator* copy = static_cast<SoSeparator*>(root->copy());
    bool pass = (copy != nullptr);
    if (pass) {
        copy->ref();
        pass = (copy->getNumChildren() == 1) &&
               (copy->getChild(0) != root->getChild(0));
        copy->unref();
    }
    root->unref();
    EXPECT_TRUE(pass) << "SoRotateSphericalDragger deep copy failed or shared child pointer";
}

TEST(NodesSuite, SoScale1DraggerDeepCopyProducesIndependentNodes)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    root->addChild(new SoScale1Dragger);
    SoSeparator* copy = static_cast<SoSeparator*>(root->copy());
    bool pass = (copy != nullptr);
    if (pass) {
        copy->ref();
        pass = (copy->getNumChildren() == 1) &&
               (copy->getChild(0) != root->getChild(0));
        copy->unref();
    }
    root->unref();
    EXPECT_TRUE(pass) << "SoScale1Dragger deep copy failed or shared child pointer";
}

TEST(NodesSuite, SoScale2DraggerDeepCopyProducesIndependentNodes)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    root->addChild(new SoScale2Dragger);
    SoSeparator* copy = static_cast<SoSeparator*>(root->copy());
    bool pass = (copy != nullptr);
    if (pass) {
        copy->ref();
        pass = (copy->getNumChildren() == 1) &&
               (copy->getChild(0) != root->getChild(0));
        copy->unref();
    }
    root->unref();
    EXPECT_TRUE(pass) << "SoScale2Dragger deep copy failed or shared child pointer";
}

TEST(NodesSuite, SoDirectionalLightDraggerDeepCopyProducesIndependentNodes)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    root->addChild(new SoDirectionalLightDragger);
    SoSeparator* copy = static_cast<SoSeparator*>(root->copy());
    bool pass = (copy != nullptr);
    if (pass) {
        copy->ref();
        pass = (copy->getNumChildren() == 1) &&
               (copy->getChild(0) != root->getChild(0));
        copy->unref();
    }
    root->unref();
    EXPECT_TRUE(pass) << "SoDirectionalLightDragger deep copy failed or shared child pointer";
}
