/*
 * Utility functions for headless rendering of Coin examples
 * 
 * This provides common functionality for converting interactive
 * Mentor examples to headless, offscreen rendering tests that
 * produce reference images for validation.
 *
 * Backend selection (compile-time):
 *   COIN3D_OSMESA_BUILD: use OSMesa for truly headless operation
 *   default:             use system OpenGL (GLX on Linux) with Xvfb
 *
 * Both paths require a SoDB::ContextManager since this Coin fork's
 * SoDB::init() always requires one.
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
#include <cstdio>
#include <cstring>
#include <cmath>

// Default image dimensions
#define DEFAULT_WIDTH 800
#define DEFAULT_HEIGHT 600

#ifdef COIN3D_OSMESA_BUILD
// ============================================================================
// OSMesa Backend: For offscreen/headless rendering without display server
// ============================================================================
#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#include <memory>

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

// OSMesa context manager for Coin
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
        (void)context;
    }
    
    virtual void destroyContext(void* context) override {
        delete static_cast<CoinOSMesaContext*>(context);
    }
};

/**
 * Initialize Coin database for headless operation (OSMesa backend)
 */
inline void initCoinHeadless() {
    static CoinHeadlessContextManager context_manager;
    SoDB::init(&context_manager);
    SoNodeKit::init();
    SoInteraction::init();
}

/**
 * Render a scene to an image file (OSMesa backend).
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

    SbViewportRegion viewport(width, height);
    SoOffscreenRenderer renderer(viewport);
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(backgroundColor);

    if (!renderer.render(root)) {
        fprintf(stderr, "Error: Failed to render scene\n");
        return false;
    }

    if (!renderer.writeToRGB(filename)) {
        fprintf(stderr, "Error: Failed to write to RGB file %s\n", filename);
        return false;
    }

    printf("Successfully rendered to %s (%dx%d)\n", filename, width, height);
    return true;
}

#else // !COIN3D_OSMESA_BUILD
// ============================================================================
// System OpenGL Backend: GLX on Linux (use Xvfb for headless operation)
// ============================================================================
#ifdef __unix__
#include <X11/Xlib.h>
#include <GL/glx.h>
#endif

#ifdef __unix__
// GLX offscreen context (pbuffer or pixmap)
struct GLXOffscreenCtx {
    Display  *dpy;
    int       width, height;
    GLXContext ctx;
    // pbuffer approach
    GLXPbuffer   pbuffer;
    GLXFBConfig  fbconfig;
    bool         use_pbuffer;
    // pixmap fallback
    Pixmap       xpixmap;
    GLXPixmap    glxpixmap;
    XVisualInfo *vi;
    // restore state
    GLXContext   prev_ctx;
    GLXDrawable  prev_draw;
    GLXDrawable  prev_read;
};

/**
 * GLX context manager for system OpenGL headless rendering.
 * Requires a running X server (real or Xvfb).
 * Set COIN_GLXGLUE_NO_PBUFFERS=1 to skip pbuffer and use pixmap fallback.
 * Set COIN_GLX_PIXMAP_DIRECT_RENDERING=1 to request direct rendering.
 */
class GLXContextManager : public SoDB::ContextManager {
public:
    GLXContextManager() : m_dpy(nullptr) {}

    virtual ~GLXContextManager() {
        if (m_dpy) {
            XCloseDisplay(m_dpy);
            m_dpy = nullptr;
        }
    }

