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
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoDirectionalLight.h>

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

    return runner.getSummary();
}
