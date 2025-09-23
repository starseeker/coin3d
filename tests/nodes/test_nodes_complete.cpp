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

#include "utils/test_common.h"
#include <catch2/catch_approx.hpp>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoVertexProperty.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoCallback.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoLOD.h>
#include <Inventor/nodes/SoInfo.h>
#include <Inventor/nodes/SoLabel.h>
#include <Inventor/SoType.h>

using namespace CoinTestUtils;

// Comprehensive node tests covering various node types and functionality

TEST_CASE("SoNode base functionality", "[nodes][SoNode][complete]") {
    CoinTestFixture fixture;

    SECTION("type system") {
        SoType nodeType = SoNode::getClassTypeId();
        CHECK(nodeType != SoType::badType());
        CHECK(nodeType.getName() == SbName("SoNode"));
    }

    SECTION("reference counting") {
        SoCube* cube = new SoCube;
        CHECK(cube->getRefCount() == 0);
        
        cube->ref();
        CHECK(cube->getRefCount() == 1);
        
        cube->unref();
        // cube is now deleted
    }
}

TEST_CASE("SoGroup complete functionality", "[nodes][SoGroup][complete]") {
    CoinTestFixture fixture;

    SECTION("child management") {
        SoGroup* group = new SoGroup;
        group->ref();
        
        CHECK(group->getNumChildren() == 0);
        
        SoCube* cube1 = new SoCube;
        SoCube* cube2 = new SoCube;
        
        group->addChild(cube1);
        CHECK(group->getNumChildren() == 1);
        CHECK(group->getChild(0) == cube1);
        CHECK(cube1->getRefCount() == 1);
        
        group->addChild(cube2);
        CHECK(group->getNumChildren() == 2);
        CHECK(group->getChild(1) == cube2);
        
        group->removeChild(0);
        CHECK(group->getNumChildren() == 1);
        CHECK(group->getChild(0) == cube2);
        
        group->removeAllChildren();
        CHECK(group->getNumChildren() == 0);
        
        group->unref();
    }

    SECTION("child insertion and replacement") {
        SoGroup* group = new SoGroup;
        group->ref();
        
        SoCube* cube = new SoCube;
        SoSphere* sphere = new SoSphere;
        SoCone* cone = new SoCone;
        
        group->addChild(cube);
        group->addChild(sphere);
        
        group->insertChild(cone, 1);
        CHECK(group->getNumChildren() == 3);
        CHECK(group->getChild(0) == cube);
        CHECK(group->getChild(1) == cone);
        CHECK(group->getChild(2) == sphere);
        
        SoCylinder* cylinder = new SoCylinder;
        group->replaceChild(1, cylinder);
        CHECK(group->getNumChildren() == 3);
        CHECK(group->getChild(1) == cylinder);
        
        group->unref();
    }
}

TEST_CASE("SoSeparator complete functionality", "[nodes][SoSeparator][complete]") {
    CoinTestFixture fixture;

    SECTION("basic properties") {
        SoSeparator* sep = new SoSeparator;
        sep->ref();
        
        CHECK(sep->renderCaching.getValue() == SoSeparator::AUTO);
        CHECK(sep->boundingBoxCaching.getValue() == SoSeparator::AUTO);
        CHECK(sep->pickCulling.getValue() == SoSeparator::AUTO);
        
        sep->unref();
    }

    SECTION("caching control") {
        SoSeparator* sep = new SoSeparator;
        sep->ref();
        
        sep->renderCaching.setValue(SoSeparator::ON);
        CHECK(sep->renderCaching.getValue() == SoSeparator::ON);
        
        sep->boundingBoxCaching.setValue(SoSeparator::OFF);
        CHECK(sep->boundingBoxCaching.getValue() == SoSeparator::OFF);
        
        sep->unref();
    }
}