    virtual void* createOffscreenContext(unsigned int width, unsigned int height) override {
        Display *dpy = getDisplay();
        if (!dpy) return nullptr;
        int screen = DefaultScreen(dpy);

        GLXOffscreenCtx *ctx = new GLXOffscreenCtx;
        ctx->dpy        = dpy;
        ctx->width      = width;
        ctx->height     = height;
        ctx->ctx        = nullptr;
        ctx->pbuffer    = 0;
        ctx->use_pbuffer = false;
        ctx->xpixmap    = 0;
        ctx->glxpixmap  = 0;
        ctx->vi         = nullptr;
        ctx->prev_ctx   = nullptr;
        ctx->prev_draw  = 0;
        ctx->prev_read  = 0;

        bool no_pbuffer = false;
        const char *env = getenv("COIN_GLXGLUE_NO_PBUFFERS");
        if (env && env[0] != '0') no_pbuffer = true;

        if (!no_pbuffer) {
            int fbattribs[] = {
                GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
                GLX_RENDER_TYPE,   GLX_RGBA_BIT,
                GLX_RED_SIZE,   8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
                GLX_DEPTH_SIZE, 16,
                GLX_DOUBLEBUFFER, False,
                None
            };
            int nfb = 0;
            GLXFBConfig *fbcfgs = glXChooseFBConfig(dpy, screen, fbattribs, &nfb);
            if (fbcfgs && nfb > 0) {
                int pbattribs[] = {
                    GLX_PBUFFER_WIDTH,  (int)width,
                    GLX_PBUFFER_HEIGHT, (int)height,
                    GLX_PRESERVED_CONTENTS, False,
                    None
                };
                ctx->fbconfig = fbcfgs[0];
                ctx->pbuffer  = glXCreatePbuffer(dpy, fbcfgs[0], pbattribs);
                if (ctx->pbuffer) {
                    // Pbuffers require direct rendering; always use True
                    ctx->ctx = glXCreateNewContext(dpy, fbcfgs[0], GLX_RGBA_TYPE, nullptr, True);
                    if (ctx->ctx) {
                        ctx->use_pbuffer = true;
                        XFree(fbcfgs);
                        return ctx;
                    }
                    glXDestroyPbuffer(dpy, ctx->pbuffer);
                    ctx->pbuffer = 0;
                }
                XFree(fbcfgs);
            }
        }

        // Fallback: Pixmap
        // Modern X servers disable indirect rendering (BadValue from X_GLXCreateContext
        // when direct=False). Check COIN_GLX_PIXMAP_DIRECT_RENDERING to use direct.
        Bool direct = False;
        const char *dr = getenv("COIN_GLX_PIXMAP_DIRECT_RENDERING");
        if (dr && dr[0] != '0') direct = True;

        int vattribs[] = {
            GLX_RGBA, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
            GLX_DEPTH_SIZE, 16, None
        };
        ctx->vi = glXChooseVisual(dpy, screen, vattribs);
        if (!ctx->vi) { delete ctx; return nullptr; }

        ctx->xpixmap = XCreatePixmap(dpy, RootWindow(dpy, screen),
                                     width, height, ctx->vi->depth);
        if (!ctx->xpixmap) { XFree(ctx->vi); delete ctx; return nullptr; }

        ctx->glxpixmap = glXCreateGLXPixmap(dpy, ctx->vi, ctx->xpixmap);
        ctx->ctx       = glXCreateContext(dpy, ctx->vi, nullptr, direct);

        if (!ctx->ctx && !direct) {
            // If indirect failed, retry with direct rendering
            ctx->ctx = glXCreateContext(dpy, ctx->vi, nullptr, True);
        }

        if (!ctx->ctx || !ctx->glxpixmap) {
            if (ctx->glxpixmap) glXDestroyGLXPixmap(dpy, ctx->glxpixmap);
            if (ctx->xpixmap)   XFreePixmap(dpy, ctx->xpixmap);
            XFree(ctx->vi);
            delete ctx;
            return nullptr;
        }
        return ctx;
    }

    virtual SbBool makeContextCurrent(void* context) override {
        GLXOffscreenCtx *ctx = static_cast<GLXOffscreenCtx*>(context);
        if (!ctx || !ctx->ctx) return FALSE;

        ctx->prev_ctx  = glXGetCurrentContext();
        ctx->prev_draw = glXGetCurrentDrawable();
        ctx->prev_read = glXGetCurrentReadDrawable();

        Bool ok = ctx->use_pbuffer
            ? glXMakeCurrent(ctx->dpy, ctx->pbuffer, ctx->ctx)
            : glXMakeCurrent(ctx->dpy, ctx->glxpixmap, ctx->ctx);
        return ok ? TRUE : FALSE;
    }

    virtual void restorePreviousContext(void* context) override {
        GLXOffscreenCtx *ctx = static_cast<GLXOffscreenCtx*>(context);
        if (!ctx) return;
        if (ctx->prev_ctx)
            glXMakeCurrent(ctx->dpy, ctx->prev_draw, ctx->prev_ctx);
        else
            glXMakeCurrent(ctx->dpy, None, nullptr);
    }

    virtual void destroyContext(void* context) override {
        GLXOffscreenCtx *ctx = static_cast<GLXOffscreenCtx*>(context);
        if (!ctx) return;
        glXMakeCurrent(ctx->dpy, None, nullptr);
        if (ctx->ctx) glXDestroyContext(ctx->dpy, ctx->ctx);
        if (ctx->use_pbuffer) {
            if (ctx->pbuffer) glXDestroyPbuffer(ctx->dpy, ctx->pbuffer);
        } else {
            if (ctx->glxpixmap) glXDestroyGLXPixmap(ctx->dpy, ctx->glxpixmap);
            if (ctx->xpixmap)   XFreePixmap(ctx->dpy, ctx->xpixmap);
            if (ctx->vi)        XFree(ctx->vi);
        }
        delete ctx;
    }

private:
    Display *m_dpy;

    Display* getDisplay() {
        if (!m_dpy) {
            m_dpy = XOpenDisplay(nullptr);
            if (!m_dpy) {
                fprintf(stderr,
                    "GLXContextManager: Cannot open X display. "
                    "Make sure DISPLAY is set (run under Xvfb).\n");
            }
        }
        return m_dpy;
    }
};
#endif // __unix__

