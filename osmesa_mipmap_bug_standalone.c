/*
 * Minimal OSMesa texture mipmap bug reproduction for OSMesa developers
 * 
 * This demonstrates a memory management bug in OSMesa's _mesa_generate_mipmap()
 * function in mipmap.c:971 where it calls free() on memory that wasn't allocated
 * with malloc().
 *
 * EXPECTED: Should upload texture and optionally generate mipmaps without crashing
 * ACTUAL: Crashes with "attempting free on address which was not malloc()-ed"
 * 
 * To reproduce:
 * 1. Compile: gcc -I/path/to/osmesa/include -L/path/to/osmesa/lib osmesa_mipmap_bug.c -losmesa -o osmesa_mipmap_bug
 * 2. Run: ./osmesa_mipmap_bug
 * 3. Observe crash in _mesa_generate_mipmap at line 971 in mipmap.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __has_include
  #if __has_include(<OSMesa/osmesa.h>)
    #include <OSMesa/osmesa.h>
    #include <OSMesa/gl.h>
    #define HAVE_OSMESA
  #endif
#endif

#ifdef HAVE_OSMESA

// Create simple test texture data
void generate_texture_data(unsigned char* data, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            data[idx] = (x % 16 < 8) ? 255 : 0;     // R - checkerboard
            data[idx + 1] = (y % 16 < 8) ? 255 : 0; // G - checkerboard  
            data[idx + 2] = 128;                     // B - constant
        }
    }
}

int test_osmesa_mipmap_bug() {
    printf("=== OSMesa Mipmap Generation Bug Test ===\n");
    
    // Create OSMesa context
    OSMesaContext ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
    if (!ctx) {
        printf("ERROR: Failed to create OSMesa context\n");
        return 1;
    }
    
    const int fb_width = 256;
    const int fb_height = 256;
    unsigned char* framebuffer = malloc(fb_width * fb_height * 4);
    
    if (!OSMesaMakeCurrent(ctx, framebuffer, GL_UNSIGNED_BYTE, fb_width, fb_height)) {
        printf("ERROR: Failed to make OSMesa context current\n");
        free(framebuffer);
        OSMesaDestroyContext(ctx);
        return 1;
    }
    
    printf("OSMesa context created successfully\n");
    
    // Clear any existing GL errors
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR) {
        printf("Clearing initial GL error: 0x%X\n", error);
    }
    
    // Create and bind texture
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    printf("Texture created and bound (ID: %u)\n", texture_id);
    
    // Create texture data - use malloc to simulate normal allocation
    const int tex_width = 64;
    const int tex_height = 64;
    unsigned char* texture_data = malloc(tex_width * tex_height * 3);
    generate_texture_data(texture_data, tex_width, tex_height);
    
    printf("Generated %dx%d texture data using malloc()\n", tex_width, tex_height);
    
    // Set texture parameters that will trigger automatic mipmap generation
    // This is the key setting that causes OSMesa's mipmap bug
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    printf("Set texture parameters with mipmap filtering\n");
    
    // This is where the crash occurs!
    // The glTexImage2D call will trigger OSMesa's automatic mipmap generation
    // which calls free() on memory that wasn't allocated by malloc()
    printf("Uploading texture data (this will crash due to OSMesa mipmap bug)...\n");
    printf("Expected crash location: _mesa_generate_mipmap() in mipmap.c:971\n");
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_width, tex_height, 0, GL_RGB, GL_UNSIGNED_BYTE, texture_data);
    
    error = glGetError();
    if (error != GL_NO_ERROR) {
        printf("ERROR: glTexImage2D failed with GL error 0x%X\n", error);
        free(texture_data);
        free(framebuffer);
        glDeleteTextures(1, &texture_id);
        OSMesaDestroyContext(ctx);
        return 1;
    }
    
    printf("SUCCESS: Texture uploaded without crash!\n");
    printf("This means the OSMesa mipmap bug has been fixed.\n");
    
    // Clean up
    free(texture_data);
    free(framebuffer);
    glDeleteTextures(1, &texture_id);
    OSMesaDestroyContext(ctx);
    
    return 0;
}

int test_osmesa_no_mipmap() {
    printf("\n=== OSMesa Texture Upload (No Mipmap) Test ===\n");
    
    OSMesaContext ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
    if (!ctx) {
        printf("ERROR: Failed to create OSMesa context\n");
        return 1;
    }
    
    const int fb_width = 256;
    const int fb_height = 256;
    unsigned char* framebuffer = malloc(fb_width * fb_height * 4);
    
    if (!OSMesaMakeCurrent(ctx, framebuffer, GL_UNSIGNED_BYTE, fb_width, fb_height)) {
        printf("ERROR: Failed to make OSMesa context current\n");
        free(framebuffer);
        OSMesaDestroyContext(ctx);
        return 1;
    }
    
    // Clear GL errors
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR) {}
    
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    // Set texture parameters WITHOUT mipmap generation
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    printf("Set texture parameters WITHOUT mipmap filtering\n");
    
    const int tex_width = 64;
    const int tex_height = 64;
    unsigned char* texture_data = malloc(tex_width * tex_height * 3);
    generate_texture_data(texture_data, tex_width, tex_height);
    
    printf("Uploading texture data without mipmap generation...\n");
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_width, tex_height, 0, GL_RGB, GL_UNSIGNED_BYTE, texture_data);
    
    error = glGetError();
    if (error != GL_NO_ERROR) {
        printf("ERROR: glTexImage2D failed with GL error 0x%X\n", error);
        free(texture_data);
        free(framebuffer);
        glDeleteTextures(1, &texture_id);
        OSMesaDestroyContext(ctx);
        return 1;
    }
    
    printf("SUCCESS: Texture uploaded without mipmap generation\n");
    
    // Clean up
    free(texture_data);
    free(framebuffer);
    glDeleteTextures(1, &texture_id);
    OSMesaDestroyContext(ctx);
    
    return 0;
}

#endif // HAVE_OSMESA

int main() {
    printf("OSMesa Texture Mipmap Bug Reproduction Test\n");
    printf("===========================================\n");
    printf("This test isolates an OSMesa memory management bug in mipmap generation.\n\n");
    
#ifdef HAVE_OSMESA
    
    printf("DIAGNOSIS:\n");
    printf("The bug is in OSMesa's _mesa_generate_mipmap() function (mipmap.c:971)\n");
    printf("which calls free() on memory that wasn't allocated with malloc().\n");
    printf("This happens when GL_TEXTURE_MIN_FILTER uses mipmap filtering.\n\n");
    
    // Test 1: Without mipmap generation (should work)
    int result1 = test_osmesa_no_mipmap();
    
    // Test 2: With mipmap generation (will crash due to bug)
    int result2 = test_osmesa_mipmap_bug();
    
    printf("\n=== RESULTS ===\n");
    if (result1 == 0 && result2 == 0) {
        printf("Both tests passed - OSMesa mipmap bug appears to be fixed!\n");
    } else if (result1 == 0 && result2 != 0) {
        printf("Confirmed: OSMesa mipmap generation bug detected\n");
        printf("- Texture upload without mipmaps: WORKS\n"); 
        printf("- Texture upload with mipmaps: FAILS/CRASHES\n");
        printf("- Fix needed in OSMesa mipmap.c:971\n");
    } else {
        printf("Unexpected test results - general texture upload issue\n");
    }
    
#else
    printf("OSMesa not available - cannot run bug reproduction test\n");
    return 1;
#endif
    
    return 0;
}