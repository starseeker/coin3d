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

#include "utils/test_common.h"
#include <catch2/catch_approx.hpp>
#include <Inventor/SbBox2d.h>
#include <Inventor/SbBox2f.h>
#include <Inventor/SbBox2i32.h>
#include <Inventor/SbBox2s.h>
#include <Inventor/SbBox3d.h>
#include <Inventor/SbBox3f.h>
#include <Inventor/SbBox3i32.h>
#include <Inventor/SbBox3s.h>
#include <Inventor/SbByteBuffer.h>
#include <Inventor/SbClip.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbColor4f.h>
#include <Inventor/SbCylinder.h>
#include <Inventor/SbDPLine.h>
#include <Inventor/SbDPMatrix.h>
#include <Inventor/SbDPPlane.h>
#include <Inventor/SbDPRotation.h>
#include <Inventor/SbDPViewVolume.h>
#include <Inventor/SbDict.h>
#include <Inventor/SbHeap.h>
#include <Inventor/SbImage.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbName.h>
#include <Inventor/SbOctTree.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbSphere.h>
#include <Inventor/SbString.h>
#include <Inventor/SbTesselator.h>
#include <Inventor/SbTime.h>
#include <Inventor/SbVec2b.h>
#include <Inventor/SbVec2d.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec2i32.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbVec2ub.h>
#include <Inventor/SbVec2ui32.h>
#include <Inventor/SbVec2us.h>
#include <Inventor/SbVec3b.h>
#include <Inventor/SbVec3d.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbVec3i32.h>
#include <Inventor/SbVec3s.h>
#include <Inventor/SbVec3ub.h>
#include <Inventor/SbVec3ui32.h>
#include <Inventor/SbVec3us.h>
#include <Inventor/SbVec4b.h>
#include <Inventor/SbVec4d.h>
#include <Inventor/SbVec4f.h>
#include <Inventor/SbVec4i32.h>
#include <Inventor/SbVec4s.h>
#include <Inventor/SbVec4ub.h>
#include <Inventor/SbVec4ui32.h>
#include <Inventor/SbVec4us.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbXfBox3d.h>
#include <Inventor/SbXfBox3f.h>

using namespace CoinTestUtils;

// Comprehensive tests for additional base classes not covered in basic test

TEST_CASE("SbVec2f complete functionality", "[base][SbVec2f][complete]") {
    CoinTestFixture fixture;

    SECTION("construction and basic operations") {
        SbVec2f v1;
        CHECK(v1[0] == 0.0f);
        CHECK(v1[1] == 0.0f);

        SbVec2f v2(1.0f, 2.0f);
        CHECK(v2[0] == 1.0f);
        CHECK(v2[1] == 2.0f);

        SbVec2f v3(v2);
        CHECK(v3[0] == 1.0f);
        CHECK(v3[1] == 2.0f);
    }

    SECTION("arithmetic operations") {
        SbVec2f v1(1.0f, 2.0f);
        SbVec2f v2(3.0f, 4.0f);

        SbVec2f sum = v1 + v2;
        CHECK(sum[0] == 4.0f);
        CHECK(sum[1] == 6.0f);

        SbVec2f diff = v2 - v1;
        CHECK(diff[0] == 2.0f);
        CHECK(diff[1] == 2.0f);

        SbVec2f scaled = v1 * 2.0f;
        CHECK(scaled[0] == 2.0f);
        CHECK(scaled[1] == 4.0f);
    }

    SECTION("length and normalization") {
        SbVec2f v(3.0f, 4.0f);
        CHECK(v.length() == 5.0f);
        CHECK(v.sqrLength() == 25.0f);

        SbVec2f normalized = v;
        normalized.normalize();
        CHECK(normalized.length() == Catch::Approx(1.0f));
    }
}

TEST_CASE("SbVec3f complete functionality", "[base][SbVec3f][complete]") {
    CoinTestFixture fixture;

    SECTION("construction and basic operations") {
        SbVec3f v1;
        CHECK(v1[0] == 0.0f);
        CHECK(v1[1] == 0.0f);
        CHECK(v1[2] == 0.0f);

        SbVec3f v2(1.0f, 2.0f, 3.0f);
        CHECK(v2[0] == 1.0f);
        CHECK(v2[1] == 2.0f);
        CHECK(v2[2] == 3.0f);
    }

    SECTION("cross product") {
        SbVec3f v1(1.0f, 0.0f, 0.0f);
        SbVec3f v2(0.0f, 1.0f, 0.0f);
        SbVec3f cross = v1.cross(v2);
        CHECK(cross[0] == 0.0f);
        CHECK(cross[1] == 0.0f);
        CHECK(cross[2] == 1.0f);
    }

    SECTION("dot product") {
        SbVec3f v1(1.0f, 2.0f, 3.0f);
        SbVec3f v2(2.0f, 3.0f, 4.0f);
        float dot = v1.dot(v2);
        CHECK(dot == 20.0f); // 1*2 + 2*3 + 3*4 = 2 + 6 + 12 = 20
    }
}

