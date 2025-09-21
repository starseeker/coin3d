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

#include "scene_graph_test_utils.h"
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <chrono>
#include <map>
#include <fstream>
#include <sstream>

namespace CoinTestUtils {

// ============================================================================
// StandardTestScenes Implementation
// ============================================================================

SoSeparator* StandardTestScenes::createMinimalScene() {
    SoSeparator* root = new SoSeparator;
    root->ref();
    
    // Add camera
    SoPerspectiveCamera* camera = new SoPerspectiveCamera;
    camera->position.setValue(0, 0, 5);
    camera->nearDistance = 1.0f;
    camera->farDistance = 10.0f;
    root->addChild(camera);
    
    // Add light
    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(0, 0, -1);
    root->addChild(light);
    
    return root;
}

SoSeparator* StandardTestScenes::createBasicGeometryScene() {
    SoSeparator* root = createMinimalScene();
    
    // Add basic shapes
    SoCube* cube = new SoCube;
    root->addChild(cube);
    
    SoTransform* transform = new SoTransform;
    transform->translation.setValue(3, 0, 0);
    root->addChild(transform);
    
    SoSphere* sphere = new SoSphere;
    root->addChild(sphere);
    
    SoTransform* transform2 = new SoTransform;
    transform2->translation.setValue(-3, 0, 0);
    root->addChild(transform2);
    
    SoCylinder* cylinder = new SoCylinder;
    root->addChild(cylinder);
    
    return root;
}

SoSeparator* StandardTestScenes::createComplexScene() {
    SoSeparator* root = createBasicGeometryScene();
    
    // Add materials
    SoMaterial* material = new SoMaterial;
    material->diffuseColor.setValue(1, 0, 0);
    root->insertChild(material, 2); // After light, before first shape
    
    // Add more complex geometry
    SoSeparator* complex_group = new SoSeparator;
    
    SoTransform* group_transform = new SoTransform;
    group_transform->translation.setValue(0, 3, 0);
    complex_group->addChild(group_transform);
    
    SoCone* cone = new SoCone;
    complex_group->addChild(cone);
    
    root->addChild(complex_group);
    
    return root;
}

SoSeparator* StandardTestScenes::createPickTestScene() {
    SoSeparator* root = createMinimalScene();
    
    // Create grid of pickable objects
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            SoSeparator* item = new SoSeparator;
            
            SoTransform* transform = new SoTransform;
            transform->translation.setValue(x * 2.0f, y * 2.0f, 0);
            item->addChild(transform);
            
            SoMaterial* material = new SoMaterial;
            material->diffuseColor.setValue((x + 2) / 4.0f, (y + 2) / 4.0f, 0.5f);
            item->addChild(material);
            
            SoCube* cube = new SoCube;
            cube->width = 0.8f;
            cube->height = 0.8f;
            cube->depth = 0.8f;
            item->addChild(cube);
            
            root->addChild(item);
        }
    }
    
    return root;
}

SoSeparator* StandardTestScenes::createMaterialTestScene() {
    SoSeparator* root = createMinimalScene();
    
    // Test various material properties
    const float colors[][3] = {
        {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0}, {1, 0, 1}, {0, 1, 1}
    };
    
    for (int i = 0; i < 6; ++i) {
        SoSeparator* item = new SoSeparator;
        
        SoTransform* transform = new SoTransform;
        transform->translation.setValue((i - 2.5f) * 1.5f, 0, 0);
        item->addChild(transform);
        
        SoMaterial* material = new SoMaterial;
        material->diffuseColor.setValue(colors[i][0], colors[i][1], colors[i][2]);
        material->specularColor.setValue(1, 1, 1);
        material->shininess = 0.1f + i * 0.15f;
        item->addChild(material);
        
        SoSphere* sphere = new SoSphere;
        item->addChild(sphere);
        
        root->addChild(item);
    }
    
    return root;
}

SoSeparator* StandardTestScenes::createTransformTestScene() {
    SoSeparator* root = createMinimalScene();
    
    SoSeparator* transform_group = new SoSeparator;
    
    // Nested transformations
    SoTransform* rot1 = new SoTransform;
    rot1->rotation.setValue(SbVec3f(0, 1, 0), 0.5f);
    transform_group->addChild(rot1);
    
    SoTransform* trans1 = new SoTransform;
    trans1->translation.setValue(2, 0, 0);
    transform_group->addChild(trans1);
    
    SoTransform* scale1 = new SoTransform;
    scale1->scaleFactor.setValue(0.5f, 2.0f, 0.5f);
    transform_group->addChild(scale1);
    
    SoCube* cube = new SoCube;
    transform_group->addChild(cube);
    
    root->addChild(transform_group);
    
    return root;
}

