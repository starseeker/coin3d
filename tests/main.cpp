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
#include <Inventor/SoDB.h>
#include <memory>

// Simple global context manager for test setup
namespace {
    class GlobalOSMesaContextManager : public SoDB::ContextManager {
    public:
        virtual void* createOffscreenContext(unsigned int width, unsigned int height) override {
            auto* context = new CoinTestUtils::OSMesaTestContext(width, height);
            if (!context->isValid()) {
                delete context;
                return nullptr;
            }
            return context;
        }
        
        virtual SbBool makeContextCurrent(void* context) override {
            if (!context) return FALSE;
            auto* osmesa_ctx = static_cast<CoinTestUtils::OSMesaTestContext*>(context);
            return osmesa_ctx->makeCurrent() ? TRUE : FALSE;
        }
        
        virtual void restorePreviousContext(void* context) override {
            // OSMesa doesn't need explicit context switching in our test setup
            (void)context;
        }
        
        virtual void destroyContext(void* context) override {
            if (context) {
                delete static_cast<CoinTestUtils::OSMesaTestContext*>(context);
            }
        }
        
        virtual SbBool Initialize() override {
            // For global test context manager, verify OSMesa functions are available
            try {
                OSMesaContext test_ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
                if (test_ctx) {
                    OSMesaDestroyContext(test_ctx);
                    return TRUE;
                }
            } catch (...) {
                // OSMesa not available
            }
            return FALSE;
        }
        
        virtual SbBool IsInitialized() override {
            // OSMesa is statically linked and should be available
            return TRUE;
        }
    };
    
    std::unique_ptr<GlobalOSMesaContextManager> g_global_context_manager;
}
#endif

// Main entry point for Catch2 explicit tests
// This replaces the comment-driven test generation approach

// Global initialization that runs before any tests  
namespace {
    struct GlobalTestSetup {
        GlobalTestSetup() {
#ifdef COIN3D_OSMESA_BUILD
            // Create and use OSMesa context manager for all tests
            if (!g_global_context_manager) {
                g_global_context_manager = std::make_unique<GlobalOSMesaContextManager>();
            }
            
            // Initialize Coin3D with our context manager
            if (!SoDB::isInitialized()) {
                SoDB::init(g_global_context_manager.get());
                SoInteraction::init();
            }
#else
            // For non-OSMesa builds, we need a null context manager
            // This will cause an error which is expected since we can't render without context management
            if (!SoDB::isInitialized()) {
                SoDB::init(nullptr);  // This will show an error and return early
                SoInteraction::init();
            }
#endif
        }
        
        ~GlobalTestSetup() {
#ifdef COIN3D_OSMESA_BUILD
            // Clean up context manager
            g_global_context_manager.reset();
#endif
        }
    } g_test_setup;
}