TEST_CASE("SbMatrix basic functionality", "[base][SbMatrix][complete]") {
    CoinTestFixture fixture;

    SECTION("identity matrix") {
        SbMatrix identity;
        identity.makeIdentity();
        
        SbVec3f v(1.0f, 2.0f, 3.0f);
        SbVec3f result;
        identity.multVecMatrix(v, result);
        
        CHECK(result[0] == 1.0f);
        CHECK(result[1] == 2.0f);
        CHECK(result[2] == 3.0f);
    }

    SECTION("translation matrix") {
        SbMatrix trans;
        trans.setTranslate(SbVec3f(5.0f, 10.0f, 15.0f));
        
        SbVec3f v(1.0f, 2.0f, 3.0f);
        SbVec3f result;
        trans.multVecMatrix(v, result);
        
        CHECK(result[0] == 6.0f);
        CHECK(result[1] == 12.0f);
        CHECK(result[2] == 18.0f);
    }
}

TEST_CASE("SbRotation basic functionality", "[base][SbRotation][complete]") {
    CoinTestFixture fixture;

    SECTION("identity rotation") {
        SbRotation identity;
        CHECK(identity.getValue()[0] == 0.0f);
        CHECK(identity.getValue()[1] == 0.0f);
        CHECK(identity.getValue()[2] == 0.0f);
        CHECK(identity.getValue()[3] == 1.0f);
    }

    SECTION("axis-angle rotation") {
        SbRotation rot(SbVec3f(0.0f, 0.0f, 1.0f), static_cast<float>(M_PI / 2.0));
        SbVec3f x_axis(1.0f, 0.0f, 0.0f);
        SbVec3f result;
        rot.multVec(x_axis, result);
        
        CHECK(result[0] == Catch::Approx(0.0f).margin(1e-6));
        CHECK(result[1] == Catch::Approx(1.0f).margin(1e-6));
        CHECK(result[2] == Catch::Approx(0.0f).margin(1e-6));
    }
}

TEST_CASE("SbString basic functionality", "[base][SbString][complete]") {
    CoinTestFixture fixture;

    SECTION("construction and assignment") {
        SbString str1;
        CHECK(str1.getLength() == 0);
        
        SbString str2("Hello");
        CHECK(str2.getLength() == 5);
        CHECK(strcmp(str2.getString(), "Hello") == 0);
        
        SbString str3(str2);
        CHECK(str3.getLength() == 5);
        CHECK(strcmp(str3.getString(), "Hello") == 0);
    }

    SECTION("concatenation") {
        SbString str1("Hello");
        SbString str2(" World");
        SbString result = str1 + str2;
        CHECK(result.getLength() == 11);
        CHECK(strcmp(result.getString(), "Hello World") == 0);
    }

    SECTION("comparison") {
        SbString str1("Hello");
        SbString str2("Hello");
        SbString str3("World");
        
        CHECK(str1 == str2);
        CHECK(str1 != str3);
    }
}

TEST_CASE("SbName basic functionality", "[base][SbName][complete]") {
    CoinTestFixture fixture;

    SECTION("construction and storage") {
        SbName name1;
        CHECK(name1.getLength() == 0);
        
        SbName name2("TestName");
        CHECK(name2.getLength() == 8);
        CHECK(strcmp(name2.getString(), "TestName") == 0);
    }

    SECTION("equality and comparison") {
        SbName name1("Test");
        SbName name2("Test");
        SbName name3("Different");
        
        CHECK(name1 == name2);
        CHECK(name1 != name3);
    }
}

TEST_CASE("SbColor basic functionality", "[base][SbColor][complete]") {
    CoinTestFixture fixture;

    SECTION("construction") {
        SbColor color1;
        CHECK(color1[0] == 0.0f);
        CHECK(color1[1] == 0.0f);
        CHECK(color1[2] == 0.0f);
        
        SbColor color2(1.0f, 0.5f, 0.25f);
        CHECK(color2[0] == 1.0f);
        CHECK(color2[1] == 0.5f);
        CHECK(color2[2] == 0.25f);
    }

    SECTION("HSV conversion") {
        SbColor red(1.0f, 0.0f, 0.0f);
        float h, s, v;
        red.getHSVValue(h, s, v);
        CHECK(h == 0.0f);
        CHECK(s == 1.0f);
        CHECK(v == 1.0f);
        
        SbColor from_hsv;
        from_hsv.setHSVValue(0.0f, 1.0f, 1.0f);
        CHECK(from_hsv[0] == Catch::Approx(1.0f));
        CHECK(from_hsv[1] == Catch::Approx(0.0f));
        CHECK(from_hsv[2] == Catch::Approx(0.0f));
    }
}

