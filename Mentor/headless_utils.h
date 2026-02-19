/*
 * Utility functions for headless rendering of Coin examples
 * 
 * This provides common functionality for converting interactive
 * Mentor examples to headless, offscreen rendering tests that
 * produce reference images for validation.
 */

#ifndef HEADLESS_UTILS_H
#define HEADLESS_UTILS_H

#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <memory>

// Default image dimensions
#define DEFAULT_WIDTH 800
#define DEFAULT_HEIGHT 600

// ============================================================================
// Context Management: Adapts to how Coin was compiled
// ============================================================================

#ifdef COIN3D_OSMESA_BUILD
// ----------------------------------------------------------------------------
// OSMesa Backend: For offscreen/headless rendering without display server
// ----------------------------------------------------------------------------
#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>

// OSMesa context structure for offscreen rendering
struct CoinOSMesaContext {
    OSMesaContext context;
    std::unique_ptr<unsigned char[]> buffer;
    int width, height;
    
    CoinOSMesaContext(int w, int h) : width(w), height(h) {
        context = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
        if (context) {
            buffer = std::make_unique<unsigned char[]>(width * height * 4);
        }
    }
    
    ~CoinOSMesaContext() {
        if (context) OSMesaDestroyContext(context);
    }
    
    bool makeCurrent() {
        return context && OSMesaMakeCurrent(context, buffer.get(), GL_UNSIGNED_BYTE, width, height);
    }
    
    bool isValid() const { return context != nullptr; }
};

// OSMesa context manager implementation for Coin
class CoinHeadlessContextManager : public SoDB::ContextManager {
public:
    virtual void* createOffscreenContext(unsigned int width, unsigned int height) override {
        auto* ctx = new CoinOSMesaContext(width, height);
        return ctx->isValid() ? ctx : (delete ctx, nullptr);
    }
    
    virtual SbBool makeContextCurrent(void* context) override {
        return context && static_cast<CoinOSMesaContext*>(context)->makeCurrent() ? TRUE : FALSE;
    }
    
    virtual void restorePreviousContext(void* context) override {
        // OSMesa doesn't require explicit context switching for single-threaded use
        (void)context;
    }
    
    virtual void destroyContext(void* context) override {
        delete static_cast<CoinOSMesaContext*>(context);
    }
};

#else
// ----------------------------------------------------------------------------
// System OpenGL Backend: For rendering with platform OpenGL (GLX/WGL/CGL)
// ----------------------------------------------------------------------------
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#include <GL/gl.h>
#else
#include <GL/gl.h>
#endif

// Note: When Coin is built with system OpenGL, applications must ensure
// an OpenGL context is available. For truly headless operation (no display),
// build Coin with OSMesa instead (COIN3D_USE_OSMESA=ON).
// 
// This stub allows examples to compile with system OpenGL builds but they
// will fail at runtime if no display server is available.

// Stub context manager for system OpenGL builds
class CoinHeadlessContextManager : public SoDB::ContextManager {
public:
    virtual void* createOffscreenContext(unsigned int width, unsigned int height) override {
        // System OpenGL builds rely on Coin's internal context management
        // which typically requires a display server (X11, Wayland, etc.)
        (void)width;
        (void)height;
        fprintf(stderr, 
            "Warning: Examples compiled with system OpenGL build.\n"
            "Offscreen rendering requires a display server or building Coin with OSMesa.\n"
            "For headless operation, rebuild Coin with -DCOIN3D_USE_OSMESA=ON\n");
        return nullptr;
    }
    
    virtual SbBool makeContextCurrent(void* context) override {
        (void)context;
        return FALSE;
    }
    
    virtual void restorePreviousContext(void* context) override {
        (void)context;
    }
    
    virtual void destroyContext(void* context) override {
        (void)context;
    }
};

#endif // COIN3D_OSMESA_BUILD

/**
 * Initialize Coin database for headless operation
 * Adapts to how Coin was compiled (OSMesa or system OpenGL)
 */
inline void initCoinHeadless() {
    static CoinHeadlessContextManager context_manager;
    SoDB::init(&context_manager);
    
#ifdef COIN3D_OSMESA_BUILD
    printf("Coin examples initialized with OSMesa backend for headless rendering\n");
#else
    printf("Coin examples initialized with system OpenGL backend (requires display)\n");
#endif
}

