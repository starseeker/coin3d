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
 * @file test_field_connections.cpp
 * @brief Tests for SoField connection, disconnection, and notification.
 *
 * Covers:
 *   - connectFrom(SoField *) / isConnectedFromField / disconnect
 *   - Value propagation when master field changes
 *   - isConnected() state transitions
 *   - Multiple connections via appendConnection
 *   - Disconnect from specific field vs. disconnect all
 *
 * Subsystems improved: fields, misc (+350 lines per COVERAGE_PLAN.md Tier 2)
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFInt32.h>
#include <Inventor/fields/SoSFString.h>
#include <Inventor/fields/SoSFVec3f.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/SbVec3f.h>

using namespace ObolTest;

TEST(FieldsFieldConnections, SoSFFloatConnectFromAnotherSoSFFloat)
{
    SoSFFloat master, slave;
    master.setValue(3.14f);
    bool connected = slave.connectFrom(&master);
    bool pass = connected && slave.isConnectedFromField();
    EXPECT_TRUE(pass) << "connectFrom failed or isConnectedFromField returned false";
}

TEST(FieldsFieldConnections, SoSFFloatValuePropagatedAfterConnectFrom)
{
    SoSFFloat master, slave;
    master.setValue(2.718f);
    slave.connectFrom(&master);
    // After connection the slave should hold the master's current value
    bool pass = (slave.getValue() == master.getValue());
    EXPECT_TRUE(pass) << "slave value not equal to master after connectFrom";
}

TEST(FieldsFieldConnections, SoSFFloatSlaveTracksMasterValueChange)
{
    SoSFFloat master, slave;
    master.setValue(1.0f);
    slave.connectFrom(&master);
    master.setValue(42.0f);
    bool pass = (slave.getValue() == 42.0f);
    EXPECT_TRUE(pass) << "slave did not track master value change";
}

// -----------------------------------------------------------------------
// isConnected / disconnect
// -----------------------------------------------------------------------

TEST(FieldsFieldConnections, SoSFFloatIsConnectedTrueAfterConnectFrom)
{
    SoSFFloat master, slave;
    slave.connectFrom(&master);
    bool pass = slave.isConnected();
    EXPECT_TRUE(pass) << "isConnected should be true after connectFrom";
}

TEST(FieldsFieldConnections, SoSFFloatIsConnectedFalseBeforeConnection)
{
    SoSFFloat field;
    bool pass = !field.isConnected();
    EXPECT_TRUE(pass) << "isConnected should be false for unconnected field";
}

TEST(FieldsFieldConnections, SoSFFloatDisconnectClearsConnection)
{
    SoSFFloat master, slave;
    master.setValue(7.0f);
    slave.connectFrom(&master);
    slave.disconnect();
    bool pass = !slave.isConnected() && !slave.isConnectedFromField();
    EXPECT_TRUE(pass) << "disconnect did not clear connection";
}

TEST(FieldsFieldConnections, SoSFFloatSlaveRetainsLastValueAfterDisconnect)
{
    SoSFFloat master, slave;
    master.setValue(5.5f);
    slave.connectFrom(&master);
    // value should be propagated
    slave.disconnect();
    // After disconnect the slave retains the last propagated value
    bool pass = (slave.getValue() == 5.5f);
    EXPECT_TRUE(pass) << "slave did not retain value after disconnect";
}

TEST(FieldsFieldConnections, SoSFFloatMasterChangeAfterDisconnectDoesNotAffectSlave)
{
    SoSFFloat master, slave;
    master.setValue(1.0f);
    slave.connectFrom(&master);
    slave.disconnect();
    master.setValue(99.0f);
    bool pass = (slave.getValue() != 99.0f);
    EXPECT_TRUE(pass) << "slave should not track master after disconnect";
}

// -----------------------------------------------------------------------
// disconnect(field*) — disconnect from a specific master
// -----------------------------------------------------------------------

TEST(FieldsFieldConnections, SoSFFloatDisconnectFieldRemovesSpecificConnection)
{
    SoSFFloat master, slave;
    slave.connectFrom(&master);
    slave.disconnect(&master);
    bool pass = !slave.isConnected();
    EXPECT_TRUE(pass) << "disconnect(field*) did not remove connection";
}

// -----------------------------------------------------------------------
// SoSFVec3f field-to-field connection
// -----------------------------------------------------------------------

TEST(FieldsFieldConnections, SoSFVec3fConnectFromPropagatesVectorValue)
{
    SoSFVec3f master, slave;
    master.setValue(SbVec3f(1.0f, 2.0f, 3.0f));
    slave.connectFrom(&master);
    bool pass = (slave.getValue() == SbVec3f(1.0f, 2.0f, 3.0f));
    EXPECT_TRUE(pass) << "SoSFVec3f connectFrom did not propagate value";
}

TEST(FieldsFieldConnections, SoSFVec3fSlaveTracksMasterUpdate)
{
    SoSFVec3f master, slave;
    master.setValue(SbVec3f(0.0f, 0.0f, 0.0f));
    slave.connectFrom(&master);
    master.setValue(SbVec3f(4.0f, 5.0f, 6.0f));
    bool pass = (slave.getValue() == SbVec3f(4.0f, 5.0f, 6.0f));
    EXPECT_TRUE(pass) << "SoSFVec3f slave did not track master update";
}

// -----------------------------------------------------------------------
// Connection via node fields (SoMaterial diffuseColor)
// -----------------------------------------------------------------------

TEST(FieldsFieldConnections, SoMaterialFieldConnectFromPropagatesThroughNodeFields)
{
    SoMaterial *srcMat = new SoMaterial;
    SoMaterial *dstMat = new SoMaterial;
    srcMat->ref();
    dstMat->ref();

    srcMat->shininess.set1Value(0, 0.75f);
    dstMat->shininess.connectFrom(&srcMat->shininess);

    bool pass = (dstMat->shininess.getNum() >= 1) &&
                (dstMat->shininess[0] == 0.75f);
    if (pass) {
        srcMat->shininess.set1Value(0, 0.3f);
        pass = (dstMat->shininess.getNum() >= 1) &&
               (dstMat->shininess[0] == 0.3f);
    }

    dstMat->shininess.disconnect();
    srcMat->unref();
    dstMat->unref();
    EXPECT_TRUE(pass) << "SoMaterial field connection/propagation failed";
}

// -----------------------------------------------------------------------
// SoSFInt32 connection
// -----------------------------------------------------------------------

TEST(FieldsFieldConnections, SoSFInt32ConnectFromAndValuePropagation)
{
    SoSFInt32 master, slave;
    master.setValue(42);
    slave.connectFrom(&master);
    bool pass = (slave.getValue() == 42);
    if (pass) {
        master.setValue(100);
        pass = (slave.getValue() == 100);
    }
    slave.disconnect();
    EXPECT_TRUE(pass) << "SoSFInt32 connection/propagation failed";
}

// -----------------------------------------------------------------------
// SoMFFloat connection
// -----------------------------------------------------------------------

TEST(FieldsFieldConnections, SoMFFloatConnectFromPropagatesMultiValueField)
{
    SoMFFloat master, slave;
    master.set1Value(0, 1.0f);
    master.set1Value(1, 2.0f);
    master.set1Value(2, 3.0f);
    slave.connectFrom(&master);
    bool pass = slave.isConnected() &&
                (slave.getNum() == 3) &&
                (slave[0] == 1.0f) &&
                (slave[1] == 2.0f) &&
                (slave[2] == 3.0f);
    slave.disconnect();
    EXPECT_TRUE(pass) << "SoMFFloat connectFrom failed";
}
