/**
 * @file test_comprehensive.cpp
 * @brief Comprehensive test suite integrating functionality from tests_old
 *
 * This consolidates the most important tests from tests_old into a single
 * comprehensive test using the simple framework and RGB output.
 */

#include "test_utils.h"
#include "utils/scene_graph_utils.h"
#include <iostream>
#include <memory>

using namespace SimpleTest;

int main() {
    TestFixture fixture;
    TestRunner runner;
    
    std::cout << "=== Comprehensive Coin3D Test Suite ===" << std::endl;
    std::cout << "Consolidated from tests_old using RGB output\n" << std::endl;
    
    // Test 1: Basic scene creation and validation
    runner.startTest("Scene Creation and Validation");
    try {
        auto scene = StandardScenes::createMinimalScene();
        if (!scene) {
            runner.endTest(false, "Failed to create minimal scene");
            return runner.getSummary();
        }
        
        if (!SceneValidator::validateSceneStructure(scene)) {
            scene->unref();
            runner.endTest(false, "Scene structure validation failed");
            return runner.getSummary();
        }
        
        if (!SceneValidator::hasRequiredComponents(scene)) {
            scene->unref();
            runner.endTest(false, "Scene missing required components");
            return runner.getSummary();
        }
        
        scene->unref();
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
    }
    
    // Test 2: Node type counting
    runner.startTest("Node Type Analysis");
    try {
        auto scene = StandardScenes::createBasicGeometryScene();
        if (!scene) {
            runner.endTest(false, "Failed to create geometry scene");
            return runner.getSummary();
        }
        
        auto nodeCounts = SceneValidator::countNodeTypes(scene);
        
        std::cout << "  Found node types: ";
        for (const auto& pair : nodeCounts) {
            std::cout << pair.first << "=" << pair.second << " ";
        }
        std::cout << std::endl;
        
        // Verify we have expected node types (note: names don't have "So" prefix)
        bool hasCamera = nodeCounts.find("PerspectiveCamera") != nodeCounts.end();
        bool hasLight = nodeCounts.find("DirectionalLight") != nodeCounts.end();
        bool hasCube = nodeCounts.find("Cube") != nodeCounts.end();
        bool hasSeparator = nodeCounts.find("Separator") != nodeCounts.end();
        
        if (!hasCamera || !hasLight || !hasCube || !hasSeparator) {
            scene->unref();
            std::string missing = "Missing: ";
            if (!hasCamera) missing += "PerspectiveCamera ";
            if (!hasLight) missing += "DirectionalLight ";  
            if (!hasCube) missing += "Cube ";
            if (!hasSeparator) missing += "Separator ";
            runner.endTest(false, missing);
            return runner.getSummary();
        }
        
        std::cout << "  Node counts: ";
        for (const auto& pair : nodeCounts) {
            std::cout << pair.first << "=" << pair.second << " ";
        }
        std::cout << std::endl;
        
        scene->unref();
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
    }
    
    // Test 3: Action utilities
    runner.startTest("Action Testing");
    try {
        auto scene = StandardScenes::createBasicGeometryScene();
        if (!scene) {
            runner.endTest(false, "Failed to create scene for action test");
            return runner.getSummary();
        }
        
        if (!ActionUtils::testBoundingBox(scene)) {
            scene->unref();
            runner.endTest(false, "Bounding box test failed");
            return runner.getSummary();
        }
        
        if (!ActionUtils::testActionTraversal(scene)) {
            scene->unref();
            runner.endTest(false, "Action traversal test failed");
            return runner.getSummary();
        }
        
        scene->unref();
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
    }

#ifdef HAVE_OSMESA
    // Test 4: Rendering with RGB output
    runner.startTest("Rendering and RGB Output");
    try {
        auto scene = StandardScenes::createMaterialTestScene();
        if (!scene) {
            runner.endTest(false, "Failed to create material test scene");
            return runner.getSummary();
        }
        
        RenderingUtils::RenderFixture renderFixture(256, 256);
        
        if (!renderFixture.renderScene(scene)) {
            scene->unref();
            runner.endTest(false, "Scene rendering failed");
            return runner.getSummary();
        }
        
        // Analyze rendered output
        auto stats = renderFixture.analyzePixels();
        std::cout << "  Render stats: " << stats.non_black_pixels << "/" 
                  << stats.total_pixels << " non-black pixels, "
                  << "avg brightness: " << stats.avg_brightness << std::endl;
        
        if (!RenderingUtils::validateRenderOutput(renderFixture)) {
            scene->unref();
            runner.endTest(false, "Render output validation failed");
            return runner.getSummary();
        }
        
        // Save result as RGB for inspection
        if (!renderFixture.saveResult("comprehensive_test_render.rgb")) {
            std::cout << "  Warning: Failed to save render result" << std::endl;
        } else {
            std::cout << "  Render saved to: comprehensive_test_render.rgb" << std::endl;
        }
        
        scene->unref();
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
    }
    
    // Test 5: Multiple scene types rendering
    runner.startTest("Multiple Scene Rendering");
    try {
        struct TestScene {
            const char* name;
            SoSeparator* (*creator)();
        };
        
        TestScene testScenes[] = {
            {"minimal", StandardScenes::createMinimalScene},
            {"geometry", StandardScenes::createBasicGeometryScene}, 
            {"material", StandardScenes::createMaterialTestScene},
            {"transform", StandardScenes::createTransformTestScene}
        };
        
        bool allPassed = true;
        for (const auto& testScene : testScenes) {
            auto scene = testScene.creator();
            if (!scene) {
                allPassed = false;
                break;
            }
            
            RenderingUtils::RenderFixture fixture(128, 128);
            bool renderOk = fixture.renderScene(scene);
            
            if (renderOk) {
                std::string filename = std::string("comprehensive_") + 
                                     testScene.name + "_scene.rgb";
                fixture.saveResult(filename);
                std::cout << "  " << testScene.name << " scene rendered → " 
                          << filename << std::endl;
            } else {
                std::cout << "  " << testScene.name << " scene render failed" << std::endl;
                allPassed = false;
            }
            
            scene->unref();
        }
        
        runner.endTest(allPassed, allPassed ? "" : "Some scene renders failed");
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
    }
    
#else
    std::cout << "Skipping rendering tests - OSMesa not available" << std::endl;
#endif // HAVE_OSMESA
    
    // Test 6: Memory management validation
    runner.startTest("Memory Management");
    try {
        // Create and destroy multiple scenes to test ref counting
        for (int i = 0; i < 10; i++) {
            auto scene = StandardScenes::createBasicGeometryScene();
            if (!scene) {
                runner.endTest(false, "Failed to create scene in memory test");
                return runner.getSummary();
            }
            scene->unref();
        }
        
        runner.endTest(true);
    } catch (const std::exception& e) {
        runner.endTest(false, std::string("Exception: ") + e.what());
    }
    
    std::cout << "\n=== Test Completion ===" << std::endl;
    return runner.getSummary();
}