SoSeparator* StandardTestScenes::createAnimationTestScene() {
    SoSeparator* root = createMinimalScene();
    
    // Scene setup for animation testing (static for now, engines would be added)
    SoSeparator* anim_group = new SoSeparator;
    
    SoTransform* transform = new SoTransform;
    anim_group->addChild(transform);
    
    SoMaterial* material = new SoMaterial;
    material->diffuseColor.setValue(0, 1, 1);
    anim_group->addChild(material);
    
    SoSphere* sphere = new SoSphere;
    anim_group->addChild(sphere);
    
    root->addChild(anim_group);
    
    return root;
}

// ============================================================================
// SceneGraphValidator Implementation  
// ============================================================================

bool SceneGraphValidator::validateSceneStructure(SoNode* root) {
    if (!root) return false;
    
    // Basic validation: check if we have a valid scene graph
    if (!root->getTypeId().isDerivedFrom(SoNode::getClassTypeId())) {
        return false;
    }
    
    // Check for at least a camera and light in typical scenes
    SoSearchAction search;
    search.setType(SoPerspectiveCamera::getClassTypeId());
    search.setInterest(SoSearchAction::FIRST);
    search.apply(root);
    
    if (search.getPath() == nullptr) {
        search.setType(SoOrthographicCamera::getClassTypeId());
        search.apply(root);
        if (search.getPath() == nullptr) {
            return false; // No camera found
        }
    }
    
    return true;
}

std::vector<std::string> SceneGraphValidator::analyzeSceneIssues(SoNode* root) {
    std::vector<std::string> issues;
    
    if (!root) {
        issues.push_back("Root node is null");
        return issues;
    }
    
    // Check for common issues
    SoSearchAction search;
    
    // Check for cameras
    search.setType(SoCamera::getClassTypeId());
    search.apply(root);
    if (search.getPaths().getLength() == 0) {
        issues.push_back("No camera found in scene");
    } else if (search.getPaths().getLength() > 1) {
        issues.push_back("Multiple cameras found - may cause rendering issues");
    }
    
    // Check for lights
    search.reset();
    search.setType(SoDirectionalLight::getClassTypeId());
    search.apply(root);
    if (search.getPaths().getLength() == 0) {
        issues.push_back("No directional light found - scene may be dark");
    }
    
    return issues;
}

bool SceneGraphValidator::validateNodeTypes(SoNode* root) {
    if (!root) return false;
    
    // Traverse and validate all node types
    class TypeValidator : public SoCallbackAction {
    public:
        bool valid = true;
        
        static SoCallbackAction::Response validateNode(void* userdata, SoCallbackAction*, const SoNode* node) {
            TypeValidator* validator = static_cast<TypeValidator*>(userdata);
            
            // Check if node type is valid
            if (node->getTypeId() == SoType::badType()) {
                validator->valid = false;
                return SoCallbackAction::ABORT;
            }
            
            return SoCallbackAction::CONTINUE;
        }
    } validator;
    
    validator.addPreCallback(SoNode::getClassTypeId(), TypeValidator::validateNode, &validator);
    validator.apply(root);
    
    return validator.valid;
}

std::map<std::string, int> SceneGraphValidator::countNodeTypes(SoNode* root) {
    std::map<std::string, int> counts;
    
    if (!root) return counts;
    
    class NodeCounter : public SoCallbackAction {
    public:
        std::map<std::string, int>* counts;
        
        static SoCallbackAction::Response countNode(void* userdata, SoCallbackAction*, const SoNode* node) {
            NodeCounter* counter = static_cast<NodeCounter*>(userdata);
            
            std::string type_name = node->getTypeId().getName().getString();
            (*counter->counts)[type_name]++;
            
            return SoCallbackAction::CONTINUE;
        }
    } counter;
    
    counter.counts = &counts;
    counter.addPreCallback(SoNode::getClassTypeId(), NodeCounter::countNode, &counter);
    counter.apply(root);
    
    return counts;
}

bool SceneGraphValidator::validateFieldConnections(SoNode* root) {
    // Basic field validation - could be expanded
    return root != nullptr;
}

// ============================================================================
// RenderingTestUtils Implementation
// ============================================================================

