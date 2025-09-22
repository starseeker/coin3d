#!/bin/bash

# Investigate OSMesa FBO function availability issue
echo "=== Investigating OSMesa FBO Function Availability ==="

cd /home/runner/work/coin3d/coin3d

# Create test program to check available functions
cat > /tmp/debug_osmesa_functions.cpp << 'EOF'
#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include <iostream>
#include <cstring>

int main() {
    // Create context
    OSMesaContext ctx = OSMesaCreateContext(OSMESA_RGBA, NULL);
    if (!ctx) {
        std::cerr << "Failed to create context" << std::endl;
        return 1;
    }
    
    // Make current
    unsigned char buffer[256 * 256 * 4];
    if (!OSMesaMakeCurrent(ctx, buffer, GL_UNSIGNED_BYTE, 256, 256)) {
        std::cerr << "Failed to make current" << std::endl;
        return 1;
    }
    
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    
    // Get full extension string
    const char* ext = (const char*)glGetString(GL_EXTENSIONS);
    if (ext) {
        std::cout << "Extensions string length: " << strlen(ext) << std::endl;
        
        // Check for FBO extension specifically
        if (strstr(ext, "GL_EXT_framebuffer_object")) {
            std::cout << "✓ GL_EXT_framebuffer_object found in extensions" << std::endl;
        } else {
            std::cout << "✗ GL_EXT_framebuffer_object NOT found" << std::endl;
            // Print first part of extensions to debug
            std::cout << "First 200 chars: " << std::string(ext, 0, 200) << std::endl;
        }
    }
    
    // Test direct function calls that should be available
    std::cout << "\nTesting direct function availability:" << std::endl;
    
    // Test standard function availability (these should work)
    OSMESAproc glGetString_ptr = OSMesaGetProcAddress("glGetString");
    OSMESAproc glGetError_ptr = OSMesaGetProcAddress("glGetError"); 
    std::cout << "glGetString: " << (void*)glGetString_ptr << std::endl;
    std::cout << "glGetError: " << (void*)glGetError_ptr << std::endl;
    
    // Test FBO functions
    OSMESAproc genFBO = OSMesaGetProcAddress("glGenFramebuffersEXT");
    OSMESAproc bindFBO = OSMesaGetProcAddress("glBindFramebufferEXT");
    OSMESAproc checkFBO = OSMesaGetProcAddress("glCheckFramebufferStatusEXT");
    
    std::cout << "glGenFramebuffersEXT: " << (void*)genFBO << std::endl;
    std::cout << "glBindFramebufferEXT: " << (void*)bindFBO << std::endl;
    std::cout << "glCheckFramebufferStatusEXT: " << (void*)checkFBO << std::endl;
    
    // Also test if the functions are directly linked (might be available without GetProcAddress)
    std::cout << "\nTesting if functions are directly available..." << std::endl;
    // This would require knowing if they're actually exported by the library
    
    OSMesaDestroyContext(ctx);
    return 0;
}
EOF

echo "Compiling debug program..."
g++ -I external/osmesa/include -L build_osmesa/osmesa_build/src -losmesa /tmp/debug_osmesa_functions.cpp -o /tmp/debug_osmesa 2>&1

if [ $? -eq 0 ]; then
    echo "Running investigation..."
    LD_LIBRARY_PATH=build_osmesa/osmesa_build/src /tmp/debug_osmesa
else
    echo "Compilation failed"
fi

rm -f /tmp/debug_osmesa_functions.cpp /tmp/debug_osmesa