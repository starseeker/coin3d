/*
 * Mock GUI Toolkit Functions for Mentor Examples
 * 
 * This header provides generic mock implementations of GUI toolkit functionality
 * that can stand in for Xt/Motif in examples that test toolkit-agnostic Coin logic.
 * 
 * Purpose:
 * - Demonstrate which Coin features can work with ANY toolkit
 * - Enable testing of core Coin logic without requiring actual GUI frameworks
 * - Establish patterns for integrating Coin with arbitrary toolkits
 * 
 * Philosophy:
 * These mocks implement the MINIMAL interface a toolkit must provide to work with Coin:
 * 1. Window/viewport dimensions
 * 2. Event translation (native events -> SoEvent)
 * 3. Material/property editors (callbacks for property changes)
 * 4. Display refresh coordination
 */

#ifndef MOCK_GUI_TOOLKIT_H
#define MOCK_GUI_TOOLKIT_H

#include "headless_utils.h"
#include <Inventor/SoDB.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SoPath.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <functional>

//=============================================================================
// Mock Render Area
// Represents the minimal interface a toolkit's render area must provide
//=============================================================================

class MockRenderArea {
public:
    MockRenderArea(int width = DEFAULT_WIDTH, int height = DEFAULT_HEIGHT)
        : m_width(width), m_height(height), m_sceneGraph(NULL),
          m_eventCallback(nullptr), m_eventCallbackUserData(nullptr)
    {
        m_viewport.setWindowSize((short)width, (short)height);
    }
    
    virtual ~MockRenderArea() {}
    
    // Scene graph management
    void setSceneGraph(SoNode* root) { m_sceneGraph = root; }
    SoNode* getSceneGraph() const { return m_sceneGraph; }
    
    // Viewport/window properties
    SbVec2s getSize() const { return SbVec2s((short)m_width, (short)m_height); }
    const SbViewportRegion& getViewportRegion() const { return m_viewport; }
    
    // Event handling - toolkit translates native events to SoEvent and applies to scene
    typedef std::function<SbBool(void*, void*)> EventCallback;
    
    void setEventCallback(EventCallback callback, void* userData) {
        m_eventCallback = callback;
        m_eventCallbackUserData = userData;
    }
    
    // Simulate processing a native event (in real toolkit, this would be native event type)
    // For mock purposes, we directly use SoEvent
    bool processEvent(const SoEvent* event) {
        if (m_eventCallback) {
            // Call application callback instead of scene graph
            return m_eventCallback(m_eventCallbackUserData, (void*)event);
        } else {
            // Default: apply event to scene graph
            if (m_sceneGraph) {
                SoHandleEventAction action(m_viewport);
                action.setEvent(event);
                action.apply(m_sceneGraph);
                return action.isHandled();
            }
        }
        return false;
    }
    
    // Display/rendering
    void setTitle(const char* title) { m_title = title; }
    const char* getTitle() const { return m_title.c_str(); }
    
    // In a real toolkit, these would show windows and start event loop
    void show() { printf("MockRenderArea::show() - %s\n", m_title.c_str()); }
    void hide() { printf("MockRenderArea::hide()\n"); }
    
    // Render current scene to file (mock of redraw)
    bool render(const char* filename) {
        if (!m_sceneGraph) return false;
        return renderToFile(m_sceneGraph, filename, m_width, m_height);
    }
    
protected:
    int m_width, m_height;
    SbViewportRegion m_viewport;
    SoNode* m_sceneGraph;
    std::string m_title;
    EventCallback m_eventCallback;
    void* m_eventCallbackUserData;
};

//=============================================================================
// Mock Material Editor
// Represents a generic material editor that could be implemented in any toolkit
//=============================================================================

class MockMaterialEditor {
public:
    MockMaterialEditor() 
        : m_attached(false), m_attachedMaterial(NULL), m_ignoreCallback(false)
    {
        // Initialize with default material
        m_currentMaterial = new SoMaterial;
        m_currentMaterial->ref();
    }
    
    virtual ~MockMaterialEditor() {
        if (m_currentMaterial) {
            m_currentMaterial->unref();
        }
    }
    
    // Material change callbacks
    typedef void (*MaterialChangedCallback)(void* userData, const SoMaterial* mtl);
    
    struct CallbackInfo {
        MaterialChangedCallback callback;
        void* userData;
    };
    
