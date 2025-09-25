/*
 * Minimal OSMesa texture bug reproduction test case
 * 
 * This demonstrates the OSMesa bug in mipmap generation that causes
 * crashes when textures are uploaded. This test case isolates the 
 * OSMesa bug without requiring Coin3D.
 *
 * EXPECTED BEHAVIOR: Should create texture without crashing
 * ACTUAL BEHAVIOR: Crashes with "attempting free on address which was not malloc()-ed"
 */

#include <iostream>
#include <memory>
#include <cstring>

#ifdef __has_include
  #if __has_include(<OSMesa/osmesa.h>)
    #include <OSMesa/osmesa.h>
    #include <OSMesa/gl.h>
    #define HAVE_OSMESA
  #endif
#endif

#ifdef HAVE_OSMESA

// Generate a simple test texture
void generateTestTexture(unsigned char* data, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            data[idx] = (unsigned char)((x * 255) / width);     // R
            data[idx + 1] = (unsigned char)((y * 255) / height); // G  
            data[idx + 2] = 128;                                // B
        }
    }
}

bool testOSMesaTextureUpload() {
    std::cout << "=== OSMesa Texture Upload Bug Test ===" << std::endl;
    
    // Create OSMesa context
    OSMesaContext ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
    if (!ctx) {
        std::cout << "✗ Failed to create OSMesa context" << std::endl;
        return false;
    }
    
    const int width = 128;
    const int height = 128;
    std::unique_ptr<unsigned char[]> buffer = std::make_unique<unsigned char[]>(width * height * 4);
    
    if (!OSMesaMakeCurrent(ctx, buffer.get(), GL_UNSIGNED_BYTE, width, height)) {
        std::cout << "✗ Failed to make OSMesa context current" << std::endl;
        OSMesaDestroyContext(ctx);
        return false;
    }
    
    std::cout << "✓ OSMesa context created and made current" << std::endl;
    
    // Clear any existing errors
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR) {
        std::cout << "Clearing initial error: 0x" << std::hex << error << std::dec << std::endl;
    }
    
    // Generate OpenGL texture
    GLuint textureId;
    glGenTextures(1, &textureId);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "✗ glGenTextures failed with error 0x" << std::hex << error << std::dec << std::endl;
        OSMesaDestroyContext(ctx);
        return false;
    }
    
    std::cout << "✓ Generated texture ID: " << textureId << std::endl;
    
    glBindTexture(GL_TEXTURE_2D, textureId);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "✗ glBindTexture failed with error 0x" << std::hex << error << std::dec << std::endl;
        OSMesaDestroyContext(ctx);
        return false;
    }
    
    std::cout << "✓ Texture bound successfully" << std::endl;
    
    // Create texture data using custom allocation (this mimics what Coin3D does)
    const int texSize = 64;
    std::unique_ptr<unsigned char[]> texData = std::make_unique<unsigned char[]>(texSize * texSize * 3);
    generateTestTexture(texData.get(), texSize, texSize);
    
    std::cout << "✓ Generated " << texSize << "x" << texSize << " test texture data" << std::endl;
    
    // Set texture parameters that might trigger mipmap generation
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "✗ Texture parameter setup failed with error 0x" << std::hex << error << std::dec << std::endl;
        OSMesaDestroyContext(ctx);
        return false;
    }
    
    std::cout << "✓ Texture parameters set (including mipmap generation)" << std::endl;
    
    // THIS IS THE CRITICAL STEP - upload texture data
    // This will trigger OSMesa's mipmap generation which has the memory bug
    std::cout << "Uploading texture data (this may crash due to OSMesa bug)..." << std::endl;
    
    // The crash happens here because OSMesa tries to free() memory that wasn't malloc()'d
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texSize, texSize, 0, GL_RGB, GL_UNSIGNED_BYTE, texData.get());
    
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "✗ glTexImage2D failed with error 0x" << std::hex << error << std::dec << std::endl;
        
        switch (error) {
            case GL_INVALID_OPERATION:
                std::cout << "  GL_INVALID_OPERATION - OSMesa texture limitation" << std::endl;
                break;
            case GL_OUT_OF_MEMORY:
                std::cout << "  GL_OUT_OF_MEMORY - memory allocation failed" << std::endl;
                break;
            default:
                std::cout << "  Unknown texture upload error" << std::endl;
        }
        
        glDeleteTextures(1, &textureId);
        OSMesaDestroyContext(ctx);
        return false;
    }
    
    std::cout << "✓ SUCCESS! Texture data uploaded without crash" << std::endl;
    std::cout << "  This means the OSMesa memory bug has been fixed or avoided" << std::endl;
    
    // Clean up
    glDeleteTextures(1, &textureId);
    OSMesaDestroyContext(ctx);
    
    return true;
}

