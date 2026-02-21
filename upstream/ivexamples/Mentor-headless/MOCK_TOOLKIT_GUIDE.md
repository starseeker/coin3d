# Mock GUI Toolkit Functions for Mentor Examples

## Overview

This document describes the generic mock GUI toolkit functions created to enable conversion of previously skipped Mentor examples. These functions demonstrate toolkit-agnostic patterns that work with ANY GUI framework.

## Problem Statement

The Mentor examples collection includes several examples that were initially skipped because they used toolkit-specific features (Xt/Motif). The question was: **Can we create "generic" mock-gui toolkit functions that demonstrate the toolkit-agnostic logic in Coin?**

Specific examples examined:
- **10.2.setEventCB** - Event callbacks using Xt-specific XButtonEvent
- **10.8.PickFilterNodeKit** - Pick filtering with material editor (used Xt widgets)
- **16.2.Callback** - Material editor callbacks
- **16.3.AttachEditor** - Material editor attachment

## Key Insight

After analysis, we discovered that **ALL the core Coin logic in these examples is toolkit-agnostic**. The toolkit-specific code was only:
1. Window creation and management
2. Native event capture and translation
3. Widget UI implementation (sliders, color pickers, etc.)

The actual **Coin integration patterns** are identical regardless of toolkit!

## Mock GUI Toolkit Architecture

### File: `mock_gui_toolkit.h`

This header provides generic implementations of the minimal interface ANY toolkit must provide to work with Coin.

### Components

#### 1. MockRenderArea

Represents the minimal interface a toolkit's render area must provide:

```cpp
class MockRenderArea {
    // Scene graph management
    void setSceneGraph(SoNode* root);
    SoNode* getSceneGraph() const;
    
    // Viewport properties
    SbVec2s getSize() const;
    const SbViewportRegion& getViewportRegion() const;
    
    // Event handling
    void setEventCallback(EventCallback callback, void* userData);
    bool processEvent(const SoEvent* event);
    
    // Display
    bool render(const char* filename);
};
```

**Toolkit responsibilities:**
- Create window with OpenGL context
- Translate native events to SoEvent
- Apply events to scene graph or callback
- Trigger redraws

**Coin responsibilities:**
- Process SoEvent through scene graph
- Render to OpenGL context
- Handle events in nodes (manipulators, callbacks)

#### 2. MockMaterialEditor

Generic material editor pattern that works with any toolkit:

```cpp
class MockMaterialEditor {
    // Material change callbacks
    typedef void (*MaterialChangedCallback)(void* userData, const SoMaterial* mtl);
    void addMaterialChangedCallback(MaterialChangedCallback cb, void* userData);
    
    // Attach to material node (bidirectional sync)
    void attach(SoMaterial* material);
    void detach();
    
    // Set material (simulates user editing)
    void setMaterial(const SoMaterial& material);
    void setDiffuseColor(const SbColor& color);
    void setShininess(float shininess);
    // ... other properties
};
```

**Pattern:**
1. Editor maintains reference to material node
2. User edits trigger `setMaterial()` calls
3. Material node is updated automatically
4. Callbacks notify interested parties
5. If attached, changes are bidirectional

**Works with ANY toolkit** - just needs UI controls that call these methods.

#### 3. Native Event Translation

Demonstrates how toolkits translate their native events to SoEvent:

```cpp
// Mock native event structure
struct MockAnyEvent {
    int type;        // Button press, motion, etc.
    int x, y;        // Mouse coordinates
    int button;      // Which button
    unsigned int state;  // Modifier/button state
};

// Translation function
SoEvent* translateNativeEvent(const MockAnyEvent* nativeEvent, 
                              const SbViewportRegion& viewport);
```

**Real toolkit examples:**
- **Xt**: XEvent → SoEvent
- **Qt**: QMouseEvent → SoEvent
- **FLTK**: Fl_Event → SoEvent
- **Win32**: MSG → SoEvent
- **Web**: JavaScript Event → SoEvent

The translation pattern is identical - only the input format differs!

## Converted Examples

### 10.2.setEventCB - Event Callbacks

**Original issue:** Used Xt-specific `XButtonEvent`, `XMotionEvent`

**Solution:** Mock demonstrates the generic event translation pattern

