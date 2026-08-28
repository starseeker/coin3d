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
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoAnnotation has bad typeId";
    node->unref();
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

    EXPECT_TRUE(notYet && created && removed && gone) << "SoType createType/removeType did not behave as expected";
}

// -----------------------------------------------------------------------
// SoNode: isOfType hierarchy
// -----------------------------------------------------------------------

TEST(NodesSuite, SoCubeIsOfTypeSoNode)
{
    SoCube* cube = new SoCube;
    cube->ref();
    EXPECT_TRUE(cube->isOfType(SoNode::getClassTypeId())) << "SoCube should be of type SoNode";
    cube->unref();
}

TEST(NodesSuite, SoSeparatorIsOfTypeSoGroup)
{
    SoSeparator* sep = new SoSeparator;
    sep->ref();
    EXPECT_TRUE(sep->isOfType(SoGroup::getClassTypeId())) << "SoSeparator should be a SoGroup";
    sep->unref();
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

    EXPECT_EQ(root->getNumChildren(), 2);
    root->removeChild(c1);
    EXPECT_EQ(root->getNumChildren(), 1);
    EXPECT_EQ(root->getChild(0), c2);

    root->unref();
}

TEST(NodesSuite, SoSeparatorInsertChild)
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    SoCube*   c1 = new SoCube;
    SoSphere* s1 = new SoSphere;
    root->addChild(c1);
    root->insertChild(s1, 0); // insert at front

    EXPECT_TRUE((root->getNumChildren() == 2) &&
                (root->getChild(0) == s1) &&
                (root->getChild(1) == c1)) << "SoSeparator insertChild failed";
    root->unref();
}

// -----------------------------------------------------------------------
// SoNode: setName / getName
// -----------------------------------------------------------------------

TEST(NodesSuite, SoNodeSetNameGetName)
{
    SoCube* cube = new SoCube;
    cube->ref();
    cube->setName("TestCube");
    EXPECT_TRUE((cube->getName() == SbName("TestCube"))) << "SoNode setName/getName failed";
    cube->unref();
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
    EXPECT_TRUE((found == cyl)) << "SoNode::getByName did not find the named node";
    cyl->unref();
}

// -----------------------------------------------------------------------
// Geometry nodes: default field values
// -----------------------------------------------------------------------

TEST(NodesSuite, SoCubeDefaultFields)
{
    SoCube* cube = new SoCube;
    cube->ref();
    EXPECT_TRUE((cube->width.getValue()  == 2.0f) &&
                (cube->height.getValue() == 2.0f) &&
                (cube->depth.getValue()  == 2.0f)) << "SoCube default field values wrong";
    cube->unref();
}

TEST(NodesSuite, SoSphereDefaultRadius)
{
    SoSphere* sphere = new SoSphere;
    sphere->ref();
    EXPECT_TRUE((sphere->radius.getValue() == 1.0f)) << "SoSphere default radius != 1.0";
    sphere->unref();
}

TEST(NodesSuite, SoConeDefaultFields)
{
    SoCone* cone = new SoCone;
    cone->ref();
    EXPECT_TRUE((cone->bottomRadius.getValue() == 1.0f) &&
                (cone->height.getValue()        == 2.0f)) << "SoCone default field values wrong";
    cone->unref();
}

// -----------------------------------------------------------------------
// SoMaterial: default field count
// -----------------------------------------------------------------------

TEST(NodesSuite, SoMaterialDefaultDiffuseColorField)
{
    SoMaterial* mat = new SoMaterial;
    mat->ref();
    // Default diffuseColor is one value (0.8, 0.8, 0.8)
    EXPECT_TRUE((mat->diffuseColor.getNum() == 1)) << "SoMaterial default diffuseColor should have 1 value";
    mat->unref();
}

// -----------------------------------------------------------------------
// SoCylinder: default field values
// -----------------------------------------------------------------------

TEST(NodesSuite, SoCylinderDefaultFields)
{
    SoCylinder* cyl = new SoCylinder;
    cyl->ref();
    EXPECT_TRUE((cyl->radius.getValue() == 1.0f) &&
                (cyl->height.getValue() == 2.0f)) << "SoCylinder default field values wrong";
    cyl->unref();
}

// -----------------------------------------------------------------------
// Light nodes: default fields
// -----------------------------------------------------------------------

TEST(NodesSuite, SoDirectionalLightClassInitialized)
{
    SoDirectionalLight* light = new SoDirectionalLight;
    light->ref();
    EXPECT_TRUE((light->getTypeId() != SoType::badType())) << "SoDirectionalLight has bad type";
    light->unref();
}

