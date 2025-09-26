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
 * @file scene_graph_utils.h
 * @brief Comprehensive scene graph testing utilities (migrated from tests_old)
 *
 * This combines the best utilities from tests_old into the simple test framework
 * using RGB output instead of PNG.
 */

#ifndef SCENE_GRAPH_UTILS_H
#define SCENE_GRAPH_UTILS_H

#include "../test_utils.h"
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <memory>
#include <vector>
#include <map>

namespace SimpleTest {

/**
 * @brief Standard test scenes for comprehensive testing
 */
class StandardScenes {
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
     * @brief Create scene with materials for color testing
     */
    static SoSeparator* createMaterialTestScene();
    
    /**
     * @brief Create scene with transformations
     */
    static SoSeparator* createTransformTestScene();
};

/**
 * @brief Scene graph validation utilities
 */
class SceneValidator {
public:
    /**
     * @brief Validate basic scene graph structure
     */
    static bool validateSceneStructure(SoNode* root);
    
    /**
     * @brief Count nodes by type
     */
    static std::map<std::string, int> countNodeTypes(SoNode* root);
    
    /**
     * @brief Check if scene has required components (camera, light)
     */
    static bool hasRequiredComponents(SoNode* root);
};

#ifdef HAVE_OSMESA

/**
 * @brief OSMesa context for headless rendering
 */
class OSMesaContext {
public:
    OSMesaContext(int width = 256, int height = 256);
    ~OSMesaContext();
    
    bool isValid() const { return context_ != nullptr; }
    bool makeCurrent();
    void* getBuffer() const { return buffer_.get(); }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    
private:
    void* context_;
    std::unique_ptr<unsigned char[]> buffer_;
    int width_, height_;
};

/**
 * @brief Rendering test utilities with OSMesa
 */
class RenderingUtils {
public:
    class RenderFixture {
    public:
        RenderFixture(int width = 256, int height = 256);
        ~RenderFixture();
        
        bool renderScene(SoNode* scene);
        bool saveResult(const std::string& filename);
        
        // Pixel analysis
        struct PixelStats {
            int non_black_pixels;
            int total_pixels;
            float avg_brightness;
            bool has_variation;
        };
        PixelStats analyzePixels() const;
        
    private:
        std::unique_ptr<OSMesaContext> context_;
        std::unique_ptr<SoOffscreenRenderer> renderer_;
        SbViewportRegion viewport_;
        int width_, height_;
    };
    
    /**
     * @brief Quick validation that render produced something
     */
    static bool validateRenderOutput(const RenderFixture& fixture);
};

#endif // HAVE_OSMESA

/**
 * @brief Action testing utilities
 */
class ActionUtils {
public:
    /**
     * @brief Test bounding box computation
     */
    static bool testBoundingBox(SoNode* scene);
    
    /**
     * @brief Test basic action traversal
     */
    static bool testActionTraversal(SoNode* scene);
};

} // namespace SimpleTest

#endif // SCENE_GRAPH_UTILS_H