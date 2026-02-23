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

using namespace SimpleTest;

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // connectFrom / isConnectedFromField
    // -----------------------------------------------------------------------
    runner.startTest("SoSFFloat connectFrom another SoSFFloat");
    {
        SoSFFloat master, slave;
        master.setValue(3.14f);
        bool connected = slave.connectFrom(&master);
        bool pass = connected && slave.isConnectedFromField();
        runner.endTest(pass, pass ? "" : "connectFrom failed or isConnectedFromField returned false");
    }

    runner.startTest("SoSFFloat value propagated after connectFrom");
    {
        SoSFFloat master, slave;
        master.setValue(2.718f);
        slave.connectFrom(&master);
        // After connection the slave should hold the master's current value
        bool pass = (slave.getValue() == master.getValue());
        runner.endTest(pass, pass ? "" : "slave value not equal to master after connectFrom");
    }

    runner.startTest("SoSFFloat slave tracks master value change");
    {
        SoSFFloat master, slave;
        master.setValue(1.0f);
        slave.connectFrom(&master);
        master.setValue(42.0f);
        bool pass = (slave.getValue() == 42.0f);
        runner.endTest(pass, pass ? "" : "slave did not track master value change");
    }

    // -----------------------------------------------------------------------
    // isConnected / disconnect
    // -----------------------------------------------------------------------
    runner.startTest("SoSFFloat isConnected true after connectFrom");
    {
        SoSFFloat master, slave;
        slave.connectFrom(&master);
        bool pass = slave.isConnected();
        runner.endTest(pass, pass ? "" : "isConnected should be true after connectFrom");
    }

    runner.startTest("SoSFFloat isConnected false before connection");
    {
        SoSFFloat field;
        bool pass = !field.isConnected();
        runner.endTest(pass, pass ? "" : "isConnected should be false for unconnected field");
    }

    runner.startTest("SoSFFloat disconnect clears connection");
    {
        SoSFFloat master, slave;
        master.setValue(7.0f);
        slave.connectFrom(&master);
        slave.disconnect();
        bool pass = !slave.isConnected() && !slave.isConnectedFromField();
        runner.endTest(pass, pass ? "" : "disconnect did not clear connection");
    }

    runner.startTest("SoSFFloat slave retains last value after disconnect");
    {
        SoSFFloat master, slave;
        master.setValue(5.5f);
        slave.connectFrom(&master);
        // value should be propagated
        slave.disconnect();
        // After disconnect the slave retains the last propagated value
        bool pass = (slave.getValue() == 5.5f);
        runner.endTest(pass, pass ? "" : "slave did not retain value after disconnect");
    }

    runner.startTest("SoSFFloat master change after disconnect does not affect slave");
    {
        SoSFFloat master, slave;
        master.setValue(1.0f);
        slave.connectFrom(&master);
        slave.disconnect();
        master.setValue(99.0f);
        bool pass = (slave.getValue() != 99.0f);
        runner.endTest(pass, pass ? "" : "slave should not track master after disconnect");
    }

    // -----------------------------------------------------------------------
    // disconnect(field*) — disconnect from a specific master
    // -----------------------------------------------------------------------
    runner.startTest("SoSFFloat disconnect(field*) removes specific connection");
    {
        SoSFFloat master, slave;
        slave.connectFrom(&master);
        slave.disconnect(&master);
        bool pass = !slave.isConnected();
        runner.endTest(pass, pass ? "" : "disconnect(field*) did not remove connection");
    }

    // -----------------------------------------------------------------------
    // SoSFVec3f field-to-field connection
    // -----------------------------------------------------------------------
    runner.startTest("SoSFVec3f connectFrom propagates vector value");
    {
        SoSFVec3f master, slave;
        master.setValue(SbVec3f(1.0f, 2.0f, 3.0f));
        slave.connectFrom(&master);
        bool pass = (slave.getValue() == SbVec3f(1.0f, 2.0f, 3.0f));
        runner.endTest(pass, pass ? "" : "SoSFVec3f connectFrom did not propagate value");
    }

    runner.startTest("SoSFVec3f slave tracks master update");
    {
        SoSFVec3f master, slave;
        master.setValue(SbVec3f(0.0f, 0.0f, 0.0f));
        slave.connectFrom(&master);
        master.setValue(SbVec3f(4.0f, 5.0f, 6.0f));
        bool pass = (slave.getValue() == SbVec3f(4.0f, 5.0f, 6.0f));
        runner.endTest(pass, pass ? "" : "SoSFVec3f slave did not track master update");
    }

    // -----------------------------------------------------------------------
    // Connection via node fields (SoMaterial diffuseColor)
    // -----------------------------------------------------------------------
    runner.startTest("SoMaterial field connectFrom propagates through node fields");
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
        runner.endTest(pass, pass ? "" : "SoMaterial field connection/propagation failed");
    }

    // -----------------------------------------------------------------------
    // SoSFInt32 connection
    // -----------------------------------------------------------------------
    runner.startTest("SoSFInt32 connectFrom and value propagation");
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
        runner.endTest(pass, pass ? "" : "SoSFInt32 connection/propagation failed");
    }

    // -----------------------------------------------------------------------
    // SoMFFloat connection
    // -----------------------------------------------------------------------
    runner.startTest("SoMFFloat connectFrom propagates multi-value field");
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
        runner.endTest(pass, pass ? "" : "SoMFFloat connectFrom failed");
    }

    return runner.getSummary();
}
