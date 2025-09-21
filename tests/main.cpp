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

#include "utils/test_common.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <iostream>

#ifdef COIN3D_OSMESA_BUILD
#include "utils/osmesa_test_context.h"
#include <memory>

// Global OSMesa context manager for all tests
namespace {
    std::unique_ptr<CoinTestUtils::OSMesaCallbackManager> g_global_osmesa_manager;
}
#endif

// Main entry point for Catch2 explicit tests
// This replaces the comment-driven test generation approach

// Global initialization that runs before any tests  
namespace {
    struct GlobalTestSetup {
        GlobalTestSetup() {
            // Initialize Coin3D first
            if (!SoDB::isInitialized()) {
                SoDB::init();
                SoInteraction::init();
            }
            
#ifdef COIN3D_OSMESA_BUILD
            // Set up OSMesa callbacks for all tests that need rendering
            if (!g_global_osmesa_manager) {
                g_global_osmesa_manager = std::make_unique<CoinTestUtils::OSMesaCallbackManager>();
            }
#endif
        }
        
        ~GlobalTestSetup() {
#ifdef COIN3D_OSMESA_BUILD
            // Clean up OSMesa callbacks
            g_global_osmesa_manager.reset();
#endif
        }
    } g_test_setup;
}