**Key points:**
- Application can intercept events before scene graph
- `setEventCallback()` pattern works with any toolkit
- Toolkit translates native events → SoEvent
- Application callback receives SoEvent (toolkit-independent)

**Demonstrates:**
- Left button: Add points via projection
- Middle button: Start/stop camera rotation
- Right button: Clear all points

**Files:**
- `10.2.setEventCB.cpp` - Complete example
- 4 output images showing interaction sequence

### 10.8.PickFilterNodeKit - Pick Filtering

**Original issue:** Used Xt window system, SoXtMaterialEditor widget

**Solution:** Mock shows pick filtering and material editing are toolkit-agnostic

**Key points:**
- Pick filter callback: pure Coin (SoPath manipulation)
- Material editor: generic pattern for any toolkit
- Selection callbacks: coordinate editor with selection
- NodeKit material editing: standard Coin API

**Demonstrates:**
- Pick filter truncates paths to nodekits
- Material editor syncs with selection
- Multi-select material editing
- All core logic works without GUI toolkit

**Files:**
- `10.8.PickFilterNodeKit.cpp` - Complete example
- 7 output images showing selection and editing

### 16.2.Callback - Material Editor Callbacks

**Original issue:** Used SoXtMaterialEditor widget

**Solution:** Mock material editor demonstrates generic callback pattern

**Key points:**
- Material editor callbacks: toolkit-independent pattern
- Callback receives SoMaterial: pure Coin type
- Application copies material values to scene graph
- Pattern works identically in Qt, FLTK, Xt, etc.

**Demonstrates:**
- User changes material in editor
- Callback updates scene material
- Multiple material changes (red, blue, gold)
- Scene automatically redraws with new materials

**Files:**
- `16.2.Callback.cpp` - Complete example
- 4 output images showing material changes

### 16.3.AttachEditor - Material Editor Attachment

**Original issue:** Used SoXtMaterialEditor::attach() method

**Solution:** Mock demonstrates bidirectional attachment pattern

**Key points:**
- Attachment is a generic editor pattern
- Editor syncs to material node automatically
- Material updates when user edits
- No toolkit-specific dependencies

**Demonstrates:**
- Attach editor to material node
- User edits automatically update material
- Programmatic changes sync to editor
- Bidirectional synchronization

**Files:**
- `16.3.AttachEditor.cpp` - Complete example
- 5 output images showing attachment behavior

## Architecture Insights

### What is Toolkit-Agnostic (100% Coin)

✅ **Scene graph management**
- SoNode hierarchy, paths, selection
- Field connections and values
- NodeKits and part management

✅ **Event system**
- SoEvent abstraction (mouse, keyboard)
- SoHandleEventAction
- Event callbacks
- Manipulator/dragger interaction

✅ **Rendering**
- SoOffscreenRenderer
- SoGLRenderAction
- Rendering to any OpenGL context

✅ **Material/property management**
- SoMaterial fields
- Field notifications
- Property copying and synchronization

✅ **Pick system**
- SoRayPickAction
- SoPickedPoint
- Path filtering
- Selection management

### What Toolkits Must Provide (Minimal Interface)

The minimal interface ANY toolkit must provide:

1. **Window with OpenGL context**
   - Create and manage window
   - Provide OpenGL rendering context
   - Handle window events (resize, close, etc.)

2. **Event translation**
   - Capture native events (mouse, keyboard)
   - Translate to SoEvent objects
   - Normalize coordinates to viewport
   - Apply to scene graph or callback

3. **Property editors** (optional)
   - Display controls (sliders, color pickers)
   - Call editor methods when user changes values
   - Update controls when properties change
   - Register callbacks for notifications

4. **Display coordination**
   - Trigger redraws when scene changes
   - Swap buffers after rendering
   - Handle animation timing

### Integration Pattern

```cpp
// Toolkit-specific initialization
QWidget* window = createWindow();
QGLWidget* glWidget = createGLWidget(window);

// Coin scene setup (toolkit-agnostic)
SoSeparator* scene = buildScene();

// Event translation (toolkit captures, translates, applies)
glWidget->mousePressEvent([](QMouseEvent* evt) {
    SoMouseButtonEvent coinEvent;
    coinEvent.setPosition(translateCoords(evt->pos()));
    coinEvent.setButton(translateButton(evt->button()));
    
    SoHandleEventAction action(viewport);
    action.setEvent(&coinEvent);
    action.apply(scene);
    
    if (action.isHandled()) 
        glWidget->update();
});

// Rendering (Coin does the work)
glWidget->paintGL([&]() {
    SoGLRenderAction ra(viewport);
    ra.apply(scene);
});
```

