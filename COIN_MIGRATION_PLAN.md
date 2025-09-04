# BRL-CAD Coin3D Migration and Refactor Plan

## Overview

This plan outlines a migration and refactor strategy for adopting and modernizing Coin3D/Open Inventor concepts within BRL-CAD, focusing on a lightweight, toolkit-agnostic, and extensible core. The plan explicitly incorporates a toolkit-agnostic Dragger API and implementation, following the style and philosophy of Coin3D while ensuring maximum flexibility for integration with diverse GUI frameworks.

---

## 1. **Goals**

- **Modernize scene management:** Replace ad-hoc or legacy BRL-CAD scene logic with a true scene graph and action system inspired by Open Inventor/Coin3D.
- **Enable advanced selection and manipulation:** Support object and sub-object (vertex, edge, face) picking and manipulation as first-class features.
- **Refactor dragger/manipulator logic:** Provide a robust, extensible, and toolkit-agnostic Dragger API, enabling interactive transformations (move, rotate, scale) across all GUIs.
- **Eliminate toolkit dependencies:** Remove all direct dependencies on Qt, Xt, Win32, or other GUI toolkits from the Coin core. GUI integration is achieved via event/callback hooks.
- **Lightweight and maintainable:** Only essential scene, action, and manipulation features are retained; legacy, unused, or heavyweight components are omitted.

---

## 2. **Migration Phases**

### **Phase 1: Core Scene Graph and Action Layer Extraction**

- Extract (or reimplement) the following core Coin3D node and action types:
  - `SoNode`, `SoGroup`, `SoSeparator`
  - `SoTransform`, `SoMatrixTransform`
  - Geometry nodes: `SoCoordinate3`, `SoIndexedFaceSet`, `SoIndexedLineSet`, etc.
  - Appearance: `SoMaterial`, `SoDrawStyle`, `SoBaseColor`
  - Camera/light: `SoPerspectiveCamera`, `SoDirectionalLight`
  - Fields: `SoField`, `SoSFFloat`, `SoSFVec3f`, etc.
  - Action base: `SoAction`, `SoGLRenderAction`, `SoGetBoundingBoxAction`

- Omit all toolkit bindings (SoQt, SoXt, SoWin, etc.), sensors, draggers with hardcoded toolkit logic, VRML, and other heavy/legacy features.

---

### **Phase 2: Picking, Selection, and Sub-Object Detail**

- Implement/port:
  - `SoRayPickAction` for toolkit-agnostic picking (input: ray, output: picked node and detail).
  - `SoPickedPoint`, `SoPath` for describing pick results.
  - Detail types: `SoFaceDetail`, `SoLineDetail`, `SoPointDetail` for sub-object identification (vertex/edge/face).
  - `SoSelection` node for selection tracking and callbacks.
  - Toolkit-independent callback system for selection/deselection events.

---

### **Phase 3: Dragger API and Implementation Refactor**

- **Design and implement a toolkit-agnostic Dragger API, Coin3D-style:**
  - All draggers subclass a core `SoDragger` class.
  - Transformation fields (`SoSFRotation`, `SoSFVec3f translation`, etc.) are exposed and connectable.
  - Visual handles/feedback are modeled as scene graph geometry (not widget-based).
  - Application/toolkit supplies input events via a clean API:
    - `handleEvent(const SoEvent*)` or toolkit-neutral `injectPointerEvent(x, y, buttonState, modifiers)`
    - All pointer, button, and modifier state is routed via explicit methods or callback hooks.
  - Callbacks for drag start, motion, finish remain, and are registerable from the application.
  - **No direct references to GUI toolkit types in dragger or core scene logic.**
  - Applications are responsible for mapping their own event system (Qt, GLFW, Tk, etc.) to the dragger API.

- **Examples of Draggers:**
  - `SoTranslate1Dragger`, `SoTranslate2Dragger`, `SoTranslate3Dragger`
  - `SoRotateSphericalDragger`, `SoScaleUniformDragger`, etc.
  - All subclass `SoDragger` and use the toolkit-agnostic event/callback interface.

---

## 3. **API Consistency and Coding Style**

- All new/refactored APIs maintain the Coin3D/C++ style: class-based, field-oriented, extensible via inheritance.
- All event/callback hooks use clear, type-safe interfaces, with minimal assumptions about event types (favor simple structs or explicit methods).
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
