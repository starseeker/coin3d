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
 * @file test_hardcopy.cpp
 * @brief Tests for the PostScript (hardcopy) export subsystem.
 *
 * Validates that SoVectorizePSAction::exportToPS() can produce a PostScript
 * file from a scene graph without requiring a viewer render-loop.
 */

#include "test_utils.h"

#include <Inventor/annex/HardCopy/SoVectorizePSAction.h>
#include <Inventor/annex/HardCopy/SoHardCopy.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/SbColor.h>

#include <cstdio>
#include <cstring>

using namespace SimpleTest;

// Helper: check that a file exists and contains at least one byte
static bool file_exists_nonempty(const char * path)
{
    FILE * fp = fopen(path, "rb");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fclose(fp);
    return sz > 0;
}

// Helper: check that a file begins with the PostScript magic comment
static bool is_valid_ps(const char * path)
{
    FILE * fp = fopen(path, "rb");
    if (!fp) return false;
    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    return strncmp(buf, "%!PS-Adobe", 10) == 0;
}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // Ensure the hardcopy subsystem is registered
    SoHardCopy::init();

    // -----------------------------------------------------------------------
    // Test 1: exportToPS() on a minimal scene
    // -----------------------------------------------------------------------
    runner.startTest("exportToPS basic scene");
    {
        const char * outfile = "/tmp/test_hardcopy_basic.ps";

        SoSeparator * root = new SoSeparator;
        root->ref();
        root->addChild(new SoCube);

        SbBool ok = SoVectorizePSAction::exportToPS(root, outfile);
        root->unref();

        if (!ok) {
            runner.endTest(false, "exportToPS returned FALSE");
        } else if (!file_exists_nonempty(outfile)) {
            runner.endTest(false, "output file is missing or empty");
        } else if (!is_valid_ps(outfile)) {
            runner.endTest(false, "output does not begin with PS magic comment");
        } else {
            runner.endTest(true);
        }
        remove(outfile);
    }

    // -----------------------------------------------------------------------
    // Test 2: exportToPS() with LANDSCAPE orientation
    // -----------------------------------------------------------------------
    runner.startTest("exportToPS LANDSCAPE orientation");
    {
        const char * outfile = "/tmp/test_hardcopy_landscape.ps";

        SoSeparator * root = new SoSeparator;
        root->ref();
        root->addChild(new SoCone);

        SbBool ok = SoVectorizePSAction::exportToPS(
            root, outfile,
            SoVectorizeAction::A4,
            SoVectorizeAction::LANDSCAPE);
        root->unref();

        if (!ok) {
            runner.endTest(false, "exportToPS (LANDSCAPE) returned FALSE");
        } else if (!is_valid_ps(outfile)) {
            runner.endTest(false, "LANDSCAPE output is not valid PS");
        } else {
            runner.endTest(true);
        }
        remove(outfile);
    }

    // -----------------------------------------------------------------------
    // Test 3: exportToPS() with background colour enabled
    // -----------------------------------------------------------------------
    runner.startTest("exportToPS with background colour");
    {
        const char * outfile = "/tmp/test_hardcopy_bg.ps";

        SoSeparator * root = new SoSeparator;
        root->ref();
        root->addChild(new SoSphere);

        SbBool ok = SoVectorizePSAction::exportToPS(
            root, outfile,
            SoVectorizeAction::A4,
            SoVectorizeAction::PORTRAIT,
            10.0f,
            TRUE,
            SbColor(0.2f, 0.2f, 0.6f));
        root->unref();

        if (!ok) {
            runner.endTest(false, "exportToPS (with bg) returned FALSE");
        } else if (!is_valid_ps(outfile)) {
            runner.endTest(false, "bg output is not valid PS");
        } else {
            runner.endTest(true);
        }
        remove(outfile);
    }

    // -----------------------------------------------------------------------
    // Test 4: exportToPS() on a richer scene (lights, materials, transforms)
    // -----------------------------------------------------------------------
    runner.startTest("exportToPS rich scene");
    {
        const char * outfile = "/tmp/test_hardcopy_rich.ps";

        SoSeparator * root = new SoSeparator;
        root->ref();

        SoDirectionalLight * light = new SoDirectionalLight;
        light->direction.setValue(SbVec3f(-1, -1, -1));
        root->addChild(light);

        SoMaterial * mat = new SoMaterial;
        mat->diffuseColor.setValue(SbColor(0.8f, 0.2f, 0.2f));
        root->addChild(mat);

        SoTranslation * tr = new SoTranslation;
        tr->translation.setValue(SbVec3f(-1.5f, 0, 0));
        root->addChild(tr);
        root->addChild(new SoCube);

        SoTranslation * tr2 = new SoTranslation;
        tr2->translation.setValue(SbVec3f(3.0f, 0, 0));
        root->addChild(tr2);
        root->addChild(new SoCone);

        SbBool ok = SoVectorizePSAction::exportToPS(root, outfile);
        root->unref();

        if (!ok) {
            runner.endTest(false, "exportToPS (rich scene) returned FALSE");
        } else if (!is_valid_ps(outfile)) {
            runner.endTest(false, "rich scene output is not valid PS");
        } else {
            runner.endTest(true);
        }
        remove(outfile);
    }

    // -----------------------------------------------------------------------
    // Test 5: exportToPS() fails gracefully for an unwritable path
    // -----------------------------------------------------------------------
    runner.startTest("exportToPS fails gracefully on bad path");
    {
        SoSeparator * root = new SoSeparator;
        root->ref();
        root->addChild(new SoCube);

        // A path that should not be writable in a sandboxed environment
        SbBool ok = SoVectorizePSAction::exportToPS(root, "/nonexistent_dir/out.ps");
        root->unref();

        if (ok) {
            runner.endTest(false, "exportToPS should have returned FALSE for bad path");
        } else {
            runner.endTest(true);
        }
    }

    return runner.getSummary();
}
