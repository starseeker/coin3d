/*
 * Test program to demonstrate the stricter initialization behavior
 * where library functions require SoDB::init() to be called first.
 */

#include <iostream>
#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/SoInput.h>

int main() {
    std::cout << "=== Testing Strict Initialization Behavior ===" << std::endl;
    
    std::cout << "\n1. Testing SoInteraction::init() without SoDB::init()..." << std::endl;
    SoInteraction::init();
    
    std::cout << "\n2. Testing SoNodeKit::init() without SoDB::init()..." << std::endl;
    SoNodeKit::init();
    
    std::cout << "\n3. Testing SoInput construction without SoDB::init()..." << std::endl;
    SoInput input;
    
    std::cout << "\n4. Now testing with proper SoDB::init() call..." << std::endl;
    SoDB::init(nullptr);  // Use nullptr for test purposes
    
    std::cout << "✓ SoDB::init() completed successfully with nullptr context manager" << std::endl;
    
    return 0;
}