TEST(NodesSuite, SoPointLightClassInitialized)
{
    SoPointLight* light = new SoPointLight;
    light->ref();
    EXPECT_TRUE((light->getTypeId() != SoType::badType())) << "SoPointLight has bad type";
    light->unref();
}

TEST(NodesSuite, SoSpotLightClassInitialized)
{
    SoSpotLight* light = new SoSpotLight;
    light->ref();
    EXPECT_TRUE((light->getTypeId() != SoType::badType())) << "SoSpotLight has bad type";
    light->unref();
}

// -----------------------------------------------------------------------
// Transform nodes: default field values
// -----------------------------------------------------------------------

TEST(NodesSuite, SoTranslationDefaultTranslation)
{
    SoTranslation* t = new SoTranslation;
    t->ref();
    SbVec3f v = t->translation.getValue();
    EXPECT_TRUE((v == SbVec3f(0, 0, 0))) << "SoTranslation default translation != (0,0,0)";
    t->unref();
}

TEST(NodesSuite, SoRotationDefaultRotation)
{
    SoRotation* r = new SoRotation;
    r->ref();
    // Default rotation is identity (0,0,1,0) = zero angle around z
    SbVec3f axis; float angle;
    r->rotation.getValue().getValue(axis, angle);
    EXPECT_TRUE((angle == 0.0f)) << "SoRotation default rotation is not identity";
    r->unref();
}

TEST(NodesSuite, SoScaleDefaultScaleFactor)
{
    SoScale* s = new SoScale;
    s->ref();
    SbVec3f sf = s->scaleFactor.getValue();
    EXPECT_TRUE((sf == SbVec3f(1, 1, 1))) << "SoScale default scaleFactor != (1,1,1)";
    s->unref();
}

TEST(NodesSuite, SoTransformDefaultTranslation)
{
    SoTransform* xf = new SoTransform;
    xf->ref();
    SbVec3f t = xf->translation.getValue();
    EXPECT_TRUE((t == SbVec3f(0, 0, 0))) << "SoTransform default translation != (0,0,0)";
    xf->unref();
}

// -----------------------------------------------------------------------
// Camera nodes: default fields
// -----------------------------------------------------------------------

TEST(NodesSuite, SoPerspectiveCameraClassInitialized)
{
    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    cam->ref();
    EXPECT_TRUE((cam->getTypeId() != SoType::badType())) << "SoPerspectiveCamera has bad type";
    cam->unref();
}

TEST(NodesSuite, SoOrthographicCameraClassInitialized)
{
    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->ref();
    EXPECT_TRUE((cam->getTypeId() != SoType::badType())) << "SoOrthographicCamera has bad type";
    cam->unref();
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
    EXPECT_TRUE((cam->nearDistance.getValue() == 1.0f)) << "SoPerspectiveCamera nearDistance default != 1.0";
    cam->unref();
}

TEST(NodesSuite, SoPerspectiveCameraDefaultFarDistance)
{
    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    cam->ref();
    // Default farDistance is 10.0
    EXPECT_TRUE((cam->farDistance.getValue() == 10.0f)) << "SoPerspectiveCamera farDistance default != 10.0";
    cam->unref();
}

TEST(NodesSuite, SoOrthographicCameraDefaultHeight)
{
    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->ref();
    // Default height is 2.0
    EXPECT_TRUE((cam->height.getValue() == 2.0f)) << "SoOrthographicCamera height default != 2.0";
    cam->unref();
}

// -----------------------------------------------------------------------
// SoSwitch: whichChild default value
// -----------------------------------------------------------------------

TEST(NodesSuite, SoSwitchDefaultWhichChild)
{
    SoSwitch* sw = new SoSwitch;
    sw->ref();
    // Default whichChild is SO_SWITCH_NONE (-1)
    EXPECT_TRUE((sw->whichChild.getValue() == SO_SWITCH_NONE)) << "SoSwitch default whichChild != SO_SWITCH_NONE";
    sw->unref();
}

// -----------------------------------------------------------------------
// Geometry support nodes: class initialized
// -----------------------------------------------------------------------

TEST(NodesSuite, SoCoordinate3ClassInitialized)
{
    SoCoordinate3* coord = new SoCoordinate3;
    coord->ref();
    EXPECT_TRUE((coord->getTypeId() != SoType::badType())) << "SoCoordinate3 has bad type";
    coord->unref();
}