    void addMaterialChangedCallback(MaterialChangedCallback callback, void* userData) {
        CallbackInfo info;
        info.callback = callback;
        info.userData = userData;
        m_callbacks.push_back(info);
    }
    
    // Attach to a material node - bidirectional sync
    void attach(SoMaterial* material) {
        m_attachedMaterial = material;
        m_attached = true;
        
        if (material) {
            // Sync editor to match attached material
            m_ignoreCallback = true;
            setMaterial(*material);
            m_ignoreCallback = false;
        }
    }
    
    void detach() {
        m_attachedMaterial = NULL;
        m_attached = false;
    }
    
    // Set material in editor (simulates user editing)
    void setMaterial(const SoMaterial& material) {
        m_currentMaterial->copyFieldValues(&material);
        
        // If attached, update the attached node
        if (m_attached && m_attachedMaterial) {
            m_attachedMaterial->copyFieldValues(&material);
        }
        
        // Notify callbacks (unless we're ignoring to prevent loops)
        if (!m_ignoreCallback) {
            notifyCallbacks();
        }
    }
    
    const SoMaterial& getMaterial() const { return *m_currentMaterial; }
    
    // Simulate user changing specific material properties
    void setAmbientColor(const SbColor& color) {
        m_currentMaterial->ambientColor.setValue(color);
        if (m_attached && m_attachedMaterial) {
            m_attachedMaterial->ambientColor.setValue(color);
        }
        if (!m_ignoreCallback) notifyCallbacks();
    }
    
    void setDiffuseColor(const SbColor& color) {
        m_currentMaterial->diffuseColor.setValue(color);
        if (m_attached && m_attachedMaterial) {
            m_attachedMaterial->diffuseColor.setValue(color);
        }
        if (!m_ignoreCallback) notifyCallbacks();
    }
    
    void setSpecularColor(const SbColor& color) {
        m_currentMaterial->specularColor.setValue(color);
        if (m_attached && m_attachedMaterial) {
            m_attachedMaterial->specularColor.setValue(color);
        }
        if (!m_ignoreCallback) notifyCallbacks();
    }
    
    void setEmissiveColor(const SbColor& color) {
        m_currentMaterial->emissiveColor.setValue(color);
        if (m_attached && m_attachedMaterial) {
            m_attachedMaterial->emissiveColor.setValue(color);
        }
        if (!m_ignoreCallback) notifyCallbacks();
    }
    
    void setShininess(float shininess) {
        m_currentMaterial->shininess.setValue(shininess);
        if (m_attached && m_attachedMaterial) {
            m_attachedMaterial->shininess.setValue(shininess);
        }
        if (!m_ignoreCallback) notifyCallbacks();
    }
    
    void setTransparency(float transparency) {
        m_currentMaterial->transparency.setValue(transparency);
        if (m_attached && m_attachedMaterial) {
            m_attachedMaterial->transparency.setValue(transparency);
        }
        if (!m_ignoreCallback) notifyCallbacks();
    }
    
    // In real toolkit, this would show the editor window
    void show() { printf("MockMaterialEditor::show()\n"); }
    void hide() { printf("MockMaterialEditor::hide()\n"); }
    void setTitle(const char* title) { m_title = title; printf("MockMaterialEditor::setTitle(\"%s\")\n", title); }
    
protected:
    void notifyCallbacks() {
        for (size_t i = 0; i < m_callbacks.size(); i++) {
            m_callbacks[i].callback(m_callbacks[i].userData, m_currentMaterial);
        }
    }
    
    bool m_attached;
    SoMaterial* m_attachedMaterial;
    SoMaterial* m_currentMaterial;
    bool m_ignoreCallback;
    std::string m_title;
    std::vector<CallbackInfo> m_callbacks;
};

//=============================================================================
// Mock Directional Light Editor
// Represents a generic directional light editor for any toolkit
//=============================================================================

class MockDirectionalLightEditor {
public:
    MockDirectionalLightEditor() 
        : m_attachedLight(NULL), m_ignoreCallback(false)
    {
        // Initialize with default light
        m_currentLight = new SoDirectionalLight;
        m_currentLight->ref();
    }
    
    virtual ~MockDirectionalLightEditor() {
        if (m_currentLight) {
            m_currentLight->unref();
        }
    }
    
