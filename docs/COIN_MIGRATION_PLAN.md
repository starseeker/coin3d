# BRL-CAD Coin3D Migration and Refactor Plan

## Overview

This document outlines the migration and refactor strategy for modernizing Coin3D/Open Inventor concepts within BRL-CAD, focusing on a lightweight, toolkit-agnostic, and extensible core. The plan emphasizes modularity, generic interfaces, and callback-driven integration points that allow applications to supply platform/toolkit-specific logic. 

---

## 1. **Goals**

- **Modernize scene management:** Replace ad-hoc or legacy BRL-CAD scene logic with a true scene graph and action system inspired by Open Inventor/Coin3D.
- **Enable advanced selection and manipulation:** Support object and sub-object (vertex, edge, face) picking and manipulation as first-class features, with generic APIs.
- **Refactor dragger/manipulator logic:** Provide a robust, extensible, and toolkit-agnostic Dragger API, enabling interactive transformations (move, rotate, scale) across all GUIs.
- **Introduce generic rendering and event handling:** Implement a core `SoRenderArea` abstraction, allowing applications to provide rendering context, event delivery, and buffer management via callbacks.
- **Eliminate toolkit dependencies:** Remove all direct dependencies on Qt, Xt, Win32, or other GUI toolkits from the Coin core. GUI integration is achieved via event/callback hooks.
- **Lightweight and maintainable:** Only essential scene, action, and manipulation features are retained; legacy, unused, or heavyweight components are omitted.

---

## 2. **Migration Status and Phases**

### **Status Summary (as of September 2025)**

- **Core scene graph and action types**: Extracted and refactored; toolkit bindings, sensors, VRML, and other legacy features have been removed or isolated.
- **Picking, selection, and sub-object detail**: Basic picking and selection logic exists, but further abstraction for toolkit-agnostic callbacks is needed.
- **Dragger/manipulator logic**: Initial removal was aggressive; dragger code is not present in the current core, but key generic logic should be recovered and refactored from upstream Coin3D for integration.
- **Toolkit decoupling**: Most hardcoded toolkit dependencies have been removed, but some event and context management code may still require refactor for full generic support.
- **Generic rendering/event handling**: Not yet present; needs new `SoRenderArea` implementation and API documentation.

---

### **Phase 1: Core Scene Graph and Action Layer Extraction (Complete)**

- Extracted/reimplemented:
  - `SoNode`, `SoGroup`, `SoSeparator`
  - `SoTransform`, `SoMatrixTransform`
  - Geometry nodes: `SoCoordinate3`, `SoIndexedFaceSet`, `SoIndexedLineSet`, etc.
  - Appearance: `SoMaterial`, `SoDrawStyle`, `SoBaseColor`
  - Camera/light: `SoPerspectiveCamera`, `SoDirectionalLight`
  - Fields: `SoField`, `SoSFFloat`, `SoSFVec3f`, etc.
  - Action base: `SoAction`, `SoGLRenderAction`, `SoGetBoundingBoxAction`

- **Toolkit bindings (SoQt, SoXt, SoWin, etc.), sensors, VRML, and legacy features omitted.**

---

### **Phase 2: Picking, Selection, and Sub-Object Detail (In Progress)**

- Needs further work to ensure complete decoupling from toolkit-specific event systems:
  - `SoRayPickAction` for toolkit-agnostic picking (input: ray, output: picked node and detail).
  - `SoPickedPoint`, `SoPath` for describing pick results.
  - Detail types: `SoFaceDetail`, `SoLineDetail`, `SoPointDetail`.
  - `SoSelection` node for tracking selection and supporting callbacks.
  - **To do:** Abstract selection change/event notification via generic callback interfaces.

---

### **Phase 3: Dragger API and Implementation Refactor (Recovery & Generalization Needed)**

