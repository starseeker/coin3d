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
  Example demonstrating FBO-based offscreen rendering with a non-trivial scene.
  This renders a cone and sphere using both traditional platform-specific
  offscreen rendering and FBO-based rendering for comparison.
*/

#include <iostream>
#include <cstdlib>
#include <cstring>

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

/* Create a test scene with cone and sphere */
SoSeparator *
create_test_scene(void)
{
  SoSeparator * root = new SoSeparator;
  root->ref();

  /* Add lighting */
  SoDirectionalLight * light = new SoDirectionalLight;
  light->direction.setValue(-1, -1, -1);
  root->addChild(light);

  /* Add camera */
  SoPerspectiveCamera * camera = new SoPerspectiveCamera;
  camera->position.setValue(0, 0, 5);
  camera->orientation.setValue(SbRotation(SbVec3f(1, 0, 0), -0.2f));
  root->addChild(camera);

  /* Create red sphere */
  SoMaterial * red_material = new SoMaterial;
  red_material->diffuseColor.setValue(0.8f, 0.2f, 0.2f);
  root->addChild(red_material);

  SoTransform * sphere_transform = new SoTransform;
  sphere_transform->translation.setValue(-1.5f, 0, 0);
  root->addChild(sphere_transform);

  SoSphere * sphere = new SoSphere;
  sphere->radius.setValue(0.8f);
  root->addChild(sphere);

  /* Create blue cone */
  SoMaterial * blue_material = new SoMaterial;
  blue_material->diffuseColor.setValue(0.2f, 0.2f, 0.8f);
  root->addChild(blue_material);

  SoTransform * cone_transform = new SoTransform;
  cone_transform->translation.setValue(1.5f, 0, 0);
  cone_transform->rotation.setValue(SbRotation(SbVec3f(0, 1, 0), 0.5f));
  root->addChild(cone_transform);

  SoCone * cone = new SoCone;
  cone->bottomRadius.setValue(0.8f);
  cone->height.setValue(1.6f);
  root->addChild(cone);

  return root;
}

/* Test FBO offscreen rendering */
int
test_fbo_offscreen_rendering(SoSeparator * scene)
{
  std::cout << "Testing FBO-based offscreen rendering..." << std::endl;

  /* Set environment variable to force FBO usage */
  setenv("COIN_USE_FBO_OFFSCREEN", "1", 1);
  setenv("COIN_DEBUG_FBO", "1", 1);

  /* Create offscreen renderer */
  SoOffscreenRenderer renderer(SbViewportRegion(512, 512));
  renderer.setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));

  /* Render the scene */
  SbBool success = renderer.render(scene);
  if (!success) {
    std::cerr << "FBO offscreen rendering failed!" << std::endl;
    return 1;
  }

  /* Get the rendered image */
  unsigned char * buffer = renderer.getBuffer();
  if (!buffer) {
    std::cerr << "Failed to get FBO render buffer!" << std::endl;
    return 1;
  }

  /* Check if we got a reasonable image (non-black pixels) */
  int width = 512;
  int height = 512;
  int components = renderer.getComponents();
  
  int non_black_pixels = 0;
  for (int i = 0; i < width * height; i++) {
    unsigned char r = buffer[i * components];
    unsigned char g = buffer[i * components + 1];
    unsigned char b = buffer[i * components + 2];
    
    if (r > 10 || g > 10 || b > 10) {
      non_black_pixels++;
    }
  }

  std::cout << "FBO render complete: " << non_black_pixels << " non-black pixels out of " 
            << (width * height) << " total pixels." << std::endl;

  if (non_black_pixels > 1000) {
    std::cout << "FBO offscreen rendering SUCCESS!" << std::endl;
    return 0;
  } else {
    std::cerr << "FBO offscreen rendering produced mostly black image - may have failed." << std::endl;
    return 1;
  }
}

/* Test traditional platform-specific offscreen rendering */
int
test_traditional_offscreen_rendering(SoSeparator * scene)
{
  std::cout << "Testing traditional platform-specific offscreen rendering..." << std::endl;

  /* Disable FBO to force traditional rendering */
  setenv("COIN_USE_FBO_OFFSCREEN", "0", 1);
  unsetenv("COIN_DEBUG_FBO");

  /* Create offscreen renderer */
  SoOffscreenRenderer renderer(SbViewportRegion(512, 512));
  renderer.setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));

  /* Render the scene */
  SbBool success = renderer.render(scene);
  if (!success) {
    std::cerr << "Traditional offscreen rendering failed!" << std::endl;
    return 1;
  }

  /* Get the rendered image */
  unsigned char * buffer = renderer.getBuffer();
  if (!buffer) {
    std::cerr << "Failed to get traditional render buffer!" << std::endl;
    return 1;
  }

  /* Check if we got a reasonable image (non-black pixels) */
  int width = 512;
  int height = 512;
  int components = renderer.getComponents();
  
  int non_black_pixels = 0;
  for (int i = 0; i < width * height; i++) {
    unsigned char r = buffer[i * components];
    unsigned char g = buffer[i * components + 1];
    unsigned char b = buffer[i * components + 2];
    
    if (r > 10 || g > 10 || b > 10) {
      non_black_pixels++;
    }
  }

  std::cout << "Traditional render complete: " << non_black_pixels << " non-black pixels out of " 
            << (width * height) << " total pixels." << std::endl;

  if (non_black_pixels > 1000) {
    std::cout << "Traditional offscreen rendering SUCCESS!" << std::endl;
    return 0;
  } else {
    std::cerr << "Traditional offscreen rendering produced mostly black image - may have failed." << std::endl;
    return 1;
  }
}

int
main(int argc, char ** argv)
{
  /* Initialize Coin */
  SoDB::init();

  /* Create test scene */
  SoSeparator * scene = create_test_scene();

  int result = 0;

  /* Test both rendering methods */
  result |= test_traditional_offscreen_rendering(scene);
  result |= test_fbo_offscreen_rendering(scene);

  /* Cleanup */
  scene->unref();
  SoDB::cleanup();

  if (result == 0) {
    std::cout << "\nAll offscreen rendering tests PASSED!" << std::endl;
  } else {
    std::cout << "\nSome offscreen rendering tests FAILED!" << std::endl;
  }

  return result;
}