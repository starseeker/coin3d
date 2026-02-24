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
 * @file test_details_suite.cpp
 * @brief Tests for Coin3D detail classes.
 *
 * Covers (Tier 5, priority 57):
 *   SoPointDetail  - construct, set/get coordinate/material/normal/texcoord indices, copy
 *   SoFaceDetail   - construct, setNumPoints, setFaceIndex, setPartIndex,
 *                    getNumPoints, getFaceIndex, getPartIndex, setPoint/getPoint, copy
 *   SoLineDetail   - construct, setLineIndex, setPartIndex, setPoint0/Point1,
 *                    getLineIndex, getPartIndex, getPoint0/getPoint1, copy
 *   SoRayPickAction against SoIndexedFaceSet: verify face detail attached to picked point
 *
 * Subsystems improved: details, actions
 */

#include "../test_utils.h"

#include <Inventor/details/SoDetail.h>
#include <Inventor/details/SoPointDetail.h>
#include <Inventor/details/SoFaceDetail.h>
#include <Inventor/details/SoLineDetail.h>
#include <Inventor/SoType.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbLinear.h>

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoPointDetail
    // -----------------------------------------------------------------------
    runner.startTest("SoPointDetail: getClassTypeId is not badType");
    {
        bool pass = (SoPointDetail::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoPointDetail has bad class type");
    }

    runner.startTest("SoPointDetail: construct and default indices are 0");
    {
        SoPointDetail pd;
        // Coin initialises all indices to 0 by default
        bool pass = (pd.getCoordinateIndex() == 0) &&
                    (pd.getMaterialIndex()   == 0) &&
                    (pd.getNormalIndex()     == 0) &&
                    (pd.getTextureCoordIndex() == 0);
        runner.endTest(pass, pass ? "" :
            "SoPointDetail default indices should be 0");
    }

    runner.startTest("SoPointDetail: setCoordinateIndex / getCoordinateIndex");
    {
        SoPointDetail pd;
        pd.setCoordinateIndex(7);
        bool pass = (pd.getCoordinateIndex() == 7);
        runner.endTest(pass, pass ? "" :
            "SoPointDetail coordinate index round-trip failed");
    }

    runner.startTest("SoPointDetail: setMaterialIndex / getMaterialIndex");
    {
        SoPointDetail pd;
        pd.setMaterialIndex(3);
        bool pass = (pd.getMaterialIndex() == 3);
        runner.endTest(pass, pass ? "" :
            "SoPointDetail material index round-trip failed");
    }

    runner.startTest("SoPointDetail: setNormalIndex / getNormalIndex");
    {
        SoPointDetail pd;
        pd.setNormalIndex(5);
        bool pass = (pd.getNormalIndex() == 5);
        runner.endTest(pass, pass ? "" :
            "SoPointDetail normal index round-trip failed");
    }

    runner.startTest("SoPointDetail: setTextureCoordIndex / getTextureCoordIndex");
    {
        SoPointDetail pd;
        pd.setTextureCoordIndex(2);
        bool pass = (pd.getTextureCoordIndex() == 2);
        runner.endTest(pass, pass ? "" :
            "SoPointDetail texture coord index round-trip failed");
    }

    runner.startTest("SoPointDetail: copy() produces independent duplicate");
    {
        SoPointDetail pd;
        pd.setCoordinateIndex(10);
        pd.setMaterialIndex(11);
        SoDetail * copied = pd.copy();
        bool pass = (copied != nullptr);
        if (pass) {
            SoPointDetail * cpd = static_cast<SoPointDetail *>(copied);
            pass = (cpd->getCoordinateIndex() == 10) &&
                   (cpd->getMaterialIndex()   == 11);
        }
        delete copied;
        runner.endTest(pass, pass ? "" :
            "SoPointDetail::copy() failed or produced wrong values");
    }

    // -----------------------------------------------------------------------
    // SoFaceDetail
    // -----------------------------------------------------------------------
    runner.startTest("SoFaceDetail: getClassTypeId is not badType");
    {
        bool pass = (SoFaceDetail::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoFaceDetail has bad class type");
    }

    runner.startTest("SoFaceDetail: construct, default getNumPoints == 0");
    {
        SoFaceDetail fd;
        bool pass = (fd.getNumPoints() == 0);
        runner.endTest(pass, pass ? "" :
            "SoFaceDetail default numPoints should be 0");
    }

    runner.startTest("SoFaceDetail: setFaceIndex / getFaceIndex round-trip");
    {
        SoFaceDetail fd;
        fd.setFaceIndex(4);
        bool pass = (fd.getFaceIndex() == 4);
        runner.endTest(pass, pass ? "" :
            "SoFaceDetail face index round-trip failed");
    }

    runner.startTest("SoFaceDetail: setPartIndex / getPartIndex round-trip");
    {
        SoFaceDetail fd;
        fd.setPartIndex(2);
        bool pass = (fd.getPartIndex() == 2);
        runner.endTest(pass, pass ? "" :
            "SoFaceDetail part index round-trip failed");
    }

    runner.startTest("SoFaceDetail: setNumPoints(3), getNumPoints == 3");
    {
        SoFaceDetail fd;
        fd.setNumPoints(3);
        bool pass = (fd.getNumPoints() == 3);
        runner.endTest(pass, pass ? "" :
            "SoFaceDetail setNumPoints(3) failed");
    }

    runner.startTest("SoFaceDetail: setPoint/getPoint round-trip");
    {
        SoFaceDetail fd;
        fd.setNumPoints(2);
        SoPointDetail pd0, pd1;
        pd0.setCoordinateIndex(10);
        pd1.setCoordinateIndex(20);
        fd.setPoint(0, &pd0);
        fd.setPoint(1, &pd1);
        const SoPointDetail * got0 = fd.getPoint(0);
        const SoPointDetail * got1 = fd.getPoint(1);
        bool pass = (got0 != nullptr) && (got1 != nullptr) &&
                    (got0->getCoordinateIndex() == 10) &&
                    (got1->getCoordinateIndex() == 20);
        runner.endTest(pass, pass ? "" :
            "SoFaceDetail setPoint/getPoint round-trip failed");
    }

    runner.startTest("SoFaceDetail: incFaceIndex increments by 1");
    {
        SoFaceDetail fd;
        fd.setFaceIndex(5);
        fd.incFaceIndex();
        bool pass = (fd.getFaceIndex() == 6);
        runner.endTest(pass, pass ? "" :
            "SoFaceDetail incFaceIndex should increment by 1");
    }

    runner.startTest("SoFaceDetail: incPartIndex increments by 1");
    {
        SoFaceDetail fd;
        fd.setPartIndex(3);
        fd.incPartIndex();
        bool pass = (fd.getPartIndex() == 4);
        runner.endTest(pass, pass ? "" :
            "SoFaceDetail incPartIndex should increment by 1");
    }

    runner.startTest("SoFaceDetail: copy() produces independent duplicate");
    {
        SoFaceDetail fd;
        fd.setFaceIndex(7);
        fd.setPartIndex(2);
        SoDetail * copied = fd.copy();
        bool pass = (copied != nullptr);
        if (pass) {
            SoFaceDetail * cfd = static_cast<SoFaceDetail *>(copied);
            pass = (cfd->getFaceIndex() == 7) &&
                   (cfd->getPartIndex() == 2);
        }
        delete copied;
        runner.endTest(pass, pass ? "" :
            "SoFaceDetail::copy() failed or produced wrong values");
    }

    // -----------------------------------------------------------------------
    // SoLineDetail
    // -----------------------------------------------------------------------
    runner.startTest("SoLineDetail: getClassTypeId is not badType");
    {
        bool pass = (SoLineDetail::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoLineDetail has bad class type");
    }

    runner.startTest("SoLineDetail: construct, default indices are 0");
    {
        SoLineDetail ld;
        // Default indices should be 0
        bool pass = (ld.getLineIndex() == 0) && (ld.getPartIndex() == 0);
        runner.endTest(pass, pass ? "" :
            "SoLineDetail default indices should be 0");
    }

    runner.startTest("SoLineDetail: setLineIndex / getLineIndex round-trip");
    {
        SoLineDetail ld;
        ld.setLineIndex(8);
        bool pass = (ld.getLineIndex() == 8);
        runner.endTest(pass, pass ? "" :
            "SoLineDetail line index round-trip failed");
    }

    runner.startTest("SoLineDetail: setPartIndex / getPartIndex round-trip");
    {
        SoLineDetail ld;
        ld.setPartIndex(3);
        bool pass = (ld.getPartIndex() == 3);
        runner.endTest(pass, pass ? "" :
            "SoLineDetail part index round-trip failed");
    }

    runner.startTest("SoLineDetail: setPoint0 / getPoint0 round-trip");
    {
        SoLineDetail ld;
        SoPointDetail pd;
        pd.setCoordinateIndex(42);
        ld.setPoint0(&pd);
        const SoPointDetail * got = ld.getPoint0();
        bool pass = (got != nullptr) && (got->getCoordinateIndex() == 42);
        runner.endTest(pass, pass ? "" :
            "SoLineDetail setPoint0/getPoint0 round-trip failed");
    }

    runner.startTest("SoLineDetail: setPoint1 / getPoint1 round-trip");
    {
        SoLineDetail ld;
        SoPointDetail pd;
        pd.setCoordinateIndex(99);
        ld.setPoint1(&pd);
        const SoPointDetail * got = ld.getPoint1();
        bool pass = (got != nullptr) && (got->getCoordinateIndex() == 99);
        runner.endTest(pass, pass ? "" :
            "SoLineDetail setPoint1/getPoint1 round-trip failed");
    }

    runner.startTest("SoLineDetail: incLineIndex increments by 1");
    {
        SoLineDetail ld;
        ld.setLineIndex(2);
        ld.incLineIndex();
        bool pass = (ld.getLineIndex() == 3);
        runner.endTest(pass, pass ? "" :
            "SoLineDetail incLineIndex should increment by 1");
    }

    runner.startTest("SoLineDetail: incPartIndex increments by 1");
    {
        SoLineDetail ld;
        ld.setPartIndex(1);
        ld.incPartIndex();
        bool pass = (ld.getPartIndex() == 2);
        runner.endTest(pass, pass ? "" :
            "SoLineDetail incPartIndex should increment by 1");
    }

    runner.startTest("SoLineDetail: copy() produces independent duplicate");
    {
        SoLineDetail ld;
        ld.setLineIndex(5);
        ld.setPartIndex(1);
        SoDetail * copied = ld.copy();
        bool pass = (copied != nullptr);
        if (pass) {
            SoLineDetail * cld = static_cast<SoLineDetail *>(copied);
            pass = (cld->getLineIndex() == 5) &&
                   (cld->getPartIndex() == 1);
        }
        delete copied;
        runner.endTest(pass, pass ? "" :
            "SoLineDetail::copy() failed or produced wrong values");
    }

    // -----------------------------------------------------------------------
    // SoRayPickAction: face detail extracted from SoIndexedFaceSet
    // -----------------------------------------------------------------------
    runner.startTest("SoRayPickAction: SoFaceDetail attached to picked IFS point");
    {
        //
        // Build a simple scene: a quad at z=0 facing the camera
        //   coords: (-1,-1,0), (1,-1,0), (1,1,0), (-1,1,0)
        //   face: 0, 1, 2, 3, -1
        //
        SoSeparator * root = new SoSeparator;
        root->ref();

        SoCoordinate3 * coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
        coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
        coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
        coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
        root->addChild(coords);

        SoIndexedFaceSet * ifs = new SoIndexedFaceSet;
        int32_t indices[] = { 0, 1, 2, 3, -1 };
        ifs->coordIndex.setValues(0, 5, indices);
        root->addChild(ifs);

        // Shoot a ray from z=10 toward z=0 through the centre of the quad
        SbViewportRegion vp(256, 256);
        SoRayPickAction rpa(vp);
        rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f),
                   SbVec3f(0.0f, 0.0f, -1.0f));
        rpa.apply(root);

        const SoPickedPoint * pp = rpa.getPickedPoint();
        bool pass = false;
        if (pp != nullptr) {
            const SoDetail * detail = pp->getDetail();
            if (detail != nullptr) {
                pass = detail->isOfType(SoFaceDetail::getClassTypeId());
            }
        }
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoRayPickAction on SoIndexedFaceSet should return SoFaceDetail");
    }

    runner.startTest("SoRayPickAction: face detail faceIndex >= 0");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();

        SoCoordinate3 * coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
        coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
        coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
        coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
        root->addChild(coords);

        SoIndexedFaceSet * ifs = new SoIndexedFaceSet;
        int32_t indices[] = { 0, 1, 2, 3, -1 };
        ifs->coordIndex.setValues(0, 5, indices);
        root->addChild(ifs);

        SbViewportRegion vp(256, 256);
        SoRayPickAction rpa(vp);
        rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f),
                   SbVec3f(0.0f, 0.0f, -1.0f));
        rpa.apply(root);

        const SoPickedPoint * pp = rpa.getPickedPoint();
        bool pass = false;
        if (pp != nullptr) {
            const SoDetail * d = pp->getDetail();
            if (d && d->isOfType(SoFaceDetail::getClassTypeId())) {
                const SoFaceDetail * fd =
                    static_cast<const SoFaceDetail *>(d);
                pass = (fd->getFaceIndex() >= 0);
            }
        }
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoFaceDetail from ray pick should have faceIndex >= 0");
    }

    runner.startTest("SoRayPickAction: face detail numPoints > 0");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();

        SoCoordinate3 * coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
        coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
        coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
        coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
        root->addChild(coords);

        SoIndexedFaceSet * ifs = new SoIndexedFaceSet;
        int32_t indices[] = { 0, 1, 2, 3, -1 };
        ifs->coordIndex.setValues(0, 5, indices);
        root->addChild(ifs);

        SbViewportRegion vp(256, 256);
        SoRayPickAction rpa(vp);
        rpa.setRay(SbVec3f(0.0f, 0.0f, 10.0f),
                   SbVec3f(0.0f, 0.0f, -1.0f));
        rpa.apply(root);

        const SoPickedPoint * pp = rpa.getPickedPoint();
        bool pass = false;
        if (pp != nullptr) {
            const SoDetail * d = pp->getDetail();
            if (d && d->isOfType(SoFaceDetail::getClassTypeId())) {
                const SoFaceDetail * fd =
                    static_cast<const SoFaceDetail *>(d);
                pass = (fd->getNumPoints() > 0);
            }
        }
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoFaceDetail from ray pick should have numPoints > 0");
    }

    // -----------------------------------------------------------------------
    // SoRayPickAction: line detail from SoIndexedLineSet
    // -----------------------------------------------------------------------
    runner.startTest("SoRayPickAction: SoLineDetail attached to picked ILS point");
    {
        // Two-point line segment along the ray axis
        SoSeparator * root = new SoSeparator;
        root->ref();

        SoCoordinate3 * coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f(-5.0f, 0.0f, 0.0f));
        coords->point.set1Value(1, SbVec3f( 5.0f, 0.0f, 0.0f));
        root->addChild(coords);

        SoIndexedLineSet * ils = new SoIndexedLineSet;
        int32_t indices[] = { 0, 1, -1 };
        ils->coordIndex.setValues(0, 3, indices);
        root->addChild(ils);

        // Ray from above, along Y axis — intersects the line at (0,0,0)
        SbViewportRegion vp(256, 256);
        SoRayPickAction rpa(vp);
        rpa.setRay(SbVec3f(0.0f, 10.0f, 0.0f),
                   SbVec3f(0.0f, -1.0f, 0.0f),
                   0.1f); // near radius for line picking
        rpa.apply(root);

        const SoPickedPoint * pp = rpa.getPickedPoint();
        bool pass = false;
        if (pp != nullptr) {
            const SoDetail * d = pp->getDetail();
            if (d != nullptr) {
                pass = d->isOfType(SoLineDetail::getClassTypeId());
            }
        }
        root->unref();
        runner.endTest(pass, pass ? "" :
            "SoRayPickAction on SoIndexedLineSet should return SoLineDetail");
    }

    return runner.getSummary();
}
