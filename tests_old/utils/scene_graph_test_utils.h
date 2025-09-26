#ifndef SCENE_GRAPH_TEST_UTILS_H
#define SCENE_GRAPH_TEST_UTILS_H

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
 * @file scene_graph_test_utils.h
 * @brief Comprehensive utilities for testing Coin3D scene graph functionality
 * 
 * Provides standardized test scenarios, validation helpers, and common
 * scene graph operations for comprehensive user-facing API testing.
 */

#include "test_common.h"
#include "osmesa_test_context.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoPickAction.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <memory>
#include <vector>
#include <map>
#include <functional>

namespace CoinTestUtils {

/**
 * @brief Standard test scenes for comprehensive testing
 */
class StandardTestScenes {
public:
    /**
     * @brief Create minimal valid scene (camera + light + separator)
     */
    static SoSeparator* createMinimalScene();
    
    /**
     * @brief Create scene with basic geometric shapes
     */
    static SoSeparator* createBasicGeometryScene();
    
    /**
     * @brief Create scene with various node types for testing
     */
    static SoSeparator* createComplexScene();
    
    /**
     * @brief Create scene optimized for picking tests
     */
    static SoSeparator* createPickTestScene();
    
    /**
     * @brief Create scene with materials and textures
     */
    static SoSeparator* createMaterialTestScene();
    
    /**
     * @brief Create scene with transformations
     */
    static SoSeparator* createTransformTestScene();
    
    /**
     * @brief Create scene with animation-capable nodes
     */
    static SoSeparator* createAnimationTestScene();
};

/**
 * @brief Scene graph validation and analysis utilities
 */
class SceneGraphValidator {
public:
    /**
     * @brief Validate basic scene graph structure
     */
    static bool validateSceneStructure(SoNode* root);
    
    /**
     * @brief Check for common scene graph issues
     */
    static std::vector<std::string> analyzeSceneIssues(SoNode* root);
    
    /**
     * @brief Validate node type hierarchy
     */
    static bool validateNodeTypes(SoNode* root);
    
    /**
     * @brief Count nodes by type
     */
    static std::map<std::string, int> countNodeTypes(SoNode* root);
    
    /**
     * @brief Validate field connections
     */
    static bool validateFieldConnections(SoNode* root);
};

/**
 * @brief Rendering test utilities with OSMesa integration
 */
class RenderingTestUtils {
public:
    /**
     * @brief Test fixture that combines OSMesa with scene rendering
     */
    class RenderTestFixture : public OSMesaTestFixture {
    public:
        RenderTestFixture(unsigned int width = 256, unsigned int height = 256);
        virtual ~RenderTestFixture();
        
        /**
         * @brief Render scene and capture result
         */
        bool renderScene(SoNode* scene);
        
        /**
         * @brief Get viewport region for rendering
         */
        SbViewportRegion getViewport() const { return viewport_; }
        
        /**
         * @brief Get render action for custom rendering
         */
        SoGLRenderAction* getRenderAction() { return render_action_.get(); }
        
        /**
         * @brief Save rendered result for debugging
         */
        bool saveRenderResult(const std::string& filename);
        
        /**
         * @brief Save rendered result as PNG for debugging
         */
        bool saveRenderResultPNG(const std::string& filename);
        
        /**
         * @brief Analyze rendered pixels for validation
         */
        struct PixelAnalysis {
            int non_black_pixels;
            int total_pixels;
            bool has_color_variation;
            float avg_brightness;
        };
        PixelAnalysis analyzeRenderedPixels() const;
        
    private:
        SbViewportRegion viewport_;
        std::unique_ptr<SoGLRenderAction> render_action_;
    };
    
    /**
     * @brief Validate that rendering produces expected output
     */
    static bool validateRenderOutput(const RenderTestFixture& fixture);
    
    /**
     * @brief Compare two render results for regression testing
     */
    static bool compareRenderResults(const RenderTestFixture& fixture1,
                                   const RenderTestFixture& fixture2,
                                   float tolerance = 0.05f);
};

/**
 * @brief Action testing utilities
 */
class ActionTestUtils {
public:
    /**
     * @brief Test bounding box computation
     */
    static bool testBoundingBoxAction(SoNode* scene);
    
    /**
     * @brief Test pick action with various ray configurations
     */
    static bool testPickAction(SoNode* scene);
    
    /**
     * @brief Test action traversal order
     */
    static bool testActionTraversal(SoNode* scene);
    
    /**
     * @brief Helper for timing action performance
     */
    template<typename ActionType>
    static double timeAction(SoNode* scene, ActionType* action, int iterations = 100);
};

/**
 * @brief Field and engine testing utilities
 */
class FieldTestUtils {
public:
    /**
     * @brief Test field connections and updates
     */
    static bool testFieldConnections(SoNode* node);
    
    /**
     * @brief Test field serialization/deserialization
     */
    static bool testFieldSerialization(SoNode* node);
    
    /**
     * @brief Validate field types and values
     */
    static bool validateFieldTypes(SoNode* node);
    
    /**
     * @brief Test field change notifications
     */
    static bool testFieldNotifications(SoNode* node);
};

/**
 * @brief Comprehensive test runner for all user-facing features
 */
class ComprehensiveTestRunner {
public:
    struct TestResult {
        std::string test_name;
        bool passed;
        std::string details;
        double execution_time_ms;
    };
    
    struct TestSuite {
        std::string suite_name;
        std::vector<TestResult> results;
        int passed_count;
        int failed_count;
    };
    
    ComprehensiveTestRunner();
    
    /**
     * @brief Run all comprehensive tests
     */
    std::vector<TestSuite> runAllTests();
    
    /**
     * @brief Run tests for specific module
     */
    TestSuite runModuleTests(const std::string& module_name);
    
    /**
     * @brief Generate test report
     */
    void generateReport(const std::vector<TestSuite>& results, 
                       const std::string& filename = "coin3d_test_report.html");

private:
    void setupTestEnvironment();
    TestSuite runBasicNodeTests();
    TestSuite runRenderingTests();
    TestSuite runActionTests();
    TestSuite runFieldTests();
    TestSuite runEngineTests();
    TestSuite runSensorTests();
    TestSuite runIntegrationTests();
};

} // namespace CoinTestUtils

#endif // SCENE_GRAPH_TEST_UTILS_H