    // Light change callbacks
    typedef void (*LightChangedCallback)(void* userData, SoNode* light);
    
    struct CallbackInfo {
        LightChangedCallback callback;
        void* userData;
    };
    
    void addLightChangedCallback(LightChangedCallback callback, void* userData) {
        CallbackInfo info;
        info.callback = callback;
        info.userData = userData;
        m_callbacks.push_back(info);
    }
    
    // Attach to a directional light node or path
    // In real toolkit, would accept SoPath* or SoDirectionalLight*
    void attach(SoPath* path) {
        if (!path) return;
        
        SoNode* tail = path->getTail();
        if (tail && tail->isOfType(SoDirectionalLight::getClassTypeId())) {
            m_attachedLight = (SoDirectionalLight*)tail;
            
            // Sync editor to match attached light
            m_ignoreCallback = true;
            m_currentLight->direction.setValue(m_attachedLight->direction.getValue());
            m_currentLight->color.setValue(m_attachedLight->color.getValue());
            m_currentLight->intensity.setValue(m_attachedLight->intensity.getValue());
            m_currentLight->on.setValue(m_attachedLight->on.getValue());
            m_ignoreCallback = false;
        }
    }
    
    void attach(SoDirectionalLight* light) {
        m_attachedLight = light;
        
        if (light) {
            // Sync editor to match attached light
            m_ignoreCallback = true;
            m_currentLight->direction.setValue(light->direction.getValue());
            m_currentLight->color.setValue(light->color.getValue());
            m_currentLight->intensity.setValue(light->intensity.getValue());
            m_currentLight->on.setValue(light->on.getValue());
            m_ignoreCallback = false;
        }
    }
    
    void detach() {
        m_attachedLight = NULL;
    }
    
    // Simulate user changing light properties
    void setDirection(const SbVec3f& direction) {
        m_currentLight->direction.setValue(direction);
        if (m_attachedLight) {
            m_attachedLight->direction.setValue(direction);
        }
        if (!m_ignoreCallback) notifyCallbacks();
    }
    
    void setColor(const SbColor& color) {
        m_currentLight->color.setValue(color);
        if (m_attachedLight) {
            m_attachedLight->color.setValue(color);
        }
        if (!m_ignoreCallback) notifyCallbacks();
    }
    
    void setIntensity(float intensity) {
        m_currentLight->intensity.setValue(intensity);
        if (m_attachedLight) {
            m_attachedLight->intensity.setValue(intensity);
        }
        if (!m_ignoreCallback) notifyCallbacks();
    }
    
    void setOn(SbBool on) {
        m_currentLight->on.setValue(on);
        if (m_attachedLight) {
            m_attachedLight->on.setValue(on);
        }
        if (!m_ignoreCallback) notifyCallbacks();
    }
    
    const SoDirectionalLight* getLight() const { return m_currentLight; }
    
    // In real toolkit, this would show the editor window
    void show() { printf("MockDirectionalLightEditor::show()\n"); }
    void hide() { printf("MockDirectionalLightEditor::hide()\n"); }
    void setTitle(const char* title) { m_title = title; }
    
protected:
    void notifyCallbacks() {
        for (size_t i = 0; i < m_callbacks.size(); i++) {
            m_callbacks[i].callback(m_callbacks[i].userData, m_currentLight);
        }
    }
    
    SoDirectionalLight* m_attachedLight;
    SoDirectionalLight* m_currentLight;
    bool m_ignoreCallback;
    std::string m_title;
    std::vector<CallbackInfo> m_callbacks;
};

//=============================================================================
// Native Event Translation Helpers
// Real toolkits translate their native events (X11, Win32, etc.) to SoEvent
//=============================================================================

// Mock X11 event types (minimal subset needed for examples)
// In real Xt/Motif, these come from X11/Xlib.h
enum MockEventType {
    MockButtonPress = 4,
    MockButtonRelease = 5,
    MockMotionNotify = 6,
    MockKeyPress = 2,
    MockKeyRelease = 3
};

// Mock X11 button definitions
enum MockButton {
    MockButton1 = 1,
    MockButton2 = 2,
    MockButton3 = 3
};

// Mock X11 button state masks
enum MockButtonMask {
    MockButton1Mask = (1<<8),
    MockButton2Mask = (1<<9),
    MockButton3Mask = (1<<10)
};

