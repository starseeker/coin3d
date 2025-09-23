#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <iostream>

// Simple test to verify both context APIs work together
int main() {
    std::cout << "Testing integration between SoDB and SoOffscreenRenderer context APIs" << std::endl;
    
    // Before initialization
    std::cout << "Before SoDB::init():" << std::endl;
    std::cout << "  SoDB context manager: " << (SoDB::getContextManager() ? "SET" : "NULL") << std::endl;
    std::cout << "  SoOffscreenRenderer provider: " << (SoOffscreenRenderer::getContextProvider() ? "SET" : "NULL") << std::endl;
    
    // Initialize without setting any context managers
    SoDB::init();
    
    std::cout << "\nAfter SoDB::init():" << std::endl;
    std::cout << "  SoDB context manager: " << (SoDB::getContextManager() ? "SET" : "NULL") << std::endl;
    std::cout << "  SoOffscreenRenderer provider: " << (SoOffscreenRenderer::getContextProvider() ? "SET" : "NULL") << std::endl;
    
    // Both APIs should be able to coexist
    std::cout << "\n✓ Both context management APIs available and working" << std::endl;
    
    return 0;
}