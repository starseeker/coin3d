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
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/SoInteraction.h>
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
#ifdef __unix__
#include <X11/Xlib.h>
#endif
#include <cstdio>
#include <cstring>
#include <cmath>

// Default image dimensions
#define DEFAULT_WIDTH 800
#define DEFAULT_HEIGHT 600

/**
 * Initialize Coin database for headless operation
 *
 * Notes:
 * - SoNodeKit::init() and SoInteraction::init() must be called explicitly
 *   because SoDB::init() does NOT call them (by design in Coin's architecture).
 * - On X11 systems, a non-exiting X error handler is installed to prevent
 *   spurious BadMatch errors from Mesa/llvmpipe (during pbuffer pixel transfer)
 *   from aborting the process.  The default Xlib error handler calls exit(1)
 *   for any X error, including non-fatal ones.
 *
 * Important: the installed X error handler is global and permanent (it is NOT
 * restored).  This is appropriate for headless single-run example programs.
 * Do not call this function in library code or long-lived applications that
 * need to preserve their own X error handling policy.
 */
inline void initCoinHeadless() {
#ifdef __unix__
    // Install a lenient X error handler before initializing Coin so that
    // spurious X errors from Mesa's internal pixel-transfer paths (e.g.
    // BadMatch from X_PutImage / X_ShmPutImage when using llvmpipe pbuffers)
    // do not terminate the process via the default Xlib exit(1) handler.
    XSetErrorHandler([](Display *, XErrorEvent *err) -> int {
        fprintf(stderr, "Coin headless: X error ignored (code=%d opcode=%d/%d)\n",
                (int)err->error_code, (int)err->request_code, (int)err->minor_code);
        return 0;
    });
#endif
    SoDB::init();
    SoNodeKit::init();
    SoInteraction::init();
}

/**
 * Return the single persistent offscreen renderer shared by all headless
 * examples.
 *
 * Only ONE GLX offscreen context can be successfully created per process in
 * Mesa/llvmpipe headless environments.  After the first SoOffscreenRenderer is
 * destroyed, subsequent renderer creation attempts fail with
 * "glXChooseFBConfig() gave no valid configs".  Sharing a single renderer
 * object across all render calls avoids this limitation.
 *
 * The renderer is always created at DEFAULT_WIDTH x DEFAULT_HEIGHT.
 * The renderer is intentionally kept alive for the entire process lifetime
 * (static storage) since recreating it causes GLX context failures.
 * @return Pointer to the shared renderer (never NULL after first call to
 *         initCoinHeadless()).
 */
inline SoOffscreenRenderer* getSharedRenderer() {
    static SoOffscreenRenderer *s_renderer = nullptr;
    if (!s_renderer) {
        SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        s_renderer = new SoOffscreenRenderer(vp);
    }
    return s_renderer;
}

/**
 * Render a scene to an image file.
 *
 * NOTE: the @p width and @p height parameters are retained for API
 * compatibility but are *not* honoured at runtime – rendering always occurs
 * at DEFAULT_WIDTH x DEFAULT_HEIGHT via the shared renderer.  All current
 * headless examples use the default 800x600 dimensions.
 *
 * @param root Scene graph root
 * @param filename Output filename (SGI RGB format)
 * @param width  Ignored; kept for API compatibility (default: DEFAULT_WIDTH)
 * @param height Ignored; kept for API compatibility (default: DEFAULT_HEIGHT)
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

    // Use the single shared persistent renderer.  See getSharedRenderer() for
    // why a fresh renderer is not created per call.
    SoOffscreenRenderer *renderer = getSharedRenderer();
    renderer->setComponents(SoOffscreenRenderer::RGB);
    renderer->setBackgroundColor(backgroundColor);

    // Render the scene
    if (!renderer->render(root)) {
        fprintf(stderr, "Error: Failed to render scene\n");
        return false;
    }

    // Write to file - use writeToRGB for SGI RGB format (doesn't need simage)
    if (!renderer->writeToRGB(filename)) {
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
 * Orbit camera around the scene center by specified angles.
 *
 * The camera position is moved along the surface of a sphere centered at the
 * origin (the default target of viewAll()), keeping the camera pointed at the
 * center. This produces correct non-blank images for side/angle views even
 * when the scene is small relative to the camera distance.
 *
 * @param camera   Camera to reposition
 * @param azimuth  Horizontal orbit angle in radians (around world Y axis)
 * @param elevation Vertical orbit angle in radians (positive = higher vantage)
 */
inline void rotateCamera(SoCamera *camera, float azimuth, float elevation) {
    if (!camera) return;

    // Scene center: viewAll() targets the origin by default
    const SbVec3f center(0.0f, 0.0f, 0.0f);

    // Vector from scene center to camera
    SbVec3f offset = camera->position.getValue() - center;

    // Step 1: Apply azimuth by orbiting around the world Y axis
    SbRotation azimuthRot(SbVec3f(0.0f, 1.0f, 0.0f), azimuth);
    azimuthRot.multVec(offset, offset);

    // Step 2: Compute the right vector (perpendicular to Y-up and view direction)
    // so that elevation orbits the camera upward/downward around that axis.
    SbVec3f viewDir = -offset;   // camera looks toward center
    viewDir.normalize();
    SbVec3f up(0.0f, 1.0f, 0.0f);
    SbVec3f rightVec = up.cross(viewDir);
    float rLen = rightVec.length();
    if (rLen < 1e-4f) {
        // Camera is near the polar axis - fall back to X as the right vector
        rightVec = SbVec3f(1.0f, 0.0f, 0.0f);
    } else {
        rightVec *= (1.0f / rLen);   // normalize
    }

    // Apply elevation orbit around the right vector
    SbRotation elevationRot(rightVec, elevation);
    elevationRot.multVec(offset, offset);

    // Move camera to new orbit position and orient it toward the scene center
    camera->position.setValue(center + offset);
    camera->pointAt(center, SbVec3f(0.0f, 1.0f, 0.0f));
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
