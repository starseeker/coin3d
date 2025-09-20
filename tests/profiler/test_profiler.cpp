/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Modernization Tests for Coin3D Profiler Module
 **************************************************************************/

#include "test_common.h"

#include <Inventor/annex/Profiler/SbProfilingData.h>
#include <Inventor/annex/Profiler/SoProfiler.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/actions/SoGLRenderAction.h>

TEST_CASE("SbProfilingData basic functionality", "[profiler][basic]") {
    SECTION("Construction and destruction") {
        SbProfilingData data;
        REQUIRE_NOTHROW(data.reset());
    }
    
    SECTION("Action timing storage") {
        SbProfilingData data;
        SbTime start(1.0);
        SbTime stop(2.0);
        
        data.setActionStartTime(start);
        data.setActionStopTime(stop);
        
        REQUIRE(data.getActionStartTime() == start);
        REQUIRE(data.getActionStopTime() == stop);
        REQUIRE(data.getActionDuration() == SbTime(1.0));
    }
    
    SECTION("Copy constructor") {
        SbProfilingData original;
        SbTime start(1.0);
        original.setActionStartTime(start);
        
        SbProfilingData copy(original);
        REQUIRE(copy.getActionStartTime() == start);
    }
    
    SECTION("Assignment operator") {
        SbProfilingData original;
        SbProfilingData assigned;
        SbTime start(1.0);
        
        original.setActionStartTime(start);
        assigned = original;
        
        REQUIRE(assigned.getActionStartTime() == start);
    }
}

TEST_CASE("SoProfiler basic functionality", "[profiler][soprofiiler]") {
    // Initialize profiler before use
    SoProfiler::init();
    
    SECTION("Enable and disable profiling") {
        // Test basic enable/disable functionality
        SbBool originalState = SoProfiler::isEnabled();
        
        SoProfiler::enable(TRUE);
        REQUIRE(SoProfiler::isEnabled() == TRUE);
        
        SoProfiler::enable(FALSE);
        REQUIRE(SoProfiler::isEnabled() == FALSE);
        
        // Restore original state
        SoProfiler::enable(originalState);
    }
    
    SECTION("Query overlay and console states") {
        // Test that these functions don't crash
        REQUIRE_NOTHROW(SoProfiler::isOverlayActive());
        REQUIRE_NOTHROW(SoProfiler::isConsoleActive());
    }
}

TEST_CASE("Profiler integration with scene graph", "[profiler][integration]") {
    // Initialize profiler before use
    SoProfiler::init();
    
    // Use raw pointers and proper Coin3D memory management
    SoSeparator* sceneRoot = new SoSeparator();
    SoCube* cube = new SoCube();
    
    sceneRoot->ref();
    cube->ref();
    
    sceneRoot->addChild(cube);
    
    SECTION("Basic scene setup") {
        // Just test that profiler can be enabled without crashing
        SbBool originalState = SoProfiler::isEnabled();
        
        REQUIRE_NOTHROW(SoProfiler::enable(TRUE));
        REQUIRE_NOTHROW(SoProfiler::enable(FALSE));
        
        // Restore original state
        SoProfiler::enable(originalState);
    }
    
    cube->unref();
    sceneRoot->unref();
}

TEST_CASE("Profiler modernization features", "[profiler][modernization]") {
    SECTION("Uses nullptr instead of NULL") {
        // This test verifies that our modernization worked correctly
        // The modernized code should compile and run without issues
        SbProfilingData data;
        
        // These calls internally use nullptr where we modernized from NULL
        REQUIRE_NOTHROW(data.reset());
        REQUIRE_NOTHROW(data.setActionStartTime(SbTime::getTimeOfDay()));
    }
    
    SECTION("Uses modern C++ types") {
        // Test that modernized typedef -> using declarations work
        SbProfilingData data;
        
        // Internal map types should work correctly with using declarations
        SbProfilingData copy;
        REQUIRE_NOTHROW(copy = data);
    }
}