**This exact pattern works with:**
- Qt (QWidget + QGLWidget)
- FLTK (Fl_Gl_Window)
- Xt/Motif (Widget + GLXDrawingArea)
- Win32 (HWND + wglMakeCurrent)
- Web (Canvas + WebGL)
- Custom/headless (for testing)

## Testing and Validation

### Building Examples

```bash
cd ivexamples/Mentor-headless
mkdir build && cd build
cmake ..
make
```

### Running Examples

```bash
# Run individual example
./bin/10.2.setEventCB
./bin/10.8.PickFilterNodeKit
./bin/16.2.Callback
./bin/16.3.AttachEditor

# Output images go to current directory
ls -la *.rgb
```

### Output

Each example generates multiple images showing:
- Initial state
- User interactions (clicks, edits)
- Material changes
- Selection states
- Final results

## Benefits

### 1. Demonstrates Toolkit Independence

These examples **prove** that Coin's core logic is truly toolkit-agnostic:
- Same code works with any GUI framework
- Only window/event/display layer changes
- No Coin API changes needed for different toolkits

### 2. Establishes Integration Patterns

Provides clear patterns for integrating Coin with ANY toolkit:
- Event translation: native → SoEvent
- Material editor: callbacks + attachment
- Render area: scene graph + event processing
- Pick filtering: path manipulation

### 3. Enables Testing

Mock implementations allow testing of core Coin logic:
- No GUI framework required
- Deterministic event sequences
- Automated image validation
- CI/CD integration possible

### 4. Documents Minimal Interface

Clearly defines what toolkits MUST provide:
- Window with GL context
- Event translation
- Display coordination
- (Optional) Property editors

## Comparison with Original Examples

| Aspect | Original (Xt/Motif) | Mock/Headless | Coin Logic |
|--------|-------------------|---------------|------------|
| Window creation | XtAppInitialize | mockToolkitInit | None |
| Render area | SoXtRenderArea | MockRenderArea | SoGLRenderAction |
| Event handling | XEvent → callback | MockEvent → callback | SoHandleEventAction |
| Material editor | SoXtMaterialEditor widget | MockMaterialEditor | SoMaterial fields |
| Viewer | SoXtExaminerViewer | MockExaminerViewer | Camera + rendering |
| Main loop | XtAppMainLoop | Test sequence | None |

**Key finding:** The Coin-related code is IDENTICAL. Only the toolkit wrapper differs!

## Future Extensions

### Additional Mock Components

Could add mocks for other toolkit features:
- Transform editor (similar to material editor)
- Light editor
- Directional light manipulator UI
- Texture coordinate editor
- NURBS editor

### Other Toolkits

The patterns established here apply to:
- **Qt**: QGLWidget, Q3DScene
- **FLTK**: Fl_Gl_Window
- **GTK**: GtkGLArea
- **wxWidgets**: wxGLCanvas
- **ImGui**: Integration with rendering
- **Web**: Canvas + WebGL
- **Mobile**: iOS/Android OpenGL contexts

### Testing Framework

Could build automated testing:
- Generate reference images
- Compare with expected outputs
- Regression testing for Coin changes
- CI/CD pipeline integration

## Conclusion

This work demonstrates that **the "toolkit-agnostic" goal is already achieved** in Coin. The previously skipped examples weren't testing Coin features - they were testing toolkit integration patterns.

By creating mock implementations, we've shown:

1. **All core Coin logic is toolkit-independent**
   - Scene graph, rendering, events, materials, picking

2. **Toolkits provide a minimal, well-defined interface**
   - Window, event translation, display coordination

3. **Integration patterns are universal**
   - Same patterns work with any toolkit
   - Only the native event/widget types differ

4. **Testing is possible without GUI frameworks**
   - Mock implementations enable headless testing
   - Validates core Coin functionality

The mock GUI toolkit functions serve as both:
- **Documentation** of toolkit integration patterns
- **Test infrastructure** for Coin core logic
- **Examples** for developers using other toolkits

This establishes a clear path for integrating Coin with ANY GUI framework, whether existing (Qt, FLTK) or future (web, mobile, VR/AR).
