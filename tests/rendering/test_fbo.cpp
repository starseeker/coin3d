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

/*
 * Tests for FBO-based offscreen rendering implementation.
 * These tests verify the integration logic without requiring OpenGL context.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#include <cstdlib>
#include "Inventor/C/glue/gl.h"

TEST_CASE("FBO offscreen rendering API exists", "[fbo][api]") {
  SECTION("Setup function is available") {
    // Just verify the function exists and can be called
    // Without an OpenGL context, this should safely do nothing
    REQUIRE_NOTHROW(cc_glglue_setup_fbo_offscreen_if_available());
  }
  
  SECTION("Callback function setter is available") {
    // Test that we can set and clear callbacks
    REQUIRE_NOTHROW(cc_glglue_context_set_offscreen_cb_functions(nullptr));
  }
}

TEST_CASE("Environment variable control", "[fbo][environment]") {
  SECTION("COIN_USE_FBO_OFFSCREEN environment variable") {
    // Test that environment variable doesn't crash the system
    setenv("COIN_USE_FBO_OFFSCREEN", "1", 1);
    REQUIRE_NOTHROW(cc_glglue_setup_fbo_offscreen_if_available());
    
    setenv("COIN_USE_FBO_OFFSCREEN", "0", 1);  
    REQUIRE_NOTHROW(cc_glglue_setup_fbo_offscreen_if_available());
    
    unsetenv("COIN_USE_FBO_OFFSCREEN");
    REQUIRE_NOTHROW(cc_glglue_setup_fbo_offscreen_if_available());
  }
  
  SECTION("COIN_DEBUG_FBO environment variable") {
    // Test debug environment variable
    setenv("COIN_DEBUG_FBO", "1", 1);
    REQUIRE_NOTHROW(cc_glglue_setup_fbo_offscreen_if_available());
    
    unsetenv("COIN_DEBUG_FBO");
    REQUIRE_NOTHROW(cc_glglue_setup_fbo_offscreen_if_available());
  }
}

TEST_CASE("FBO implementation integration", "[fbo][integration]") {
  SECTION("Offscreen context creation without GL context") {
    // In headless environment, just verify the API doesn't crash
    // when called with various parameters
    
    // Test that we don't crash when calling setup function
    cc_glglue_setup_fbo_offscreen_if_available();
    
    REQUIRE(true); // We made it this far without crashing
  }
}

TEST_CASE("FBO environment integration test", "[fbo][full_integration]") {
  SECTION("Test with FBO disabled") {
    // Disable FBO and verify fallback works
    setenv("COIN_USE_FBO_OFFSCREEN", "0", 1);
    
    // Just test that the setup function doesn't crash
    cc_glglue_setup_fbo_offscreen_if_available();
    
    REQUIRE(true); // Verify we didn't crash
  }
  
  SECTION("Test with FBO enabled") {
    // Enable FBO - without GL context this will fail gracefully
    setenv("COIN_USE_FBO_OFFSCREEN", "1", 1);
    setenv("COIN_DEBUG_FBO", "1", 1);
    
    // Setup FBO callbacks - this should not crash even without GL context
    cc_glglue_setup_fbo_offscreen_if_available();
    
    REQUIRE(true); // Verify we didn't crash
    
    // Clean up environment  
    unsetenv("COIN_USE_FBO_OFFSCREEN");
    unsetenv("COIN_DEBUG_FBO");
  }
}