TEST_CASE("Geometry nodes complete functionality", "[nodes][geometry][complete]") {
    CoinTestFixture fixture;

    SECTION("SoCube properties") {
        SoCube* cube = new SoCube;
        cube->ref();
        
        CHECK(cube->width.getValue() == 2.0f);
        CHECK(cube->height.getValue() == 2.0f);
        CHECK(cube->depth.getValue() == 2.0f);
        
        cube->width.setValue(4.0f);
        cube->height.setValue(6.0f);
        cube->depth.setValue(8.0f);
        
        CHECK(cube->width.getValue() == 4.0f);
        CHECK(cube->height.getValue() == 6.0f);
        CHECK(cube->depth.getValue() == 8.0f);
        
        cube->unref();
    }

    SECTION("SoSphere properties") {
        SoSphere* sphere = new SoSphere;
        sphere->ref();
        
        CHECK(sphere->radius.getValue() == 1.0f);
        
        sphere->radius.setValue(2.5f);
        CHECK(sphere->radius.getValue() == 2.5f);
        
        sphere->unref();
    }

    SECTION("SoCone properties") {
        SoCone* cone = new SoCone;
        cone->ref();
        
        CHECK(cone->bottomRadius.getValue() == 1.0f);
        CHECK(cone->height.getValue() == 2.0f);
        CHECK(cone->parts.getValue() == SoCone::ALL);
        
        cone->bottomRadius.setValue(1.5f);
        cone->height.setValue(3.0f);
        cone->parts.setValue(SoCone::SIDES);
        
        CHECK(cone->bottomRadius.getValue() == 1.5f);
        CHECK(cone->height.getValue() == 3.0f);
        CHECK(cone->parts.getValue() == SoCone::SIDES);
        
        cone->unref();
    }

    SECTION("SoCylinder properties") {
        SoCylinder* cylinder = new SoCylinder;
        cylinder->ref();
        
        CHECK(cylinder->radius.getValue() == 1.0f);
        CHECK(cylinder->height.getValue() == 2.0f);
        CHECK(cylinder->parts.getValue() == SoCylinder::ALL);
        
        cylinder->radius.setValue(0.8f);
        cylinder->height.setValue(4.0f);
        cylinder->parts.setValue(SoCylinder::SIDES | SoCylinder::TOP);
        
        CHECK(cylinder->radius.getValue() == 0.8f);
        CHECK(cylinder->height.getValue() == 4.0f);
        CHECK((cylinder->parts.getValue() & SoCylinder::SIDES) != 0);
        CHECK((cylinder->parts.getValue() & SoCylinder::TOP) != 0);
        CHECK((cylinder->parts.getValue() & SoCylinder::BOTTOM) == 0);
        
        cylinder->unref();
    }
}

TEST_CASE("Transform nodes complete functionality", "[nodes][transform][complete]") {
    CoinTestFixture fixture;

    SECTION("SoTransform properties") {
        SoTransform* transform = new SoTransform;
        transform->ref();
        
        // Default values
        CHECK(transform->translation.getValue() == SbVec3f(0.0f, 0.0f, 0.0f));
        CHECK(transform->rotation.getValue().getValue()[3] == 1.0f); // Identity quaternion
        CHECK(transform->scaleFactor.getValue() == SbVec3f(1.0f, 1.0f, 1.0f));
        CHECK(transform->center.getValue() == SbVec3f(0.0f, 0.0f, 0.0f));
        
        // Set values
        transform->translation.setValue(1.0f, 2.0f, 3.0f);
        transform->rotation.setValue(SbVec3f(0.0f, 1.0f, 0.0f), static_cast<float>(M_PI / 2.0));
        transform->scaleFactor.setValue(2.0f, 2.0f, 2.0f);
        transform->center.setValue(0.5f, 0.5f, 0.5f);
        
        CHECK(transform->translation.getValue() == SbVec3f(1.0f, 2.0f, 3.0f));
        CHECK(transform->scaleFactor.getValue() == SbVec3f(2.0f, 2.0f, 2.0f));
        CHECK(transform->center.getValue() == SbVec3f(0.5f, 0.5f, 0.5f));
        
        transform->unref();
    }

    SECTION("SoTranslation properties") {
        SoTranslation* translation = new SoTranslation;
        translation->ref();
        
        CHECK(translation->translation.getValue() == SbVec3f(0.0f, 0.0f, 0.0f));
        
        translation->translation.setValue(5.0f, 10.0f, 15.0f);
        CHECK(translation->translation.getValue() == SbVec3f(5.0f, 10.0f, 15.0f));
        
        translation->unref();
    }

    SECTION("SoRotation properties") {
        SoRotation* rotation = new SoRotation;
        rotation->ref();
        
        // Default should be identity
        SbRotation defaultRot = rotation->rotation.getValue();
        CHECK(defaultRot.getValue()[3] == 1.0f);
        
        rotation->rotation.setValue(SbVec3f(0.0f, 0.0f, 1.0f), static_cast<float>(M_PI / 4.0));
        SbRotation newRot = rotation->rotation.getValue();
        
        SbVec3f axis;
        float angle;
        newRot.getValue(axis, angle);
        CHECK(axis[2] == Catch::Approx(1.0f));
        CHECK(angle == Catch::Approx(M_PI / 4.0));
        
        rotation->unref();
    }

    SECTION("SoScale properties") {
        SoScale* scale = new SoScale;
        scale->ref();
        
        CHECK(scale->scaleFactor.getValue() == SbVec3f(1.0f, 1.0f, 1.0f));
        
        scale->scaleFactor.setValue(2.0f, 3.0f, 4.0f);
        CHECK(scale->scaleFactor.getValue() == SbVec3f(2.0f, 3.0f, 4.0f));
        
        scale->unref();
    }
}

