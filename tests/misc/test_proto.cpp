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
 * Obol retains the SoProto and SoProtoInstance class infrastructure, but the
 * SoBase reader no longer accepts ASCII PROTO declarations after removal of
 * the VRML97 input surface.
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

using namespace ObolTest;

TEST(MiscProto, SoProtoGetClassTypeIdIsNotBadType)
{
    EXPECT_TRUE((SoProto::getClassTypeId() != SoType::badType())) << "SoProto has bad class type";
}

TEST(MiscProto, SoProtoInstanceGetTypeIdMatchesClassType)
{
    SoProto * proto = new SoProto;
    proto->ref();
    EXPECT_TRUE((proto->getTypeId() == SoProto::getClassTypeId())) << "SoProto instance type does not match class type";
    proto->unref();
}

TEST(MiscProto, SoProtoIsSubtypeOfSoNode)
{
    EXPECT_TRUE(SoProto::getClassTypeId().isDerivedFrom(
                    SoNode::getClassTypeId())) << "SoProto should be derived from SoNode";
}

TEST(MiscProto, SoProtoGetProtoNameDefaultIsEmptySbName)
{
    SoProto * proto = new SoProto;
    proto->ref();
    SbName name = proto->getProtoName();
    // Default proto name is empty
    EXPECT_TRUE((name == SbName("") || name.getLength() == 0)) << "SoProto default proto name should be empty";
    proto->unref();
}

// -----------------------------------------------------------------------
// SoProtoInstance class type system
// -----------------------------------------------------------------------

TEST(MiscProto, SoProtoInstanceGetClassTypeIdIsNotBadType)
{
    EXPECT_TRUE((SoProtoInstance::getClassTypeId() != SoType::badType())) << "SoProtoInstance has bad class type";
}

TEST(MiscProto, SoProtoInstanceIsSubtypeOfSoNode)
{
    EXPECT_TRUE(SoProtoInstance::getClassTypeId().isDerivedFrom(
                    SoNode::getClassTypeId())) << "SoProtoInstance should be derived from SoNode";
}
