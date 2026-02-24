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
 * @file test_nodekit_deep.cpp
 * @brief Deep nodekit tests supplementing test_nodekit_traversal.cpp.
 *
 * Covers:
 *   SoCameraKit    - getPart("camera", TRUE) returns a camera node
 *   SoLightKit     - getPart("light", TRUE) returns a light node
 *   SoShapeKit     - setPart("shape", new SoCube) and getPart round-trip
 *   SoSeparatorKit - construct, type check
 *   SoWrapperKit   - setPart("contents", SoSeparator), getPart round-trip
 *   Write/read round-trip for SoShapeKit
 */

#include "../test_utils.h"

#include <Inventor/nodekits/SoCameraKit.h>
#include <Inventor/nodekits/SoLightKit.h>
#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/nodekits/SoSeparatorKit.h>
#include <Inventor/nodekits/SoWrapperKit.h>
#include <Inventor/nodekits/SoBaseKit.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoLight.h>
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoOutput.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/SoType.h>
#include <cstdlib>
#include <cstring>

using namespace SimpleTest;

// ---------------------------------------------------------------------------
// Write/read helpers
// ---------------------------------------------------------------------------
static char*  g_buf      = nullptr;
static size_t g_buf_size = 0;

static void * bufGrow(void * ptr, size_t size)
{
    g_buf      = static_cast<char *>(std::realloc(ptr, size));
    g_buf_size = size;
    return g_buf;
}

static void writeNode(SoNode * root, char ** outBuf, size_t * outSize)
{
    g_buf = nullptr; g_buf_size = 0;
    SoOutput out;
    out.setBuffer(nullptr, 1, bufGrow);
    SoWriteAction wa(&out);
    wa.apply(root);
    void * ptr = nullptr; size_t nbytes = 0;
    out.getBuffer(ptr, nbytes);
    *outBuf  = static_cast<char *>(ptr);
    *outSize = nbytes;
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoCameraKit: getPart("camera", TRUE) returns a camera
    // -----------------------------------------------------------------------
    runner.startTest("SoCameraKit: type check");
    {
        SoCameraKit * kit = new SoCameraKit;
        kit->ref();
        bool pass = (kit->getTypeId() != SoType::badType()) &&
                    kit->isOfType(SoBaseKit::getClassTypeId());
        kit->unref();
        runner.endTest(pass, pass ? "" : "SoCameraKit bad type or not SoBaseKit subtype");
    }

    runner.startTest("SoCameraKit: getPart('camera', TRUE) returns non-null camera");
    {
        SoCameraKit * kit = new SoCameraKit;
        kit->ref();
        SoNode * cam = kit->getPart("camera", TRUE);
        bool pass = (cam != nullptr) &&
                    cam->isOfType(SoCamera::getClassTypeId());
        kit->unref();
        runner.endTest(pass, pass ? "" :
            "SoCameraKit: getPart('camera') should return a camera node");
    }

    // -----------------------------------------------------------------------
    // SoLightKit: getPart("light", TRUE) returns a light
    // -----------------------------------------------------------------------
    runner.startTest("SoLightKit: getPart('light', TRUE) returns non-null light");
    {
        SoLightKit * kit = new SoLightKit;
        kit->ref();
        SoNode * light = kit->getPart("light", TRUE);
        bool pass = (light != nullptr) &&
                    light->isOfType(SoLight::getClassTypeId());
        kit->unref();
        runner.endTest(pass, pass ? "" :
            "SoLightKit: getPart('light') should return a light node");
    }

    // -----------------------------------------------------------------------
    // SoShapeKit: setPart("shape", new SoCube) and getPart round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SoShapeKit: setPart and getPart round-trip");
    {
        SoShapeKit * kit = new SoShapeKit;
        kit->ref();
        SoCube * cube = new SoCube;
        kit->setPart("shape", cube);
        SoNode * got = kit->getPart("shape", FALSE);
        bool pass = (got == cube);
        kit->unref();
        runner.endTest(pass, pass ? "" :
            "SoShapeKit: getPart should return the cube that was set");
    }

    // -----------------------------------------------------------------------
    // SoSeparatorKit: type check
    // -----------------------------------------------------------------------
    runner.startTest("SoSeparatorKit: type check");
    {
        SoSeparatorKit * kit = new SoSeparatorKit;
        kit->ref();
        bool pass = (kit->getTypeId() != SoType::badType()) &&
                    kit->isOfType(SoBaseKit::getClassTypeId());
        kit->unref();
        runner.endTest(pass, pass ? "" :
            "SoSeparatorKit bad type or not SoBaseKit subtype");
    }

    // -----------------------------------------------------------------------
    // SoWrapperKit: setPart("contents", new SoSeparator) and getPart round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SoWrapperKit: setPart('contents') and getPart round-trip");
    {
        SoWrapperKit * kit = new SoWrapperKit;
        kit->ref();
        SoSeparator * sep = new SoSeparator;
        kit->setPart("contents", sep);
        SoNode * got = kit->getPart("contents", FALSE);
        bool pass = (got != nullptr);
        kit->unref();
        runner.endTest(pass, pass ? "" :
            "SoWrapperKit: getPart('contents') should return non-null after setPart");
    }

    // -----------------------------------------------------------------------
    // SoShapeKit: write/read round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SoShapeKit: write/read round-trip returns non-null root");
    {
        // Build a scene with a SoShapeKit
        SoSeparator * scene = new SoSeparator;
        scene->ref();
        SoShapeKit * kit = new SoShapeKit;
        kit->setPart("shape", new SoCube);
        scene->addChild(kit);

        char * buf = nullptr; size_t sz = 0;
        writeNode(scene, &buf, &sz);
        scene->unref();

        bool pass = false;
        if (buf != nullptr && sz > 0) {
            SoInput in;
            in.setBuffer(buf, std::strlen(buf));
            SoSeparator * r = SoDB::readAll(&in);
            pass = (r != nullptr);
            if (r) r->unref();
        }
        runner.endTest(pass, pass ? "" :
            "SoShapeKit write/read round-trip: SoDB::readAll returned null");
    }

    return runner.getSummary();
}
