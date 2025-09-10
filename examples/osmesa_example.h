/* Example demonstrating OSMesa vs System OpenGL conditional compilation */

#ifdef COIN3D_OSMESA_BUILD
/* OSMesa-specific code */
#include <GL/osmesa.h>

// Example: Create OSMesa context for offscreen rendering
inline OSMesaContext createOffscreenContext(int width, int height) {
    return OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
}

#else
/* System OpenGL code */
#include <GL/gl.h>
#include <GL/glu.h>

// Example: Standard OpenGL context creation would go here
// This would typically use platform-specific context creation

#endif

/* Common OpenGL code that works with both backends */
inline void setupBasicRendering() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}