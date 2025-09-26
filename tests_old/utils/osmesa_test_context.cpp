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

#include "osmesa_test_context.h"
#include "png_test_utils.h"

#ifdef COIN3D_OSMESA_BUILD

#include <iostream>
#include <fstream>
#include <cstring>
#include <Inventor/SoDB.h>

namespace CoinTestUtils {

// ============================================================================
// OSMesaTestContext Implementation
// ============================================================================

OSMesaTestContext::OSMesaTestContext(unsigned int width, unsigned int height, GLenum format)
    : context_(nullptr), width_(width), height_(height), format_(format) {
    
    // Allocate larger buffer like BRL-CAD does - OSMesa needs space for internal
    // operations like textures, framebuffers, etc. beyond just the final image
    // Use at least 4096x4096 to match OSMesa's MAX_WIDTH/MAX_HEIGHT settings
    size_t buffer_size = std::max(static_cast<size_t>(4096 * 4096 * 4), 
                                 static_cast<size_t>(width_ * height_ * 4));
    buffer_ = std::make_unique<unsigned char[]>(buffer_size);
    
    // Create OSMesa context
    context_ = OSMesaCreateContext(format_, nullptr);
    if (!context_) {
        std::cerr << "Failed to create OSMesa context" << std::endl;
        return;
    }
    
    // Bind context to buffer
    if (!OSMesaMakeCurrent(context_, buffer_.get(), GL_UNSIGNED_BYTE, width_, height_)) {
        std::cerr << "Failed to make OSMesa context current" << std::endl;
        OSMesaDestroyContext(context_);
        context_ = nullptr;
        return;
    }
    
    // Clear any GL errors that might have occurred during context creation
    // This prevents warnings in cc_glglue_instance() about context setup errors
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR) {
        // Clear errors without reporting - context creation can generate spurious errors
    }
}

OSMesaTestContext::~OSMesaTestContext() {
    cleanup();
}

OSMesaTestContext::OSMesaTestContext(OSMesaTestContext&& other) noexcept
    : context_(other.context_), buffer_(std::move(other.buffer_)),
      width_(other.width_), height_(other.height_), format_(other.format_) {
    other.context_ = nullptr;
    other.width_ = 0;
    other.height_ = 0;
}

OSMesaTestContext& OSMesaTestContext::operator=(OSMesaTestContext&& other) noexcept {
    if (this != &other) {
        cleanup();
        context_ = other.context_;
        buffer_ = std::move(other.buffer_);
        width_ = other.width_;
        height_ = other.height_;
        format_ = other.format_;
        
        other.context_ = nullptr;
        other.width_ = 0;
        other.height_ = 0;
    }
    return *this;
}

bool OSMesaTestContext::makeCurrent() {
    if (!context_ || !buffer_) return false;
    
    bool result = OSMesaMakeCurrent(context_, buffer_.get(), GL_UNSIGNED_BYTE, width_, height_);
    if (result) {
        // Clear any GL errors that might have occurred during context creation
        // This prevents warnings in cc_glglue_instance() about context setup errors
        GLenum error;
        while ((error = glGetError()) != GL_NO_ERROR) {
            // Clear errors without reporting - context creation can generate spurious errors
        }
        
        // After making context current, ensure OpenGL extensions are properly detected
        // This is crucial for FBO support detection - equivalent to glewInit() in OSMesa examples
        
        // Verify extensions are available
        const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
        if (extensions && strstr(extensions, "GL_EXT_framebuffer_object")) {
            // Extension is in the string, ensure functions are loadable
            void* genFBO = (void*)OSMesaGetProcAddress("glGenFramebuffersEXT");
            void* bindFBO = (void*)OSMesaGetProcAddress("glBindFramebufferEXT"); 
            if (genFBO && bindFBO) {
                std::cout << "OSMesa FBO functions successfully detected" << std::endl;
            }
        }
    }
    return result;
}

bool OSMesaTestContext::saveToPPM(const std::string& filename) const {
    if (!buffer_ || !isValid()) return false;
    
    std::ofstream file(filename, std::ios::binary);
    if (!file) return false;
    
    // PPM header
    file << "P6\n" << width_ << " " << height_ << "\n255\n";
    
    // Convert RGBA to RGB and flip vertically (PPM expects top-down)
    for (int y = height_ - 1; y >= 0; --y) {
        for (unsigned int x = 0; x < width_; ++x) {
            size_t offset = (y * width_ + x) * 4;
            file.put(buffer_[offset]);     // R
            file.put(buffer_[offset + 1]); // G  
            file.put(buffer_[offset + 2]); // B
            // Skip alpha
        }
    }
    
    return file.good();
}

bool OSMesaTestContext::saveToPNG(const std::string& filename) const {
    if (!buffer_ || !isValid()) return false;
    
    return CoinTestUtils::writePNG(filename, buffer_.get(), width_, height_, true);
}

void OSMesaTestContext::clearBuffer(float r, float g, float b, float a) {
    if (!isValid()) return;
    
    makeCurrent();
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OSMesaTestContext::cleanup() {
    if (context_) {
        OSMesaDestroyContext(context_);
        context_ = nullptr;
    }
}

// ============================================================================
// OSMesaCallbackManager Implementation  
// ============================================================================

// OSMesa ContextManager implementation for SoDB::init()
class OSMesaCallbackManager::OSMesaContextManager : public SoDB::ContextManager {
public:
    virtual void * createOffscreenContext(unsigned int width, unsigned int height) override {
        auto* context = new OSMesaTestContext(width, height);
        if (!context->isValid()) {
            delete context;
            return nullptr;
        }
        return context;
    }
    
    virtual SbBool makeContextCurrent(void * context) override {
        if (!context) return FALSE;
        auto* osmesa_ctx = static_cast<OSMesaTestContext*>(context);
        return osmesa_ctx->makeCurrent() ? TRUE : FALSE;
    }
    
    virtual void restorePreviousContext(void * context) override {
        // OSMesa doesn't need explicit context switching in our test setup
        (void)context;
    }
    
    virtual void destroyContext(void * context) override {
        if (context) {
            delete static_cast<OSMesaTestContext*>(context);
        }
    }
};

OSMesaCallbackManager::OSMesaCallbackManager() 
    : context_manager_(std::make_unique<OSMesaContextManager>()) {
    // Set up the context manager via SoDB::init()
    SoDB::init(context_manager_.get());
}

OSMesaCallbackManager::~OSMesaCallbackManager() {
    // Context manager will be automatically cleaned up when the unique_ptr is destroyed
    // SoDB will continue to use the context manager until the library is shut down
}

// ============================================================================
// OSMesaTestFixture Implementation
// ============================================================================

OSMesaTestFixture::OSMesaTestFixture(unsigned int width, unsigned int height)
    : callback_manager_(), context_(width, height) {
    
    if (!context_.isValid()) {
        std::cerr << "Warning: OSMesa test context creation failed" << std::endl;
    }
}

} // namespace CoinTestUtils

#endif // COIN3D_OSMESA_BUILD