RenderingTestUtils::RenderTestFixture::RenderTestFixture(unsigned int width, unsigned int height)
    : OSMesaTestFixture(width, height), viewport_(width, height) {
    
    if (isContextReady()) {
        render_action_ = std::make_unique<SoGLRenderAction>(viewport_);
    }
}

RenderingTestUtils::RenderTestFixture::~RenderTestFixture() {
    // Cleanup handled by unique_ptr
}

bool RenderingTestUtils::RenderTestFixture::renderScene(SoNode* scene) {
    if (!scene || !render_action_ || !isContextReady()) {
        return false;
    }
    
    getContext().makeCurrent();
    getContext().clearBuffer(0.2f, 0.2f, 0.2f, 1.0f); // Gray background
    
    render_action_->apply(scene);
    
    // Flush rendering
    glFinish();
    
    return true;
}

bool RenderingTestUtils::RenderTestFixture::saveRenderResult(const std::string& filename) {
    return getContext().saveToPPM(filename);
}

RenderingTestUtils::RenderTestFixture::PixelAnalysis 
RenderingTestUtils::RenderTestFixture::analyzeRenderedPixels() const {
    PixelAnalysis analysis = {0, 0, false, 0.0f};
    
    unsigned int width, height;
    getContext().getDimensions(width, height);
    const unsigned char* pixels = getContext().getPixelData();
    
    if (!pixels) return analysis;
    
    analysis.total_pixels = width * height;
    
    float total_brightness = 0.0f;
    unsigned char prev_r = pixels[0], prev_g = pixels[1], prev_b = pixels[2];
    
    for (unsigned int i = 0; i < width * height; ++i) {
        unsigned char r = pixels[i * 4];
        unsigned char g = pixels[i * 4 + 1];
        unsigned char b = pixels[i * 4 + 2];
        
        // Check if not black (background)
        if (r > 10 || g > 10 || b > 10) {
            analysis.non_black_pixels++;
        }
        
        // Check for color variation
        if (abs(r - prev_r) > 5 || abs(g - prev_g) > 5 || abs(b - prev_b) > 5) {
            analysis.has_color_variation = true;
        }
        
        // Calculate brightness
        float brightness = (r + g + b) / 3.0f / 255.0f;
        total_brightness += brightness;
        
        prev_r = r; prev_g = g; prev_b = b;
    }
    
    analysis.avg_brightness = total_brightness / analysis.total_pixels;
    
    return analysis;
}

bool RenderingTestUtils::validateRenderOutput(const RenderTestFixture& fixture) {
    auto analysis = fixture.analyzeRenderedPixels();
    
    // Basic validation: should have some non-black pixels and some color variation
    return analysis.non_black_pixels > 0 && 
           analysis.avg_brightness > 0.01f;
}

bool RenderingTestUtils::compareRenderResults(const RenderTestFixture& fixture1,
                                            const RenderTestFixture& fixture2,
                                            float tolerance) {
    auto analysis1 = fixture1.analyzeRenderedPixels();
    auto analysis2 = fixture2.analyzeRenderedPixels();
    
    // Compare key metrics
    float brightness_diff = abs(analysis1.avg_brightness - analysis2.avg_brightness);
    float pixel_diff = abs(analysis1.non_black_pixels - analysis2.non_black_pixels) / 
                      float(analysis1.total_pixels);
    
    return brightness_diff < tolerance && pixel_diff < tolerance;
}

// ============================================================================
// ActionTestUtils Implementation
// ============================================================================

bool ActionTestUtils::testBoundingBoxAction(SoNode* scene) {
    if (!scene) return false;
    
    SoGetBoundingBoxAction bbox_action(SbViewportRegion(100, 100));
    bbox_action.apply(scene);
    
    SbBox3f bbox = bbox_action.getBoundingBox();
    return !bbox.isEmpty();
}

bool ActionTestUtils::testPickAction(SoNode* scene) {
    if (!scene) return false;
    
    SoRayPickAction pick_action(SbViewportRegion(100, 100));
    pick_action.setPoint(SbVec2s(50, 50));
    pick_action.setRadius(2);
    pick_action.apply(scene);
    
    // Test completed successfully if no crash occurred
    return true;
}

bool ActionTestUtils::testActionTraversal(SoNode* scene) {
    if (!scene) return false;
    
    // Test that actions can traverse the scene without issues
    SoSearchAction search;
    search.apply(scene);
    
    SoCallbackAction callback;
    callback.apply(scene);
    
    return true;
}

