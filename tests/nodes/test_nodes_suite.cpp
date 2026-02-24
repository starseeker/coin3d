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
 * Baselined against upstream COIN_TEST_SUITE blocks.
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

using namespace SimpleTest;

// Factory function needed by SoType::createType
static void* createDummyInstance(void) { return reinterpret_cast<void*>(0x1); }

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoAnnotation: class initialized (ref/unref, getTypeId)
    // Baseline: src/nodes/SoAnnotation.cpp COIN_TEST_SUITE (initialized)
    // -----------------------------------------------------------------------
    runner.startTest("SoAnnotation class initialized");
    {
        SoAnnotation* node = new SoAnnotation;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoAnnotation has bad typeId");
    }

    // -----------------------------------------------------------------------
    // SoType: createType / removeType
    // Baseline: src/misc/SoType.cpp COIN_TEST_SUITE (testRemoveType)
    // -----------------------------------------------------------------------
    runner.startTest("SoType createType and removeType");
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
        runner.endTest(pass, pass ? "" :
            "SoType createType/removeType did not behave as expected");
    }

    // -----------------------------------------------------------------------
    // SoNode: isOfType hierarchy
    // -----------------------------------------------------------------------
    runner.startTest("SoCube isOfType SoNode");
    {
        SoCube* cube = new SoCube;
        cube->ref();
        bool pass = cube->isOfType(SoNode::getClassTypeId());
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoCube should be of type SoNode");
    }

    runner.startTest("SoSeparator isOfType SoGroup");
    {
        SoSeparator* sep = new SoSeparator;
        sep->ref();
        bool pass = sep->isOfType(SoGroup::getClassTypeId());
        sep->unref();
        runner.endTest(pass, pass ? "" : "SoSeparator should be a SoGroup");
    }

    // -----------------------------------------------------------------------
    // SoGroup / SoSeparator: child management
    // -----------------------------------------------------------------------
    runner.startTest("SoSeparator addChild/getNumChildren/removeChild");
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
        runner.endTest(pass, pass ? "" : "SoSeparator child management failed");
    }

    runner.startTest("SoSeparator insertChild");
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
        runner.endTest(pass, pass ? "" : "SoSeparator insertChild failed");
    }

    // -----------------------------------------------------------------------
    // SoNode: setName / getName
    // -----------------------------------------------------------------------
    runner.startTest("SoNode setName/getName");
    {
        SoCube* cube = new SoCube;
        cube->ref();
        cube->setName("TestCube");
        bool pass = (cube->getName() == SbName("TestCube"));
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoNode setName/getName failed");
    }

    // -----------------------------------------------------------------------
    // SoNode: SoNode::getByName
    // -----------------------------------------------------------------------
    runner.startTest("SoNode::getByName");
    {
        SoCylinder* cyl = new SoCylinder;
        cyl->ref();
        cyl->setName("UniqueCylinder");
        SoNode* found = SoNode::getByName(SbName("UniqueCylinder"));
        bool pass = (found == cyl);
        cyl->unref();
        runner.endTest(pass, pass ? "" :
            "SoNode::getByName did not find the named node");
    }

    // -----------------------------------------------------------------------
    // Geometry nodes: default field values
    // -----------------------------------------------------------------------
    runner.startTest("SoCube default fields");
    {
        SoCube* cube = new SoCube;
        cube->ref();
        bool pass = (cube->width.getValue()  == 2.0f) &&
                    (cube->height.getValue() == 2.0f) &&
                    (cube->depth.getValue()  == 2.0f);
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoCube default field values wrong");
    }

    runner.startTest("SoSphere default radius");
    {
        SoSphere* sphere = new SoSphere;
        sphere->ref();
        bool pass = (sphere->radius.getValue() == 1.0f);
        sphere->unref();
        runner.endTest(pass, pass ? "" : "SoSphere default radius != 1.0");
    }

    runner.startTest("SoCone default fields");
    {
        SoCone* cone = new SoCone;
        cone->ref();
        bool pass = (cone->bottomRadius.getValue() == 1.0f) &&
                    (cone->height.getValue()        == 2.0f);
        cone->unref();
        runner.endTest(pass, pass ? "" : "SoCone default field values wrong");
    }

    // -----------------------------------------------------------------------
    // SoMaterial: default field count
    // -----------------------------------------------------------------------
    runner.startTest("SoMaterial default diffuseColor field");
    {
        SoMaterial* mat = new SoMaterial;
        mat->ref();
        // Default diffuseColor is one value (0.8, 0.8, 0.8)
        bool pass = (mat->diffuseColor.getNum() == 1);
        mat->unref();
        runner.endTest(pass, pass ? "" :
            "SoMaterial default diffuseColor should have 1 value");
    }

    // -----------------------------------------------------------------------
    // SoCylinder: default field values
    // -----------------------------------------------------------------------
    runner.startTest("SoCylinder default fields");
    {
        SoCylinder* cyl = new SoCylinder;
        cyl->ref();
        bool pass = (cyl->radius.getValue() == 1.0f) &&
                    (cyl->height.getValue() == 2.0f);
        cyl->unref();
        runner.endTest(pass, pass ? "" : "SoCylinder default field values wrong");
    }

    // -----------------------------------------------------------------------
    // Light nodes: default fields
    // -----------------------------------------------------------------------
    runner.startTest("SoDirectionalLight class initialized");
    {
        SoDirectionalLight* light = new SoDirectionalLight;
        light->ref();
        bool pass = (light->getTypeId() != SoType::badType());
        light->unref();
        runner.endTest(pass, pass ? "" : "SoDirectionalLight has bad type");
    }

    runner.startTest("SoPointLight class initialized");
    {
        SoPointLight* light = new SoPointLight;
        light->ref();
        bool pass = (light->getTypeId() != SoType::badType());
        light->unref();
        runner.endTest(pass, pass ? "" : "SoPointLight has bad type");
    }

    runner.startTest("SoSpotLight class initialized");
    {
        SoSpotLight* light = new SoSpotLight;
        light->ref();
        bool pass = (light->getTypeId() != SoType::badType());
        light->unref();
        runner.endTest(pass, pass ? "" : "SoSpotLight has bad type");
    }

    // -----------------------------------------------------------------------
    // Transform nodes: default field values
    // -----------------------------------------------------------------------
    runner.startTest("SoTranslation default translation");
    {
        SoTranslation* t = new SoTranslation;
        t->ref();
        SbVec3f v = t->translation.getValue();
        bool pass = (v == SbVec3f(0, 0, 0));
        t->unref();
        runner.endTest(pass, pass ? "" : "SoTranslation default translation != (0,0,0)");
    }

    runner.startTest("SoRotation default rotation");
    {
        SoRotation* r = new SoRotation;
        r->ref();
        // Default rotation is identity (0,0,1,0) = zero angle around z
        SbVec3f axis; float angle;
        r->rotation.getValue().getValue(axis, angle);
        bool pass = (angle == 0.0f);
        r->unref();
        runner.endTest(pass, pass ? "" : "SoRotation default rotation is not identity");
    }

    runner.startTest("SoScale default scaleFactor");
    {
        SoScale* s = new SoScale;
        s->ref();
        SbVec3f sf = s->scaleFactor.getValue();
        bool pass = (sf == SbVec3f(1, 1, 1));
        s->unref();
        runner.endTest(pass, pass ? "" : "SoScale default scaleFactor != (1,1,1)");
    }

    runner.startTest("SoTransform default translation");
    {
        SoTransform* xf = new SoTransform;
        xf->ref();
        SbVec3f t = xf->translation.getValue();
        bool pass = (t == SbVec3f(0, 0, 0));
        xf->unref();
        runner.endTest(pass, pass ? "" : "SoTransform default translation != (0,0,0)");
    }

    // -----------------------------------------------------------------------
    // Camera nodes: default fields
    // -----------------------------------------------------------------------
    runner.startTest("SoPerspectiveCamera class initialized");
    {
        SoPerspectiveCamera* cam = new SoPerspectiveCamera;
        cam->ref();
        bool pass = (cam->getTypeId() != SoType::badType());
        cam->unref();
        runner.endTest(pass, pass ? "" : "SoPerspectiveCamera has bad type");
    }

    runner.startTest("SoOrthographicCamera class initialized");
    {
        SoOrthographicCamera* cam = new SoOrthographicCamera;
        cam->ref();
        bool pass = (cam->getTypeId() != SoType::badType());
        cam->unref();
        runner.endTest(pass, pass ? "" : "SoOrthographicCamera has bad type");
    }

    // -----------------------------------------------------------------------
    // Camera default field values
    // Baseline: SoCamera base class documented defaults
    // -----------------------------------------------------------------------
    runner.startTest("SoPerspectiveCamera default nearDistance");
    {
        SoPerspectiveCamera* cam = new SoPerspectiveCamera;
        cam->ref();
        // Default nearDistance is 1.0
        bool pass = (cam->nearDistance.getValue() == 1.0f);
        cam->unref();
        runner.endTest(pass, pass ? "" : "SoPerspectiveCamera nearDistance default != 1.0");
    }

    runner.startTest("SoPerspectiveCamera default farDistance");
    {
        SoPerspectiveCamera* cam = new SoPerspectiveCamera;
        cam->ref();
        // Default farDistance is 10.0
        bool pass = (cam->farDistance.getValue() == 10.0f);
        cam->unref();
        runner.endTest(pass, pass ? "" : "SoPerspectiveCamera farDistance default != 10.0");
    }

    runner.startTest("SoOrthographicCamera default height");
    {
        SoOrthographicCamera* cam = new SoOrthographicCamera;
        cam->ref();
        // Default height is 2.0
        bool pass = (cam->height.getValue() == 2.0f);
        cam->unref();
        runner.endTest(pass, pass ? "" : "SoOrthographicCamera height default != 2.0");
    }

    // -----------------------------------------------------------------------
    // SoSwitch: whichChild default value
    // -----------------------------------------------------------------------
    runner.startTest("SoSwitch default whichChild");
    {
        SoSwitch* sw = new SoSwitch;
        sw->ref();
        // Default whichChild is SO_SWITCH_NONE (-1)
        bool pass = (sw->whichChild.getValue() == SO_SWITCH_NONE);
        sw->unref();
        runner.endTest(pass, pass ? "" : "SoSwitch default whichChild != SO_SWITCH_NONE");
    }

    // -----------------------------------------------------------------------
    // Geometry support nodes: class initialized
    // -----------------------------------------------------------------------
    runner.startTest("SoCoordinate3 class initialized");
    {
        SoCoordinate3* coord = new SoCoordinate3;
        coord->ref();
        bool pass = (coord->getTypeId() != SoType::badType());
        coord->unref();
        runner.endTest(pass, pass ? "" : "SoCoordinate3 has bad type");
    }

    runner.startTest("SoNormal class initialized");
    {
        SoNormal* norm = new SoNormal;
        norm->ref();
        bool pass = (norm->getTypeId() != SoType::badType());
        norm->unref();
        runner.endTest(pass, pass ? "" : "SoNormal has bad type");
    }

    // -----------------------------------------------------------------------
    // Shader nodes: class initialized
    // Baseline: src/shaders/SoShaderProgram.cpp, SoFragmentShader.cpp, etc.
    // -----------------------------------------------------------------------
    runner.startTest("SoShaderProgram class initialized");
    {
        SoShaderProgram* prog = new SoShaderProgram;
        prog->ref();
        bool pass = (prog->getTypeId() != SoType::badType());
        prog->unref();
        runner.endTest(pass, pass ? "" : "SoShaderProgram has bad type");
    }

    runner.startTest("SoFragmentShader class initialized");
    {
        SoFragmentShader* fs = new SoFragmentShader;
        fs->ref();
        bool pass = (fs->getTypeId() != SoType::badType());
        fs->unref();
        runner.endTest(pass, pass ? "" : "SoFragmentShader has bad type");
    }

    runner.startTest("SoVertexShader class initialized");
    {
        SoVertexShader* vs = new SoVertexShader;
        vs->ref();
        bool pass = (vs->getTypeId() != SoType::badType());
        vs->unref();
        runner.endTest(pass, pass ? "" : "SoVertexShader has bad type");
    }

    runner.startTest("SoGeometryShader class initialized");
    {
        SoGeometryShader* gs = new SoGeometryShader;
        gs->ref();
        bool pass = (gs->getTypeId() != SoType::badType());
        gs->unref();
        runner.endTest(pass, pass ? "" : "SoGeometryShader has bad type");
    }

    // -----------------------------------------------------------------------
    // Shadow nodes: class initialized
    // Baseline: src/shadows/SoShadowGroup.cpp, SoShadowStyle.cpp COIN_TEST_SUITE
    // -----------------------------------------------------------------------
    runner.startTest("SoShadowGroup class initialized");
    {
        SoShadowGroup* node = new SoShadowGroup;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShadowGroup has bad type");
    }

    runner.startTest("SoShadowStyle class initialized");
    {
        SoShadowStyle* node = new SoShadowStyle;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShadowStyle has bad type");
    }

    // -----------------------------------------------------------------------
    // Additional transform nodes: default fields
    // -----------------------------------------------------------------------
    runner.startTest("SoRotationXYZ class initialized");
    {
        SoRotationXYZ* node = new SoRotationXYZ;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoRotationXYZ has bad type");
    }

    runner.startTest("SoMatrixTransform class initialized");
    {
        SoMatrixTransform* node = new SoMatrixTransform;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoMatrixTransform has bad type");
    }

    // -----------------------------------------------------------------------
    // Property / appearance nodes
    // -----------------------------------------------------------------------
    runner.startTest("SoLightModel class initialized");
    {
        SoLightModel* node = new SoLightModel;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoLightModel has bad type");
    }

    runner.startTest("SoDrawStyle class initialized");
    {
        SoDrawStyle* node = new SoDrawStyle;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoDrawStyle has bad type");
    }

    runner.startTest("SoComplexity class initialized");
    {
        SoComplexity* node = new SoComplexity;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoComplexity has bad type");
    }

    runner.startTest("SoEnvironment class initialized");
    {
        SoEnvironment* node = new SoEnvironment;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoEnvironment has bad type");
    }

    runner.startTest("SoClipPlane class initialized");
    {
        SoClipPlane* node = new SoClipPlane;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoClipPlane has bad type");
    }

    runner.startTest("SoNormalBinding class initialized");
    {
        SoNormalBinding* node = new SoNormalBinding;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoNormalBinding has bad type");
    }

    runner.startTest("SoMaterialBinding class initialized");
    {
        SoMaterialBinding* node = new SoMaterialBinding;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoMaterialBinding has bad type");
    }

    runner.startTest("SoVertexProperty class initialized");
    {
        SoVertexProperty* node = new SoVertexProperty;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoVertexProperty has bad type");
    }

    // -----------------------------------------------------------------------
    // Texture nodes
    // -----------------------------------------------------------------------
    runner.startTest("SoTexture2 class initialized");
    {
        SoTexture2* node = new SoTexture2;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoTexture2 has bad type");
    }

    runner.startTest("SoTextureCoordinate2 class initialized");
    {
        SoTextureCoordinate2* node = new SoTextureCoordinate2;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoTextureCoordinate2 has bad type");
    }

    runner.startTest("SoTextureCoordinateBinding class initialized");
    {
        SoTextureCoordinateBinding* node = new SoTextureCoordinateBinding;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoTextureCoordinateBinding has bad type");
    }

    // -----------------------------------------------------------------------
    // Geometry / shape nodes
    // -----------------------------------------------------------------------
    runner.startTest("SoFaceSet class initialized");
    {
        SoFaceSet* node = new SoFaceSet;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoFaceSet has bad type");
    }

    runner.startTest("SoIndexedFaceSet class initialized");
    {
        SoIndexedFaceSet* node = new SoIndexedFaceSet;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoIndexedFaceSet has bad type");
    }

    runner.startTest("SoTriangleStripSet class initialized");
    {
        SoTriangleStripSet* node = new SoTriangleStripSet;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoTriangleStripSet has bad type");
    }

    runner.startTest("SoIndexedTriangleStripSet class initialized");
    {
        SoIndexedTriangleStripSet* node = new SoIndexedTriangleStripSet;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoIndexedTriangleStripSet has bad type");
    }

    runner.startTest("SoLineSet class initialized");
    {
        SoLineSet* node = new SoLineSet;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoLineSet has bad type");
    }

    runner.startTest("SoIndexedLineSet class initialized");
    {
        SoIndexedLineSet* node = new SoIndexedLineSet;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoIndexedLineSet has bad type");
    }

    runner.startTest("SoPointSet class initialized");
    {
        SoPointSet* node = new SoPointSet;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoPointSet has bad type");
    }

    // -----------------------------------------------------------------------
    // Misc utility nodes
    // -----------------------------------------------------------------------
    runner.startTest("SoFile class initialized");
    {
        SoFile* node = new SoFile;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoFile has bad type");
    }

    runner.startTest("SoInfo class initialized");
    {
        SoInfo* node = new SoInfo;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoInfo has bad type");
    }

    runner.startTest("SoLOD class initialized");
    {
        SoLOD* node = new SoLOD;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoLOD has bad type");
    }

    // -----------------------------------------------------------------------
    // Animation nodes
    // -----------------------------------------------------------------------
    runner.startTest("SoBlinker class initialized");
    {
        SoBlinker* node = new SoBlinker;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoBlinker has bad type");
    }

    runner.startTest("SoRotor class initialized");
    {
        SoRotor* node = new SoRotor;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoRotor has bad type");
    }

    // -----------------------------------------------------------------------
    // Shader parameter nodes
    // Baseline: src/shaders/SoShaderParameter.cpp COIN_TEST_SUITE (initialized)
    // -----------------------------------------------------------------------
    runner.startTest("SoShaderParameter1f class initialized");
    {
        SoShaderParameter1f* node = new SoShaderParameter1f;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShaderParameter1f has bad type");
    }

    runner.startTest("SoShaderParameter1i class initialized");
    {
        SoShaderParameter1i* node = new SoShaderParameter1i;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShaderParameter1i has bad type");
    }

    runner.startTest("SoShaderParameter2f class initialized");
    {
        SoShaderParameter2f* node = new SoShaderParameter2f;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShaderParameter2f has bad type");
    }

    runner.startTest("SoShaderParameter2i class initialized");
    {
        SoShaderParameter2i* node = new SoShaderParameter2i;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShaderParameter2i has bad type");
    }

    runner.startTest("SoShaderParameter3f class initialized");
    {
        SoShaderParameter3f* node = new SoShaderParameter3f;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShaderParameter3f has bad type");
    }

    runner.startTest("SoShaderParameter3i class initialized");
    {
        SoShaderParameter3i* node = new SoShaderParameter3i;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShaderParameter3i has bad type");
    }

    runner.startTest("SoShaderParameter4f class initialized");
    {
        SoShaderParameter4f* node = new SoShaderParameter4f;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShaderParameter4f has bad type");
    }

    runner.startTest("SoShaderParameter4i class initialized");
    {
        SoShaderParameter4i* node = new SoShaderParameter4i;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShaderParameter4i has bad type");
    }

    runner.startTest("SoShaderParameterArray1f class initialized");
    {
        SoShaderParameterArray1f* node = new SoShaderParameterArray1f;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShaderParameterArray1f has bad type");
    }

    runner.startTest("SoShaderParameterArray1i class initialized");
    {
        SoShaderParameterArray1i* node = new SoShaderParameterArray1i;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShaderParameterArray1i has bad type");
    }

    runner.startTest("SoShaderParameterMatrix class initialized");
    {
        SoShaderParameterMatrix* node = new SoShaderParameterMatrix;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShaderParameterMatrix has bad type");
    }

    runner.startTest("SoShaderParameterMatrixArray class initialized");
    {
        SoShaderParameterMatrixArray* node = new SoShaderParameterMatrixArray;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShaderParameterMatrixArray has bad type");
    }

    runner.startTest("SoShaderStateMatrixParameter class initialized");
    {
        SoShaderStateMatrixParameter* node = new SoShaderStateMatrixParameter;
        node->ref();
        bool pass = (node->getTypeId() != SoType::badType());
        node->unref();
        runner.endTest(pass, pass ? "" : "SoShaderStateMatrixParameter has bad type");
    }

    // -----------------------------------------------------------------------
    // Dragger class types: all dragger classes registered in the type system
    // Baseline: src/draggers/SoXxxDragger.cpp COIN_TEST_SUITE blocks
    // -----------------------------------------------------------------------
    runner.startTest("All dragger class types registered");
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
        runner.endTest(pass, pass ? "" : "One or more dragger class types not registered");
    }

    // -----------------------------------------------------------------------
    // Dragger deep-copy: construction and copy() for single-piece draggers
    // Baseline: src/draggers/SoTransformerDragger.cpp COIN_TEST_SUITE
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
    runner.startTest("SoTranslate1Dragger deep copy produces independent nodes");
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
        runner.endTest(pass, pass ? "" :
            "SoTranslate1Dragger deep copy failed or shared child pointer");
    }

    runner.startTest("SoTranslate2Dragger deep copy produces independent nodes");
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
        runner.endTest(pass, pass ? "" :
            "SoTranslate2Dragger deep copy failed or shared child pointer");
    }

    runner.startTest("SoDragPointDragger deep copy produces independent nodes");
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
        runner.endTest(pass, pass ? "" :
            "SoDragPointDragger deep copy failed or shared child pointer");
    }

    runner.startTest("SoRotateDiscDragger deep copy produces independent nodes");
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
        runner.endTest(pass, pass ? "" :
            "SoRotateDiscDragger deep copy failed or shared child pointer");
    }

    runner.startTest("SoRotateCylindricalDragger deep copy produces independent nodes");
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
        runner.endTest(pass, pass ? "" :
            "SoRotateCylindricalDragger deep copy failed or shared child pointer");
    }

    runner.startTest("SoRotateSphericalDragger deep copy produces independent nodes");
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
        runner.endTest(pass, pass ? "" :
            "SoRotateSphericalDragger deep copy failed or shared child pointer");
    }

    runner.startTest("SoScale1Dragger deep copy produces independent nodes");
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
        runner.endTest(pass, pass ? "" :
            "SoScale1Dragger deep copy failed or shared child pointer");
    }

    runner.startTest("SoScale2Dragger deep copy produces independent nodes");
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
        runner.endTest(pass, pass ? "" :
            "SoScale2Dragger deep copy failed or shared child pointer");
    }

    runner.startTest("SoDirectionalLightDragger deep copy produces independent nodes");
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
        runner.endTest(pass, pass ? "" :
            "SoDirectionalLightDragger deep copy failed or shared child pointer");
    }

    return runner.getSummary();
}