// Generic event structure (simplified from XEvent)
struct MockAnyEvent {
    int type;
    int x, y;           // For button/motion events
    unsigned int state; // Button/modifier state
    int button;         // For button events
};

/**
 * Translate mock native event to Coin SoEvent
 * This demonstrates the pattern any toolkit must implement
 * 
 * @param nativeEvent Mock native event (in real code, would be XEvent*, Win32 MSG*, etc.)
 * @param viewport Viewport for coordinate normalization
 * @return Allocated SoEvent* (caller must delete) or NULL if not translated
 */
inline SoEvent* translateNativeEvent(const MockAnyEvent* nativeEvent, const SbViewportRegion& viewport) {
    if (!nativeEvent) return NULL;
    
    SoEvent* coinEvent = NULL;
    
    switch (nativeEvent->type) {
        case MockButtonPress:
        case MockButtonRelease: {
            SoMouseButtonEvent* mouseEvent = new SoMouseButtonEvent;
            
            // Translate button
            switch (nativeEvent->button) {
                case MockButton1: mouseEvent->setButton(SoMouseButtonEvent::BUTTON1); break;
                case MockButton2: mouseEvent->setButton(SoMouseButtonEvent::BUTTON2); break;
                case MockButton3: mouseEvent->setButton(SoMouseButtonEvent::BUTTON3); break;
            }
            
            // Translate state
            mouseEvent->setState(nativeEvent->type == MockButtonPress ? 
                                SoButtonEvent::DOWN : SoButtonEvent::UP);
            
            // Set position (Y coordinate is flipped: X11 top=0, Inventor bottom=0)
            SbVec2s size = viewport.getViewportSizePixels();
            mouseEvent->setPosition(SbVec2s((short)nativeEvent->x, 
                                           (short)(size[1] - nativeEvent->y)));
            
            mouseEvent->setTime(SbTime::getTimeOfDay());
            coinEvent = mouseEvent;
            break;
        }
        
        case MockMotionNotify: {
            SoLocation2Event* motionEvent = new SoLocation2Event;
            
            // Set position (Y coordinate flipped)
            SbVec2s size = viewport.getViewportSizePixels();
            motionEvent->setPosition(SbVec2s((short)nativeEvent->x,
                                            (short)(size[1] - nativeEvent->y)));
            
            motionEvent->setTime(SbTime::getTimeOfDay());
            coinEvent = motionEvent;
            break;
        }
        
        // KeyPress/KeyRelease would require keysym translation
        // Omitted for brevity, but follows same pattern
    }
    
    return coinEvent;
}

//=============================================================================
// Mock Examiner Viewer (minimal interface)
//=============================================================================

class MockExaminerViewer {
public:
    MockExaminerViewer(int width = DEFAULT_WIDTH, int height = DEFAULT_HEIGHT)
        : m_renderArea(width, height), m_sceneGraph(NULL)
    {
    }
    
    void setSceneGraph(SoNode* root) {
        m_sceneGraph = root;
        m_renderArea.setSceneGraph(root);
    }
    
    SoNode* getSceneGraph() const { return m_sceneGraph; }
    
    void setTitle(const char* title) { m_renderArea.setTitle(title); }
    void show() { m_renderArea.show(); }
    
    bool render(const char* filename) {
        return m_renderArea.render(filename);
    }
    
    const SbViewportRegion& getViewportRegion() const {
        return m_renderArea.getViewportRegion();
    }
    
protected:
    MockRenderArea m_renderArea;
    SoNode* m_sceneGraph;
};

//=============================================================================
// Main Loop Mock
// Real toolkits have event loops; headless examples just run sequences
//=============================================================================

/**
 * Mock main loop - in headless mode, we just run our test sequence
 * Real toolkits would enter event loop and wait for user input
 */
inline void mockMainLoop() {
    printf("MockToolkit: In real toolkit, would enter event loop here\n");
    printf("MockToolkit: In headless mode, test sequence has already run\n");
}

/**
 * Mock toolkit initialization
 * Real toolkits would initialize X11 connection, create display, etc.
 */
inline void* mockToolkitInit(const char* appName) {
    printf("MockToolkit: Initializing for application '%s'\n", appName);
    // In real toolkit, would return display connection, widget, or window handle
    return (void*)0x1; // Non-null to indicate success
}

#endif // MOCK_GUI_TOOLKIT_H