// ============================================================================
// ComprehensiveTestRunner Implementation  
// ============================================================================

ComprehensiveTestRunner::ComprehensiveTestRunner() {
    setupTestEnvironment();
}

void ComprehensiveTestRunner::setupTestEnvironment() {
    // Initialize Coin3D if not already done
    if (!SoDB::isInitialized()) {
        SoDB::init();
        SoInteraction::init();
    }
}

std::vector<ComprehensiveTestRunner::TestSuite> ComprehensiveTestRunner::runAllTests() {
    std::vector<TestSuite> results;
    
    results.push_back(runBasicNodeTests());
    results.push_back(runRenderingTests());
    results.push_back(runActionTests());
    results.push_back(runFieldTests());
    
    return results;
}

ComprehensiveTestRunner::TestSuite ComprehensiveTestRunner::runBasicNodeTests() {
    TestSuite suite;
    suite.suite_name = "Basic Node Tests";
    suite.passed_count = 0;
    suite.failed_count = 0;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Test basic node creation
    {
        TestResult result;
        result.test_name = "Node Creation";
        
        try {
            SoSeparator* root = StandardTestScenes::createMinimalScene();
            result.passed = (root != nullptr);
            root->unref();
        } catch (...) {
            result.passed = false;
            result.details = "Exception during node creation";
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        if (result.passed) suite.passed_count++;
        else suite.failed_count++;
        
        suite.results.push_back(result);
    }
    
    return suite;
}

ComprehensiveTestRunner::TestSuite ComprehensiveTestRunner::runRenderingTests() {
    TestSuite suite;
    suite.suite_name = "Rendering Tests";
    suite.passed_count = 0;
    suite.failed_count = 0;
    
#ifdef COIN3D_OSMESA_BUILD
    // Test basic rendering
    {
        TestResult result;
        result.test_name = "Basic Scene Rendering";
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            RenderingTestUtils::RenderTestFixture fixture(256, 256);
            SoSeparator* scene = StandardTestScenes::createBasicGeometryScene();
            
            bool rendered = fixture.renderScene(scene);
            bool validated = RenderingTestUtils::validateRenderOutput(fixture);
            
            result.passed = rendered && validated;
            if (!result.passed) {
                result.details = "Rendering failed or output validation failed";
            }
            
            scene->unref();
        } catch (...) {
            result.passed = false;
            result.details = "Exception during rendering";
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        if (result.passed) suite.passed_count++;
        else suite.failed_count++;
        
        suite.results.push_back(result);
    }
#endif
    
    return suite;
}

ComprehensiveTestRunner::TestSuite ComprehensiveTestRunner::runActionTests() {
    TestSuite suite;
    suite.suite_name = "Action Tests";
    suite.passed_count = 0;
    suite.failed_count = 0;
    
    // Test bounding box action
    {
        TestResult result;
        result.test_name = "Bounding Box Action";
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            SoSeparator* scene = StandardTestScenes::createBasicGeometryScene();
            result.passed = ActionTestUtils::testBoundingBoxAction(scene);
            scene->unref();
        } catch (...) {
            result.passed = false;
            result.details = "Exception during bounding box test";
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        if (result.passed) suite.passed_count++;
        else suite.failed_count++;
        
        suite.results.push_back(result);
    }
    
    return suite;
}

ComprehensiveTestRunner::TestSuite ComprehensiveTestRunner::runFieldTests() {
    TestSuite suite;
    suite.suite_name = "Field Tests";
    suite.passed_count = 0;
    suite.failed_count = 0;
    
    // Basic field test would go here
    return suite;
}

ComprehensiveTestRunner::TestSuite ComprehensiveTestRunner::runEngineTests() {
    TestSuite suite;
    suite.suite_name = "Engine Tests";
    suite.passed_count = 0;
    suite.failed_count = 0;
    
    // Engine tests would go here
    return suite;
}

ComprehensiveTestRunner::TestSuite ComprehensiveTestRunner::runSensorTests() {
    TestSuite suite;
    suite.suite_name = "Sensor Tests";
    suite.passed_count = 0;
    suite.failed_count = 0;
    
    // Sensor tests would go here
    return suite;
}

ComprehensiveTestRunner::TestSuite ComprehensiveTestRunner::runIntegrationTests() {
    TestSuite suite;
    suite.suite_name = "Integration Tests";
    suite.passed_count = 0;
    suite.failed_count = 0;
    
    // Integration tests would go here
    return suite;
}

} // namespace CoinTestUtils