TEST_CASE("Material nodes complete functionality", "[nodes][material][complete]") {
    CoinTestFixture fixture;

    SECTION("SoMaterial properties") {
        SoMaterial* material = new SoMaterial;
        material->ref();
        
        // Set material properties
        material->diffuseColor.setValue(1.0f, 0.0f, 0.0f);
        material->specularColor.setValue(1.0f, 1.0f, 1.0f);
        material->emissiveColor.setValue(0.0f, 0.0f, 0.0f);
        material->shininess.setValue(0.8f);
        material->transparency.setValue(0.2f);
        
        CHECK(material->diffuseColor[0] == SbColor(1.0f, 0.0f, 0.0f));
        CHECK(material->specularColor[0] == SbColor(1.0f, 1.0f, 1.0f));
        CHECK(material->emissiveColor[0] == SbColor(0.0f, 0.0f, 0.0f));
        CHECK(material->shininess[0] == 0.8f);
        CHECK(material->transparency[0] == 0.2f);
        
        material->unref();
    }

    SECTION("SoBaseColor properties") {
        SoBaseColor* baseColor = new SoBaseColor;
        baseColor->ref();
        
        baseColor->rgb.setValue(0.5f, 0.7f, 0.9f);
        CHECK(baseColor->rgb[0] == SbColor(0.5f, 0.7f, 0.9f));
        
        baseColor->unref();
    }
}

TEST_CASE("Light nodes complete functionality", "[nodes][lights][complete]") {
    CoinTestFixture fixture;

    SECTION("SoDirectionalLight properties") {
        SoDirectionalLight* light = new SoDirectionalLight;
        light->ref();
        
        CHECK(light->on.getValue() == TRUE);
        CHECK(light->intensity.getValue() == 1.0f);
        CHECK(light->color.getValue() == SbColor(1.0f, 1.0f, 1.0f));
        CHECK(light->direction.getValue() == SbVec3f(0.0f, 0.0f, -1.0f));
        
        light->direction.setValue(1.0f, -1.0f, 0.0f);
        CHECK(light->direction.getValue() == SbVec3f(1.0f, -1.0f, 0.0f));
        
        light->unref();
    }

    SECTION("SoPointLight properties") {
        SoPointLight* light = new SoPointLight;
        light->ref();
        
        CHECK(light->location.getValue() == SbVec3f(0.0f, 0.0f, 1.0f));
        
        light->location.setValue(5.0f, 10.0f, 0.0f);
        CHECK(light->location.getValue() == SbVec3f(5.0f, 10.0f, 0.0f));
        
        light->unref();
    }

    SECTION("SoSpotLight properties") {
        SoSpotLight* light = new SoSpotLight;
        light->ref();
        
        CHECK(light->location.getValue() == SbVec3f(0.0f, 0.0f, 1.0f));
        CHECK(light->direction.getValue() == SbVec3f(0.0f, 0.0f, -1.0f));
        CHECK(light->cutOffAngle.getValue() == static_cast<float>(M_PI / 4.0));
        CHECK(light->dropOffRate.getValue() == 0.0f);
        
        light->cutOffAngle.setValue(static_cast<float>(M_PI / 6.0));
        CHECK(light->cutOffAngle.getValue() == Catch::Approx(M_PI / 6.0));
        
        light->unref();
    }
}

