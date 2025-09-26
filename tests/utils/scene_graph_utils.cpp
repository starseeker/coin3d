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
 * @file scene_graph_utils.cpp
 * @brief Implementation of scene graph testing utilities
 */

#include "scene_graph_utils.h"
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoLight.h>
#include <Inventor/SbVec3f.h>
#include <iostream>

#ifdef HAVE_OSMESA
#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#endif

namespace SimpleTest {

// StandardScenes implementations
SoSeparator* StandardScenes::createMinimalScene() {
    SoSeparator* root = new SoSeparator;
    root->ref();
    
    // Add camera
    SoPerspectiveCamera* camera = new SoPerspectiveCamera;
    camera->position.setValue(0, 0, 5);
    camera->nearDistance.setValue(1.0f);
    camera->farDistance.setValue(10.0f);
    root->addChild(camera);
    
    // Add light
    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(0, 0, -1);
    root->addChild(light);
    
    return root;
}

SoSeparator* StandardScenes::createBasicGeometryScene() {
    SoSeparator* root = createMinimalScene();
    
    // Add a cube
    SoCube* cube = new SoCube;
    cube->width.setValue(2.0f);
    cube->height.setValue(2.0f);
    cube->depth.setValue(2.0f);
    root->addChild(cube);
    
    return root;
}

SoSeparator* StandardScenes::createMaterialTestScene() {
    SoSeparator* root = createBasicGeometryScene();
    
    // Insert material before geometry
    SoMaterial* material = new SoMaterial;
    material->diffuseColor.setValue(0.8f, 0.2f, 0.2f); // Red
    root->insertChild(material, root->getNumChildren() - 1);
    
    return root;
}

SoSeparator* StandardScenes::createTransformTestScene() {
    SoSeparator* root = createMinimalScene();
    
    // Add transform
    SoTransform* transform = new SoTransform;
    transform->translation.setValue(1, 1, 0);
    transform->rotation.setValue(SbVec3f(0, 1, 0), 0.785f); // 45 degrees
    root->addChild(transform);
    
    // Add geometry
    SoSphere* sphere = new SoSphere;
    sphere->radius.setValue(1.0f);
    root->addChild(sphere);
    
    return root;
}

// SceneValidator implementations
bool SceneValidator::validateSceneStructure(SoNode* root) {
    if (!root) return false;
    
    // Check that it's a valid node
    if (!root->getTypeId().isDerivedFrom(SoNode::getClassTypeId())) {
        return false;
    }
    
    return true;
}

std::map<std::string, int> SceneValidator::countNodeTypes(SoNode* root) {
    std::map<std::string, int> counts;
    
    if (!root) return counts;
    
    // Count this node
    std::string typeName = root->getTypeId().getName().getString();
    counts[typeName]++;
    
    // If it's a group, count children
    if (root->isOfType(SoGroup::getClassTypeId())) {
        SoGroup* group = static_cast<SoGroup*>(root);
        for (int i = 0; i < group->getNumChildren(); i++) {
            SoNode* child = group->getChild(i);
            auto childCounts = countNodeTypes(child);
            for (const auto& pair : childCounts) {
                counts[pair.first] += pair.second;
            }
        }
    }
    
    return counts;
}

bool SceneValidator::hasRequiredComponents(SoNode* root) {
    if (!root) return false;
    
    // Search for camera
    SoSearchAction searchCamera;
    searchCamera.setType(SoCamera::getClassTypeId());
    searchCamera.setInterest(SoSearchAction::FIRST);
    searchCamera.apply(root);
    
    if (searchCamera.getPath() == nullptr) {
        return false;
    }
    
    // Search for light
    SoSearchAction searchLight;
    searchLight.setType(SoLight::getClassTypeId());
    searchLight.setInterest(SoSearchAction::FIRST);
    searchLight.apply(root);
    
    return (searchLight.getPath() != nullptr);
}

#ifdef HAVE_OSMESA

// OSMesaContext implementation
OSMesaContext::OSMesaContext(int width, int height) 
    : context_(nullptr), width_(width), height_(height) {
    
    buffer_.reset(new unsigned char[width * height * 4]); // RGBA
    context_ = OSMesaCreateContext(OSMESA_RGBA, nullptr);
    
    if (context_ && buffer_) {
        if (OSMesaMakeCurrent(context_, buffer_.get(), GL_UNSIGNED_BYTE, width, height)) {
            OSMesaPixelStore(OSMESA_Y_UP, 0); // OpenGL bottom-left origin
        }
    }
}

OSMesaContext::~OSMesaContext() {
    if (context_) {
        OSMesaDestroyContext(context_);
    }
}

bool OSMesaContext::makeCurrent() {
    if (!context_ || !buffer_) return false;
    
    bool result = OSMesaMakeCurrent(context_, buffer_.get(), GL_UNSIGNED_BYTE, width_, height_);
    if (result) {
        OSMesaPixelStore(OSMESA_Y_UP, 0);
    }
    return result;
}

// RenderingUtils::RenderFixture implementation
RenderingUtils::RenderFixture::RenderFixture(int width, int height) 
    : width_(width), height_(height), viewport_(width, height) {
    
    context_.reset(new OSMesaContext(width, height));
    if (context_->isValid()) {
        renderer_.reset(new SoOffscreenRenderer(viewport_));
        renderer_->setBackgroundColor(SbColor(0.2f, 0.2f, 0.3f)); // Dark blue
    }
}

RenderingUtils::RenderFixture::~RenderFixture() = default;

bool RenderingUtils::RenderFixture::renderScene(SoNode* scene) {
    if (!context_ || !context_->isValid() || !renderer_ || !scene) {
        return false;
    }
    
    if (!context_->makeCurrent()) {
        return false;
    }
    
    return renderer_->render(scene);
}

bool RenderingUtils::RenderFixture::saveResult(const std::string& filename) {
    if (!renderer_) return false;
    
    const unsigned char* buffer = renderer_->getBuffer();
    if (!buffer) return false;
    
    return RGBOutput::saveRGBA_toRGB(filename, buffer, width_, height_);
}

RenderingUtils::RenderFixture::PixelStats RenderingUtils::RenderFixture::analyzePixels() const {
    PixelStats stats = {0, 0, 0.0f, false};
    
    if (!renderer_) return stats;
    
    const unsigned char* buffer = renderer_->getBuffer();
    if (!buffer) return stats;
    
    stats.total_pixels = width_ * height_;
    int brightness_sum = 0;
    unsigned char first_pixel[3] = {buffer[0], buffer[1], buffer[2]};
    
    for (int i = 0; i < stats.total_pixels; i++) {
        unsigned char r = buffer[i * 4 + 0];
        unsigned char g = buffer[i * 4 + 1];
        unsigned char b = buffer[i * 4 + 2];
        
        // Check if non-black (with small tolerance)
        if (r > 10 || g > 10 || b > 10) {
            stats.non_black_pixels++;
        }
        
        // Calculate brightness
        int brightness = (r + g + b) / 3;
        brightness_sum += brightness;
        
        // Check for variation
        if (!stats.has_variation && i > 0) {
            if (abs(r - first_pixel[0]) > 10 ||
                abs(g - first_pixel[1]) > 10 ||
                abs(b - first_pixel[2]) > 10) {
                stats.has_variation = true;
            }
        }
    }
    
    stats.avg_brightness = static_cast<float>(brightness_sum) / stats.total_pixels;
    return stats;
}

bool RenderingUtils::validateRenderOutput(const RenderFixture& fixture) {
    auto stats = fixture.analyzePixels();
    
    // Basic validation: should have some non-black pixels and some variation
    return (stats.non_black_pixels > 0 && 
            stats.avg_brightness > 10.0f);
}

#endif // HAVE_OSMESA

// ActionUtils implementation
bool ActionUtils::testBoundingBox(SoNode* scene) {
    if (!scene) return false;
    
    SbViewportRegion viewport;
    SoGetBoundingBoxAction action(viewport);
    action.apply(scene);
    
    SbBox3f bbox = action.getBoundingBox();
    return !bbox.isEmpty();
}

bool ActionUtils::testActionTraversal(SoNode* scene) {
    if (!scene) return false;
    
    // Test that action traversal works without crashing
    try {
        SbViewportRegion viewport;
        SoGetBoundingBoxAction action(viewport);
        action.apply(scene);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace SimpleTest