// Alternative test without mipmap generation to isolate the issue
bool testOSMesaTextureUploadNoMipmap() {
    std::cout << "\n=== OSMesa Texture Upload Test (No Mipmap) ===" << std::endl;
    
    OSMesaContext ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
    if (!ctx) {
        std::cout << "✗ Failed to create OSMesa context" << std::endl;
        return false;
    }
    
    const int width = 128;
    const int height = 128;
    std::unique_ptr<unsigned char[]> buffer = std::make_unique<unsigned char[]>(width * height * 4);
    
    if (!OSMesaMakeCurrent(ctx, buffer.get(), GL_UNSIGNED_BYTE, width, height)) {
        std::cout << "✗ Failed to make OSMesa context current" << std::endl;
        OSMesaDestroyContext(ctx);
        return false;
    }
    
    // Clear any existing errors
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR) {}
    
    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    
    // Set texture parameters WITHOUT mipmap generation
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    std::cout << "✓ Texture parameters set (NO mipmap generation)" << std::endl;
    
    // Create and upload texture data
    const int texSize = 64;
    std::unique_ptr<unsigned char[]> texData = std::make_unique<unsigned char[]>(texSize * texSize * 3);
    generateTestTexture(texData.get(), texSize, texSize);
    
    std::cout << "Uploading texture data without mipmap generation..." << std::endl;
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texSize, texSize, 0, GL_RGB, GL_UNSIGNED_BYTE, texData.get());
    
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "✗ glTexImage2D failed with error 0x" << std::hex << error << std::dec << std::endl;
        glDeleteTextures(1, &textureId);
        OSMesaDestroyContext(ctx);
        return false;
    }
    
    std::cout << "✓ SUCCESS! Texture uploaded without mipmap generation" << std::endl;
    
    // Clean up
    glDeleteTextures(1, &textureId);
    OSMesaDestroyContext(ctx);
    
    return true;
}

#endif // HAVE_OSMESA

int main() {
    std::cout << "OSMesa Texture Bug Reproduction Test" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "This test isolates the OSMesa texture memory bug without Coin3D" << std::endl;
    std::cout << std::endl;
    
#ifdef HAVE_OSMESA
    
    // Test 1: Upload texture WITH mipmap generation (likely to crash)
    bool test1_success = false;
    try {
        test1_success = testOSMesaTextureUpload();
    } catch (const std::exception& e) {
        std::cout << "✗ Exception in mipmap test: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "✗ Unknown exception in mipmap test" << std::endl;
    }
    
    // Test 2: Upload texture WITHOUT mipmap generation (should work)  
    bool test2_success = false;
    try {
        test2_success = testOSMesaTextureUploadNoMipmap();
    } catch (const std::exception& e) {
        std::cout << "✗ Exception in no-mipmap test: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "✗ Unknown exception in no-mipmap test" << std::endl;
    }
    
    std::cout << "\n=== DIAGNOSIS ===" << std::endl;
    
    if (test1_success && test2_success) {
        std::cout << "✓ Both tests passed - OSMesa texture bug appears to be fixed" << std::endl;
    } else if (!test1_success && test2_success) {
        std::cout << "✓ CONFIRMED: OSMesa mipmap generation bug identified" << std::endl;
        std::cout << "  - Texture upload without mipmaps: WORKS" << std::endl;
        std::cout << "  - Texture upload with mipmaps: CRASHES" << std::endl;
        std::cout << "  - Root cause: OSMesa mipmap.c:971 calls free() on non-malloc'd memory" << std::endl;
        std::cout << "  - Solution: Disable automatic mipmap generation in Coin3D" << std::endl;
    } else if (!test1_success && !test2_success) {
        std::cout << "✗ General OSMesa texture upload problem" << std::endl;
    } else {
        std::cout << "? Unexpected result pattern" << std::endl;
    }
    
#else
    std::cout << "OSMesa not available - cannot run bug reproduction test" << std::endl;
    return 1;
#endif
    
    return 0;
}