TEST(NodesSuite, SoNormalClassInitialized)
{
    SoNormal* norm = new SoNormal;
    norm->ref();
    EXPECT_TRUE((norm->getTypeId() != SoType::badType())) << "SoNormal has bad type";
    norm->unref();
}

// -----------------------------------------------------------------------
// Shader nodes: class initialized
// Baseline: src/shaders/SoShaderProgram.cpp, SoFragmentShader.cpp, etc.
// -----------------------------------------------------------------------

TEST(NodesSuite, SoShaderProgramClassInitialized)
{
    SoShaderProgram* prog = new SoShaderProgram;
    prog->ref();
    EXPECT_TRUE((prog->getTypeId() != SoType::badType())) << "SoShaderProgram has bad type";
    prog->unref();
}

TEST(NodesSuite, SoFragmentShaderClassInitialized)
{
    SoFragmentShader* fs = new SoFragmentShader;
    fs->ref();
    EXPECT_TRUE((fs->getTypeId() != SoType::badType())) << "SoFragmentShader has bad type";
    fs->unref();
}

TEST(NodesSuite, SoVertexShaderClassInitialized)
{
    SoVertexShader* vs = new SoVertexShader;
    vs->ref();
    EXPECT_TRUE((vs->getTypeId() != SoType::badType())) << "SoVertexShader has bad type";
    vs->unref();
}

TEST(NodesSuite, SoGeometryShaderClassInitialized)
{
    SoGeometryShader* gs = new SoGeometryShader;
    gs->ref();
    EXPECT_TRUE((gs->getTypeId() != SoType::badType())) << "SoGeometryShader has bad type";
    gs->unref();
}

// -----------------------------------------------------------------------
// Shadow nodes: class initialized
// Baseline: src/shadows/SoShadowGroup.cpp, SoShadowStyle.cpp OBOL_TEST_SUITE
// -----------------------------------------------------------------------

TEST(NodesSuite, SoShadowGroupClassInitialized)
{
    SoShadowGroup* node = new SoShadowGroup;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoShadowGroup has bad type";
    node->unref();
}

TEST(NodesSuite, SoShadowStyleClassInitialized)
{
    SoShadowStyle* node = new SoShadowStyle;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoShadowStyle has bad type";
    node->unref();
}

// -----------------------------------------------------------------------
// Additional transform nodes: default fields
// -----------------------------------------------------------------------

TEST(NodesSuite, SoRotationXYZClassInitialized)
{
    SoRotationXYZ* node = new SoRotationXYZ;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoRotationXYZ has bad type";
    node->unref();
}

TEST(NodesSuite, SoMatrixTransformClassInitialized)
{
    SoMatrixTransform* node = new SoMatrixTransform;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoMatrixTransform has bad type";
    node->unref();
}

// -----------------------------------------------------------------------
// Property / appearance nodes
// -----------------------------------------------------------------------

TEST(NodesSuite, SoLightModelClassInitialized)
{
    SoLightModel* node = new SoLightModel;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoLightModel has bad type";
    node->unref();
}

TEST(NodesSuite, SoDrawStyleClassInitialized)
{
    SoDrawStyle* node = new SoDrawStyle;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoDrawStyle has bad type";
    node->unref();
}

TEST(NodesSuite, SoComplexityClassInitialized)
{
    SoComplexity* node = new SoComplexity;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoComplexity has bad type";
    node->unref();
}

TEST(NodesSuite, SoEnvironmentClassInitialized)
{
    SoEnvironment* node = new SoEnvironment;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoEnvironment has bad type";
    node->unref();
}

TEST(NodesSuite, SoClipPlaneClassInitialized)
{
    SoClipPlane* node = new SoClipPlane;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoClipPlane has bad type";
    node->unref();
}

TEST(NodesSuite, SoNormalBindingClassInitialized)
{
    SoNormalBinding* node = new SoNormalBinding;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoNormalBinding has bad type";
    node->unref();
}

TEST(NodesSuite, SoMaterialBindingClassInitialized)
{
    SoMaterialBinding* node = new SoMaterialBinding;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoMaterialBinding has bad type";
    node->unref();
}

TEST(NodesSuite, SoVertexPropertyClassInitialized)
{
    SoVertexProperty* node = new SoVertexProperty;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoVertexProperty has bad type";
    node->unref();
}

// -----------------------------------------------------------------------
// Texture nodes
// -----------------------------------------------------------------------

