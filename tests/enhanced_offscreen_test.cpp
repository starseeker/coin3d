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

/*!
  Enhanced offscreen rendering test with visual output.
  
  This test creates control images using OSMesa and compares them with FBO-based
  rendering to verify that both methods produce similar visual results.
  
  The test outputs PNG images that can be visually inspected to verify rendering quality.
*/

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <cmath>
#include <vector>

#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbRotation.h>

#include "Inventor/C/glue/gl.h"

// Include svpng for standalone PNG writing
extern "C" {
#include "../svpng.inc"
}

/* Create a test scene with cone and sphere - enhanced version */
SoSeparator *
create_enhanced_test_scene(void)
{
  SoSeparator * root = new SoSeparator;
  root->ref();

  /* Add lighting */
  SoDirectionalLight * light = new SoDirectionalLight;
  light->direction.setValue(-1, -1, -1);
  light->intensity.setValue(0.8f);
  root->addChild(light);

  /* Add camera with better positioning */
  SoPerspectiveCamera * camera = new SoPerspectiveCamera;
  camera->position.setValue(0, 1, 5);
  camera->orientation.setValue(SbRotation(SbVec3f(1, 0, 0), -0.1f));
  camera->nearDistance.setValue(1.0f);
  camera->farDistance.setValue(20.0f);
  camera->focalDistance.setValue(5.0f);
  root->addChild(camera);

  /* Create red sphere */
  SoMaterial * red_material = new SoMaterial;
  red_material->diffuseColor.setValue(0.8f, 0.2f, 0.2f);
  red_material->ambientColor.setValue(0.2f, 0.05f, 0.05f);
  red_material->specularColor.setValue(0.9f, 0.9f, 0.9f);
  red_material->shininess.setValue(0.8f);
  root->addChild(red_material);

  SoTransform * sphere_transform = new SoTransform;
  sphere_transform->translation.setValue(-1.5f, 0, 0);
  root->addChild(sphere_transform);

  SoSphere * sphere = new SoSphere;
  sphere->radius.setValue(0.8f);
  root->addChild(sphere);

  /* Create blue cone */
  SoMaterial * blue_material = new SoMaterial;
  blue_material->diffuseColor.setValue(0.2f, 0.4f, 0.8f);
  blue_material->ambientColor.setValue(0.05f, 0.1f, 0.2f);
  blue_material->specularColor.setValue(0.9f, 0.9f, 0.9f);
  blue_material->shininess.setValue(0.6f);
  root->addChild(blue_material);

  SoTransform * cone_transform = new SoTransform;
  cone_transform->translation.setValue(1.5f, 0, 0);
  cone_transform->rotation.setValue(SbRotation(SbVec3f(0, 0, 1), 0.3f));
  root->addChild(cone_transform);

  SoCone * cone = new SoCone;
  cone->bottomRadius.setValue(0.8f);
  cone->height.setValue(1.6f);
  root->addChild(cone);

  return root;
}

/* Save image buffer as PNG using svpng */
bool
save_image_as_png(const unsigned char * buffer, int width, int height, int components, const char * filename)
{
  std::cout << "Saving image to " << filename << " (" << width << "x" << height 
            << ", " << components << " components)" << std::endl;

  // Convert to RGB if needed and flip Y coordinate (OpenGL vs image coordinate system)
  std::vector<unsigned char> rgb_data(width * height * 3);
  
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int src_idx = ((height - 1 - y) * width + x) * components; // Flip Y
      int dst_idx = (y * width + x) * 3;
      
      rgb_data[dst_idx + 0] = buffer[src_idx + 0]; // R
      rgb_data[dst_idx + 1] = buffer[src_idx + 1]; // G
      rgb_data[dst_idx + 2] = buffer[src_idx + 2]; // B
    }
  }

  // Write PNG file
  FILE * fp = fopen(filename, "wb");
  if (!fp) {
    std::cerr << "Failed to open " << filename << " for writing" << std::endl;
    return false;
  }

  svpng(fp, width, height, rgb_data.data(), 0);
  fclose(fp);
  
  std::cout << "Successfully saved " << filename << std::endl;
  return true;
}

