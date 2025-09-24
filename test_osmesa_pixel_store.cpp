/**
 * Quick test to verify OSMesaPixelStore behavior
 */
#include <iostream>

#ifdef __has_include
  #if __has_include(<OSMesa/osmesa.h>)
    #include <OSMesa/osmesa.h>
    #include <OSMesa/gl.h>
    #define HAVE_OSMESA
  #endif
#endif

#ifdef HAVE_OSMESA
int main() {
    // Create a simple context
    OSMesaContext ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
    if (!ctx) {
        std::cout << "Failed to create OSMesa context" << std::endl;
        return 1;
    }
    
    unsigned char buffer[64*64*4];
    if (!OSMesaMakeCurrent(ctx, buffer, GL_UNSIGNED_BYTE, 64, 64)) {
        std::cout << "Failed to make context current" << std::endl;
        return 1;
    }
    
    // Check default Y_UP setting
    GLint y_up_default;
    OSMesaGetIntegerv(OSMESA_Y_UP, &y_up_default);
    std::cout << "Default OSMESA_Y_UP: " << y_up_default << std::endl;
    
    // Set Y_UP to 0 (downward)
    OSMesaPixelStore(OSMESA_Y_UP, 0);
    
    // Check if it was set
    GLint y_up_after;
    OSMesaGetIntegerv(OSMESA_Y_UP, &y_up_after);
    std::cout << "After setting to 0, OSMESA_Y_UP: " << y_up_after << std::endl;
    
    // Set Y_UP to 1 (upward)
    OSMesaPixelStore(OSMESA_Y_UP, 1);
    
    // Check if it was set
    GLint y_up_final;
    OSMesaGetIntegerv(OSMESA_Y_UP, &y_up_final);
    std::cout << "After setting to 1, OSMESA_Y_UP: " << y_up_final << std::endl;
    
    OSMesaDestroyContext(ctx);
    return 0;
}
#else
int main() {
    std::cout << "OSMesa not available" << std::endl;
    return 1;
}
#endif