TEST(NodesSuite, SoTexture2ClassInitialized)
{
    SoTexture2* node = new SoTexture2;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoTexture2 has bad type";
    node->unref();
}

TEST(NodesSuite, SoTextureCoordinate2ClassInitialized)
{
    SoTextureCoordinate2* node = new SoTextureCoordinate2;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoTextureCoordinate2 has bad type";
    node->unref();
}

TEST(NodesSuite, SoTextureCoordinateBindingClassInitialized)
{
    SoTextureCoordinateBinding* node = new SoTextureCoordinateBinding;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoTextureCoordinateBinding has bad type";
    node->unref();
}

// -----------------------------------------------------------------------
// Geometry / shape nodes
// -----------------------------------------------------------------------

TEST(NodesSuite, SoFaceSetClassInitialized)
{
    SoFaceSet* node = new SoFaceSet;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoFaceSet has bad type";
    node->unref();
}

TEST(NodesSuite, SoIndexedFaceSetClassInitialized)
{
    SoIndexedFaceSet* node = new SoIndexedFaceSet;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoIndexedFaceSet has bad type";
    node->unref();
}

TEST(NodesSuite, SoTriangleStripSetClassInitialized)
{
    SoTriangleStripSet* node = new SoTriangleStripSet;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoTriangleStripSet has bad type";
    node->unref();
}

TEST(NodesSuite, SoIndexedTriangleStripSetClassInitialized)
{
    SoIndexedTriangleStripSet* node = new SoIndexedTriangleStripSet;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoIndexedTriangleStripSet has bad type";
    node->unref();
}

TEST(NodesSuite, SoLineSetClassInitialized)
{
    SoLineSet* node = new SoLineSet;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoLineSet has bad type";
    node->unref();
}

TEST(NodesSuite, SoIndexedLineSetClassInitialized)
{
    SoIndexedLineSet* node = new SoIndexedLineSet;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoIndexedLineSet has bad type";
    node->unref();
}

TEST(NodesSuite, SoPointSetClassInitialized)
{
    SoPointSet* node = new SoPointSet;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoPointSet has bad type";
    node->unref();
}

// -----------------------------------------------------------------------
// Misc utility nodes
// -----------------------------------------------------------------------

TEST(NodesSuite, SoFileClassInitialized)
{
    SoFile* node = new SoFile;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoFile has bad type";
    node->unref();
}

TEST(NodesSuite, SoInfoClassInitialized)
{
    SoInfo* node = new SoInfo;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoInfo has bad type";
    node->unref();
}

TEST(NodesSuite, SoLODClassInitialized)
{
    SoLOD* node = new SoLOD;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoLOD has bad type";
    node->unref();
}

// -----------------------------------------------------------------------
// Animation nodes
// -----------------------------------------------------------------------

TEST(NodesSuite, SoBlinkerClassInitialized)
{
    SoBlinker* node = new SoBlinker;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoBlinker has bad type";
    node->unref();
}

TEST(NodesSuite, SoRotorClassInitialized)
{
    SoRotor* node = new SoRotor;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoRotor has bad type";
    node->unref();
}

// -----------------------------------------------------------------------
// Shader parameter nodes
// Baseline: src/shaders/SoShaderParameter.cpp OBOL_TEST_SUITE (initialized)
// -----------------------------------------------------------------------

TEST(NodesSuite, SoShaderParameter1fClassInitialized)
{
    SoShaderParameter1f* node = new SoShaderParameter1f;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoShaderParameter1f has bad type";
    node->unref();
}

TEST(NodesSuite, SoShaderParameter1iClassInitialized)
{
    SoShaderParameter1i* node = new SoShaderParameter1i;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoShaderParameter1i has bad type";
    node->unref();
}

TEST(NodesSuite, SoShaderParameter2fClassInitialized)
{
    SoShaderParameter2f* node = new SoShaderParameter2f;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoShaderParameter2f has bad type";
    node->unref();
}

TEST(NodesSuite, SoShaderParameter2iClassInitialized)
{
    SoShaderParameter2i* node = new SoShaderParameter2i;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoShaderParameter2i has bad type";
    node->unref();
}

TEST(NodesSuite, SoShaderParameter3fClassInitialized)
{
    SoShaderParameter3f* node = new SoShaderParameter3f;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoShaderParameter3f has bad type";
    node->unref();
}

TEST(NodesSuite, SoShaderParameter3iClassInitialized)
{
    SoShaderParameter3i* node = new SoShaderParameter3i;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoShaderParameter3i has bad type";
    node->unref();
}

