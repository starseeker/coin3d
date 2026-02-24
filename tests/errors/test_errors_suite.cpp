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
 * @file test_errors_suite.cpp
 * @brief Tests for Coin3D error handling classes.
 *
 * Covers:
 *   SoError        - getClassTypeId, setHandlerCallback, post
 *   SoDebugError   - getClassTypeId, setHandlerCallback, postWarning, postInfo
 *   SoReadError    - getClassTypeId
 */

#include "../test_utils.h"

#include <Inventor/errors/SoError.h>
#include <Inventor/errors/SoDebugError.h>
#include <Inventor/errors/SoReadError.h>
#include <Inventor/SoType.h>
#include <string>
#include <cstring>

using namespace SimpleTest;

// ---------------------------------------------------------------------------
// Callback infrastructure for capturing error messages
// ---------------------------------------------------------------------------
struct ErrorCapture {
    bool fired;
    std::string msg;
    ErrorCapture() : fired(false) {}
};

static void myErrorCb(const SoError * err, void * data)
{
    ErrorCapture * cap = static_cast<ErrorCapture *>(data);
    cap->fired = true;
    cap->msg   = err->getDebugString().getString();
}

// Silent sink — used to suppress error output during negative tests
static void silentCb(const SoError * /*err*/, void * /*data*/) {}

int main()
{
    TestFixture fixture;
    TestRunner runner;

    // -----------------------------------------------------------------------
    // Class type IDs are valid
    // -----------------------------------------------------------------------
    runner.startTest("SoError::getClassTypeId is not badType");
    {
        bool pass = (SoError::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoError has bad class type");
    }

    runner.startTest("SoDebugError::getClassTypeId is not badType");
    {
        bool pass = (SoDebugError::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoDebugError has bad class type");
    }

    runner.startTest("SoReadError::getClassTypeId is not badType");
    {
        bool pass = (SoReadError::getClassTypeId() != SoType::badType());
        runner.endTest(pass, pass ? "" : "SoReadError has bad class type");
    }

    // -----------------------------------------------------------------------
    // SoError::setHandlerCallback / post
    // -----------------------------------------------------------------------
    runner.startTest("SoError::post triggers custom handler and message contains value");
    {
        ErrorCapture cap;

        // Save old handler (data pointer not retrievable; restore with null)
        SoErrorCB * oldCb = SoError::getHandlerCallback();

        SoError::setHandlerCallback(myErrorCb, &cap);
        SoError::post("test message %d", 42);

        // Restore original handler
        SoError::setHandlerCallback(oldCb, nullptr);

        bool pass = cap.fired && (cap.msg.find("42") != std::string::npos);
        runner.endTest(pass, pass ? "" :
            "SoError::post did not fire callback or message missing '42'");
    }

    // -----------------------------------------------------------------------
    // SoDebugError: postWarning fires custom handler
    // -----------------------------------------------------------------------
    runner.startTest("SoDebugError::postWarning triggers custom handler");
    {
        ErrorCapture cap;

        SoErrorCB * oldCb = SoDebugError::getHandlerCallback();

        SoDebugError::setHandlerCallback(myErrorCb, &cap);
        SoDebugError::postWarning("test_errors_suite", "warning %s", "hello");

        SoDebugError::setHandlerCallback(oldCb, nullptr);

        bool pass = cap.fired;
        runner.endTest(pass, pass ? "" :
            "SoDebugError::postWarning did not fire custom handler");
    }

    runner.startTest("SoDebugError::postWarning message contains keyword");
    {
        ErrorCapture cap;

        SoErrorCB * oldCb = SoDebugError::getHandlerCallback();

        SoDebugError::setHandlerCallback(myErrorCb, &cap);
        SoDebugError::postWarning("test_errors_suite", "warning %s", "hello");

        SoDebugError::setHandlerCallback(oldCb, nullptr);

        bool pass = cap.fired && (cap.msg.find("hello") != std::string::npos);
        runner.endTest(pass, pass ? "" :
            "SoDebugError::postWarning message missing 'hello'");
    }

    // -----------------------------------------------------------------------
    // SoDebugError: postInfo fires custom handler
    // -----------------------------------------------------------------------
    runner.startTest("SoDebugError::postInfo triggers custom handler");
    {
        ErrorCapture cap;

        SoErrorCB * oldCb = SoDebugError::getHandlerCallback();

        SoDebugError::setHandlerCallback(myErrorCb, &cap);
        SoDebugError::postInfo("test_errors_suite", "info %d", 99);

        SoDebugError::setHandlerCallback(oldCb, nullptr);

        bool pass = cap.fired;
        runner.endTest(pass, pass ? "" :
            "SoDebugError::postInfo did not fire custom handler");
    }

    // -----------------------------------------------------------------------
    // SoDebugError is a subtype of SoError
    // -----------------------------------------------------------------------
    runner.startTest("SoDebugError is subtype of SoError");
    {
        bool pass = SoDebugError::getClassTypeId().isDerivedFrom(
                        SoError::getClassTypeId());
        runner.endTest(pass, pass ? "" :
            "SoDebugError should be derived from SoError");
    }

    return runner.getSummary();
}