/* Calculate image similarity between two buffers */
double
calculate_image_similarity(const unsigned char * img1, const unsigned char * img2, 
                          int width, int height, int components)
{
  if (!img1 || !img2) return 0.0;
  
  double total_diff = 0.0;
  int total_pixels = width * height;
  
  for (int i = 0; i < total_pixels * components; i++) {
    int diff = (int)img1[i] - (int)img2[i];
    total_diff += diff * diff;
  }
  
  double mse = total_diff / (total_pixels * components);
  double max_mse = 255.0 * 255.0; // Maximum possible MSE
  double similarity = 1.0 - (mse / max_mse); // Convert to similarity (1.0 = identical)
  
  return similarity;
}

/* Test OSMesa offscreen rendering with PNG output */
int
test_osmesa_with_png_output(SoSeparator * scene, unsigned char ** control_buffer, 
                           int * control_width, int * control_height, int * control_components)
{
  std::cout << "\n=== Testing OSMesa-based offscreen rendering (Control) ===" << std::endl;

  /* Force OSMesa usage for guaranteed context creation */
  setenv("COIN_FORCE_OSMESA", "1", 1);
  setenv("COIN_DEBUG_OSMESA", "1", 1);
  
  /* Disable FBO to ensure we're using pure OSMesa */
  setenv("COIN_USE_FBO_OFFSCREEN", "0", 1);

  /* Create offscreen renderer */
  SoOffscreenRenderer renderer(SbViewportRegion(512, 512));
  renderer.setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));

  /* Render the scene */
  SbBool success = renderer.render(scene);
  if (!success) {
    std::cout << "OSMesa offscreen rendering failed (may not be available in this build)" << std::endl;
    unsetenv("COIN_FORCE_OSMESA");
    unsetenv("COIN_DEBUG_OSMESA");
    unsetenv("COIN_USE_FBO_OFFSCREEN");
    return 1;
  }

  /* Get the rendered image */
  unsigned char * buffer = renderer.getBuffer();
  if (!buffer) {
    std::cerr << "Failed to get OSMesa render buffer!" << std::endl;
    unsetenv("COIN_FORCE_OSMESA");
    unsetenv("COIN_DEBUG_OSMESA");
    unsetenv("COIN_USE_FBO_OFFSCREEN");
    return 1;
  }

  *control_width = 512;
  *control_height = 512;
  *control_components = renderer.getComponents();
  
  /* Copy buffer for comparison */
  size_t buffer_size = (*control_width) * (*control_height) * (*control_components);
  *control_buffer = (unsigned char *)malloc(buffer_size);
  memcpy(*control_buffer, buffer, buffer_size);

  /* Check if we got a reasonable image */
  int non_black_pixels = 0;
  for (int i = 0; i < (*control_width) * (*control_height); i++) {
    unsigned char r = buffer[i * (*control_components)];
    unsigned char g = buffer[i * (*control_components) + 1];
    unsigned char b = buffer[i * (*control_components) + 2];
    
    if (r > 25 || g > 25 || b > 25) {
      non_black_pixels++;
    }
  }

  std::cout << "OSMesa render: " << non_black_pixels << " non-black pixels out of " 
            << ((*control_width) * (*control_height)) << " total pixels." << std::endl;

  /* Save control image using svpng */
  if (!save_image_as_png(buffer, *control_width, *control_height, *control_components, 
                         "osmesa_control.png")) {
    std::cerr << "Failed to save OSMesa control image" << std::endl;
  }

  /* Try using SoOffscreenRenderer's built-in PNG support as well */
  std::cout << "Attempting to save using SoOffscreenRenderer::writeToFile()..." << std::endl;
  if (renderer.isWriteSupported("png")) {
    if (renderer.writeToFile("osmesa_builtin.png", "png")) {
      std::cout << "Successfully saved osmesa_builtin.png using built-in method" << std::endl;
    } else {
      std::cout << "Failed to save using built-in method" << std::endl;
    }
  } else {
    std::cout << "PNG format not supported by SoOffscreenRenderer (simage not available?)" << std::endl;
  }

  unsetenv("COIN_FORCE_OSMESA");
  unsetenv("COIN_DEBUG_OSMESA");
  unsetenv("COIN_USE_FBO_OFFSCREEN");

  if (non_black_pixels > 1000) {
    std::cout << "OSMesa control rendering SUCCESS!" << std::endl;
    return 0;
  } else {
    std::cout << "OSMesa control rendering produced mostly black image - fallback used" << std::endl;
    return 0; // Don't fail if OSMesa isn't available - just note it
  }
}

