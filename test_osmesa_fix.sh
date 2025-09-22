#!/bin/bash

# Test the core OSMesa extension loading fix by compiling and running a minimal test

echo "=== Testing OSMesa Extension Loading Fix ==="

cd /home/runner/work/coin3d/coin3d

# Create a minimal test program that doesn't depend on the full Coin3D build
cat > /tmp/test_osmesa_extensions.cpp << 'EOF'
#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include <iostream>
#include <cstring>

int main() {
    std::cout << "Creating OSMesa context..." << std::endl;
    
    // Create context following OSMesa glew example pattern
    OSMesaContext ctx = OSMesaCreateContext(OSMESA_RGBA, NULL);
    if (!ctx) {
        std::cerr << "Failed to create OSMesa context" << std::endl;
        return 1;
    }
    
    // Allocate buffer
    int width = 256, height = 256;
    unsigned char* buffer = new unsigned char[width * height * 4];
    
    // Make context current
    if (!OSMesaMakeCurrent(ctx, buffer, GL_UNSIGNED_BYTE, width, height)) {
        std::cerr << "Failed to make OSMesa context current" << std::endl;
        OSMesaDestroyContext(ctx);
        delete[] buffer;
        return 1;
    }
    
    std::cout << "OSMesa context created and made current" << std::endl;
    
    // Test what the OSMesa glew example shows - extension availability
    std::cout << "OpenGL Info:" << std::endl;
    std::cout << "  Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "  Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "  Renderer: " << glGetString(GL_RENDERER) << std::endl;
    
    // Test extension detection
    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (extensions) {
        std::cout << "Extensions string available (length: " << strlen(extensions) << ")" << std::endl;
        
        if (strstr(extensions, "GL_EXT_framebuffer_object")) {
            std::cout << "✓ GL_EXT_framebuffer_object found in extension string" << std::endl;
            
            // Test function loading like glew example
            void* genFBO = OSMesaGetProcAddress("glGenFramebuffersEXT");
            void* bindFBO = OSMesaGetProcAddress("glBindFramebufferEXT");
            void* checkFBO = OSMesaGetProcAddress("glCheckFramebufferStatusEXT");
            void* deleteFBO = OSMesaGetProcAddress("glDeleteFramebuffersEXT");
            
            std::cout << "FBO Function Pointers via OSMesaGetProcAddress:" << std::endl;
            std::cout << "  glGenFramebuffersEXT: " << genFBO << std::endl;
            std::cout << "  glBindFramebufferEXT: " << bindFBO << std::endl;
            std::cout << "  glCheckFramebufferStatusEXT: " << checkFBO << std::endl;
            std::cout << "  glDeleteFramebuffersEXT: " << deleteFBO << std::endl;
            
            if (genFBO && bindFBO && checkFBO && deleteFBO) {
                std::cout << "✓ All core FBO functions available" << std::endl;
                
                // Test actually calling the function like glew does
                typedef void (*PFNGLGENFRAMEBUFFERSEXTPROC)(GLsizei n, GLuint *framebuffers);
                PFNGLGENFRAMEBUFFERSEXTPROC genFramebuffers = (PFNGLGENFRAMEBUFFERSEXTPROC)genFBO;
                
                GLuint fbo;
                genFramebuffers(1, &fbo);
                GLenum error = glGetError();
                if (error == GL_NO_ERROR && fbo != 0) {
                    std::cout << "✓ Successfully created FBO with ID: " << fbo << std::endl;
                    
                    // Clean up
                    typedef void (*PFNGLDELETEFRAMEBUFFERSEXTPROC)(GLsizei n, const GLuint *framebuffers);
                    PFNGLDELETEFRAMEBUFFERSEXTPROC deleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSEXTPROC)deleteFBO;
                    deleteFramebuffers(1, &fbo);
                    
                    std::cout << "✓ FBO creation and deletion test successful" << std::endl;
                } else {
                    std::cout << "✗ FBO creation failed, error: 0x" << std::hex << error << std::endl;
                }
            } else {
                std::cout << "✗ Some FBO functions not available" << std::endl;
            }
        } else {
            std::cout << "✗ GL_EXT_framebuffer_object NOT found in extension string" << std::endl;
        }
    } else {
        std::cout << "✗ Extension string is NULL" << std::endl;
    }
    
    // Cleanup
    OSMesaDestroyContext(ctx);
    delete[] buffer;
    
    std::cout << "Test completed successfully" << std::endl;
    return 0;
}
EOF

echo "Compiling test program..."
g++ -I external/osmesa/include -L build_osmesa/osmesa_build -losmesa /tmp/test_osmesa_extensions.cpp -o /tmp/test_osmesa_extensions 2>&1

if [ $? -eq 0 ]; then
    echo "Running test..."
    LD_LIBRARY_PATH=build_osmesa/osmesa_build /tmp/test_osmesa_extensions
else
    echo "Compilation failed"
fi

# Cleanup
rm -f /tmp/test_osmesa_extensions.cpp /tmp/test_osmesa_extensions

echo "=== Test Complete ==="