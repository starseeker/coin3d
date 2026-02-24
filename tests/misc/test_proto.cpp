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
 * @file test_proto.cpp
 * @brief Tests for SoProto and SoProtoInstance class infrastructure.
 *
 * Note: PROTO definition parsing from ASCII IV files is no longer supported
 * in this Obol fork (the functionality was removed from SoProto).  These
 * tests verify the class type system and construction of SoProto objects
 * that are available.
 *
 * Covers:
 *   SoProto::getClassTypeId()       - class type registered
 *   SoProto::getTypeId()            - instance type matches class
 *   SoProto isOfType(SoNode)        - subtype relationship
 *   SoProtoInstance::getClassTypeId() - class type registered
 *   SoProtoInstance isOfType(SoNode)  - subtype relationship
 *   SoProto::getProtoName()         - default name is empty
 */

#include "../test_utils.h"

#include <Inventor/misc/SoProto.h>
#include <Inventor/misc/SoProtoInstance.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/SoType.h>
#include <Inventor/SbName.h>

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // SoProto class type system
    // -----------------------------------------------------------------------
    runner.startTest("SoProto::getClassTypeId() is not badType");
    {
        bool pass = (SoProto::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoProto has bad class type");
    }

    runner.startTest("SoProto instance getTypeId() matches class type");
    {
        SoProto * proto = new SoProto;
        proto->ref();
        bool pass = (proto->getTypeId() == SoProto::getClassTypeId());
        proto->unref();
        runner.endTest(pass, pass ? "" :
            "SoProto instance type does not match class type");
    }

    runner.startTest("SoProto is subtype of SoNode");
    {
        bool pass = SoProto::getClassTypeId().isDerivedFrom(
                        SoNode::getClassTypeId());
        runner.endTest(pass, pass ? "" :
            "SoProto should be derived from SoNode");
    }

    runner.startTest("SoProto::getProtoName() default is empty SbName");
    {
        SoProto * proto = new SoProto;
        proto->ref();
        SbName name = proto->getProtoName();
        // Default proto name is empty
        bool pass = (name == SbName("") || name.getLength() == 0);
        proto->unref();
        runner.endTest(pass, pass ? "" :
            "SoProto default proto name should be empty");
    }

    // -----------------------------------------------------------------------
    // SoProtoInstance class type system
    // -----------------------------------------------------------------------
    runner.startTest("SoProtoInstance::getClassTypeId() is not badType");
    {
        bool pass = (SoProtoInstance::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoProtoInstance has bad class type");
    }

    runner.startTest("SoProtoInstance is subtype of SoNode");
    {
        bool pass = SoProtoInstance::getClassTypeId().isDerivedFrom(
                        SoNode::getClassTypeId());
        runner.endTest(pass, pass ? "" :
            "SoProtoInstance should be derived from SoNode");
    }

    return runner.getSummary();
}