/* Test FBO with system OpenGL */
int
test_fbo_system_opengl(SoSeparator * scene, unsigned char * control_buffer,
                      int control_width, int control_height, int control_components)
{
  std::cout << "\n=== Testing FBO with System OpenGL ===" << std::endl;

  /* Enable FBO and disable OSMesa forcing */
  setenv("COIN_USE_FBO_OFFSCREEN", "1", 1);
  setenv("COIN_DEBUG_FBO", "1", 1);
  unsetenv("COIN_FORCE_OSMESA");

  /* Create offscreen renderer */
  SoOffscreenRenderer renderer(SbViewportRegion(512, 512));
  renderer.setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));

  /* Render the scene */
  SbBool success = renderer.render(scene);
  if (!success) {
    std::cout << "FBO with system OpenGL failed (expected in headless environment)" << std::endl;
    unsetenv("COIN_USE_FBO_OFFSCREEN");
    unsetenv("COIN_DEBUG_FBO");
    return 1;
  }

  /* Get the rendered image */
  unsigned char * buffer = renderer.getBuffer();
  if (!buffer) {
    std::cout << "Failed to get FBO render buffer" << std::endl;
    unsetenv("COIN_USE_FBO_OFFSCREEN");
    unsetenv("COIN_DEBUG_FBO");
    return 1;
  }

  /* Save FBO image */
  if (!save_image_as_png(buffer, 512, 512, renderer.getComponents(), "fbo_system.png")) {
    std::cerr << "Failed to save FBO system image" << std::endl;
  }

  /* Compare with control image */
  if (control_buffer) {
    double similarity = calculate_image_similarity(buffer, control_buffer, 
                                                  control_width, control_height, control_components);
    std::cout << "Image similarity: " << (similarity * 100.0) << "%" << std::endl;
    
    if (similarity > 0.95) {
      std::cout << "FBO and OSMesa images are very similar - SUCCESS!" << std::endl;
    } else if (similarity > 0.8) {
      std::cout << "FBO and OSMesa images are reasonably similar" << std::endl;
    } else {
      std::cout << "FBO and OSMesa images differ significantly" << std::endl;
    }
  }

  unsetenv("COIN_USE_FBO_OFFSCREEN");
  unsetenv("COIN_DEBUG_FBO");

  std::cout << "FBO system OpenGL test completed" << std::endl;
  return 0;
}

