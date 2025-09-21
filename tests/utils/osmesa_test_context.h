#ifndef OSMESA_TEST_CONTEXT_H
#define OSMESA_TEST_CONTEXT_H

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
 * @file osmesa_test_context.h
 * @brief Centralized OSMesa context management for comprehensive Coin3D testing
 * 
 * This provides a unified interface for all rendering tests to use OSMesa
 * offscreen rendering, enabling comprehensive testing of user-facing features
 * without requiring a display server.
 */

#ifdef COIN3D_OSMESA_BUILD

#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include <Inventor/C/glue/gl.h>
#include <memory>
#include <vector>
#include <string>

namespace CoinTestUtils {

/**
 * @brief RAII wrapper for OSMesa context management in tests
 * 
 * Provides automatic setup and cleanup of OSMesa contexts with proper
 * Coin3D integration for comprehensive rendering tests.
 */
class OSMesaTestContext {
public:
    /**
     * @brief Create OSMesa context with specified dimensions
     * @param width Framebuffer width
     * @param height Framebuffer height
     * @param format Color format (OSMESA_RGBA recommended)
     */
    OSMesaTestContext(unsigned int width = 256, unsigned int height = 256, 
                      GLenum format = OSMESA_RGBA);
    
    ~OSMesaTestContext();
    
    // Non-copyable, movable
    OSMesaTestContext(const OSMesaTestContext&) = delete;
    OSMesaTestContext& operator=(const OSMesaTestContext&) = delete;
    OSMesaTestContext(OSMesaTestContext&& other) noexcept;
    OSMesaTestContext& operator=(OSMesaTestContext&& other) noexcept;
    
    /**
     * @brief Check if context creation was successful
     */
    bool isValid() const { return context_ != nullptr; }
    
    /**
     * @brief Make this context current for rendering
     */
    bool makeCurrent();
    
    /**
     * @brief Get framebuffer dimensions
     */
    void getDimensions(unsigned int& width, unsigned int& height) const {
        width = width_; height = height_;
    }
    
    /**
     * @brief Get raw pixel data from framebuffer
     * @return RGBA pixel data, bottom-left origin
     */
    const unsigned char* getPixelData() const { return buffer_.get(); }
    
    /**
     * @brief Save framebuffer to PPM file for debugging
     */
    bool saveToPPM(const std::string& filename) const;
    
    /**
     * @brief Clear framebuffer to specified color
     */
    void clearBuffer(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);
    
    /**
     * @brief Get OpenGL context handle (for Coin3D integration)
     */
    void* getGLContext() const { return static_cast<void*>(context_); }

private:
    OSMesaContext context_;
    std::unique_ptr<unsigned char[]> buffer_;
    unsigned int width_;
    unsigned int height_;
    GLenum format_;
    
    void cleanup();
};

/**
 * @brief RAII manager for OSMesa callback registration with Coin3D
 * 
 * Automatically registers and unregisters OSMesa callbacks with Coin3D's
 * context management system for seamless integration.
 */
class OSMesaCallbackManager {
public:
    OSMesaCallbackManager();
    ~OSMesaCallbackManager();
    
    // Non-copyable, non-movable (singleton-like behavior)
    OSMesaCallbackManager(const OSMesaCallbackManager&) = delete;
    OSMesaCallbackManager& operator=(const OSMesaCallbackManager&) = delete;
    OSMesaCallbackManager(OSMesaCallbackManager&&) = delete;
    OSMesaCallbackManager& operator=(OSMesaCallbackManager&&) = delete;

private:
    static void* createOffscreen(unsigned int width, unsigned int height);
    static SbBool makeCurrent(void* context);
    static void reinstatePrevious(void* context);
    static void destruct(void* context);
};

/**
 * @brief Test fixture that provides OSMesa context for rendering tests
 * 
 * Use this as a base class or member for any test that needs to render
 * to validate visual output or OpenGL state.
 */
class OSMesaTestFixture {
public:
    OSMesaTestFixture(unsigned int width = 256, unsigned int height = 256);
    virtual ~OSMesaTestFixture() = default;
    
    /**
     * @brief Get the test context for rendering operations
     */
    OSMesaTestContext& getContext() { return context_; }
    const OSMesaTestContext& getContext() const { return context_; }
    
    /**
     * @brief Check if context is ready for rendering
     */
    bool isContextReady() const { return context_.isValid(); }

protected:
    OSMesaCallbackManager callback_manager_;
    OSMesaTestContext context_;
};

} // namespace CoinTestUtils

#endif // COIN3D_OSMESA_BUILD

#endif // OSMESA_TEST_CONTEXT_H