/**
 * Render a scene to an image file
 * @param root Scene graph root
 * @param filename Output filename (extension determines format: .png, .jpg, .rgb)
 * @param width Image width
 * @param height Image height
 * @param backgroundColor Background color (default: black)
 * @return true if successful
 */
inline bool renderToFile(
    SoNode *root, 
    const char *filename,
    int width = DEFAULT_WIDTH,
    int height = DEFAULT_HEIGHT,
    const SbColor &backgroundColor = SbColor(0.0f, 0.0f, 0.0f))
{
    if (!root || !filename) {
        fprintf(stderr, "Error: Invalid parameters to renderToFile\n");
        return false;
    }

    // Create viewport and renderer
    SbViewportRegion viewport(width, height);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(backgroundColor);

    // Render the scene
    if (!renderer.render(root)) {
        fprintf(stderr, "Error: Failed to render scene\n");
        return false;
    }

    // Write to file - use writeToRGB for SGI RGB format (doesn't need simage)
    if (!renderer.writeToRGB(filename)) {
        fprintf(stderr, "Error: Failed to write to RGB file %s\n", filename);
        return false;
    }

    printf("Successfully rendered to %s (%dx%d)\n", filename, width, height);
    return true;
}

/**
 * Find camera in scene graph
 * @param root Scene graph root
 * @return First camera found, or NULL
 */
inline SoCamera* findCamera(SoNode *root) {
    SoSearchAction search;
    search.setType(SoCamera::getClassTypeId());
    search.setInterest(SoSearchAction::FIRST);
    search.apply(root);
    
    if (search.getPath()) {
        return (SoCamera*)search.getPath()->getTail();
    }
    return NULL;
}

/**
 * Ensure scene has a camera, add one if missing
 * @param root Scene graph separator (must be SoSeparator or compatible)
 * @return Camera in scene (existing or newly added)
 */
inline SoCamera* ensureCamera(SoSeparator *root) {
    SoCamera *camera = findCamera(root);
    if (camera) {
        return camera;
    }
    
    // Add a default perspective camera
    SoPerspectiveCamera *newCam = new SoPerspectiveCamera;
    root->insertChild(newCam, 0);
    return newCam;
}

/**
 * Ensure scene has a light, add one if missing
 * @param root Scene graph separator
 */
inline void ensureLight(SoSeparator *root) {
    SoSearchAction search;
    search.setType(SoDirectionalLight::getClassTypeId());
    search.setInterest(SoSearchAction::FIRST);
    search.apply(root);
    
    if (!search.getPath()) {
        // Add a default directional light
        SoDirectionalLight *light = new SoDirectionalLight;
        // Insert after camera (if exists) or at beginning
        SoCamera *cam = findCamera(root);
        int insertPos = 0;
        if (cam) {
            for (int i = 0; i < root->getNumChildren(); i++) {
                if (root->getChild(i) == cam) {
                    insertPos = i + 1;
                    break;
                }
            }
        }
        root->insertChild(light, insertPos);
    }
}

/**
 * Setup camera to view entire scene
 * @param root Scene graph root
 * @param camera Camera to position
 * @param viewport Viewport region for aspect ratio
 */
inline void viewAll(SoNode *root, SoCamera *camera, const SbViewportRegion &viewport) {
    if (!camera) return;
    camera->viewAll(root, viewport);
}

/**
 * Rotate camera around scene by specified angles
 * @param camera Camera to rotate
 * @param azimuth Horizontal rotation in radians
 * @param elevation Vertical rotation in radians
 */
inline void rotateCamera(SoCamera *camera, float azimuth, float elevation) {
    if (!camera) return;
    
    // Get current position and orientation
    SbVec3f position = camera->position.getValue();
    SbRotation orientation = camera->orientation.getValue();
    
    // Create rotation around Y axis (azimuth) and X axis (elevation)
    SbRotation azimuthRot(SbVec3f(0, 1, 0), azimuth);
    SbRotation elevationRot(SbVec3f(1, 0, 0), elevation);
    
    // Apply rotations
    SbRotation newOrientation = orientation * azimuthRot * elevationRot;
    camera->orientation.setValue(newOrientation);
}