- **Action required:** The initial migration removed dragger code, but the core logic of draggers (interactive transformation, visual handles, event-driven manipulation) should be recovered from upstream Coin3D.
- **Plan:**
  - Recover, review, and refactor Coin3D's dragger classes: `SoDragger`, `SoTranslate1Dragger`, `SoRotateSphericalDragger`, etc.
  - Redesign the API for toolkit-agnostic use:
    - All draggers subclass a core `SoDragger` class.
    - Transformation fields are exposed and connectable.
    - Visual handles/feedback are modeled as scene graph geometry (not widget-based).
    - Application/toolkit supplies input events via a clean API (`handleEvent(const SoEvent*)` or generic `injectPointerEvent(x, y, buttonState, modifiers)`).
    - All pointer, button, and modifier state routed via explicit methods or callback hooks.
    - Callbacks for drag start, motion, finish remain, and are registerable from the application.
    - **No direct references to GUI toolkit types within dragger or core scene logic.**

- **Applications are responsible for mapping their own event systems to the dragger API.**

---

### **Phase 4: Generic Rendering and Event Handling (New Work)**

- **Introduce a core `SoRenderArea` abstraction:**
  - Defines a toolkit-neutral interface for rendering and event delivery.
  - Applications provide:
    - OpenGL context creation/destruction callbacks
    - Buffer swap callback
    - Event handling callback (mouse, keyboard, resize, etc.)
    - Resize notification callback

  - Example API:
    ```cpp
    class SoRenderArea {
    public:
        void setContextCallbacks(create, destroy, swap);
        void setEventCallback(eventHandler);
        void setResizeCallback(resizeHandler);
        void setSceneRoot(SoNode* root);
        void render();
        // ...
    };
    ```
  - **Document integration points and callback signatures.**
  - **Provide sample application code for integration with popular toolkits (GLFW, SDL, Qt, etc.).**

- **Other APIs to generalize:**
  - Font/text rendering via a provider interface.
  - Sensors/event dispatch via application-driven events.
  - Offscreen/framebuffer rendering via callback/provider.

---

### **Phase 5: Documentation, Testing, and CI**

- **Update documentation** to clarify toolkit-agnostic architecture, callback patterns, and integration strategies.
- **Remove all references to built-in viewers/widgets.**
- **Provide migration guides and examples for legacy applications.**
- **Ensure CI tests cover all generic APIs and application integration scenarios.**

---

### **Phase 6: Archive and Branch Legacy Code**

- **Move toolkit-specific code (viewers, old draggers, GUI logic) to a legacy branch or archive directory for reference.**
- **Document the separation for future maintainers.**

---

## 3. **API Consistency and Coding Style**

- All new/refactored APIs maintain the Coin3D/C++ style: class-based, field-oriented, extensible via inheritance.
- Event/callback hooks use clear, type-safe interfaces, with minimal assumptions about event types (favor simple structs or explicit methods).
- Core remains 100% toolkit-agnostic; all GUI-specific logic is handled externally via well-defined APIs.

---

## 4. **Benefits**

- **Extensible, maintainable, and modern:** Scene management, selection, and manipulation logic is reusable across all BRL-CAD GUIs and applications.
- **Toolkit-agnostic:** Core logic is portable, testable, and future-proof—integrates with any GUI or headless workflow.
- **Advanced user interaction:** Enables robust object and sub-object selection, and interactive manipulation, with clean separation of concerns.

---

## 5. **References**

- [Coin3D: SoDragger](https://coin3d.github.io/Coin/html/classSoDragger.html)
- [Coin3D: Picking and Selection](https://coin3d.github.io/Coin/html/classSoRayPickAction.html)
- [Open Inventor: Dragger Concepts](https://openinventor.com/resources/doc/RefManCpp/class_so_dragger.html)
- [BRL-CAD Design Documents and libdm API](https://brlcad.org/)

---

## 6. **Next Steps**

1. **Complete generic selection and picking APIs** with callback/event support.
2. **Recover and refactor dragger logic from upstream Coin3D** for toolkit-agnostic use.
3. **Implement and document the SoRenderArea abstraction**; provide integration samples.
4. **Generalize other platform-specific APIs** (font, sensor, offscreen rendering) as described.
5. **Archive legacy toolkit-specific code** and provide migration notes for downstream projects.

---