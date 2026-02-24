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
 * @file test_tools_suite.cpp
 * @brief Tests for the SbModernUtils C++17 utility module.
 *
 * Covers (tools/ subsystem, currently 0 % coverage):
 *   SbModernUtils::findNodeByName   - returns node when found / nullopt when absent
 *   SbModernUtils::nameEquals       - string_view comparison
 *   SbModernUtils::SoNodeRef        - RAII ref-counting, move semantics, release()
 *   SbModernUtils::makeNodeRef      - factory helper
 *   SbModernUtils::RefCountedPtr<T> - generic ref-counted wrapper, reset(), move
 *   SbModernUtils::makeRefCountedPtr - template factory
 */

#include "../test_utils.h"

#include <Inventor/tools/SbModernUtils.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/SbName.h>
#include <Inventor/SoType.h>

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SbModernUtils::nameEquals
    // -----------------------------------------------------------------------
    runner.startTest("SbModernUtils::nameEquals matches equal strings");
    {
        SbName name("MyNode");
        bool pass = SbModernUtils::nameEquals(name, "MyNode");
        runner.endTest(pass, pass ? "" : "nameEquals returned false for equal strings");
    }

    runner.startTest("SbModernUtils::nameEquals rejects different strings");
    {
        SbName name("Alpha");
        bool pass = !SbModernUtils::nameEquals(name, "Beta");
        runner.endTest(pass, pass ? "" : "nameEquals returned true for different strings");
    }

    runner.startTest("SbModernUtils::nameEquals empty name and empty view");
    {
        SbName name("");
        bool pass = SbModernUtils::nameEquals(name, "");
        runner.endTest(pass, pass ? "" : "nameEquals failed for two empty strings");
    }

    runner.startTest("SbModernUtils::nameEquals empty name vs non-empty view");
    {
        SbName name("");
        bool pass = !SbModernUtils::nameEquals(name, "something");
        runner.endTest(pass, pass ? "" : "nameEquals should return false for empty name vs non-empty view");
    }

    // -----------------------------------------------------------------------
    // SbModernUtils::findNodeByName
    // -----------------------------------------------------------------------
    runner.startTest("findNodeByName returns nullopt for unknown name");
    {
        auto result = SbModernUtils::findNodeByName(SbName("__no_such_node__"));
        bool pass = !result.has_value();
        runner.endTest(pass, pass ? "" : "Expected nullopt for missing node");
    }

    runner.startTest("findNodeByName finds a named node");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        cube->setName("findme_cube_tool");

        auto result = SbModernUtils::findNodeByName(SbName("findme_cube_tool"));
        bool pass = result.has_value() && (result.value() == cube);
        cube->unref();
        runner.endTest(pass, pass ? "" : "findNodeByName did not find named node");
    }

    // -----------------------------------------------------------------------
    // SbModernUtils::SoNodeRef — construction and auto-unref
    // -----------------------------------------------------------------------
    runner.startTest("SoNodeRef holds a node and operator bool is true");
    {
        SoCube * cube = new SoCube;
        cube->ref(); // extra ref so it survives SoNodeRef destruction
        {
            SbModernUtils::SoNodeRef nr(cube);
            bool pass = static_cast<bool>(nr) && (nr.get() == cube);
            cube->unref();
            runner.endTest(pass, pass ? "" : "SoNodeRef bool or get() failed");
        }
    }

    runner.startTest("SoNodeRef operator-> and operator* work");
    {
        SoCube * cube = new SoCube;
        cube->ref(); // keep alive
        {
            SbModernUtils::SoNodeRef nr(cube);
            // operator->
            SoType t1 = nr->getTypeId();
            // operator*
            SoType t2 = (*nr).getTypeId();
            bool pass = (t1 == SoCube::getClassTypeId()) &&
                        (t2 == SoCube::getClassTypeId());
            cube->unref();
            runner.endTest(pass, pass ? "" : "SoNodeRef operator-> or operator* failed");
        }
    }

    runner.startTest("SoNodeRef move constructor transfers ownership");
    {
        SoCube * cube = new SoCube;
        cube->ref(); // prevent immediate deletion
        SbModernUtils::SoNodeRef nr1(cube);
        SbModernUtils::SoNodeRef nr2(std::move(nr1));
        bool pass = (!static_cast<bool>(nr1)) && static_cast<bool>(nr2) &&
                    (nr2.get() == cube);
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoNodeRef move constructor failed");
    }

    runner.startTest("SoNodeRef move assignment transfers ownership");
    {
        SoCube * cube = new SoCube;
        cube->ref();
        SbModernUtils::SoNodeRef nr1(cube);
        SbModernUtils::SoNodeRef nr2(nullptr);
        nr2 = std::move(nr1);
        bool pass = (!static_cast<bool>(nr1)) && static_cast<bool>(nr2) &&
                    (nr2.get() == cube);
        cube->unref();
        runner.endTest(pass, pass ? "" : "SoNodeRef move assignment failed");
    }

    runner.startTest("SoNodeRef::release() returns raw pointer and nulls ref");
    {
        SoCube * cube = new SoCube;
        cube->ref(); // extra ref so node survives the ref that SoNodeRef added then we released
        SbModernUtils::SoNodeRef nr(cube);
        SoNode * released = nr.release();
        bool pass = (released == cube) && !static_cast<bool>(nr);
        // SoNodeRef added a ref; since we released without unref, balance the extra ref
        cube->unref(); // the ref added by SoNodeRef (now our responsibility)
        cube->unref(); // the extra ref we added above
        runner.endTest(pass, pass ? "" : "SoNodeRef::release() failed");
    }

    // -----------------------------------------------------------------------
    // SbModernUtils::makeNodeRef
    // -----------------------------------------------------------------------
    runner.startTest("makeNodeRef creates a valid SoNodeRef");
    {
        SoCube * cube = new SoCube;
        cube->ref(); // keep alive past makeNodeRef scope
        {
            auto nr = SbModernUtils::makeNodeRef(cube);
            bool pass = static_cast<bool>(nr) && (nr.get() == cube);
            cube->unref();
            runner.endTest(pass, pass ? "" : "makeNodeRef failed");
        }
    }

    // -----------------------------------------------------------------------
    // SbModernUtils::RefCountedPtr<T>
    // -----------------------------------------------------------------------
    runner.startTest("RefCountedPtr construction and get()");
    {
        SoCube * cube = new SoCube;
        {
            SbModernUtils::RefCountedPtr<SoCube> rp(cube);
            bool pass = static_cast<bool>(rp) && (rp.get() == cube);
            runner.endTest(pass, pass ? "" : "RefCountedPtr construction failed");
        }
        // cube should be deleted here (ref count back to 0)
    }

    runner.startTest("RefCountedPtr operator-> and operator*");
    {
        SoCube * cube = new SoCube;
        SbModernUtils::RefCountedPtr<SoCube> rp(cube);
        SoType t1 = rp->getTypeId();
        SoType t2 = (*rp).getTypeId();
        bool pass = (t1 == SoCube::getClassTypeId()) &&
                    (t2 == SoCube::getClassTypeId());
        runner.endTest(pass, pass ? "" : "RefCountedPtr operator-> or * failed");
    }

    runner.startTest("RefCountedPtr move constructor transfers ownership");
    {
        SoCube * cube = new SoCube;
        SbModernUtils::RefCountedPtr<SoCube> rp1(cube);
        SbModernUtils::RefCountedPtr<SoCube> rp2(std::move(rp1));
        bool pass = (!static_cast<bool>(rp1)) && static_cast<bool>(rp2) &&
                    (rp2.get() == cube);
        runner.endTest(pass, pass ? "" : "RefCountedPtr move ctor failed");
    }

    runner.startTest("RefCountedPtr::reset() replaces managed object");
    {
        SoCube * cube1 = new SoCube;
        SoCube * cube2 = new SoCube;
        SbModernUtils::RefCountedPtr<SoCube> rp(cube1);
        rp.reset(cube2);
        bool pass = (rp.get() == cube2);
        runner.endTest(pass, pass ? "" : "RefCountedPtr::reset() failed");
    }

    runner.startTest("RefCountedPtr::release() returns raw pointer");
    {
        SoCube * cube = new SoCube;
        cube->ref(); // extra ref so node survives after release
        SbModernUtils::RefCountedPtr<SoCube> rp(cube);
        SoCube * released = rp.release();
        bool pass = (released == cube) && !static_cast<bool>(rp);
        // We need to unref both the ref added by RefCountedPtr and our extra ref
        cube->unref(); // ref added by RefCountedPtr (released to us)
        cube->unref(); // our extra ref
        runner.endTest(pass, pass ? "" : "RefCountedPtr::release() failed");
    }

    // -----------------------------------------------------------------------
    // SbModernUtils::makeRefCountedPtr
    // -----------------------------------------------------------------------
    runner.startTest("makeRefCountedPtr creates valid wrapper");
    {
        SoCube * cube = new SoCube;
        auto rp = SbModernUtils::makeRefCountedPtr(cube);
        bool pass = static_cast<bool>(rp) && (rp.get() == cube);
        runner.endTest(pass, pass ? "" : "makeRefCountedPtr failed");
    }

    return runner.getSummary();
}