TEST_CASE("SbTime basic functionality", "[base][SbTime][complete]") {
    CoinTestFixture fixture;

    SECTION("construction and basic operations") {
        SbTime t1;
        CHECK(t1.getValue() == 0.0);
        
        SbTime t2(5.5);
        CHECK(t2.getValue() == 5.5);
        
        SbTime t3(t2);
        CHECK(t3.getValue() == 5.5);
    }

    SECTION("arithmetic operations") {
        SbTime t1(3.0);
        SbTime t2(2.0);
        
        SbTime sum = t1 + t2;
        CHECK(sum.getValue() == 5.0);
        
        SbTime diff = t1 - t2;
        CHECK(diff.getValue() == 1.0);
        
        SbTime scaled = t1 * 2.0;
        CHECK(scaled.getValue() == 6.0);
    }

    SECTION("comparison") {
        SbTime t1(3.0);
        SbTime t2(5.0);
        SbTime t3(3.0);
        
        CHECK(t1 < t2);
        CHECK(t2 > t1);
        CHECK(t1 == t3);
        CHECK(t1 != t2);
    }
}

TEST_CASE("SbPlane basic functionality", "[base][SbPlane][complete]") {
    CoinTestFixture fixture;

    SECTION("construction from normal and distance") {
        SbVec3f normal(0.0f, 1.0f, 0.0f);
        float distance = 5.0f;
        SbPlane plane(normal, distance);
        
        CHECK(plane.getNormal()[0] == 0.0f);
        CHECK(plane.getNormal()[1] == 1.0f);
        CHECK(plane.getNormal()[2] == 0.0f);
        CHECK(plane.getDistanceFromOrigin() == 5.0f);
    }

    SECTION("point-to-plane distance") {
        SbPlane plane(SbVec3f(0.0f, 1.0f, 0.0f), 0.0f);
        SbVec3f point(1.0f, 5.0f, 2.0f);
        float distance = plane.getDistance(point);
        CHECK(distance == 5.0f);
    }
}

TEST_CASE("SbSphere basic functionality", "[base][SbSphere][complete]") {
    CoinTestFixture fixture;

    SECTION("construction and basic properties") {
        SbSphere sphere;
        CHECK(sphere.getRadius() == 1.0f);
        
        SbVec3f center(1.0f, 2.0f, 3.0f);
        float radius = 5.0f;
        SbSphere sphere2(center, radius);
        CHECK(sphere2.getCenter()[0] == 1.0f);
        CHECK(sphere2.getCenter()[1] == 2.0f);
        CHECK(sphere2.getCenter()[2] == 3.0f);
        CHECK(sphere2.getRadius() == 5.0f);
    }

    SECTION("bounding box circumscription") {
        SbVec3f p1(0.0f, 0.0f, 0.0f);
        SbVec3f p2(1.0f, 0.0f, 0.0f);
        SbVec3f p3(0.0f, 1.0f, 0.0f);
        SbVec3f p4(0.0f, 0.0f, 1.0f);
        
        // Create bounding box from points
        SbBox3f bbox;
        bbox.extendBy(p1);
        bbox.extendBy(p2);
        bbox.extendBy(p3);
        bbox.extendBy(p4);
        
        SbSphere sphere;
        sphere.circumscribe(bbox);
        
        // All points should be on or within the sphere
        CHECK((sphere.getCenter() - p1).length() <= sphere.getRadius());
        CHECK((sphere.getCenter() - p2).length() <= sphere.getRadius());
        CHECK((sphere.getCenter() - p3).length() <= sphere.getRadius());
        CHECK((sphere.getCenter() - p4).length() <= sphere.getRadius());
    }
}

TEST_CASE("SbViewportRegion basic functionality", "[base][SbViewportRegion][complete]") {
    CoinTestFixture fixture;

    SECTION("construction and basic properties") {
        SbViewportRegion vp;
        CHECK(vp.getViewportSizePixels()[0] > 0);
        CHECK(vp.getViewportSizePixels()[1] > 0);
        
        SbViewportRegion vp2(640, 480);
        CHECK(vp2.getViewportSizePixels()[0] == 640);
        CHECK(vp2.getViewportSizePixels()[1] == 480);
    }

    SECTION("aspect ratio") {
        SbViewportRegion vp(800, 600);
        float aspectRatio = vp.getViewportAspectRatio();
        CHECK(aspectRatio == Catch::Approx(800.0f / 600.0f));
    }
}