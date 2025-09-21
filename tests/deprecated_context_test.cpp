#define COIN_INTERNAL
#include "../src/glue/glp.h"
#include <Inventor/SoDB.h>
#include <iostream>

int main() {
    std::cout << "Testing deprecated context creation (should show warning)" << std::endl;
    
    // Initialize Coin3D
    SoDB::init();
    
    // Try to create context WITHOUT setting callbacks
    // This should show the deprecation warning
    void* ctx = cc_glglue_context_create_offscreen(128, 128);
    
    if (ctx) {
        std::cout << "Context created with deprecated system" << std::endl;
        cc_glglue_context_destruct(ctx);
    } else {
        std::cout << "Context creation failed (expected if no X11 available)" << std::endl;
    }
    
    return 0;
}