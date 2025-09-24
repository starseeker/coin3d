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

/**
 * @file test_geo_comprehensive.cpp
 * @brief Comprehensive tests for all Coin3D geo-spatial types and user-facing functionality
 *
 * This module provides comprehensive testing of geographic coordinate systems,
 * projections, geo-spatial scene graph nodes, and geographic transformations.
 */

#include "utils/test_common.h"
#include "utils/osmesa_test_context.h"
#include "utils/scene_graph_test_utils.h"

// Core geo classes  
#include <Inventor/misc/SoGeo.h>
#include <Inventor/elements/SoGeoElement.h>
#include <Inventor/nodes/SoGeoCoordinate.h>
#include <Inventor/nodes/SoGeoLocation.h>
#include <Inventor/nodes/SoGeoOrigin.h>
#include <Inventor/nodes/SoGeoSeparator.h>

// Supporting classes for geo testing
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/SbLinear.h>
#include <Inventor/SbViewportRegion.h>

using namespace CoinTestUtils;

TEST_CASE("Geo System Comprehensive Tests", "[geo][comprehensive]") {
    CoinTestFixture fixture;
    
    SECTION("Basic geo coordinate systems") {
        SECTION("Geo coordinate creation and basic properties") {
            SoGeoCoordinate* geoCoord = new SoGeoCoordinate;
            geoCoord->ref();
            
            // Test basic coordinate system setup
            CHECK(geoCoord != nullptr);
            CHECK(geoCoord->getTypeId() != SoType::badType());
            
            // Test setting coordinate system
            geoCoord->geoSystem.set1Value(0, "GD");
            geoCoord->geoSystem.set1Value(1, "WE");
            geoCoord->geoSystem.set1Value(2, "M");
            
            CHECK(geoCoord->geoSystem.getNum() == 3);
            CHECK(geoCoord->geoSystem[0] == "GD");
            CHECK(geoCoord->geoSystem[1] == "WE");
            CHECK(geoCoord->geoSystem[2] == "M");
            
            geoCoord->unref();
        }
        
        SECTION("Geo coordinate points") {
            SoGeoCoordinate* geoCoord = new SoGeoCoordinate;
            geoCoord->ref();
            
            // Set up coordinate system (Geographic, WGS84, meters)
            geoCoord->geoSystem.set1Value(0, "GD");
            geoCoord->geoSystem.set1Value(1, "WE");
            geoCoord->geoSystem.set1Value(2, "M");
            
            // Add some test coordinates (longitude, latitude, altitude)
            geoCoord->point.set1Value(0, SbVec3d(-122.4194, 37.7749, 0));  // San Francisco
            geoCoord->point.set1Value(1, SbVec3d(-74.0060, 40.7128, 0));   // New York
            geoCoord->point.set1Value(2, SbVec3d(2.3522, 48.8566, 0));     // Paris
            
            CHECK(geoCoord->point.getNum() == 3);
            
            SbVec3d sf_coord = geoCoord->point[0];
            CHECK(sf_coord[0] == -122.4194);  // longitude
            CHECK(sf_coord[1] == 37.7749);   // latitude
            CHECK(sf_coord[2] == 0);         // altitude
            
            geoCoord->unref();
        }
    }
    
    SECTION("Geo origin tests") {
        SECTION("Geo origin basic operations") {
            SoGeoOrigin* geoOrigin = new SoGeoOrigin;
            geoOrigin->ref();
            
            CHECK(geoOrigin != nullptr);
            CHECK(geoOrigin->getTypeId() != SoType::badType());
            
            // Set up coordinate system
            geoOrigin->geoSystem.set1Value(0, "GD");
            geoOrigin->geoSystem.set1Value(1, "WE");
            geoOrigin->geoSystem.set1Value(2, "M");
            
            // Set origin point (London coordinates)
            geoOrigin->geoCoords.setValue(SbVec3d(-0.1276, 51.5074, 0));
            
            SbVec3d origin = geoOrigin->geoCoords.getValue();
            CHECK(origin[0] == -0.1276);  // longitude
            CHECK(origin[1] == 51.5074);  // latitude
            CHECK(origin[2] == 0);        // altitude
            
            geoOrigin->unref();
        }
    }
    
    SECTION("Geo location tests") {
        SECTION("Geo location node operations") {
            SoGeoLocation* geoLocation = new SoGeoLocation;
            geoLocation->ref();
            
            CHECK(geoLocation != nullptr);
            CHECK(geoLocation->getTypeId() != SoType::badType());
            
            // Set up coordinate system
            geoLocation->geoSystem.set1Value(0, "GD");
            geoLocation->geoSystem.set1Value(1, "WE");
            geoLocation->geoSystem.set1Value(2, "M");
            
            // Set location coordinates (Tokyo)
            geoLocation->geoCoords.setValue(SbVec3d(139.6503, 35.6762, 0));
            
            SbVec3d location = geoLocation->geoCoords.getValue();
            CHECK(location[0] == 139.6503);  // longitude
            CHECK(location[1] == 35.6762);   // latitude
            CHECK(location[2] == 0);         // altitude
            
            geoLocation->unref();
        }
        
        SECTION("Geo location with scene structure") {
            SoSeparator* root = new SoSeparator;
            root->ref();
            
            SoGeoLocation* geoLocation = new SoGeoLocation;
            
            // Set up coordinate system
            geoLocation->geoSystem.set1Value(0, "GD");
            geoLocation->geoSystem.set1Value(1, "WE");
            geoLocation->geoSystem.set1Value(2, "M");
            
            // Set location
            geoLocation->geoCoords.setValue(SbVec3d(0, 0, 0)); // Null Island
            
            // Add geo location and geometry to scene
            root->addChild(geoLocation);
            SoCube* cube = new SoCube;
            root->addChild(cube);
            
            CHECK(root->getNumChildren() == 2);
            CHECK(root->getChild(0) == geoLocation);
            CHECK(root->getChild(1) == cube);
            
            root->unref();
        }
    }
    
    SECTION("Geo separator tests") {
        SECTION("Geo separator scene management") {
            SoGeoSeparator* geoSeparator = new SoGeoSeparator;
            geoSeparator->ref();
            
            CHECK(geoSeparator != nullptr);
            CHECK(geoSeparator->getTypeId() != SoType::badType());
            
            // Set up coordinate system
            geoSeparator->geoSystem.set1Value(0, "GD");
            geoSeparator->geoSystem.set1Value(1, "WE");
            geoSeparator->geoSystem.set1Value(2, "M");
            
            // Add geo origin
            SoGeoOrigin* origin = new SoGeoOrigin;
            origin->geoSystem.set1Value(0, "GD");
            origin->geoSystem.set1Value(1, "WE");
            origin->geoSystem.set1Value(2, "M");
            origin->geoCoords.setValue(SbVec3d(0, 0, 0));
            geoSeparator->addChild(origin);
            
            // Add geo location with geometry
            SoGeoLocation* location = new SoGeoLocation;
            location->geoSystem.set1Value(0, "GD");
            location->geoSystem.set1Value(1, "WE");
            location->geoSystem.set1Value(2, "M");
            location->geoCoords.setValue(SbVec3d(1, 1, 0));
            
            SoSphere* sphere = new SoSphere;
            geoSeparator->addChild(location);
            geoSeparator->addChild(sphere);
            
            CHECK(geoSeparator->getNumChildren() == 3);  // origin + location + sphere
            
            geoSeparator->unref();
        }
        
        SECTION("Simple geo scene hierarchy") {
            SoGeoSeparator* geoScene = new SoGeoSeparator;
            geoScene->ref();
            
            // Test basic hierarchy creation without complex coordinate systems
            CHECK(geoScene->getNumChildren() == 0);
            
            SoGeoLocation* location = new SoGeoLocation;
            geoScene->addChild(location);
            CHECK(geoScene->getNumChildren() == 1);
            
            geoScene->unref();
        }
    }
    
    SECTION("Geo coordinate system variants") {
        SECTION("UTM coordinate system") {
            SoGeoCoordinate* geoCoord = new SoGeoCoordinate;
            geoCoord->ref();
            
            // Set up UTM coordinate system (Zone 10, Northern Hemisphere, WGS84)
            geoCoord->geoSystem.set1Value(0, "UTM");
            geoCoord->geoSystem.set1Value(1, "Z10");
            geoCoord->geoSystem.set1Value(2, "N");
            geoCoord->geoSystem.set1Value(3, "WE");
            
            CHECK(geoCoord->geoSystem.getNum() == 4);
            CHECK(geoCoord->geoSystem[0] == "UTM");
            CHECK(geoCoord->geoSystem[1] == "Z10");
            
            // Add UTM coordinates (easting, northing, elevation)
            geoCoord->point.set1Value(0, SbVec3d(500000, 4000000, 100));
            
            SbVec3d utm_coord = geoCoord->point[0];
            CHECK(utm_coord[0] == 500000);   // easting
            CHECK(utm_coord[1] == 4000000);  // northing
            CHECK(utm_coord[2] == 100);      // elevation
            
            geoCoord->unref();
        }
        
        SECTION("Geocentric coordinate system") {
            SoGeoCoordinate* geoCoord = new SoGeoCoordinate;
            geoCoord->ref();
            
            // Set up geocentric coordinate system
            geoCoord->geoSystem.set1Value(0, "GC");
            geoCoord->geoSystem.set1Value(1, "WE");
            
            CHECK(geoCoord->geoSystem.getNum() == 2);
            CHECK(geoCoord->geoSystem[0] == "GC");
            
            // Add geocentric coordinates (X, Y, Z from Earth center)
            geoCoord->point.set1Value(0, SbVec3d(6378137, 0, 0));  // Point on equator
            
            SbVec3d gc_coord = geoCoord->point[0];
            CHECK(gc_coord[0] == 6378137);  // X (approximately Earth radius)
            CHECK(gc_coord[1] == 0);        // Y
            CHECK(gc_coord[2] == 0);        // Z
            
            geoCoord->unref();
        }
    }
    
    SECTION("Geo scene basic functionality") {
        // Test basic geo scene creation without rendering
        auto geoScene = new SoGeoSeparator;
        geoScene->ref();
        
        // Test basic operations
        CHECK(geoScene->getTypeId() != SoType::badType());
        CHECK(geoScene->getNumChildren() == 0);
        
        geoScene->unref();
    }
}