/**
 * Initialize Coin database for headless operation (system OpenGL backend).
 *
 * On X11 systems, a non-exiting X error handler is installed to prevent
 * spurious BadMatch errors from Mesa/llvmpipe from aborting the process.
 * A GLXContextManager is provided so SoDB::init() gets a valid context manager.
 */
inline void initCoinHeadless() {
#ifdef __unix__
    XSetErrorHandler([](Display *, XErrorEvent *err) -> int {
        fprintf(stderr, "Coin headless: X error ignored (code=%d opcode=%d/%d)\n",
                (int)err->error_code, (int)err->request_code, (int)err->minor_code);
        return 0;
    });
    static GLXContextManager glx_context_manager;
    SoDB::init(&glx_context_manager);
#else
    // Non-Unix: provide a stub context manager (rendering may not work)
    class StubContextManager : public SoDB::ContextManager {
    public:
        virtual void* createOffscreenContext(unsigned int, unsigned int) override { return nullptr; }
        virtual SbBool makeContextCurrent(void*) override { return FALSE; }
        virtual void restorePreviousContext(void*) override {}
        virtual void destroyContext(void*) override {}
    };
    static StubContextManager stub;
    SoDB::init(&stub);
#endif
    SoNodeKit::init();
    SoInteraction::init();
}

/**
 * Return the single persistent offscreen renderer shared by all headless
 * examples.
 *
 * Only ONE GLX offscreen context can be successfully created per process in
 * Mesa/llvmpipe headless environments.  Sharing a single renderer object
 * across all render calls avoids this limitation.
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
 * Render a scene to an image file (system OpenGL backend).
 * Uses the shared renderer to avoid GLX context recreation issues.
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

    SoOffscreenRenderer *renderer = getSharedRenderer();
    renderer->setComponents(SoOffscreenRenderer::RGB);
    renderer->setBackgroundColor(backgroundColor);

    if (!renderer->render(root)) {
        fprintf(stderr, "Error: Failed to render scene\n");
        return false;
    }

    if (!renderer->writeToRGB(filename)) {
        fprintf(stderr, "Error: Failed to write to RGB file %s\n", filename);
        return false;
    }

    printf("Successfully rendered to %s (%dx%d)\n", filename, width, height);
    return true;
}

#endif // COIN3D_OSMESA_BUILD

/**
 * Find camera in scene graph
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
 */
inline SoCamera* ensureCamera(SoSeparator *root) {
    SoCamera *camera = findCamera(root);
    if (camera) {
        return camera;
    }
    
    SoPerspectiveCamera *newCam = new SoPerspectiveCamera;
    root->insertChild(newCam, 0);
    return newCam;
}

/**
 * Ensure scene has a light, add one if missing
 */
inline void ensureLight(SoSeparator *root) {
    SoSearchAction search;
    search.setType(SoDirectionalLight::getClassTypeId());
    search.setInterest(SoSearchAction::FIRST);
    search.apply(root);
    
    if (!search.getPath()) {
        SoDirectionalLight *light = new SoDirectionalLight;
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

    const SbVec3f center(0.0f, 0.0f, 0.0f);
    SbVec3f offset = camera->position.getValue() - center;

    SbRotation azimuthRot(SbVec3f(0.0f, 1.0f, 0.0f), azimuth);
    azimuthRot.multVec(offset, offset);

    SbVec3f viewDir = -offset;
    viewDir.normalize();
    SbVec3f up(0.0f, 1.0f, 0.0f);
    SbVec3f rightVec = up.cross(viewDir);
    float rLen = rightVec.length();
    if (rLen < 1e-4f) {
        rightVec = SbVec3f(1.0f, 0.0f, 0.0f);
    } else {
        rightVec *= (1.0f / rLen);
    }

    SbRotation elevationRot(rightVec, elevation);
    elevationRot.multVec(offset, offset);

    camera->position.setValue(center + offset);
    camera->pointAt(center, SbVec3f(0.0f, 1.0f, 0.0f));
}

/**
 * Simulate a mouse button press event
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
 */
inline void simulateMouseDrag(
    SoNode *root,
    const SbViewportRegion &viewport,
    int startX, int startY,
    int endX, int endY,
    int steps = 10,
    SoMouseButtonEvent::Button button = SoMouseButtonEvent::BUTTON1)
{
    simulateMousePress(root, viewport, startX, startY, button);
    
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        int x = (int)(startX + t * (endX - startX));
        int y = (int)(startY + t * (endY - startY));
        simulateMouseMotion(root, viewport, x, y);
    }
    
    simulateMouseRelease(root, viewport, endX, endY, button);
}

/**
 * Simulate a keyboard key press event
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