TEST(NodesSuite, SoShaderParameter4fClassInitialized)
{
    SoShaderParameter4f* node = new SoShaderParameter4f;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoShaderParameter4f has bad type";
    node->unref();
}

TEST(NodesSuite, SoShaderParameter4iClassInitialized)
{
    SoShaderParameter4i* node = new SoShaderParameter4i;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoShaderParameter4i has bad type";
    node->unref();
}

TEST(NodesSuite, SoShaderParameterArray1fClassInitialized)
{
    SoShaderParameterArray1f* node = new SoShaderParameterArray1f;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoShaderParameterArray1f has bad type";
    node->unref();
}

TEST(NodesSuite, SoShaderParameterArray1iClassInitialized)
{
    SoShaderParameterArray1i* node = new SoShaderParameterArray1i;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoShaderParameterArray1i has bad type";
    node->unref();
}

TEST(NodesSuite, SoShaderParameterMatrixClassInitialized)
{
    SoShaderParameterMatrix* node = new SoShaderParameterMatrix;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoShaderParameterMatrix has bad type";
    node->unref();
}

TEST(NodesSuite, SoShaderParameterMatrixArrayClassInitialized)
{
    SoShaderParameterMatrixArray* node = new SoShaderParameterMatrixArray;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoShaderParameterMatrixArray has bad type";
    node->unref();
}

TEST(NodesSuite, SoShaderStateMatrixParameterClassInitialized)
{
    SoShaderStateMatrixParameter* node = new SoShaderStateMatrixParameter;
    node->ref();
    EXPECT_TRUE((node->getTypeId() != SoType::badType())) << "SoShaderStateMatrixParameter has bad type";
    node->unref();
}

// -----------------------------------------------------------------------
// Dragger class types: all dragger classes registered in the type system
// Baseline: src/draggers/SoXxxDragger.cpp OBOL_TEST_SUITE blocks
// -----------------------------------------------------------------------

TEST(NodesSuite, AllDraggerClassTypesRegistered)
{
    EXPECT_TRUE(
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
        (SoTranslate2Dragger::getClassTypeId()       != SoType::badType())) << "One or more dragger class types not registered";
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

namespace {

template <typename Dragger>
void expectIndependentDraggerCopy()
{
    SoSeparator* root = new SoSeparator;
    root->ref();
    root->addChild(new Dragger);
    SoSeparator* copy = static_cast<SoSeparator*>(root->copy());

    EXPECT_NE(copy, nullptr);
    if (copy != nullptr) {
        copy->ref();
        EXPECT_EQ(copy->getNumChildren(), 1);
        if (copy->getNumChildren() == 1) {
            EXPECT_NE(copy->getChild(0), root->getChild(0));
        }
        copy->unref();
    }
    root->unref();
}

} // namespace

TEST(NodesSuite, SoTranslate1DraggerDeepCopyProducesIndependentNodes)
{
    expectIndependentDraggerCopy<SoTranslate1Dragger>();
}

TEST(NodesSuite, SoTranslate2DraggerDeepCopyProducesIndependentNodes)
{
    expectIndependentDraggerCopy<SoTranslate2Dragger>();
}

TEST(NodesSuite, SoDragPointDraggerDeepCopyProducesIndependentNodes)
{
    expectIndependentDraggerCopy<SoDragPointDragger>();
}

TEST(NodesSuite, SoRotateDiscDraggerDeepCopyProducesIndependentNodes)
{
    expectIndependentDraggerCopy<SoRotateDiscDragger>();
}

TEST(NodesSuite, SoRotateCylindricalDraggerDeepCopyProducesIndependentNodes)
{
    expectIndependentDraggerCopy<SoRotateCylindricalDragger>();
}

TEST(NodesSuite, SoRotateSphericalDraggerDeepCopyProducesIndependentNodes)
{
    expectIndependentDraggerCopy<SoRotateSphericalDragger>();
}

TEST(NodesSuite, SoScale1DraggerDeepCopyProducesIndependentNodes)
{
    expectIndependentDraggerCopy<SoScale1Dragger>();
}

TEST(NodesSuite, SoScale2DraggerDeepCopyProducesIndependentNodes)
{
    expectIndependentDraggerCopy<SoScale2Dragger>();
}

TEST(NodesSuite, SoDirectionalLightDraggerDeepCopyProducesIndependentNodes)
{
    expectIndependentDraggerCopy<SoDirectionalLightDragger>();
}