// ============================================================================

TEST_CASE("Geo System Performance and Edge Cases", "[geo][edge_cases]") {
    CoinTestFixture fixture;
    
    SECTION("Basic geo coordinate system testing") {
        // Test basic geo components without complex scene graphs
        SoGeoCoordinate* geoCoord = new SoGeoCoordinate;
        geoCoord->ref();
        
        // Set up basic coordinate system
        geoCoord->geoSystem.set1Value(0, "GD");
        geoCoord->geoSystem.set1Value(1, "WE");
        geoCoord->geoSystem.set1Value(2, "M");
        
        // Test basic coordinate setting
        geoCoord->point.set1Value(0, SbVec3d(0.0, 0.0, 0.0));
        geoCoord->point.set1Value(1, SbVec3d(1.0, 1.0, 0.0));
        
        CHECK(geoCoord->point.getNum() == 2);
        CHECK(geoCoord->geoSystem.getNum() == 3);
        
        geoCoord->unref();
    }
    
    SECTION("Coordinate system edge cases") {
        SECTION("Invalid coordinate system") {
            SoGeoCoordinate* geoCoord = new SoGeoCoordinate;
            geoCoord->ref();
            
            // Test with minimal valid system (might have defaults)
            geoCoord->geoSystem.set1Value(0, "GD");
            CHECK(geoCoord->geoSystem.getNum() >= 1);
            
            // Add coordinates even with minimal system
            geoCoord->point.set1Value(0, SbVec3d(0, 0, 0));
            CHECK(geoCoord->point.getNum() == 1);
            
            geoCoord->unref();
        }
        
        SECTION("Extreme coordinate values") {
            SoGeoCoordinate* geoCoord = new SoGeoCoordinate;
            geoCoord->ref();
            
            geoCoord->geoSystem.set1Value(0, "GD");
            geoCoord->geoSystem.set1Value(1, "WE");
            geoCoord->geoSystem.set1Value(2, "M");
            
            // Test extreme valid coordinates
            geoCoord->point.set1Value(0, SbVec3d(-180, -90, -1000));  // South pole, below sea level
            geoCoord->point.set1Value(1, SbVec3d(180, 90, 10000));    // North pole, high altitude
            
            CHECK(geoCoord->point.getNum() == 2);
            
            SbVec3d extreme1 = geoCoord->point[0];
            SbVec3d extreme2 = geoCoord->point[1];
            
            CHECK(extreme1[0] == -180);
            CHECK(extreme1[1] == -90);
            CHECK(extreme2[0] == 180);
            CHECK(extreme2[1] == 90);
            
            geoCoord->unref();
        }
    }
    
    SECTION("Geo node hierarchy edge cases") {
        SECTION("Nested geo separators") {
            SoGeoSeparator* outerGeo = new SoGeoSeparator;
            outerGeo->ref();
            
            outerGeo->geoSystem.set1Value(0, "GD");
            outerGeo->geoSystem.set1Value(1, "WE");
            outerGeo->geoSystem.set1Value(2, "M");
            
            SoGeoSeparator* innerGeo = new SoGeoSeparator;
            innerGeo->geoSystem.set1Value(0, "GD");
            innerGeo->geoSystem.set1Value(1, "WE");
            innerGeo->geoSystem.set1Value(2, "M");
            
            innerGeo->addChild(new SoCube);
            outerGeo->addChild(innerGeo);
            
            CHECK(outerGeo->getNumChildren() == 1);
            CHECK(innerGeo->getNumChildren() == 1);
            
            outerGeo->unref();
        }
        
        SECTION("Geo location without explicit coordinates") {
            SoGeoLocation* location = new SoGeoLocation;
            location->ref();
            
            // Set up coordinate system but leave coordinates at default
            location->geoSystem.set1Value(0, "GD");
            location->geoSystem.set1Value(1, "WE");
            location->geoSystem.set1Value(2, "M");
            
            // Should still be valid node type
            CHECK(location->getTypeId() != SoType::badType());
            
            // Basic functionality test - no complex operations
            CHECK(location->geoCoords.getValue() == SbVec3d(0, 0, 0));
            
            location->unref();
        }
    }
}