TEST_CASE("Camera nodes complete functionality", "[nodes][cameras][complete]") {
    CoinTestFixture fixture;

    SECTION("SoPerspectiveCamera properties") {
        SoPerspectiveCamera* camera = new SoPerspectiveCamera;
        camera->ref();
        
        CHECK(camera->heightAngle.getValue() == static_cast<float>(M_PI / 4.0));
        CHECK(camera->position.getValue() == SbVec3f(0.0f, 0.0f, 1.0f));
        CHECK(camera->orientation.getValue().getValue()[3] == 1.0f); // Identity
        CHECK(camera->nearDistance.getValue() == 1.0f);
        CHECK(camera->farDistance.getValue() == 10.0f);
        
        camera->heightAngle.setValue(static_cast<float>(M_PI / 3.0));
        camera->position.setValue(0.0f, 5.0f, 10.0f);
        camera->nearDistance.setValue(0.1f);
        camera->farDistance.setValue(100.0f);
        
        CHECK(camera->heightAngle.getValue() == Catch::Approx(M_PI / 3.0));
        CHECK(camera->position.getValue() == SbVec3f(0.0f, 5.0f, 10.0f));
        CHECK(camera->nearDistance.getValue() == 0.1f);
        CHECK(camera->farDistance.getValue() == 100.0f);
        
        camera->unref();
    }

    SECTION("SoOrthographicCamera properties") {
        SoOrthographicCamera* camera = new SoOrthographicCamera;
        camera->ref();
        
        CHECK(camera->height.getValue() == 2.0f);
        
        camera->height.setValue(10.0f);
        CHECK(camera->height.getValue() == 10.0f);
        
        camera->unref();
    }
}

TEST_CASE("Utility nodes complete functionality", "[nodes][utility][complete]") {
    CoinTestFixture fixture;

    SECTION("SoInfo properties") {
        SoInfo* info = new SoInfo;
        info->ref();
        
        info->string.setValue("Test information");
        CHECK(info->string.getValue() == SbString("Test information"));
        
        info->unref();
    }

    SECTION("SoLabel properties") {
        SoLabel* label = new SoLabel;
        label->ref();
        
        label->label.setValue("TestLabel");
        CHECK(label->label.getValue() == SbName("TestLabel"));
        
        label->unref();
    }

    SECTION("SoComplexity properties") {
        SoComplexity* complexity = new SoComplexity;
        complexity->ref();
        
        CHECK(complexity->value.getValue() == 0.5f);
        CHECK(complexity->type.getValue() == SoComplexity::OBJECT_SPACE);
        
        complexity->value.setValue(0.8f);
        complexity->type.setValue(SoComplexity::SCREEN_SPACE);
        
        CHECK(complexity->value.getValue() == 0.8f);
        CHECK(complexity->type.getValue() == SoComplexity::SCREEN_SPACE);
        
        complexity->unref();
    }
}

TEST_CASE("Group utility nodes complete functionality", "[nodes][group_utility][complete]") {
    CoinTestFixture fixture;

    SECTION("SoSwitch properties") {
        SoSwitch* switchNode = new SoSwitch;
        switchNode->ref();
        
        CHECK(switchNode->whichChild.getValue() == -1);
        
        SoCube* cube1 = new SoCube;
        SoCube* cube2 = new SoCube;
        switchNode->addChild(cube1);
        switchNode->addChild(cube2);
        
        switchNode->whichChild.setValue(0);
        CHECK(switchNode->whichChild.getValue() == 0);
        
        switchNode->whichChild.setValue(1);
        CHECK(switchNode->whichChild.getValue() == 1);
        
        switchNode->whichChild.setValue(SO_SWITCH_ALL);
        CHECK(switchNode->whichChild.getValue() == SO_SWITCH_ALL);
        
        switchNode->unref();
    }

    SECTION("SoSelection basic functionality") {
        SoSelection* selection = new SoSelection;
        selection->ref();
        
        CHECK(selection->policy.getValue() == SoSelection::SHIFT);
        
        selection->policy.setValue(SoSelection::SINGLE);
        CHECK(selection->policy.getValue() == SoSelection::SINGLE);
        
        selection->unref();
    }
}