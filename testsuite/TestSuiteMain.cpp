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

#include <iostream>
#include "CoinTestFramework.h"

#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include "TestSuiteUtils.h"

using namespace SIM::Coin3D::Coin;

// Simple null context manager for legacy test suite
// This allows tests to run without rendering capabilities
class LegacyTestContextManager : public SoDB::ContextManager {
public:
    virtual void* createOffscreenContext(unsigned int width, unsigned int height) override {
        (void)width; (void)height;
        return nullptr; // No rendering context available
    }
    
    virtual SbBool makeContextCurrent(void* context) override {
        (void)context;
        return FALSE; // No context to make current
    }
    
    virtual void restorePreviousContext(void* context) override {
        (void)context;
        // No-op
    }
    
    virtual void destroyContext(void* context) override {
        (void)context;
        // No-op
    }
};

int main(int argc, char* argv[])
{
    LegacyTestContextManager contextManager;
    SoDB::init(&contextManager);
    SoInteraction::init();
    TestSuite::Init();

    int rc = unit_test_main(argc, argv);

    SoDB::finish();

    return rc;
}
