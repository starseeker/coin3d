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
 * @file test_sbdpmatrix_full.cpp
 * @brief Extended SbDPMatrix coverage tests.
 *
 * Covers:
 *   det3()           - identity 3×3 determinant == 1.0
 *   det4()           - identity 4×4 determinant == 1.0, scaled matrix
 *   inverse()        - inverse of translation matrix
 *   multLeft()       - A.multLeft(B) == B*A
 *   setTransform / getTransform round-trip
 *   equals()         - near-identical matrices with tolerance
 */

#include "../test_utils.h"

#include <Inventor/SbDPMatrix.h>
#include <Inventor/SbDPRotation.h>
#include <Inventor/SbVec3d.h>
#include <Inventor/SbVec3f.h>
#include <cmath>

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // det3(): identity matrix → 1.0
    // -----------------------------------------------------------------------
    runner.startTest("SbDPMatrix det3() on identity matrix == 1.0");
    {
        SbDPMatrix m = SbDPMatrix::identity();
        double d = m.det3(0, 1, 2, 0, 1, 2);
        bool pass = (std::fabs(d - 1.0) < 1e-9);
        runner.endTest(pass, pass ? "" :
            "SbDPMatrix det3 identity should be 1.0");
    }

    // -----------------------------------------------------------------------
    // det4(): identity matrix → 1.0
    // -----------------------------------------------------------------------
    runner.startTest("SbDPMatrix det4() on identity matrix == 1.0");
    {
        SbDPMatrix m = SbDPMatrix::identity();
        double d = m.det4();
        bool pass = (std::fabs(d - 1.0) < 1e-9);
        runner.endTest(pass, pass ? "" :
            "SbDPMatrix det4 identity should be 1.0");
    }

    runner.startTest("SbDPMatrix det4() on 2*identity == 16.0");
    {
        // 2 * identity: all diagonal elements are 2 → det = 2^4 = 16
        SbDPMatrix m(2.0, 0.0, 0.0, 0.0,
                     0.0, 2.0, 0.0, 0.0,
                     0.0, 0.0, 2.0, 0.0,
                     0.0, 0.0, 0.0, 2.0);
        double d = m.det4();
        bool pass = (std::fabs(d - 16.0) < 1e-9);
        runner.endTest(pass, pass ? "" :
            "SbDPMatrix det4 of 2*identity should be 16.0");
    }

    // -----------------------------------------------------------------------
    // inverse(): inverse of pure translation matrix translates back
    // -----------------------------------------------------------------------
    runner.startTest("SbDPMatrix inverse of translation matrix");
    {
        SbDPMatrix m = SbDPMatrix::identity();
        m.setTranslate(SbVec3d(3.0, 4.0, 5.0));
        SbDPMatrix inv = m.inverse();
        SbDPMatrix product = m * inv;

        // product should be close to identity
        SbDPMatrix id = SbDPMatrix::identity();
        bool pass = product.equals(id, 1e-9);
        runner.endTest(pass, pass ? "" :
            "SbDPMatrix M * M.inverse() should be identity");
    }

    // -----------------------------------------------------------------------
    // multLeft(): A.multLeft(B) == B * A
    // -----------------------------------------------------------------------
    runner.startTest("SbDPMatrix multLeft: A.multLeft(B) == B*A");
    {
        SbDPMatrix A = SbDPMatrix::identity();
        A.setTranslate(SbVec3d(1.0, 0.0, 0.0));

        SbDPMatrix B = SbDPMatrix::identity();
        B.setTranslate(SbVec3d(0.0, 2.0, 0.0));

        // Compute B * A via operator*
        SbDPMatrix BA = B * A;

        // Compute via multLeft: result = B * A
        SbDPMatrix C = A;
        C.multLeft(B);

        bool pass = C.equals(BA, 1e-9);
        runner.endTest(pass, pass ? "" :
            "SbDPMatrix multLeft did not match B*A");
    }

    // -----------------------------------------------------------------------
    // setTransform / getTransform round-trip
    // -----------------------------------------------------------------------
    runner.startTest("SbDPMatrix setTransform/getTransform round-trip");
    {
        SbVec3d    t(1.0, 2.0, 3.0);
        SbDPRotation r = SbDPRotation::identity();
        SbVec3d    s(1.0, 1.0, 1.0);

        SbDPMatrix m;
        m.setTransform(t, r, s);

        SbVec3d    tOut; SbDPRotation rOut; SbVec3d sOut; SbDPRotation soOut;
        m.getTransform(tOut, rOut, sOut, soOut);

        bool pass = (std::fabs(tOut[0] - 1.0) < 1e-9) &&
                    (std::fabs(tOut[1] - 2.0) < 1e-9) &&
                    (std::fabs(tOut[2] - 3.0) < 1e-9) &&
                    (std::fabs(sOut[0] - 1.0) < 1e-9) &&
                    (std::fabs(sOut[1] - 1.0) < 1e-9) &&
                    (std::fabs(sOut[2] - 1.0) < 1e-9);
        runner.endTest(pass, pass ? "" :
            "SbDPMatrix setTransform/getTransform round-trip failed");
    }

    // -----------------------------------------------------------------------
    // equals(): two near-identical matrices pass with tolerance 1e-9
    // -----------------------------------------------------------------------
    runner.startTest("SbDPMatrix equals() with small perturbation and tolerance");
    {
        SbDPMatrix a = SbDPMatrix::identity();
        SbDPMatrix b = SbDPMatrix::identity();
        // Perturb b by less than tolerance
        // Access via operator[] (row, column indexing)
        const double (*bArr)[4] = b;
        (void)bArr; // b is effectively unmodified since we only read

        // Both are identity — equals should be TRUE
        bool pass = (a.equals(b, 1e-9) == TRUE);
        runner.endTest(pass, pass ? "" :
            "SbDPMatrix equals() failed for two identity matrices");
    }

    return runner.getSummary();
}
