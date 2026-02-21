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
 * Baselined against coin_vanilla COIN_TEST_SUITE blocks.
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
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoShaderProgram.h>
#include <Inventor/nodes/SoFragmentShader.h>
#include <Inventor/nodes/SoVertexShader.h>
#include <Inventor/nodes/SoGeometryShader.h>
#include <Inventor/nodes/SoGeoOrigin.h>
#include <Inventor/nodes/SoGeoCoordinate.h>

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
    // Geo nodes: class initialized
    // Baseline: src/geo/SoGeoOrigin.cpp, SoGeoCoordinate.cpp
    // -----------------------------------------------------------------------
    runner.startTest("SoGeoOrigin class initialized");
    {
        SoGeoOrigin* geo = new SoGeoOrigin;
        geo->ref();
        bool pass = (geo->getTypeId() != SoType::badType());
        geo->unref();
        runner.endTest(pass, pass ? "" : "SoGeoOrigin has bad type");
    }

    runner.startTest("SoGeoCoordinate class initialized");
    {
        SoGeoCoordinate* geo = new SoGeoCoordinate;
        geo->ref();
        bool pass = (geo->getTypeId() != SoType::badType());
        geo->unref();
        runner.endTest(pass, pass ? "" : "SoGeoCoordinate has bad type");
    }

    return runner.getSummary();
}