/**
 * Simulate a mouse button press event
 * @param root Scene graph root
 * @param viewport Viewport region
 * @param x Screen X coordinate (pixels)
 * @param y Screen Y coordinate (pixels)
 * @param button Mouse button (default: BUTTON1)
 */
inline void simulateMousePress(
    SoNode *root,
    const SbViewportRegion &viewport,
    int x, int y,
    SoMouseButtonEvent::Button button = SoMouseButtonEvent::BUTTON1)
{
    SoMouseButtonEvent event;
    event.setButton(button);
    event.setState(SoButtonEvent::DOWN);
    event.setPosition(SbVec2s((short)x, (short)y));
    event.setTime(SbTime::getTimeOfDay());
    
    SoHandleEventAction action(viewport);
    action.setEvent(&event);
    action.apply(root);
}

/**
 * Simulate a mouse button release event
 * @param root Scene graph root
 * @param viewport Viewport region
 * @param x Screen X coordinate (pixels)
 * @param y Screen Y coordinate (pixels)
 * @param button Mouse button (default: BUTTON1)
 */
inline void simulateMouseRelease(
    SoNode *root,
    const SbViewportRegion &viewport,
    int x, int y,
    SoMouseButtonEvent::Button button = SoMouseButtonEvent::BUTTON1)
{
    SoMouseButtonEvent event;
    event.setButton(button);
    event.setState(SoButtonEvent::UP);
    event.setPosition(SbVec2s((short)x, (short)y));
    event.setTime(SbTime::getTimeOfDay());
    
    SoHandleEventAction action(viewport);
    action.setEvent(&event);
    action.apply(root);
}

/**
 * Simulate mouse motion event
 * @param root Scene graph root
 * @param viewport Viewport region
 * @param x Screen X coordinate (pixels)
 * @param y Screen Y coordinate (pixels)
 */
inline void simulateMouseMotion(
    SoNode *root,
    const SbViewportRegion &viewport,
    int x, int y)
{
    SoLocation2Event event;
    event.setPosition(SbVec2s((short)x, (short)y));
    event.setTime(SbTime::getTimeOfDay());
    
    SoHandleEventAction action(viewport);
    action.setEvent(&event);
    action.apply(root);
}

/**
 * Simulate a mouse drag gesture from start to end position
 * @param root Scene graph root
 * @param viewport Viewport region
 * @param startX Start X coordinate
 * @param startY Start Y coordinate
 * @param endX End X coordinate
 * @param endY End Y coordinate
 * @param steps Number of intermediate steps (default: 10)
 * @param button Mouse button (default: BUTTON1)
 */
inline void simulateMouseDrag(
    SoNode *root,
    const SbViewportRegion &viewport,
    int startX, int startY,
    int endX, int endY,
    int steps = 10,
    SoMouseButtonEvent::Button button = SoMouseButtonEvent::BUTTON1)
{
    // Initial press
    simulateMousePress(root, viewport, startX, startY, button);
    
    // Simulate dragging with intermediate motion events
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        int x = (int)(startX + t * (endX - startX));
        int y = (int)(startY + t * (endY - startY));
        simulateMouseMotion(root, viewport, x, y);
    }
    
    // Final release
    simulateMouseRelease(root, viewport, endX, endY, button);
}

/**
 * Simulate a keyboard key press event
 * @param root Scene graph root
 * @param viewport Viewport region
 * @param key Key to press
 */
inline void simulateKeyPress(
    SoNode *root,
    const SbViewportRegion &viewport,
    SoKeyboardEvent::Key key)
{
    SoKeyboardEvent event;
    event.setKey(key);
    event.setState(SoButtonEvent::DOWN);
    event.setTime(SbTime::getTimeOfDay());
    
    SoHandleEventAction action(viewport);
    action.setEvent(&event);
    action.apply(root);
}

/**
 * Simulate a keyboard key release event
 * @param root Scene graph root
 * @param viewport Viewport region
 * @param key Key to release
 */
inline void simulateKeyRelease(
    SoNode *root,
    const SbViewportRegion &viewport,
    SoKeyboardEvent::Key key)
{
    SoKeyboardEvent event;
    event.setKey(key);
    event.setState(SoButtonEvent::UP);
    event.setTime(SbTime::getTimeOfDay());
    
    SoHandleEventAction action(viewport);
    action.setEvent(&event);
    action.apply(root);
}

#endif // HEADLESS_UTILS_H
