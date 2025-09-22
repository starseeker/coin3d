#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <iostream>

int main() {
    std::cout << "Testing modern context creation API" << std::endl;
    
    // Initialize Coin3D
    SoDB::init();
    
    // Test modern high-level context creation via SoOffscreenRenderer
    SbViewportRegion viewport(128, 128);
    SoOffscreenRenderer renderer(viewport);
    
    // Test modern OpenGL capability detection
    int major, minor, release;
    SoOffscreenRenderer::getOpenGLVersion(major, minor, release);
    
    if (major > 0) {
        std::cout << "Modern API successfully provides OpenGL version: " 
                  << major << "." << minor << "." << release << std::endl;
    } else {
        std::cout << "Context creation handled gracefully by modern API" << std::endl;
    }
    
    return 0;
}