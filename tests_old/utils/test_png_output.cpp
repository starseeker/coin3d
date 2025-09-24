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

#include <catch2/catch_test_macros.hpp>
#include "utils/png_test_utils.h"
#include "utils/scene_graph_test_utils.h"
#include "utils/test_common.h"

#ifdef COIN3D_OSMESA_BUILD

#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <fstream>

using namespace CoinTestUtils;

TEST_CASE("PNG Test Output", "[png][utility][rendering]") {
    CoinTestFixture coinInit;
    
    SECTION("Basic PNG utility function test") {
        // Create a simple test pattern
        const int width = 64;
        const int height = 64;
        unsigned char* test_rgba = new unsigned char[width * height * 4];
        
        // Generate a simple test pattern (red-green gradient)
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int idx = (y * width + x) * 4;
                test_rgba[idx] = (unsigned char)(255 * x / width);     // R
                test_rgba[idx + 1] = (unsigned char)(255 * y / height); // G
                test_rgba[idx + 2] = 128; // B
                test_rgba[idx + 3] = 255; // A
            }
        }
        
        // Test PNG writing
        bool result = writePNG("/tmp/test_pattern.png", test_rgba, width, height, false);
        CHECK(result == true);
        
        // Verify file was created
        std::ifstream file("/tmp/test_pattern.png", std::ios::binary);
        CHECK(file.good());
        if (file.good()) {
            // Check PNG signature
            unsigned char png_sig[8];
            file.read((char*)png_sig, 8);
            CHECK(png_sig[0] == 0x89);
            CHECK(png_sig[1] == 0x50); // 'P'
            CHECK(png_sig[2] == 0x4E); // 'N'
            CHECK(png_sig[3] == 0x47); // 'G'
        }
        
        delete[] test_rgba;
    }
    
    SECTION("PNG output with scene rendering") {
        RenderingTestUtils::RenderTestFixture fixture(128, 128);
        
        if (fixture.isContextReady()) {
            // Create a simple test scene
            auto scene = StandardTestScenes::createBasicGeometryScene();
            REQUIRE(scene != nullptr);
            
            // Render the scene
            bool renderSuccess = fixture.renderScene(scene);
            CHECK(renderSuccess);
            
            // Save both PPM and PNG for comparison
            bool ppmResult = fixture.saveRenderResult("/tmp/test_scene_render.ppm");
            bool pngResult = fixture.saveRenderResultPNG("/tmp/test_scene_render.png");
            
            CHECK(ppmResult);
            CHECK(pngResult);
            
            // Verify both files exist
            std::ifstream ppmFile("/tmp/test_scene_render.ppm");
            std::ifstream pngFile("/tmp/test_scene_render.png");
            CHECK(ppmFile.good());
            CHECK(pngFile.good());
            
            scene->unref();
        } else {
            SKIP("OSMesa context not available");
        }
    }
    
    SECTION("RGB vs RGBA PNG output") {
        const int size = 32;
        unsigned char* rgb_data = new unsigned char[size * size * 3];
        unsigned char* rgba_data = new unsigned char[size * size * 4];
        
        // Create simple RGB and RGBA test patterns
        for (int i = 0; i < size * size; i++) {
            rgb_data[i * 3] = (i % 256);     // R
            rgb_data[i * 3 + 1] = ((i * 2) % 256); // G
            rgb_data[i * 3 + 2] = ((i * 3) % 256); // B
            
            rgba_data[i * 4] = rgb_data[i * 3];     // R
            rgba_data[i * 4 + 1] = rgb_data[i * 3 + 1]; // G
            rgba_data[i * 4 + 2] = rgb_data[i * 3 + 2]; // B
            rgba_data[i * 4 + 3] = 255; // A
        }
        
        bool rgb_result = writePNG_RGB("/tmp/test_rgb.png", rgb_data, size, size, false);
        bool rgba_result = writePNG("/tmp/test_rgba.png", rgba_data, size, size, false);
        
        CHECK(rgb_result);
        CHECK(rgba_result);
        
        delete[] rgb_data;
        delete[] rgba_data;
    }
}

#endif // COIN3D_OSMESA_BUILD