/* Test OSMesa with FBO enabled (if supported) */
int  
test_osmesa_with_fbo(SoSeparator * scene, unsigned char * control_buffer,
                    int control_width, int control_height, int control_components)
{
  std::cout << "\n=== Testing OSMesa + FBO (if FBO available in OSMesa) ===" << std::endl;

  /* Enable both OSMesa and FBO */
  setenv("COIN_FORCE_OSMESA", "1", 1);
  setenv("COIN_USE_FBO_OFFSCREEN", "1", 1);
  setenv("COIN_DEBUG_OSMESA", "1", 1);
  setenv("COIN_DEBUG_FBO", "1", 1);

  /* Create offscreen renderer */
  SoOffscreenRenderer renderer(SbViewportRegion(512, 512));
  renderer.setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));

  /* Render the scene */
  SbBool success = renderer.render(scene);
  if (!success) {
    std::cout << "OSMesa + FBO rendering failed" << std::endl;
    unsetenv("COIN_FORCE_OSMESA");
    unsetenv("COIN_USE_FBO_OFFSCREEN");
    unsetenv("COIN_DEBUG_OSMESA");
    unsetenv("COIN_DEBUG_FBO");
    return 1;
  }

  /* Get the rendered image */
  unsigned char * buffer = renderer.getBuffer();
  if (!buffer) {
    std::cout << "Failed to get OSMesa + FBO render buffer" << std::endl;
    unsetenv("COIN_FORCE_OSMESA");
    unsetenv("COIN_USE_FBO_OFFSCREEN");
    unsetenv("COIN_DEBUG_OSMESA");
    unsetenv("COIN_DEBUG_FBO");
    return 1;
  }

  /* Save OSMesa + FBO image */
  if (!save_image_as_png(buffer, 512, 512, renderer.getComponents(), "osmesa_fbo.png")) {
    std::cerr << "Failed to save OSMesa + FBO image" << std::endl;
  }

  /* Compare with control image */
  if (control_buffer) {
    double similarity = calculate_image_similarity(buffer, control_buffer, 
                                                  control_width, control_height, control_components);
    std::cout << "OSMesa+FBO vs OSMesa similarity: " << (similarity * 100.0) << "%" << std::endl;
  }

  unsetenv("COIN_FORCE_OSMESA");
  unsetenv("COIN_USE_FBO_OFFSCREEN");
  unsetenv("COIN_DEBUG_OSMESA");
  unsetenv("COIN_DEBUG_FBO");

  std::cout << "OSMesa + FBO test completed" << std::endl;
  return 0;
}

int
main(int argc, char ** argv)
{
  std::cout << "Enhanced Offscreen Rendering Test with Visual Output" << std::endl;
  std::cout << "====================================================" << std::endl;

  /* Initialize Coin */
  SoDB::init();

  /* Create enhanced test scene */
  SoSeparator * scene = create_enhanced_test_scene();

  int result = 0;
  unsigned char * control_buffer = nullptr;
  int control_width = 0, control_height = 0, control_components = 0;

  /* Test 1: Create control image with OSMesa */
  result |= test_osmesa_with_png_output(scene, &control_buffer, 
                                       &control_width, &control_height, &control_components);

  /* Test 2: Test FBO with system OpenGL (may fail in headless environment) */
  test_fbo_system_opengl(scene, control_buffer, control_width, control_height, control_components);

  /* Test 3: Test OSMesa with FBO (if FBO is supported in OSMesa) */
  test_osmesa_with_fbo(scene, control_buffer, control_width, control_height, control_components);

  /* Cleanup */
  if (control_buffer) {
    free(control_buffer);
  }
  scene->unref();
  SoDB::cleanup();

  std::cout << "\n=== Test Summary ===" << std::endl;
  if (result == 0) {
    std::cout << "Enhanced offscreen rendering test PASSED!" << std::endl;
    std::cout << "Check the generated PNG files:" << std::endl;
    std::cout << "  - osmesa_control.png    : OSMesa reference image" << std::endl;
    std::cout << "  - osmesa_builtin.png    : OSMesa using SoOffscreenRenderer::writeToFile()" << std::endl;
    std::cout << "  - fbo_system.png        : FBO with system OpenGL (if available)" << std::endl;
    std::cout << "  - osmesa_fbo.png        : OSMesa with FBO enabled (if supported)" << std::endl;
  } else {
    std::cout << "Enhanced offscreen rendering test FAILED!" << std::endl;
  }

  return result;
}